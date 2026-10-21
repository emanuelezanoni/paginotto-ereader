#include "src/ui/screens/library_screen.h"

#include "src/hal/display.h"
#include "src/config.h"
#include "src/pure/library_nav.h"         // buildLibraryEntries
#include "src/pure/paths.h"               // folderLeafLabel, bookLeafLabel
#include "src/storage/library.h"
#include "src/ui/font.h"
#include "src/ui/lock.h"
#include "src/ui/reader.h"
#include "src/ui/reader_actions.h"
#include "src/ui/screens/update_screen.h"
#include "src/ui/screens/reader_screen.h"
#include "src/ui/screens/upload_screen.h"
#include "src/ui/sleep.h"
#include "src/ui/widgets.h"

// ============================================================================
//  Library screen nav state
//
//  Lives here, not in `storage/library.h`: only this screen consumes any of
//  it. The catalog (books + folders inventory) is what the rest of the
//  codebase shares; everything below is private screen state.
// ============================================================================

// LibraryScreen-specific row indenting. Other menu screens pass nothing
// extra to drawMenuRow; the library is the only one with folder nesting.
static const int LIBRARY_DEPTH_INDENT = 10;

// Nav state — cursor + folder expansion. Persists across library visits
// (entering library doesn't collapse open folders).
//
// Folder expansion is keyed by *folder name*, not by index, because web
// handlers can call `loadBooks()` between our visits — that re-sorts
// `g_library.folders[]` and shifts indices arbitrarily. Storing names
// means we don't have to chase those reshuffles to stay in sync.
static int  s_cursor = 0;
static char s_expandedNames[MAX_FOLDERS][MAX_FOLDER_PATH + 1];
static int  s_expandedCount = 0;
static char s_openFolder[MAX_FOLDER_PATH + 1] = "";

// Cached entry list, rebuilt each draw, read by onButton until the next
// draw. Sized for the same upper bound the catalog can produce.
static LibEntry s_entries[MAX_LIBRARY_ENTRIES];
static int      s_entryCount = 0;

// ----------------------------------------------------------------------------
//  Folder expansion as a name-keyed set
// ----------------------------------------------------------------------------
static bool isExpanded(const char* name) {
  for (int i = 0; i < s_expandedCount; i++) {
    if (strcmp(s_expandedNames[i], name) == 0) return true;
  }
  return false;
}

// Toggle a folder's expansion. Prunes dead entries (folders that no longer
// exist in the catalog) before mutating — the user expanding/collapsing a
// folder is the natural moment to reconcile our set with the current
// catalog, rather than chasing every web handler that calls `loadBooks()`.
// Without this, the set would slowly fill with names of folders that have
// been deleted/renamed, until we'd refuse to expand a 33rd live one.
static void toggleExpanded(const char* name) {
  constexpr size_t kBuf = MAX_FOLDER_PATH + 1;

  int kept = 0;
  for (int i = 0; i < s_expandedCount; i++) {
    bool stillExists = false;
    for (int j = 0; j < g_library.folderCount; j++) {
      if (strcmp(s_expandedNames[i], g_library.folders[j]) == 0) {
        stillExists = true;
        break;
      }
    }
    if (stillExists) {
      if (kept != i) memcpy(s_expandedNames[kept], s_expandedNames[i], kBuf);
      kept++;
    }
  }
  s_expandedCount = kept;

  for (int i = 0; i < s_expandedCount; i++) {
    if (strcmp(s_expandedNames[i], name) == 0) {
      s_expandedCount--;
      if (i < s_expandedCount) {
        memcpy(s_expandedNames[i], s_expandedNames[s_expandedCount], kBuf);
      }
      return;
    }
  }
  if (s_expandedCount < MAX_FOLDERS) {
    strncpy(s_expandedNames[s_expandedCount], name, MAX_FOLDER_PATH);
    s_expandedNames[s_expandedCount][MAX_FOLDER_PATH] = '\0';
    s_expandedCount++;
  }
}

