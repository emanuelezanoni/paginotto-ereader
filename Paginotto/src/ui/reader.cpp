#include "src/ui/reader.h"
#include "src/diagnostics/log.h"

#include "src/hal/display.h"
#include "src/pure/hashing.h"
#include "src/storage/book_metadata.h"
#include "src/storage/library.h"   // g_library — openBookByIndex reads it
#include "src/storage/page_cache.h"
#include "src/storage/preferences_store.h"
#include "src/hal/input.h"                  // markUserActivity during long opens

#include "src/ui/font.h"                    // layoutForCache for cache stamping
#include "src/ui/screens/library_screen.h"  // navigateToLibraryRoot — fallback on error
#include "src/ui/text.h"
#include "src/ui/toast.h"                   // Toast::isActive / Toast::draw
#include "src/ui/widgets.h"                 // drawCenter (error fallback)

// The currently open book and where the user is looking. Produced by
// `openBookByIndex()` below, fully torn down by `resetBookView()` on leaving
// the reader.
BookView g_bookview;

// One-shot flag: force the next renderCurrentPage() to do a full e-ink refresh
// regardless of pageTurnsSinceFull. Set by forceNextRenderFull() (called from
// the unlock path to clear screensaver ghosting).
static bool s_forceNextFull = false;

static bool     s_openingBook = false;
static uint32_t s_openTargetOffset = 0;
static uint32_t s_openScanOffset = 0;
static uint32_t s_openScanPage = 0;
static bool     s_openTargetIsPage = false;
static uint32_t s_lastOpeningPaint = 0;

static void drawOpeningProgress() {
  display.fastmodeOn();
  beginPageCanvas();
  Font::useBody();

  const int lineH = 16;
  const int page = (int)s_openScanPage + 1;
  char progress[48];
  snprintf(progress, sizeof(progress), "Scanning page %d...", page);

  const char* title = "Opening book...";
  int y = (SCREEN_H / 2) - lineH;
  int titleW = u8g2.getUTF8Width(title);
  u8g2.setCursor((SCREEN_W - titleW) / 2, y);
  u8g2.print(title);

  y += lineH + 4;
  int progressW = u8g2.getUTF8Width(progress);
  u8g2.setCursor((SCREEN_W - progressW) / 2, y);
  u8g2.print(progress);
  display.update();
  s_lastOpeningPaint = millis();
}

void forceNextRenderFull() { s_forceNextFull = true; }

// Fixed reader footer. This is intentionally independent from the removed
// reader menu: the page number and progress indicator are part of the reading
// view itself, not an optional settings/status menu.
static void drawReaderFooter(uint32_t startOffset) {
  size_t total = g_bookview.book.size();
  if (total == 0) total = 1;

  int pageTextW = 0;
  if (SHOW_PAGE_NUMBER) {
    Font::useUiTiny();
    char page[20];
    snprintf(page, sizeof(page), "%d", g_bookview.cursor.pageIndex + 1);
    pageTextW = u8g2.getUTF8Width(page);
    u8g2.setCursor(SCREEN_W - MARGIN_X - pageTextW, SCREEN_H - 1);
    u8g2.print(page);
    Font::useBody();
  }

  if (SHOW_PROGRESS_BAR) {
    const int rightPadding = SHOW_PAGE_NUMBER ? pageTextW + 8 : 0;
    int width = (SCREEN_W - 2 * MARGIN_X) - rightPadding;
    if (width < 40) width = 40;

    const int top = SCREEN_H - 7;
    int filled = (int)((startOffset * (uint32_t)width) / (uint32_t)total);
    if (filled < 0) filled = 0;
    if (filled > width) filled = width;

    gfx.drawRect(MARGIN_X, top, width, 4, 1);
    if (filled > 0) {
      gfx.fillRect(MARGIN_X + 1, top + 1, max(0, filled - 2), 2, 1);
    }
  }
}

// Auto-save throttle bookkeeping. Private to this translation unit — only
// the save-progress functions and `resetSaveThrottle` touch it, and only
// the reader calls them on its hot path (preview never auto-saves).
// Hiding it here keeps the "this is the open book" public type from
// carrying reader-only baggage.
struct SaveThrottle {
  uint32_t lastSaveMs    = 0;
  int      lastSavedPage = -1;
};
static SaveThrottle s_save;

static void resetSaveThrottle() {
  s_save = SaveThrottle{};
}

