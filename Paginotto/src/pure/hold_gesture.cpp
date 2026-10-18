#include "src/pure/hold_gesture.h"

HoldGestureKind classifyHoldRelease(uint32_t dur_ms, uint8_t click_count) {
  if (dur_ms < LONG_MS) return HoldNone;
  if (click_count == 1) return HoldClickHold;
  if (click_count >= 2) return HoldNone;
  return HoldLong;
}

HoldGestureKind classifyHoldInProgress(uint32_t held_ms, uint8_t click_count) {
  if (click_count == 1 && held_ms >= LONG_MS) return HoldClickHold;
  return HoldNone;
}
