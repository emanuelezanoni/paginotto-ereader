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
  scanInProgress_ = true;
  WiFi.mode(WIFI_STA);
  WiFi.scanNetworks(true, true);
}

void UpdateScreen::cancelWifiScan() {
  if (scanInProgress_) WiFi.scanDelete();
  scanInProgress_ = false;
}

void UpdateScreen::pollWifiScan() {
  if (phase_ != Phase::SelectingWifi || !scanInProgress_) return;
  if ((uint32_t)(millis() - scanStartedMs_) >= kWifiScanTimeoutMs) {
    cancelWifiScan();
    draw();
    return;
  }
  int found = WiFi.scanComplete();
  if (found == -1) return;
  if (found < 0) {
    WiFi.scanDelete();
    WiFi.scanNetworks(true, true);
    return;
  }
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
      }
      break;
    }
  }
  WiFi.scanDelete();
  WiFi.scanNetworks(true, true);
  draw();
}

void UpdateScreen::startSelectedWifi() {
  if (!visibleCount_ || selectedWifi_ >= visibleCount_) return;
  WifiCreds::Network saved = WifiCreds::network(visible_[selectedWifi_]);
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

  if (!WifiCreds::has()) {
    phase_ = Phase::NoCreds;
  } else {
    WifiProvisioning::notifyUploadSession(true);
    beginWifiScan();
  }
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
    draw();
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
    if (!visibleCount_) {
      u8g2.setCursor(MARGIN_X, y);
      u8g2.print(scanInProgress_ ? D_UPDATE_WIFI_SEARCHING : D_UPDATE_WIFI_NONE);
    } else {
      drawScrollableList(y, visibleCount_, selectedWifi_,
        [&](int idx, int rowY, bool selected, int) {
          drawMenuRow(rowY, WifiCreds::network(visible_[idx]).ssid, selected);
          return 1;
        });
      Font::useBody();
      u8g2.setCursor(MARGIN_X, SCREEN_H - 6);
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
    u8g2.setCursor(MARGIN_X, y);
    u8g2.print(D_UPDATE_CHECKING);
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

  bool selectedInstallable = stableChan_ ? stableAvailable_ : devAvailable_;
  bool hasInstall = selectedInstallable || phase_ == Phase::DownloadFailed;
  int maxItems = hasInstall ? 4 : 3;
  if (focusItem_ >= maxItems) focusItem_ = 0;

  // Version line
  u8g2.setCursor(MARGIN_X, y);
  u8g2.print(D_UPDATE_VERSION_PREFIX FW_VERSION);
  y += 16;

  // Channel label on its own line
  Font::useBody();
  u8g2.setCursor(MARGIN_X, y);
  u8g2.print(D_UPDATE_CHANNEL_LABEL);
  y += 14;

  // Channel checkboxes
  int cx = MARGIN_X;

  Font::useBody();
  u8g2.setCursor(cx, y);
  u8g2.print(stableChan_ ? "[x] " : "[ ] ");
  cx += u8g2.getUTF8Width("[x] ");
  if (focusItem_ == 0) Font::useBold(); else Font::useBody();
  u8g2.setCursor(cx, y);
  u8g2.print(D_UPDATE_CHAN_STABLE);
  cx += u8g2.getUTF8Width(D_UPDATE_CHAN_STABLE);
  Font::useBody();
  cx += u8g2.getUTF8Width("   ");

  u8g2.setCursor(cx, y);
  u8g2.print(!stableChan_ ? "[x] " : "[ ] ");
  cx += u8g2.getUTF8Width("[x] ");
  if (focusItem_ == 1) Font::useBold(); else Font::useBody();
  u8g2.setCursor(cx, y);
  u8g2.print(D_UPDATE_CHAN_DEV);
  y += 14;


  // Check button
  if (focusItem_ == 2) Font::useBold(); else Font::useBody();
  u8g2.setCursor(MARGIN_X, y);
  u8g2.print(D_UPDATE_BTN_CHECK);
  y += 20;

  // Status lines — stable and dev are checked together.
  Font::useBody();
  if (phase_ == Phase::ServerFail) {
    u8g2.setCursor(MARGIN_X, y);
    u8g2.print(D_UPDATE_SERVER_FAIL);
    y += 14;
  } else if (phase_ == Phase::UpdateAvailable || phase_ == Phase::UpToDate) {
    u8g2.setCursor(MARGIN_X, y);
    if (stableAvailable_) {
      u8g2.print(D_UPDATE_STABLE_AVAILABLE);
      u8g2.print(FW_VERSION);
      u8g2.print(" -> ");
      u8g2.print(stableVersion_.c_str());
    } else {
      u8g2.print(D_UPDATE_STABLE_CURRENT);
    }
    y += 14;
    u8g2.setCursor(MARGIN_X, y);
    if (devAvailable_) {
      u8g2.print(D_UPDATE_DEV_AVAILABLE);
      u8g2.print(devCommit_.c_str());
    } else {
      u8g2.print(D_UPDATE_DEV_CURRENT);
    }
    y += 14;
  } else if (phase_ == Phase::DownloadFailed) {
    u8g2.setCursor(MARGIN_X, y);
    u8g2.print(D_UPDATE_DOWNLOAD_FAILED);
    y += 14;
  }

  // Install button — shown when update is available or after failed download
  if (hasInstall) {
    if (focusItem_ == 3) Font::useBold(); else Font::useBody();
    u8g2.setCursor(MARGIN_X, y);
    u8g2.print(D_UPDATE_BTN_INSTALL);
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
    if (!visibleCount_) return;
    if (e.kind == ButtonEvent::Short) {
      selectedWifi_ = (selectedWifi_ + 1) % visibleCount_;
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
    bool hasInstall = stableAvailable_ || devAvailable_ ||
                      phase_ == Phase::DownloadFailed;
    focusItem_ = (focusItem_ + 1) % (hasInstall ? 4 : 3);
    draw();
    return;
  }

  if (e.kind == ButtonEvent::Double) {
    if (focusItem_ == 0) {
      stableChan_ = true;
      saveChannel();
      draw();

    } else if (focusItem_ == 1) {
      stableChan_ = false;
      saveChannel();
      draw();

    } else if (focusItem_ == 2) {
      // Check both channels so the screen can show the two release tracks.
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
      if (!stableReachable_ && !devReachable_) {
        phase_ = Phase::ServerFail;
      } else if (stableAvailable_ || devAvailable_) {
        phase_ = Phase::UpdateAvailable;
      } else {
        phase_ = Phase::UpToDate;
      }
      clearButtonQueue();
      draw();

    } else if (focusItem_ == 3) {
      if (!(stableChan_ ? stableAvailable_ : devAvailable_)) return;
      // Install: download + flash
      progress_ = 0;
      phase_    = Phase::Downloading;
      draw();
      OtaDownloadResult r = OTA::download(stableChan_ ? "stable" : "dev",
                                          onDownloadProgress);
      clearButtonQueue();
      if (r.success) {
        // Flash complete — tear down WiFi and drop CPU to 80 MHz before
        // showing the reboot prompt. Sleep is now allowed again.
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
    return;
  }
}
