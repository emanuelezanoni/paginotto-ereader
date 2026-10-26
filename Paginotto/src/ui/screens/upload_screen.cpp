#include "src/ui/screens/upload_screen.h"

#include <WiFi.h>
#include <qrcode.h>

#include "src/config.h"
#include "src/hal/display.h"
#include "src/hal/input.h"        // resetInputFrontend() — used on exit only
#include "src/hal/wifi.h"
#include "src/hal/wifi_provisioning.h"
#include "src/state.h"            // server
#include "src/storage/library.h"
#include "src/storage/wifi_creds.h"
#include "src/ui/font.h"
#include "src/ui/screens/library_screen.h"
#include "src/ui/screens/update_screen.h"
#include "src/ui/widgets.h"
#include "src/web/upload.h"       // resetBookUpload() / resetSleepUpload()

// How long to wait for an STA association before falling back to AP. Long
// enough for a typical 2.4 GHz home network (~1-3s); short enough that an
// unreachable network doesn't leave the user staring at "Connecting…".
static constexpr uint32_t kStaTimeoutMs = 10000;
// Give the radio a short settling window after the asynchronous scan is
// stopped. ESP32 can briefly report WL_CONNECT_FAILED from the previous scan
// transition before the new association attempt has actually started.
static constexpr uint32_t kStaFailureGraceMs = 1500;
// Keep the QR compact enough to sit beside the connection details on the
// 250x122 display. Version 3 at ECC_LOW fits both the AP and mDNS/IP URLs.
static constexpr uint8_t kQrVersion = 3;
static constexpr uint8_t kQrScale = 2;
static constexpr uint8_t kQrQuietModules = 2;
static constexpr int kQrRight = SCREEN_W - 4;

static void drawConnectionQr(const String& url, int topY) {
  if (!url.length()) return;
  uint8_t data[256] = {};
  QRCode qr;
  if (qrcode_initText(&qr, data, kQrVersion, ECC_LOW, url.c_str()) != 0) return;

  const int side = (qr.size + 2 * kQrQuietModules) * kQrScale;
  const int left = kQrRight - side;
  const int top = max(0, min(topY, SCREEN_H - side));
  gfx.fillRect(left, top, side, side, 0);
  for (uint8_t row = 0; row < qr.size; row++) {
    for (uint8_t col = 0; col < qr.size; col++) {
      if (qrcode_getModule(&qr, col, row)) {
        gfx.fillRect(left + (col + kQrQuietModules) * kQrScale,
                     top + (row + kQrQuietModules) * kQrScale,
                     kQrScale, kQrScale, 1);
      }
    }
  }
}

