#ifndef PAGINOTTO_PURE_BUTTON_POLICY_H
#define PAGINOTTO_PURE_BUTTON_POLICY_H

#include "src/pure/arduino_compat.h"

// Decisions shared by the hardware-facing input loop and host tests. The
// GPIO/ISR implementation stays in hal/input.cpp; these rules do not depend
// on Arduino or ESP-IDF.
enum ButtonClickKind {
  ButtonNoClick = 0,
  ButtonShortClick,
  ButtonDoubleClick,
  ButtonTripleClick,
  ButtonQuadClick,
};

bool acceptsButtonEdge(bool edge_pressed, bool stable_pressed,
                       uint32_t edge_ms, uint32_t last_change_ms,
                       uint32_t debounce_ms);

ButtonClickKind clickKindForCount(uint8_t count);

#endif  // PAGINOTTO_PURE_BUTTON_POLICY_H
