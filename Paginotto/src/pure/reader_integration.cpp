#include "reader_integration.h"

#include <cerrno>
#include <cstdlib>
#include <limits>

ReaderWakeDecision decideReaderWake(const char* wakePath, bool bookExists) {
  ReaderWakeDecision decision;
  decision.clearIntent = true;
  if (wakePath == nullptr || wakePath[0] == '\0') return decision;
  decision.action = bookExists ? ReaderWakeAction::Restore
                               : ReaderWakeAction::ConsumeInvalid;
  return decision;
}

ReaderLayoutDecision decideReaderLayoutRemap(bool readerActive,
                                             bool layoutChanged,
                                             int currentPage,
                                             uint32_t savedOffset) {
  ReaderLayoutDecision decision;
  decision.savedOffset = savedOffset;
  decision.remap = readerActive && layoutChanged && currentPage >= 0;
  return decision;
}

int64_t parseReaderJumpOffset(const char* text) {
  if (text == nullptr || text[0] == '\0') return 0;
  errno = 0;
  char* end = nullptr;
  const long long parsed = std::strtoll(text, &end, 10);
  if (end == text) return 0;
  if (errno == ERANGE) {
    return parsed < 0 ? std::numeric_limits<int64_t>::min()
                      : std::numeric_limits<int64_t>::max();
  }
  return static_cast<int64_t>(parsed);
}

ReaderJumpDecision decideReaderJump(int64_t requestedOffset, size_t fileSize) {
  ReaderJumpDecision decision;
  if (fileSize == 0) {
    decision.emptyFile = true;
    return decision;
  }

  const uint64_t lastFileByte = static_cast<uint64_t>(fileSize - 1);
  const uint64_t maxOffset =
      lastFileByte < std::numeric_limits<uint32_t>::max()
          ? lastFileByte
          : std::numeric_limits<uint32_t>::max();
  uint64_t normalized = requestedOffset < 0
                            ? 0u
                            : static_cast<uint64_t>(requestedOffset);
  if (normalized > maxOffset) {
    normalized = maxOffset;
    decision.clamped = true;
  } else if (requestedOffset < 0) {
    decision.clamped = true;
  }
  decision.offset = static_cast<uint32_t>(normalized);
  return decision;
}
