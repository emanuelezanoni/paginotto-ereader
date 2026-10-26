#include "src/web/find.h"

#include <WiFi.h>   // WiFiClient

#include "src/config.h"
#include "src/state.h"
#include "src/pure/hashing.h"             // prefKeyForBook
#include "src/pure/reader_integration.h"
#include "src/storage/book_metadata.h"
#include "src/storage/fs_util.h"
#include "src/storage/page_cache.h"
#include "src/storage/library.h"
#include "src/storage/preferences_store.h"
#include "src/ui/reader.h"                // g_bookview / findPageForOffset
#include "src/ui/screens/reader_screen.h" // g_readerScreen (active-reader check)
#include "src/ui/text.h"                  // pageOffsetForPage
#include "src/web/chrome.h"

namespace {
struct EditUploadState {
  File tmp;
  String tmpPath;
  String bookPath;
  String error;
  String bookmarkAnchor;
  uint32_t bookmarkAnchorAt = 0;
  uint32_t bookmarkOffset = kOffsetUnset;
  bool hadBookmark = false;
  int bookmarkStatus = 0;
  bool ok = false;
};
EditUploadState s_edit;
}

namespace {

// Validate ?id and return the book index, or -1 with an error response sent.
int requireBookId(const char* arg = "id") {
  if (!server.hasArg(arg)) {
    server.send(400, "text/plain; charset=utf-8", "missing id");
    return -1;
  }
  int id = server.arg(arg).toInt();
  if (id < 0 || id >= g_library.bookCount) {
    server.send(400, "text/plain; charset=utf-8", "bad id");
    return -1;
  }
  return id;
}

}  // namespace

// Re-anchor a saved byte offset after a complete text replacement. The
// anchor includes context before and after the bookmark, so an exact unique
// match is considerably safer than guessing from a page number.
static int remapBookmarkAfterEdit(const String& path, const String& key,
                                  const String& anchor, uint32_t anchorAt) {
  if (anchor.length() == 0 || anchorAt >= anchor.length()
      || anchor.length() > kBookmarkAnchorMaxBytes) {
    PreferencesStore kv(prefs);
    saveSavedOffset(kv, key, 0);
    saveSavedPage(kv, key, 0);
    saveSavedAnchor(kv, key, String(), 0);
    return -1;
  }

  File file = FS.open(path, "r");
  if (!file) return -1;

  const size_t length = anchor.length();
  static uint8_t ring[kBookmarkAnchorMaxBytes];
  size_t seen = 0;
  size_t matches = 0;
  size_t matchStart = 0;
  while (file.available()) {
    int value = file.read();
    if (value < 0) break;
    ring[seen % length] = (uint8_t)value;
    seen++;
    if (seen < length) continue;

    const size_t start = (seen - length) % length;
    bool equal = true;
    for (size_t i = 0; i < length; i++) {
      if (ring[(start + i) % length] != (uint8_t)anchor[i]) {
        equal = false;
        break;
      }
    }
    if (equal) {
      matches++;
      matchStart = seen - length;
      if (matches > 1) break;
    }
  }
  file.close();

  PreferencesStore kv(prefs);
  if (matches != 1) {
    saveSavedOffset(kv, key, 0);
    saveSavedPage(kv, key, 0);
    saveSavedAnchor(kv, key, String(), 0);
    return -1;
  }

  saveSavedOffset(kv, key, (uint32_t)(matchStart + anchorAt));
  saveSavedPage(kv, key, 0);
  saveSavedAnchor(kv, key, anchor, anchorAt);
  return 1;
}

// ============================================================================
//  GET /readbook-text?id=N — stream the raw book bytes.
//
//  Books are stored as normalized UTF-8 plain text after upload, so the
//  client can search them directly without any device-side processing. Use
//  chunked transfer so we don't have to load the whole book into RAM.
// ============================================================================
static void handleReadbookText() {
  int id = requireBookId();
  if (id < 0) return;

  String path = String(g_library.books[id].path);
  File f = FS.open(path, "r");
  if (!f) {
    server.send(404, "text/plain; charset=utf-8", "Open failed");
    return;
  }

  size_t total = f.size();
  server.setContentLength(total);
  server.send(200, "text/plain; charset=utf-8", "");
  WiFiClient client = server.client();

  uint8_t buf[512];
  while (f.available() && client.connected()) {
    size_t want = f.available() > sizeof(buf) ? sizeof(buf) : f.available();
    size_t got = f.read(buf, want);
    if (got == 0) break;
    client.write(buf, got);
  }
  f.close();
}

