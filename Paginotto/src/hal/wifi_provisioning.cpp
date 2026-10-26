// Implementation of the Improv Serial Wi-Fi provisioning wrapper.
//
// The on-device protocol work is done by jnthas/Improv-WiFi-Library
// (<ImprovWiFiLibrary.h>, pinned in platformio.ini) — this file owns the
// ImprovWiFi instance, wires its callbacks, drives it from loop(), and
// coordinates Wi-Fi ownership with the upload session. See the header for the
// "what / why" of Improv and the listening model.
#include "src/hal/wifi_provisioning.h"

#include <ImprovWiFiLibrary.h>
#include <WiFi.h>

#include "src/config.h"
#include "src/pure/wifi_credentials.h"
#include "src/storage/wifi_creds.h"

namespace WifiProvisioning {

namespace {
struct State {
  ImprovWiFi lib{&Serial};
  bool       uploadSession   = false;
  bool       credsReceived   = false;
  uint32_t   credsReceivedMs = 0;
  bool       ownsWifi        = false;  // true iff we brought Wi-Fi up to verify creds
};

State s_state;
}  // namespace

// After the credentials callback fires we give the library this much time to
// flush its "Provisioned" reply over USB-CDC before tearing Wi-Fi down. The
// reply itself takes microseconds at 115200 baud; this is just headroom.
static constexpr uint32_t kShutdownGraceMs = 2000;

static constexpr uint32_t kConnectTimeoutMs = 15000;

static void releaseWifi() {
  if (!s_state.ownsWifi) return;
  WiFi.disconnect(true, true);
  WiFi.mode(WIFI_OFF);
  s_state.ownsWifi = false;
}

static void clearGraceTimer() {
  s_state.credsReceived   = false;
  s_state.credsReceivedMs = 0;
}

// Improv's default connector switches the radio to WIFI_STA. During an
// upload the AP is owned by UploadScreen and must survive provisioning, so
// keep AP+STA in that mode. Outside upload mode, use STA only and let the
// caller tear it down after the protocol reply has been flushed.
static bool connectForProvisioning(const char* ssid, const char* password) {
  if (!ssid || !password || !WifiCredentialPolicy::validSsid(String(ssid)) ||
      !WifiCredentialPolicy::validPassword(String(password))) {
    return false;
  }

  WiFi.mode(s_state.uploadSession ? WIFI_AP_STA : WIFI_STA);
  WiFi.begin(ssid, password);
  const uint32_t started = millis();
  while (WiFi.status() != WL_CONNECTED &&
         static_cast<uint32_t>(millis() - started) < kConnectTimeoutMs) {
    delay(50);
  }
  if (WiFi.status() == WL_CONNECTED) return true;

  // A failed association must not leave a stale STA attempt behind. During
  // upload the SoftAP belongs to UploadScreen, so only disconnect the STA;
  // outside upload mode the temporary radio session can be shut down fully.
  WiFi.disconnect(false, false);
  if (!s_state.uploadSession) WiFi.mode(WIFI_OFF);
  return false;
}

void begin() {
  s_state.lib.setDeviceInfo(
      ImprovTypes::ChipFamily::CF_ESP32_S3,
      "Paginotto",
      FW_VERSION,
      "Paginotto",
      "http://paginotto.local/");
  s_state.lib.setCustomConnectWiFi(connectForProvisioning);

  // The library handles WiFi.begin() itself; we only persist creds once the
  // association succeeds. If the association fails the library reports the
  // error back to the browser and the saved creds stay untouched, which is
  // exactly what we want (don't poison NVS with bad creds).
  //
  // The custom connector below is important while an upload session is
  // active: the library's default connector switches to WIFI_STA and would
  // kill the upload SoftAP. The wrapper preserves AP+STA ownership instead.
  s_state.lib.onImprovConnected([](const char* ssid, const char* password) {
    WifiCreds::save(ssid, password);
    if (!s_state.uploadSession) {
      // We brought Wi-Fi up just to verify creds — schedule teardown after
      // the library finishes sending its "Provisioned" reply.
      s_state.ownsWifi        = true;
      s_state.credsReceived   = true;
      s_state.credsReceivedMs = millis();
    }
    // Upload session: leave Wi-Fi alone — upload screen owns it.
  });
}

void notifyUploadSession(bool active) {
  s_state.uploadSession = active;
  if (active) {
    // Upload takes over the radio. Cancel any pending teardown so the grace
    // timer doesn't yank Wi-Fi out from under it.
    s_state.ownsWifi = false;
    clearGraceTimer();
  }
}

bool isActive() {
  // Sleep is blocked only while this adapter actually owns a radio session.
  // Merely receiving a CDC byte must not pin the device awake for minutes:
  // the upload screen already owns the radio explicitly, and a provisioning
  // connection owns it through `ownsWifi`.
  return s_state.uploadSession || s_state.ownsWifi;
}

void loop() {
  if (s_state.credsReceived
      && (uint32_t)(millis() - s_state.credsReceivedMs) > kShutdownGraceMs) {
    releaseWifi();
    clearGraceTimer();
  }

  // Improv is polled continuously; with no incoming bytes this is cheap and
  // the sleep gate above is controlled by radio ownership, not CDC presence.
  s_state.lib.handleSerial();
}

}  // namespace WifiProvisioning
