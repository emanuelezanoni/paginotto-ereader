#ifndef PALA_UI_READER_ACTIONS_H
#define PALA_UI_READER_ACTIONS_H

#include "src/hal/input.h"  // ButtonEvent::Kind

// ============================================================================
//  Reader hold-gesture bindings
//
//  Maps ClickHold to its remappable action and
//  persists the choice. Lives here, not in `src/hal/input.h`, because the
//  bindable actions are reader-screen concepts — the input layer just
//  classifies button events; what an event *means* belongs to the screen
//  that consumes it. If another screen ever wants its own gesture-to-action
//  table it can do so without touching the HAL.
//
//  Defaults (chosen to be useful out of the box):
//    ClickHold = Lock
// ============================================================================

enum ButtonAction {
  ACTION_NONE     = 0,
  ACTION_LOCK     = 2,
};

namespace Gestures {

// NVS load on boot — call once from setup() after `prefs.begin`.
void loadSettings();

// Current ClickHold action.
ButtonAction actionClickHold();

// Apply + persist a binding. Out-of-range values clamp to ACTION_NONE.
void setActionClickHold(ButtonAction a);

// Convenience: which action (if any) is currently bound to the gesture
// kind that just fired. Returns ACTION_NONE for all except ClickHold.
ButtonAction actionFor(ButtonEvent::Kind kind);

}  // namespace Gestures

#endif  // PALA_UI_READER_ACTIONS_H
