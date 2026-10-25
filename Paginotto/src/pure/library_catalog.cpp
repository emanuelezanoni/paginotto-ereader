#include "library_catalog.h"

#include <cstring>

namespace {

template <size_t N>
void copyBounded(char (&destination)[N], const char* source) {
  if (source == nullptr) source = "";
  std::strncpy(destination, source, N - 1);
  destination[N - 1] = '\0';
}

void sortFolders(Catalog& catalog) {
  for (int i = 0; i < catalog.folderCount - 1; ++i) {
    for (int j = i + 1; j < catalog.folderCount; ++j) {
      if (std::strcmp(catalog.folders[j], catalog.folders[i]) < 0) {
        char temporary[MAX_FOLDER_PATH + 1];
        std::memcpy(temporary, catalog.folders[i], sizeof(temporary));
        std::memcpy(catalog.folders[i], catalog.folders[j], sizeof(temporary));
        std::memcpy(catalog.folders[j], temporary, sizeof(temporary));
      }
    }
  }
}

void sortBooks(Catalog& catalog) {
  for (int i = 0; i < catalog.bookCount - 1; ++i) {
    for (int j = i + 1; j < catalog.bookCount; ++j) {
      const int byName = std::strcmp(catalog.books[j].name,
                                     catalog.books[i].name);
      const bool shouldSwap =
          byName < 0 ||
          (byName == 0 &&
           std::strcmp(catalog.books[j].path, catalog.books[i].path) < 0);
      if (shouldSwap) {
        BookInfo temporary = catalog.books[i];
        catalog.books[i] = catalog.books[j];
        catalog.books[j] = temporary;
      }
    }
  }
}

}  // namespace

void resetCatalog(Catalog& catalog) {
  catalog.bookCount = 0;
  catalog.folderCount = 0;
}

bool addCatalogFolder(Catalog& catalog, const char* folder) {
  if (folder == nullptr || folder[0] == '\0') return false;
  for (int i = 0; i < catalog.folderCount; ++i) {
    if (std::strcmp(catalog.folders[i], folder) == 0) return false;
  }
  if (catalog.folderCount >= MAX_FOLDERS) return false;
  copyBounded(catalog.folders[catalog.folderCount], folder);
  ++catalog.folderCount;
  return true;
}

bool addCatalogBook(Catalog& catalog, const char* path, const char* folder,
                    const char* name, size_t size) {
  if (path == nullptr || path[0] == '\0' ||
      catalog.bookCount >= MAX_BOOKS) {
    return false;
  }
  BookInfo& book = catalog.books[catalog.bookCount];
  copyBounded(book.path, path);
  copyBounded(book.folder, folder);
  copyBounded(book.name, name);
  book.size = size;
  ++catalog.bookCount;
  return true;
}

void sortCatalog(Catalog& catalog) {
  sortFolders(catalog);
  sortBooks(catalog);
}
