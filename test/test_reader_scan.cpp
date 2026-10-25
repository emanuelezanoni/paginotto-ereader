#include "test_framework.h"
#include "src/pure/reader_scan.h"

TEST_CASE("reader scan starts complete when target is already at the cursor") {
  ReaderScanState state{4, 400, 4, true, false};
  CHECK(readerScanTargetReached(state));
  CHECK(advanceReaderScan(state, 500) == ReaderScanStep::TargetReached);
  CHECK(state.page == 4u);
  CHECK(state.offset == 400u);
}

TEST_CASE("reader scan reaches a page target after one page") {
  ReaderScanState state{4, 400, 5, true, false};
  CHECK(advanceReaderScan(state, 500) == ReaderScanStep::TargetReached);
  CHECK(state.page == 5u);
  CHECK(state.offset == 500u);
  CHECK(!state.eof);
}

TEST_CASE("reader scan reaches a byte target after crossing it") {
  ReaderScanState state{4, 400, 750, false, false};
  CHECK(advanceReaderScan(state, 500) == ReaderScanStep::Continue);
  CHECK(advanceReaderScan(state, 800) == ReaderScanStep::TargetReached);
  CHECK(state.page == 6u);
  CHECK(state.offset == 800u);
}

TEST_CASE("reader scan treats a non-progressing offset as EOF") {
  ReaderScanState state{4, 400, 900, false, false};
  CHECK(advanceReaderScan(state, 400) == ReaderScanStep::EndOfFile);
  CHECK(state.eof);
  CHECK(state.page == 4u);
  CHECK(state.offset == 400u);
}

TEST_CASE("reader scan never accepts a backward offset") {
  ReaderScanState state{4, 400, 900, false, false};
  CHECK(advanceReaderScan(state, 399) == ReaderScanStep::EndOfFile);
  CHECK(state.eof);
  CHECK(state.offset == 400u);
}