// ---- Drawing --------------------------------------------------------------
void UploadScreen::draw() {
  prepareMenuFrame();

  int y = drawSectionHeader(firmwareMode_ ? D_UPDATE_HEADER : D_UPLOAD_HEADER);

  if (phase_ == Phase::Goodbye) {
    drawCenter("Wi-Fi off.", "Returning to library automatically in 3s.");
    return;
  } else if (phase_ == Phase::ConnectingSta) {
    drawStatusLine(y, D_UPLOAD_CONNECTING, true); y += 20;
    Font::useBody(); u8g2.setCursor(MARGIN_X, y); u8g2.print(net_.staSsid.c_str());
  } else if (net_.mode == WifiMode::Station) {
    Font::useBold();
    u8g2.setCursor(MARGIN_X, y);
    u8g2.print(D_UPLOAD_CONNECTED);
    y += 18;

    Font::useBody();
    u8g2.setCursor(MARGIN_X, y);
    u8g2.print(net_.staSsid.c_str());
    y += 18;

    Font::useBold();
    u8g2.setCursor(MARGIN_X, y);
    u8g2.print(D_UPLOAD_OPEN);
    y += 18;

    Font::useBody();
    u8g2.setCursor(MARGIN_X, y);
    u8g2.print(net_.primaryUrl.c_str());
    y += 18;

    if (net_.fallbackUrl.length() > 0) {
      u8g2.setCursor(MARGIN_X, y);
      u8g2.print(net_.fallbackUrl.c_str());
    }
    drawConnectionQr(net_.fallbackUrl.length() ? net_.fallbackUrl : net_.primaryUrl, 34);
  } else if (!sessionActive_) {
    // Selection screen: no network is active yet. Hotspot details are shown
    // only after the user explicitly selects Hotspot.
    Font::useBody();
    const int itemCount = firmwareMode_
      ? (scanner_.count() ? static_cast<int>(scanner_.count()) : 1)
      : static_cast<int>(scanner_.count()) + 1;
    drawScrollableList(y, itemCount, selectedItem_,
      [&](int idx, int rowY, bool selected, int) {
        if (!firmwareMode_ && idx == 0) {
          drawMenuRow(rowY, D_UPLOAD_HOTSPOT_OPTION, selected);
        } else if (firmwareMode_ && !scanner_.count()) {
          const char* message = scanner_.state() == WifiNetworkScanState::NoSavedNetworks
            ? D_UPLOAD_NO_CONFIGURED : D_UPLOAD_WAITING_NETWORK;
          drawMenuRow(rowY, message, false);
        } else {
          const uint8_t savedIndex = firmwareMode_ ? idx : idx - 1;
          drawMenuRow(rowY, WifiCreds::network(scanner_.savedIndexAt(savedIndex)).ssid, selected);
        }
        return 1;
      });
    Font::useBody(); u8g2.setCursor(MARGIN_X, SCREEN_H - 6);
    if (connectionFailed_ && (int32_t)(connectionFailedUntilMs_ - millis()) > 0) {
      if (connectionFailure_ == WifiStaResult::NoSsidAvailable) u8g2.print(D_UPLOAD_NETWORK_NOT_FOUND);
      else if (connectionFailure_ == WifiStaResult::AuthenticationFailed) u8g2.print(D_UPLOAD_PASSWORD_REJECTED);
      else u8g2.print(D_UPLOAD_CONN_FAILED);
    }
    else if (scanner_.state() == WifiNetworkScanState::NoSavedNetworks) u8g2.print(D_UPLOAD_NO_CONFIGURED);
    else if (scanner_.state() == WifiNetworkScanState::Searching) u8g2.print(D_UPLOAD_SEARCHING_CONFIGURED);
    else if (scanner_.state() == WifiNetworkScanState::TimedOut) u8g2.print(D_UPLOAD_SCAN_TIMED_OUT);
    else u8g2.print(D_UPLOAD_NONE_VISIBLE);
  } else {
    // Hotspot details are shown only after Hotspot was selected.
    Font::useBold(); u8g2.setCursor(MARGIN_X, y);
    u8g2.print(connectionFailed_ ? D_UPLOAD_CONN_FAILED : D_UPLOAD_WIFI); y += 18;
    Font::useBody(); u8g2.setCursor(MARGIN_X, y);
    u8g2.print(net_.apSsid.c_str()); y += 20;
    u8g2.setCursor(MARGIN_X, y); u8g2.print(D_UPLOAD_PASSWORD); u8g2.print(": ");
    u8g2.print(net_.apPass.c_str()); y += 20;
    Font::useBold();
    u8g2.setCursor(MARGIN_X, y); u8g2.print(D_UPLOAD_OPEN); y += 16;
    Font::useBody();
    u8g2.setCursor(MARGIN_X, y); u8g2.print(net_.primaryUrl.c_str());
    drawConnectionQr(net_.primaryUrl, 34);

  }

  display.update();
}

// ---- Lifecycle ------------------------------------------------------------
void UploadScreen::onEnter() {
  beginSession();
}

void UploadScreen::openForFirmwareUpdate() {
  firmwareMode_ = true;
}

void UploadScreen::beginSession() {
  resetBookUpload();
  resetSleepUpload();

  // Tell WifiProvisioning to keep its hands off the radio for the duration
  // of the session — set BEFORE wifiStaBegin so a same-tick
  // WifiProvisioning::loop() can't race the WiFi.begin() we're about to fire.
  WifiProvisioning::notifyUploadSession(true);
  notifyUploadActivity();

  sessionActive_ = false;
  exitRequested_ = false;
  screenActivityMs_ = millis();
  phase_ = Phase::Ready;
  net_ = WifiSession{};
  net_.mode = WifiMode::AccessPoint;
  beginConfiguredScan();
  draw();
}

void UploadScreen::enterReady() {
  phase_     = Phase::Ready;
  sessionActive_ = true;
  startedMs_ = millis();
  if (firmwareMode_) {
    sessionActive_ = false;
    g_updateScreen.beginWithConnectedWifi(net_);
    nextScreen = &g_updateScreen;
    return;
  }
  server.begin();
  draw();
}

void UploadScreen::useHotspot() {
  scanner_.cancel();
  if (phase_ == Phase::ConnectingSta) wifiStaAbort();
  net_ = wifiBeginAccessPoint();
  phase_ = Phase::Ready;
  sessionActive_ = true;
  startedMs_ = millis();
  server.begin();
  draw();
}

void UploadScreen::beginConfiguredScan() {
  selectedItem_ = 0;
  scanner_.begin();
}

void UploadScreen::startSelectedStation() {
  if (firmwareMode_) {
    if (selectedItem_ >= scanner_.count()) return;
  } else if (!selectedItem_ || selectedItem_ > scanner_.count()) {
    return;
  }
  const uint8_t savedIndex = firmwareMode_ ? selectedItem_ : selectedItem_ - 1;
  WifiCreds::Network saved = WifiCreds::network(scanner_.savedIndexAt(savedIndex));
  scanner_.cancel();
  server.stop();
  WiFi.softAPdisconnect(true);
  // Do not start STA association while the asynchronous scan is still
  // handing control back to the radio. A short OFF/STA transition avoids
  // stale scan status being interpreted as an immediate connection failure.
  WiFi.mode(WIFI_OFF);
  delay(100);
  net_.staSsid = saved.ssid;
  wifiStaBegin(saved.ssid, saved.pass);
  phase_ = Phase::ConnectingSta;
  staStartedMs_ = millis();
  draw();
}