static String bookmarkAnchor(File& file, uint32_t offset, uint32_t& anchorAt) {
  const size_t fileSize = file.size();
  if (!file || fileSize == 0) {
    anchorAt = 0;
    return String();
  }

  const size_t before = 64;
  const size_t after = 128;
  const size_t start = offset > before ? (size_t)offset - before : 0;
  const size_t offsetSize = (size_t)offset;
  const size_t end = offsetSize >= fileSize || fileSize - offsetSize < after
                   ? fileSize : offsetSize + after;

  const size_t oldPosition = file.position();
  if (!file.seek(start)) {
    anchorAt = 0;
    return String();
  }

  String anchor;
  anchor.reserve(end - start);
  while (file.position() < end && file.available()) {
    int value = file.read();
    if (value < 0) break;
    anchor += (char)value;
  }
  file.seek(oldPosition);
  anchorAt = (uint32_t)((size_t)offset - start);
  return anchor;
}

// ============================================================================
//  OpenBook — file + path + key managed together. Strong invariant:
//  `isOpen()` ⇔ `path()` is non-empty. Public callers never observe
//  the file-closed-with-path-still-set intermediate state.
// ============================================================================
bool OpenBook::open(const String& path) {
  Diagnostics::trace("reader.book.open.begin");
  close();
  File f = FS.open(path, "r");
  if (!f || f.isDirectory()) {
    Diagnostics::warning("reader.book.open.failed");
    if (f) f.close();
    return false;
  }
  file_ = f;
  path_ = path;
  key_  = prefKeyForBook(path);
  Diagnostics::trace("reader.book.open.ok");
  return true;
}

void OpenBook::close() {
  if (file_) {
    Diagnostics::trace("reader.book.close");
    file_.close();
  }
  path_ = "";
  key_  = "";
}

void armResumeOnWake() {
  if (!g_bookview.book.isOpen()) return;
  prefs.putString("wake_path", g_bookview.book.path());
}

static void clearResumeOnWake() {
  prefs.remove("wake_path");
}

int findPageForOffset(uint32_t targetOffset) {
  uint32_t offset = 0;
  uint32_t page = 0;
  g_bookview.pages.eofReached = false;
  while (offset < targetOffset) {
    uint32_t next = nextPageOffset(g_bookview.book.file(), offset);
    if (next <= offset) {
      g_bookview.pages.eofReached = true;
      break;
    }
    offset = next;
    page++;
    if (offset >= (uint32_t)g_bookview.book.size()) {
      g_bookview.pages.eofReached = true;
      break;
    }
  }
  g_bookview.pages.currentOffset = offset;
  return (int)page;
}

bool isOpeningBook() { return s_openingBook; }

void cancelBookOpening() {
  s_openingBook = false;
  s_openTargetOffset = 0;
  s_openScanOffset = 0;
  s_openScanPage = 0;
  s_openTargetIsPage = false;
}

void serviceBookOpening() {
  if (!s_openingBook || !g_bookview.book.isOpen()) return;

  // Keep each main-loop slice bounded. This is what makes the emergency
  // triple-click useful while a large book is being indexed.
  static constexpr int kPagesPerSlice = 24;
  for (int i = 0; i < kPagesPerSlice; i++) {
    bool targetReached = s_openTargetIsPage
                       ? s_openScanPage >= s_openTargetOffset
                       : s_openScanOffset >= s_openTargetOffset;
    if (targetReached) {
      g_bookview.cursor.pageIndex = (int)s_openScanPage;
      g_bookview.pages.currentOffset = s_openScanOffset;
      s_openingBook = false;
      s_openTargetOffset = 0;
      s_openTargetIsPage = false;
      markUserActivity();
      return;
    }
    uint32_t next = nextPageOffset(g_bookview.book.file(), s_openScanOffset);
    if (next <= s_openScanOffset) {
      g_bookview.pages.eofReached = true;
      g_bookview.pages.currentOffset = s_openScanOffset;
      g_bookview.cursor.pageIndex = (int)s_openScanPage;
      s_openingBook = false;
      s_openTargetOffset = 0;
      s_openTargetIsPage = false;
      markUserActivity();
      return;
    }
    s_openScanOffset = next;
    s_openScanPage++;
    if (s_openScanOffset >= (uint32_t)g_bookview.book.size()) {
      g_bookview.pages.eofReached = true;
    }
  }

  // E-ink refreshes are comparatively expensive; keep progress visible
  // without slowing the sequential scan with needless panel updates.
  if ((uint32_t)(millis() - s_lastOpeningPaint) >= 5000) {
    drawOpeningProgress();
  }
}

