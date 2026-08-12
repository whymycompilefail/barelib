/**********************************************************************************************
*
*   test_rtextures_color.c - Unit tests for color manipulation functions
*
*   Tests pure color operations (ColorIsEqual, Fade, ColorToInt, etc.) —
*   no windowing or GPU needed.
*
*   LICENSE: zlib/libpng
*
**********************************************************************************************/

#include "raylib_textures.h"
#include <stdio.h>
#include <math.h>

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

#define FLOAT_EQ(a, b) (fabsf((a) - (b)) < 0.0001f)

//----------------------------------------------------------------------------------
// Tests
//----------------------------------------------------------------------------------

static void TestColorIsEqual(void)
{
    TEST_SECTION("ColorIsEqual");

    Color red = { 230, 41, 55, 255 };
    Color red2 = { 230, 41, 55, 255 };
    Color green = { 0, 228, 48, 255 };

    TEST("same color values", ColorIsEqual(red, red2));
    TEST("different colors", !ColorIsEqual(red, green));
    TEST("self comparison", ColorIsEqual(red, red));
    TEST("black vs white", !ColorIsEqual(BLACK, WHITE));
    TEST("identity", ColorIsEqual(BLACK, BLACK));
}

static void TestFade(void)
{
    TEST_SECTION("Fade");

    Color c = { 255, 128, 64, 255 };

    // Full alpha
    Color faded = Fade(c, 1.0f);
    TEST("Fade(1.0) keeps r", faded.r == 255);
    TEST("Fade(1.0) keeps g", faded.g == 128);
    TEST("Fade(1.0) keeps b", faded.b == 64);
    TEST("Fade(1.0) a=255", faded.a == 255);

    // Half alpha
    faded = Fade(c, 0.5f);
    TEST("Fade(0.5) a=127", faded.a == 127);

    // Zero alpha
    faded = Fade(c, 0.0f);
    TEST("Fade(0.0) a=0", faded.a == 0);

    // Clamp below 0
    faded = Fade(c, -1.0f);
    TEST("Fade(-1.0) a=0", faded.a == 0);

    // Clamp above 1
    faded = Fade(c, 2.0f);
    TEST("Fade(2.0) a=255", faded.a == 255);
}

static void TestColorToInt(void)
{
    TEST_SECTION("ColorToInt");

    Color c = { 0xAA, 0xBB, 0xCC, 0xDD };
    int hex = ColorToInt(c);
    TEST("ColorToInt hex value", (unsigned int)hex == 0xAABBCCDD);

    // Black
    TEST("ColorToInt BLACK", ColorToInt(BLACK) == 0x000000FF);

    // White
    TEST("ColorToInt WHITE", ColorToInt(WHITE) == 0xFFFFFFFF);
}

static void TestColorNormalize(void)
{
    TEST_SECTION("ColorNormalize");

    Color c = { 128, 64, 32, 255 };
    Vector4 n = ColorNormalize(c);

    TEST("ColorNormalize x ~ 0.502", FLOAT_EQ(n.x, 128.0f/255.0f));
    TEST("ColorNormalize y ~ 0.251", FLOAT_EQ(n.y, 64.0f/255.0f));
    TEST("ColorNormalize z ~ 0.125", FLOAT_EQ(n.z, 32.0f/255.0f));
    TEST("ColorNormalize w = 1.0", FLOAT_EQ(n.w, 1.0f));

    // Black
    n = ColorNormalize(BLACK);
    TEST("ColorNormalize BLACK all zero", n.x == 0.0f && n.y == 0.0f && n.z == 0.0f && n.w == 1.0f);

    // White
    n = ColorNormalize(WHITE);
    TEST("ColorNormalize WHITE all 1", FLOAT_EQ(n.x, 1.0f) && FLOAT_EQ(n.y, 1.0f) && FLOAT_EQ(n.z, 1.0f) && FLOAT_EQ(n.w, 1.0f));
}

static void TestColorFromNormalized(void)
{
    TEST_SECTION("ColorFromNormalized");

    Vector4 n = { 0.5f, 0.25f, 0.125f, 1.0f };
    Color c = ColorFromNormalized(n);

    // NOTE: 0.5f*255.0f = 127.5, truncated to 127 (unsigned char cast)
    TEST("ColorFromNormalized r ~ 127", c.r == 127);
    TEST("ColorFromNormalized g ~ 63", c.g == 63);
    TEST("ColorFromNormalized b ~ 31", c.b == 31);
    TEST("ColorFromNormalized a = 255", c.a == 255);

    // Round-trip
    Color orig = { 100, 150, 200, 255 };
    Vector4 norm = ColorNormalize(orig);
    Color back = ColorFromNormalized(norm);
    TEST("Color normalize round-trip r", back.r == 100);
    TEST("Color normalize round-trip g", back.g == 150);
    TEST("Color normalize round-trip b", back.b == 200);
    TEST("Color normalize round-trip a", back.a == 255);
}

