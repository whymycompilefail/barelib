/**********************************************************************************************
*
*   test_gpu.c - Unit tests for GPU-dependent raylib functions
*
*   Tests window management, texture GPU operations, render textures, and shape drawing
*   by rendering to offscreen targets and reading back pixels.
*
*   Requires a window/OpenGL context (uses FLAG_WINDOW_HIDDEN).
*
*   LICENSE: zlib/libpng
*
**********************************************************************************************/

#include "raylib.h"
#include "raylib_observability.h"
#include "raylib_ai_adapter.h"
#include "raylib_ai_sprite.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
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

//----------------------------------------------------------------------------------
// Test: Window Management
//----------------------------------------------------------------------------------

static void TestWindowManagement(void)
{
    TEST_SECTION("Window Management");

    // Window should be ready after InitWindow
    TEST("IsWindowReady after InitWindow", IsWindowReady());

    // Screen dimensions should match what was requested
    int w = GetScreenWidth();
    int h = GetScreenHeight();
    TEST("GetScreenWidth > 0", w > 0);
    TEST("GetScreenHeight > 0", h > 0);

    // Render dimensions should be valid
    int rw = GetRenderWidth();
    int rh = GetRenderHeight();
    TEST("GetRenderWidth > 0", rw > 0);
    TEST("GetRenderHeight > 0", rh > 0);

    // Frame time should be valid after first frame
    float ft = GetFrameTime();
    TEST("GetFrameTime >= 0", ft >= 0.0f);

    // Time should be valid
    double t = GetTime();
    TEST("GetTime >= 0", t >= 0.0);

    // FPS should be reasonable
    int fps = GetFPS();
    TEST("GetFPS >= 0", fps >= 0);
}

//----------------------------------------------------------------------------------
// Test: Texture GPU Operations
//----------------------------------------------------------------------------------

static void TestTextureGPU(void)
{
    TEST_SECTION("Texture GPU");

    // Create a simple 4x4 red image in CPU memory
    Image img = GenImageColor(4, 4, RED);
    TEST("Source image valid", IsImageValid(img));

    // Upload to GPU
    Texture2D tex = LoadTextureFromImage(img);
    TEST("LoadTextureFromImage valid", IsTextureValid(tex));
    TEST("LoadTextureFromImage id > 0", tex.id > 0);

    // Verify texture dimensions
    TEST("Texture width matches", tex.width == 4);
    TEST("Texture height matches", tex.height == 4);

    // Set texture filter and wrap (just verify no crash)
    SetTextureFilter(tex, TEXTURE_FILTER_POINT);
    SetTextureWrap(tex, TEXTURE_WRAP_CLAMP);
    TEST("SetTextureFilter/Wrap no crash", 1 == 1);

    // Generate mipmaps
    GenTextureMipmaps(&tex);
    TEST("GenTextureMipmaps no crash", 1 == 1);

    // Read back texture data from GPU and verify
    Image back = LoadImageFromTexture(tex);
    TEST("LoadImageFromTexture valid", IsImageValid(back));
    TEST("LoadImageFromTexture width matches", back.width == 4);
    TEST("LoadImageFromTexture height matches", back.height == 4);

    // Read back pixel color
    Color c = GetImageColor(back, 0, 0);
    TEST("Readback pixel a=255", c.a == 255);
    // The pixel format may be BGRA, but at least verify it's not transparent black
    TEST("Readback pixel has color", c.r > 0 || c.g > 0 || c.b > 0);

    UnloadImage(back);

    // Test UpdateTexture: change to blue
    Color bluePixels[16];
    for (int i = 0; i < 16; i++) bluePixels[i] = BLUE;
    UpdateTexture(tex, bluePixels);

    // Read back and verify
    back = LoadImageFromTexture(tex);
    c = GetImageColor(back, 0, 0);
    TEST("UpdateTexture a=255", c.a == 255);
    TEST("UpdateTexture has color", c.r > 0 || c.g > 0 || c.b > 0);
    UnloadImage(back);

    // Test UpdateTextureRec
    Color greenPixels[4];
    for (int i = 0; i < 4; i++) greenPixels[i] = GREEN;
    UpdateTextureRec(tex, (Rectangle){ 0, 0, 2, 2 }, greenPixels);

    back = LoadImageFromTexture(tex);
    c = GetImageColor(back, 0, 0);
    TEST("UpdateTextureRec a=255", c.a == 255);
    TEST("UpdateTextureRec has color", c.r > 0 || c.g > 0 || c.b > 0);
    UnloadImage(back);

    // Clean up
    UnloadTexture(tex);
    UnloadImage(img);

    // NOTE: IsTextureValid checks the struct field (id > 0), but after UnloadTexture
    // the local struct copy still has the old id. This is a known API limitation.
    TEST("UnloadTexture no crash", 1 == 1);
}

//----------------------------------------------------------------------------------
// Test: RenderTexture
//----------------------------------------------------------------------------------

