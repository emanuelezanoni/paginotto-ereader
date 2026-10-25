#ifndef PALA_CONFIG_H
#define PALA_CONFIG_H

// ============================================================================
//  Project-wide compile-time configuration.
//
//  IMPORTANT: This header is included transitively by the pure/ modules
//  (paginator, etc.), which are also compiled
//  into the host-test build. It MUST therefore remain host-compatible:
//
//    - No <Arduino.h>, <WiFi.h>, or any library header.
//    - No code, only `#define`s, `static const` constants, type aliases,
//      and `extern` declarations of host-safe types.
//    - If you need to add hardware- or library-specific declarations,
//      put them in a different header that only firmware sources include.
//
//  The build will catch violations (the host test compile will fail), but
//  treat this comment as the convention so the surprise is small.
// ============================================================================

#include "src/pure/arduino_compat.h"

// FW_VERSION and BUILD_GIT_HASH are injected by scripts/build_info.py from
// `git describe --tags --always --dirty` and the short SHA respectively, at
// PlatformIO build time. Host-test builds skip that script, so provide
// fallbacks so the preprocessor never sees an undefined macro.
#ifndef FW_VERSION
#define FW_VERSION "dev"
#endif

#ifndef BUILD_GIT_HASH
#define BUILD_GIT_HASH "unknown"
#endif

#ifndef BUILD_GIT_TIMESTAMP
#define BUILD_GIT_TIMESTAMP 0
#endif

// DEBUG_BUILD = 1 shows the git hash in the on-device library screen header.
// Keep it disabled for normal device builds: the firmware identity should be
// the product name, not the state of the developer's working tree.
#ifndef DEBUG_BUILD
#define DEBUG_BUILD 0
#endif

#if DEBUG_BUILD
  #define LIB_HEADER_TITLE "Paginotto " BUILD_GIT_HASH
#else
  #define LIB_HEADER_TITLE "Paginotto"
#endif

// The firmware string table lives in src/lang/lang.h and is included at the
// end of this header so every TU that pulls in config.h transitively sees the
// D_* macros.

static const int SCREEN_W = 250;
static const int SCREEN_H = 122;

static const int MAX_BOOKS = 80;
static const int MAX_FOLDERS = 32;
static const int MAX_FOLDER_PATH = 63;  // chars, excluding null
static const int MAX_FOLDER_NAME_BYTES = 24;
// Keep enough entries for large plain-text books. 10,000 was an artificial
// ceiling: readers that exceeded it appeared to reach EOF even when bytes
// remained in the file. The offset table is static, so this is deliberately
// increased only to 16,000 rather than made unbounded.
static const int MAX_LIBRARY_ENTRIES = (MAX_BOOKS * 2) + (MAX_FOLDERS * 2) + 8;
// Conservative character limit for book labels in the e-ink library list.
// The renderer appends an ellipsis when a label reaches this limit.
static const int MAX_DEVICE_BOOK_LABEL_CHARS = 32;
// BookInfo::name has 80 bytes including the terminator. This is the maximum
// safe title length exposed by the WebUI; the display has its own smaller
// visual limit above.
static const int MAX_BOOK_TITLE_BYTES = 79;

// Optional external catalog opened by the Books page. Leave empty until the
// catalog URL is known; the direct EPUB URL importer works independently.
static const char ONLINE_BOOK_LIBRARY_URL[] = "https://open-slum.org/";

// Max silence after the most recent release before we commit a click sequence.
// The press-in-progress gate in input.cpp's trailing-silence check means this
// effectively bounds "dead time between release and the next press" — once a
// press starts, the commit pauses until that release lands. So the user can
// take this long to start next click.
static const uint32_t MAX_CLICK_GAP_MS = 175;

// Max total duration of a multi-click sequence, measured from the first release.
// Conceptually: "the whole multi-click input has to complete within this window."
// Combined with MAX_CLICK_GAP_MS this caps both per-gap and overall sloppiness.
static const uint32_t MAX_CLICK_SEQUENCE_MS = 550;

static const uint32_t CLICK_HOLD_MS = 850;

static const uint32_t DEBOUNCE_MS = 14;

static const uint32_t SAVE_EVERY_MS = 7000;
static const uint32_t TOAST_MS = 650;
static const uint32_t UPLOAD_INACTIVITY_TIMEOUT_MS = 5UL * 60UL * 1000UL;
// Standalone Wi-Fi picker/update screens return to the library after five
// minutes without a physical-button interaction.
static const uint32_t WIFI_SCREEN_INACTIVITY_TIMEOUT_MS = 5UL * 60UL * 1000UL;
static const uint32_t BAT_CACHE_MS = 180000; // 3 min — battery changes slowly

static const int FULL_REFRESH_EVERY_N_PAGES = 100;
static const int MENU_FULL_REFRESH_EVERY = 60;

static const int MARGIN_X = 6;
static const int TOP_PAD = 0;
static const int BOT_PAD = 0;
static const int STATUS_H = 8;

static const bool SHOW_PROGRESS_BAR = true;
static const bool SHOW_PAGE_NUMBER = true;
static const bool ENABLE_DEEP_SLEEP = true;

#define BTN 0
#define HAS_BATTERY 1
#if HAS_BATTERY
  #define BAT_ADC_CTRL 19
  #define BAT_ADC_IN   20
#endif

// NOTE: `#define FS LittleFS` lives in state.h AFTER all system headers, so it
// doesn't collide with the `class FS` declared inside Arduino's FS.h.

// Fonts live behind the role API in `ui/font.h` (Font::useBody/useBold/
// useUiSmall/useUiTiny). No code outside font.cpp references u8g2 font
// tables directly.

// UI strings (D_* macros) live in src/lang/. Included here so every
// TU that pulls in config.h transitively sees the macros without per-file
// includes. lang.h is pure preprocessor — no Arduino dependencies, host-test
// safe.
#include "src/lang/lang.h"

#endif  // PALA_CONFIG_H