static void TestColorToHSV(void)
{
    TEST_SECTION("ColorToHSV");

    // Red (R:230, G:41, B:55) — raylib's RED is not pure
    Vector3 hsv = ColorToHSV(RED);
    TEST("ColorToHSV RED hue ~ 356", fabsf(hsv.x - 356.0f) < 2.0f);
    TEST("ColorToHSV RED sat ~ 0.822", fabsf(hsv.y - 0.822f) < 0.001f);
    TEST("ColorToHSV RED val ~ 0.902", FLOAT_EQ(hsv.z, 230.0f/255.0f));

    // Black
    hsv = ColorToHSV(BLACK);
    TEST("ColorToHSV BLACK sat = 0", FLOAT_EQ(hsv.y, 0.0f));
    TEST("ColorToHSV BLACK val = 0", FLOAT_EQ(hsv.z, 0.0f));

    // White
    hsv = ColorToHSV(WHITE);
    TEST("ColorToHSV WHITE sat = 0", FLOAT_EQ(hsv.y, 0.0f));
    TEST("ColorToHSV WHITE val = 1", FLOAT_EQ(hsv.z, 1.0f));

    // Green (R:0, G:228, B:48) — raylib's GREEN is not pure
    hsv = ColorToHSV(GREEN);
    TEST("ColorToHSV GREEN hue ~ 133", fabsf(hsv.x - 133.0f) < 2.0f);
    TEST("ColorToHSV GREEN sat ~ 1", FLOAT_EQ(hsv.y, 1.0f));
}

static void TestColorFromHSV(void)
{
    TEST_SECTION("ColorFromHSV");

    // Red (hue=0)
    Color c = ColorFromHSV(0.0f, 1.0f, 1.0f);
    TEST("ColorFromHSV(0,1,1) r=255", c.r == 255);
    TEST("ColorFromHSV(0,1,1) g=0", c.g == 0);
    TEST("ColorFromHSV(0,1,1) b=0", c.b == 0);
    TEST("ColorFromHSV(0,1,1) a=255", c.a == 255);

    // Green (hue=120)
    c = ColorFromHSV(120.0f, 1.0f, 1.0f);
    TEST("ColorFromHSV(120,1,1) g=255", c.g == 255);

    // Blue (hue=240)
    c = ColorFromHSV(240.0f, 1.0f, 1.0f);
    TEST("ColorFromHSV(240,1,1) b=255", c.b == 255);

    // Zero saturation -> gray
    c = ColorFromHSV(0.0f, 0.0f, 0.5f);
    TEST("ColorFromHSV(0,0,0.5) r=g=b", c.r == c.g && c.g == c.b);
    TEST("ColorFromHSV(0,0,0.5) a=255", c.a == 255);
}

static void TestColorTint(void)
{
    TEST_SECTION("ColorTint");

    Color white = { 255, 255, 255, 255 };
    Color red = { 255, 0, 0, 255 };

    // Tint white with red = red
    Color tinted = ColorTint(white, red);
    TEST("ColorTint(white, red) r=255", tinted.r == 255);
    TEST("ColorTint(white, red) g=0", tinted.g == 0);
    TEST("ColorTint(white, red) b=0", tinted.b == 0);

    // Tint with black = black
    Color tinted2 = ColorTint(white, BLACK);
    TEST("ColorTint(white, black) is black", tinted2.r == 0 && tinted2.g == 0 && tinted2.b == 0);

    // Tint with white = identity
    Color tinted3 = ColorTint(red, white);
    TEST("ColorTint(red, white) = red", ColorIsEqual(tinted3, red));
}

static void TestColorBrightness(void)
{
    TEST_SECTION("ColorBrightness");

    Color gray = { 128, 128, 128, 255 };

    // Full brightness = white
    Color bright = ColorBrightness(gray, 1.0f);
    TEST("ColorBrightness(128,1.0) r=255", bright.r == 255);

    // No change
    Color same = ColorBrightness(gray, 0.0f);
    TEST("ColorBrightness(128,0.0) unchanged", ColorIsEqual(same, gray));

    // Negative brightness = darker
    Color dark = ColorBrightness(gray, -0.5f);
    TEST("ColorBrightness(128,-0.5) darker", dark.r < 128);

    // Clamp above 1
    Color clamped = ColorBrightness(gray, 2.0f);
    TEST("ColorBrightness(128,2.0) clamped", clamped.r == 255);
}

static void TestColorContrast(void)
{
    TEST_SECTION("ColorContrast");

    Color gray = { 128, 128, 128, 255 };

    // Zero contrast = no change
    Color same = ColorContrast(gray, 0.0f);
    TEST("ColorContrast(128,0.0) unchanged", ColorIsEqual(same, gray));

    // Positive contrast — for gray 128, contrast 0.5 barely changes (128.65 -> 128)
    Color high = ColorContrast(gray, 0.5f);
    TEST("ColorContrast(128,0.5) unchanged or slightly higher", high.r >= 128);

    // Negative contrast
    Color low = ColorContrast(gray, -0.5f);
    TEST("ColorContrast(128,-0.5) r < 128", low.r < 128);

    // Maximum contrast — for gray 128, contrast 1.0 gives ~129
    Color maxContrast = ColorContrast(gray, 1.0f);
    TEST("ColorContrast(128,1.0) r > 128", maxContrast.r > 128);

    // Clamp outside [-1, 1]
    Color clamped = ColorContrast(gray, -2.0f);
    TEST("ColorContrast(128,-2.0) clamped", clamped.r >= 0 && clamped.r < 128);
}

