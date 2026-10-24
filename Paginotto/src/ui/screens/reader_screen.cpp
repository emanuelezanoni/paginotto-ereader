#include "src/ui/screens/reader_screen.h"
#include "src/diagnostics/log.h"

#include "src/hal/display.h"
#include "src/hal/input.h"
#include "src/ui/lock.h"                     // Lock::engage on ACTION_LOCK
#include "src/ui/reader.h"
#include "src/ui/reader_actions.h"           // Gestures::actionFor + ButtonAction
#include "src/ui/screens/library_screen.h"
#include "src/ui/sleep.h"                    // Sleep::enter on ACTION_LOCK
#include "src/ui/text.h"

// Carry out one of the bindable reader actions. No-op for ACTION_NONE.
// Lives at the screen layer because the actions are reader-context things
// (lock) — not generic input-layer concepts.
static void performReaderAction(ButtonAction action) {
  switch (action) {
    case ACTION_LOCK:
      // Persist lock state, then sleep immediately. On wake, the main loop
      // sees Lock::isLocked() and swallows input until an unlock gesture.
      // ReaderScreen::onSleep persists progress + arms resume, so the next
      // boot lands back on this page.
      Lock::engage();
      Sleep::enter();   // does not return
      break;
    case ACTION_NONE:
    default:
      break;
  }
}

void ReaderScreen::onEnter() {
  Diagnostics::checkpoint("reader.screen.enter");
  draw();
}

void ReaderScreen::draw() {
  Diagnostics::trace("reader.screen.draw");
  renderCurrentPage();
}

void ReaderScreen::onButton(const ButtonEvent& e) {
  Diagnostics::trace("reader.screen.button");
  if (isOpeningBook()) {
    if (e.kind == ButtonEvent::Triple) {
      // LibraryScreen::onEnter() performs the full reader reset after the
      // transition. Stop the incremental scan immediately so no stale file
      // handle survives the cancel gesture.
      cancelBookOpening();
      navigateToLibraryRoot();
    }
    return;
  }

  if (e.kind == ButtonEvent::Triple) {
    // Catch any progress / pagination since the last throttled save before
    // we lose the active book.
    persistReaderState();
    navigateToLibraryRoot();
    return;
  }

  // Remappable hold gestures dispatch through the user's binding. NB:
  // An unbound gesture (ACTION_NONE) is a no-op.
  if (e.kind == ButtonEvent::Hold || e.kind == ButtonEvent::ClickHold) {
    performReaderAction(Gestures::actionFor(e.kind));
    return;
  }

  if (e.kind == ButtonEvent::Double) {
    if (retreatPage()) {
      saveProgressThrottled();
      draw();
    }
    return;
  }

  if (e.kind == ButtonEvent::Short) {
    if (advancePage()) {
      saveProgressThrottled();
      draw();
    }
    return;
  }
}

void ReaderScreen::onIdleTick() {
  if (!isOpeningBook()) return;
  serviceBookOpening();
  if (!isOpeningBook()) draw();
}

void ReaderScreen::onSleep() {
  Diagnostics::checkpoint("reader.screen.sleep");
  // Strong invariant: reader screen is never active without an open book.
  persistReaderState();
  armResumeOnWake();        // captures path before resetBookView() drops it
  resetBookView();
}
