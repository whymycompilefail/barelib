/**********************************************************************************************
*
*   test_rtext_font.c - Unit tests for rtext font loading and glyph functions
*
*   Tests font loading, glyph info, and font validation functions that can be
*   tested without a GPU or window (pure CPU operations).
*
*   LICENSE: zlib/libpng
*
**********************************************************************************************/

#include "raylib.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

//----------------------------------------------------------------------------------
// Test Framework Macros
//----------------------------------------------------------------------------------

static int testsPassed = 0;
static int testsFailed = 0;

#define TEST_SECTION(name) \
    do { \
        printf("\n=== %s ===\n", name); \
    } while(0)

#define TEST(name, condition) \
    do { \
        if (!(condition)) { \
            printf("  FAIL: %s\n", name); \
            testsFailed++; \
        } else { \
            printf("  PASS: %s\n", name); \
            testsPassed++; \
        } \
    } while(0)

//----------------------------------------------------------------------------------
// Tests
//----------------------------------------------------------------------------------

static void TestIsFontValid(void)
{
    TEST_SECTION("IsFontValid");

    // Zeroed font should be invalid
    Font f = { 0 };
    TEST("IsFontValid zeroed font", !IsFontValid(f));

    // Partially filled font (only baseSize) should be invalid
    Font f2 = { 0 };
    f2.baseSize = 12;
    TEST("IsFontValid no glyphs", !IsFontValid(f2));

    // Partially filled font (only glyphCount) should be invalid
    Font f3 = { 0 };
    f3.glyphCount = 1;
    TEST("IsFontValid no recs/glyphs", !IsFontValid(f3));
}

static void TestLoadFontDataFromFile(void)
{
    TEST_SECTION("LoadFontData + GenImageFontAtlas + UnloadFontData");

    // Load a .ttf file from the examples resources
    const char *fontPath = "examples/text/resources/pixantiqua.ttf";
    int dataSize = 0;
    unsigned char *fileData = LoadFileData(fontPath, &dataSize);

    if (fileData == NULL || dataSize == 0)
    {
        // Try alternate path
        fontPath = "../examples/text/resources/pixantiqua.ttf";
        fileData = LoadFileData(fontPath, &dataSize);
    }

    if (fileData == NULL || dataSize == 0)
    {
        printf("  SKIP: font file not available (pixantiqua.ttf)\n");
        return;
    }

    TEST("LoadFileData loaded font", fileData != NULL && dataSize > 0);

    // Load font data for the ASCII character set
    // Use a small subset: 32-126 (printable ASCII)
    int codepoints[95] = { 0 };
    for (int i = 0; i < 95; i++) codepoints[i] = 32 + i;

    int glyphCount = 0;
    GlyphInfo *glyphs = LoadFontData(fileData, dataSize, 16, codepoints, 95, FONT_DEFAULT, &glyphCount);

    TEST("LoadFontData not NULL", glyphs != NULL);
    TEST("LoadFontData glyphCount > 0", glyphCount > 0);

    if (glyphs != NULL && glyphCount > 0)
    {
        // Generate font atlas
        Rectangle *glyphRecs = NULL;
        Image atlas = GenImageFontAtlas(glyphs, &glyphRecs, glyphCount, 16, 2, 0);

        TEST("GenImageFontAtlas valid", IsImageValid(atlas));
        TEST("GenImageFontAtlas glyphRecs not NULL", glyphRecs != NULL);
        TEST("GenImageFontAtlas atlas width > 0", atlas.width > 0);
        TEST("GenImageFontAtlas atlas height > 0", atlas.height > 0);

        // Test GetGlyphIndex with manual font struct
        Font testFont = { 0 };
        testFont.baseSize = 16;
        testFont.glyphCount = glyphCount;
        testFont.glyphPadding = 2;
        testFont.texture.id = 0;  // No GPU texture, but IsFontValid doesn't check this
        testFont.recs = glyphRecs;
        testFont.glyphs = glyphs;

        TEST("IsFontValid constructed font", IsFontValid(testFont));

        // Test GetGlyphIndex
        int idxA = GetGlyphIndex(testFont, 65);  // 'A'
        TEST("GetGlyphIndex('A') >= 0", idxA >= 0);
        TEST("GetGlyphIndex('A') < glyphCount", idxA < glyphCount);

        int idxSpace = GetGlyphIndex(testFont, 32);  // ' '
        TEST("GetGlyphIndex(' ') >= 0", idxSpace >= 0);

        // Test GetGlyphInfo
        GlyphInfo infoA = GetGlyphInfo(testFont, 65);
        TEST("GetGlyphInfo('A') value != 0", infoA.value != 0);
        // The glyph value should be the codepoint value
        if (infoA.value != 0) {
            TEST("GetGlyphInfo('A') matches codepoint 65", infoA.value == 65);
        }

        // Test GetGlyphAtlasRec
        Rectangle recA = GetGlyphAtlasRec(testFont, 65);
        TEST("GetGlyphAtlasRec('A') width > 0", recA.width > 0);
        TEST("GetGlyphAtlasRec('A') height > 0", recA.height > 0);

        // Test with missing codepoint (should fallback to '?')
        int idxMissing = GetGlyphIndex(testFont, 0x2000);  // EN QUAD, unlikely to be in font
        TEST("GetGlyphIndex(missing) >= 0", idxMissing >= 0);

        GlyphInfo infoMissing = GetGlyphInfo(testFont, 0x2000);
        TEST("GetGlyphInfo(missing) value != 0", infoMissing.value != 0);

        // Clean up
        UnloadImage(atlas);
        RL_FREE(glyphRecs);  // GenImageFontAtlas allocates recs with RL_MALLOC
        UnloadFontData(glyphs, glyphCount);
    }

    UnloadFileData(fileData);
}

static void TestLoadFontDataNullInput(void)
{
    TEST_SECTION("LoadFontData edge cases");

    int glyphCount = 0;
    GlyphInfo *glyphs = LoadFontData(NULL, 0, 16, NULL, 0, FONT_DEFAULT, &glyphCount);
    TEST("LoadFontData NULL input", glyphs == NULL);
}

//----------------------------------------------------------------------------------
// Main
//----------------------------------------------------------------------------------

int main(void)
{
    SetTraceLogLevel(LOG_WARNING);

    printf("========================================\n");
    printf("  rtext Font Loading Tests\n");
    printf("========================================\n");

    TestIsFontValid();
    TestLoadFontDataFromFile();
    TestLoadFontDataNullInput();

    SetTraceLogLevel(LOG_INFO);

    printf("\n========================================\n");
    printf("  Results: %d passed, %d failed out of %d\n",
           testsPassed, testsFailed, testsPassed + testsFailed);
    printf("========================================\n");

    return testsFailed > 0 ? 1 : 0;
}