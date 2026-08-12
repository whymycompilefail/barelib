/**********************************************************************************************
*
*   test_rtext_utf8.c - Unit tests for UTF-8 encoding functions
*
*   Tests CodepointToUTF8, GetCodepoint, GetCodepointNext, GetCodepointPrevious,
*   GetCodepointCount, LoadUTF8, and LoadCodepoints — no windowing needed.
*
*   LICENSE: zlib/libpng
*
**********************************************************************************************/

#include "raylib_text.h"
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

static void TestCodepointToUTF8(void)
{
    TEST_SECTION("CodepointToUTF8");

    int size = 0;
    const char *utf8 = NULL;

    // ASCII (1 byte)
    size = 0;
    utf8 = CodepointToUTF8(0x41, &size); // 'A'
    TEST("CodepointToUTF8 0x41 size=1", size == 1);
    TEST("CodepointToUTF8 0x41 = 'A'", utf8[0] == 'A');
    TEST("CodepointToUTF8 0x41 null-terminated", utf8[1] == '\0');

    // 2-byte: U+00A9 (©)
    size = 0;
    utf8 = CodepointToUTF8(0xA9, &size);
    TEST("CodepointToUTF8 0xA9 size=2", size == 2);
    TEST("CodepointToUTF8 0xA9 byte1 ok", (unsigned char)utf8[0] == 0xC2);
    TEST("CodepointToUTF8 0xA9 byte2 ok", (unsigned char)utf8[1] == 0xA9);

    // 3-byte: U+4E16 (世)
    size = 0;
    utf8 = CodepointToUTF8(0x4E16, &size);
    TEST("CodepointToUTF8 0x4E16 size=3", size == 3);
    TEST("CodepointToUTF8 0x4E16 byte1", (unsigned char)utf8[0] == 0xE4);
    TEST("CodepointToUTF8 0x4E16 byte2", (unsigned char)utf8[1] == 0xB8);
    TEST("CodepointToUTF8 0x4E16 byte3", (unsigned char)utf8[2] == 0x96);

    // 4-byte: U+1F600 (😀)
    size = 0;
    utf8 = CodepointToUTF8(0x1F600, &size);
    TEST("CodepointToUTF8 0x1F600 size=4", size == 4);

    // NULL size parameter should not crash
    CodepointToUTF8(0x41, NULL);
    TEST("CodepointToUTF8 NULL size no crash", 1 == 1);
}

static void TestGetCodepoint(void)
{
    TEST_SECTION("GetCodepoint");

    int size = 0;
    int cp = 0;

    // ASCII
    size = 0;
    cp = GetCodepoint("A", &size);
    TEST("GetCodepoint 'A' = 0x41", cp == 0x41);
    TEST("GetCodepoint 'A' size=1", size == 1);

    // 2-byte: ©
    size = 0;
    cp = GetCodepoint("\xC2\xA9", &size);
    TEST("GetCodepoint © = 0xA9", cp == 0xA9);
    TEST("GetCodepoint © size=2", size == 2);

    // 3-byte: 世
    size = 0;
    cp = GetCodepoint("\xE4\xB8\x96", &size);
    TEST("GetCodepoint 世 = 0x4E16", cp == 0x4E16);
    TEST("GetCodepoint 世 size=3", size == 3);

    // NULL text
    size = 0;
    cp = GetCodepoint(NULL, &size);
    TEST("GetCodepoint NULL = '?'", cp == 0x3F);
    TEST("GetCodepoint NULL size=1", size == 1);
}

static void TestGetCodepointNext(void)
{
    TEST_SECTION("GetCodepointNext");

    int size = 0;
    int cp = 0;

    // ASCII
    size = 0;
    cp = GetCodepointNext("Hello", &size);
    TEST("GetCodepointNext 'H' = 0x48", cp == 0x48);
    TEST("GetCodepointNext 'H' size=1", size == 1);

    // 3-byte in middle of string
    size = 0;
    cp = GetCodepointNext("\xE4\xB8\x96\xE7\x95\x8C", &size); // 世界
    TEST("GetCodepointNext 世 = 0x4E16", cp == 0x4E16);
    TEST("GetCodepointNext 世 size=3", size == 3);

    // NULL text
    size = 0;
    cp = GetCodepointNext(NULL, &size);
    TEST("GetCodepointNext NULL = '?'", cp == 0x3F);
    TEST("GetCodepointNext NULL size=1", size == 1);
}

static void TestGetCodepointPrevious(void)
{
    TEST_SECTION("GetCodepointPrevious");

    int size = 0;
    int cp = 0;

    // Navigate backward from end of "Hello"
    size = 0;
    cp = GetCodepointPrevious("Hello" + 5, &size);
    TEST("GetCodepointPrevious 'o' = 0x6F", cp == 0x6F);
    TEST("GetCodepointPrevious 'o' size=1", size == 1);

    // 3-byte character: navigate backward
    size = 0;
    cp = GetCodepointPrevious("\xE4\xB8\x96\xE7\x95\x8C" + 6, &size); // go back to 界
    TEST("GetCodepointPrevious 界 = 0x754C", cp == 0x754C);
    TEST("GetCodepointPrevious 界 size=3", size == 3);

    // From middle of string, before first byte
    size = 0;
    cp = GetCodepointPrevious("\xE4\xB8\x96\xE7\x95\x8C" + 3, &size); // go back to 世
    TEST("GetCodepointPrevious 世 = 0x4E16", cp == 0x4E16);

    // NULL text
    size = 0;
    cp = GetCodepointPrevious(NULL, &size);
    TEST("GetCodepointPrevious NULL = '?'", cp == 0x3F);
}

