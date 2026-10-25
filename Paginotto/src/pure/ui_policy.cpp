#include "src/pure/ui_policy.h"

namespace UiPolicy {

bool deadlineActive(unsigned now, unsigned deadline) {
  return deadline != 0 && static_cast<int>(now - deadline) <= 0;
}

int listWindowTop(int itemCount, int selectedIndex, int visibleRows) {
  if (itemCount <= 0 || visibleRows <= 0) return 0;
  if (selectedIndex < 0) selectedIndex = 0;
  if (selectedIndex >= itemCount) selectedIndex = itemCount - 1;
  int top = selectedIndex - (visibleRows / 2);
  if (top < 0) top = 0;
  const int lastTop = itemCount - visibleRows;
  if (top > lastTop) top = lastTop > 0 ? lastTop : 0;
  return top;
}

}  // namespace UiPolicy
