#include "src/storage/library.h"

#include "src/pure/paths.h"
#include "src/pure/library_catalog.h"
#include "src/state.h"                   // FS macro
#include "src/storage/fs_util.h"         // ensureBooksDir

// The library catalog. Populated by `loadBooks()` from on-disk `/books/**`.
// Navigation state (cursor, folder expansion, derived entry list) lives on
// the library screen — see `ui/screens/library_screen.cpp`.
Catalog g_library;

static void scanBooksRecursive(const String& absDir, const String& relDir) {
  File dir = FS.open(absDir);
  if (!dir || !dir.isDirectory()) return;

  File f = dir.openNextFile();
  while (f) {
    String entryName = String(f.name());
    String absPath = entryName.startsWith("/") ? entryName : (absDir + "/" + entryName);
    String leaf = lastPathComponent(absPath);

    if (f.isDirectory()) {
      String childRel = relDir.length() ? (relDir + "/" + leaf) : leaf;
      addCatalogFolder(g_library, childRel.c_str());
      scanBooksRecursive(absPath, childRel);
    } else if (g_library.bookCount < MAX_BOOKS && absPath.endsWith(".txt")) {
      String relFile = relDir.length() ? (relDir + "/" + leaf) : leaf;
      // The folder is metadata, not part of the book title. Keeping it out
      // of BookInfo::name prevents WebUI rename and device labels from
      // becoming "folder / title" for books stored below /books.
      String title = bookLeafLabel(relFile);
      addCatalogBook(g_library, absPath.c_str(), relDir.c_str(),
                     title.c_str(), f.size());
    }

    f.close();
    f = dir.openNextFile();
  }

  dir.close();
}

void loadBooks() {
  resetCatalog(g_library);

  ensureBooksDir();
  scanBooksRecursive("/books", "");
  sortCatalog(g_library);
}

const char* bookPath(int idx) {
  if (idx < 0 || idx >= g_library.bookCount) return nullptr;
  return g_library.books[idx].path;
}

int bookIndexForPath(const String& path) {
  for (int i = 0; i < g_library.bookCount; i++) {
    if (strcmp(g_library.books[i].path, path.c_str()) == 0) return i;
  }
  return -1;
}