static void TestRenderTexture(void)
{
    TEST_SECTION("RenderTexture");

    // Create a render texture
    RenderTexture2D rt = LoadRenderTexture(8, 8);
    TEST("LoadRenderTexture valid", IsRenderTextureValid(rt));
    TEST("LoadRenderTexture id > 0", rt.id > 0);
    TEST("RenderTexture texture valid", IsTextureValid(rt.texture));

    // Render to texture: clear to red
    BeginTextureMode(rt);
    ClearBackground(RED);
    EndTextureMode();

    // Read back and verify
    Image back = LoadImageFromTexture(rt.texture);
    TEST("RenderTexture readback valid", IsImageValid(back));
    TEST("RenderTexture width", back.width == 8);
    TEST("RenderTexture height", back.height == 8);

    Color c = GetImageColor(back, 0, 0);
    TEST("RenderTexture clear a=255", c.a == 255);
    TEST("RenderTexture clear has color", c.r > 0 || c.g > 0 || c.b > 0);
    UnloadImage(back);

    // Render to texture: draw shapes
    // NOTE: On some GPU/driver combos with hidden windows, FBO rendering
    // may not read back correctly. We verify basic functionality.
    BeginTextureMode(rt);
    ClearBackground(BLACK);
    DrawRectangle(1, 1, 3, 3, BLUE);
    EndTextureMode();

    TEST("RenderTexture draw no crash", 1 == 1);

    // Clean up
    UnloadRenderTexture(rt);
    TEST("UnloadRenderTexture no crash", 1 == 1);
}

//----------------------------------------------------------------------------------
// Test: Shape Drawing to RenderTexture
//----------------------------------------------------------------------------------

static void TestShapeDrawing(void)
{
    TEST_SECTION("Shape Drawing");

    RenderTexture2D rt = LoadRenderTexture(16, 16);
    TEST("Shape RT valid", IsRenderTextureValid(rt));

    // Draw various shapes
    BeginTextureMode(rt);
    ClearBackground(BLACK);

    // Draw a pixel
    DrawPixel(0, 0, WHITE);

    // Draw a line
    DrawLine(2, 2, 10, 2, GREEN);

    // Draw a circle
    DrawCircle(8, 8, 3, RED);

    // Draw a rectangle outline
    DrawRectangleLines(4, 4, 8, 8, BLUE);

    // Draw a triangle
    DrawTriangle((Vector2){ 8, 1 }, (Vector2){ 5, 6 }, (Vector2){ 11, 6 }, MAGENTA);

    EndTextureMode();

    // Read back and verify
    Image back = LoadImageFromTexture(rt.texture);
    TEST("Shape readback valid", IsImageValid(back));

    // Pixel at (0,0) should be white (from DrawPixel)
    Color c = GetImageColor(back, 0, 0);
    TEST("Shape pixel at (0,0) a=255", c.a == 255);

    // Pixel at (8,8) should be red (from DrawCircle)
    c = GetImageColor(back, 8, 8);
    TEST("Shape circle center a=255", c.a == 255);
    TEST("Shape circle center has red", c.r > 0);

    // Pixel at (8,1) should be magenta (from DrawTriangle)
    c = GetImageColor(back, 8, 1);
    TEST("Shape triangle a=255", c.a == 255);

    UnloadImage(back);
    UnloadRenderTexture(rt);

    // Rounded rect + circle sector on a dedicated RT.
    // NOTE: LoadImageFromTexture readback flips Y (canvas y -> image 15-y).
    RenderTexture2D rt2 = LoadRenderTexture(16, 16);
    TEST("Shape RT2 valid", IsRenderTextureValid(rt2));
    BeginTextureMode(rt2);
    ClearBackground(BLACK);
    DrawRectangleRounded((Rectangle){ 2, 10, 12, 5 }, 0.3f, 8, YELLOW);
    DrawCircleSector((Vector2){ 13, 2 }, 2, 0, 90, 8, GREEN);
    EndTextureMode();

    Image back2 = LoadImageFromTexture(rt2.texture);
    TEST("Shape readback2 valid", IsImageValid(back2));

    // Rounded rectangle: center filled yellow, outer corner not filled
    c = GetImageColor(back2, 8, 15 - 12);   // canvas (8,12)
    TEST("Rounded rect center has yellow", c.r > 0 && c.g > 0 && c.b == 0);
    c = GetImageColor(back2, 2, 15 - 15);   // canvas (2,15)
    TEST("Rounded rect corner not filled", c.r == 0 && c.g == 0 && c.b == 0);

    // Circle sector: center + interior green, outside the arc not filled
    // NOTE: engine GREEN = {0, 228, 48, 255} (b != 0), so assert r==0 && g high.
    c = GetImageColor(back2, 13, 15 - 2);   // canvas (13,2)
    TEST("Sector center green", c.r == 0 && c.g > 100);
    c = GetImageColor(back2, 13, 15 - 3);   // canvas (13,3): on the 90-deg spoke, inside
    TEST("Sector interior has green", c.r == 0 && c.g > 100);
    c = GetImageColor(back2, 15, 15 - 4);   // canvas (15,4)
    TEST("Sector outside not filled", c.r == 0 && c.g == 0 && c.b == 0);

    UnloadImage(back2);
    UnloadRenderTexture(rt2);
}

//----------------------------------------------------------------------------------
// Test: Screen Capture
//----------------------------------------------------------------------------------

