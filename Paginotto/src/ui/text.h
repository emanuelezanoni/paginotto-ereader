#ifndef PAGINOTTO_UI_TEXT_H
#define PAGINOTTO_UI_TEXT_H

#include "src/config.h"
#include "src/state.h"

// Re-export pure modules so existing callers that include "ui/text.h" keep
// working unchanged.
#include "src/pure/text_util.h"
#include "src/pure/paginator.h"
#include "src/hal/file_stream.h"

// ============================================================================
//  Page-level operations on a book file.
//
//  Three primitives that take an open `File&` + a starting byte offset and
//  do exactly one thing each, all returning the next page's start offset:
//
//    drawPageAt        — render one page to the e-ink at the body font
//    extractPageText   — append one page's text (newline-separated) to `out`
//    nextPageOffset    — compute the next page boundary, no draw, no capture
//
//  All three use the pure paginator with a u8g2 width-measure adapter under
//  the current Font::useBody(). None touch `g_bookview` — they're pure on
//  the file argument.
//
//  Above those, two cross-book helpers:
//
//    pageOffsetForPage     — locate page N in an arbitrary file (uses disk cache)
//    pageOffsetForPage     — locate a page in an arbitrary file
// ============================================================================
uint32_t drawPageAt(File& f, uint32_t startPos);
uint32_t extractPageText(File& f, uint32_t startPos, String& out);
uint32_t nextPageOffset(File& f, uint32_t startPos);

// Find page N's byte offset in `f` (which must be opened to `path`). Reads
// the on-disk page cache for an O(1) start, walks forward via
// `nextPageOffset` if the requested page is past the cached portion.
uint32_t pageOffsetForPage(File& f, const String& path, int page);

#endif  // PAGINOTTO_UI_TEXT_H