// ----------------------------------------------------------------------------
//  Per-entry helpers
// ----------------------------------------------------------------------------
static bool isSystemEntryType(LibraryEntryType t) {
  return t == LIB_ENTRY_UPDATE || t == LIB_ENTRY_UPLOAD;
}

static int rowIndent(const LibEntry& e) {
  return e.depth * LIBRARY_DEPTH_INDENT;
}

static String truncateBookLabel(const String& label) {
  if (label.length() <= MAX_DEVICE_BOOK_LABEL_CHARS) return label;
  const int keep = MAX_DEVICE_BOOK_LABEL_CHARS - 3;
  return label.substring(0, keep) + "...";
}

static String entryLabel(const LibEntry& e) {
  switch (e.type) {
    case LIB_ENTRY_FOLDER: {
      return folderLeafLabel(String(g_library.folders[e.ref]));
    }
    case LIB_ENTRY_BOOK:      return truncateBookLabel(bookLeafLabel(String(g_library.books[e.ref].path)));
    case LIB_ENTRY_UPDATE:     return D_MENU_UPDATE;
    case LIB_ENTRY_UPLOAD:     return D_MENU_WEB_UI;
  }
  return "";
}

// Small bitmap folder icon: no Unicode glyphs, no font dependency, and it
// remains legible after the e-ink panel's partial refreshes. Selected rows use
// a heavier outline but keep exactly the same bounding box.
static void drawFolderMenuRow(int yBaseline, const String& label,
                              bool selected, int extraIndent) {
  const int x = UI_LIST_LEFT + extraIndent;
  if (selected) Font::useBold();
  else          Font::useBody();

  gfx.drawRect(x + 1, yBaseline - 9, 13, 8, 1);
  gfx.drawFastHLine(x + 3, yBaseline - 11, 6, 1);
  if (selected) {
    gfx.drawRect(x + 2, yBaseline - 8, 11, 6, 1);
    gfx.drawFastHLine(x + 3, yBaseline - 10, 6, 1);
  }
  u8g2.setCursor(x + 19, yBaseline);
  u8g2.print(label.c_str());
  Font::useBody();
}

static bool beginsSystemSection(int idx) {
  // drawScrollableList supplies valid entry indices, but keep the array bound
  // explicit here so static analysis can verify both indexed accesses.
  if (idx <= 0 || idx >= MAX_LIBRARY_ENTRIES) return false;
  return isSystemEntryType(s_entries[idx].type)
      && !isSystemEntryType(s_entries[idx - 1].type);
}

static void performLibraryAction(ButtonAction action) {
  switch (action) {
    case ACTION_LOCK:
      Lock::engage();
      Sleep::enter();
      break;
    case ACTION_NONE:
    default:
      break;
  }
}

// ----------------------------------------------------------------------------
//  Screen lifecycle
// ----------------------------------------------------------------------------
void LibraryScreen::onEnter() {
  // Full reset of reader scratch state on every library entry — downstream
  // code never has to reason about stale reader state. Wake
  // state doesn't need clearing — it's consumed at boot, so it's already
  // empty during runtime unless the reader has explicitly armed it.
  resetBookView();

  // Reset the cursor but preserve folder expansion across visits — user
  // returning from a sub-screen expects their open folders still open.
  s_cursor = 0;

  draw();
}

