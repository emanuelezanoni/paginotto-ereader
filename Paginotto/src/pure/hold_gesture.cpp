#include "src/pure/hold_gesture.h"

HoldGestureKind classifyHoldInProgress(uint32_t held_ms, uint8_t click_count) {
  // A hold is a gesture in its own right.  It must not require a preceding
  // click: that made locking appear to happen only after release (or not at
  // all) and left the input classifier waiting for a second gesture.
  if (click_count <= 1 && held_ms >= CLICK_HOLD_MS) return HoldClickHold;
  return HoldNone;
}
