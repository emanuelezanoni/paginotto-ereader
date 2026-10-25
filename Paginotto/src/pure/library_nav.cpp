#include "library_nav.h"

#include "paths.h"

namespace {

int boundedCount(int count, int limit) {
  if (count < 0) return 0;
  return count < limit ? count : limit;
}

void appendEntry(LibraryEntryType type, int ref, int depth,
                 LibEntry* out, int& count, int capacity) {
  if (!out || count >= capacity) return;
  out[count++] = {type, ref, depth};
}

void appendFolderTree(const Catalog& cat, int folderCount, int bookCount,
                      const bool* folderExpanded, const String& parent,
                      int depth, bool* active, LibEntry* out, int& count,
                      int capacity) {
  for (int folderIndex = 0;
       folderIndex < folderCount && count < capacity;
       ++folderIndex) {
    if (active[folderIndex]) continue;

    const String folder = String(cat.folders[folderIndex]);
    // Empty folder names are invalid catalog entries and must not become a
    // phantom root row. `active` is defensive protection for malformed input
    // even though valid relative paths always shorten toward the root.
    if (folder.length() == 0 || folderParent(folder) != parent) continue;

    appendEntry(LIB_ENTRY_FOLDER, folderIndex, depth, out, count, capacity);
    if (!folderExpanded || !folderExpanded[folderIndex]) continue;

    active[folderIndex] = true;
    for (int bookIndex = 0;
         bookIndex < bookCount && count < capacity;
         ++bookIndex) {
      if (String(cat.books[bookIndex].folder) == folder) {
        appendEntry(LIB_ENTRY_BOOK, bookIndex, depth + 1,
                    out, count, capacity);
      }
    }
    appendFolderTree(cat, folderCount, bookCount, folderExpanded, folder,
                     depth + 1, active, out, count, capacity);
    active[folderIndex] = false;
  }
}

}  // namespace

int buildLibraryEntries(
    const Catalog& cat,
    const bool* folderExpanded,
    const LibraryEntryType* systemEntries, int systemCount,
    LibEntry* out, int outCap) {
  if (!out || outCap <= 0) return 0;

  const int folderCount = boundedCount(cat.folderCount, MAX_FOLDERS);
  const int bookCount = boundedCount(cat.bookCount, MAX_BOOKS);
  int count = 0;
  bool active[MAX_FOLDERS] = {};

  appendFolderTree(cat, folderCount, bookCount, folderExpanded, String(""),
                   0, active, out, count, outCap);

  for (int bookIndex = 0;
       bookIndex < bookCount && count < outCap;
       ++bookIndex) {
    if (String(cat.books[bookIndex].folder).length() == 0) {
      appendEntry(LIB_ENTRY_BOOK, bookIndex, 0, out, count, outCap);
    }
  }

  if (systemEntries && systemCount > 0) {
    for (int systemIndex = 0;
         systemIndex < systemCount && count < outCap;
         ++systemIndex) {
      appendEntry(systemEntries[systemIndex], -1, 0,
                  out, count, outCap);
    }
  }

  return count;
}