// ============================================================================
//  Reader operations
// ============================================================================
bool openBookByIndex(int idx) {
  Diagnostics::checkpoint("reader.open.begin");
  // Invariant: callers reset the view before calling here. `OpenBook::open`
  // is also atomic about the file-handle swap, so a stale handle would be
  // overwritten cleanly even if the invariant were violated.
  const char* p = bookPath(idx);
  if (!p) {
    Diagnostics::warning("reader.open.invalid_index");
    return false;
  }

  String path(p);
  if (!g_bookview.book.open(path)) {
    Diagnostics::error("reader.open.file_failed");
    return false;
  }

  g_bookview.pages.currentOffset = 0;
  g_bookview.pages.eofReached = false;

  // Resolve the reading position. The byte offset (`_off`) is canonical and
  // survives font changes; if it's set, find which page contains it under
  // the current layout. If absent (legacy data from older firmware), fall
  // back to the saved page number — it'll get rewritten as an offset on
  // the next save.
  // Both branches below produce a >= 0 page index (findPageForOffset's
  // floor is 0; loadSavedPage clamps internally), so no extra guard needed.
  // Any out-of-range value vs. the page table gets normalized in
  // prepareForRender on the first draw.
  PreferencesStore kv(prefs);
  uint32_t savedOffset = loadSavedOffset(kv, g_bookview.book.key());
  if (savedOffset != kOffsetUnset) {
    g_bookview.cursor.pageIndex = 0;
    if (savedOffset > 0) {
      s_openScanOffset = 0;
      s_openScanPage = 0;
      s_openTargetOffset = savedOffset;
      s_openTargetIsPage = false;
      s_openingBook = true;
      s_lastOpeningPaint = 0;
    }
  } else {
    g_bookview.cursor.pageIndex = loadSavedPage(kv, g_bookview.book.key());
    if (g_bookview.cursor.pageIndex > 0) {
      s_openScanOffset = 0;
      s_openScanPage = 0;
      s_openTargetOffset = (uint32_t)g_bookview.cursor.pageIndex;
      s_openTargetIsPage = true;
      s_openingBook = true;
      s_lastOpeningPaint = 0;
      g_bookview.cursor.pageIndex = 0;
    }
  }
  g_bookview.cursor.pageTurnsSinceFull = 0;
  resetSaveThrottle();
  Diagnostics::checkpoint("reader.open.ready");
  return true;
}

// State normalization for rendering: lazily paginates up to the cursor,
// clamps the cursor to a valid range, and recovers from out-of-file
// offsets. Returns false if the book has nothing renderable (empty file
// or no pages) — caller should show an error and bail.
//
// All g_bookview mutations triggered by rendering are concentrated here;
// the actual draw in renderCurrentPage is read-only on view state
// (modulo pageTurnsSinceFull, which is render-side bookkeeping).
static bool prepareForRender() {
  Diagnostics::trace("reader.render.prepare");
  if (g_bookview.book.size() == 0) {
    Diagnostics::warning("reader.render.empty");
    return false;
  }
  if (g_bookview.cursor.pageIndex < 0) g_bookview.cursor.pageIndex = 0;
  if (g_bookview.pages.currentOffset >= g_bookview.book.size())
    g_bookview.pages.currentOffset = 0;
  return true;
}

void renderCurrentPage() {
  Diagnostics::trace("reader.render.begin");
  if (s_openingBook) {
    if (s_lastOpeningPaint == 0) drawOpeningProgress();
    return;
  }
  // Strong invariant: if we got here, a book is open. The reader screen
  // is never active otherwise.
  if (!prepareForRender()) {
    drawCenter(D_READER_BOOK_EMPTY, D_READER_BACK_LIBRARY);
    navigateToLibraryRoot();
    return;
  }

  uint32_t start = g_bookview.pages.currentOffset;

  bool doFull = s_forceNextFull
             || (g_bookview.cursor.pageTurnsSinceFull >= FULL_REFRESH_EVERY_N_PAGES);
  s_forceNextFull = false;
  if (doFull) {
    display.fastmodeOff();
    g_bookview.cursor.pageTurnsSinceFull = 0;
  } else {
    display.fastmodeOn();
  }

  beginPageCanvas();
  Font::useBody();

  uint32_t nextOff = drawPageAt(g_bookview.book.file(), start);
  // Keep the offset of the current page only. The next page is calculated on
  // demand by advancePage(), so page count never consumes RAM.
  (void)nextOff;

  if (Toast::isActive()) Toast::draw();
  drawReaderFooter(start);

  display.update();
  Diagnostics::trace("reader.render.end");
}