void LibraryScreen::draw() {
  prepareMenuFrame();
  Font::useBody();

  // The system actions are always available after the book catalog.
  LibraryEntryType systemEntries[2];
  int systemCount = 0;
  systemEntries[systemCount++] = LIB_ENTRY_UPLOAD;
  systemEntries[systemCount++] = LIB_ENTRY_UPDATE;

  String folderTitle = s_openFolder[0]
                     ? folderLeafLabel(String(s_openFolder))
                     : String(LIB_HEADER_TITLE);
  if (s_openFolder[0]) {
    s_entryCount = 0;
    for (int i = 0; i < g_library.bookCount && s_entryCount < MAX_LIBRARY_ENTRIES; i++) {
      if (strcmp(g_library.books[i].folder, s_openFolder) == 0) {
        s_entries[s_entryCount++] = {LIB_ENTRY_BOOK, i, 0};
      }
    }
  } else {
    // Root view: folders are entries in their own right. Opening one enters
    // a dedicated folder page instead of expanding a second tree inline.
    const bool collapsed[MAX_FOLDERS] = {false};
    s_entryCount = buildLibraryEntries(g_library, collapsed,
                                       systemEntries, systemCount,
                                       s_entries, MAX_LIBRARY_ENTRIES);
  }

  if (s_cursor < 0) s_cursor = 0;
  if (s_cursor >= s_entryCount) s_cursor = max(0, s_entryCount - 1);

  int y = drawSectionHeader(folderTitle.c_str());

  if (s_openFolder[0] && s_entryCount == 0) {
    drawMenuRow(y, "Folder is empty", false, 0);
    display.update();
    return;
  }

  drawScrollableList(y, s_entryCount, s_cursor,
    [&](int idx, int rowY, bool selected, int budget) {
      const bool spacer = beginsSystemSection(idx) && budget >= 2;
      if (spacer) rowY += menuLineH();
      else if (beginsSystemSection(idx)) {
        gfx.drawFastHLine(UI_LIST_LEFT, rowY - (menuLineH() / 2),
                          SCREEN_W - UI_LIST_LEFT - MARGIN_X, 1);
      }
      if (s_entries[idx].type == LIB_ENTRY_FOLDER) {
        drawFolderMenuRow(rowY, entryLabel(s_entries[idx]), selected,
                          rowIndent(s_entries[idx]));
      } else {
        drawMenuRow(rowY, entryLabel(s_entries[idx]), selected,
                    rowIndent(s_entries[idx]));
      }
      return spacer ? 2 : 1;
    });

  display.update();
}

void LibraryScreen::onButton(const ButtonEvent& e) {
  if (!e.any()) return;

  if (e.kind == ButtonEvent::Triple) {
    if (s_openFolder[0]) {
      s_openFolder[0] = '\0';
      s_cursor = 0;
      draw();
      return;
    }
    navigateToLibraryRoot();
    return;
  }

  if (e.kind == ButtonEvent::Hold || e.kind == ButtonEvent::ClickHold) {
    performLibraryAction(Gestures::actionFor(e.kind));
    return;
  }

  if (e.kind == ButtonEvent::Short) {
    if (s_entryCount > 0) {
      s_cursor = (s_cursor + 1) % s_entryCount;
    }
    draw();
    return;
  }

  if (e.kind != ButtonEvent::Double) return;

  if (s_cursor < 0 || s_cursor >= s_entryCount) {
    draw();
    return;
  }

  const LibEntry& sel = s_entries[s_cursor];

  if (sel.type == LIB_ENTRY_FOLDER) {
    strncpy(s_openFolder, g_library.folders[sel.ref], MAX_FOLDER_PATH);
    s_openFolder[MAX_FOLDER_PATH] = '\0';
    s_cursor = 0;
    draw();
    return;
  }

  if (sel.type == LIB_ENTRY_BOOK) {
    if (openBookByIndex(sel.ref)) {
      nextScreen = &g_readerScreen;
    } else {
      drawCenter(D_LIBRARY_OPEN_FAILED, D_LIBRARY_TRY_UPLOAD);
      draw();
    }
    return;
  }

  if (sel.type == LIB_ENTRY_UPDATE) {
    // Use the exact same Wi-Fi screen as Web UI/upload for the first OTA
    // integration pass. The update-specific picker is intentionally bypassed
    // until this known-good path has been validated on hardware.
    nextScreen = &g_uploadScreen;
    return;
  }

  if (sel.type == LIB_ENTRY_UPLOAD) {
    nextScreen = &g_uploadScreen;
    return;
  }

  Serial.println("LibraryScreen: unhandled entry type " + String(sel.type));
}

// ============================================================================
//  Public ops for callers outside the screen
// ============================================================================
void navigateToLibraryRoot() {
  s_openFolder[0] = '\0';
  if (g_currentScreen) g_currentScreen->nextScreen = &g_libraryScreen;
}

void resetLibraryNav() {
  s_cursor = 0;
  s_expandedCount = 0;
  s_openFolder[0] = '\0';
}
