#ifndef PAGINOTTO_PURE_SCREENSAVER_POLICY_H
#define PAGINOTTO_PURE_SCREENSAVER_POLICY_H

namespace ScreensaverPolicy {

enum class Mode { Single = 0, Cycle = 1, Shuffle = 2 };

struct Selection {
  int slot = -1;
  int nextCycleIndex = 0;
  int lastShown = -1;
};

bool validMode(int value);
Selection chooseCycle(const int* populated, int count, int cursor);
Selection chooseShuffle(const int* populated, int count, int lastShown,
                        unsigned randomValue);

}  // namespace ScreensaverPolicy

#endif  // PAGINOTTO_PURE_SCREENSAVER_POLICY_H
