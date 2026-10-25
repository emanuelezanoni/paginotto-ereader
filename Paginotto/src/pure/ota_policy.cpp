#include "src/pure/ota_policy.h"

#include <string.h>

bool validOtaChannel(const char* channel) {
  return channel && (strcmp(channel, "stable") == 0 ||
                     strcmp(channel, "dev") == 0);
}

bool otaContentLengthFits(int64_t content_length, size_t partition_size) {
  // -1 is HTTPClient's chunked/unknown sentinel; the OTA writer will enforce
  // the partition boundary while receiving it.
  if (content_length < 0) return true;
  return content_length > 0 && static_cast<uint64_t>(content_length) <= partition_size;
}

bool otaDownloadComplete(int64_t content_length, size_t written) {
  if (content_length < 0) return written > 0;
  return content_length > 0 && written == static_cast<size_t>(content_length);
}
