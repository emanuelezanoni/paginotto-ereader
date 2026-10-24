#include "src/web/upload.h"

#include "src/config.h"
#include "src/state.h"
#include "src/pure/paths.h"
#include "src/pure/text_util.h"
#include "src/storage/fs_util.h"
#include "src/storage/book_metadata.h"
#include "src/storage/library.h"
#include "src/web/chrome.h"

// ============================================================================
//  Per-session state. File-static because the route handlers below are the
//  only readers; the screen reaches it only through the reset functions.
// ============================================================================
namespace {
struct BookUpload {
  File   tmpFile;
  String tmpPath;
  String pendingUtf8Tail;
  String finalName;
  String finalPath;
  bool   ok = false;
  String error;
  size_t maxBytes = 0;
  // Cross-chunk state for streaming compactText() during upload, so a
  // whitespace or newline run that spans a chunk boundary collapses
  // correctly. Reset in UPLOAD_FILE_START. See pure/text_util.h.
  bool   compactLastWasSpace = false;
  int    compactNewlineCount = 0;
};

struct SleepUpload {
  File   tmpFile;
  String tmpPath;
  bool   ok = false;
  String error;
};

BookUpload  s_book;
SleepUpload s_sleep;
uint32_t s_lastActivityMs = 0;
}  // namespace

static String uploadReviewPage();

static int pendingBookId() {
  if (!s_book.finalPath.length()) return -1;
  loadBooks();
  for (int i = 0; i < g_library.bookCount; i++) {
    if (String(g_library.books[i].path) == s_book.finalPath) return i;
  }
  return -1;
}

void notifyUploadActivity() { s_lastActivityMs = millis(); }

uint32_t uploadLastActivityMs() { return s_lastActivityMs; }

static void handleWebUiKeepalive() {
  notifyUploadActivity();
  server.send(204, "text/plain", "");
}

void resetBookUpload() {
  if (s_book.tmpFile) s_book.tmpFile.close();
  s_book = BookUpload{};
}

void resetSleepUpload() {
  if (s_sleep.tmpFile) s_sleep.tmpFile.close();
  s_sleep = SleepUpload{};
}

// ============================================================================
//  Book upload
//
//  Two handlers per route: a streaming chunk receiver (`*Stream`) and a final
//  response handler (`*Done`). The stream handler maintains a 4-byte UTF-8
//  tail across chunks so a multibyte codepoint isn't split mid-character;
//  each chunk is normalized + compacted before being written to the temp
//  file. On END, atomic rename into place.
// ============================================================================

static void handleUploadDone() {
  notifyUploadActivity();
  if (!s_book.ok) {
    server.send(400, "text/plain; charset=utf-8",
                s_book.error.length()
                  ? s_book.error
                  : String(D_WEB_UPLOAD_ERR_FALLBACK));
    return;
  }

  loadBooks();   // refresh after the stream handler appended the new book

  s_book.finalPath = "/books/" + s_book.finalName;
  String page = uploadReviewPage();
  server.send(200, "text/html; charset=utf-8", page);
}

