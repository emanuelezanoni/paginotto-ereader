#include "reader_navigation.h"

ReaderAdvanceDecision decideReaderAdvance(uint32_t currentPage,
                                          uint32_t currentOffset,
                                          uint32_t nextOffset,
                                          size_t fileSize,
                                          bool alreadyAtEof) {
  ReaderAdvanceDecision decision;
  decision.page = currentPage;
  decision.offset = currentOffset;
  if (alreadyAtEof || fileSize == 0 || nextOffset <= currentOffset ||
      static_cast<uint64_t>(nextOffset) >= static_cast<uint64_t>(fileSize)) {
    decision.eof = alreadyAtEof || fileSize == 0 || nextOffset <= currentOffset ||
                   static_cast<uint64_t>(nextOffset) >= static_cast<uint64_t>(fileSize);
    return decision;
  }

  decision.moved = true;
  decision.page = currentPage + 1;
  decision.offset = nextOffset;
  return decision;
}

ReaderRetreatDecision decideReaderRetreat(int currentPage) {
  ReaderRetreatDecision decision;
  if (currentPage <= 0) return decision;
  decision.moved = true;
  decision.targetPage = currentPage - 1;
  return decision;
}
