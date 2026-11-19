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

// The bookmark may be written only from a settled reader view. While a book
// is still opening, the view holds the scan cursor (typically offset 0), and
// the bookmark already stored in NVS is the only valid position.
bool readerBookmarkWritable(bool bookOpen, bool openingBook);

struct LockedLoopDecision {
  bool unlock = false;       // disengage the lock and draw the real screen
  bool rearmIdle = false;    // a non-unlock press: give the user time to retry
  bool sleep = false;        // go back to deep sleep
};

// One main-loop iteration while locked. A non-unlock press never forces
// sleep (a slightly short unlock hold must stay retryable); only the short
// locked-idle timeout does, and never while a gesture may still complete.
LockedLoopDecision decideLockedLoop(bool unlockGesture, bool anyEvent,
                                    bool sleepAllowed, uint32_t idleMs,
                                    uint32_t idleLimitMs, bool buttonPressed,
                                    bool clicksPending);

#endif  // PAGINOTTO_PURE_READER_INTEGRATION_H
