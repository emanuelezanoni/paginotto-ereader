#ifndef PAGINOTTO_PURE_READER_RESUME_H
#define PAGINOTTO_PURE_READER_RESUME_H

#include <cstdint>

// Result of resolving persisted reader position against the best usable
// page-cache entry. This is deliberately independent of Preferences,
// LittleFS, fonts, and the display so the compatibility contract can be
// tested without hardware.
struct ReaderResumePlan {
  bool     opening = false;
  bool     targetIsPage = false;
  int      initialPage = 0;
  uint32_t initialOffset = 0;
  uint32_t scanPage = 0;
  uint32_t scanOffset = 0;
  uint32_t target = 0;
};

// Resolve the current persistence contract:
// - a saved byte offset is canonical;
// - the saved page is only a legacy fallback when no offset exists;
// - a cache entry ahead of the requested position cannot be used;
// - a missing cache entry falls back to the beginning of the book.
ReaderResumePlan makeReaderResumePlan(bool hasSavedOffset,
                                      uint32_t savedOffset,
                                      int savedPage,
                                      int cachedPage,
                                      uint32_t cachedOffset);

#endif  // PAGINOTTO_PURE_READER_RESUME_H
