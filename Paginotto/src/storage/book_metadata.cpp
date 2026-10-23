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

String loadSavedAnchor(KeyValueStore& kv, const String& bookKey) {
  return kv.getString((bookKey + "_anc").c_str(), String());
}

uint32_t loadSavedAnchorAt(KeyValueStore& kv, const String& bookKey) {
  int value = kv.getInt((bookKey + "_anc_at").c_str(), -1);
  return value < 0 ? 0U : (uint32_t)value;
}

void saveSavedAnchor(KeyValueStore& kv, const String& bookKey,
                     const String& anchor, uint32_t anchorAt) {
  if (anchor.length() == 0 || anchorAt >= anchor.length()
      || anchor.length() > kBookmarkAnchorMaxBytes) {
    kv.remove((bookKey + "_anc").c_str());
    kv.remove((bookKey + "_anc_at").c_str());
    return;
  }
  kv.putString((bookKey + "_anc").c_str(), anchor);
  kv.putInt((bookKey + "_anc_at").c_str(), (int)anchorAt);
}

bool clearBookMetadata(KeyValueStore& kv, const String& bookKey) {
  kv.remove((bookKey + "_p").c_str());
  kv.remove((bookKey + "_off").c_str());
  kv.remove((bookKey + "_anc").c_str());
  kv.remove((bookKey + "_anc_at").c_str());
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

  String anchor = loadSavedAnchor(kv, oldKey);
  if (anchor.length() > 0) {
    saveSavedAnchor(kv, newKey, anchor, loadSavedAnchorAt(kv, oldKey));
    kv.remove((oldKey + "_anc").c_str());
    kv.remove((oldKey + "_anc_at").c_str());
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
