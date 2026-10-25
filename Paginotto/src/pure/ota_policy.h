#ifndef PAGINOTTO_PURE_OTA_POLICY_H
#define PAGINOTTO_PURE_OTA_POLICY_H

#include <stddef.h>
#include <stdint.h>

bool validOtaChannel(const char* channel);
bool otaContentLengthFits(int64_t content_length, size_t partition_size);
bool otaDownloadComplete(int64_t content_length, size_t written);

#endif  // PAGINOTTO_PURE_OTA_POLICY_H
