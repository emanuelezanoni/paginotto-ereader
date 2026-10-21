#ifndef PALA_UI_WIFI_NETWORK_SCANNER_H
#define PALA_UI_WIFI_NETWORK_SCANNER_H

#include <Arduino.h>

#include "src/storage/wifi_creds.h"

enum class WifiNetworkScanState {
  NoSavedNetworks,
  Searching,
  TimedOut
};

// Shared asynchronous scanner for saved Wi-Fi networks. Screens own the
// presentation and input; this class owns only radio scanning and results.
class WifiNetworkScanner {
public:
  static constexpr uint32_t kTimeoutMs = 60000;

  void begin();
  bool poll();
  void cancel();

  WifiNetworkScanState state() const { return state_; }
  uint8_t count() const { return visibleCount_; }
  uint8_t savedIndexAt(uint8_t index) const { return visible_[index]; }

private:
  uint8_t visible_[WifiCreds::kMaxNetworks] = {};
  uint8_t visibleCount_ = 0;
  uint32_t startedMs_ = 0;
  WifiNetworkScanState state_ = WifiNetworkScanState::NoSavedNetworks;
  bool scanInProgress_ = false;
};

#endif  // PALA_UI_WIFI_NETWORK_SCANNER_H
