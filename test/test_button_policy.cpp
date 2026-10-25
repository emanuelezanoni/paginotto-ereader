#include "test_framework.h"
#include "pure/button_policy.h"

TEST_CASE("button edge policy rejects duplicates and debounce bounce") {
  CHECK(!acceptsButtonEdge(true, true, 100, 0, 14));
  CHECK(!acceptsButtonEdge(false, true, 110, 100, 14));
  CHECK(acceptsButtonEdge(false, true, 115, 100, 14));
  CHECK(acceptsButtonEdge(true, false, 20, 0xfffffff0u, 14));
}

TEST_CASE("button click policy maps only supported click counts") {
  CHECK_EQ(clickKindForCount(0), ButtonNoClick);
  CHECK_EQ(clickKindForCount(1), ButtonShortClick);
  CHECK_EQ(clickKindForCount(2), ButtonDoubleClick);
  CHECK_EQ(clickKindForCount(3), ButtonTripleClick);
  CHECK_EQ(clickKindForCount(4), ButtonQuadClick);
  CHECK_EQ(clickKindForCount(5), ButtonNoClick);
}
