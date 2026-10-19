#include "src/web/files.h"

#include <WiFi.h>

#include "src/config.h"
#include "src/state.h"
#include "src/pure/hashing.h"
#include "src/pure/paths.h"
#include "src/storage/book_metadata.h"
#include "src/storage/fs_util.h"
#include "src/storage/library.h"
#include "src/storage/preferences_store.h"
#include "src/ui/text.h"            // pageOffsetForPage
#include "src/web/chrome.h"

// ============================================================================
//  /               — redirect to book/folder browser
//  /books          — book/folder browser
//  /download-book  — GET: stream book .txt as attachment (?id=N)
//  /del            — POST: delete book
//  /mkdir          — POST: create folder
//  /rmdir          — POST: delete (empty) folder
//  /move           — POST: move book to a different folder
//  /jumppage       — POST: set the page that opens next on device
// ============================================================================

static void handleDownloadBook() {
  if (!server.hasArg("id")) {
    server.send(400, "text/plain; charset=utf-8", D_WEB_ERR_MISSING_ID);
    return;
  }
  int id = server.arg("id").toInt();
  if (id < 0 || id >= g_library.bookCount) {
    server.send(400, "text/plain; charset=utf-8", D_WEB_ERR_BAD_ID);
    return;
  }

  String path = String(g_library.books[id].path);
  File f = FS.open(path, "r");
  if (!f) {
    server.send(404, "text/plain; charset=utf-8", "Not found");
    return;
  }

  String filename = lastPathComponent(path);
  server.setContentLength(f.size());
  server.sendHeader("Content-Disposition",
                    "attachment; filename=\"" + filename + "\"");
  server.send(200, "text/plain; charset=utf-8", "");

  WiFiClient client = server.client();
  uint8_t buf[512];
  while (f.available()) {
    size_t n = f.read(buf, sizeof(buf));
    if (n > 0) client.write(buf, n);
  }
  f.close();
}

static void handleRoot() {
  server.sendHeader("Location", "/books");
  server.send(302, "text/plain", "");
}

