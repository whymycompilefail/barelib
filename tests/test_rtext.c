/**********************************************************************************************
*
*   test_rtext.c - Unit tests for rtext (string utility functions)
*
*   Tests pure string manipulation functions — no windowing or GPU needed.
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

static void TestTextCopy(void)
{
    TEST_SECTION("TextCopy");

    char dst[64] = {0};

    // Basic copy
    int n = TextCopy(dst, "hello");
    TEST("TextCopy returns 5", n == 5);
    TEST("TextCopy dst is 'hello'", strcmp(dst, "hello") == 0);

    // Empty source
    n = TextCopy(dst, "");
    TEST("TextCopy empty returns 0", n == 0);
    TEST("TextCopy empty dst is ''", strcmp(dst, "") == 0);

    // NULL source
    n = TextCopy(dst, NULL);
    TEST("TextCopy NULL returns 0", n == 0);
}

static void TestTextIsEqual(void)
{
    TEST_SECTION("TextIsEqual");

    TEST("TextIsEqual same strings", TextIsEqual("abc", "abc"));
    TEST("TextIsEqual different strings", !TextIsEqual("abc", "def"));
    TEST("TextIsEqual case sensitive", !TextIsEqual("ABC", "abc"));
    TEST("TextIsEqual both empty", TextIsEqual("", ""));
    TEST("TextIsEqual one empty", !TextIsEqual("", "a"));
    TEST("TextIsEqual both NULL is false", !TextIsEqual(NULL, NULL));
    TEST("TextIsEqual one NULL", !TextIsEqual(NULL, "a"));
}

static void TestTextLength(void)
{
    TEST_SECTION("TextLength");

    TEST("TextLength 'hello' = 5", TextLength("hello") == 5);
    TEST("TextLength '' = 0", TextLength("") == 0);
    TEST("TextLength NULL = 0", TextLength(NULL) == 0);
    TEST("TextLength with spaces", TextLength("a b") == 3);
}

static void TestTextSubtext(void)
{
    TEST_SECTION("TextSubtext");

    const char *s = TextSubtext("Hello World", 0, 5);
    TEST("TextSubtext(0,5) = 'Hello'", strcmp(s, "Hello") == 0);

    s = TextSubtext("Hello World", 6, 5);
    TEST("TextSubtext(6,5) = 'World'", strcmp(s, "World") == 0);

    // Position beyond length
    s = TextSubtext("Hi", 10, 5);
    TEST("TextSubtext beyond position returns empty", strcmp(s, "") == 0);

    // Length beyond available
    s = TextSubtext("Hi", 0, 10);
    TEST("TextSubtext clamped length", strcmp(s, "Hi") == 0);

    // Zero length
    s = TextSubtext("Hi", 1, 0);
    TEST("TextSubtext zero length", strcmp(s, "") == 0);

    // NULL text
    s = TextSubtext(NULL, 0, 5);
    TEST("TextSubtext NULL returns empty", strcmp(s, "") == 0);
}

static void TestTextRemoveSpaces(void)
{
    TEST_SECTION("TextRemoveSpaces");

    const char *s = TextRemoveSpaces("hello world");
    TEST("TextRemoveSpaces removes space", strcmp(s, "helloworld") == 0);

    s = TextRemoveSpaces("  leading");
    TEST("TextRemoveSpaces leading spaces", strcmp(s, "leading") == 0);

    s = TextRemoveSpaces("trailing  ");
    TEST("TextRemoveSpaces trailing spaces", strcmp(s, "trailing") == 0);

    s = TextRemoveSpaces("a  b  c");
    TEST("TextRemoveSpaces multiple spaces", strcmp(s, "abc") == 0);

    s = TextRemoveSpaces("");
    TEST("TextRemoveSpaces empty", strcmp(s, "") == 0);

    s = TextRemoveSpaces(NULL);
    TEST("TextRemoveSpaces NULL", strcmp(s, "") == 0);
}

static void TestTextReplace(void)
{
    TEST_SECTION("TextReplace");

    // Single replacement
    char *s = TextReplace("hello world", "world", "there");
    TEST("TextReplace 'world'->'there'", strcmp(s, "hello there") == 0);

    // Multiple replacements
    s = TextReplace("a b a b", "a", "x");
    TEST("TextReplace multiple", strcmp(s, "x b x b") == 0);

    // No match
    s = TextReplace("hello", "xyz", "abc");
    TEST("TextReplace no match", strcmp(s, "hello") == 0);

    // NULL replacement (treated as "")
    s = TextReplace("hello", "h", NULL);
    TEST("TextReplace NULL replacement", strcmp(s, "ello") == 0);

    // Empty search (no-op, returns empty buffer)
    s = TextReplace("hello", "", "x");
    TEST("TextReplace empty search returns empty", strcmp(s, "") == 0);
}

static void TestTextReplaceBetween(void)
{
    TEST_SECTION("TextReplaceBetween");

    // NOTE: Implementation keeps the begin/end markers in output
    char *s = TextReplaceBetween("hello [world] here", "[", "]", "there");
    TEST("TextReplaceBetween basic", strcmp(s, "hello [there] here") == 0);

    // No begin marker (returns empty)
    s = TextReplaceBetween("hello world", "{", "}", "xyz");
    TEST("TextReplaceBetween no begin marker returns empty", strcmp(s, "") == 0);

    // NULL text
    s = TextReplaceBetween(NULL, "[", "]", "x");
    TEST("TextReplaceBetween NULL text", s == NULL || strcmp(s, "") == 0);
}

static void TestTextInsert(void)
{
    TEST_SECTION("TextInsert");

    // Insert at middle
    char *s = TextInsert("Helorld", "lo W", 3);
    TEST("TextInsert middle", strcmp(s, "Hello World") == 0);

    // Insert at beginning
    s = TextInsert("world", "hello ", 0);
    TEST("TextInsert beginning", strcmp(s, "hello world") == 0);

    // Insert at end
    s = TextInsert("hello", " world", 5);
    TEST("TextInsert end", strcmp(s, "hello world") == 0);

    // Insert empty string
    s = TextInsert("hello", "", 0);
    TEST("TextInsert empty insert", strcmp(s, "hello") == 0);
}

static void TestTextJoin(void)
{
    TEST_SECTION("TextJoin");

    char *parts[] = { "a", "b", "c" };
    char *s = TextJoin(parts, 3, ",");
    TEST("TextJoin 3 parts with comma", strcmp(s, "a,b,c") == 0);

    s = TextJoin(parts, 3, "");
    TEST("TextJoin 3 parts no delimiter", strcmp(s, "abc") == 0);

    char *single[] = { "only" };
    s = TextJoin(single, 1, ",");
    TEST("TextJoin single part", strcmp(s, "only") == 0);

    // Empty list
    s = TextJoin(NULL, 0, ",");
    TEST("TextJoin empty list", strcmp(s, "") == 0);
}

static void TestTextSplit(void)
{
    TEST_SECTION("TextSplit");

    int count = 0;
    char **result = TextSplit("a,b,c", ',', &count);
    TEST("TextSplit count = 3", count == 3);
    TEST("TextSplit[0] = 'a'", strcmp(result[0], "a") == 0);
    TEST("TextSplit[1] = 'b'", strcmp(result[1], "b") == 0);
    TEST("TextSplit[2] = 'c'", strcmp(result[2], "c") == 0);

    // No delimiter in string
    result = TextSplit("hello", ',', &count);
    TEST("TextSplit no delimiter count = 1", count == 1);
    TEST("TextSplit no delimiter[0] = 'hello'", strcmp(result[0], "hello") == 0);

    // Empty string (returns a single empty entry)
    result = TextSplit("", ',', &count);
    TEST("TextSplit empty count = 1", count == 1);
    TEST("TextSplit empty[0] = ''", strcmp(result[0], "") == 0);

    // Consecutive delimiters produce empty strings
    result = TextSplit("a,,c", ',', &count);
    TEST("TextSplit consecutive delimiters count = 3", count == 3);
    TEST("TextSplit[0] = 'a'", strcmp(result[0], "a") == 0);
    TEST("TextSplit[1] = ''", strcmp(result[1], "") == 0);
    TEST("TextSplit[2] = 'c'", strcmp(result[2], "c") == 0);

    // NULL text
    result = TextSplit(NULL, ',', &count);
    TEST("TextSplit NULL count = 0", count == 0);
}

static void TestTextToUpper(void)
{
    TEST_SECTION("TextToUpper");

    char *s = TextToUpper("hello");
    TEST("TextToUpper basic", strcmp(s, "HELLO") == 0);

    s = TextToUpper("HELLO");
    TEST("TextToUpper already upper", strcmp(s, "HELLO") == 0);

    s = TextToUpper("HeLLo");
    TEST("TextToUpper mixed", strcmp(s, "HELLO") == 0);

    s = TextToUpper("");
    TEST("TextToUpper empty", strcmp(s, "") == 0);

    s = TextToUpper(NULL);
    TEST("TextToUpper NULL", strcmp(s, "") == 0);
}

static void TestTextToLower(void)
{
    TEST_SECTION("TextToLower");

    char *s = TextToLower("HELLO");
    TEST("TextToLower basic", strcmp(s, "hello") == 0);

    s = TextToLower("hello");
    TEST("TextToLower already lower", strcmp(s, "hello") == 0);

    s = TextToLower("HeLLo");
    TEST("TextToLower mixed", strcmp(s, "hello") == 0);

    s = TextToLower("");
    TEST("TextToLower empty", strcmp(s, "") == 0);

    s = TextToLower(NULL);
    TEST("TextToLower NULL", strcmp(s, "") == 0);
}

static void TestTextToInteger(void)
{
    TEST_SECTION("TextToInteger");

    TEST("TextToInteger '42' = 42", TextToInteger("42") == 42);
    TEST("TextToInteger '-5' = -5", TextToInteger("-5") == -5);
    TEST("TextToInteger '0' = 0", TextToInteger("0") == 0);
    TEST("TextToInteger 'abc' = 0", TextToInteger("abc") == 0);
    TEST("TextToInteger '' = 0", TextToInteger("") == 0);
    TEST("TextToInteger NULL = 0", TextToInteger(NULL) == 0);
}

static void TestGetTextBetween(void)
{
    TEST_SECTION("GetTextBetween");

    char *s = GetTextBetween("hello [world] bye", "[", "]");
    TEST("GetTextBetween basic", strcmp(s, "world") == 0);

    s = GetTextBetween("no markers here", "[", "]");
    TEST("GetTextBetween no markers", strcmp(s, "") == 0);

    s = GetTextBetween("", "[", "]");
    TEST("GetTextBetween empty text", strcmp(s, "") == 0);
}

static void TestTextFormat(void)
{
    TEST_SECTION("TextFormat");

    // Basic formatting
    const char *s = TextFormat("hello %s", "world");
    TEST("TextFormat basic string", strcmp(s, "hello world") == 0);

    // Integer formatting
    s = TextFormat("%d + %d = %d", 1, 2, 3);
    TEST("TextFormat integers", strcmp(s, "1 + 2 = 3") == 0);

    // Float formatting
    s = TextFormat("%.2f", 3.14159f);
    TEST("TextFormat float", strcmp(s, "3.14") == 0);

    // Multiple format specifiers
    s = TextFormat("name=%s, age=%d, score=%.1f", "Alice", 30, 95.5f);
    TEST("TextFormat mixed", strcmp(s, "name=Alice, age=30, score=95.5") == 0);

    // NULL text returns empty buffer
    s = TextFormat(NULL);
    TEST("TextFormat NULL returns empty", strcmp(s, "") == 0);

    // Multiple calls (ring buffer: up to 4 calls before overwrite)
    const char *a = TextFormat("first");
    const char *b = TextFormat("second");
    const char *c = TextFormat("third");
    const char *d = TextFormat("fourth");
    TEST("TextFormat ring buffer 1", strcmp(a, "first") == 0);
    TEST("TextFormat ring buffer 2", strcmp(b, "second") == 0);
    TEST("TextFormat ring buffer 3", strcmp(c, "third") == 0);
    TEST("TextFormat ring buffer 4", strcmp(d, "fourth") == 0);
}

//----------------------------------------------------------------------------------
// Main
//----------------------------------------------------------------------------------

int main(void)
{
    printf("========================================\n");
    printf("  rtext Unit Tests\n");
    printf("========================================\n");

    TestTextCopy();
    TestTextIsEqual();
    TestTextLength();
    TestTextSubtext();
    TestTextRemoveSpaces();
    TestTextReplace();
    TestTextReplaceBetween();
    TestTextInsert();
    TestTextJoin();
    TestTextSplit();
    TestTextToUpper();
    TestTextToLower();
    TestTextToInteger();
    TestGetTextBetween();
    TestTextFormat();

    printf("\n========================================\n");
    printf("  Results: %d passed, %d failed out of %d\n",
           testsPassed, testsFailed, testsPassed + testsFailed);
    printf("========================================\n");

    return testsFailed > 0 ? 1 : 0;
}