// Tests for src/pure/hold_gesture.cpp.
//
// ClickHold classifier. On-device input.cpp drives it from ISR state.

#include "test_framework.h"
#include "pure/hold_gesture.h"

TEST_CASE("classifyHoldInProgress — below thresholds is None") {
  CHECK_EQ(classifyHoldInProgress(0,                     0), HoldNone);
  CHECK_EQ(classifyHoldInProgress(CLICK_HOLD_MS - 1,     0), HoldNone);
  CHECK_EQ(classifyHoldInProgress(10 * CLICK_HOLD_MS,    0), HoldNone);
}

TEST_CASE("classifyHoldInProgress — hold fires while held") {
  CHECK_EQ(classifyHoldInProgress(CLICK_HOLD_MS,         0), HoldClickHold);
  CHECK_EQ(classifyHoldInProgress(CLICK_HOLD_MS + 1,     0), HoldClickHold);
  CHECK_EQ(classifyHoldInProgress(10 * CLICK_HOLD_MS,    0), HoldClickHold);
  CHECK_EQ(classifyHoldInProgress(CLICK_HOLD_MS,         1), HoldClickHold);
}

TEST_CASE("classifyHoldInProgress — multi-click held is None") {
  CHECK_EQ(classifyHoldInProgress(CLICK_HOLD_MS,         2), HoldNone);
  CHECK_EQ(classifyHoldInProgress(10 * CLICK_HOLD_MS,    2), HoldNone);
  CHECK_EQ(classifyHoldInProgress(10 * CLICK_HOLD_MS,    3), HoldNone);
}
