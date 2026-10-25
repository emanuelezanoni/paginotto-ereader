#include "test_framework.h"
#include "pure/ui_policy.h"

TEST_CASE("UI deadline policy is wrap-safe and one-shot compatible") {
  CHECK(UiPolicy::deadlineActive(100, 200));
  CHECK(!UiPolicy::deadlineActive(201, 200));
  CHECK(!UiPolicy::deadlineActive(100, 0));
  CHECK(UiPolicy::deadlineActive(0xFFFFFFF0u, 0x00000010u));
}

TEST_CASE("UI list policy centers selection and clamps ends") {
  CHECK(UiPolicy::listWindowTop(10, 0, 4) == 0);
  CHECK(UiPolicy::listWindowTop(10, 5, 4) == 3);
  CHECK(UiPolicy::listWindowTop(10, 9, 4) == 6);
  CHECK(UiPolicy::listWindowTop(0, 0, 4) == 0);
}
