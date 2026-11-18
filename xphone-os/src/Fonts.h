#pragma once

// xphone-os M1 — the two embedded UI fonts (Ubuntu 12pt regular/bold from the
// x4-os builtin EpdFontData headers). Defined in Fonts.cpp — the ONLY
// translation unit that includes the ~400KB font data headers, so the static
// arrays exist exactly once.

#include "Gfx.h"

extern const XpFont kFontRegular;
extern const XpFont kFontBold;
// M3: 10pt regular — soft-key tab labels only (subset like the 12pt pair).
extern const XpFont kFontSmall;
// Big display numerals (digits, '/', space only), rendered at size from the
// TTF so they stay crisp instead of pixel-doubling the 12pt face.
extern const XpFont kFontDigitsXL;  // Ubuntu Bold 64pt