static void handleEditBookStream() {
  HTTPUpload& up = server.upload();
  if (up.status == UPLOAD_FILE_START) {
    s_edit = EditUploadState{};
    int id = server.hasArg("id") ? server.arg("id").toInt() : -1;
    if (id < 0 || id >= g_library.bookCount) { s_edit.error = "bad id"; return; }
    s_edit.bookPath = String(g_library.books[id].path);
    PreferencesStore kv(prefs);
    const String key = prefKeyForBook(s_edit.bookPath);
    s_edit.bookmarkOffset = loadSavedOffset(kv, key);
    s_edit.hadBookmark = s_edit.bookmarkOffset != kOffsetUnset;
    s_edit.bookmarkAnchor = loadSavedAnchor(kv, key);
    s_edit.bookmarkAnchorAt = loadSavedAnchorAt(kv, key);
    // Direct replacement deliberately avoids requiring a second full copy of
    // the book on LittleFS. The WebUI asks the user to download a manual
    // backup first; an interrupted edit can leave a partial book.
    s_edit.tmpPath = "";
    File existing = FS.open(s_edit.bookPath, "r");
    const size_t existingSize = existing ? existing.size() : 0;
    if (existing) existing.close();
    const size_t freeBytes = fsFreeBytesSafe();
    const size_t declaredSize = server.hasArg("size")
                              ? (size_t)server.arg("size").toInt() : 0;
    if (declaredSize > 0 && declaredSize + 8192 > existingSize + freeBytes) {
      s_edit.error = "edited book is too large (need " +
                     String(declaredSize + 8192) + " bytes, can fit " +
                     String(existingSize + freeBytes) + ")";
      return;
    }
    s_edit.tmp = FS.open(s_edit.bookPath, "w");
    if (!s_edit.tmp) s_edit.error = "cannot open book for direct edit";
  } else if (up.status == UPLOAD_FILE_WRITE) {
    if (s_edit.error.length() == 0 && s_edit.tmp && up.currentSize > 0
        && s_edit.tmp.write(up.buf, up.currentSize) != up.currentSize) {
      s_edit.error = "direct edit write failed";
      s_edit.tmp.close();
    }
  } else if (up.status == UPLOAD_FILE_END) {
    if (s_edit.tmp) s_edit.tmp.close();
    if (s_edit.error.length() == 0 && FS.exists(s_edit.bookPath)) {
      s_edit.ok = true;
    }
  } else if (up.status == UPLOAD_FILE_ABORTED) {
    if (s_edit.tmp) s_edit.tmp.close();
    s_edit.error = "direct edit upload aborted; book may be incomplete";
  }
}

static void handleEditBookDone() {
  if (!s_edit.ok) {
    server.send(400, "text/plain; charset=utf-8", s_edit.error.length() ? s_edit.error : "edit failed");
    return;
  }
  deletePageCacheForBook(s_edit.bookPath);
  loadBooks();
  if (s_edit.hadBookmark) {
    const String key = prefKeyForBook(s_edit.bookPath);
    if (s_edit.bookmarkOffset == 0) {
      // Offset zero is already the canonical beginning of the book. There is
      // nothing to translate, and a missing anchor here is expected for new
      // books or text removed from the beginning.
      PreferencesStore kv(prefs);
      saveSavedOffset(kv, key, 0);
      saveSavedPage(kv, key, 0);
      saveSavedAnchor(kv, key, String(), 0);
      s_edit.bookmarkStatus = 0;
    } else {
      s_edit.bookmarkStatus = remapBookmarkAfterEdit(
          s_edit.bookPath, key, s_edit.bookmarkAnchor, s_edit.bookmarkAnchorAt);
    }
  }
  String location;
  if (server.hasArg("return") && server.arg("return") == "/upload-review") {
    location = "/upload-review";
    location += "?notice=edited";
  } else {
    location = String("/read?id=")
             + String(server.hasArg("id") ? server.arg("id") : "0");
    if (s_edit.bookmarkStatus > 0) location += "&bookmark=restored";
    else if (s_edit.bookmarkStatus < 0) location += "&bookmark=reset";
  }
  server.sendHeader("Location", location);
  server.send(302, "text/plain", "");
}

