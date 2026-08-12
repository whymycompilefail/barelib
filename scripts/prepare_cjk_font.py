"""
Prepare CJK font for embedding into raylib.

Downloads Sarasa Gothic SC Regular, subsets to CJK + ASCII + common symbols,
and generates a C header file for embedding.

Usage: python scripts/prepare_cjk_font.py
"""

import os
import sys
import requests
import io
import py7zr
from fontTools import subset
from fontTools.ttLib import TTFont

# Characters to include
# ASCII printable (0x20-0x7E)
ASCII_CHARS = list(range(0x20, 0x7F))

# CJK Unified Ideographs (U+4E00-U+9FFF) - covers all GB2312
CJK_IDEOGRAPHS = list(range(0x4E00, 0x9FFF + 1))

# CJK Compatibility Ideographs (U+F900-U+FAFF)
CJK_COMPAT = list(range(0xF900, 0xFAFF + 1))

# Full-width forms (U+FF00-U+FFEF)
FULLWIDTH = list(range(0xFF00, 0xFFEF + 1))

# CJK Symbols and Punctuation (U+3000-U+303F) + U+3000 is ideographic space
CJK_SYMBOLS = list(range(0x3000, 0x303F + 1))

# Enclosed CJK (U+3200-U+32FF)
ENCLOSED_CJK = list(range(0x3200, 0x32FF + 1))

# CJK Compatibility (U+3300-U+33FF)
CJK_COMPAT2 = list(range(0x3300, 0x33FF + 1))

# General Punctuation (U+2000-U+206F)
GEN_PUNCT = list(range(0x2000, 0x206F + 1))

# Currency Symbols (U+20A0-U+20CF)
CURRENCY = list(range(0x20A0, 0x20CF + 1))

# Letterlike Symbols (U+2100-U+214F)
LETTERLIKE = list(range(0x2100, 0x214F + 1))

# Arrows (U+2190-U+21FF)
ARROWS = list(range(0x2190, 0x21FF + 1))

# Mathematical Operators (U+2200-U+22FF)
MATH_OPS = list(range(0x2200, 0x22FF + 1))

# Box Drawing (U+2500-U+257F)
BOX_DRAWING = list(range(0x2500, 0x257F + 1))

# Block Elements (U+2580-U+259F)
BLOCK_ELEM = list(range(0x2580, 0x259F + 1))

# Geometric Shapes (U+25A0-U+25FF)
GEOMETRIC = list(range(0x25A0, 0x25FF + 1))

# Miscellaneous Symbols (U+2600-U+26FF)
MISC_SYMBOLS = list(range(0x2600, 0x26FF + 1))

# Dingbats (U+2700-U+27BF)
DINGBATS = list(range(0x2700, 0x27BF + 1))

# CJK Strokes (U+31C0-U+31EF)
CJK_STROKES = list(range(0x31C0, 0x31EF + 1))

# Kangxi Radicals (U+2F00-U+2FDF)
KANGXI = list(range(0x2F00, 0x2FDF + 1))

# Bopomofo (U+3100-U+312F)
BOPOMOFO = list(range(0x3100, 0x312F + 1))

# Hiragana (U+3040-U+309F)
HIRAGANA = list(range(0x3040, 0x309F + 1))

# Katakana (U+30A0-U+30FF)
KATAKANA = list(range(0x30A0, 0x30FF + 1))

# Hangul Compatibility Jamo (U+3130-U+318F)
HANGUL = list(range(0x3130, 0x318F + 1))

# Latin-1 Supplement (U+00A0-U+00FF)
LATIN1 = list(range(0x00A0, 0x0100))

# Build the complete list of codepoints to include
all_codepoints = set()
all_codepoints.update(ASCII_CHARS)
all_codepoints.update(CJK_IDEOGRAPHS)
all_codepoints.update(CJK_COMPAT)
all_codepoints.update(FULLWIDTH)
all_codepoints.update(CJK_SYMBOLS)
all_codepoints.update(ENCLOSED_CJK)
all_codepoints.update(CJK_COMPAT2)
all_codepoints.update(GEN_PUNCT)
all_codepoints.update(CURRENCY)
all_codepoints.update(LETTERLIKE)
all_codepoints.update(ARROWS)
all_codepoints.update(MATH_OPS)
all_codepoints.update(BOX_DRAWING)
all_codepoints.update(BLOCK_ELEM)
all_codepoints.update(GEOMETRIC)
all_codepoints.update(MISC_SYMBOLS)
all_codepoints.update(DINGBATS)
all_codepoints.update(CJK_STROKES)
all_codepoints.update(KANGXI)
all_codepoints.update(BOPOMOFO)
all_codepoints.update(HIRAGANA)
all_codepoints.update(KATAKANA)
all_codepoints.update(HANGUL)
all_codepoints.update(LATIN1)

# Control chars needed for line breaks
all_codepoints.update([0x000A, 0x000D, 0x0009])

print(f"Total codepoints to include: {len(all_codepoints)}")
print(f"  CJK Ideographs: {len(CJK_IDEOGRAPHS)} (U+4E00-U+9FFF)")

# Sarasa Gothic SC release from Nanjing University mirror (China-friendly)
FONT_VERSION = "1.0.40"
GITHUB_RELEASE = f"https://mirror.nju.edu.cn/github-release/be5invis/Sarasa-Gothic/LatestRelease/SarasaGothicSC-TTF-{FONT_VERSION}.7z"
FONT_7Z = "sarasa-gothic-sc.7z"
FONT_TTF = "sarasa-gothic-sc-regular.ttf"
SUBSET_PATH = "cjk_subset.ttf"
C_HEADER_PATH = os.path.join("src", "cjk_font_data.h")

