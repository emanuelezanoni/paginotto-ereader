#ifndef PAGINOTTO_STORAGE_WIFI_CREDS_H
#define PAGINOTTO_STORAGE_WIFI_CREDS_H

#include <Arduino.h>

// Persisted home-network Wi-Fi credentials, written by the Improv Serial flow
// and read by the upload screen when deciding STA vs SoftAP. Stored alongside
// the project's other settings in the shared "ereader" NVS namespace
// (`prefs`), under per-module keys following the existing convention.
namespace WifiCreds {

static constexpr uint8_t kMaxNetworks = 5;

struct Network { String ssid; String pass; };

uint8_t count();
bool has();
Network network(uint8_t index);
bool add(const String& ssid, const String& pass); // replaces same SSID; false when full
void remove(uint8_t index);

// Legacy convenience for firmware-update flow. It deliberately keeps using
// first saved network; WebUI has explicit user selection and no fallback.
String ssid();
String pass();
void save(const String& ssid, const String& pass);
void clear();

}  // namespace WifiCreds

#endif  // PAGINOTTO_STORAGE_WIFI_CREDS_H