static String uploadReviewPage() {
  int id = pendingBookId();
  if (id < 0) return successPage("Upload", "Books", "Upload could not be found.",
                                 "<div class='card'><a class='btn' href='/books'>Back to books</a></div>", "books");

  String folder = String(g_library.books[id].folder);
  String leaf = lastPathComponent(s_book.finalPath);
  String displayName = stripTxtExt(leaf);
  const size_t prefixBytes = folder.length() == 0
                           ? sizeof("/books/") - 1
                           : sizeof("/books/") - 1 + folder.length() + 1;
  const size_t leafBytes = sizeof(g_library.books[id].path) - 1 > prefixBytes
                         ? sizeof(g_library.books[id].path) - 1 - prefixBytes : 4;
  const size_t titleLimit = leafBytes > 4 && leafBytes - 4 < (size_t)MAX_BOOK_TITLE_BYTES
                          ? leafBytes - 4 : (size_t)MAX_BOOK_TITLE_BYTES;
  size_t storedSize = 0;
  File stored = FS.open(s_book.finalPath, "r");
  if (stored) { storedSize = stored.size(); stored.close(); }

  String inner;
  inner.reserve(2400);
  inner += "<div class='card review-card'><div class='review-head'><div class='review-check'>&#10003;</div><div><h2>Book uploaded</h2><p class='muted'>Review the book before saving the final changes.</p></div></div>";
  inner += "<div class='review-summary'><span class='pill'>TXT</span><strong>" + htmlEscape(displayName) + "</strong><span class='muted'>" + humanBytes(storedSize) + "</span></div>";
  inner += "<form method='POST' action='/upload-finalize' class='stack review-form' accept-charset='UTF-8'>"
           "<input type='hidden' name='id' value='" + String(id) + "'>"
           "<label for='uploadedBookName'>File name</label>"
           "<input id='uploadedBookName' type='text' name='name' value='" + htmlEscape(displayName) + "' maxlength='" + String(titleLimit) + "' required autocomplete='off' autocapitalize='off' spellcheck='false' oninput=\"document.getElementById('uploadedBookNameCount').textContent=this.value.length\">"
           "<div class='hint'><span id='uploadedBookNameCount'>" + String(displayName.length()) + "</span>/" + String(titleLimit) + " characters. The .txt extension is added automatically.</div>"
           "<div class='actions review-actions-top'><button type='submit' name='stay' value='1' class='btn secondary'>Rename</button>"
           "<a class='btn secondary' href='/edit-text?id=" + String(id) + "&return=/upload-review'>Edit text</a></div>"
           "<label for='uploadedBookFolder'>Folder</label><select id='uploadedBookFolder' name='folder'><option value=''" +
           (folder.length() == 0 ? String(" selected") : String()) +
           ">Library root</option>";
  for (int folderIndex = 0; folderIndex < g_library.folderCount; folderIndex++) {
    String option = String(g_library.folders[folderIndex]);
    if (option.indexOf('/') >= 0) continue;
    inner += "<option value='" + htmlEscape(option) + "'" +
             (option == folder ? String(" selected") : String()) + ">" +
             htmlEscape(folderLeafLabel(option)) + "</option>";
  }
  inner += "</select><div class='hint'>Choose the destination folder before confirming.</div>"
           "<div class='actions review-actions-bottom'><button type='submit'>Done</button>"
           "<button type='submit' class='btn danger' formaction='/upload-cancel' formnovalidate onclick=\"return confirm('Cancel and delete this uploaded book?')\">Cancel</button></div></form></div>";
  String banner = "Upload complete";
  if (server.hasArg("notice")) {
    String notice = server.arg("notice");
    if (notice == "renamed") banner = "Book name saved";
    else if (notice == "edited") banner = "Book text saved";
  }
  return successPage("Book uploaded", "Review and save", banner, inner, "books");
}

static void handleUploadReview() {
  notifyUploadActivity();
  if (!s_book.ok) {
    server.sendHeader("Location", "/books");
    server.send(302, "text/plain", "");
    return;
  }
  server.send(200, "text/html; charset=utf-8", uploadReviewPage());
}

static bool uploadFolderExists(const String& folder) {
  if (folder.length() == 0) return true;
  for (int i = 0; i < g_library.folderCount; i++) {
    if (String(g_library.folders[i]) == folder) return true;
  }
  return false;
}

