#include "src/storage/wifi_creds.h"

#include "src/state.h"  // prefs

namespace WifiCreds {

static constexpr const char* kLegacySsid = "wifi_ssid";
static constexpr const char* kLegacyPass = "wifi_pass";

static String key(const char* prefix, uint8_t index) { return String(prefix) + index; }

static void migrate() {
  if (prefs.getBool("wifi_multi_v1", false)) return;
  String oldSsid = prefs.getString(kLegacySsid, "");
  if (oldSsid.length()) {
    prefs.putString("wifi_s0", oldSsid);
    prefs.putString("wifi_p0", prefs.getString(kLegacyPass, ""));
  }
  prefs.remove(kLegacySsid);
  prefs.remove(kLegacyPass);
  prefs.putBool("wifi_multi_v1", true);
}

uint8_t count() {
  migrate();
  uint8_t n = 0;
  for (; n < kMaxNetworks; n++) if (!prefs.getString(key("wifi_s", n).c_str(), "").length()) break;
  return n;
}

bool has() {
  return count() > 0;
}

Network network(uint8_t index) {
  Network out;
  if (index >= count()) return out;
  out.ssid = prefs.getString(key("wifi_s", index).c_str(), "");
  out.pass = prefs.getString(key("wifi_p", index).c_str(), "");
  return out;
}

bool add(const String& inSsid, const String& inPass) {
  uint8_t n = count();
  uint8_t slot = n;
  for (uint8_t i = 0; i < n; i++) if (network(i).ssid == inSsid) { slot = i; break; }
  if (slot == n && n >= kMaxNetworks) return false;
  prefs.putString(key("wifi_s", slot).c_str(), inSsid);
  prefs.putString(key("wifi_p", slot).c_str(), inPass);
  return true;
}

void remove(uint8_t index) {
  uint8_t n = count();
  if (index >= n) return;
  for (uint8_t i = index; i + 1 < n; i++) {
    Network next = network(i + 1);
    prefs.putString(key("wifi_s", i).c_str(), next.ssid);
    prefs.putString(key("wifi_p", i).c_str(), next.pass);
  }
  prefs.remove(key("wifi_s", n - 1).c_str());
  prefs.remove(key("wifi_p", n - 1).c_str());
}

String ssid() { return network(0).ssid; }
String pass() { return network(0).pass; }

void save(const String& ssid, const String& pass) {
  add(ssid, pass);
}

void clear() {
  while (count()) remove(0);
}

}  // namespace WifiCreds
