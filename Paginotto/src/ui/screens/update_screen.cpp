#include "src/ui/screens/update_screen.h"

#include <esp_system.h>
#include "src/config.h"
#include "src/hal/display.h"
#include "src/hal/input.h"
#include "src/hal/ota.h"
#include "src/hal/wifi.h"
#include "src/hal/wifi_provisioning.h"
#include "src/state.h"
#include "src/storage/wifi_creds.h"
#include "src/ui/font.h"
#include "src/ui/screens/library_screen.h"
#include "src/ui/widgets.h"

static constexpr uint32_t    kStaTimeoutMs  = 15000;
static constexpr uint32_t    kWifiScanTimeoutMs = 60000;
static constexpr const char* kKeyOtaChannel = "cfg_ota_channel";

// Static progress callback — called from OTA::download() during the blocking
// binary stream. Fires g_updateScreen.setProgress() which redraws at 10%.
static void onDownloadProgress(int pct) {
  g_updateScreen.setProgress(pct);
}

// ----------------------------------------------------------------------------
//  NVS helpers
// ----------------------------------------------------------------------------
void UpdateScreen::loadChannel() {
  stableChan_ = (prefs.getString(kKeyOtaChannel, "stable") == "stable");
}

void UpdateScreen::saveChannel() {
  prefs.putString(kKeyOtaChannel, stableChan_ ? "stable" : "dev");
}

// ----------------------------------------------------------------------------
//  Progress update (called during blocking download)
// ----------------------------------------------------------------------------
void UpdateScreen::setProgress(int pct) {
  progress_ = pct;
  if (pct % 10 == 0 || pct == 100) draw();
}

void UpdateScreen::beginWithConnectedWifi(const WifiSession& session) {
  handoffSession_ = session;
  connectedHandoff_ = true;
}

// ----------------------------------------------------------------------------
//  Exit
// ----------------------------------------------------------------------------
void UpdateScreen::exitToLibrary() {
  cancelWifiScan();
  if (wifiStarted_) {
    wifiEnd();
    wifiStarted_ = false;
  }
  WifiProvisioning::notifyUploadSession(false);
  nextScreen = &g_libraryScreen;
}

void UpdateScreen::beginWifiScan() {
  WifiProvisioning::notifyUploadSession(true);
  cancelWifiScan();
  visibleCount_ = 0;
  selectedWifi_ = 0;
  if (!WifiCreds::has()) {
    phase_ = Phase::NoCreds;
    return;
  }
  phase_ = Phase::SelectingWifi;
  scanStartedMs_ = millis();
  scanState_ = ScanState::Searching;
  WiFi.mode(WIFI_STA);
  scanInProgress_ = true;
  WiFi.scanNetworks(true, true);
}

void UpdateScreen::cancelWifiScan() {
  if (scanInProgress_) {
    WiFi.scanDelete();
  }
  scanInProgress_ = false;
}