static void TestColorAlpha(void)
{
    TEST_SECTION("ColorAlpha");

    Color c = { 255, 128, 64, 255 };

    // Full alpha
    Color a = ColorAlpha(c, 1.0f);
    TEST("ColorAlpha(1.0) a=255", a.a == 255);
    TEST("ColorAlpha(1.0) keeps r", a.r == 255);

    // Half alpha
    a = ColorAlpha(c, 0.5f);
    TEST("ColorAlpha(0.5) a=127", a.a == 127);

    // Zero alpha
    a = ColorAlpha(c, 0.0f);
    TEST("ColorAlpha(0.0) a=0", a.a == 0);

    // Clamp below 0
    a = ColorAlpha(c, -1.0f);
    TEST("ColorAlpha(-1.0) a=0", a.a == 0);

    // Clamp above 1
    a = ColorAlpha(c, 2.0f);
    TEST("ColorAlpha(2.0) a=255", a.a == 255);
}

static void TestColorAlphaBlend(void)
{
    TEST_SECTION("ColorAlphaBlend");

    // Opaque source over opaque destination
    Color result = ColorAlphaBlend(RED, GREEN, WHITE);
    TEST("ColorAlphaBlend opaque src over dst", !ColorIsEqual(result, RED));

    // Transparent source (alpha=0) = destination
    Color transparent = { 0, 0, 0, 0 };
    result = ColorAlphaBlend(RED, transparent, WHITE);
    TEST("ColorAlphaBlend transparent src = dst", ColorIsEqual(result, RED));

    // Fully opaque source (alpha=255) = source
    Color opaque = { 255, 255, 255, 255 };
    result = ColorAlphaBlend(RED, opaque, WHITE);
    TEST("ColorAlphaBlend opaque src = src", ColorIsEqual(result, opaque));
}

static void TestColorLerp(void)
{
    TEST_SECTION("ColorLerp");

    Color black = { 0, 0, 0, 0 };
    Color white = { 255, 255, 255, 255 };

    // Lerp factor 0 = first color
    Color c = ColorLerp(black, white, 0.0f);
    TEST("ColorLerp(0.0) = black", ColorIsEqual(c, black));

    // Lerp factor 1 = second color
    c = ColorLerp(black, white, 1.0f);
    TEST("ColorLerp(1.0) = white", ColorIsEqual(c, white));

    // Lerp factor 0.5 = midpoint
    c = ColorLerp(black, white, 0.5f);
    TEST("ColorLerp(0.5) r=127", c.r == 127);
    TEST("ColorLerp(0.5) g=127", c.g == 127);
    TEST("ColorLerp(0.5) b=127", c.b == 127);
    TEST("ColorLerp(0.5) a=127", c.a == 127);

    // Clamp factor below 0
    c = ColorLerp(black, white, -1.0f);
    TEST("ColorLerp(-1.0) clamped to black", ColorIsEqual(c, black));

    // Clamp factor above 1
    c = ColorLerp(black, white, 2.0f);
    TEST("ColorLerp(2.0) clamped to white", ColorIsEqual(c, white));
}

static void TestGetColor(void)
{
    TEST_SECTION("GetColor");

    // From hex 0xAABBCCDD
    Color c = GetColor(0xAABBCCDD);
    TEST("GetColor(0xAABBCCDD) r=0xAA", c.r == 0xAA);
    TEST("GetColor(0xAABBCCDD) g=0xBB", c.g == 0xBB);
    TEST("GetColor(0xAABBCCDD) b=0xCC", c.b == 0xCC);
    TEST("GetColor(0xAABBCCDD) a=0xDD", c.a == 0xDD);

    // Black
    c = GetColor(0x000000FF);
    TEST("GetColor(0x000000FF) r=0", c.r == 0 && c.g == 0 && c.b == 0);
    TEST("GetColor(0x000000FF) a=255", c.a == 255);

    // White
    c = GetColor(0xFFFFFFFF);
    TEST("GetColor(0xFFFFFFFF) all 255", c.r == 255 && c.g == 255 && c.b == 255 && c.a == 255);
}

//----------------------------------------------------------------------------------
// Main
//----------------------------------------------------------------------------------

int main(void)
{
    printf("========================================\n");
    printf("  rtextures Color Unit Tests\n");
    printf("========================================\n");

    TestColorIsEqual();
    TestFade();
    TestColorToInt();
    TestColorNormalize();
    TestColorFromNormalized();
    TestColorToHSV();
    TestColorFromHSV();
    TestColorTint();
    TestColorBrightness();
    TestColorContrast();
    TestColorAlpha();
    TestColorAlphaBlend();
    TestColorLerp();
    TestGetColor();

    printf("\n========================================\n");
    printf("  Results: %d passed, %d failed out of %d\n",
           testsPassed, testsFailed, testsPassed + testsFailed);
    printf("========================================\n");

    return testsFailed > 0 ? 1 : 0;
}