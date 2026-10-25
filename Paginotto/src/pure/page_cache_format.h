#ifndef PAGINOTTO_PURE_PAGE_CACHE_FORMAT_H
#define PAGINOTTO_PURE_PAGE_CACHE_FORMAT_H

#include <cstddef>
#include <cstdint>

struct PageCacheLayout {
  int     bodySize;
  int     lineGap;
  uint8_t bionic;
};

uint32_t pageCacheMagic();
size_t pageCacheHeaderBytes();
uint32_t encodePageCacheLayout(const PageCacheLayout& layout);

// Validate the fixed header and file-size-derived entry count without
// touching LittleFS. `actualFileBytes` includes the three-word header.
bool validatePageCacheHeader(uint32_t magic, uint32_t layoutVersion,
                             uint32_t sourceSize, size_t expectedSize,
                             size_t actualFileBytes,
                             const PageCacheLayout& layout,
                             uint32_t& outCount);

#endif  // PAGINOTTO_PURE_PAGE_CACHE_FORMAT_H
