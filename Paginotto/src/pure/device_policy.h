#ifndef PAGINOTTO_PURE_DEVICE_POLICY_H
#define PAGINOTTO_PURE_DEVICE_POLICY_H

#include "src/pure/arduino_compat.h"

int clampIdleTimeoutSeconds(int seconds);
int batteryPercentFromVoltage(float volts);
int slewBatteryPercent(int shown, int measured, int max_step);
bool updateLowBatteryState(bool was_low, int percent);

#endif  // PAGINOTTO_PURE_DEVICE_POLICY_H