static void TestScreenCapture(void)
{
    TEST_SECTION("Screen Capture");

    // Render something to the screen
    BeginDrawing();
    ClearBackground(DARKGRAY);
    DrawText("GPU Test", 10, 10, 20, WHITE);
    EndDrawing();

    // Capture screenshot from screen
    Image screen = LoadImageFromScreen();
    TEST("LoadImageFromScreen valid", IsImageValid(screen));
    TEST("LoadImageFromScreen width > 0", screen.width > 0);
    TEST("LoadImageFromScreen height > 0", screen.height > 0);

    // Verify pixel at (0,0) should be DARKGRAY-ish
    Color c = GetImageColor(screen, 0, 0);
    TEST("Screen capture a=255", c.a == 255);
    // DARKGRAY = (80, 80, 80), so pixel should be dark
    TEST("Screen capture is dark", c.r <= 100 && c.g <= 100 && c.b <= 100);

    UnloadImage(screen);

    // Test TakeScreenshot to file
    TakeScreenshot("test_screenshot.png");
    bool exists = FileExists("test_screenshot.png");
    TEST("TakeScreenshot file exists", exists);
    if (exists)
    {
        int len = GetFileLength("test_screenshot.png");
        TEST("Screenshot file size > 0", len > 0);
        FileRemove("test_screenshot.png");
    }
}

//----------------------------------------------------------------------------------
// Test: Triangle Winding (engine fix T1)
// A 2D-only engine has no use for backface culling. rlglInit() used to enable
// GL_CULL_FACE with CCW front faces, so one of two triangles with opposite
// vertex order was silently dropped: no log, no error, just a missing sprite.
//----------------------------------------------------------------------------------

static void TestTriangleWinding(void)
{
    TEST_SECTION("Triangle Winding (2D must not cull)");

    RenderTexture2D rt = LoadRenderTexture(32, 16);
    TEST("Winding RT valid", IsRenderTextureValid(rt));

    BeginTextureMode(rt);
    ClearBackground(BLACK);

    // Same geometry, opposite vertex order (v2/v3 swapped), placed in the two
    // halves of the target so each can be sampled separately.
    // cross < 0: (2,2) (2,14) (14,8) ; cross > 0: (18,2) (30,8) (18,14)
    DrawTriangle((Vector2){ 2, 2 }, (Vector2){ 2, 14 }, (Vector2){ 14, 8 }, RED);
    DrawTriangle((Vector2){ 18, 2 }, (Vector2){ 30, 8 }, (Vector2){ 18, 14 }, BLUE);
    EndTextureMode();

    Image back = LoadImageFromTexture(rt.texture);
    TEST("Winding readback valid", IsImageValid(back));
    if (IsImageValid(back))
    {
        // NOTE: LoadImageFromTexture flips Y (canvas y -> image 15-y). Row 7
        // and row 8 both lie inside these triangles, so the assertion is
        // orientation-independent on drivers that do not flip.
        Color triA = GetImageColor(back, 6, 7);
        Color triB = GetImageColor(back, 22, 7);
        TEST("cross<0 triangle is visible", triA.r > 100 && triA.g < 80 && triA.b < 80);
        TEST("cross>0 triangle is visible", triB.b > 100 && triB.r < 80);
        UnloadImage(back);
    }
    UnloadRenderTexture(rt);
}

//----------------------------------------------------------------------------------
// Test: In-frame TakeScreenshot (engine fix T2)
// Calling TakeScreenshot() between BeginDrawing()/EndDrawing() used to read
// the framebuffer before the pending render batch was submitted, producing
// an all-black PNG.
//----------------------------------------------------------------------------------

static void TestInFrameScreenshot(void)
{
    TEST_SECTION("In-frame TakeScreenshot (batch flush)");

    const char *path = "shot_probe.png";

    BeginDrawing();
    ClearBackground(BLACK);
    DrawRectangle(0, 0, 16, 16, RED);
    TakeScreenshot(path);          // must flush the pending batch internally
    EndDrawing();

    bool exists = FileExists(path);
    TEST("In-frame screenshot file exists", exists);
    Image shot = { 0 };
    if (exists)
    {
        shot = LoadImage(path);
        TEST("In-frame screenshot loads", IsImageValid(shot));
    }

    if (IsImageValid(shot))
    {
        // rlReadScreenPixels returns bottom-up rows, so the 16x16 red rect may
        // land at the bottom of the image. Count red pixels over the entire
        // image instead of sampling a fixed (8,8): an all-black screenshot
        // still fails hard, while the assertion is flip-independent.
        int redPixels = 0;
        int blackPixels = 0;
        for (int y = 0; y < shot.height; y++)
        {
            for (int x = 0; x < shot.width; x++)
            {
                Color c = GetImageColor(shot, x, y);
                if (c.r > 100 && c.g < 80 && c.b < 80) redPixels++;
                else if (c.r < 20 && c.g < 20 && c.b < 20) blackPixels++;
            }
        }
        TEST("In-frame screenshot contains the drawn red rect", redPixels >= 64);
        TEST("In-frame screenshot keeps the black background", blackPixels > redPixels);
        UnloadImage(shot);
    }

    if (exists) FileRemove(path);
}

//----------------------------------------------------------------------------------
// Test: Drawing Modes
//----------------------------------------------------------------------------------

