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
  if (wifiStarted_) {
    wifiEnd();
    wifiStarted_ = false;
  }
  WifiProvisioning::notifyUploadSession(false);
  nextScreen = &g_libraryScreen;
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

  // Firmware Update is entered through UploadScreen, which owns the proven
  // saved-network picker and hands this screen an already connected session.
  phase_ = Phase::NoCreds;
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

// ----------------------------------------------------------------------------
//  Drawing
// ----------------------------------------------------------------------------
void UpdateScreen::draw() {
  prepareMenuFrame();
  Font::useBody();
  int y = drawSectionHeader(D_UPDATE_HEADER);

  // ---- Transient / single-message states -----------------------------------

  if (phase_ == Phase::NoCreds) {
    drawStatusLine(y, D_UPDATE_NO_CREDS_L1);
    y += 14;
    drawStatusLine(y, D_UPDATE_NO_CREDS_L2);
    display.update();
    return;
  }
  if (phase_ == Phase::Checking) {
    drawStatusLine(y, D_UPDATE_CHECKING, true);
    display.update();
    return;
  }
  if (phase_ == Phase::Downloading) {
    drawStatusLine(y, D_UPDATE_INSTALLING);
    y += 16;
    char pctBuf[8];
    snprintf(pctBuf, sizeof(pctBuf), "%d%%", progress_);
    u8g2.setCursor(MARGIN_X, y);
    u8g2.print(pctBuf);
    display.update();
    return;
  }
  if (phase_ == Phase::RebootPrompt) {
    drawStatusLine(y, D_UPDATE_REBOOT_MSG, true);
    y += 18;
    drawStatusLine(y, D_UPDATE_REBOOT_HINT);
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
  } else if (phase_ == Phase::DownloadFailed) {
    u8g2.setCursor(MARGIN_X, y);
    u8g2.print(D_UPDATE_DOWNLOAD_FAILED);
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
