#include "test_framework.h"
#include "pure/device_policy.h"

TEST_CASE("device policies clamp sleep timeout") {
  CHECK_EQ(clampIdleTimeoutSeconds(0), 10);
  CHECK_EQ(clampIdleTimeoutSeconds(120), 120);
  CHECK_EQ(clampIdleTimeoutSeconds(99999), 3600);
}

TEST_CASE("battery policy interpolates and clamps voltage") {
  CHECK_EQ(batteryPercentFromVoltage(4.30f), 100);
  CHECK_EQ(batteryPercentFromVoltage(3.30f), 0);
  CHECK_EQ(batteryPercentFromVoltage(2.90f), 0);
  CHECK_EQ(batteryPercentFromVoltage(4.075f), 85);
}

TEST_CASE("battery policy limits visible jumps and uses hysteresis") {
  CHECK_EQ(slewBatteryPercent(50, 10, 3), 47);
  CHECK_EQ(slewBatteryPercent(50, 90, 3), 53);
  CHECK(updateLowBatteryState(false, 8));
  CHECK(!updateLowBatteryState(false, 9));
  CHECK(!updateLowBatteryState(true, 11));
  CHECK(updateLowBatteryState(true, 12) == false);
}
