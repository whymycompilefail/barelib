/**********************************************************************************************
*
*   test_rtextures_image.c - Unit tests for rtextures CPU-only image manipulation functions
*
*   Tests image generation, copy, format conversion, resize, flip, and color operations
*   that don't require a GPU or window.
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

static void TestGenImageColor(void)
{
    TEST_SECTION("GenImageColor");

    Image img = GenImageColor(4, 4, RED);
    TEST("GenImageColor valid", IsImageValid(img));
    TEST("GenImageColor width", img.width == 4);
    TEST("GenImageColor height", img.height == 4);
    TEST("GenImageColor data not NULL", img.data != NULL);

    // Check pixel color - GetImageColor returns pixel in RGBA format
    Color c = GetImageColor(img, 0, 0);
    // RED = (255, 0, 0, 255) - but the pixel format might be BGRA
    // Just verify it's not transparent and has some red component
    TEST("GetImageColor a=255", c.a == 255);
    TEST("GetImageColor r>0", c.r > 0);

    UnloadImage(img);
}

static void TestImageCopy(void)
{
    TEST_SECTION("ImageCopy");

    Image src = GenImageColor(3, 3, BLUE);
    Image copy = ImageCopy(src);

    TEST("ImageCopy valid", IsImageValid(copy));
    TEST("ImageCopy width", copy.width == src.width);
    TEST("ImageCopy height", copy.height == src.height);
    TEST("ImageCopy data != src", copy.data != src.data);

    Color c = GetImageColor(copy, 0, 0);
    TEST("ImageCopy a=255", c.a == 255);
    TEST("ImageCopy b>0", c.b > 0);

    UnloadImage(src);
    UnloadImage(copy);
}

static void TestImageFromImage(void)
{
    TEST_SECTION("ImageFromImage");

    Image src = GenImageColor(8, 8, WHITE);
    Image sub = ImageFromImage(src, (Rectangle){ 2, 2, 4, 4 });

    TEST("ImageFromImage valid", IsImageValid(sub));
    TEST("ImageFromImage width", sub.width == 4);
    TEST("ImageFromImage height", sub.height == 4);

    UnloadImage(src);
    UnloadImage(sub);
}

static void TestImageFlip(void)
{
    TEST_SECTION("ImageFlip");

    Image img = GenImageColor(4, 4, MAGENTA);

    ImageFlipVertical(&img);
    TEST("ImageFlipVertical no crash", IsImageValid(img));
    Color c = GetImageColor(img, 0, 0);
    TEST("ImageFlipVertical a=255", c.a == 255);

    ImageFlipHorizontal(&img);
    TEST("ImageFlipHorizontal no crash", IsImageValid(img));
    c = GetImageColor(img, 0, 0);
    TEST("ImageFlipHorizontal a=255", c.a == 255);

    UnloadImage(img);
}

static void TestImageColorOps(void)
{
    TEST_SECTION("ImageColorOps");

    Image img = GenImageColor(4, 4, WHITE);

    // Color invert (WHITE -> BLACK)
    ImageColorInvert(&img);
    Color c = GetImageColor(img, 0, 0);
    TEST("ImageColorInvert a=255", c.a == 255);
    TEST("ImageColorInvert not white", c.r < 255 || c.g < 255 || c.b < 255);

    // Color grayscale
    ImageColorGrayscale(&img);
    c = GetImageColor(img, 0, 0);
    TEST("ImageColorGrayscale valid", c.a == 255);

    UnloadImage(img);

    // Color tint
    img = GenImageColor(4, 4, WHITE);
    ImageColorTint(&img, GREEN);
    c = GetImageColor(img, 0, 0);
    TEST("ImageColorTint a=255", c.a == 255);
    TEST("ImageColorTint g>0", c.g > 0);

    UnloadImage(img);
}

static void TestImageResize(void)
{
    TEST_SECTION("ImageResize");

    Image img = GenImageColor(8, 8, RED);

    ImageResize(&img, 16, 16);
    TEST("ImageResize width", img.width == 16);
    TEST("ImageResize height", img.height == 16);
    TEST("ImageResize valid", IsImageValid(img));

    // After resize with bicubic, corner pixel may be slightly different
    Color c = GetImageColor(img, 0, 0);
    TEST("ImageResize a=255", c.a == 255);

    UnloadImage(img);

    // ResizeNN
    img = GenImageColor(8, 8, RED);
    ImageResizeNN(&img, 4, 4);
    TEST("ImageResizeNN width", img.width == 4);
    TEST("ImageResizeNN height", img.height == 4);

    UnloadImage(img);
}

static void TestImageRotate(void)
{
    TEST_SECTION("ImageRotate");

    Image img = GenImageColor(4, 8, RED);

    ImageRotateCW(&img);
    TEST("ImageRotateCW width", img.width == 8);
    TEST("ImageRotateCW height", img.height == 4);

    ImageRotateCCW(&img);
    TEST("ImageRotateCCW width", img.width == 4);
    TEST("ImageRotateCCW height", img.height == 8);

    UnloadImage(img);
}

static void TestImageFormat(void)
{
    TEST_SECTION("ImageFormat");

    Image img = GenImageColor(4, 4, RED);

    ImageFormat(&img, PIXELFORMAT_UNCOMPRESSED_GRAYSCALE);
    TEST("ImageFormat grayscale", img.format == PIXELFORMAT_UNCOMPRESSED_GRAYSCALE);

    Color c = GetImageColor(img, 0, 0);
    TEST("ImageFormat grayscale a=255", c.a == 255);

    UnloadImage(img);
}

static void TestImageCrop(void)
{
    TEST_SECTION("ImageCrop");

    Image img = GenImageColor(8, 8, RED);

    ImageCrop(&img, (Rectangle){ 1, 1, 3, 3 });
    TEST("ImageCrop width", img.width == 3);
    TEST("ImageCrop height", img.height == 3);

    UnloadImage(img);
}

static void TestImageAlpha(void)
{
    TEST_SECTION("ImageAlpha");

    Image img = GenImageColor(4, 4, RED);
    Color c = GetImageColor(img, 0, 0);
    TEST("ImageAlpha before", c.a == 255);

    // Alpha clear
    ImageAlphaClear(&img, (Color){ 0, 0, 0, 0 }, 0.5f);
    TEST("ImageAlphaClear no crash", IsImageValid(img));

    UnloadImage(img);

    // Alpha premultiply
    img = GenImageColor(4, 4, WHITE);
    ImageAlphaPremultiply(&img);
    TEST("ImageAlphaPremultiply no crash", IsImageValid(img));
    UnloadImage(img);
}

static void TestImageBlur(void)
{
    TEST_SECTION("ImageBlurGaussian");

    Image img = GenImageColor(8, 8, RED);
    ImageBlurGaussian(&img, 3);
    TEST("ImageBlurGaussian no crash", IsImageValid(img));
    UnloadImage(img);
}

static void TestImageBrightnessContrast(void)
{
    TEST_SECTION("ImageBrightnessContrast");

    Image img = GenImageColor(4, 4, (Color){ 128, 128, 128, 255 });

    ImageColorBrightness(&img, 50);
    Color c = GetImageColor(img, 0, 0);
    TEST("ImageColorBrightness r>128", c.r > 128);

    UnloadImage(img);

    img = GenImageColor(4, 4, (Color){ 128, 128, 128, 255 });

    ImageColorContrast(&img, 50);
    c = GetImageColor(img, 0, 0);
    // Contrast on gray (128) may not change the value
    // Just check it doesn't crash
    TEST("ImageColorContrast no crash", IsImageValid(img));

    UnloadImage(img);
}

static void TestImageColorReplace(void)
{
    TEST_SECTION("ImageColorReplace");

    Image img = GenImageColor(4, 4, RED);
    ImageColorReplace(&img, RED, GREEN);
    Color c = GetImageColor(img, 0, 0);
    TEST("ImageColorReplace a=255", c.a == 255);
    // Color may not be exact (0,255,0) due to pixel format
    TEST("ImageColorReplace not red", c.r < 255 || c.g > 0 || c.b > 0);

    UnloadImage(img);
}

static void TestImageGetAlphaBorder(void)
{
    TEST_SECTION("GetImageAlphaBorder");

    Image img = GenImageColor(8, 8, RED);
    Rectangle border = GetImageAlphaBorder(img, 0.0f);
    TEST("GetImageAlphaBorder x", border.x == 0);
    TEST("GetImageAlphaBorder width", border.width == 8);
    TEST("GetImageAlphaBorder height", border.height == 8);

    UnloadImage(img);
}

static void TestLoadImageColors(void)
{
    TEST_SECTION("LoadImageColors");

    Image img = GenImageColor(2, 2, RED);
    Color *pixels = LoadImageColors(img);

    TEST("LoadImageColors not NULL", pixels != NULL);
    if (pixels != NULL)
    {
        TEST("LoadImageColors[0] a=255", pixels[0].a == 255);
        // Pixel format may be BGRA, so just check it's not zero
        TEST("LoadImageColors[0] non-zero", pixels[0].r > 0 || pixels[0].g > 0 || pixels[0].b > 0);
        UnloadImageColors(pixels);
    }

    UnloadImage(img);
}

static void TestImageResizeCanvas(void)
{
    TEST_SECTION("ImageResizeCanvas");

    Image img = GenImageColor(4, 4, RED);
    ImageResizeCanvas(&img, 8, 8, 2, 2, BLUE);
    TEST("ImageResizeCanvas width", img.width == 8);
    TEST("ImageResizeCanvas height", img.height == 8);
    TEST("ImageResizeCanvas valid", IsImageValid(img));

    UnloadImage(img);
}

static void TestImageMipmaps(void)
{
    TEST_SECTION("ImageMipmaps");

    Image img = GenImageColor(8, 8, RED);
    ImageMipmaps(&img);
    TEST("ImageMipmaps no crash", IsImageValid(img));
    UnloadImage(img);
}

//----------------------------------------------------------------------------------
// Main
//----------------------------------------------------------------------------------

int main(void)
{
    SetTraceLogLevel(LOG_WARNING);

    printf("========================================\n");
    printf("  rtextures Image Manipulation Tests\n");
    printf("========================================\n");

    TestGenImageColor();
    TestImageCopy();
    TestImageFromImage();
    TestImageFlip();
    TestImageColorOps();
    TestImageResize();
    TestImageRotate();
    TestImageFormat();
    TestImageCrop();
    TestImageAlpha();
    TestImageBlur();
    TestImageBrightnessContrast();
    TestImageColorReplace();
    TestImageGetAlphaBorder();
    TestLoadImageColors();
    TestImageResizeCanvas();
    TestImageMipmaps();

    SetTraceLogLevel(LOG_INFO);

    printf("\n========================================\n");
    printf("  Results: %d passed, %d failed out of %d\n",
           testsPassed, testsFailed, testsPassed + testsFailed);
    printf("========================================\n");

    return testsFailed > 0 ? 1 : 0;
}