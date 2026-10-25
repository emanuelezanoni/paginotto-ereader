#include "src/pure/device_policy.h"

int clampIdleTimeoutSeconds(int seconds) {
  if (seconds < 10) return 10;
  if (seconds > 3600) return 3600;
  return seconds;
}

int batteryPercentFromVoltage(float volts) {
  struct Point { float volts; int percent; };
  static const Point curve[] = {
      {4.20f, 100}, {4.15f, 95}, {4.10f, 90}, {4.05f, 80},
      {4.00f, 70},  {3.95f, 60}, {3.90f, 50}, {3.85f, 40},
      {3.80f, 30},  {3.75f, 20}, {3.70f, 15}, {3.65f, 10},
      {3.60f, 5},   {3.50f, 2},  {3.30f, 0},
  };
  constexpr int count = sizeof(curve) / sizeof(curve[0]);
  if (volts >= curve[0].volts) return 100;
  if (volts <= curve[count - 1].volts) return 0;

  for (int i = 0; i < count - 1; ++i) {
    if (volts > curve[i].volts || volts < curve[i + 1].volts) continue;
    const float fraction = (volts - curve[i + 1].volts) /
                           (curve[i].volts - curve[i + 1].volts);
    int result = static_cast<int>(curve[i + 1].percent +
                                  fraction * (curve[i].percent - curve[i + 1].percent) +
                                  0.5f);
    if (result < 0) result = 0;
    if (result > 100) result = 100;
    return result;
  }
  return 0;
}

int slewBatteryPercent(int shown, int measured, int max_step) {
  if (max_step < 0) max_step = 0;
  int delta = measured - shown;
  if (delta < -max_step) delta = -max_step;
  if (delta > max_step) delta = max_step;
  int result = shown + delta;
  if (result < 0) return 0;
  if (result > 100) return 100;
  return result;
}

bool updateLowBatteryState(bool was_low, int percent) {
  return was_low ? percent < 12 : percent <= 8;
}
