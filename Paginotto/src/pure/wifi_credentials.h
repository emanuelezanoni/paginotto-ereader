#ifndef PAGINOTTO_PURE_WIFI_CREDENTIALS_H
#define PAGINOTTO_PURE_WIFI_CREDENTIALS_H

#include "arduino_compat.h"

namespace WifiCredentialPolicy {

// IEEE 802.11 SSIDs are at most 32 octets. The Arduino String length is a
// byte length, which is the relevant limit for the Wi-Fi API.
bool validSsid(const String& ssid);

// WPA-PSK passphrases are at most 63 octets. Empty passwords are valid for
// open networks and are therefore intentionally accepted.
bool validPassword(const String& password);

}  // namespace WifiCredentialPolicy

#endif  // PAGINOTTO_PURE_WIFI_CREDENTIALS_H
