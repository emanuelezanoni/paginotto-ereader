#include "src/pure/button_policy.h"

bool acceptsButtonEdge(bool edge_pressed, bool stable_pressed,
                       uint32_t edge_ms, uint32_t last_change_ms,
                       uint32_t debounce_ms) {
  if (edge_pressed == stable_pressed) return false;
  return static_cast<uint32_t>(edge_ms - last_change_ms) > debounce_ms;
}

ButtonClickKind clickKindForCount(uint8_t count) {
  switch (count) {
    case 1: return ButtonShortClick;
    case 2: return ButtonDoubleClick;
    case 3: return ButtonTripleClick;
    case 4: return ButtonQuadClick;
    default: return ButtonNoClick;
  }
}
