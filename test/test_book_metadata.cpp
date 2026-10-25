#include "test_framework.h"
#include "map_kv_store.h"
#include "src/storage/book_metadata.h"

TEST_CASE("book metadata defaults are safe") {
  MapKvStore kv;
  CHECK(loadSavedPage(kv, "book") == 0);
  CHECK(loadSavedOffset(kv, "book") == kOffsetUnset);
  CHECK(loadSavedAnchor(kv, "book") == "");
  CHECK(loadSavedAnchorAt(kv, "book") == 0u);
  CHECK(!clearBookMetadata(kv, ""));
}

TEST_CASE("book metadata preserves progress and full uint32 offsets") {
  MapKvStore kv;
  saveSavedPage(kv, "book", -4);
  saveSavedOffset(kv, "book", 0xFFFFFFFFu);
  saveSavedAnchor(kv, "book", "chapter", 2);

  CHECK(loadSavedPage(kv, "book") == 0);
  CHECK(loadSavedOffset(kv, "book") == 0xFFFFFFFFu);
  CHECK(loadSavedAnchor(kv, "book") == "chapter");
  CHECK(loadSavedAnchorAt(kv, "book") == 2u);
}

TEST_CASE("invalid anchors are removed instead of persisted") {
  MapKvStore kv;
  saveSavedAnchor(kv, "book", "valid", 1);
  saveSavedAnchor(kv, "book", "", 0);
  CHECK(loadSavedAnchor(kv, "book") == "");
  CHECK(!kv.has("book_anc"));
  CHECK(!kv.has("book_anc_at"));

  String oversized;
  for (size_t i = 0; i < kBookmarkAnchorMaxBytes + 1; ++i) oversized += 'x';
  saveSavedAnchor(kv, "book", oversized, 0);
  CHECK(loadSavedAnchor(kv, "book") == "");
}

TEST_CASE("renaming moves all valid metadata and clears old keys") {
  MapKvStore kv;
  saveSavedPage(kv, "old", 7);
  saveSavedOffset(kv, "old", 0xF0000000u);
  saveSavedAnchor(kv, "old", "anchor", 3);

  renameBookMetadata(kv, "old", "new");

  CHECK(loadSavedPage(kv, "new") == 7);
  CHECK(loadSavedOffset(kv, "new") == 0xF0000000u);
  CHECK(loadSavedAnchor(kv, "new") == "anchor");
  CHECK(loadSavedAnchorAt(kv, "new") == 3u);
  CHECK(!kv.has("old_p"));
  CHECK(!kv.has("old_off"));
  CHECK(!kv.has("old_anc"));
  CHECK(!kv.has("old_anc_at"));
}

TEST_CASE("renaming malformed metadata does not leave orphan keys") {
  MapKvStore kv;
  saveSavedAnchor(kv, "old", "bad", 99);
  // Simulate a partial legacy record: anchor-at exists without an anchor.
  kv.putInt("old_anc_at", 4);
  renameBookMetadata(kv, "old", "new");

  CHECK(loadSavedAnchor(kv, "new") == "");
  CHECK(!kv.has("old_anc"));
  CHECK(!kv.has("old_anc_at"));
}

TEST_CASE("renaming a key onto itself is a no-op") {
  MapKvStore kv;
  saveSavedPage(kv, "same", 9);
  renameBookMetadata(kv, "same", "same");
  CHECK(loadSavedPage(kv, "same") == 9);
}
