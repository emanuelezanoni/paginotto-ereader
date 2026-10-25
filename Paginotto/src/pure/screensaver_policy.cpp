#include "src/pure/screensaver_policy.h"

namespace ScreensaverPolicy {

bool validMode(int value) {
  return value >= static_cast<int>(Mode::Single) &&
         value <= static_cast<int>(Mode::Shuffle);
}

Selection chooseCycle(const int* populated, int count, int cursor) {
  Selection result;
  if (!populated || count <= 0) return result;
  int index = cursor;
  if (index < 0 || index >= count) index = 0;
  result.slot = populated[index];
  result.nextCycleIndex = (index + 1) % count;
  result.lastShown = result.slot;
  return result;
}

Selection chooseShuffle(const int* populated, int count, int lastShown,
                        unsigned randomValue) {
  Selection result;
  if (!populated || count <= 0) return result;
  int index = static_cast<int>(randomValue % static_cast<unsigned>(count));
  if (count > 1 && populated[index] == lastShown) index = (index + 1) % count;
  result.slot = populated[index];
  result.lastShown = result.slot;
  return result;
}

}  // namespace ScreensaverPolicy
