#include "test_framework.h"
#include "src/pure/reader_integration.h"

TEST_CASE("wake intent is empty and cleared when no session is armed") {
  const ReaderWakeDecision decision = decideReaderWake("", true);
  CHECK(decision.action == ReaderWakeAction::None);
  CHECK(decision.clearIntent);
}

TEST_CASE("wake intent restores an existing book exactly once") {
  const ReaderWakeDecision decision = decideReaderWake("/books/a.txt", true);
  CHECK(decision.action == ReaderWakeAction::Restore);
  CHECK(decision.clearIntent);
}

TEST_CASE("wake intent is consumed when the book disappeared") {
  const ReaderWakeDecision decision = decideReaderWake("/books/gone.txt", false);
  CHECK(decision.action == ReaderWakeAction::ConsumeInvalid);
  CHECK(decision.clearIntent);
}

TEST_CASE("layout changes preserve the byte position but require remapping") {
  const ReaderLayoutDecision decision =
      decideReaderLayoutRemap(true, true, 7, 123456u);
  CHECK(decision.remap);
  CHECK(decision.invalidateRecentOffsets);
  CHECK(decision.savedOffset == 123456u);
}

TEST_CASE("layout remapping is not requested for inactive or invalid views") {
  CHECK(!decideReaderLayoutRemap(false, true, 2, 10).remap);
  CHECK(!decideReaderLayoutRemap(false, true, 2, 10).invalidateRecentOffsets);
  CHECK(!decideReaderLayoutRemap(true, false, 2, 10).remap);
  CHECK(!decideReaderLayoutRemap(true, true, -1, 10).remap);
}

TEST_CASE("WebUI jump parsing supports offsets beyond signed int") {
  CHECK(parseReaderJumpOffset("2147483648") == 2147483648LL);
  CHECK(parseReaderJumpOffset("-7") == -7);
  CHECK(parseReaderJumpOffset("not-a-number") == 0);
}

TEST_CASE("WebUI jump clamps negative and past-end offsets") {
  CHECK(decideReaderJump(-7, 100).offset == 0u);
  CHECK(decideReaderJump(-7, 100).clamped);
  CHECK(decideReaderJump(1000, 100).offset == 99u);
  CHECK(decideReaderJump(1000, 100).clamped);
  CHECK(decideReaderJump(42, 100).offset == 42u);
  CHECK(!decideReaderJump(42, 100).clamped);
}

TEST_CASE("WebUI jump on an empty file remains at zero") {
  const ReaderJumpDecision decision = decideReaderJump(42, 0);
  CHECK(decision.emptyFile);
  CHECK(decision.offset == 0u);
}

TEST_CASE("bookmark is never written while a book is still opening") {
  CHECK(readerBookmarkWritable(true, false));
  CHECK(!readerBookmarkWritable(true, true));   // scan cursor != reader position
  CHECK(!readerBookmarkWritable(false, false));
  CHECK(!readerBookmarkWritable(false, true));
}

TEST_CASE("locked: unlock gesture unlocks") {
  const LockedLoopDecision d = decideLockedLoop(true, true, true, 5000, 1500,
                                                false, false);
  CHECK(d.unlock);
  CHECK(!d.sleep);
}

TEST_CASE("locked: a failed unlock press re-arms idle instead of sleeping") {
  // Regression: a short unlock hold used to count as "second tap -> sleep",
  // which persisted a mid-scan view and reset the bookmark.
  const LockedLoopDecision d = decideLockedLoop(false, true, true, 5000, 1500,
                                                false, false);
  CHECK(!d.unlock);
  CHECK(d.rearmIdle);
  CHECK(!d.sleep);
}

TEST_CASE("locked: idle timeout sleeps only when no gesture can complete") {
  CHECK(decideLockedLoop(false, false, true, 1501, 1500, false, false).sleep);
  CHECK(!decideLockedLoop(false, false, true, 1500, 1500, false, false).sleep);
  CHECK(!decideLockedLoop(false, false, true, 9000, 1500, true, false).sleep);
  CHECK(!decideLockedLoop(false, false, true, 9000, 1500, false, true).sleep);
  CHECK(!decideLockedLoop(false, false, false, 9000, 1500, false, false).sleep);
}
