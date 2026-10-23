#ifndef PALA_STORAGE_BOOK_METADATA_H
#define PALA_STORAGE_BOOK_METADATA_H

#include "src/storage/kv_store.h"
#include <stdint.h>

static constexpr uint32_t kOffsetUnset = 0xFFFFFFFFu;
static constexpr size_t kBookmarkAnchorMaxBytes = 192;

// ============================================================================
//  Testable KV-store-backed API (host-compatible)
// ============================================================================

// Per-book reading progress.
//
// The canonical position is the **byte offset** in the source file — that's
// invariant under font/layout changes. The page-number sibling is kept as a
// stale display hint (used by the web file list and as a legacy fallback for
// books saved by older firmware that didn't store offsets).
//
// `loadSavedOffset` returns `kOffsetUnset` if no offset has been saved yet.
int      loadSavedPage(KeyValueStore& kv, const String& bookKey);
void     saveSavedPage(KeyValueStore& kv, const String& bookKey, int pageIndex);
uint32_t loadSavedOffset(KeyValueStore& kv, const String& bookKey);
void     saveSavedOffset(KeyValueStore& kv, const String& bookKey, uint32_t byteOffset);
String   loadSavedAnchor(KeyValueStore& kv, const String& bookKey);
uint32_t loadSavedAnchorAt(KeyValueStore& kv, const String& bookKey);
void     saveSavedAnchor(KeyValueStore& kv, const String& bookKey,
                         const String& anchor, uint32_t anchorAt);

// Remove all progress metadata for one book.
bool clearBookMetadata(KeyValueStore& kv, const String& bookKey);

// Move progress metadata from oldKey to newKey.
void renameBookMetadata(KeyValueStore& kv, const String& oldKey, const String& newKey);

#ifdef ARDUINO
// ============================================================================
//  Firmware glue — wraps the testable API with a PreferencesStore around
//  the global `prefs` so callers can keep their existing call sites.
// ============================================================================
#include "src/config.h"
#include "src/state.h"

int  savedPageForBookPath(const String& path);


// Per-book lifecycle. Compose the pure NVS operations above with the
// matching page-cache file work in storage/page_cache.h. They do NOT
// touch device wake state or `g_bookview` — those are higher-level
// concerns and callers run in contexts (web during upload) where the
// reader has already been fully cleared by `LibraryScreen::onEnter`.
void deleteBookMetadata(const String& path);
void migrateBookMetadata(const String& oldPath, const String& newPath);

#endif  // ARDUINO

#endif  // PALA_STORAGE_BOOK_METADATA_H
