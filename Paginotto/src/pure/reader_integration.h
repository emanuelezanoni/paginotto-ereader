#ifndef PAGINOTTO_PURE_READER_INTEGRATION_H
#define PAGINOTTO_PURE_READER_INTEGRATION_H

#include <cstddef>
#include <cstdint>

enum class ReaderWakeAction {
  None,
  ConsumeInvalid,
  Restore,
};

struct ReaderWakeDecision {
  ReaderWakeAction action = ReaderWakeAction::None;
  bool clearIntent = false;
};

// The wake key is single-shot: any non-empty value is consumed, even when
// the book has disappeared since sleep.
ReaderWakeDecision decideReaderWake(const char* wakePath, bool bookExists);

struct ReaderLayoutDecision {
  bool remap = false;
  bool invalidateRecentOffsets = false;
  uint32_t savedOffset = 0;
};

// A layout change invalidates the page hint, but not the canonical byte
// position. The caller then resolves that byte under the new layout.
ReaderLayoutDecision decideReaderLayoutRemap(bool readerActive,
                                             bool layoutChanged,
                                             int currentPage,
                                             uint32_t savedOffset);

struct ReaderJumpDecision {
  uint32_t offset = 0;
  bool emptyFile = false;
  bool clamped = false;
};

// Parse and clamp a WebUI jump without depending on Arduino String/toInt.
int64_t parseReaderJumpOffset(const char* text);
ReaderJumpDecision decideReaderJump(int64_t requestedOffset, size_t fileSize);

#endif  // PAGINOTTO_PURE_READER_INTEGRATION_H
