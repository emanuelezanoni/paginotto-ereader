#include "src/ui/reader_actions.h"

#include "src/state.h"  // prefs

namespace Gestures {

static constexpr const char* kKeyClickHold = "cfg_btnCH";

// Defaults chosen to make the device useful out of the box:
//   clickhold = lock
static ButtonAction s_clickHold = ACTION_LOCK;

static ButtonAction clamp(int v) {
  if (v != ACTION_NONE && v != ACTION_LOCK) return ACTION_NONE;
  return (ButtonAction)v;
}

void loadSettings() {
  s_clickHold = clamp(prefs.getInt(kKeyClickHold, ACTION_LOCK));
}

ButtonAction actionClickHold() { return s_clickHold; }

static void persist(const char* key, ButtonAction& dest, ButtonAction value) {
  ButtonAction v = clamp(value);
  if (v == dest) return;
  dest = v;
  prefs.putInt(key, (int)v);
}

void setActionClickHold(ButtonAction a) { persist(kKeyClickHold, s_clickHold, a); }

ButtonAction actionFor(ButtonEvent::Kind kind) {
  switch (kind) {
    case ButtonEvent::ClickHold: return s_clickHold;
    default:                     return ACTION_NONE;
  }
}

}  // namespace Gestures