void UpdateScreen::pollWifiScan() {
  if (phase_ != Phase::SelectingWifi || !scanInProgress_) return;
  if ((uint32_t)(millis() - scanStartedMs_) >= kWifiScanTimeoutMs) {
    cancelWifiScan();
    scanState_ = ScanState::TimedOut;
    draw();
    return;
  }
  int found = WiFi.scanComplete();
  if (found == -1) return;
  if (found < 0) {
    // Keep the same retry cycle as the working WebUI/upload scanner.
    WiFi.scanDelete();
    WiFi.scanNetworks(true, true);
    return;
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
  scanState_ = ScanState::Complete;
  if (changed) draw();
  WiFi.scanNetworks(true, true);
}

void UpdateScreen::startSelectedWifi() {
  // Item zero is the static placeholder shown while no network has been
  // found. Network rows start at one, exactly like UploadScreen's hotspot
  // row, but the placeholder is intentionally not actionable.
  if (!visibleCount_ || selectedWifi_ == 0 || selectedWifi_ > visibleCount_) return;
  WifiCreds::Network saved = WifiCreds::network(visible_[selectedWifi_ - 1]);
  cancelWifiScan();
  WiFi.mode(WIFI_OFF);
  delay(100);
  wifiStaBegin(saved.ssid, saved.pass);
  wifiStarted_ = true;
  WifiProvisioning::notifyUploadSession(true);
  phase_ = Phase::Connecting;
  staStartMs_ = millis();
  draw();
}

// ----------------------------------------------------------------------------
//  Lifecycle
// ----------------------------------------------------------------------------
void UpdateScreen::onEnter() {
  loadChannel();
  focusItem_     = 0;
  stableVersion_ = "";
  devCommit_     = "";
  stableAvailable_ = false;
  devAvailable_ = false;
  stableReachable_ = false;
  devReachable_ = false;
  progress_      = 0;
  wifiStarted_   = false;

  if (connectedHandoff_) {
    net_ = handoffSession_;
    handoffSession_ = WifiSession{};
    connectedHandoff_ = false;
    wifiStarted_ = true;
    phase_ = Phase::Idle;
    checkForUpdates();
    return;
  }

  if (!WifiCreds::has()) {
    phase_ = Phase::NoCreds;
  } else {
    WifiProvisioning::notifyUploadSession(true);
    beginWifiScan();
  }
  draw();
}

void UpdateScreen::checkForUpdates() {
  phase_ = Phase::Checking;
  draw();
  stableAvailable_ = false;
  devAvailable_ = false;
  stableReachable_ = false;
  devReachable_ = false;
  if (OTA::isUpdateServerReachable()) {
    OtaCheckResult stable = OTA::checkAvailable("stable");
    OtaCheckResult dev = OTA::checkAvailable("dev");
    stableReachable_ = stable.reachable;
    devReachable_ = dev.reachable;
    stableAvailable_ = stable.updateAvailable;
    devAvailable_ = dev.updateAvailable;
    stableVersion_ = stable.remoteVersion;
    devCommit_ = dev.remoteCommit;
  }
  if (!stableReachable_ && !devReachable_) phase_ = Phase::ServerFail;
  else if (stableAvailable_ || devAvailable_) phase_ = Phase::UpdateAvailable;
  else phase_ = Phase::UpToDate;
  clearButtonQueue();
  draw();
}

void UpdateScreen::onIdleTick() {
  if (phase_ == Phase::SelectingWifi) {
    pollWifiScan();
    return;
  }
  if (phase_ != Phase::Connecting) return;

  WifiStaResult r = wifiStaPoll(net_);
  if (r == WifiStaResult::Connected) {
    phase_ = Phase::Idle;
    checkForUpdates();
    return;
  }
  if (r == WifiStaResult::Failed ||
      (uint32_t)(millis() - staStartMs_) > kStaTimeoutMs) {
    wifiEnd();
    WifiProvisioning::notifyUploadSession(false);
    wifiStarted_ = false;
    phase_ = Phase::ConnFailed;
    draw();
  }
}

// ----------------------------------------------------------------------------
//  Drawing
// ----------------------------------------------------------------------------
void UpdateScreen::draw() {
  prepareMenuFrame();
  Font::useBody();
  int y = drawSectionHeader(D_UPDATE_HEADER);

  // ---- Transient / single-message states -----------------------------------

  if (phase_ == Phase::NoCreds) {
    u8g2.setCursor(MARGIN_X, y);
    u8g2.print(D_UPDATE_NO_CREDS_L1);
    y += 14;
    u8g2.setCursor(MARGIN_X, y);
    u8g2.print(D_UPDATE_NO_CREDS_L2);
    display.update();
    return;
  }
  if (phase_ == Phase::Connecting) {
    u8g2.setCursor(MARGIN_X, y);
    u8g2.print(D_UPDATE_CONNECTING);
    display.update();
    return;
  }
  if (phase_ == Phase::SelectingWifi) {
    Font::useBold();
    u8g2.setCursor(MARGIN_X, y);
    u8g2.print(D_UPDATE_WIFI_HEADER);
    y += 16;
    Font::useBody();
    const int itemCount = static_cast<int>(visibleCount_) + 1;
    const int selectedItem = static_cast<int>(selectedWifi_);
    drawScrollableList(y, itemCount, selectedItem,
      [&](int idx, int rowY, bool selected, int) {
        if (idx == 0) {
          drawMenuRow(rowY, D_UPDATE_WIFI_NOT_FOUND, selected);
        } else {
          drawMenuRow(rowY, WifiCreds::network(visible_[idx - 1]).ssid, selected);
        }
        return 1;
      });
    Font::useBody();
    u8g2.setCursor(MARGIN_X, SCREEN_H - 6);
    if (scanState_ == ScanState::Searching && !visibleCount_) {
      u8g2.print(D_UPDATE_WIFI_SEARCHING);
    } else {
      u8g2.print(D_UPDATE_WIFI_HINT);
    }
    display.update();
    return;
  }
  if (phase_ == Phase::ConnFailed) {
    u8g2.setCursor(MARGIN_X, y);
    u8g2.print(D_UPDATE_CONN_FAILED);
    display.update();
    return;
  }
  if (phase_ == Phase::Checking) {
    Font::useBold();
    u8g2.setCursor(MARGIN_X, y);
    u8g2.print(D_UPDATE_CHECKING);
    Font::useBody();
    display.update();
    return;
  }
  if (phase_ == Phase::Downloading) {
    u8g2.setCursor(MARGIN_X, y);
    u8g2.print(D_UPDATE_INSTALLING);
    y += 16;
    char pctBuf[8];
    snprintf(pctBuf, sizeof(pctBuf), "%d%%", progress_);
    u8g2.setCursor(MARGIN_X, y);
    u8g2.print(pctBuf);
    display.update();
    return;
  }
  if (phase_ == Phase::RebootPrompt) {
    Font::useBold();
    u8g2.setCursor(MARGIN_X, y);
    u8g2.print(D_UPDATE_REBOOT_MSG);
    y += 18;
    Font::useBody();
    u8g2.setCursor(MARGIN_X, y);
    u8g2.print(D_UPDATE_REBOOT_HINT);
    display.update();
    return;
  }

  // ---- Full UI (Idle, ServerFail, UpToDate, UpdateAvailable, DownloadFailed)

  if (focusItem_ >= 2) focusItem_ = 0;

  // Version line
  u8g2.setCursor(MARGIN_X, y);
  u8g2.print(D_UPDATE_VERSION_PREFIX FW_VERSION);
  y += 16;

  // The check already happened immediately after Wi-Fi connection. The only
  // interactive elements here are the two update channels.
  String stableLabel = "Stable";
  if (!stableReachable_) stableLabel += " (unavailable)";
  else if (stableAvailable_) {
    stableLabel += " -> ";
    stableLabel += stableVersion_;
  }
  else stableLabel += " (up to date)";
  drawMenuRow(y, stableLabel, focusItem_ == 0);
  y += 14;

  String devLabel = "Dev";
  if (!devReachable_) devLabel += " (unavailable)";
  else if (devAvailable_) {
    devLabel += " -> ";
    devLabel += devCommit_;
  }
  else devLabel += " (up to date)";
  drawMenuRow(y, devLabel, focusItem_ == 1);
  y += 18;

  // Only connection/download errors remain below the two channel rows. The
  // channel rows themselves already contain their current status/version.
  Font::useBody();
  if (phase_ == Phase::ServerFail) {
    u8g2.setCursor(MARGIN_X, y);
    u8g2.print(D_UPDATE_SERVER_FAIL);
    y += 14;
  } else if (phase_ == Phase::DownloadFailed) {
    u8g2.setCursor(MARGIN_X, y);
    u8g2.print(D_UPDATE_DOWNLOAD_FAILED);
    y += 14;
  }

  display.update();
}

// ----------------------------------------------------------------------------
//  Input
// ----------------------------------------------------------------------------
void UpdateScreen::onButton(const ButtonEvent& e) {
  if (!e.any()) return;

  // Reboot prompt — only 2x is accepted; device has no manual reboot option.
  if (phase_ == Phase::RebootPrompt) {
    if (e.kind == ButtonEvent::Double) {
      esp_restart();
    }
    return;
  }

  if (e.kind == ButtonEvent::Triple) {
    exitToLibrary();
    return;
  }

  if (phase_ == Phase::SelectingWifi) {
    if (e.kind == ButtonEvent::Short) {
      selectedWifi_ = (selectedWifi_ + 1) % (visibleCount_ + 1);
      draw();
    } else if (e.kind == ButtonEvent::Double) {
      startSelectedWifi();
    }
    return;
  }

  if (phase_ == Phase::ConnFailed) {
    if (e.kind == ButtonEvent::Double) beginWifiScan();
    return;
  }

  // Full UI phases only
  const bool fullUi = phase_ == Phase::Idle        ||
                      phase_ == Phase::ServerFail   ||
                      phase_ == Phase::UpToDate     ||
                      phase_ == Phase::UpdateAvailable ||
                      phase_ == Phase::DownloadFailed;
  if (!fullUi) return;

  if (e.kind == ButtonEvent::Short) {
    focusItem_ = (focusItem_ + 1) % 2;
    draw();
    return;
  }

  if (e.kind == ButtonEvent::Double) {
    if (focusItem_ == 0) {
      stableChan_ = true;
      saveChannel();
      if (stableAvailable_) installSelectedUpdate();
      else draw();

    } else if (focusItem_ == 1) {
      stableChan_ = false;
      saveChannel();
      if (devAvailable_) installSelectedUpdate();
      else draw();

    }
    return;
  }
}

void UpdateScreen::installSelectedUpdate() {
  progress_ = 0;
  phase_ = Phase::Downloading;
  draw();
  OtaDownloadResult r = OTA::download(stableChan_ ? "stable" : "dev",
                                      onDownloadProgress);
  clearButtonQueue();
  if (r.success) {
    if (wifiStarted_) {
      wifiEnd();
      WifiProvisioning::notifyUploadSession(false);
      wifiStarted_ = false;
    }
    phase_ = Phase::RebootPrompt;
  } else {
    phase_ = Phase::DownloadFailed;
  }
  draw();
}
