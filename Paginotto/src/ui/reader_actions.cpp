#include "src/ui/reader_actions.h"

#include "src/state.h"  // prefs

namespace Gestures {

static constexpr const char* kKeyLong      = "cfg_btnL";
static constexpr const char* kKeyClickHold = "cfg_btnCH";

// Defaults chosen to make the device useful out of the box:
//   long      = none
//   clickhold = menu     — easy chord, doesn't fight short-click paging
static ButtonAction s_long      = ACTION_NONE;
static ButtonAction s_clickHold = ACTION_MENU;

static ButtonAction clamp(int v) {
  if (v < ACTION_NONE || v > ACTION_MENU) return ACTION_NONE;
  return (ButtonAction)v;
}

void loadSettings() {
  s_long      = clamp(prefs.getInt(kKeyLong,      ACTION_NONE));
  s_clickHold = clamp(prefs.getInt(kKeyClickHold, ACTION_MENU));
}

ButtonAction actionLong()      { return s_long; }
ButtonAction actionClickHold() { return s_clickHold; }

static void persist(const char* key, ButtonAction& dest, ButtonAction value) {
  ButtonAction v = clamp(value);
  if (v == dest) return;
  dest = v;
  prefs.putInt(key, (int)v);
}

void setActionLong(ButtonAction a)      { persist(kKeyLong,      s_long,      a); }
void setActionClickHold(ButtonAction a) { persist(kKeyClickHold, s_clickHold, a); }

ButtonAction actionFor(ButtonEvent::Kind kind) {
  switch (kind) {
    case ButtonEvent::Long:      return s_long;
    case ButtonEvent::ClickHold: return s_clickHold;
    default:                     return ACTION_NONE;
  }
}

}  // namespace Gestures