static void TestGetCodepointCount(void)
{
    TEST_SECTION("GetCodepointCount");

    int count = 0;

    // ASCII only
    count = GetCodepointCount("Hello");
    TEST("GetCodepointCount 'Hello' = 5", count == 5);

    // Mixed ASCII and multi-byte
    count = GetCodepointCount("Hello\xE4\xB8\x96\xE7\x95\x8C"); // Hello世界
    TEST("GetCodepointCount 'Hello世界' = 7", count == 7);

    // Empty string
    count = GetCodepointCount("");
    TEST("GetCodepointCount '' = 0", count == 0);

    // NULL
    count = GetCodepointCount(NULL);
    TEST("GetCodepointCount NULL = 0", count == 0);
}

static void TestLoadUTF8(void)
{
    TEST_SECTION("LoadUTF8");

    char *text = NULL;

    // Single ASCII codepoint
    {
        int codepoints[] = { 0x48, 0x65, 0x6C, 0x6C, 0x6F }; // Hello
        text = LoadUTF8(codepoints, 5);
        TEST("LoadUTF8 'Hello' allocated", text != NULL);
        if (text != NULL) {
            TEST("LoadUTF8 'Hello'", strcmp(text, "Hello") == 0);
            UnloadUTF8(text);
        }
    }

    // Mixed codepoints
    {
        int mixed[] = { 0x48, 0x4E16, 0x754C }; // H世界
        text = LoadUTF8(mixed, 3);
        TEST("LoadUTF8 'H世界' allocated", text != NULL);
        if (text != NULL) {
            TEST("LoadUTF8 'H世界'", strcmp(text, "H\xE4\xB8\x96\xE7\x95\x8C") == 0);
            UnloadUTF8(text);
        }
    }

    // Empty list
    text = LoadUTF8(NULL, 0);
    TEST("LoadUTF8 NULL list", text == NULL);

    // Single codepoint
    {
        int single[] = { 0x41 }; // A
        text = LoadUTF8(single, 1);
        TEST("LoadUTF8 single 'A' allocated", text != NULL);
        if (text != NULL) {
            TEST("LoadUTF8 single 'A'", strcmp(text, "A") == 0);
            UnloadUTF8(text);
        }
    }
}

static void TestLoadCodepoints(void)
{
    TEST_SECTION("LoadCodepoints");

    int count = 0;
    int *cps = NULL;

    // ASCII
    cps = LoadCodepoints("Hello", &count);
    TEST("LoadCodepoints 'Hello' allocated", cps != NULL);
    if (cps != NULL) {
        TEST("LoadCodepoints 'Hello' count=5", count == 5);
        TEST("LoadCodepoints[0] = 0x48", cps[0] == 0x48);
        TEST("LoadCodepoints[4] = 0x6F", cps[4] == 0x6F);
        UnloadCodepoints(cps);
    }

    // Mixed ASCII and multi-byte
    cps = LoadCodepoints("H\xE4\xB8\x96", &count); // H世
    TEST("LoadCodepoints 'H世' allocated", cps != NULL);
    if (cps != NULL) {
        TEST("LoadCodepoints 'H世' count=2", count == 2);
        TEST("LoadCodepoints[0] = 0x48", cps[0] == 0x48);
        TEST("LoadCodepoints[1] = 0x4E16", cps[1] == 0x4E16);
        UnloadCodepoints(cps);
    }

    // Empty string (malloc(0) may return non-NULL, so only check count)
    cps = LoadCodepoints("", &count);
    TEST("LoadCodepoints '' count=0", count == 0);
    if (cps != NULL) UnloadCodepoints(cps);

    // NULL text
    cps = LoadCodepoints(NULL, &count);
    TEST("LoadCodepoints NULL count=0", count == 0);
    TEST("LoadCodepoints NULL result=NULL", cps == NULL);
}

//----------------------------------------------------------------------------------
// Main
//----------------------------------------------------------------------------------

int main(void)
{
    printf("========================================\n");
    printf("  rtext UTF-8 Unit Tests\n");
    printf("========================================\n");

    TestCodepointToUTF8();
    TestGetCodepoint();
    TestGetCodepointNext();
    TestGetCodepointPrevious();
    TestGetCodepointCount();
    TestLoadUTF8();
    TestLoadCodepoints();

    printf("\n========================================\n");
    printf("  Results: %d passed, %d failed out of %d\n",
           testsPassed, testsFailed, testsPassed + testsFailed);
    printf("========================================\n");

    return testsFailed > 0 ? 1 : 0;
}