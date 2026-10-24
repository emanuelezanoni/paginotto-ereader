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
//    uint32 offsets[]         byte offsets of pages 0..N-1
//
//  Magic history:
//    0x50434F46 — original, no stamp
//    0x50434F47 — added 16-bit `(bodySize, lineGap)` stamp
//    0x50434F48 — widened stamp to 32 bits to also cover bionic
//    0x50434F4A — removed the obsolete font-family field
//    0x50434F4B — removed obsolete display-reserve field
//    0x50434F4C — streaming index; count is derived from file size
//
//  Old files fail the magic check, get ignored, then overwritten on the next
//  save. No migration code needed.
// ============================================================================

static constexpr uint32_t kPageCacheMagic = 0x50434F4CUL;

static constexpr size_t kHeaderBytes =
    sizeof(uint32_t)   // magic
  + sizeof(uint32_t)   // layoutVersion
  + sizeof(uint32_t);  // fileSize

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
// stamp, expected source-file size, and at least page zero), and return it
// positioned just past the header. On false, any opened file is closed and
// `outFile` / `outCount` are left untouched. Both load functions share this
// gate; only the work that follows differs.
static bool openAndValidateCache(const String& path, size_t expectedSize,
                                 const PageCacheLayout& layout,
                                 File& outFile, uint32_t& outCount) {
  File f = FS.open(pageCachePathForBook(path), "r");
  if (!f) return false;

  uint32_t magic = 0;
  uint32_t layoutVersion = 0;
  uint32_t fileSize = 0;
  uint32_t count = 0;

  if (f.read(reinterpret_cast<uint8_t*>(&magic), sizeof(magic)) != sizeof(magic))                         { f.close(); return false; }
  if (f.read(reinterpret_cast<uint8_t*>(&layoutVersion), sizeof(layoutVersion)) != sizeof(layoutVersion)) { f.close(); return false; }
  if (f.read(reinterpret_cast<uint8_t*>(&fileSize), sizeof(fileSize)) != sizeof(fileSize))                { f.close(); return false; }
  if (magic != kPageCacheMagic
      || layoutVersion != encodeLayoutVersion(layout)
      || fileSize != (uint32_t)expectedSize
      || f.size() < kHeaderBytes + sizeof(uint32_t)
      || ((f.size() - kHeaderBytes) % sizeof(uint32_t)) != 0) {
    f.close();
    return false;
  }

  count = (uint32_t)((f.size() - kHeaderBytes) / sizeof(uint32_t));

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

static bool writeCacheHeader(File& f, size_t fileSize,
                             const PageCacheLayout& layout) {
  uint32_t magic = kPageCacheMagic;
  uint32_t layoutVersion = encodeLayoutVersion(layout);
  uint32_t sourceSize = (uint32_t)fileSize;
  return f.write(reinterpret_cast<const uint8_t*>(&magic), sizeof(magic)) == sizeof(magic)
      && f.write(reinterpret_cast<const uint8_t*>(&layoutVersion), sizeof(layoutVersion)) == sizeof(layoutVersion)
      && f.write(reinterpret_cast<const uint8_t*>(&sourceSize), sizeof(sourceSize)) == sizeof(sourceSize);
}

static bool createPageOffsetCache(const String& path, size_t fileSize,
                                  const PageCacheLayout& layout) {
  String cachePath = pageCachePathForBook(path);
  if (FS.exists(cachePath)) FS.remove(cachePath);
  File f = FS.open(cachePath, "w");
  if (!f || !writeCacheHeader(f, fileSize, layout)) {
    if (f) f.close();
    return false;
  }
  uint32_t zero = 0;
  bool ok = f.write(reinterpret_cast<const uint8_t*>(&zero), sizeof(zero)) == sizeof(zero);
  f.close();
  return ok;
}

bool ensurePageOffsetCacheForBook(const String& path, size_t fileSize,
                                  const PageCacheLayout& layout) {
  File f;
  uint32_t count = 0;
  if (openAndValidateCache(path, fileSize, layout, f, count)) {
    f.close();
    return true;
  }
  return createPageOffsetCache(path, fileSize, layout);
}

uint32_t pageOffsetCacheCount(const String& path, size_t fileSize,
                              const PageCacheLayout& layout) {
  File f;
  uint32_t count = 0;
  if (!openAndValidateCache(path, fileSize, layout, f, count)) return 0;
  f.close();
  return count;
}

bool appendPageOffsetForBook(const String& path, size_t fileSize,
                             const PageCacheLayout& layout,
                             uint32_t pageIndex, uint32_t offset) {
  if (!ensurePageOffsetCacheForBook(path, fileSize, layout)) return false;
  uint32_t count = pageOffsetCacheCount(path, fileSize, layout);
  if (count == 0 || pageIndex < count) return true;
  if (pageIndex != count) return false;

  File f = FS.open(pageCachePathForBook(path), "a");
  if (!f) return false;
  bool ok = f.write(reinterpret_cast<const uint8_t*>(&offset), sizeof(offset)) == sizeof(offset);
  f.close();
  return ok;
}

int loadOffsetForPageFromDisk(const String& path, size_t expectedSize,
                              const PageCacheLayout& layout,
                              int maxPage, uint32_t* out) {
  if (maxPage < 0) return -1;

  File f;
  uint32_t count = 0;
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
