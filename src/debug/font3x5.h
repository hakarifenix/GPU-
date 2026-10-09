// NOVA GPU - Software GPU
// src/debug/font3x5.h - GENERATED FILE, do not edit by hand.
// Regenerate with: python3 tools/gen_font3x5.py
//
// A very small 3x5 uppercase bitmap font (ASCII 32..126, 0 = blank)
// used by the debug overlay so that the HUD is drawn by the software
// rasterizer itself. A glyph is 15 bits: row r (0 = top) occupies bits
// [r * 3, r * 3 + 2], and bit 0 inside a row is the leftmost column.
#pragma once

#include <cstdint>

namespace nova::debug {

inline constexpr int kFontGlyphWidth = 3;
inline constexpr int kFontGlyphHeight = 5;
inline constexpr int kFontFirstChar = 32;
inline constexpr int kFontLastChar = 126;

inline constexpr uint16_t kFont3x5[95] = {
    /*  32  33  34  35  36  37  38  39 */ 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x52A5, 0x0000, 0x0000,
    /*  40  41  42  43  44  45  46  47 */ 0x624E, 0x3923, 0x0000, 0x05D0, 0x0000, 0x01C0, 0x2000, 0x12A4,
    /*  48  49  50  51  52  53  54  55 */ 0x7B6F, 0x749A, 0x73E7, 0x79E7, 0x49ED, 0x79CF, 0x7BCF, 0x4927,
    /*  56  57  58  59  60  61  62  63 */ 0x7BEF, 0x79EF, 0x0410, 0x0000, 0x0000, 0x0E38, 0x0000, 0x0000,
    /*  64  65  66  67  68  69  70  71 */ 0x0000, 0x5BEF, 0x3AEB, 0x624E, 0x3B6B, 0x72CF, 0x12CF, 0x6B4E,
    /*  72  73  74  75  76  77  78  79 */ 0x5BED, 0x7497, 0x7B24, 0x5AED, 0x7249, 0x5BFD, 0x5B6B, 0x7B6F,
    /*  80  81  82  83  84  85  86  87 */ 0x13EF, 0x4F6F, 0x5AEF, 0x388E, 0x2497, 0x7B6D, 0x2B6D, 0x5FED,
    /*  88  89  90  91  92  93  94  95 */ 0x5AAD, 0x25ED, 0x72A7, 0x0000, 0x0000, 0x0000, 0x0000, 0x7000,
    /*  96  97  98  99 100 101 102 103 */ 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    /* 104 105 106 107 108 109 110 111 */ 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    /* 112 113 114 115 116 117 118 119 */ 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    /* 120 121 122 123 124 125 126 */ 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
};

// Returns 0 (blank) for characters with no glyph.
inline uint16_t glyphFor(char c) {
    const int code = static_cast<unsigned char>(c);
    if (code < kFontFirstChar || code > kFontLastChar) return 0;
    return kFont3x5[code - kFontFirstChar];
}

}  // namespace nova::debug
