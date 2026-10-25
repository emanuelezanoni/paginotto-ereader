#ifndef PAGINOTTO_PURE_READER_NAVIGATION_H
#define PAGINOTTO_PURE_READER_NAVIGATION_H

#include <cstddef>
#include <cstdint>

struct ReaderAdvanceDecision {
  bool     moved = false;
  bool     eof = false;
  uint32_t page = 0;
  uint32_t offset = 0;
};

// Decide whether a forward page turn is valid. The actual paginator remains
// outside this module: callers provide the candidate next-page offset.
ReaderAdvanceDecision decideReaderAdvance(uint32_t currentPage,
                                          uint32_t currentOffset,
                                          uint32_t nextOffset,
                                          size_t fileSize,
                                          bool alreadyAtEof);

#endif  // PAGINOTTO_PURE_READER_NAVIGATION_H
