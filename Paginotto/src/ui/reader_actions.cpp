#include "src/ui/reader_actions.h"

#include "src/state.h"  // prefs

namespace Gestures {

static constexpr const char* kKeyClickHold = "cfg_btnCH";
static constexpr const char* kKeyHold = "cfg_btnH";
static constexpr const char* kKeyVersion = "cfg_btnV2";

// Defaults chosen to make the device useful out of the box:
static ButtonAction s_hold = ACTION_LOCK;
static ButtonAction s_clickHold = ACTION_NONE;

static ButtonAction clamp(int v) {
  if (v != ACTION_NONE && v != ACTION_LOCK) return ACTION_NONE;
  return (ButtonAction)v;
}

void loadSettings() {
  if (!prefs.isKey(kKeyVersion)) {
    // One-time migration: the old firmware had only Click, then hold and
    // defaulted it to Lock. The new defaults intentionally move Lock to the
    // standalone Hold gesture and leave the chord unbound.
    s_hold = ACTION_LOCK;
    s_clickHold = ACTION_NONE;
    prefs.putInt(kKeyHold, (int)s_hold);
    prefs.putInt(kKeyClickHold, (int)s_clickHold);
    prefs.putBool(kKeyVersion, true);
  } else {
    s_hold = clamp(prefs.getInt(kKeyHold, ACTION_LOCK));
    s_clickHold = clamp(prefs.getInt(kKeyClickHold, ACTION_NONE));
  }
}

ButtonAction actionHold() { return s_hold; }
ButtonAction actionClickHold() { return s_clickHold; }

static void persist(const char* key, ButtonAction& dest, ButtonAction value) {
  ButtonAction v = clamp(value);
  if (v == dest) return;
  dest = v;
  prefs.putInt(key, (int)v);
}

void setActionHold(ButtonAction a) { persist(kKeyHold, s_hold, a); }
void setActionClickHold(ButtonAction a) { persist(kKeyClickHold, s_clickHold, a); }

ButtonAction actionFor(ButtonEvent::Kind kind) {
  switch (kind) {
    case ButtonEvent::Hold:      return s_hold;
    case ButtonEvent::ClickHold: return s_clickHold;
    default:                     return ACTION_NONE;
  }
}

}  // namespace Gestures