// ============================================================================
//  POST /jumpoffset — set the resume position to a specific byte offset.
//
//  Mirrors /jumppage's persistence: write both the byte offset (canonical)
//  and the derived page number (display hint). If the reader is currently
//  active on this book, also update the in-memory cursor so the next render
//  lands at the new position without needing a reopen.
// ============================================================================
static void handleJumpOffset() {
  int id = requireBookId();
  if (id < 0) return;
  if (!server.hasArg("offset")) {
    server.send(400, "text/plain; charset=utf-8", "missing offset");
    return;
  }

  String path = String(g_library.books[id].path);
  String key  = prefKeyForBook(path);

  File f = FS.open(path, "r");
  if (!f) {
    server.send(500, "text/plain; charset=utf-8", "Open failed");
    return;
  }
  size_t fileSize = f.size();
  const ReaderJumpDecision jump =
      decideReaderJump(parseReaderJumpOffset(server.arg("offset").c_str()),
                       fileSize);

  PreferencesStore kv(prefs);
  saveSavedOffset(kv, key, jump.offset);
  if (server.hasArg("anchor") && server.hasArg("anchorAt")) {
    uint32_t anchorAt = (uint32_t)server.arg("anchorAt").toInt();
    saveSavedAnchor(kv, key, server.arg("anchor"), anchorAt);
  }

  // If the reader is currently active on this book, update the in-memory
  // cursor + save the matching page number so the device sees the jump on
  // its next render. Otherwise the offset alone is enough: openBookByIndex
  // resolves it to a page on next open via findPageForOffset (the saved
  // page-number key is just a stale display hint, refreshed on next open).
  bool activeOnThisBook =
      (g_currentScreen == &g_readerScreen) &&
      g_bookview.book.isOpen() &&
      g_bookview.book.path() == path;

  if (activeOnThisBook) {
    int newPage = findPageForOffset(jump.offset);
    g_bookview.cursor.pageIndex = newPage;
    saveSavedPage(kv, key, newPage);
  }
  f.close();

  server.sendHeader("Location", String("/read?id=") + String(id));
  server.send(302, "text/plain", "");
}

// ============================================================================
//  GET /read?id=N — in-browser reader with find + jump UI.
//
//  The page loads the book text once via /readbook-text, runs all searches
//  client-side (browser handles the regex), and offers a "jump to here"
//  button per match that POSTs the byte offset back to the device.
// ============================================================================

