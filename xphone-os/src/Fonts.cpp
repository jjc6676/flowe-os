// The one and only TU that includes the font data headers.
//
// The headers are generated subsets (tools/gen_epd_font.py) of the Ubuntu
// builtin fonts: ASCII U+0020..U+007E, Latin-1 + Latin Extended-A
// U+00A0..U+017F, plus Greek and Cyrillic subsets.
// Codepoints outside the generated intervals — notably General Punctuation
// U+2000..U+206F (dashes, ellipsis, curly quotes) —
// have no glyph and render as '?' via the Gfx findGlyph fallback. Only the
// Bitmaps/Glyphs/Intervals arrays are emitted; the originals' kern/ligature
// tables were unreferenced (gc-sections discarded them) and are gone.
//
// lineAdvance/ascender come from the original headers' (discarded)
// EpdFontData initializers: both ubuntu_12 styles are {advanceY=29,
// ascender=24, descender=-5}.

#include "Fonts.h"

#include "fonts/ubuntu_10_regular_ascii.h"
#include "fonts/ubuntu_12_bold_ascii.h"
#include "fonts/ubuntu_12_regular_ascii.h"
#include "fonts/ubuntu_22_bold_text.h"
#include "fonts/ubuntu_64_bold_digits.h"

const XpFont kFontRegular = {
    ubuntu_12_regularBitmaps,
    ubuntu_12_regularGlyphs,
    ubuntu_12_regularIntervals,
    sizeof(ubuntu_12_regularIntervals) / sizeof(ubuntu_12_regularIntervals[0]),
    29,  // advanceY
    24,  // ascender
};

const XpFont kFontBold = {
    ubuntu_12_boldBitmaps,
    ubuntu_12_boldGlyphs,
    ubuntu_12_boldIntervals,
    sizeof(ubuntu_12_boldIntervals) / sizeof(ubuntu_12_boldIntervals[0]),
    29,  // advanceY
    24,  // ascender
};

// M3 soft-key bar labels. Metrics from the original ubuntu_10_regular.h
// EpdFontData initializer (x4-os builtinFonts): advanceY=24, ascender=20.
const XpFont kFontSmall = {
    ubuntu_10_regularBitmaps,
    ubuntu_10_regularGlyphs,
    ubuntu_10_regularIntervals,
    sizeof(ubuntu_10_regularIntervals) / sizeof(ubuntu_10_regularIntervals[0]),
    24,  // advanceY
    20,  // ascender
};

// Big numerals for counters. Metrics from FreeType at 150 dpi, the same
// recipe that reproduces the 12pt faces above (advanceY 29 / ascender 24).
const XpFont kFontDigitsXL = {
    ubuntu_64_bold_digitsBitmaps,
    ubuntu_64_bold_digitsGlyphs,
    ubuntu_64_bold_digitsIntervals,
    sizeof(ubuntu_64_bold_digitsIntervals) / sizeof(ubuntu_64_bold_digitsIntervals[0]),
    153,  // advanceY
    125,  // ascender
};


const XpFont kFontSpeed = {
    ubuntu_22_bold_textBitmaps,
    ubuntu_22_bold_textGlyphs,
    ubuntu_22_bold_textIntervals,
    sizeof(ubuntu_22_bold_textIntervals) / sizeof(ubuntu_22_bold_textIntervals[0]),
    53,  // advanceY
    43,  // ascender
};
