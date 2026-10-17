#ifndef PALA_LANG_H
#define PALA_LANG_H

// ============================================================================
//  String table — included transitively via src/config.h, so every
//  translation unit sees the D_* string-literal macros without an extra
//  per-file include.
//
//  All D_* values are plain string-literal #defines. Adjacent string-literal
//  concatenation is exploited at call sites, e.g.
//    out += "<h2>" D_WEB_BOOKS_HEADING "</h2>";
//  The compiler folds these into a single literal — zero runtime cost.
//
//  Authoring rules:
//    1. Preserve %u / %d / %s placeholders when editing messages.
//    2. HTML entities (&middot; &mdash; &#10003; &#9888; &times; &#8592;)
//       and tags (<b> </b>) survive intact across translations.
//    3. D_WEB_CONFIRM_* strings are inlined inside JS confirm('...') calls in
//       HTML onclick attributes. They MUST NOT contain a single quote (') or
//       backslash (\) — either breaks attribute parsing and the destructive
//       form submits without prompting the user. If a translation needs a
//       contraction, rephrase or use a different punctuation mark.
// ============================================================================

#include "src/lang/strings.h"

#endif  // PALA_LANG_H
