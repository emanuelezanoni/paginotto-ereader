#include "src/diagnostics/log.h"

#include <WiFi.h>
#include <esp_system.h>

#include "src/state.h"

namespace {

constexpr uint8_t kSlots = 12;
constexpr size_t kRecordSize = 192;

void writeRecord(const char* level, const char* event, int code, bool hasCode) {
  uint8_t index = prefs.getUChar("next", 0) % kSlots;
  char key[8];
  snprintf(key, sizeof(key), "l%u", index);

  char record[kRecordSize];
  if (hasCode) {
    snprintf(record, sizeof(record), "%lu %s %s code=%d heap=%u wifi=%d",
             (unsigned long)millis(), level, event, code,
             ESP.getFreeHeap(), (int)WiFi.status());
  } else {
    snprintf(record, sizeof(record), "%lu %s %s heap=%u wifi=%d",
             (unsigned long)millis(), level, event,
             ESP.getFreeHeap(), (int)WiFi.status());
  }

  prefs.putString(key, record);
  prefs.putUChar("next", (uint8_t)((index + 1) % kSlots));
  uint8_t count = prefs.getUChar("count", 0);
  if (count < kSlots) prefs.putUChar("count", (uint8_t)(count + 1));
  Serial.printf("[diag] %s\n", record);
}

}  // namespace

namespace Diagnostics {

void begin() {
  if (!prefs.isKey("boot_count")) prefs.putUInt("boot_count", 0);
  prefs.putUInt("boot_count", prefs.getUInt("boot_count", 0) + 1);
  writeRecord("INFO", "boot", (int)esp_reset_reason(), true);
}

void info(const char* event) { writeRecord("INFO", event, 0, false); }
void warning(const char* event) { writeRecord("WARN", event, 0, false); }
void error(const char* event) { writeRecord("ERROR", event, 0, false); }
void errorCode(const char* event, int code) { writeRecord("ERROR", event, code, true); }

}  // namespace Diagnostics