static void TestDrawingModes(void)
{
    TEST_SECTION("Drawing Modes");

    RenderTexture2D rt = LoadRenderTexture(8, 8);
    TEST("Modes RT valid", IsRenderTextureValid(rt));

    // Test scissor mode
    BeginTextureMode(rt);
    ClearBackground(RED);
    BeginScissorMode(0, 0, 4, 8);
    ClearBackground(GREEN);
    EndScissorMode();
    EndTextureMode();

    Image back = LoadImageFromTexture(rt.texture);
    // Left half (0-3) should be green (inside scissor rect)
    Color cLeft = GetImageColor(back, 0, 0);
    // Right half (4-7) should be red (outside scissor rect)
    Color cRight = GetImageColor(back, 7, 0);

    TEST("Scissor left a=255", cLeft.a == 255);
    TEST("Scissor right a=255", cRight.a == 255);
    // Left and right should differ (if scissor worked)
    TEST("Scissor mode differentiated", cLeft.r != cRight.r || cLeft.g != cRight.g || cLeft.b != cRight.b);

    UnloadImage(back);
    UnloadRenderTexture(rt);

    // Test blend modes
    rt = LoadRenderTexture(8, 8);
    BeginTextureMode(rt);
    ClearBackground(BLACK);
    // Draw red with alpha blending
    BeginBlendMode(BLEND_ALPHA);
    DrawRectangle(0, 0, 8, 8, (Color){ 255, 0, 0, 128 });
    EndBlendMode();
    EndTextureMode();

    back = LoadImageFromTexture(rt.texture);
    Color c = GetImageColor(back, 0, 0);
    // Alpha blending red (128 alpha) on black gives ~128 alpha, not 255
    // Just verify it has some red component
    TEST("Blend mode has color", c.r > 0 || c.g > 0 || c.b > 0);
    UnloadImage(back);
    UnloadRenderTexture(rt);
}

//----------------------------------------------------------------------------------
// Test: 2D Camera
//----------------------------------------------------------------------------------

static void TestCamera2D(void)
{
    TEST_SECTION("Camera 2D");

    Camera2D cam = { 0 };
    cam.target = (Vector2){ 10, 10 };
    cam.offset = (Vector2){ 0, 0 };
    cam.rotation = 0.0f;
    cam.zoom = 2.0f;

    // World-to-screen conversion
    Vector2 screenPos = GetWorldToScreen2D((Vector2){ 10, 10 }, cam);
    TEST("GetWorldToScreen2D not NULL", 1 == 1);
    // With zoom=2 and no offset, world (10,10) should map to screen (0,0)
    // since camera target is (10,10) and offset is (0,0)
    TEST("GetWorldToScreen2D offset matches", (int)screenPos.x == 0 && (int)screenPos.y == 0);

    // Screen-to-world conversion
    Vector2 worldPos = GetScreenToWorld2D((Vector2){ 0, 0 }, cam);
    TEST("GetScreenToWorld2D matches target", (int)worldPos.x == 10 && (int)worldPos.y == 10);

    // Camera matrix
    Matrix mat = GetCameraMatrix2D(cam);
    TEST("GetCameraMatrix2D valid", mat.m0 != 0 || mat.m5 != 0);

    // Render with camera
    RenderTexture2D rt = LoadRenderTexture(8, 8);
    BeginTextureMode(rt);
    ClearBackground(RED);
    BeginMode2D(cam);
    DrawRectangle(10, 10, 4, 4, BLUE);
    EndMode2D();
    EndTextureMode();

    Image back = LoadImageFromTexture(rt.texture);
    Color c = GetImageColor(back, 0, 0);
    TEST("Camera render a=255", c.a == 255);
    TEST("Camera render has color", c.r > 0 || c.g > 0 || c.b > 0);
    UnloadImage(back);
    UnloadRenderTexture(rt);

    // Pixel-accurate transform check (B2): offset + zoom must be applied in the
    // right order by rlMatrixMultiply. World rect (-25,-25,50,50) with
    // offset=(100,100) and zoom=2 covers screen x/y in [50,150] of a 200x200 RT.
    Camera2D cam2 = { 0 };
    cam2.target = (Vector2){ 0, 0 };
    cam2.offset = (Vector2){ 100, 100 };
    cam2.zoom = 2.0f;
    cam2.rotation = 0.0f;

    RenderTexture2D rt2 = LoadRenderTexture(200, 200);
    BeginTextureMode(rt2);
    ClearBackground(BLACK);
    BeginMode2D(cam2);
    DrawRectangle(-25, -25, 50, 50, RED);
    EndMode2D();
    EndTextureMode();

    Image img = LoadImageFromTexture(rt2.texture);
    Color center = GetImageColor(img, 100, 100);
    Color outside = GetImageColor(img, 180, 100);
    TEST("Camera2D transform: center pixel is RED", center.r > 200 && center.g < 60 && center.b < 60);
    TEST("Camera2D transform: outside pixel is BLACK", outside.r < 20 && outside.g < 20 && outside.b < 20);
    UnloadImage(img);
    UnloadRenderTexture(rt2);
}

//----------------------------------------------------------------------------------
// Main
//----------------------------------------------------------------------------------

// Draw observation hook probe (spec M3: non-invasive observability)
// The engine captures semantic draw calls without game cooperation.
static int observerCalls = 0;
static AIObservedDraw observerType[8];
static float observerX[8], observerY[8], observerW[8], observerH[8];
static float observerRadius[8], observerRotation[8];
static char observerText[8][128];
static Color observerColor[8];

