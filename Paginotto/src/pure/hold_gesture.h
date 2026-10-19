#ifndef PALA_PURE_HOLD_GESTURE_H
#define PALA_PURE_HOLD_GESTURE_H

#include "src/pure/arduino_compat.h"  // uint32_t, uint8_t
#include "src/config.h"               // CLICK_HOLD_MS

// Hold classifier. A held press emits at the threshold while still down;
// release is never an action.

enum HoldGestureKind {
  HoldNone = 0,
  HoldClickHold = 1,
};

// Called while the press is down. `threshold_ms` is the hold threshold.
// Any release before threshold remains normal click-sequence input.
HoldGestureKind classifyHoldInProgress(uint32_t held_ms, uint8_t click_count);

#endif  // PALA_PURE_HOLD_GESTURE_H
