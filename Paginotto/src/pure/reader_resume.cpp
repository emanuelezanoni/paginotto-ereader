#include "reader_resume.h"

ReaderResumePlan makeReaderResumePlan(bool hasSavedOffset,
                                      uint32_t savedOffset,
                                      int savedPage,
                                      int cachedPage,
                                      uint32_t cachedOffset) {
  ReaderResumePlan plan;

  if (hasSavedOffset) {
    // Offset zero is the normal new-book state. It must not start an
    // asynchronous scan merely because an old page hint exists.
    if (savedOffset == 0) return plan;

    if (cachedPage < 0 || cachedOffset > savedOffset) {
      cachedPage = 0;
      cachedOffset = 0;
    }

    plan.opening = cachedOffset < savedOffset;
    plan.initialPage = plan.opening ? 0 : cachedPage;
    plan.initialOffset = plan.opening ? 0 : cachedOffset;
    plan.scanPage = static_cast<uint32_t>(cachedPage);
    plan.scanOffset = cachedOffset;
    plan.target = savedOffset;
    return plan;
  }

  // Legacy firmware stored only a page number. A missing/negative page is
  // equivalent to opening the book at its beginning.
  if (savedPage <= 0) return plan;

  if (cachedPage < 0) {
    cachedPage = 0;
    cachedOffset = 0;
  }

  plan.targetIsPage = true;
  plan.opening = static_cast<uint32_t>(cachedPage) <
                 static_cast<uint32_t>(savedPage);
  plan.initialPage = cachedPage;
  plan.initialOffset = cachedOffset;
  plan.scanPage = static_cast<uint32_t>(cachedPage);
  plan.scanOffset = cachedOffset;
  plan.target = static_cast<uint32_t>(savedPage);
  return plan;
}
