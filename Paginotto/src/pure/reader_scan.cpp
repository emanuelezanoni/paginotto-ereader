#include "reader_scan.h"

bool readerScanTargetReached(const ReaderScanState& state) {
  return state.targetIsPage ? state.page >= state.target
                            : state.offset >= state.target;
}

ReaderScanStep advanceReaderScan(ReaderScanState& state, uint32_t nextOffset) {
  if (readerScanTargetReached(state)) return ReaderScanStep::TargetReached;
  if (nextOffset <= state.offset) {
    state.eof = true;
    return ReaderScanStep::EndOfFile;
  }

  state.offset = nextOffset;
  ++state.page;
  if (readerScanTargetReached(state)) return ReaderScanStep::TargetReached;
  return ReaderScanStep::Continue;
}

void cancelReaderScan(ReaderScanState& state) {
  state.page = 0;
  state.offset = 0;
  state.target = 0;
  state.targetIsPage = false;
}
