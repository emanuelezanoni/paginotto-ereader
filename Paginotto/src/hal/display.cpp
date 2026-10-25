#include "src/hal/display.h"

U8G2_FOR_ADAFRUIT_GFX u8g2;
HeltecGFXAdapter gfx(display);

void beginPageCanvas(bool clearMem) {
  if (clearMem) {
    display.clearMemory();
  }

  // The board API controls the physical orientation; U8g2 only needs the
  // foreground/background mode for transparent text over the GFX canvas.
  display.landscape();
  u8g2.setFontMode(1);
  u8g2.setForegroundColor(1);
  u8g2.setBackgroundColor(0);
}