// Embedded JS — loads the book text, builds a match list, scrolls between
// matches, and POSTs the byte offset of the active match to /jumpoffset.
//
// Matching is forgiving: the query gets the same typography folding that
// book uploads get on the device (smart quotes / dashes / ellipsis / NBSP
// to ASCII), then both sides are compared case-folded with whitespace runs
// collapsed. A sentence pasted from another reading app therefore matches
// the stored text even when quotes, dashes, or line breaks differ. An
// index map converts normalized match positions back to raw string
// positions for highlighting.
//
// Offsets sent to /jumpoffset are true UTF-8 byte offsets (TextEncoder),
// not JS string indices — the two diverge on any book containing
// multi-byte characters (accented letters, etc.).
static const char kReadScript[] PROGMEM =
  "<script>(function(){"
  "var bookId=parseInt(document.getElementById('bvData').dataset.id,10);"
  "var elText=document.getElementById('bvText'),elQ=document.getElementById('bvFind'),"
  "elReady=document.getElementById('bvReady'),elLoading=document.getElementById('bvLoading'),"
  "elBrowse=document.getElementById('bvBrowseUi'),elEditingTitle=document.getElementById('bvEditingTitle'),"
  "elBtnAll=document.getElementById('bvFindBtn'),elNext=document.getElementById('bvFindNext'),"
  "elPrev=document.getElementById('bvFindPrev'),elStat=document.getElementById('bvFindStat'),"
  "elEditor=document.getElementById('bvEditor'),elEdit=document.getElementById('bvEditBtn'),elSearchPanel=document.querySelector('.bv-search-panel'),"
  "elSave=document.getElementById('bvSaveBtn'),elCancel=document.getElementById('bvCancelBtn'),"
  "elWarning=document.getElementById('bvEditWarning'),"
  "elJump=document.getElementById('bvJumpBtn'),elJumpStat=document.getElementById('bvJumpStat');"
  "var rawText='',hits=[],curHit=-1,normCache=null;"
  "function setEdit(on){elBrowse.hidden=on;elEditingTitle.hidden=!on;elSearchPanel.hidden=on;elEditor.hidden=!on;elText.hidden=on;elSave.hidden=!on;elCancel.hidden=!on;elEdit.hidden=on;elWarning.hidden=!on;if(on){elEditor.value=rawText;setTimeout(keepEditorVisible,80);}}"
  "function keepEditorVisible(){if(elEditor.hidden||document.activeElement!==elEditor)return;var top=elEditor.getBoundingClientRect().top+window.scrollY-12;window.scrollTo({top:Math.max(0,top),behavior:'smooth'});}"
  "function esc(s){return s.replace(/[&<>]/g,function(c){return c==='&'?'&amp;':c==='<'?'&lt;':'&gt;'});}"
  "var TYPO={'‘':\"'\",'’':\"'\",'‚':\"'\",'‛':\"'\",'“':'\"','”':'\"','„':'\"','‟':'\"','‹':'\"','›':'\"','«':'\"','»':'\"','–':'-','—':'-','―':'-','…':'...','\\u00A0':' '};"
  "function normTextMap(s){"
  "var n='',map=[],pend=false;"
  "for(var i=0;i<s.length;i++){"
  "var t=TYPO[s[i]]||s[i];"
  "for(var k=0;k<t.length;k++){"
  "var ch=t[k];"
  "if(ch===' '||ch==='\\n'||ch==='\\r'||ch==='\\t'){if(n.length)pend=true;continue;}"
  "if(pend){n+=' ';map.push(i);pend=false;}"
  "n+=ch.toLowerCase();map.push(i);"
  "}"
  "}"
  "return{n:n,map:map};"
  "}"
  "function findMatches(raw,q,cache){"
  "var qn=normTextMap(q).n;"
  "if(!qn)return{hits:[],cache:cache,empty:true};"
  "if(!cache)cache=normTextMap(raw);"
  "var hits=[],i=0;"
  "while(true){"
  "var p=cache.n.indexOf(qn,i);"
  "if(p<0)break;"
  "hits.push({start:cache.map[p],end:cache.map[p+qn.length-1]+1});"
  "i=p+qn.length;"
  "}"
  "return{hits:hits,cache:cache,empty:false};"
  "}"
  "function utf8Len(s){return new TextEncoder().encode(s).length;}"
  "function render(highlight){"
  "if(!highlight){elText.textContent=rawText;return;}"
  "var html='',last=0;"
  "for(var i=0;i<hits.length;i++){"
  "var h=hits[i];"
  "html+=esc(rawText.slice(last,h.start));"
  "html+='<span class=\"find-hit'+(i===curHit?' find-cur':'')+'\" data-i=\"'+i+'\">'+esc(rawText.slice(h.start,h.end))+'</span>';"
  "last=h.end;"
  "}"
  "html+=esc(rawText.slice(last));"
  "elText.innerHTML=html;"
  "}"
  "function gotoHit(i){"
  "if(hits.length===0){elStat.textContent='No matches';return;}"
  "curHit=((i%hits.length)+hits.length)%hits.length;"
  "render(true);"
  "var n=elText.querySelector('.find-cur');"
  "if(n)n.scrollIntoView({block:'center',behavior:'smooth'});"
  "elStat.textContent='Match '+(curHit+1)+' of '+hits.length+'  (byte '+utf8Len(rawText.slice(0,hits[curHit].start))+')';"
  "}"
  "function search(){"
  "var r=findMatches(rawText,elQ.value,normCache);"
  "normCache=r.cache||normCache;"
  "if(r.empty){hits=[];curHit=-1;render(false);elStat.textContent='Enter a phrase to find.';return;}"
  "hits=r.hits;curHit=hits.length>0?0:-1;render(true);"
  "if(hits.length===0)elStat.textContent='No matches';"
  "else gotoHit(0);"
  "}"
  "elBtnAll.addEventListener('click',search);"
  "elQ.addEventListener('keydown',function(e){if(e.key==='Enter'){e.preventDefault();search();}});"
  "elNext.addEventListener('click',function(){if(hits.length)gotoHit(curHit+1);});"
  "elPrev.addEventListener('click',function(){if(hits.length)gotoHit(curHit-1);});"
  "elText.addEventListener('click',function(e){var t=e.target;if(t.classList&&t.classList.contains('find-hit')){var idx=parseInt(t.dataset.i,10);if(!isNaN(idx))gotoHit(idx);}});"
  "elEditor.addEventListener('focus',function(){setTimeout(keepEditorVisible,120);});"
  "if(window.visualViewport)window.visualViewport.addEventListener('resize',function(){if(document.activeElement===elEditor)setTimeout(keepEditorVisible,80);});"
  "elEdit.addEventListener('click',function(){setEdit(true);});"
  "elCancel.addEventListener('click',function(){var returnTo=new URLSearchParams(window.location.search).get('return');window.paginottoDirty=false;if(returnTo==='/upload-review')window.location.href='/upload-review';else setEdit(false);});"
  "elEditor.addEventListener('input',function(){window.paginottoDirty=true;});"
  "elSave.addEventListener('click',function(){if(!confirm('WARNING: this replaces the book on the device immediately. There is no device backup. Download the current TXT first if needed. Continue?'))return;var editedBytes=new TextEncoder().encode(elEditor.value).length;var fd=new FormData();fd.append('file',new Blob([elEditor.value],{type:'text/plain;charset=utf-8'}),'edited-book.txt');elSave.disabled=true;elSave.textContent='Saving...';var returnTo=new URLSearchParams(window.location.search).get('return');var target='/edit-book?id='+encodeURIComponent(bookId)+'&size='+editedBytes+(returnTo==='/upload-review'?'&return=%2Fupload-review':'');fetch(target,{method:'POST',body:fd}).then(function(r){if(!r.ok)return r.text().then(function(t){throw Error(t||('HTTP '+r.status));});window.location.href=returnTo==='/upload-review'?'/upload-review?notice=edited':r.url;}).catch(function(e){elJumpStat.textContent='Save failed: '+(e&&e.message?e.message:'error');elSave.disabled=false;elSave.textContent='Save & repaginate';});});"
  "elJump.addEventListener('click',function(){if(curHit<0){elJumpStat.textContent='Find something first.';return;}"
  "var hitStart=hits[curHit].start,off=utf8Len(rawText.slice(0,hitStart));var anchorStart=Math.max(0,hitStart-64),anchorEnd=Math.min(rawText.length,hitStart+128),anchor=rawText.slice(anchorStart,anchorEnd),anchorAt=utf8Len(rawText.slice(anchorStart,hitStart));var fd=new FormData();fd.append('id',String(bookId));fd.append('offset',String(off));fd.append('anchor',anchor);fd.append('anchorAt',String(anchorAt));"
  "elJumpStat.textContent='Saving...';elJump.disabled=true;"
  "fetch('/jumpoffset',{method:'POST',body:fd,redirect:'follow'}).then(function(r){"
  "elJumpStat.textContent=r.ok?('Saved. Open the book on the device to jump to byte '+off+'.'):('Save failed: HTTP '+r.status);"
  "}).catch(function(e){elJumpStat.textContent='Save failed: '+(e&&e.message?e.message:'error');"
  "}).finally(function(){elJump.disabled=false;});"
  "});"
  "fetch('/readbook-text?id='+encodeURIComponent(bookId)).then(function(r){return r.text();}).then(function(t){"
  "rawText=t;normCache=null;render(false);elStat.textContent='Loaded '+rawText.length+' chars. Enter a phrase to find.';elLoading.hidden=true;elReady.hidden=false;"
  "if(document.getElementById('bvData').dataset.editOnly==='1')setEdit(true);"
  "}).catch(function(){elLoading.textContent='Could not load book text.';});"
  "})();</script>";

