#ifndef PAGINOTTO_PURE_READER_SCAN_H
#define PAGINOTTO_PURE_READER_SCAN_H

#include <cstdint>

enum class ReaderScanStep {
  Continue,
  TargetReached,
  EndOfFile,
};

struct ReaderScanState {
  uint32_t page = 0;
  uint32_t offset = 0;
  uint32_t target = 0;
  bool targetIsPage = false;
  bool eof = false;
};

bool readerScanTargetReached(const ReaderScanState& state);

// Advance one page in the incremental opening scan. The caller supplies the
// next offset produced by the paginator; no filesystem or UI is involved.
// Non-progressing offsets are treated as EOF/corruption and never accepted.
ReaderScanStep advanceReaderScan(ReaderScanState& state, uint32_t nextOffset);

// Clear the transient scan cursor after a user cancellation. The EOF flag is
// intentionally preserved because it belongs to the active book view, not
// to the discarded opening operation.
void cancelReaderScan(ReaderScanState& state);

#endif  // PAGINOTTO_PURE_READER_SCAN_H
