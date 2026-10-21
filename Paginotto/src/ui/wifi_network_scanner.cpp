#include "src/ui/wifi_network_scanner.h"

#include <WiFi.h>

void WifiNetworkScanner::begin() {
  cancel();
  visibleCount_ = 0;
  if (!WifiCreds::has()) {
    state_ = WifiNetworkScanState::NoSavedNetworks;
    return;
  }

  state_ = WifiNetworkScanState::Searching;
  startedMs_ = millis();
  WiFi.mode(WIFI_STA);
  WiFi.scanNetworks(true, true);
  scanInProgress_ = true;
}

bool WifiNetworkScanner::poll() {
  if (state_ != WifiNetworkScanState::Searching || !scanInProgress_) return false;

  if ((uint32_t)(millis() - startedMs_) >= kTimeoutMs) {
    cancel();
    state_ = WifiNetworkScanState::TimedOut;
    return true;
  }

  int found = WiFi.scanComplete();
  if (found == -1) return false;
  if (found < 0) {
    WiFi.scanDelete();
    WiFi.scanNetworks(true, true);
    return false;
  }

  bool changed = false;
  for (uint8_t saved = 0; saved < WifiCreds::count(); saved++) {
    String ssid = WifiCreds::network(saved).ssid;
    for (int i = 0; i < found; i++) {
      if (WiFi.SSID(i) != ssid) continue;

      bool alreadyVisible = false;
      for (uint8_t j = 0; j < visibleCount_; j++) {
        if (visible_[j] == saved) alreadyVisible = true;
      }
      if (!alreadyVisible && visibleCount_ < WifiCreds::kMaxNetworks) {
        visible_[visibleCount_++] = saved;
        changed = true;
      }
      break;
    }
  }

  WiFi.scanDelete();
  // Keep the scanner in Searching: scans are intentionally repeated so a
  // network that was not visible in the first cycle can appear later.
  WiFi.scanNetworks(true, true);
  return changed;
}

void WifiNetworkScanner::cancel() {
  if (scanInProgress_) WiFi.scanDelete();
  scanInProgress_ = false;
}