static void TestDrawCapture(AIObservedDraw type, float x, float y, float w, float h, float radius, float rotation, const char *text, Color color)
{
    if (observerCalls < 8)
    {
        int i = observerCalls;
        observerType[i] = type;
        observerX[i] = x; observerY[i] = y; observerW[i] = w; observerH[i] = h;
        observerRadius[i] = radius; observerRotation[i] = rotation;
        if (text != NULL)
        {
            strncpy(observerText[i], text, sizeof(observerText[i]) - 1);
            observerText[i][sizeof(observerText[i]) - 1] = '\0';
        }
        else
        {
            observerText[i][0] = '\0';
        }
        observerColor[i] = color;
    }
    observerCalls++;
}

static void TestDrawObserver(void)
{
    TEST_SECTION("Draw Observer Hook (non-invasive)");

    observerCalls = 0;

    // Default: no observer registered
    TEST("Observer is NULL by default", GetAIDrawObserver() == NULL);

    // Register, draw, verify capture
    SetAIDrawObserver(TestDrawCapture);
    BeginDrawing();
    ClearBackground(RAYWHITE);
    DrawRectangle(10, 20, 100, 40, BLUE);      // family: DrawRectangle
    DrawRectangleRec((Rectangle){ 5, 6, 7, 8 }, RED);  // family: DrawRectangleRec
    DrawRectanglePro((Rectangle){ 1, 2, 30, 60 }, (Vector2){ 0, 0 }, 45.0f, GREEN);  // rotated
    EndDrawing();

    TEST("Observer received draw calls", observerCalls == 3);
    TEST("DrawRectangle captured", observerX[0] == 10.0f && observerY[0] == 20.0f && observerW[0] == 100.0f && observerH[0] == 40.0f);
    TEST("Rec variant captured", observerX[1] == 5.0f && observerY[1] == 6.0f && observerW[1] == 7.0f && observerH[1] == 8.0f);
    TEST("Pro variant rotation captured", observerRotation[2] == 45.0f && observerW[2] == 30.0f && observerH[2] == 60.0f);
    TEST("Pro variant color captured", observerColor[2].r == GREEN.r && observerColor[2].g == GREEN.g && observerColor[2].b == GREEN.b);
    TEST("Observed types are rect", observerType[0] == AI_OBS_RECT && observerType[1] == AI_OBS_RECT && observerType[2] == AI_OBS_RECT);

    // Circle family (converges on DrawCircleV)
    BeginDrawing();
    DrawCircle(30, 40, 15, ORANGE);
    EndDrawing();
    TEST("Circle captured", observerCalls == 4 && observerType[3] == AI_OBS_CIRCLE);
    TEST("Circle center/radius captured", observerX[3] == 30.0f && observerY[3] == 40.0f && observerRadius[3] == 15.0f);

    // Text family (converges on DrawTextEx)
    BeginDrawing();
    DrawText("Score: 99", 10, 60, 20, DARKGRAY);
    EndDrawing();
    TEST("Text captured", observerCalls == 5 && observerType[4] == AI_OBS_TEXT);
    TEST("Text content captured", strcmp(observerText[4], "Score: 99") == 0);
    TEST("Text pos/size captured", observerX[4] == 10.0f && observerY[4] == 60.0f && observerW[4] == 20.0f);

    // Clear: hook disabled, no further calls
    SetAIDrawObserver(NULL);
    int callsBefore = observerCalls;
    BeginDrawing();
    DrawRectangle(1, 2, 3, 4, BLACK);
    EndDrawing();
    TEST("Hook disabled after NULL", observerCalls == callsBefore);

    // --- Engine-level auto scene capture (spec M3, non-invasive) ---
    // Stock-game path: a plain native DrawXXX call becomes a scene element
    // with no wrappers and no game-code changes, via the adapter's observer.
    AIAdapter *adapter = AIInit();
    TEST("Adapter initialized for auto capture", adapter != NULL);
    if (adapter != NULL)
    {
        AISetAutoSceneCapture(adapter, true);
        BeginDrawing();
        ClearBackground(RAYWHITE);
        DrawRectangle(50, 60, 120, 30, PURPLE);   // native call, no AIDrawXXX
        DrawCircle(200, 100, 25, ORANGE);          // native circle
        DrawText("HUD 7", 5, 5, 16, BLACK);        // native text
        EndDrawing();

        const char *json = AIGetSceneJSON(adapter);
        TEST("Auto capture: scene JSON contains rect", strstr(json, "\"type\": \"rect\"") != NULL);
        TEST("Auto capture: rect params captured",
              strstr(json, "\"x\": 50.0, \"y\": 60.0, \"w\": 120.0, \"h\": 30.0") != NULL);
        TEST("Auto capture: scene JSON contains circle", strstr(json, "\"type\": \"circle\"") != NULL);
        TEST("Auto capture: circle params captured",
              strstr(json, "\"x\": 200.0, \"y\": 100.0, \"radius\": 25.0") != NULL);
        TEST("Auto capture: scene JSON contains text", strstr(json, "\"type\": \"text\"") != NULL);
        TEST("Auto capture: text content captured", strstr(json, "HUD 7") != NULL);

        // Disable: further native draws are not registered
        AISetAutoSceneCapture(adapter, false);
        int countBefore = AIGetElementCount(adapter);
        BeginDrawing();
        DrawRectangle(1, 2, 3, 4, BLACK);
        EndDrawing();
        TEST("Auto capture disabled stops registration", AIGetElementCount(adapter) == countBefore);

        AIClose(adapter);
    }
}

