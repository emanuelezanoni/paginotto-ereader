#ifndef PAGINOTTO_HAL_BATTERY_H
#define PAGINOTTO_HAL_BATTERY_H

#include "src/config.h"
#include "src/state.h"

#if HAS_BATTERY
void adcSetupOnce();
void updateBatteryCached(bool force = false);
void drawBatteryTopRight();
#endif

#endif  // PAGINOTTO_HAL_BATTERY_H
