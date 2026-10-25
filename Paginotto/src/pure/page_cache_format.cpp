#include "page_cache_format.h"

#include <limits>

namespace {
constexpr uint32_t kMagic = 0x50434F4CUL;
constexpr size_t kHeaderBytes = 3 * sizeof(uint32_t);
}

uint32_t pageCacheMagic() { return kMagic; }

size_t pageCacheHeaderBytes() { return kHeaderBytes; }

uint32_t encodePageCacheLayout(const PageCacheLayout& layout) {
  return ((uint32_t)(layout.bodySize & 0xFF))
       | ((uint32_t)(layout.lineGap & 0xFF) << 8)
       | ((uint32_t)(layout.bionic & 0x0F) << 16);
}

bool validatePageCacheHeader(uint32_t magic, uint32_t layoutVersion,
                             uint32_t sourceSize, size_t expectedSize,
                             size_t actualFileBytes,
                             const PageCacheLayout& layout,
                             uint32_t& outCount) {
  if (expectedSize > std::numeric_limits<uint32_t>::max()) return false;
  if (magic != pageCacheMagic()
      || layoutVersion != encodePageCacheLayout(layout)
      || sourceSize != (uint32_t)expectedSize
      || actualFileBytes < pageCacheHeaderBytes() + sizeof(uint32_t)
      || ((actualFileBytes - pageCacheHeaderBytes()) % sizeof(uint32_t)) != 0) {
    return false;
  }
  outCount = (uint32_t)((actualFileBytes - pageCacheHeaderBytes())
                        / sizeof(uint32_t));
  return true;
}
