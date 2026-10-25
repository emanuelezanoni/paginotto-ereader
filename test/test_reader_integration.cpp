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
  CHECK(decision.savedOffset == 123456u);
}

TEST_CASE("layout remapping is not requested for inactive or invalid views") {
  CHECK(!decideReaderLayoutRemap(false, true, 2, 10).remap);
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