def download_font():
    """Download the 7z archive from GitHub."""
    if os.path.exists(FONT_TTF):
        print(f"Font already exists at {FONT_TTF}, skipping download")
        return True
    
    if not os.path.exists(FONT_7Z):
        print(f"Downloading from GitHub release...")
        print(f"  URL: {GITHUB_RELEASE}")
        try:
            resp = requests.get(GITHUB_RELEASE, timeout=300, stream=True)
            if resp.status_code == 200:
                total_size = int(resp.headers.get('content-length', 0))
                downloaded = 0
                with open(FONT_7Z, 'wb') as f:
                    for chunk in resp.iter_content(chunk_size=8192):
                        f.write(chunk)
                        downloaded += len(chunk)
                        if total_size > 0:
                            pct = downloaded * 100 // total_size
                            print(f"  Downloading: {pct}% ({downloaded // 1024 // 1024}MB / {total_size // 1024 // 1024}MB)", end='\r')
                print(f"\nDownloaded to {FONT_7Z} ({os.path.getsize(FONT_7Z) // 1024 // 1024}MB)")
            else:
                print(f"  Failed: HTTP {resp.status_code}")
                return False
        except Exception as e:
            print(f"  Error: {e}")
            return False
    
    # Extract the regular TTF from the 7z archive
    print(f"Extracting from 7z archive...")
    try:
        with py7zr.SevenZipFile(FONT_7Z, mode='r') as z:
            # List all files in archive
            all_files = z.getnames()
            print(f"  Files in archive: {len(all_files)}")
            
            # Find the regular TTF
            target_name = None
            for fname in all_files:
                if 'regular' in fname.lower() and fname.endswith('.ttf'):
                    target_name = fname
                    break
            
            if target_name is None:
                # Fallback: find any TTF that doesn't have bold/italic/light
                for fname in all_files:
                    if fname.endswith('.ttf') and 'bold' not in fname.lower() and 'italic' not in fname.lower() and 'light' not in fname.lower() and 'semibold' not in fname.lower() and 'extralight' not in fname.lower():
                        target_name = fname
                        break
            
            if target_name is None:
                # Fallback: just use the first TTF
                for fname in all_files:
                    if fname.endswith('.ttf'):
                        target_name = fname
                        break
            
            if target_name:
                print(f"  Extracting: {target_name}")
                z.extract(targets=[target_name])
                # Move the extracted file to the expected location
                os.rename(target_name, FONT_TTF)
                print(f"  Extracted to {FONT_TTF}")
            else:
                print("  No TTF file found in archive!")
                return False
    except Exception as e:
        print(f"  Extraction error: {e}")
        return False
    finally:
        # Clean up the 7z archive
        if os.path.exists(FONT_7Z):
            os.remove(FONT_7Z)
    
    return True

def subset_font():
    """Subset the font to include only the specified codepoints."""
    print(f"Subsetting font to {len(all_codepoints)} codepoints...")
    
    codepoints_sorted = sorted(all_codepoints)
    
    options = subset.Options()
    options.flavor = None  # Keep as TTF
    options.ignore_missing_glyphs = True  # Don't fail on missing glyphs
    
    # Create the subsetter
    s = subset.Subsetter(options=options)
    s.populate(unicodes=codepoints_sorted)
    
    # Load the font
    font = TTFont(FONT_TTF)
    
    # Subset
    s.subset(font)
    
    # Save
    font.save(SUBSET_PATH)
    font.close()
    
    size_kb = os.path.getsize(SUBSET_PATH) // 1024
    print(f"Subsetted font saved to {SUBSET_PATH} ({size_kb}KB)")
    return True

def generate_c_header():
    """Generate C header file with the font data embedded."""
    print(f"Generating C header: {C_HEADER_PATH}")
    
    with open(SUBSET_PATH, 'rb') as f:
        font_data = f.read()
    
    data_len = len(font_data)
    
    with open(C_HEADER_PATH, 'w', encoding='utf-8') as f:
        f.write("""/**********************************************************************************************
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
**********************************************************************************************/
""")
        
        f.write("#ifndef CJK_FONT_DATA_H\n")
        f.write("#define CJK_FONT_DATA_H\n\n")
        f.write(f"#define CJK_FONT_DATA_SIZE {data_len}\n\n")
        f.write("static const unsigned char cjkFontData[CJK_FONT_DATA_SIZE] = {\n")
        
        # Write the data as a C array, 12 bytes per line
        for i in range(0, data_len, 12):
            chunk = font_data[i:i+12]
            hex_bytes = ', '.join(f'0x{b:02x}' for b in chunk)
            f.write(f"    {hex_bytes},\n")
        
        f.write("};\n\n")
        f.write("#endif // CJK_FONT_DATA_H\n")
    
    size_kb = data_len // 1024
    print(f"C header generated: {C_HEADER_PATH} ({size_kb}KB)")

def main():
    # Change to project root
    os.chdir(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
    
    if not download_font():
        sys.exit(1)
    
    if not subset_font():
        sys.exit(1)
    
    generate_c_header()
    
    # Cleanup temp files
    for f in [FONT_TTF, SUBSET_PATH]:
        if os.path.exists(f):
            os.remove(f)
    
    print("\nDone! CJK font data embedded in src/cjk_font_data.h")

if __name__ == "__main__":
    main()