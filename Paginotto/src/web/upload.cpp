#include "src/web/upload.h"

#include "src/config.h"
#include "src/state.h"
#include "src/pure/paths.h"
#include "src/pure/text_util.h"
#include "src/storage/fs_util.h"
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

  String finalPath = "/books/" + s_book.finalName;
  int newBookId = -1;
  for (int i = 0; i < g_library.bookCount; i++) {
    if (String(g_library.books[i].path) == finalPath) {
      newBookId = i;
      break;
    }
  }
  size_t storedSize = 0;
  File stored = FS.open(finalPath, "r");
  if (stored) {
    storedSize = stored.size();
    stored.close();
  }

  String inner;
  inner.reserve(1200);
  inner += "<div class='card'><h2>" D_WEB_UPLOAD_COMPLETE_HEADING "</h2><p class='muted'>" D_WEB_UPLOAD_COMPLETE_DESC "</p>";
  inner += "<div class='stats'>";
  inner += "<div class='stat'><span class='muted'>" D_WEB_UPLOAD_BOOK_LABEL  "</span><b>" + htmlEscape(s_book.finalName) + "</b></div>";
  inner += "<div class='stat'><span class='muted'>" D_WEB_UPLOAD_STORED_SIZE "</span><b>" + humanBytes(storedSize)         + "</b></div>";
  inner += "</div>";
  if (newBookId >= 0) {
    inner += "<h3 style='margin-top:18px'>Manage uploaded book</h3>"
             "<form method='POST' action='/rename' class='stack' style='margin-top:10px'>"
             "<input type='hidden' name='id' value='" + String(newBookId) + "'>"
             "<label for='uploadedBookName'>File name</label>"
             "<input id='uploadedBookName' type='text' name='name' value='" + htmlEscape(s_book.finalName) + "' maxlength='" + String(MAX_BOOK_TITLE_BYTES) + "' required autocomplete='off' spellcheck='false'>"
             "<div class='actions'><button type='submit'>Rename file</button>"
             "<a class='btn secondary' href='/read?id=" + String(newBookId) + "'>Edit text</a></div></form>";
  }
  inner += "<div class='actions' style='margin-top:18px'>" + webBackLink("/books", D_WEB_BACK_TO_BOOKS) + "<a class='btn' href='/books'>" D_WEB_UPLOAD_ANOTHER "</a></div></div>";

  String page = successPage(
    D_WEB_UPLOAD_COMPLETE_HEADING,
    D_WEB_UPLOAD_BOOK_SAVED,
    D_WEB_UPLOAD_FINISHED,
    inner
  );
  server.send(200, "text/html; charset=utf-8", page);
}

static void handleUploadBookStream() {
  notifyUploadActivity();
  HTTPUpload& up = server.upload();

  if (up.status == UPLOAD_FILE_START) {
    s_book.ok = false;
    s_book.error = "";
    s_book.finalName = "";
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
}
