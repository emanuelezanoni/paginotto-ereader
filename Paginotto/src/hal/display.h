#ifndef PAGINOTTO_HAL_DISPLAY_H
#define PAGINOTTO_HAL_DISPLAY_H

#include <Adafruit_GFX.h>
#include <U8g2_for_Adafruit_GFX.h>

#include "src/config.h"
#include "src/state.h"

// Product-owned bridge between Adafruit_GFX and the Heltec panel. The panel
// driver remains a third-party dependency; this class owns only the firmware's
// coordinate and monochrome-colour policy.
class HeltecGFXAdapter : public Adafruit_GFX {
public:
  explicit HeltecGFXAdapter(EInkDisplay& panel)
      : Adafruit_GFX(SCREEN_W, SCREEN_H), panel_(panel) {}

  void drawPixel(int16_t x, int16_t y, uint16_t color) override {
    if (!inside(x, y)) return;

    // The physical panel is mounted upside down relative to the logical GFX
    // coordinate system. Keep the transform here so all callers share it.
    const int16_t panelX = static_cast<int16_t>(SCREEN_W - 1 - x);
    const int16_t panelY = static_cast<int16_t>(SCREEN_H - 1 - y);
    panel_.drawPixel(panelX, panelY, color == 0 ? WHITE : BLACK);
  }

private:
  static bool inside(int16_t x, int16_t y) {
    return x >= 0 && y >= 0 && x < SCREEN_W && y < SCREEN_H;
  }

  EInkDisplay& panel_;
};

extern HeltecGFXAdapter gfx;
extern U8G2_FOR_ADAFRUIT_GFX u8g2;

// ============================================================================
//  Drawing primitives
// ============================================================================

// Set up the device for any drawing pass: clear the offscreen buffer (unless
// the caller has already managed that) and put u8g2 into the project's
// transparent-foreground mode. Higher-level helpers (drawCenter,
// prepareMenuFrame in ui/widgets.h) call this internally.
void beginPageCanvas(bool clearMem = true);

#endif  // PAGINOTTO_HAL_DISPLAY_H
