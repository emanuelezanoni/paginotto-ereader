// Tests for src/pure/hold_gesture.cpp.
//
// The classifier is the deterministic heart of the two remappable hold
// gestures (Long / ClickHold). The on-device input.cpp drives it from the
// ISR-fed state machine; here we exercise it directly.

#include "test_framework.h"
#include "pure/hold_gesture.h"

// ============================================================================
//  Release-time classifier
// ============================================================================

TEST_CASE("classifyHoldRelease — short release is not a hold") {
  CHECK_EQ(classifyHoldRelease(0,             0), HoldNone);
  CHECK_EQ(classifyHoldRelease(LONG_MS - 1,   0), HoldNone);
}

TEST_CASE("classifyHoldRelease — solo hold at LONG_MS is Long") {
  CHECK_EQ(classifyHoldRelease(LONG_MS,                  0), HoldLong);
  CHECK_EQ(classifyHoldRelease(LONG_MS + 1,              0), HoldLong);
  CHECK_EQ(classifyHoldRelease(10 * LONG_MS,             0), HoldLong);
}

TEST_CASE("classifyHoldRelease — click then hold of any length is ClickHold") {
  CHECK_EQ(classifyHoldRelease(LONG_MS,                  1), HoldClickHold);
  CHECK_EQ(classifyHoldRelease(10 * LONG_MS,             1), HoldClickHold);
}

TEST_CASE("classifyHoldRelease — short release with pending click is None") {
  // Below LONG_MS the release is just another click in the sequence, not
  // a hold. The accumulator handles it, not the hold classifier.
  CHECK_EQ(classifyHoldRelease(LONG_MS - 1,              1), HoldNone);
  CHECK_EQ(classifyHoldRelease(0,                        1), HoldNone);
}

TEST_CASE("classifyHoldRelease — multi-click followed by hold is None") {
  // Tap-tap-hold isn't a defined gesture. The pending two clicks commit
  // on their own; the hold contributes nothing.
  CHECK_EQ(classifyHoldRelease(LONG_MS,                  2), HoldNone);
  CHECK_EQ(classifyHoldRelease(10 * LONG_MS,             2), HoldNone);
  CHECK_EQ(classifyHoldRelease(LONG_MS,                  3), HoldNone);
  CHECK_EQ(classifyHoldRelease(LONG_MS,                255), HoldNone);
}

// ============================================================================
//  In-progress (still-pressed) classifier
// ============================================================================

TEST_CASE("classifyHoldInProgress — below thresholds is None") {
  CHECK_EQ(classifyHoldInProgress(0,                     0), HoldNone);
  CHECK_EQ(classifyHoldInProgress(LONG_MS - 1,           0), HoldNone);
  CHECK_EQ(classifyHoldInProgress(10 * LONG_MS,          0), HoldNone);
}

TEST_CASE("classifyHoldInProgress — plain Long is deferred to release") {
  // Release-time classifier handles Long once the press ends.
  CHECK_EQ(classifyHoldInProgress(LONG_MS,               0), HoldNone);
  CHECK_EQ(classifyHoldInProgress(LONG_MS + 100,         0), HoldNone);
  CHECK_EQ(classifyHoldInProgress(10 * LONG_MS,          0), HoldNone);
}

TEST_CASE("classifyHoldInProgress — ClickHold fires at LONG_MS with pending click") {
  CHECK_EQ(classifyHoldInProgress(LONG_MS,               1), HoldClickHold);
  CHECK_EQ(classifyHoldInProgress(LONG_MS + 1,           1), HoldClickHold);
  CHECK_EQ(classifyHoldInProgress(10 * LONG_MS,          1), HoldClickHold);
}

TEST_CASE("classifyHoldInProgress — multi-click held is None") {
  // Same rationale as release-time: tap-tap-hold etc. isn't a gesture.
  CHECK_EQ(classifyHoldInProgress(LONG_MS,               2), HoldNone);
  CHECK_EQ(classifyHoldInProgress(10 * LONG_MS,          2), HoldNone);
  CHECK_EQ(classifyHoldInProgress(10 * LONG_MS,          3), HoldNone);
}
