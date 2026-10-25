#include "test_framework.h"
#include "pure/screensaver_policy.h"

TEST_CASE("screensaver policy accepts only known modes") {
  CHECK(ScreensaverPolicy::validMode(0));
  CHECK(ScreensaverPolicy::validMode(1));
  CHECK(ScreensaverPolicy::validMode(2));
  CHECK(!ScreensaverPolicy::validMode(-1));
  CHECK(!ScreensaverPolicy::validMode(3));
}

TEST_CASE("screensaver cycle normalizes cursor and advances") {
  const int slots[] = {1, 4, 7};
  auto first = ScreensaverPolicy::chooseCycle(slots, 3, -2);
  CHECK(first.slot == 1);
  CHECK(first.nextCycleIndex == 1);
  auto last = ScreensaverPolicy::chooseCycle(slots, 3, 2);
  CHECK(last.slot == 7);
  CHECK(last.nextCycleIndex == 0);
}

TEST_CASE("screensaver shuffle avoids immediate repeat") {
  const int slots[] = {2, 5, 9};
  auto pick = ScreensaverPolicy::chooseShuffle(slots, 3, 5, 1);
  CHECK(pick.slot == 9);
  CHECK(pick.lastShown == 9);
}

TEST_CASE("screensaver policy handles empty populations") {
  auto cycle = ScreensaverPolicy::chooseCycle(nullptr, 0, 0);
  auto shuffle = ScreensaverPolicy::chooseShuffle(nullptr, 0, -1, 0);
  CHECK(cycle.slot == -1);
  CHECK(shuffle.slot == -1);
}
