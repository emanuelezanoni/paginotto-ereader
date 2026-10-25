#ifndef PAGINOTTO_HAL_WIFI_H
#define PAGINOTTO_HAL_WIFI_H

#include <Arduino.h>

// Upload-session network primitives. All non-blocking — the upload screen
// drives the STA-vs-AP decision itself with a small state machine so the
// main loop stays alive throughout (button events, Improv polling, screen
// redraws keep working through a slow STA association).
//
// Typical sequence:
//
//   if (wifiStaBegin()) {
//     // poll wifiStaPoll() each loop iteration; on Connected, take the
//     // session. On Failed (or after the caller's own timeout) call
//     // wifiStaAbort() and fall back to wifiBeginAccessPoint().
//   } else {
//     // no stored creds — straight to AP.
//     session = wifiBeginAccessPoint();
//   }
//   ... use session ...
//   wifiEnd();
//
// CPU clock: any of the begin functions bumps to 240 MHz (Wi-Fi needs it);
// wifiEnd() drops back to 80 MHz. wifiStaAbort() leaves the clock alone in
// case the caller is about to call wifiBeginAccessPoint().

enum class WifiMode { Station, AccessPoint };

struct WifiSession {
  WifiMode    mode = WifiMode::AccessPoint;
  String      primaryUrl;     // STA: http://paginotto.local  AP: http://192.168.4.1
  String      fallbackUrl;    // STA: http://<lan-ip>        AP: ""
  String      apSsid;          // populated in AccessPoint mode only
  String      apPass;          // populated in AccessPoint mode only
  String      staSsid;        // populated in Station mode only
};

enum class WifiStaResult {
  Connecting,   // still associating — keep polling
  Connected,    // associated; `out` is filled, stop polling
  NoSsidAvailable,
  AuthenticationFailed,
  ConnectionLost,
  TimedOut,
  Failed,       // other hard error from the stack
};

// Kick off an STA association attempt against the stored credentials.
// Returns false if no creds are saved (caller should go straight to AP).
// Non-blocking; bumps CPU to 240 MHz.
bool          wifiStaBegin();
bool          wifiStaBegin(const String& ssid, const String& pass, bool keepAccessPoint = false);

// Poll the in-flight STA attempt. On Connected, `out` is filled with the
// session and the caller takes ownership. Any other terminal result should
// be followed by wifiStaAbort(). The caller decides when to give up on
// Connecting (typical: 5–10s timeout).
WifiStaResult wifiStaPoll(WifiSession& out);

// Blocking adapter for browser-side credential tests. The AP remains active
// while the station attempt runs, so the browser can receive the result.
WifiStaResult wifiStaTest(const String& ssid, const String& pass,
                          uint32_t timeoutMs = 10000);

// Tear down an in-flight STA attempt. With keepAccessPoint=true, the current
// SoftAP remains available to a browser while the station attempt is cleared.
// Otherwise this also turns Wi-Fi off; the caller typically starts a SoftAP
// next.
void          wifiStaAbort(bool keepAccessPoint = false);

// Bring up SoftAP. Always succeeds. Bumps CPU to 240 MHz if not already.
WifiSession   wifiBeginAccessPoint();

// Tears down whichever mode is active and drops CPU back to 80 MHz.
void          wifiEnd();

#endif  // PAGINOTTO_HAL_WIFI_H
