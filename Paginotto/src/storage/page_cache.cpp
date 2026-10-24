#include "src/storage/page_cache.h"

#include "src/pure/hashing.h"   // prefKeyForBook

// ============================================================================
//  On-disk page-offset cache
//
//  File format (little-endian):
//    uint32 magic            kPageCacheMagic
//    uint32 layoutVersion    encodeLayoutVersion(layout)
//                            — bodySize/lineGap/bionic
//    uint32 fileSize         source-book size at save time
//    uint16 count            number of entries that follow
//    uint32 offsets[count]   byte offsets of pages 0..count-1
//
//  Magic history:
//    0x50434F46 — original, no stamp
//    0x50434F47 — added 16-bit `(bodySize, lineGap)` stamp
//    0x50434F48 — widened stamp to 32 bits to also cover bionic
//    0x50434F4A — removed the obsolete font-family field
//    0x50434F4B — removed obsolete display-reserve field
//
//  Old files fail the magic check, get ignored, then overwritten on the next
//  save. No migration code needed.
// ============================================================================

static constexpr uint32_t kPageCacheMagic = 0x50434F4BUL;

static constexpr size_t kHeaderBytes =
    sizeof(uint32_t)   // magic
  + sizeof(uint32_t)   // layoutVersion
  + sizeof(uint32_t)   // fileSize
  + sizeof(uint16_t);  // count

// Compact encoding of "what layout were the offsets in this file computed
// under?" — every field tucks into one byte (bodySize ∈ {8,10,12,14},
// lineGap ∈ [0,4], bionic ∈ {0,1}.
static uint32_t encodeLayoutVersion(const PageCacheLayout& layout) {
  return ((uint32_t)(layout.bodySize         & 0xFF))
       | ((uint32_t)(layout.lineGap          & 0xFF) << 8)
       | ((uint32_t)(layout.bionic           & 0x0F) << 16);
}

static String pageCachePathForBook(const String& path) {
  return String("/pc_") + prefKeyForBook(path) + ".bin";
}

// Open the cache file for `path`, read + validate the header (magic, layout
// stamp, expected source-file size, non-zero entry count), and return it
// positioned just past the header. On false, any opened file is closed and
// `outFile` / `outCount` are left untouched. Both load functions share this
// gate; only the work that follows differs.
static bool openAndValidateCache(const String& path, size_t expectedSize,
                                 const PageCacheLayout& layout,
                                 File& outFile, uint16_t& outCount) {
  File f = FS.open(pageCachePathForBook(path), "r");
  if (!f) return false;

  uint32_t magic = 0;
  uint32_t layoutVersion = 0;
  uint32_t fileSize = 0;
  uint16_t count = 0;

  if (f.read(reinterpret_cast<uint8_t*>(&magic), sizeof(magic)) != sizeof(magic))                         { f.close(); return false; }
  if (f.read(reinterpret_cast<uint8_t*>(&layoutVersion), sizeof(layoutVersion)) != sizeof(layoutVersion)) { f.close(); return false; }
  if (f.read(reinterpret_cast<uint8_t*>(&fileSize), sizeof(fileSize)) != sizeof(fileSize))                { f.close(); return false; }
  if (f.read(reinterpret_cast<uint8_t*>(&count), sizeof(count)) != sizeof(count))                         { f.close(); return false; }

  if (magic != kPageCacheMagic
      || layoutVersion != encodeLayoutVersion(layout)
      || fileSize != (uint32_t)expectedSize
      || count == 0) {
    f.close();
    return false;
  }

  outFile = f;
  outCount = count;
  return true;
}

bool loadPageOffsetCacheForBook(const String& path, size_t expectedSize,
                                const PageCacheLayout& layout,
                                PageOffsetTable& out) {
  // Dense cache files from the old implementation are not bulk-loaded: doing
  // so made the number of pages a hidden RAM limit. The reader now keeps only
  // its current offset and paginates on demand.
  (void)path;
  (void)expectedSize;
  (void)layout;
  (void)out;
  return false;
}

void savePageOffsetCacheForBook(const String& path, size_t fileSize,
                                const PageCacheLayout& layout,
                                const PageOffsetTable& in) {
  // Compatibility hook. A streaming disk index will be written separately;
  // never recreate the old RAM-sized dense table here.
  (void)path;
  (void)fileSize;
  (void)layout;
  (void)in;
}

int loadOffsetForPageFromDisk(const String& path, size_t expectedSize,
                              const PageCacheLayout& layout,
                              int maxPage, uint32_t* out) {
  if (maxPage < 0) return -1;

  File f;
  uint16_t count = 0;
  if (!openAndValidateCache(path, expectedSize, layout, f, count)) return -1;

  int targetPage = (maxPage >= (int)count) ? (int)count - 1 : maxPage;
  size_t entryPos = kHeaderBytes + (size_t)targetPage * sizeof(uint32_t);
  if (!f.seek(entryPos)) { f.close(); return -1; }

  uint32_t off = 0;
  if (f.read(reinterpret_cast<uint8_t*>(&off), sizeof(off)) != sizeof(off)) { f.close(); return -1; }
  f.close();

  *out = off;
  return targetPage;
}

void deletePageCacheForBook(const String& path) {
  String cachePath = pageCachePathForBook(path);
  if (FS.exists(cachePath)) FS.remove(cachePath);
}

void renamePageCacheForBook(const String& oldPath, const String& newPath) {
  String oldCache = pageCachePathForBook(oldPath);
  if (!FS.exists(oldCache)) return;
  String newCache = pageCachePathForBook(newPath);
  if (FS.exists(newCache)) FS.remove(newCache);
  FS.rename(oldCache, newCache);
}
