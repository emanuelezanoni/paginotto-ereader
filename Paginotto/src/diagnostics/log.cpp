#include "src/diagnostics/log.h"

#include <WiFi.h>
#include <esp_system.h>

#include "src/state.h"

namespace {

// Persistent records are deliberately limited: NVS is not a log filesystem.
// Runtime traces are much larger and do not consume flash or LittleFS space.
constexpr uint8_t kPersistentSlots = 24;
constexpr uint8_t kRuntimeSlots = 96;
constexpr size_t kRuntimeRecordSize = 112;
constexpr size_t kRecordSize = 192;

char runtimeRecords[kRuntimeSlots][kRuntimeRecordSize];
uint8_t runtimeNext = 0;
uint8_t runtimeCount = 0;

const char* resetReasonName(esp_reset_reason_t reason) {
  switch (reason) {
    case ESP_RST_POWERON: return "ESP_RST_POWERON";
    case ESP_RST_EXT: return "ESP_RST_EXT";
    case ESP_RST_SW: return "ESP_RST_SW";
    case ESP_RST_PANIC: return "ESP_RST_PANIC";
    case ESP_RST_INT_WDT: return "ESP_RST_INT_WDT";
    case ESP_RST_TASK_WDT: return "ESP_RST_TASK_WDT";
    case ESP_RST_WDT: return "ESP_RST_WDT";
    case ESP_RST_DEEPSLEEP: return "ESP_RST_DEEPSLEEP";
    case ESP_RST_BROWNOUT: return "ESP_RST_BROWNOUT";
    case ESP_RST_SDIO: return "ESP_RST_SDIO";
    default: return "ESP_RST_UNKNOWN";
  }
}

void writeRuntime(const char* level, const char* event, int code, bool hasCode) {
  char* record = runtimeRecords[runtimeNext];
  if (hasCode) {
    snprintf(record, kRuntimeRecordSize, "%lu %s %s code=%d heap=%u wifi=%d",
             (unsigned long)millis(), level, event, code,
             ESP.getFreeHeap(), (int)WiFi.status());
  } else {
    snprintf(record, kRuntimeRecordSize, "%lu %s %s heap=%u wifi=%d",
             (unsigned long)millis(), level, event,
             ESP.getFreeHeap(), (int)WiFi.status());
  }
  runtimeNext = (uint8_t)((runtimeNext + 1) % kRuntimeSlots);
  if (runtimeCount < kRuntimeSlots) runtimeCount++;
  Serial.printf("[diag] %s\n", record);
}

void writeRecord(const char* level, const char* event, int code, bool hasCode) {
  uint8_t index = prefs.getUChar("next", 0) % kPersistentSlots;
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
  prefs.putUChar("next", (uint8_t)((index + 1) % kPersistentSlots));
  uint8_t count = prefs.getUChar("count", 0);
  if (count < kPersistentSlots) prefs.putUChar("count", (uint8_t)(count + 1));
  Serial.printf("[diag] %s\n", record);
}

}  // namespace

namespace Diagnostics {

void begin() {
  if (!prefs.isKey("boot_count")) prefs.putUInt("boot_count", 0);
  prefs.putUInt("boot_count", prefs.getUInt("boot_count", 0) + 1);
  esp_reset_reason_t reason = esp_reset_reason();
  char event[96];
  snprintf(event, sizeof(event), "boot reset=%s previous_phase=%s",
           resetReasonName(reason), prefs.getString("phase", "none").c_str());
  writeRuntime("INFO", event, (int)reason, true);
  writeRecord("INFO", event, (int)reason, true);
}

void trace(const char* event) { writeRuntime("TRACE", event, 0, false); }

void checkpoint(const char* phase) {
  // A checkpoint is intentionally persistent: after a panic the last phase
  // tells us which operation was entered, even if no later record was saved.
  prefs.putString("phase", phase);
  writeRuntime("PHASE", phase, 0, false);
  writeRecord("INFO", phase, 0, false);
}

void info(const char* event) {
  writeRuntime("INFO", event, 0, false);
  writeRecord("INFO", event, 0, false);
}
void warning(const char* event) {
  writeRuntime("WARN", event, 0, false);
  writeRecord("WARN", event, 0, false);
}
void error(const char* event) {
  writeRuntime("ERROR", event, 0, false);
  writeRecord("ERROR", event, 0, false);
}
void errorCode(const char* event, int code) {
  writeRuntime("ERROR", event, code, true);
  writeRecord("ERROR", event, code, true);
}

String persistentText() {
  String out;
  uint8_t count = prefs.getUChar("count", 0);
  uint8_t next = prefs.getUChar("next", 0) % kPersistentSlots;
  uint8_t first = (count < kPersistentSlots) ? 0 : next;
  for (uint8_t i = 0; i < count; i++) {
    uint8_t slot = (uint8_t)((first + i) % kPersistentSlots);
    char key[8];
    snprintf(key, sizeof(key), "l%u", slot);
    String line = prefs.getString(key, "");
    if (line.length()) out += line + "\n";
  }
  return out;
}

String runtimeText() {
  String out;
  uint8_t first = (runtimeCount < kRuntimeSlots) ? 0 : runtimeNext;
  for (uint8_t i = 0; i < runtimeCount; i++) {
    uint8_t slot = (uint8_t)((first + i) % kRuntimeSlots);
    if (runtimeRecords[slot][0]) {
      out += runtimeRecords[slot];
      out += "\n";
    }
  }
  return out;
}

void clearRuntime() {
  for (uint8_t i = 0; i < kRuntimeSlots; i++) runtimeRecords[i][0] = '\0';
  runtimeNext = 0;
  runtimeCount = 0;
}

}  // namespace Diagnostics
