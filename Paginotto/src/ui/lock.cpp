#include "src/ui/lock.h"

#include "src/state.h"   // prefs

namespace Lock {

static constexpr const char* kKey = "cfg_locked";

static bool s_locked = false;

static void persistLockState(bool locked) {
  s_locked = locked;
  prefs.putBool(kKey, locked);
}

void loadSettings() {
  s_locked = prefs.getBool(kKey, false);
}

bool isLocked() { return s_locked; }

void engage() {
  if (s_locked) return;
  persistLockState(true);
}

void disengage() {
  if (!s_locked) return;
  persistLockState(false);
}

bool isUnlockGesture(const ButtonEvent& ev) {
  return ev.kind == ButtonEvent::Hold || ev.kind == ButtonEvent::ClickHold;
}

}  // namespace Lock
