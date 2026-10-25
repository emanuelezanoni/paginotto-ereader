#include "test_framework.h"
#include "src/pure/reader_navigation.h"

TEST_CASE("reader advance moves to a valid next page") {
  const ReaderAdvanceDecision d = decideReaderAdvance(3, 300, 450, 1000, false);
  CHECK(d.moved);
  CHECK(!d.eof);
  CHECK(d.page == 4u);
  CHECK(d.offset == 450u);
}

TEST_CASE("reader advance stops when already at EOF") {
  const ReaderAdvanceDecision d = decideReaderAdvance(3, 300, 450, 1000, true);
  CHECK(!d.moved);
  CHECK(d.eof);
  CHECK(d.page == 3u);
  CHECK(d.offset == 300u);
}

TEST_CASE("reader advance rejects non-progressing offsets") {
  const ReaderAdvanceDecision same = decideReaderAdvance(3, 300, 300, 1000, false);
  const ReaderAdvanceDecision backwards = decideReaderAdvance(3, 300, 299, 1000, false);
  CHECK(!same.moved);
  CHECK(same.eof);
  CHECK(!backwards.moved);
  CHECK(backwards.eof);
}

TEST_CASE("reader advance treats file end as no next page") {
  const ReaderAdvanceDecision d = decideReaderAdvance(3, 300, 1000, 1000, false);
  CHECK(!d.moved);
  CHECK(d.eof);
  CHECK(d.page == 3u);
  CHECK(d.offset == 300u);
}

TEST_CASE("reader advance handles empty files safely") {
  const ReaderAdvanceDecision d = decideReaderAdvance(0, 0, 0, 0, false);
  CHECK(!d.moved);
  CHECK(d.eof);
}

TEST_CASE("reader retreat stops at the first page") {
  const ReaderRetreatDecision d = decideReaderRetreat(0);
  CHECK(!d.moved);
  CHECK(d.targetPage == 0);
}

TEST_CASE("reader retreat selects the previous page") {
  const ReaderRetreatDecision d = decideReaderRetreat(7);
  CHECK(d.moved);
  CHECK(d.targetPage == 6);
}
