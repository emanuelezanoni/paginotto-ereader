#include "test_framework.h"
#include "pure/ota_policy.h"

TEST_CASE("OTA policy accepts only published channels") {
  CHECK(validOtaChannel("stable"));
  CHECK(validOtaChannel("dev"));
  CHECK(!validOtaChannel("staging"));
  CHECK(!validOtaChannel("dev/../stable"));
  CHECK(!validOtaChannel(nullptr));
}

TEST_CASE("OTA policy validates lengths and truncation") {
  CHECK(otaContentLengthFits(-1, 100));
  CHECK(otaContentLengthFits(100, 100));
  CHECK(!otaContentLengthFits(0, 100));
  CHECK(!otaContentLengthFits(101, 100));
  CHECK(otaDownloadComplete(-1, 1));
  CHECK(!otaDownloadComplete(-1, 0));
  CHECK(otaDownloadComplete(100, 100));
  CHECK(!otaDownloadComplete(100, 99));
}
