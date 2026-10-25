#include "test_framework.h"
#include "src/pure/reader_resume.h"

TEST_CASE("reader resume starts cleanly without persisted position") {
  const ReaderResumePlan plan = makeReaderResumePlan(false, 0, 0, -1, 123);
  CHECK(!plan.opening);
  CHECK(!plan.targetIsPage);
  CHECK(plan.initialPage == 0);
  CHECK(plan.initialOffset == 0u);
}

TEST_CASE("reader resume uses canonical byte offset") {
  const ReaderResumePlan plan = makeReaderResumePlan(true, 900, 42, 3, 300);
  CHECK(plan.opening);
  CHECK(!plan.targetIsPage);
  CHECK(plan.initialPage == 0);
  CHECK(plan.initialOffset == 0u);
  CHECK(plan.scanPage == 3u);
  CHECK(plan.scanOffset == 300u);
  CHECK(plan.target == 900u);
}

TEST_CASE("reader resume accepts an exact cached byte offset") {
  const ReaderResumePlan plan = makeReaderResumePlan(true, 900, 42, 9, 900);
  CHECK(!plan.opening);
  CHECK(plan.initialPage == 9);
  CHECK(plan.initialOffset == 900u);
  CHECK(plan.scanPage == 9u);
  CHECK(plan.scanOffset == 900u);
}

TEST_CASE("reader resume rejects a cache entry ahead of the target") {
  const ReaderResumePlan plan = makeReaderResumePlan(true, 900, 42, 12, 1200);
  CHECK(plan.opening);
  CHECK(plan.initialPage == 0);
  CHECK(plan.initialOffset == 0u);
  CHECK(plan.scanPage == 0u);
  CHECK(plan.scanOffset == 0u);
  CHECK(plan.target == 900u);
}

TEST_CASE("reader resume falls back to legacy page hint") {
  const ReaderResumePlan plan = makeReaderResumePlan(false, 0, 12, 4, 400);
  CHECK(plan.opening);
  CHECK(plan.targetIsPage);
  CHECK(plan.initialPage == 4);
  CHECK(plan.initialOffset == 400u);
  CHECK(plan.scanPage == 4u);
  CHECK(plan.scanOffset == 400u);
  CHECK(plan.target == 12u);
}

TEST_CASE("reader resume keeps legacy page at the beginning when cache is absent") {
  const ReaderResumePlan plan = makeReaderResumePlan(false, 0, 12, -1, 400);
  CHECK(plan.opening);
  CHECK(plan.targetIsPage);
  CHECK(plan.initialPage == 0);
  CHECK(plan.initialOffset == 0u);
  CHECK(plan.scanPage == 0u);
  CHECK(plan.scanOffset == 0u);
  CHECK(plan.target == 12u);
}

TEST_CASE("reader resume ignores legacy page when canonical offset is zero") {
  const ReaderResumePlan plan = makeReaderResumePlan(true, 0, 99, 5, 500);
  CHECK(!plan.opening);
  CHECK(!plan.targetIsPage);
  CHECK(plan.initialPage == 0);
  CHECK(plan.initialOffset == 0u);
}