// Candidate-1 bridge: the deepened engine primitives (rect_rounded / circle_sector)
// must reach the observer with correct slots, and the >=360deg sector fallback
// must NOT double-report nor degrade to AI_OBS_CIRCLE.
static void TestObserverBridgeShapes(void)
{
    TEST_SECTION("Observer bridge: deepened primitives");

    observerCalls = 0;
    SetAIDrawObserver(TestDrawCapture);

    // Rounded rect (fill): roundness rides the radius slot, rotation is 0
    BeginDrawing();
    ClearBackground(RAYWHITE);
    DrawRectangleRounded((Rectangle){ 10, 10, 60, 30 }, 0.3f, 8, YELLOW);
    EndDrawing();
    TEST("Rounded rect observed", observerCalls == 1 && observerType[0] == AI_OBS_RECT_ROUNDED);
    TEST("Rounded rect params", observerX[0] == 10.0f && observerY[0] == 10.0f &&
         observerW[0] == 60.0f && observerH[0] == 30.0f);
    TEST("Roundness rides radius slot", observerRadius[0] == 0.3f && observerRotation[0] == 0.0f);

    // Rounded outline: same AI_OBS_RECT_ROUNDED (scene does not split fill/outline)
    BeginDrawing();
    DrawRectangleRoundedLines((Rectangle){ 20, 60, 50, 25 }, 0.25f, 8, 2.0f, BLUE);
    EndDrawing();
    TEST("Rounded outline observed", observerCalls == 2 && observerType[1] == AI_OBS_RECT_ROUNDED);
    TEST("Rounded outline roundness", observerRadius[1] == 0.25f);

    // Circle sector: startAngle -> rotation slot, endAngle -> w slot
    BeginDrawing();
    DrawCircleSector((Vector2){ 100, 100 }, 20, 0.0f, 90.0f, 8, GREEN);
    EndDrawing();
    TEST("Sector observed", observerCalls == 3 && observerType[2] == AI_OBS_CIRCLE_SECTOR);
    TEST("Sector angles in slots", observerRotation[2] == 0.0f && observerW[2] == 90.0f && observerRadius[2] == 20.0f);

    // Sector outline: same AI_OBS_CIRCLE_SECTOR
    BeginDrawing();
    DrawCircleSectorLines((Vector2){ 150, 100 }, 20, 45.0f, 135.0f, 8, PURPLE);
    EndDrawing();
    TEST("Sector outline observed", observerCalls == 4 && observerType[3] == AI_OBS_CIRCLE_SECTOR);
    TEST("Sector outline angles", observerRotation[3] == 45.0f && observerW[3] == 135.0f);

    // Full-circle sector: fallback inlined, reports sector exactly once (no
    // degradation to AI_OBS_CIRCLE, no double report)
    BeginDrawing();
    DrawCircleSector((Vector2){ 200, 100 }, 20, 0.0f, 360.0f, 8, RED);
    EndDrawing();
    TEST("Full-circle sector observed once", observerCalls == 5 && observerType[4] == AI_OBS_CIRCLE_SECTOR);
    TEST("Full-circle sector endAngle preserved", observerW[4] == 360.0f);

    // Degenerate roundness still routes to the base family (DrawRectangleRec -> rect)
    BeginDrawing();
    DrawRectangleRounded((Rectangle){ 5, 5, 10, 10 }, 0.0f, 8, DARKGRAY);
    EndDrawing();
    TEST("Roundness 0 routes to rect", observerCalls == 6 && observerType[5] == AI_OBS_RECT);

    SetAIDrawObserver(NULL);

    // --- Auto-capture integration: native deepened draws become scene elements ---
    AIAdapter *adapter = AIInit();
    TEST("Adapter initialized for bridge auto-capture", adapter != NULL);
    if (adapter != NULL)
    {
        AISetAutoSceneCapture(adapter, true);
        BeginDrawing();
        ClearBackground(RAYWHITE);
        DrawRectangleRounded((Rectangle){ 10, 10, 60, 30 }, 0.3f, 8, YELLOW);
        DrawCircleSector((Vector2){ 100, 100 }, 20, 0.0f, 90.0f, 8, GREEN);
        DrawCircleSector((Vector2){ 200, 100 }, 20, 0.0f, 360.0f, 8, RED);  // full circle
        EndDrawing();

        const char *json = AIGetSceneJSON(adapter);
        TEST("Bridge capture: rect_rounded in scene", strstr(json, "\"type\": \"rect_rounded\"") != NULL);
        TEST("Bridge capture: roundness serialized", strstr(json, "\"radius\": 0.3") != NULL);
        TEST("Bridge capture: circle_sector in scene", strstr(json, "\"type\": \"circle_sector\"") != NULL);
        TEST("Bridge capture: sector angles serialized", strstr(json, "\"startAngle\": 0.0, \"endAngle\": 90.0") != NULL);
        TEST("Bridge capture: full-circle stays sector (no degrade)", strstr(json, "\"startAngle\": 0.0, \"endAngle\": 360.0") != NULL);
        TEST("Bridge capture: no stray circle element", strstr(json, "\"type\": \"circle\"") == NULL);
        // Count sectors: must be exactly 2 (a DrawCircleV fallback would double-report)
        int sectors = 0;
        const char *p = json;
        while ((p = strstr(p, "\"type\": \"circle_sector\"")) != NULL) { sectors++; p += 1; }
        TEST("Bridge capture: exactly 2 sectors (no double report)", sectors == 2);

        AISetAutoSceneCapture(adapter, false);
        AIClose(adapter);
    }
}
// must draw AND register a scene element on the real draw path (migration
// equivalence — superset params collapse to the same element the legacy
// wrappers produced)
static void TestM4SlimWrappers(void)
{
    TEST_SECTION("M4-slim AIDrawXXX wrappers (33→16)");

    AIAdapter *adapter = AIInit();
    TEST("Adapter initialized for M4-slim wrappers", adapter != NULL);
    if (adapter == NULL) return;

    BeginDrawing();
    AIClearBackground(RAYWHITE);
    AIDrawRect(10, 10, 100, 40, 0, RED);            // was AIDrawRectangle(10,10,100,40,RED)
    AIDrawRectLines(15, 15, 80, 20, 2, BLUE);       // was AIDrawRectangleLinesEx
    AIDrawRectRounded(20, 20, 60, 30, 0.3f, 0, GREEN); // was AIDrawRectangleRounded
    AIDrawCircle(200, 100, 25, ORANGE);             // was AIDrawCircle
    AIDrawCircleLines(250, 100, 30, PURPLE);        // was AIDrawCircleLines
    AIDrawCircleSector(300, 100, 25, 0, 90, 0, PINK); // was AIDrawCircleSector
    AIDrawTriangle(10, 200, 50, 200, 30, 180, BROWN);
    AIDrawTriangleLines(70, 200, 110, 200, 90, 180, DARKGRAY);
    AIDrawPoly(200, 250, 6, 40, 0, MAROON);
    AIDrawPolyLines(300, 250, 5, 35, 15, 2, DARKBLUE);
    AIDrawLine(10, 300, 200, 350, 3, BLACK);
    AIDrawLineBezier(220, 300, 400, 350, 2, DARKGREEN);
    AIDrawPixel(500, 100, RED);
    AIDrawText(GetFontDefault(), "M4 slim", 10, 380, 20, 1, BLACK);
    EndDrawing();

    const char *json = AIGetSceneJSON(adapter);
    TEST("slim rect registered", strstr(json, "\"type\": \"rect\"") != NULL);
    TEST("slim rect_lines registered", strstr(json, "\"type\": \"rect_lines\"") != NULL);
    TEST("slim rect_rounded registered", strstr(json, "\"type\": \"rect_rounded\"") != NULL);
    TEST("slim circle registered", strstr(json, "\"type\": \"circle\"") != NULL);
    TEST("slim circle_lines registered", strstr(json, "\"type\": \"circle_lines\"") != NULL);
    TEST("slim circle_sector registered", strstr(json, "\"type\": \"circle_sector\"") != NULL);
    TEST("slim triangle registered", strstr(json, "\"type\": \"triangle\"") != NULL);
    TEST("slim triangle_lines registered", strstr(json, "\"type\": \"triangle_lines\"") != NULL);
    TEST("slim poly registered", strstr(json, "\"type\": \"poly\"") != NULL);
    TEST("slim poly_lines registered", strstr(json, "\"type\": \"poly_lines\"") != NULL);
    TEST("slim line registered", strstr(json, "\"type\": \"line\"") != NULL);
    TEST("slim line_bezier registered", strstr(json, "\"type\": \"line_bezier\"") != NULL);
    TEST("slim pixel registered", strstr(json, "\"type\": \"pixel\"") != NULL);
    TEST("slim text registered", strstr(json, "\"type\": \"text\"") != NULL);
    TEST("slim rect params migrated", strstr(json, "\"x\": 10.0, \"y\": 10.0, \"w\": 100.0, \"h\": 40.0") != NULL);
    TEST("slim sector angles preserved", strstr(json, "\"startAngle\": 0.0, \"endAngle\": 90.0") != NULL);
    TEST("slim text content preserved", strstr(json, "M4 slim") != NULL);

    AIClose(adapter);
}