bool tryRestoreReadingSession() {
  Diagnostics::trace("reader.restore.begin");
  // Single-shot: read wake intent, then clear it. From this point on
  // (during this runtime session) wake state is empty unless the reader
  // re-arms it on its way into deep sleep.
  String wp = prefs.getString("wake_path", "");
  clearResumeOnWake();
  if (wp.length() == 0) {
    Diagnostics::trace("reader.restore.none");
    return false;
  }

  int idx = bookIndexForPath(wp);
  if (idx < 0) {
    Diagnostics::warning("reader.restore.invalid_book");
    return false;
  }
  if (!openBookByIndex(idx)) {
    Diagnostics::error("reader.restore.open_failed");
    return false;
  }
  Diagnostics::trace("reader.restore.ok");
  return true;
}

// ============================================================================
//  Cursor navigation. Both move the cursor and bump `pageTurnsSinceFull` on a
//  real change; the caller decides what to save and render.
// ============================================================================
bool advancePage() {
  Diagnostics::trace("reader.page.next");
  if (g_bookview.pages.eofReached) {
    Diagnostics::trace("reader.page.next_eof");
    return false;
  }
  uint32_t next = nextPageOffset(g_bookview.book.file(), g_bookview.pages.currentOffset);
  if (next <= g_bookview.pages.currentOffset
      || next >= (uint32_t)g_bookview.book.size()) {
    g_bookview.pages.eofReached = true;
    return false;
  }
  g_bookview.pages.currentOffset = next;
  g_bookview.cursor.pageIndex++;
  g_bookview.cursor.pageTurnsSinceFull++;
  return true;
}

bool retreatPage() {
  Diagnostics::trace("reader.page.previous");
  if (g_bookview.cursor.pageIndex <= 0) {
    Diagnostics::trace("reader.page.first");
    return false;
  }
  uint32_t offset = 0;
  for (int page = 0; page < g_bookview.cursor.pageIndex - 1; page++) {
    uint32_t next = nextPageOffset(g_bookview.book.file(), offset);
    if (next <= offset) return false;
    offset = next;
  }
  g_bookview.pages.currentOffset = offset;
  g_bookview.pages.eofReached = false;
  g_bookview.cursor.pageIndex--;
  g_bookview.cursor.pageTurnsSinceFull++;
  return true;
}

// ============================================================================
//  Full view reset — used when leaving the reader entirely.
// ============================================================================
void resetBookView() {
  Diagnostics::checkpoint("reader.reset");
  g_bookview.book.close();
  g_bookview.cursor = ReaderCursor{};
  g_bookview.pages  = PageOffsetTable{};
  cancelBookOpening();
  resetSaveThrottle();
}

// ============================================================================
//  Progress glue. Reads/writes g_bookview, delegates the actual
//  persistence to the pure storage API. Lives here (not in storage/) because
//  the throttle decision and the "is a book open" guard are view-state
//  concerns.
// ============================================================================
void saveProgress() {
  if (!g_bookview.book.isOpen()) return;

  Diagnostics::trace("reader.progress.save");

  PreferencesStore kv(prefs);
  // Canonical: the byte offset where the user currently is. Invariant
  // under font/layout changes.
  const uint32_t offset = g_bookview.pages.currentOffset;
  saveSavedOffset(kv, g_bookview.book.key(), offset);
  uint32_t anchorAt = 0;
  saveSavedAnchor(kv, g_bookview.book.key(),
                  bookmarkAnchor(g_bookview.book.file(), offset, anchorAt), anchorAt);
  // Hint: the page number at the current layout. Used by the web file
  // list for display and as a legacy fallback for openBookByIndex when
  // no offset has been saved yet.
  saveSavedPage(kv, g_bookview.book.key(), g_bookview.cursor.pageIndex);
  s_save.lastSaveMs    = millis();
  s_save.lastSavedPage = g_bookview.cursor.pageIndex;
}

void saveProgressThrottled() {
  if (!g_bookview.book.isOpen()) return;
  if (g_bookview.cursor.pageIndex == s_save.lastSavedPage) return;
  uint32_t now = millis();
  if (s_save.lastSaveMs != 0 && (now - s_save.lastSaveMs) < SAVE_EVERY_MS) return;
  saveProgress();
}

// Commit everything the active reader has accumulated to durable storage.
// Used at every "leaving" moment (sleep, exit-to-home, preview commit) so a
// later boot or open finds both the latest cursor and a fresh page table.
// Mid-session uses `saveProgressThrottled` for cheaper, more frequent writes.
void persistReaderState() {
  if (!g_bookview.book.isOpen()) return;
  Diagnostics::checkpoint("reader.persist.begin");
  saveProgress();
  savePageOffsetCacheForBook(g_bookview.book.path(), g_bookview.book.size(),
                             Font::layoutForCache(),
                             g_bookview.pages);
  Diagnostics::checkpoint("reader.persist.done");
}