void UploadScreen::returnToSelection(WifiStaResult result) {
  wifiStaAbort();
  server.stop();
  sessionActive_ = false;
  net_ = WifiSession{};
  net_.mode = WifiMode::AccessPoint;
  phase_ = Phase::Ready;
  screenActivityMs_ = millis();
  connectionFailed_ = result != WifiStaResult::Connected;
  connectionFailure_ = result;
  connectionFailedUntilMs_ = connectionFailed_ ? millis() + 5000UL : 0;
  beginConfiguredScan();
  draw();
}

void UploadScreen::stopSessionToLibrary() {
  scanner_.cancel();
  server.stop();
  wifiEnd();
  WifiProvisioning::notifyUploadSession(false);

  resetBookUpload();
  resetSleepUpload();
  firmwareMode_ = false;

  loadBooks();
  // Discard the exit-click so it doesn't leak into the library screen as a
  // menu selection.
  resetInputFrontend();
  nextScreen = &g_libraryScreen;
}

void UploadScreen::requestWebUiExit() {
  if (phase_ == Phase::Goodbye) return;
  exitRequested_ = true;
}

void UploadScreen::beginGoodbye() {
  if (phase_ == Phase::Goodbye) return;
  exitRequested_ = false;
  phase_ = Phase::Goodbye;
  sessionActive_ = false;
  goodbyeStartedMs_ = millis();
  server.stop();
  draw();
}

// ---- Per-iteration input + tick -------------------------------------------
void UploadScreen::onButton(const ButtonEvent& e) {
  if (!e.any()) return;

  // The standalone picker/connection screen has its own five-minute budget.
  // Active WebUI sessions use the browser-activity timer instead.
  screenActivityMs_ = millis();

  if (phase_ == Phase::Goodbye) return;

  if (phase_ == Phase::ConnectingSta) {
    if (e.kind == ButtonEvent::Triple) stopSessionToLibrary();
    return;
  }

  if (net_.mode == WifiMode::AccessPoint && !sessionActive_) {
    if (e.kind == ButtonEvent::Short) {
      const uint8_t itemCount = firmwareMode_ ? scanner_.count() : scanner_.count() + 1;
      if (!itemCount) return;
      selectedItem_ = (selectedItem_ + 1) % itemCount;
      connectionFailed_ = false;
      connectionFailure_ = WifiStaResult::Failed;
      connectionFailedUntilMs_ = 0;
      draw();
      return;
    }
    if (e.kind == ButtonEvent::Double) {
      if (firmwareMode_) startSelectedStation();
      else if (selectedItem_) startSelectedStation();
      else useHotspot();
      return;
    }
    if (e.kind == ButtonEvent::Triple) {
      stopSessionToLibrary();
      return;
    }
  }

  if (e.kind == ButtonEvent::Triple) {
    stopSessionToLibrary();
  }
}

void UploadScreen::onIdleTick() {
  if (exitRequested_) {
    beginGoodbye();
    return;
  }
  if (phase_ == Phase::Goodbye) {
    if ((uint32_t)(millis() - goodbyeStartedMs_) >= 3000) stopSessionToLibrary();
    return;
  }
  if (!sessionActive_
      && (uint32_t)(millis() - screenActivityMs_) >= WIFI_SCREEN_INACTIVITY_TIMEOUT_MS) {
    stopSessionToLibrary();
    return;
  }
  if (phase_ == Phase::ConnectingSta) {
    WifiStaResult r = wifiStaPoll(net_);
    if (r == WifiStaResult::Connected) {
      enterReady();
      return;
    }
    const uint32_t elapsed = millis() - staStartedMs_;
    if ((r != WifiStaResult::Connecting && r != WifiStaResult::Connected &&
         elapsed >= kStaFailureGraceMs) || elapsed > kStaTimeoutMs) {
      if (r == WifiStaResult::Connecting) r = WifiStaResult::TimedOut;
      returnToSelection(r);
      return;
    }
    return;   // still associating — server isn't up yet
  }

  if (!sessionActive_ && scanner_.poll()) draw();
  if (!sessionActive_ && connectionFailed_ &&
      (int32_t)(connectionFailedUntilMs_ - millis()) <= 0) {
    connectionFailed_ = false;
    draw();
  }
  if (sessionActive_) server.handleClient();
  if (sessionActive_ && (uint32_t)(millis() - uploadLastActivityMs()) > UPLOAD_INACTIVITY_TIMEOUT_MS) {
    stopSessionToLibrary();
  }
}