static void handleUploadFinalize() {
  notifyUploadActivity();
  int id = pendingBookId();
  if (!s_book.ok || id < 0 || !server.hasArg("name") || !server.hasArg("folder")) {
    server.send(400, "text/plain; charset=utf-8", "invalid upload review");
    return;
  }
  String folder = sanitizeFolderInput(server.arg("folder"));
  if (!uploadFolderExists(folder) || folder.indexOf('/') >= 0) {
    server.send(400, "text/plain; charset=utf-8", "invalid folder");
    return;
  }
  size_t prefixBytes = folder.length() == 0 ? sizeof("/books/") - 1
                                           : sizeof("/books/") - 1 + folder.length() + 1;
  size_t maxLeafBytes = sizeof(g_library.books[id].path) - 1 > prefixBytes
                      ? sizeof(g_library.books[id].path) - 1 - prefixBytes : 0;
  size_t maxTitleBytes = maxLeafBytes > 4 ? maxLeafBytes - 4 : 0;
  String requested = server.arg("name");
  if (requested.length() == 0 || requested.length() > maxTitleBytes
      || requested.length() > MAX_BOOK_TITLE_BYTES) {
    server.send(400, "text/plain; charset=utf-8", "file name is too long or empty");
    return;
  }
  String leaf = sanitizeUploadedFilename(requested, maxLeafBytes);
  String oldPath = s_book.finalPath;
  String newPath = (folder.length() == 0 ? String("/books/") : String("/books/") + folder + "/") + leaf;
  if (newPath.length() >= sizeof(g_library.books[id].path)) {
    server.send(400, "text/plain; charset=utf-8", "file name is too long");
    return;
  }
  if (newPath != oldPath) {
    if (FS.exists(newPath)) {
      server.send(409, "text/plain; charset=utf-8", D_WEB_ERR_DEST_EXISTS);
      return;
    }
    String destDir = folder.length() == 0 ? String("/books") : String("/books/") + folder;
    if (!ensureDirRecursive(destDir) || !FS.rename(oldPath, newPath)) {
      server.send(500, "text/plain; charset=utf-8", "could not save book changes");
      return;
    }
    migrateBookMetadata(oldPath, newPath);
  }
  s_book.finalPath = newPath;
  s_book.finalName = leaf;
  loadBooks();
  if (server.hasArg("stay")) {
    server.sendHeader("Location", String("/upload-review?notice=") +
                                (newPath != oldPath ? "renamed" : "review"));
    server.send(302, "text/plain", "");
    return;
  }
  resetBookUpload();
  server.sendHeader("Location", "/books");
  server.send(302, "text/plain", "");
}

static void handleUploadCancel() {
  notifyUploadActivity();
  if (s_book.ok && s_book.finalPath.length() && FS.exists(s_book.finalPath)) FS.remove(s_book.finalPath);
  resetBookUpload();
  loadBooks();
  server.sendHeader("Location", "/books");
  server.send(302, "text/plain", "");
}