static void handleReadView() {
  int id = requireBookId();
  if (id < 0) return;

  const bool editOnly = server.uri() == "/edit-text";

  String path = String(g_library.books[id].path);
  int savedPage = savedPageForBookPath(path) + 1;
  if (savedPage < 1) savedPage = 1;

  String out = webPageStart(
    editOnly ? String("Edit text") : String(D_WEB_READ_TITLE),
    editOnly ? String("Edit the text of this book.") : String(D_WEB_READ_SUBTITLE),
    webNav("books")
  );
  if (server.arg("bookmark") == "restored") {
    out += "<div class='banner-ok'>Reading position restored after text edit.</div>";
  } else if (server.arg("bookmark") == "reset") {
    out += "<div class='banner-warn'>The previous reading position could not be matched after editing; reading position reset to the beginning.</div>";
  }
  out.reserve(out.length() + 6000);
  out += "<div id='bvLoading' class='bv-text'>" D_WEB_READ_LOADING "</div>";
  out += "<div id='bvReady' hidden>";
  out += "<div class='card'>";
  out += "<div id='bvBrowseUi'>";
  out += "<div class='actions bv-top-actions compact'>";
  out += webBackLink("/books", D_WEB_BACK_TO_BOOKS);
  out += "<button type='button' class='btn secondary small' id='bvEditBtn'>" D_WEB_READ_EDIT_TEXT "</button>";
  out += "</div>";

  out += "<h2>";
  out += htmlEscape(String(g_library.books[id].name));
  out += "</h2><div class='meta'>";
  out += String((int)g_library.books[id].size);
  out += " " D_WEB_READ_BYTES_LABEL " (";
  out += String((float)g_library.books[id].size / 1000.0f, 1);
  out += " " D_WEB_READ_KB_LABEL ") | " D_WEB_READ_CURRENT_PAGE_LABEL " ";
  out += String(savedPage);
  out += "</div>";

  // Jump-by-page form (uses the existing /jumppage route).
  out += "<div class='bv-page-jump compact'>"
         "<form method='POST' action='/jumppage' class='bv-find bv-page-form'>"
         "<input type='hidden' name='id' value='" + String(id) + "'>"
         "<input type='text' name='page' inputmode='numeric' placeholder='" D_WEB_READ_PAGE_PLACEHOLDER "' value='" + String(savedPage) + "'>"
         "<button type='submit' class='btn small'>" D_WEB_READ_JUMP_PAGE "</button>"
         "<span class='muted small bv-page-hint'>" D_WEB_READ_JUMP_HINT "</span>"
         "</form></div>";

  out += "</div>";
  out += "<h2 id='bvEditingTitle' hidden>Editing book text</h2>";

  out += "<div id='bvData' data-id='" + String(id) + "' data-edit-only='" + String(editOnly ? "1" : "0") + "'></div>";
  // Keep find controls immediately next to the text. The sticky panel prevents
  // the controls from disappearing when the browser scrolls to a match.
  out += "<div class='bv-search-panel'>"
         "<div class='bv-find bv-search-row'>"
         "<input id='bvFind' type='search' placeholder='" D_WEB_READ_FIND_PLACEHOLDER "' autocomplete='off'>"
         "<div class='bv-find-actions'>"
         "<div class='bv-find-stack'>"
         "<button type='button' class='btn small' id='bvFindBtn'>" D_WEB_READ_FIND_ALL "</button>"
         "<button type='button' class='btn small' id='bvJumpBtn'>Jump here</button>"
         "</div>"
         "<div class='bv-find-stack'>"
         "<button type='button' class='btn secondary small' id='bvFindPrev'>&#9650; " D_WEB_READ_FIND_PREV "</button>"
         "<button type='button' class='btn secondary small' id='bvFindNext'>&#9660; " D_WEB_READ_FIND_NEXT "</button>"
         "</div>"
         "</div>"
         "</div>"
         "<div class='bv-status' id='bvFindStat'></div>"
         "<div class='bv-status' id='bvJumpStat'></div>"
         "</div>";
  out += "<div id='bvText' class='bv-text'></div>";
  out += "<textarea id='bvEditor' class='bv-editor' hidden spellcheck='false'></textarea>";
  out += "<div id='bvEditWarning' class='banner-warn' hidden>" D_WEB_READ_EDIT_WARNING " <a class='link' href='/download-book?id=" + String(id) + "' download>" D_WEB_READ_DOWNLOAD_BACKUP "</a></div>";
  out += "<div class='bv-edit-actions'><button type='button' class='btn small' id='bvSaveBtn' hidden>Save &amp; repaginate</button><button type='button' class='btn secondary small' id='bvCancelBtn' hidden>Cancel editing</button></div>";
  out += "</div>";
  out += "</div>";

  out += FPSTR(kReadScript);
  out += webPageEnd();
  server.send(200, "text/html; charset=utf-8", out);
}

void registerFindRoutes() {
  server.on("/read",          HTTP_GET,  handleReadView);
  server.on("/edit-text",     HTTP_GET,  handleReadView);
  server.on("/readbook-text", HTTP_GET,  handleReadbookText);
  server.on("/jumpoffset",    HTTP_POST, handleJumpOffset);
  server.on("/edit-book",     HTTP_POST, handleEditBookDone, handleEditBookStream);
}