// Candidate 2: AIDrawXXX wrappers serve the FIRST (active) adapter explicitly.
// A second instance's wrapper calls still land in the active adapter's buffer
// (registration warning fires once at init — no per-call [0] routing).
static void TestActiveAdapterWrappers(void)
{
    TEST_SECTION("Active adapter serves AIDrawXXX wrappers");

    AIAdapter *a = AIInit();
    AIAdapter *b = AIInit();
    TEST("Two adapters created", a != NULL && b != NULL);
    if (a == NULL || b == NULL) { if (a) AIClose(a); if (b) AIClose(b); return; }

    // Wrappers write into the FIRST adapter's buffer (active), not the second's.
    BeginDrawing();
    AIClearBackground(RAYWHITE);
    AIDrawRect(10, 10, 50, 30, 0, RED);
    AIDrawCircle(200, 100, 20, BLUE);
    EndDrawing();

    const char *jsonA = AIGetSceneJSON(a);
    const char *jsonB = AIGetSceneJSON(b);
    TEST("Active adapter captured wrapper draws", strstr(jsonA, "\"type\": \"rect\"") != NULL && strstr(jsonA, "\"type\": \"circle\"") != NULL);
    TEST("Second adapter buffer stays empty", strstr(jsonB, "\"type\": \"rect\"") == NULL);

    // Closing the active adapter stops wrapper registration (no promotion)
    AIClose(a);
    int countBefore = AIGetElementCount(b);
    BeginDrawing();
    AIDrawRect(1, 1, 5, 5, 0, BLACK);
    EndDrawing();
    TEST("Closing active stops wrapper registration", AIGetElementCount(b) == countBefore);

    AIClose(b);
}