static void handleUploadBookStream() {
  notifyUploadActivity();
  HTTPUpload& up = server.upload();

  if (up.status == UPLOAD_FILE_START) {
    s_book.ok = false;
    s_book.error = "";
    s_book.finalName = "";
    s_book.finalPath = "";
    s_book.pendingUtf8Tail = "";
    s_book.tmpPath = "";
    s_book.compactLastWasSpace = false;
    s_book.compactNewlineCount = 0;

    loadBooks();   // defensive — protects MAX_BOOKS check from a stale catalog
    if (g_library.bookCount >= MAX_BOOKS) {
      s_book.error = D_WEB_ERR_LIBRARY_FULL;
      return;
    }

    size_t freeBytes = fsFreeBytesSafe();
    if (freeBytes < 8192) {
      s_book.error = D_WEB_ERR_NOT_ENOUGH_SPACE;
      return;
    }
    s_book.maxBytes = freeBytes;

    // Keep uploaded stems within the catalog name buffer as well as the
    // filesystem path buffer. The reader can display only 29 title chars;
    // longer names are still preserved up to the safe catalog limit.
    String clean = sanitizeUploadedFilename(up.filename, MAX_BOOK_TITLE_BYTES + 4);
    s_book.finalName = clean;
    s_book.tmpPath   = "/books/" + clean + ".tmp";

    // Never let an upload replace an existing book. Check again before the
    // final rename below in case storage changed during a long upload.
    if (FS.exists("/books/" + clean)) {
      s_book.error = D_WEB_ERR_DEST_EXISTS;
      s_book.tmpPath = "";
      return;
    }

    if (FS.exists(s_book.tmpPath)) FS.remove(s_book.tmpPath);
    s_book.tmpFile = FS.open(s_book.tmpPath, "w");
    if (!s_book.tmpFile) {
      s_book.error = D_WEB_ERR_CANT_CREATE_TEMP_BOOK;
      s_book.tmpPath = "";
    }
  }
  else if (up.status == UPLOAD_FILE_WRITE) {
    if (s_book.error.length() > 0) return;
    if (s_book.tmpFile && up.currentSize > 0) {
      if (s_book.tmpFile.size() + up.currentSize > s_book.maxBytes) {
        s_book.error = D_WEB_ERR_WRITE_FAILED;
        s_book.tmpFile.close();
        if (s_book.tmpPath.length() > 0 && FS.exists(s_book.tmpPath)) FS.remove(s_book.tmpPath);
        return;
      }
      String chunk = s_book.pendingUtf8Tail + String(reinterpret_cast<const char*>(up.buf), up.currentSize);
      int len = (int)chunk.length();
      if (len > 4) {
        s_book.pendingUtf8Tail = chunk.substring(len - 4);
        chunk = chunk.substring(0, len - 4);
      } else {
        s_book.pendingUtf8Tail = chunk;
        chunk = "";
      }
      if (chunk.length() > 0) {
        String cleaned = normalizeTypography(chunk);
        cleaned = compactText(cleaned,
                              &s_book.compactLastWasSpace,
                              &s_book.compactNewlineCount,
                              /*trimTail=*/false);
        size_t cleanedLen = cleaned.length();
        size_t wrote = s_book.tmpFile.print(cleaned);
        if (wrote != cleanedLen) {
          // Short write — out of space or FS error. Abort so a truncated
          // file isn't promoted to a finalized book.
          s_book.error = D_WEB_ERR_WRITE_FAILED;
          s_book.tmpFile.close();
          if (s_book.tmpPath.length() > 0
              && FS.exists(s_book.tmpPath)) {
            FS.remove(s_book.tmpPath);
          }
        }
      }
    }
  }
  else if (up.status == UPLOAD_FILE_END) {
    if (s_book.error.length() > 0 && !s_book.tmpFile) return;
    if (s_book.tmpFile) {
      if (s_book.pendingUtf8Tail.length() > 0) {
        String cleaned = normalizeTypography(s_book.pendingUtf8Tail);
        cleaned = compactText(cleaned,
                              &s_book.compactLastWasSpace,
                              &s_book.compactNewlineCount,
                              /*trimTail=*/true);
        size_t cleanedLen = cleaned.length();
        size_t wrote = s_book.tmpFile.print(cleaned);
        if (wrote != cleanedLen && s_book.error.length() == 0) {
          s_book.error = D_WEB_ERR_WRITE_FAILED;
        }
        s_book.pendingUtf8Tail = "";
      }
      s_book.tmpFile.close();

      if (s_book.error.length() > 0) {
        // Short write or earlier error — never promote a truncated tmp file
        // to a finalized book.
        if (s_book.tmpPath.length() > 0
            && FS.exists(s_book.tmpPath)) {
          FS.remove(s_book.tmpPath);
        }
      } else if (s_book.tmpPath.length() > 0 && up.totalSize > 0) {
        String finalPath = s_book.tmpPath.substring(0, s_book.tmpPath.length() - 4);
        if (FS.exists(finalPath)) {
          FS.remove(s_book.tmpPath);
          s_book.error = D_WEB_ERR_DEST_EXISTS;
        } else if (FS.rename(s_book.tmpPath, finalPath)) {
          s_book.ok = true;
        } else {
          if (FS.exists(s_book.tmpPath)) FS.remove(s_book.tmpPath);
          s_book.error = D_WEB_ERR_FINALIZE_UPLOAD;
        }
      } else {
        if (s_book.tmpPath.length() > 0 && FS.exists(s_book.tmpPath)) FS.remove(s_book.tmpPath);
        s_book.error = D_WEB_ERR_EMPTY_UPLOAD;
      }
      s_book.tmpPath = "";
    } else {
      if (s_book.tmpPath.length() > 0 && FS.exists(s_book.tmpPath)) FS.remove(s_book.tmpPath);
      if (s_book.error.length() == 0) s_book.error = D_WEB_UPLOAD_ERR_FALLBACK;
      s_book.tmpPath = "";
    }
  }
  else if (up.status == UPLOAD_FILE_ABORTED) {
    if (s_book.tmpFile) s_book.tmpFile.close();
    if (s_book.tmpPath.length() > 0 && FS.exists(s_book.tmpPath)) FS.remove(s_book.tmpPath);
    s_book.pendingUtf8Tail = "";
    s_book.tmpPath = "";
    s_book.ok = false;
    s_book.error = D_WEB_ERR_UPLOAD_ABORTED;
  }
}

// ============================================================================
//  Sleep image upload — straight binary, must be exactly 3904 bytes.
// ============================================================================

