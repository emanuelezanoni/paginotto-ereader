#include "src/storage/book_metadata.h"

#include "src/pure/hashing.h"

// ============================================================================
//  Testable KV-store-backed API
// ============================================================================
int loadSavedPage(KeyValueStore& kv, const String& bookKey) {
  int p = kv.getInt((bookKey + "_p").c_str(), 0);
  return (p < 0) ? 0 : p;
}

void saveSavedPage(KeyValueStore& kv, const String& bookKey, int pageIndex) {
  kv.putInt((bookKey + "_p").c_str(), pageIndex);
}

uint32_t loadSavedOffset(KeyValueStore& kv, const String& bookKey) {
  // Stored as int; -1 (default when key absent) sentinels "not set."
  int v = kv.getInt((bookKey + "_off").c_str(), -1);
  return (v < 0) ? kOffsetUnset : (uint32_t)v;
}

void saveSavedOffset(KeyValueStore& kv, const String& bookKey, uint32_t byteOffset) {
  kv.putInt((bookKey + "_off").c_str(), (int)byteOffset);
}

bool clearBookMetadata(KeyValueStore& kv, const String& bookKey) {
  kv.remove((bookKey + "_p").c_str());
  kv.remove((bookKey + "_off").c_str());
  return true;
}

void renameBookMetadata(KeyValueStore& kv, const String& oldKey, const String& newKey) {
  int progress = kv.getInt((oldKey + "_p").c_str(), -1);
  if (progress >= 0) {
    kv.putInt((newKey + "_p").c_str(), progress);
    kv.remove((oldKey + "_p").c_str());
  }

  int off = kv.getInt((oldKey + "_off").c_str(), -1);
  if (off >= 0) {
    kv.putInt((newKey + "_off").c_str(), off);
    kv.remove((oldKey + "_off").c_str());
  }

}

#ifdef ARDUINO
// ============================================================================
//  Firmware glue
// ============================================================================
#include "src/storage/library.h"           // g_library (bulk invalidation iterates books)
#include "src/storage/page_cache.h"        // deletePageCacheForBook / renamePageCacheForBook
#include "src/storage/preferences_store.h"

int savedPageForBookPath(const String& path) {
  PreferencesStore kv(prefs);
  return loadSavedPage(kv, prefKeyForBook(path));
}

void deleteBookMetadata(const String& path) {
  PreferencesStore kv(prefs);
  clearBookMetadata(kv, prefKeyForBook(path));   // NVS: reading progress
  deletePageCacheForBook(path);                  // disk: pc_<hash>.bin
}

void migrateBookMetadata(const String& oldPath, const String& newPath) {
  PreferencesStore kv(prefs);
  renameBookMetadata(kv, prefKeyForBook(oldPath), prefKeyForBook(newPath));
  renamePageCacheForBook(oldPath, newPath);
}
#endif  // ARDUINO
