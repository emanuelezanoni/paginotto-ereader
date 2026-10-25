#include "test_framework.h"

#include "src/pure/wifi_credentials.h"

TEST_CASE("wifi credential policy accepts valid network values") {
  CHECK(WifiCredentialPolicy::validSsid("Paginotto"));
  CHECK(WifiCredentialPolicy::validPassword(""));
  CHECK(WifiCredentialPolicy::validPassword(String("123456789012345678901234567890123456789012345678901234567890123")));
}

TEST_CASE("wifi credential policy rejects invalid network values") {
  CHECK(!WifiCredentialPolicy::validSsid(""));
  CHECK(!WifiCredentialPolicy::validSsid(String("123456789012345678901234567890123")));
  CHECK(!WifiCredentialPolicy::validPassword(String("1234567890123456789012345678901234567890123456789012345678901234")));
}
