#ifndef PAGINOTTO_HAL_OTA_H
#define PAGINOTTO_HAL_OTA_H

#include <Arduino.h>

struct OtaCheckResult {
  bool   updateAvailable = false;
  bool   reachable       = false;
  bool   invalidChannel   = false;
  bool   invalidManifest  = false;
  String remoteVersion;
  String remoteCommit;
  uint32_t remoteCommitTimestamp = 0;
};

struct OtaDownloadResult {
  bool success           = false;
  bool partitionTooSmall = false;
  bool invalidChannel     = false;
  bool truncated          = false;
};

// Callback fired during OTA::download() with progress 0–100.
// Only called when Content-Length is known. Callers typically redraw at 10%
// granularity to avoid hammering the e-ink display.
using OtaProgressFn = void (*)(int percent);

class OTA {
public:
  // Confirm the currently running image after boot when ESP-IDF rollback is
  // enabled. Returns false when the bootloader has no pending rollback state.
  static bool confirmRunningFirmware();

  // HTTPS HEAD probe to the firmware distribution root.
  // Blocks for up to 5 s. Call only when Wi-Fi STA is connected.
  static bool isUpdateServerReachable();

  // Fetches the manifest for the given channel and compares its version
  // against the running FW_VERSION. The board token is resolved from the
  // build-time display define. Blocks for up to 5 s.
  static OtaCheckResult checkAvailable(const String& channel);

  // Downloads the firmware binary for the given channel into the idle OTA
  // partition, validates it, and sets it as the next boot partition.
  // Blocks until complete (typically 5–30 s depending on network speed).
  // Does NOT reboot — caller decides when to call esp_restart().
  static OtaDownloadResult download(const String& channel,
                                    OtaProgressFn progressCb = nullptr);
};

#endif  // PAGINOTTO_HAL_OTA_H
