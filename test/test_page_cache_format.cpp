#include "test_framework.h"
#include "src/pure/page_cache_format.h"

TEST_CASE("page cache layout encoding is deterministic") {
  PageCacheLayout layout{10, 2, 1};
  CHECK(encodePageCacheLayout(layout) == 0x0001020Au);
  CHECK(pageCacheMagic() == 0x50434F4Cu);
  CHECK(pageCacheHeaderBytes() == 12u);
}

TEST_CASE("page cache header accepts valid entry counts") {
  PageCacheLayout layout{12, 0, 0};
  uint32_t count = 0;
  CHECK(validatePageCacheHeader(pageCacheMagic(), encodePageCacheLayout(layout),
                                4096, 4096, 12 + 4 * 3, layout, count));
  CHECK(count == 3u);
}

TEST_CASE("page cache header rejects stale or malformed files") {
  PageCacheLayout layout{12, 0, 0};
  uint32_t count = 77;
  const uint32_t encoded = encodePageCacheLayout(layout);
  CHECK(!validatePageCacheHeader(0, encoded, 4096, 4096, 16, layout, count));
  CHECK(!validatePageCacheHeader(pageCacheMagic(), encoded + 1, 4096,
                                 4096, 16, layout, count));
  CHECK(!validatePageCacheHeader(pageCacheMagic(), encoded, 4095,
                                 4096, 16, layout, count));
  CHECK(!validatePageCacheHeader(pageCacheMagic(), encoded, 4096,
                                 4096, 15, layout, count));
  CHECK(count == 77u);
}

TEST_CASE("page cache layout changes invalidate the stamp") {
  PageCacheLayout saved{10, 0, 0};
  PageCacheLayout current{10, 1, 0};
  uint32_t count = 0;
  CHECK(!validatePageCacheHeader(pageCacheMagic(), encodePageCacheLayout(saved),
                                 100, 100, 16, current, count));
}
