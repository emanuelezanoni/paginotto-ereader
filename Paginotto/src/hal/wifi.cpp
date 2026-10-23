#include "src/hal/wifi.h"

#include <ESPmDNS.h>
#include <WiFi.h>
#include <esp_bt.h>
#include <esp_wifi.h>

#include "src/state.h"
#include "src/storage/wifi_creds.h"

static constexpr const char* kMdnsHost = "paginotto";

// Cached STA SSID so wifiStaPoll() can fill the WifiSession without going
// back to NVS. Cleared by wifiStaAbort() / wifiEnd().
static String s_staSsid;

bool wifiStaBegin() {
  if (!WifiCreds::has()) return false;
  return wifiStaBegin(WifiCreds::ssid(), WifiCreds::pass());
}

bool wifiStaBegin(const String& ssid, const String& pass, bool keepAccessPoint) {
  if (!ssid.length()) return false;

  setCpuFrequencyMhz(240);

  s_staSsid = ssid;

  WiFi.mode(keepAccessPoint ? WIFI_AP_STA : WIFI_STA);
  WiFi.begin(s_staSsid.c_str(), pass.c_str());
  return true;
}

WifiStaResult wifiStaPoll(WifiSession& out) {
  switch (WiFi.status()) {
    case WL_CONNECTED: {
      IPAddress ip = WiFi.localIP();
      MDNS.begin(kMdnsHost);
      MDNS.addService("http", "tcp", 80);

      out.mode        = WifiMode::Station;
      out.staSsid     = s_staSsid;
      out.primaryUrl  = String("http://") + kMdnsHost + ".local";
      out.fallbackUrl = String("http://") + ip.toString();
      return WifiStaResult::Connected;
    }
    case WL_NO_SSID_AVAIL:
      return WifiStaResult::NoSsidAvailable;
    case WL_CONNECT_FAILED:
      return WifiStaResult::AuthenticationFailed;
    case WL_CONNECTION_LOST:
      return WifiStaResult::ConnectionLost;
    default:
      return WifiStaResult::Connecting;
  }
}

WifiStaResult wifiStaTest(const String& ssid, const String& pass, uint32_t timeoutMs) {
  if (!wifiStaBegin(ssid, pass, true)) return WifiStaResult::Failed;

  WifiSession session;
  const uint32_t startedMs = millis();
  const uint32_t failureGraceMs = 1500;
  WifiStaResult result = WifiStaResult::Connecting;
  while ((uint32_t)(millis() - startedMs) < timeoutMs) {
    result = wifiStaPoll(session);
    if (result == WifiStaResult::Connected) break;
    if (result != WifiStaResult::Connecting &&
        (uint32_t)(millis() - startedMs) >= failureGraceMs) break;
    delay(50);
  }
  if (result == WifiStaResult::Connecting) result = WifiStaResult::TimedOut;
  wifiStaAbort(true);
  return result;
}

void wifiStaAbort(bool keepAccessPoint) {
  WiFi.disconnect(!keepAccessPoint, !keepAccessPoint);
  if (!keepAccessPoint) WiFi.mode(WIFI_OFF);
  s_staSsid = "";
  // CPU clock stays at 240 MHz — the typical next call is
  // wifiBeginAccessPoint() which needs it anyway. wifiEnd() drops it.
}

WifiSession wifiBeginAccessPoint() {
  setCpuFrequencyMhz(240);

  WiFi.mode(WIFI_AP);
  String ssid = hotspotSsid();
  String pass = hotspotPass();
  WiFi.softAP(ssid.c_str(), pass.c_str());
  IPAddress ip = WiFi.softAPIP();

  WifiSession s;
  s.mode       = WifiMode::AccessPoint;
  s.apSsid     = ssid;
  s.apPass     = pass;
  s.primaryUrl = String("http://") + ip.toString();
  return s;
}

void wifiEnd() {
  MDNS.end();
  WiFi.softAPdisconnect(true);
  WiFi.disconnect(true, true);
  WiFi.mode(WIFI_OFF);
  delay(100);
  esp_wifi_stop();
  btStop();
  s_staSsid = "";
  setCpuFrequencyMhz(80);  // back to low-power idle
}
