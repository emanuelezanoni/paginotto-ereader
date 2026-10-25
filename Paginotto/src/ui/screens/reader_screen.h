#ifndef PAGINOTTO_UI_SCREENS_READER_SCREEN_H
#define PAGINOTTO_UI_SCREENS_READER_SCREEN_H

#include "src/ui/screen.h"

class ReaderScreen : public Screen {
public:
  void onEnter() override;
  void onButton(const ButtonEvent& e) override;
  void draw() override;
  void onIdleTick() override;
  void onSleep() override;
};

extern ReaderScreen g_readerScreen;

#endif  // PAGINOTTO_UI_SCREENS_READER_SCREEN_H