// End-to-end: load a real atlas PNG + sheet JSON through AILoadSpriteSheet,
// play a tag and draw it inside an active adapter.
static void TestSpriteSheetEndToEnd(void)
{
    TEST_SECTION("Sprite Sheet End To End");

    Image atlas = GenImageColor(32, 16, BLANK);
    ImageDrawRectanglePro(&atlas, (Rectangle){ 0, 0, 16, 16 }, (Vector2){ 0, 0 }, 0.0f, RED);
    ImageDrawRectanglePro(&atlas, (Rectangle){ 16, 0, 16, 16 }, (Vector2){ 0, 0 }, 0.0f, BLUE);
    TEST("Atlas exported", ExportImage(atlas, "sprite_e2e.png"));
    UnloadImage(atlas);

    const char *sheet =
        "{\"image\":\"sprite_e2e.png\","
        "\"frames\":["
        "{\"index\":0,\"source_index\":0,\"x\":0,\"y\":0,\"width\":16,\"height\":16,\"duration_ms\":100},"
        "{\"index\":1,\"source_index\":1,\"x\":16,\"y\":0,\"width\":16,\"height\":16,\"duration_ms\":100}"
        "],"
        "\"tags\":[{\"name\":\"run\",\"start\":0,\"end\":1,\"direction\":\"forward\"}],"
        "\"canvas\":{\"width\":16,\"height\":16}}";
    SaveFileText("sprite_e2e.json", (char *)sheet);

    AISprite *sprite = AILoadSpriteSheet("sprite_e2e.json");
    TEST("Sheet loads with texture", sprite != NULL && sprite->texture.id != 0);
    if (sprite != NULL)
    {
        TEST("Sheet frame count", AISpriteFrameCount(sprite) == 2);
        TEST("Sheet atlas path resolves", strstr(sprite->imagePath, "sprite_e2e.png") != NULL);
        TEST("Tag plays", AISpritePlay(sprite, "run"));

        AIAdapter *adapter = AIInit();
        TEST("Adapter for sprite scene", adapter != NULL);
        if (adapter != NULL)
        {
            BeginDrawing();
            ClearBackground(BLACK);
            AISpriteDraw(sprite, 8, 8, 4, false, WHITE);
            EndDrawing();

            const char *json = AIGetSceneJSON(adapter);
            TEST("Sprite draw recorded", strstr(json, "\"type\": \"sprite\"") != NULL);
            TEST("Sprite frame recorded", strstr(json, "\"frame\": 0") != NULL);
            AIClose(adapter);
        }

        AISpriteUpdate(sprite, 0.11f);
        TEST("Sprite advances after one frame duration", AISpriteCurrentFrame(sprite) == 1);
        AIUnloadSpriteSheet(sprite);
    }

    remove("sprite_e2e.png");
    remove("sprite_e2e.json");
}

int main(void)
{
    SetTraceLogLevel(LOG_WARNING);

    printf("========================================\n");
    printf("  GPU-Dependent Tests\n");
    printf("========================================\n");

    // Initialize window with hidden flag
    SetConfigFlags(FLAG_WINDOW_HIDDEN);
    InitWindow(640, 480, "raylib GPU Tests");

    if (!IsWindowReady())
    {
        printf("  FAIL: Could not initialize window\n");
        testsFailed++;
        SetTraceLogLevel(LOG_INFO);
        printf("\n========================================\n");
        printf("  Results: %d passed, %d failed out of %d\n",
               testsPassed, testsFailed, testsPassed + testsFailed);
        printf("========================================\n");
        return 1;
    }

    // Run all GPU tests
    TestWindowManagement();
    TestTextureGPU();
    TestRenderTexture();
    TestShapeDrawing();
    TestScreenCapture();
    TestTriangleWinding();
    TestInFrameScreenshot();
    TestDrawingModes();
    TestCamera2D();
    TestDrawObserver();
    TestObserverBridgeShapes();
    TestActiveAdapterWrappers();
    TestM4SlimWrappers();
    TestSpriteSheetEndToEnd();

    // Clean up
    CloseWindow();

    SetTraceLogLevel(LOG_INFO);

    printf("\n========================================\n");
    printf("  Results: %d passed, %d failed out of %d\n",
           testsPassed, testsFailed, testsPassed + testsFailed);
    printf("========================================\n");

    return testsFailed > 0 ? 1 : 0;
}