static void handleUploadSleepDone() {
  if (!s_sleep.ok) {
    server.send(400, "text/plain; charset=utf-8",
                s_sleep.error.length()
                  ? s_sleep.error
                  : String(D_WEB_SLEEP_UPLOAD_ERR_FALLBACK));
    return;
  }

  String inner;
  inner.reserve(500);
  inner += "<div class='card'><h2>" D_WEB_SLEEP_UPLOAD_HEADING "</h2><p class='muted'>" D_WEB_SLEEP_UPLOAD_DESC "</p><div class='actions'>" + webBackLink("/screensavers", D_WEB_BACK_TO_SCREENSAVERS) + "</div></div>";

  String page = successPage(
    D_WEB_UPLOAD_COMPLETE_HEADING,
    D_WEB_SLEEP_UPLOAD_SUBTITLE,
    D_WEB_SLEEP_UPLOAD_BANNER,
    inner,
    "screensavers"
  );
  server.send(200, "text/html; charset=utf-8", page);
}

static void handleUploadSleepStream() {
  HTTPUpload& upS = server.upload();

  if (upS.status == UPLOAD_FILE_START) {
    s_sleep.ok = false;
    s_sleep.error = "";
    s_sleep.tmpPath = "/sleep.bin.tmp";
    if (FS.exists(s_sleep.tmpPath)) FS.remove(s_sleep.tmpPath);
    s_sleep.tmpFile = FS.open(s_sleep.tmpPath, "w");
    if (!s_sleep.tmpFile) s_sleep.error = D_WEB_SLEEP_ERR_TEMP;
  }
  else if (upS.status == UPLOAD_FILE_WRITE) {
    if (s_sleep.error.length() > 0) return;
    if (s_sleep.tmpFile && upS.currentSize > 0) {
      if (s_sleep.tmpFile.size() + upS.currentSize > 8192) {
        s_sleep.tmpFile.close();
        if (FS.exists(s_sleep.tmpPath)) FS.remove(s_sleep.tmpPath);
        s_sleep.error = D_WEB_SLEEP_ERR_SIZE;
        return;
      }
      size_t wrote = s_sleep.tmpFile.write(upS.buf, upS.currentSize);
      if (wrote != upS.currentSize) {
        s_sleep.tmpFile.close();
        if (FS.exists(s_sleep.tmpPath)) FS.remove(s_sleep.tmpPath);
        s_sleep.error = D_WEB_SLEEP_ERR_SAVE;
        return;
      }
    }
  }
  else if (upS.status == UPLOAD_FILE_END) {
    if (s_sleep.tmpFile) s_sleep.tmpFile.close();
    File f = FS.open(s_sleep.tmpPath, "r");
    size_t sz = f ? f.size() : 0;
    if (f) f.close();

    if (sz != 3904) {
      if (FS.exists(s_sleep.tmpPath)) FS.remove(s_sleep.tmpPath);
      s_sleep.error = D_WEB_SLEEP_ERR_SIZE;
      s_sleep.ok = false;
    } else {
      if (FS.exists("/sleep.bin")) FS.remove("/sleep.bin");
      if (FS.rename(s_sleep.tmpPath, "/sleep.bin")) s_sleep.ok = true;
      else {
        if (FS.exists(s_sleep.tmpPath)) FS.remove(s_sleep.tmpPath);
        s_sleep.error = D_WEB_SLEEP_ERR_SAVE;
      }
    }
    s_sleep.tmpPath = "";
  }
  else if (upS.status == UPLOAD_FILE_ABORTED) {
    if (s_sleep.tmpFile) s_sleep.tmpFile.close();
    if (s_sleep.tmpPath.length() > 0 && FS.exists(s_sleep.tmpPath)) FS.remove(s_sleep.tmpPath);
    s_sleep.error = D_WEB_SLEEP_ERR_ABORTED;
    s_sleep.ok = false;
    s_sleep.tmpPath = "";
  }
}

void registerUploadRoutes() {
  server.on("/upload",       HTTP_POST, handleUploadDone,      handleUploadBookStream);
  server.on("/upload-sleep", HTTP_POST, handleUploadSleepDone, handleUploadSleepStream);
  server.on("/keepalive",    HTTP_GET,  handleWebUiKeepalive);
  server.on("/upload-review", HTTP_GET, handleUploadReview);
  server.on("/upload-finalize", HTTP_POST, handleUploadFinalize);
  server.on("/upload-cancel", HTTP_POST, handleUploadCancel);
}
