#include "src/pure/wifi_credentials.h"

namespace WifiCredentialPolicy {

bool validSsid(const String& ssid) {
  return ssid.length() > 0 && ssid.length() <= 32;
}

bool validPassword(const String& password) {
  return password.length() <= 63;
}

}  // namespace WifiCredentialPolicy
