#include "src/storage/book_metadata.h"

#include "src/pure/hashing.h"

namespace {

String metadataKey(const String& bookKey, const char* suffix) {
  return bookKey + suffix;
}

bool validBookKey(const String& bookKey) {
  return bookKey.length() > 0;
}

}  // namespace

// ============================================================================
//  Testable KV-store-backed API
// ============================================================================
int loadSavedPage(KeyValueStore& kv, const String& bookKey) {
  if (!validBookKey(bookKey)) return 0;
  int p = kv.getInt(metadataKey(bookKey, "_p").c_str(), 0);
  return (p < 0) ? 0 : p;
}

void saveSavedPage(KeyValueStore& kv, const String& bookKey, int pageIndex) {
  if (!validBookKey(bookKey)) return;
  kv.putInt(metadataKey(bookKey, "_p").c_str(), pageIndex < 0 ? 0 : pageIndex);
}

uint32_t loadSavedOffset(KeyValueStore& kv, const String& bookKey) {
  if (!validBookKey(bookKey)) return kOffsetUnset;

  // New writes use a four-byte blob so the complete uint32_t range survives.
  // The int fallback keeps offsets written by older firmware readable.
  const String key = metadataKey(bookKey, "_off");
  uint32_t offset = 0;
  if (kv.getBytes(key.c_str(), &offset, sizeof(offset)) == sizeof(offset)) {
    return offset;
  }
  int v = kv.getInt(key.c_str(), -1);
  return (v < 0) ? kOffsetUnset : (uint32_t)v;
}

void saveSavedOffset(KeyValueStore& kv, const String& bookKey, uint32_t byteOffset) {
  if (!validBookKey(bookKey)) return;
  kv.putBytes(metadataKey(bookKey, "_off").c_str(), &byteOffset,
              sizeof(byteOffset));
}

String loadSavedAnchor(KeyValueStore& kv, const String& bookKey) {
  if (!validBookKey(bookKey)) return String();
  return kv.getString(metadataKey(bookKey, "_anc").c_str(), String());
}

uint32_t loadSavedAnchorAt(KeyValueStore& kv, const String& bookKey) {
  if (!validBookKey(bookKey)) return 0U;
  int value = kv.getInt(metadataKey(bookKey, "_anc_at").c_str(), -1);
  return value < 0 ? 0U : (uint32_t)value;
}

void saveSavedAnchor(KeyValueStore& kv, const String& bookKey,
                     const String& anchor, uint32_t anchorAt) {
  if (!validBookKey(bookKey)) return;
  const String anchorKey = metadataKey(bookKey, "_anc");
  const String anchorAtKey = metadataKey(bookKey, "_anc_at");
  if (anchor.length() == 0 || anchorAt >= anchor.length()
      || anchor.length() > kBookmarkAnchorMaxBytes) {
    kv.remove(anchorKey.c_str());
    kv.remove(anchorAtKey.c_str());
    return;
  }
  kv.putString(anchorKey.c_str(), anchor);
  kv.putInt(anchorAtKey.c_str(), (int)anchorAt);
}

bool clearBookMetadata(KeyValueStore& kv, const String& bookKey) {
  if (!validBookKey(bookKey)) return false;
  kv.remove(metadataKey(bookKey, "_p").c_str());
  kv.remove(metadataKey(bookKey, "_off").c_str());
  kv.remove(metadataKey(bookKey, "_anc").c_str());
  kv.remove(metadataKey(bookKey, "_anc_at").c_str());
  return true;
}

void renameBookMetadata(KeyValueStore& kv, const String& oldKey, const String& newKey) {
  if (!validBookKey(oldKey) || !validBookKey(newKey) || oldKey == newKey) return;

  int progress = kv.getInt(metadataKey(oldKey, "_p").c_str(), -1);
  if (progress >= 0) {
    kv.putInt(metadataKey(newKey, "_p").c_str(), progress);
  }
  kv.remove(metadataKey(oldKey, "_p").c_str());

  const uint32_t offset = loadSavedOffset(kv, oldKey);
  if (offset != kOffsetUnset) {
    saveSavedOffset(kv, newKey, offset);
  }
  kv.remove(metadataKey(oldKey, "_off").c_str());

  String anchor = loadSavedAnchor(kv, oldKey);
  const uint32_t anchorAt = loadSavedAnchorAt(kv, oldKey);
  if (anchor.length() > 0 && anchorAt < anchor.length()
      && anchor.length() <= kBookmarkAnchorMaxBytes) {
    saveSavedAnchor(kv, newKey, anchor, anchorAt);
  }
  // Remove both keys even when the old anchor was empty or malformed, so
  // stale partial metadata cannot survive a rename.
  kv.remove(metadataKey(oldKey, "_anc").c_str());
  kv.remove(metadataKey(oldKey, "_anc_at").c_str());
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