// EPUB conversion deliberately lives entirely in the browser.  The ESP32
// continues to receive only the compact UTF-8 text stream handled by /upload.
// Modern offline browsers provide raw-DEFLATE through DecompressionStream, so
// this avoids adding a ZIP/XML parser (and its ROM cost) to firmware.
static const char kEpubUploadScript[] = R"EPUBJS(
<script>
(()=>{
const form=document.getElementById('bookUpload'),input=document.getElementById('bookFile'),status=document.getElementById('bookUploadStatus'),button=document.getElementById('bookUploadButton');
let converted=null,choice=0;
const fail=m=>{converted=null;status.textContent='EPUB conversion failed: '+m;button.disabled=true;};
const u16=(a,o)=>a[o]|a[o+1]<<8, u32=(a,o)=>(u16(a,o)|u16(a,o+2)<<16)>>>0;
const name=(a,o,n)=>new TextDecoder('utf-8').decode(a.subarray(o,o+n));
const badXml=d=>d.getElementsByTagName('parsererror').length!==0;
const xml=(s,label)=>{const d=new DOMParser().parseFromString(s,'application/xml');if(badXml(d))throw Error('invalid '+label);return d;};
const path=(base,href)=>{try{const u=new URL(href,'https://epub.invalid/'+base);if(u.origin!=='https://epub.invalid')throw Error();return decodeURIComponent(u.pathname.replace(/^\//,''));}catch(_){throw Error('bad EPUB path');}};
async function zip(file){
 if(file.size>64*1024*1024)throw Error('file exceeds 64 MiB browser conversion limit');
 if(!('DecompressionStream'in window))throw Error('browser lacks offline EPUB decompression support');
 const a=new Uint8Array(await file.arrayBuffer());let end=-1;
 for(let i=a.length-22;i>=Math.max(0,a.length-65557);i--)if(u32(a,i)===0x06054b50){end=i;break;}
 if(end<0)throw Error('ZIP end record missing');
 if(u16(a,end+4)||u16(a,end+6))throw Error('multi-disk EPUB unsupported');
 const count=u16(a,end+10),start=u32(a,end+16),size=u32(a,end+12);if(start+size>a.length)throw Error('bad ZIP directory');
 const entries=new Map();let p=start;
 for(let i=0;i<count;i++){
  if(p+46>a.length||u32(a,p)!==0x02014b50)throw Error('bad ZIP entry');
  const flags=u16(a,p+8),method=u16(a,p+10),packed=u32(a,p+20),plain=u32(a,p+24),nl=u16(a,p+28),xl=u16(a,p+30),cl=u16(a,p+32),off=u32(a,p+42),n=name(a,p+46,nl);
  if(flags&1)throw Error('encrypted EPUB unsupported');if(off+30>a.length||u32(a,off)!==0x04034b50)throw Error('bad ZIP data');
  const data=off+30+u16(a,off+26)+u16(a,off+28);if(data+packed>a.length)throw Error('truncated ZIP entry');
  entries.set(n,{method,packed,plain,data});p+=46+nl+xl+cl;
 }
 return async n=>{const e=entries.get(n);if(!e)throw Error('missing '+n);if(e.plain>64*1024*1024)throw Error('EPUB entry exceeds 64 MiB limit');let out;
  if(e.method===0)out=a.slice(e.data,e.data+e.packed);else if(e.method===8)out=new Uint8Array(await new Response(new Blob([a.slice(e.data,e.data+e.packed)]).stream().pipeThrough(new DecompressionStream('deflate-raw'))).arrayBuffer());else throw Error('unsupported ZIP compression');
  if(out.length!==e.plain)throw Error('corrupt ZIP entry');return new TextDecoder('utf-8',{fatal:true}).decode(out);
 };
}
function chapter(source){
 let d=new DOMParser().parseFromString(source,'application/xhtml+xml');if(badXml(d))d=new DOMParser().parseFromString(source,'text/html');
 const root=d.body||d.documentElement,parts=[],block=new Set(['address','article','aside','blockquote','br','div','dl','dt','dd','figcaption','figure','footer','h1','h2','h3','h4','h5','h6','header','hr','li','main','nav','ol','p','pre','section','table','td','th','tr','ul']);
 const walk=n=>{if(n.nodeType===3){parts.push(n.nodeValue);return;}if(n.nodeType!==1)return;const tag=n.localName.toLowerCase();if(['head','script','style','noscript','template','svg','math','audio','video','canvas','iframe','object'].includes(tag))return;if(block.has(tag))parts.push('\n');for(const c of n.childNodes)walk(c);if(block.has(tag))parts.push('\n');};
 walk(root);return parts.join('').replace(/\u00a0/g,' ').replace(/[ \t\f\v]+/g,' ').replace(/ *\n */g,'\n').replace(/\n{3,}/g,'\n\n').trim();
}
async function convert(file){
 const get=await zip(file),container=xml(await get('META-INF/container.xml'),'container.xml'),root=container.getElementsByTagNameNS('*','rootfile')[0];if(!root)throw Error('package document missing');
 const opfPath=root.getAttribute('full-path');if(!opfPath)throw Error('package path missing');const opf=xml(await get(opfPath),'package document'),base=opfPath.replace(/[^/]*$/,'');
 const manifest=new Map();for(const item of opf.getElementsByTagNameNS('*','item'))manifest.set(item.getAttribute('id'),item);
 const sections=[];for(const ref of opf.getElementsByTagNameNS('*','itemref')){const item=manifest.get(ref.getAttribute('idref'));if(!item)throw Error('spine item missing from manifest');const href=item.getAttribute('href');if(!href)throw Error('spine item path missing');const text=chapter(await get(path(base,href)));if(text)sections.push(text);}
 if(!sections.length)throw Error('no readable XHTML/HTML spine content');const text=sections.join('\n\n').replace(/\n{3,}/g,'\n\n');if(!text)throw Error('empty EPUB');
 const filename=(file.name.replace(/\.epub$/i,'')||'book')+'.txt';return new File([text],filename,{type:'text/plain;charset=utf-8'});
}
input.addEventListener('change',async()=>{const file=input.files[0],selected=++choice;converted=null;button.disabled=false;if(!file){status.textContent='';return;}if(!/\.epub$/i.test(file.name)){status.textContent='';return;}status.textContent='Converting EPUB…';button.disabled=true;try{const result=await convert(file);if(selected!==choice)return;converted=result;status.textContent='Ready: '+converted.name+' ('+converted.size.toLocaleString()+' bytes)';button.disabled=false;}catch(e){if(selected===choice)fail(e&&e.message?e.message:'invalid EPUB');}});
form.addEventListener('submit',async e=>{if(!converted)return;e.preventDefault();button.disabled=true;status.textContent='Uploading '+converted.name+'…';try{const fd=new FormData();fd.append('file',converted,converted.name);const r=await fetch('/upload',{method:'POST',body:fd});const body=await r.text();if(!r.ok)throw Error(body||('HTTP '+r.status));document.open();document.write(body);document.close();}catch(err){status.textContent='Upload failed: '+(err&&err.message?err.message:'error');button.disabled=false;}});
})();
</script>
)EPUBJS";

static void handleFiles() {
  String out = webPageStart(
    D_WEB_BOOKS_HEADING,
    D_WEB_BOOKS_SUBTITLE,
    webNav("books")
  );

  out += storageCardHtml();

  out += "<div class='card'><p class='muted'>" D_WEB_BOOKS_PATHS_DESC "</p></div>";

  out +=
    "<div class='card'><h2>" D_WEB_UPLOAD_BOOK_HEADING "</h2>"
    "<p class='muted'>" D_WEB_UPLOAD_BOOK_DESC "</p>"
    "<form id='bookUpload' method='POST' action='/upload' enctype='multipart/form-data' accept-charset='UTF-8' style='margin-top:14px'>"
    "<input id='bookFile' type='file' name='file' accept='.txt,text/plain,.epub,application/epub+zip' required>"
    "<p id='bookUploadStatus' class='muted' aria-live='polite'></p>"
    "<div class='actions'><button id='bookUploadButton' type='submit'>" D_WEB_UPLOAD_BOOK_BUTTON "</button></div>"
    "</form>";
  out += kEpubUploadScript;
  out += "</div>";

  out +=
    "<div class='card'><h2>" D_WEB_CREATE_FOLDER_HEADING "</h2>"
    "<form method='POST' action='/mkdir' class='stack' accept-charset='UTF-8' style='margin-top:12px'>"
    "<input type='text' name='folder' placeholder='" D_WEB_CREATE_FOLDER_PLACEHOLDER "' maxlength='64'>"
    "<div class='actions'><button type='submit'>" D_WEB_CREATE_FOLDER_BUTTON "</button><span class='muted'>" D_WEB_CREATE_FOLDER_HINT "</span></div>"
    "</form></div>";

  out += "<div class='card'><h2>" D_WEB_FOLDERS_HEADING "</h2>";
  if (g_library.folderCount == 0) {
    out += "<p class='muted'>" D_WEB_NO_FOLDERS "</p>";
  } else {
    out += "<ul class='list'>";
    for (int i = 0; i < g_library.folderCount; i++) {
      out += "<li><div class='row'><div><span class='pill'>";
      out += htmlEscape(prettyRelativeLabel(String(g_library.folders[i])));
      out += "</span></div><div><form method='POST' action='/rmdir' style='display:inline'>";
      out += "<input type='hidden' name='folder' value='";
      out += htmlEscape(g_library.folders[i]);
      out += "'><button type='submit' class='btn secondary' onclick=\"return confirm('" D_WEB_CONFIRM_DELETE_FOLDER "')\">" D_WEB_DELETE_BUTTON "</button></form></div></div></li>";
    }
    out += "</ul>";
  }
  out += "</div>";

  out += "<div class='card'><h2>" D_WEB_LIBRARY_BOOKS_HEADING "</h2>";
  if (g_library.bookCount   >= MAX_BOOKS)   out += "<p style='color:#b91c1c;font-weight:600'>" D_WEB_LIBRARY_FULL_WARN "</p>";
  if (g_library.folderCount >= MAX_FOLDERS) out += "<p style='color:#b91c1c;font-weight:600'>" D_WEB_FOLDER_LIMIT_WARN "</p>";

  if (g_library.bookCount == 0) {
    out += "<p class='muted'>" D_WEB_NO_BOOKS_UPLOADED "</p>";
  } else {
    out += "<ul class='list book-list'>";
    for (int i = 0; i < g_library.bookCount; i++) {
      String filePath = String(g_library.books[i].path);
      String folderLabel = g_library.books[i].folder[0] ? prettyRelativeLabel(g_library.books[i].folder) : String(D_WEB_BOOK_ROOT);
      int savedPage = savedPageForBookPath(filePath) + 1;
      if (savedPage < 1) savedPage = 1;

      out += "<li class='book-item'><div class='book-row'><div class='book-main'><h3 class='book-title'>";
      out += htmlEscape(String(g_library.books[i].name));
      out += "</h3><div class='meta'>";
      out += String((int)g_library.books[i].size);
      out += D_WEB_BOOK_BYTES_LABEL D_WEB_BOOK_FOLDER_LABEL;
      out += htmlEscape(folderLabel);
      out += D_WEB_BOOK_CURRENT_PAGE;
      out += String(savedPage);
      out += "</div>";

      out += "<form method='POST' action='/jumppage' class='stack small' accept-charset='UTF-8' style='margin-top:10px'>";
      out += "<input type='hidden' name='id' value='" + String(i) + "'>";
      out += "<div class='row' style='align-items:end;gap:10px'><div style='flex:1'><input type='text' name='page' value='" + String(savedPage) + "' inputmode='numeric' placeholder='" D_WEB_PAGE_PLACEHOLDER "'></div><div><button type='submit'>" D_WEB_JUMP_BUTTON "</button></div></div>";
      out += "<div class='muted'>" D_WEB_JUMP_HINT "<br><span class='muted'>" D_WEB_JUMP_HINT2 "</span></div></form>";

      out += "<div class='actions small' style='margin-top:8px'><a class='btn secondary small' href='/read?id=" + String(i) + "' style='padding:6px 10px;font-size:13px'>" D_WEB_READ_AND_FIND_LINK "</a></div>";

      out += "<form method='POST' action='/move' class='stack small' accept-charset='UTF-8' style='margin-top:10px'>";
      out += "<input type='hidden' name='id' value='" + String(i) + "'>";
      out += "<input type='text' name='folder' value='" + htmlEscape(String(g_library.books[i].folder)) + "' placeholder='" D_WEB_MOVE_PLACEHOLDER "' maxlength='64'>";
      out += "<div class='actions'><button type='submit'>" D_WEB_MOVE_BUTTON "</button><span class='muted'>" D_WEB_MOVE_HINT "</span></div></form>";
      out += "<form method='POST' action='/rename' class='stack small' accept-charset='UTF-8' style='margin-top:10px'>";
      out += "<input type='hidden' name='id' value='" + String(i) + "'>";
      out += "<input type='text' name='name' value='" + htmlEscape(stripTxtExt(lastPathComponent(filePath))) + "' placeholder='" D_WEB_RENAME_PLACEHOLDER "' maxlength='79' required autocomplete='off' autocapitalize='off' spellcheck='false'>";
      out += "<div class='actions'><button type='submit'>" D_WEB_RENAME_BUTTON "</button><span class='muted'>" D_WEB_RENAME_HINT "</span></div></form></div>";
      out += "<div class='book-tools'>";
      out += "<a class='btn secondary' href='/download-book?id=" + String(i) + "' download>" D_WEB_DOWNLOAD_BUTTON "</a>";
      out += "<form method='POST' action='/del' style='margin:0'><input type='hidden' name='id' value='" + String(i) + "'>";
      out += "<button type='submit' class='btn secondary' onclick=\"return confirm('" D_WEB_CONFIRM_DELETE_FILE "')\">" D_WEB_DELETE_BUTTON "</button></form>";
      out += "</div></div></li>";
    }
    out += "</ul>";
  }

  out += "</div>";

  out += webPageEnd();
  server.send(200, "text/html; charset=utf-8", out);
}

static void handleDelete() {
  if (!server.hasArg("id")) {
    server.send(400, "text/plain; charset=utf-8", D_WEB_ERR_MISSING_ID);
    return;
  }

  int id = server.arg("id").toInt();
  if (id < 0 || id >= g_library.bookCount) {
    server.send(400, "text/plain; charset=utf-8", D_WEB_ERR_BAD_ID);
    return;
  }

  // Library entry already cleared g_bookview; no book is "current" here.
  String path = String(g_library.books[id].path);
  if (FS.exists(path)) FS.remove(path);
  deleteBookMetadata(path);
  loadBooks();   // refresh catalog so the next read sees the deletion

  server.sendHeader("Location", "/books");
  server.send(302, "text/plain", "");
}

static void handleCreateFolder() {
  ensureBooksDir();
  if (!server.hasArg("folder")) {
    server.send(400, "text/plain; charset=utf-8", D_WEB_ERR_MISSING_FOLDER);
    return;
  }

  String folder = sanitizeFolderInput(server.arg("folder"));
  if (folder.length() == 0) {
    server.send(400, "text/plain; charset=utf-8", D_WEB_ERR_BAD_FOLDER);
    return;
  }
  if (g_library.folderCount >= MAX_FOLDERS) {
    server.send(409, "text/plain; charset=utf-8", D_WEB_ERR_FOLDER_LIMIT);
    return;
  }

  String fullPath = "/books/" + folder;
  if (!ensureDirRecursive(fullPath)) {
    server.send(500, "text/plain; charset=utf-8", D_WEB_ERR_MKDIR_FAILED);
    return;
  }
  loadBooks();   // refresh catalog so the new folder appears

  server.sendHeader("Location", "/books");
  server.send(302, "text/plain", "");
}

static void handleDeleteFolder() {
  if (!server.hasArg("folder")) {
    server.send(400, "text/plain; charset=utf-8", D_WEB_ERR_MISSING_FOLDER);
    return;
  }

  String folder = sanitizeFolderInput(server.arg("folder"));
  if (folder.length() == 0) {
    server.send(400, "text/plain; charset=utf-8", D_WEB_ERR_BAD_FOLDER);
    return;
  }

  String fullPath = "/books/" + folder;
  if (!FS.exists(fullPath)) {
    server.send(404, "text/plain; charset=utf-8", D_WEB_ERR_FOLDER_NOT_FOUND);
    return;
  }
  if (!isDirEmpty(fullPath)) {
    server.send(409, "text/plain; charset=utf-8", D_WEB_ERR_FOLDER_NOT_EMPTY);
    return;
  }
  if (!FS.rmdir(fullPath)) {
    server.send(500, "text/plain; charset=utf-8", D_WEB_ERR_DELETE_FAILED);
    return;
  }
  loadBooks();   // refresh catalog so the folder disappears

  server.sendHeader("Location", "/books");
  server.send(302, "text/plain", "");
}

static void handleMoveBook() {
  if (!server.hasArg("id")) {
    server.send(400, "text/plain; charset=utf-8", D_WEB_ERR_MISSING_ID);
    return;
  }

  int id = server.arg("id").toInt();
  if (id < 0 || id >= g_library.bookCount) {
    server.send(400, "text/plain; charset=utf-8", D_WEB_ERR_BAD_ID);
    return;
  }

  String oldPath = String(g_library.books[id].path);
  String folder = sanitizeFolderInput(server.arg("folder"));
  String destDir = (folder.length() == 0) ? String("/books") : String("/books/") + folder;
  String newPath = destDir + "/" + lastPathComponent(oldPath);
  if (newPath.length() >= sizeof(g_library.books[id].path)) {
    server.send(400, "text/plain; charset=utf-8", D_WEB_ERR_BAD_NAME);
    return;
  }

  if (!ensureDirRecursive(destDir)) {
    server.send(500, "text/plain; charset=utf-8", D_WEB_ERR_FOLDER_CREATE_FAILED);
    return;
  }

  if (newPath == oldPath) {
    server.sendHeader("Location", "/books");
    server.send(302, "text/plain", "");
    return;
  }
  if (FS.exists(newPath)) {
    server.send(409, "text/plain; charset=utf-8", D_WEB_ERR_DEST_EXISTS);
    return;
  }

  // Library entry already cleared g_bookview; no book is "current" here.
  if (!FS.rename(oldPath, newPath)) {
    server.send(500, "text/plain; charset=utf-8", D_WEB_ERR_MOVE_FAILED);
    return;
  }

  migrateBookMetadata(oldPath, newPath);   // NVS keys + page-cache file
  loadBooks();   // refresh catalog so the moved book shows in its new folder

  server.sendHeader("Location", "/books");
  server.send(302, "text/plain", "");
}

static void handleRenameBook() {
  if (!server.hasArg("id")) {
    server.send(400, "text/plain; charset=utf-8", D_WEB_ERR_MISSING_ID);
    return;
  }
  if (!server.hasArg("name")) {
    server.send(400, "text/plain; charset=utf-8", D_WEB_ERR_MISSING_NAME);
    return;
  }

  int id = server.arg("id").toInt();
  if (id < 0 || id >= g_library.bookCount) {
    server.send(400, "text/plain; charset=utf-8", D_WEB_ERR_BAD_ID);
    return;
  }

  String oldPath = String(g_library.books[id].path);
  String folder = String(g_library.books[id].folder);
  const size_t prefixBytes = (folder.length() == 0 ? sizeof("/books/") - 1
                                                    : sizeof("/books/") - 1 + folder.length() + 1);
  const size_t maxLeafBytes = (sizeof(g_library.books[id].path) - 1 > prefixBytes)
                              ? sizeof(g_library.books[id].path) - 1 - prefixBytes : 0;
  String leaf = sanitizeUploadedFilename(server.arg("name"), maxLeafBytes);
  String newPath = (folder.length() == 0 ? String("/books/") : String("/books/") + folder + "/") + leaf;
  if (newPath.length() >= sizeof(g_library.books[id].path)) {
    server.send(400, "text/plain; charset=utf-8", D_WEB_ERR_BAD_NAME);
    return;
  }

  if (newPath == oldPath) {
    server.sendHeader("Location", "/books");
    server.send(302, "text/plain", "");
    return;
  }
  if (FS.exists(newPath)) {
    server.send(409, "text/plain; charset=utf-8", D_WEB_ERR_DEST_EXISTS);
    return;
  }
  if (!FS.rename(oldPath, newPath)) {
    server.send(500, "text/plain; charset=utf-8", D_WEB_ERR_RENAME_FAILED);
    return;
  }

  migrateBookMetadata(oldPath, newPath);
  loadBooks();
  server.sendHeader("Location", "/books");
  server.send(302, "text/plain", "");
}

static void handleJumpPageWeb() {
  if (!server.hasArg("id") || !server.hasArg("page")) {
    server.send(400, "text/plain; charset=utf-8", D_WEB_ERR_MISSING_ID_PAGE);
    return;
  }

  int id = server.arg("id").toInt();
  if (id < 0 || id >= g_library.bookCount) {
    server.send(400, "text/plain; charset=utf-8", D_WEB_ERR_BAD_ID);
    return;
  }

  int page = server.arg("page").toInt();
  if (page < 1) page = 1;
  int zeroBasedPage = page - 1;

  // Library entry already cleared g_bookview. We write both the page number
  // (display hint) and the byte offset (canonical position used by the
  // reader on next open). Computing the offset requires paginating the
  // book at the current font — file open + page walk. Same cost as the
  // page-offset lookup path.
  String path = String(g_library.books[id].path);
  String key = prefKeyForBook(path);
  PreferencesStore kv(prefs);
  saveSavedPage(kv, key, zeroBasedPage);

  File f = FS.open(path, "r");
  if (f) {
    uint32_t offset = pageOffsetForPage(f, path, zeroBasedPage);
    f.close();
    saveSavedOffset(kv, key, offset);
  }

  server.sendHeader("Location", "/books");
  server.send(302, "text/plain", "");
}

void registerFilesRoutes() {
  server.on("/",             HTTP_GET,  handleRoot);
  server.on("/books",        HTTP_GET,  handleFiles);
  server.on("/download-book", HTTP_GET, handleDownloadBook);
  server.on("/del",      HTTP_POST, handleDelete);          // POST: prevents accidental deletion via browser prefetch
  server.on("/mkdir",    HTTP_POST, handleCreateFolder);
  server.on("/rmdir",    HTTP_POST, handleDeleteFolder);    // POST: destructive
  server.on("/move",     HTTP_POST, handleMoveBook);
  server.on("/rename",   HTTP_POST, handleRenameBook);
  server.on("/jumppage", HTTP_POST, handleJumpPageWeb);
}
