/**********************************************************************************************
*
*   cjk_font_data.h - Embedded CJK font data for raylib default font
*
*   This file is auto-generated from scripts/prepare_cjk_font.py
*   Font: Sarasa Gothic SC Regular (SIL OFL 1.1 license)
*   https://github.com/be5invis/Sarasa-Gothic
*
*   The font is subsetted to include:
*     - ASCII printable characters (0x20-0x7E)
*     - CJK Unified Ideographs (U+4E00-U+9FFF) - covers all GB2312
*     - CJK Symbols and Punctuation (U+3000-U+303F)
*     - Full-width forms (U+FF00-U+FFEF)
*     - Latin-1 Supplement (U+00A0-U+00FF)
*     - Arrows, math operators, box drawing, geometric shapes
*     - Hiragana, Katakana, Hangul, Bopomofo
*     - General punctuation, currency, letterlike symbols
*
*   LICENSE: zlib/libpng (same as raylib)
*   The embedded font is licensed under SIL Open Font License 1.1
*
*   NOTE: The actual font data is compiled from cjk_font_data.c (separate compilation unit).
*   This header only declares the extern reference.
*
**********************************************************************************************/
#ifndef CJK_FONT_DATA_H
#define CJK_FONT_DATA_H

#define CJK_FONT_DATA_SIZE 13283128

extern const unsigned char cjkFontData[CJK_FONT_DATA_SIZE];

#endif // CJK_FONT_DATA_H