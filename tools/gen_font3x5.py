#!/usr/bin/env python3
"""Generates src/debug/font3x5.h (a 3x5 bitmap font) from the ASCII art below.

Glyph layout: 3 columns x 5 rows, '#' = set pixel, '.' = clear. The packed form
puts row r (0 = top) in bits [r*3, r*3+2] with bit 0 = leftmost column, so a
glyph is 15 bits and the table is 95 16-bit entries for ASCII 32..126.

Run from the repository root:  python3 tools/gen_font3x5.py
"""
import os

GLYPHS = {
    ' ': ['...','...','...','...','...'],
    '-': ['...','...','###','...','...'],
    '.': ['...','...','...','...','.#.'],
    ':': ['...','.#.','...','.#.','...'],
    '/': ['..#','..#','.#.','#..','#..'],
    '%': ['#.#','..#','.#.','#..','#.#'],
    '+': ['...','.#.','###','.#.','...'],
    '=': ['...','###','...','###','...'],
    '_': ['...','...','...','...','###'],
    '(': ['.##','#..','#..','#..','.##'],
    ')': ['##.','..#','..#','..#','##.'],
    '0': ['###','#.#','#.#','#.#','###'],
    '1': ['.#.','##.','.#.','.#.','###'],
    '2': ['###','..#','###','#..','###'],
    '3': ['###','..#','###','..#','###'],
    '4': ['#.#','#.#','###','..#','..#'],
    '5': ['###','#..','###','..#','###'],
    '6': ['###','#..','###','#.#','###'],
    '7': ['###','..#','..#','..#','..#'],
    '8': ['###','#.#','###','#.#','###'],
    '9': ['###','#.#','###','..#','###'],
    'A': ['###','#.#','###','#.#','#.#'],
    'B': ['##.','#.#','##.','#.#','##.'],
    'C': ['.##','#..','#..','#..','.##'],
    'D': ['##.','#.#','#.#','#.#','##.'],
    'E': ['###','#..','##.','#..','###'],
    'F': ['###','#..','##.','#..','#..'],
    'G': ['.##','#..','#.#','#.#','.##'],
    'H': ['#.#','#.#','###','#.#','#.#'],
    'I': ['###','.#.','.#.','.#.','###'],
    'J': ['..#','..#','..#','#.#','###'],
    'K': ['#.#','#.#','##.','#.#','#.#'],
    'L': ['#..','#..','#..','#..','###'],
    'M': ['#.#','###','###','#.#','#.#'],
    'N': ['##.','#.#','#.#','#.#','#.#'],
    'O': ['###','#.#','#.#','#.#','###'],
    'P': ['###','#.#','###','#..','#..'],
    'Q': ['###','#.#','#.#','###','..#'],
    'R': ['###','#.#','##.','#.#','#.#'],
    'S': ['.##','#..','.#.','..#','##.'],
    'T': ['###','.#.','.#.','.#.','.#.'],
    'U': ['#.#','#.#','#.#','#.#','###'],
    'V': ['#.#','#.#','#.#','#.#','.#.'],
    'W': ['#.#','#.#','###','###','#.#'],
    'X': ['#.#','#.#','.#.','#.#','#.#'],
    'Y': ['#.#','#.#','###','.#.','.#.'],
    'Z': ['###','..#','.#.','#..','###'],
}


def pack(rows):
    value = 0
    for r, row in enumerate(rows):
        assert len(row) == 3, row
        for c, ch in enumerate(row):
            if ch == '#':
                value |= 1 << (r * 3 + c)
    return value


def main():
    table = [pack(GLYPHS[chr(c)]) if chr(c) in GLYPHS else 0 for c in range(32, 127)]
    lines = []
    lines.append('// NOVA GPU - Software GPU')
    lines.append('// src/debug/font3x5.h - GENERATED FILE, do not edit by hand.')
    lines.append('// Regenerate with: python3 tools/gen_font3x5.py')
    lines.append('//')
    lines.append('// A very small 3x5 uppercase bitmap font (ASCII 32..126, 0 = blank)')
    lines.append('// used by the debug overlay so that the HUD is drawn by the software')
    lines.append('// rasterizer itself. A glyph is 15 bits: row r (0 = top) occupies bits')
    lines.append('// [r * 3, r * 3 + 2], and bit 0 inside a row is the leftmost column.')
    lines.append('#pragma once')
    lines.append('')
    lines.append('#include <cstdint>')
    lines.append('')
    lines.append('namespace nova::debug {')
    lines.append('')
    lines.append('inline constexpr int kFontGlyphWidth = 3;')
    lines.append('inline constexpr int kFontGlyphHeight = 5;')
    lines.append('inline constexpr int kFontFirstChar = 32;')
    lines.append('inline constexpr int kFontLastChar = 126;')
    lines.append('')
    lines.append('inline constexpr uint16_t kFont3x5[95] = {')
    for start in range(0, 95, 8):
        chunk = table[start:start + 8]
        codes = ' '.join(f'{32 + start + i:3d}' for i in range(len(chunk)))
        lines.append(f'    /* {codes} */ ' + ', '.join(f'0x{v:04X}' for v in chunk) + ',')
    lines.append('};')
    lines.append('')
    lines.append('// Returns 0 (blank) for characters with no glyph.')
    lines.append('inline uint16_t glyphFor(char c) {')
    lines.append('    const int code = static_cast<unsigned char>(c);')
    lines.append('    if (code < kFontFirstChar || code > kFontLastChar) return 0;')
    lines.append('    return kFont3x5[code - kFontFirstChar];')
    lines.append('}')
    lines.append('')
    lines.append('}  // namespace nova::debug')
    lines.append('')

    out = os.path.join(os.path.dirname(__file__), '..', 'src', 'debug', 'font3x5.h')
    with open(out, 'w') as f:
        f.write('\n'.join(lines))
    print('wrote', os.path.normpath(out), 'with', len(GLYPHS), 'glyphs')


if __name__ == '__main__':
    main()
