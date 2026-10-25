#include "src/ui/font.h"

#include <string.h>             // memcpy
#include <U8g2_for_Adafruit_GFX.h>

#include "src/config.h"
#include "src/hal/display.h"  // u8g2 instance
#include "src/state.h"        // prefs
#include "src/pure/bionic_text.h"

namespace Font {

// File-private font tables. The rest of the codebase only ever sees the
// role accessors (useBody, useBold, ...).
static const uint8_t* s_body    = u8g2_font_helvR08_te;
static const uint8_t* s_bold    = u8g2_font_helvB08_te;
// _tf = ASCII only. Translated strings must NOT use these — see font.h.
static const uint8_t* s_uiSmall = u8g2_font_6x10_tf;
static const uint8_t* s_uiTiny  = u8g2_font_5x8_tf;
// _te = Latin Extended. Used by toasts so translations with accents render.
static const uint8_t* s_toast   = u8g2_font_helvR08_te;

// Owned settings.
//   s_size    — body font size (8/10/12/14). Out-of-set values fall back to 10.
//   s_lineGap — extra px between body lines, [0, 4].
//   s_bionic  — render body text in "bionic reading" mode (bold prefix of
//               each word). Hooked into text.cpp's draw path; the paginator
//               picks it up via measureBionicWord.
static int    s_size    = 10;
static int    s_lineGap = 0;
static bool   s_bionic  = false;

// Layout-metrics cache. Invalid after the mutators below; recomputed on the
// next bodyLayout() call.
static LayoutMetrics s_layout;
static bool          s_layoutValid = false;

// NVS keys — file-private; nothing outside font.cpp should reference them.
static constexpr const char* kKeyBodySize   = "cfg_font";
static constexpr const char* kKeyLineGap    = "cfg_lgap";
static constexpr const char* kKeyBionic     = "cfg_bionic";

// Pick the (regular, bold) pair for a given size + family. Centralized so
// applyBodySize and applyFamily share one switch. Returns true if `sz` was
// in the supported set; out params are always set (10pt fallback otherwise).
static bool pickFaces(int sz,
                      const uint8_t*& outBody, const uint8_t*& outBold) {
  switch (sz) {
    case 8:  outBody = u8g2_font_helvR08_te; outBold = u8g2_font_helvB08_te; return true;
    case 10: outBody = u8g2_font_helvR10_te; outBold = u8g2_font_helvB10_te; return true;
    case 12: outBody = u8g2_font_helvR12_te; outBold = u8g2_font_helvB12_te; return true;
    case 14: outBody = u8g2_font_helvR14_te; outBold = u8g2_font_helvB14_te; return true;
    default: outBody = u8g2_font_helvR10_te; outBold = u8g2_font_helvB10_te; return false;
  }
}

// In-memory body-size apply: select font tables + invalidate the layout
// cache. Any out-of-set size falls back to 10. Does NOT persist — public
// callers go through setBodySize() / loadSettings().
static void applyBodySize(int sz) {
  if (!pickFaces(sz, s_body, s_bold)) sz = 10;
  s_size = sz;
  s_layoutValid = false;
}

// In-memory line-gap apply: clamp + invalidate the layout cache. Does NOT
// persist — public callers go through setLineGap() / loadSettings().
static void applyLineGap(int gap) {
  if (gap < 0) gap = 0;
  if (gap > 4) gap = 4;
  s_lineGap = gap;
  s_layoutValid = false;
}

void useBody()     { u8g2.setFont(s_body); }
void useBold()     { u8g2.setFont(s_bold); }
void useToast()    { u8g2.setFont(s_toast); }
void useUiSmall()  { u8g2.setFont(s_uiSmall); }
void useUiTiny()   { u8g2.setFont(s_uiTiny); }
void useAppLarge() { u8g2.setFont(u8g2_font_helvB14_te); }

const LayoutMetrics& bodyLayout() {
  if (!s_layoutValid) {
    useBody();
    s_layout.ascent  = u8g2.getFontAscent();
    s_layout.descent = u8g2.getFontDescent();
    s_layout.lineH   = (s_layout.ascent - s_layout.descent) + s_lineGap;

    s_layout.maxWidth = SCREEN_W - (MARGIN_X * 2);

    int maxHeight = SCREEN_H - TOP_PAD - BOT_PAD;

    s_layout.maxLines = maxHeight / s_layout.lineH;
    if (s_layout.maxLines < 1) s_layout.maxLines = 1;
    s_layoutValid = true;
  }
  return s_layout;
}

void loadSettings() {
  applyBodySize(prefs.getInt(kKeyBodySize, 10));
  applyLineGap(prefs.getInt(kKeyLineGap, 0));
  s_bionic = (prefs.getInt(kKeyBionic, 0) != 0);
}

void setBodySize(int sz) {
  applyBodySize(sz);
  prefs.putInt(kKeyBodySize, s_size);  // s_size reflects validation fallback
}

void setLineGap(int gap) {
  applyLineGap(gap);
  prefs.putInt(kKeyLineGap, s_lineGap);  // s_lineGap reflects clamp
}

void setBionic(bool on) {
  if (s_bionic == on) return;
  s_bionic = on;
  // Layout cache is unchanged (per-line height/width-budget don't move), but
  // the on-disk page cache is stamped with the bionic flag so it'll reject
  // any stale entries on the next load — no explicit invalidation needed.
  prefs.putInt(kKeyBionic, on ? 1 : 0);
}

int    currentBodySize() { return s_size; }
int    currentLineGap()  { return s_lineGap; }
bool   bionicEnabled()   { return s_bionic; }

PageCacheLayout layoutForCache() {
  return PageCacheLayout{
    /*bodySize        =*/s_size,
    /*lineGap         =*/s_lineGap,
    /*bionic          =*/(uint8_t)(s_bionic ? 1 : 0),
  };
}

// ----------------------------------------------------------------------------
//  Bionic helpers — UTF-8-aware. The prefix policy is part of the Paginotto
//  typography contract:
//    chars ≤ 3 → 1 strong char; otherwise chars/2 clamped to [1, 4].
//  Punctuation-only words (no ASCII alnum / non-ASCII lead byte) get 0 so
//  we don't bold things like "—" or "...".
// ----------------------------------------------------------------------------

int bionicPrefixBytes(const char* word, int byteLen) {
  return bionicPrefixBytesPure(word, byteLen);
}

// Largest line the paginator emits in one shot — see kLineMax in
// paginator.cpp. Word-level slicing fits in the same envelope.
static constexpr int kBionicScratchMax = 320;

// Walk a body-text line and call `onPart` for each maximal stretch of
// either non-space bytes (one word) or space bytes (one space run). For
// non-space stretches, `isWord` is true. Used by both the measure and the
// draw entry points so the segmentation logic only lives here.
template <typename Fn>
static void forEachLineSegment(const char* line, Fn onPart) {
  if (!line) return;
  int i = 0;
  while (line[i]) {
    int start = i;
    bool space = (line[i] == ' ');
    while (line[i] && ((line[i] == ' ') == space)) i++;
    int len = i - start;
    onPart(line + start, len, !space);
  }
}

// Measure helper — copies bytes [src, src+len) into a NUL-terminated stack
// buffer and returns u8g2.getUTF8Width on it. Caller has already set the
// active font.
static int measureSlice(const char* src, int len) {
  if (len <= 0) return 0;
  if (len >= kBionicScratchMax) len = kBionicScratchMax - 1;
  char scratch[kBionicScratchMax];
  memcpy(scratch, src, (size_t)len);
  scratch[len] = 0;
  return u8g2.getUTF8Width(scratch);
}

// Print helper — same idea, but prints at (x, y).
static void printSlice(int x, int y, const char* src, int len) {
  if (len <= 0) return;
  if (len >= kBionicScratchMax) len = kBionicScratchMax - 1;
  char scratch[kBionicScratchMax];
  memcpy(scratch, src, (size_t)len);
  scratch[len] = 0;
  u8g2.setCursor(x, y);
  u8g2.print(scratch);
}

// Width of one rendered word — measures the bold prefix in the Bold face
// and the tail in the Body face, plus kBionicRestGapPx between them.
// Falls back to a single Body measurement if the word doesn't qualify.
static int measureBionicWord(const char* word, int len) {
  if (len <= 0) return 0;
  int strong = bionicPrefixBytes(word, len);
  if (strong <= 0) {
    useBody();
    return measureSlice(word, len);
  }
  useBold();
  int wStrong = measureSlice(word, strong);
  useBody();
  int wTail = measureSlice(word + strong, len - strong);
  return wStrong + kBionicRestGapPx + wTail;
}

int measureBionicLine(const char* line) {
  if (!line) return 0;
  if (!s_bionic) {
    useBody();
    return u8g2.getUTF8Width(line);
  }
  int total = 0;
  forEachLineSegment(line, [&](const char* seg, int segLen, bool isWord) {
    if (!isWord) {
      useBody();
      total += measureSlice(seg, segLen);
      return;
    }
    total += measureBionicWord(seg, segLen);
  });
  useBody();
  return total;
}

void drawBionicLine(int x, int y, const char* line) {
  if (!line) return;
  if (!s_bionic) {
    useBody();
    u8g2.setCursor(x, y);
    u8g2.print(line);
    return;
  }
  int cursorX = x;
  forEachLineSegment(line, [&](const char* seg, int segLen, bool isWord) {
    if (segLen <= 0) return;
    if (!isWord) {
      useBody();
      printSlice(cursorX, y, seg, segLen);
      cursorX += measureSlice(seg, segLen);
      return;
    }
    int strong = bionicPrefixBytes(seg, segLen);
    if (strong <= 0) {
      useBody();
      printSlice(cursorX, y, seg, segLen);
      cursorX += measureSlice(seg, segLen);
      return;
    }
    useBold();
    printSlice(cursorX, y, seg, strong);
    cursorX += measureSlice(seg, strong);
    cursorX += kBionicRestGapPx;
    useBody();
    printSlice(cursorX, y, seg + strong, segLen - strong);
    cursorX += measureSlice(seg + strong, segLen - strong);
  });
  useBody();
}

void invalidateLayoutCache() { s_layoutValid = false; }

}  // namespace Font
