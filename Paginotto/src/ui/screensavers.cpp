#include "src/ui/screensavers.h"

#include <esp_random.h>

#include "src/config.h"
#include "src/hal/display.h"
#include "src/pure/screensaver_policy.h"
#include "src/state.h"

namespace Screensavers {
namespace {

constexpr const char* kDirectory = "/screensavers";
constexpr const char* kLegacyImage = "/sleep.bin";
constexpr const char* kModeKey = "cfg_ss_mode";
constexpr const char* kCursorKey = "cfg_ss_idx";
constexpr const char* kPreviousKey = "cfg_ss_last";

struct Runtime {
  Mode mode = Mode::Single;
  int cursor = 0;
  int previous = -1;
};

Runtime s_runtime;

bool validSlot(int slot) { return slot >= 0 && slot < MAX_SLOTS; }

void ensureDirectory() {
  if (!FS.exists(kDirectory)) FS.mkdir(kDirectory);
}

void resetRotation() {
  s_runtime.cursor = 0;
  s_runtime.previous = -1;
  prefs.putInt(kCursorKey, 0);
  prefs.putInt(kPreviousKey, -1);
}

bool hasExpectedSize(const String& path) {
  File file = FS.open(path, "r");
  const bool valid = file &&
      file.size() == static_cast<size_t>(SCREENSAVER_BYTES);
  if (file) file.close();
  return valid;
}

bool readExact(const String& path, uint8_t* buffer) {
  if (!buffer || !hasExpectedSize(path)) return false;
  File file = FS.open(path, "r");
  if (!file) return false;
  const size_t received = file.read(buffer, SCREENSAVER_BYTES);
  file.close();
  return received == static_cast<size_t>(SCREENSAVER_BYTES);
}

int collectPopulated(int* slots) {
  if (!slots) return 0;
  int count = 0;
  for (int slot = 0; slot < MAX_SLOTS; ++slot) {
    if (slotExists(slot)) slots[count++] = slot;
  }
  return count;
}

bool paint(const uint8_t* image) {
  if (!image) return false;
  gfx.fillScreen(0);
  gfx.drawXBitmap(0, 0, image, SCREEN_W, SCREEN_H, 1);
  return true;
}

}  // namespace

String slotPath(int slot) {
  char path[32];
  snprintf(path, sizeof(path), "%s/%d.bin", kDirectory, slot);
  return String(path);
}

bool slotExists(int slot) {
  return validSlot(slot) && hasExpectedSize(slotPath(slot));
}

int populatedCount() {
  int slots[MAX_SLOTS];
  return collectPopulated(slots);
}

int firstFreeSlot() {
  for (int slot = 0; slot < MAX_SLOTS; ++slot) {
    if (!slotExists(slot)) return slot;
  }
  return -1;
}

bool readSlot(int slot, uint8_t out[SCREENSAVER_BYTES]) {
  return validSlot(slot) && readExact(slotPath(slot), out);
}

void loadSettings() {
  ensureDirectory();
  const int storedMode = prefs.getInt(kModeKey, static_cast<int>(Mode::Single));
  s_runtime.mode = ScreensaverPolicy::validMode(storedMode)
      ? static_cast<Mode>(storedMode) : Mode::Single;
  s_runtime.cursor = prefs.getInt(kCursorKey, 0);
  if (s_runtime.cursor < 0) s_runtime.cursor = 0;
  s_runtime.previous = prefs.getInt(kPreviousKey, -1);
}

void setMode(Mode mode) {
  const int encoded = static_cast<int>(mode);
  s_runtime.mode = ScreensaverPolicy::validMode(encoded)
      ? mode : Mode::Single;
  prefs.putInt(kModeKey, static_cast<int>(s_runtime.mode));
  resetRotation();
}

Mode currentMode() { return s_runtime.mode; }

bool installFromTemp(int slot, const String& tmpPath) {
  if (!validSlot(slot) || tmpPath.length() == 0 ||
      !hasExpectedSize(tmpPath)) {
    if (tmpPath.length() > 0 && FS.exists(tmpPath)) FS.remove(tmpPath);
    return false;
  }
  ensureDirectory();
  const String destination = slotPath(slot);
  if (FS.exists(destination)) FS.remove(destination);
  if (!FS.rename(tmpPath, destination)) {
    if (FS.exists(tmpPath)) FS.remove(tmpPath);
    return false;
  }
  resetRotation();
  return true;
}

void deleteSlot(int slot) {
  if (!validSlot(slot)) return;
  const String path = slotPath(slot);
  if (FS.exists(path)) FS.remove(path);
  resetRotation();
}

bool drawNext() {
  static uint8_t image[SCREENSAVER_BYTES];
  if (s_runtime.mode == Mode::Single) {
    return readExact(kLegacyImage, image) && paint(image);
  }

  int slots[MAX_SLOTS];
  const int count = collectPopulated(slots);
  if (count == 0) return false;

  ScreensaverPolicy::Selection selection;
  if (s_runtime.mode == Mode::Shuffle) {
    selection = ScreensaverPolicy::chooseShuffle(
        slots, count, s_runtime.previous, esp_random());
  } else {
    selection = ScreensaverPolicy::chooseCycle(
        slots, count, s_runtime.cursor);
    s_runtime.cursor = selection.nextCycleIndex;
    prefs.putInt(kCursorKey, s_runtime.cursor);
  }
  if (!readSlot(selection.slot, image)) return false;

  s_runtime.previous = selection.lastShown;
  prefs.putInt(kPreviousKey, s_runtime.previous);
  return paint(image);
}

}  // namespace Screensavers
