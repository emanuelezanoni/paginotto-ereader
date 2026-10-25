#ifndef PAGINOTTO_PURE_UI_POLICY_H
#define PAGINOTTO_PURE_UI_POLICY_H

namespace UiPolicy {

bool deadlineActive(unsigned now, unsigned deadline);
int listWindowTop(int itemCount, int selectedIndex, int visibleRows);

}  // namespace UiPolicy

#endif  // PAGINOTTO_PURE_UI_POLICY_H
