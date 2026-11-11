/**********************************************************************************************
*
*   rcore - Window/display management, Graphic device/context management and input management
*
*   PLATFORMS SUPPORTED:
*       > PLATFORM_DESKTOP_GLFW (GLFW backend):
*           - Windows (Win32, Win64)
*           - Linux (X11/Wayland desktop mode)
*           - macOS/OSX (x64, arm64)
*           - FreeBSD, OpenBSD, NetBSD, DragonFly (X11 desktop)
*       > PLATFORM_DESKTOP_SDL (SDL backend):
*           - Windows (Win32, Win64)
*           - Linux (X11/Wayland desktop mode)
*           - Others (not tested)
*       > PLATFORM_DESKTOP_RGFW (RGFW backend):
*           - Windows (Win32, Win64)
*           - Linux (X11/Wayland desktop mode)
*           - macOS/OSX (x64, arm64)
*           - Others (not tested)
*       > PLATFORM_DESKTOP_WIN32 (native Win32):
*           - Windows (Win32, Win64)
*       > PLATFORM_WEB (GLFW + Emscripten):
*           - HTML5 (WebAssembly)
*       > PLATFORM_WEB_EMSCRIPTEN (Emscripten):
*           - HTML5 (WebAssembly)
*       > PLATFORM_WEB_RGFW (Emscripten):
*           - HTML5 (WebAssembly)
*       > PLATFORM_DRM (native DRM):
*           - Raspberry Pi 0-5 (DRM/KMS)
*           - Linux DRM subsystem (KMS mode)
*           - Embedded devices (with GPU)
*       > PLATFORM_MEMORY
*           - Memory framebuffer output, using software renderer, no OS required
*
*   CONFIGURATION:
*       #define SUPPORT_CAMERA_SYSTEM       1
*           Camera module is included (rcamera.h) and multiple predefined cameras are available:
*               free, 1st/3rd person, orbital, custom
*
*       #define SUPPORT_GESTURES_SYSTEM     1
*           Gestures module is included (rgestures.h) to support gestures detection: tap, hold, swipe, drag
*
*       #define SUPPORT_MOUSE_GESTURES      1
*           Mouse gestures are directly mapped like touches and processed by gestures system
*
*       #define SUPPORT_BUSY_WAIT_LOOP      1
*           Use busy wait loop for timing sync, if not defined, a high-resolution timer is setup and used
*
*       #define SUPPORT_PARTIALBUSY_WAIT_LOOP 0
*           Use a partial-busy wait loop, in this case frame sleeps for most of the time and runs a busy-wait-loop at the end
*
*       #define SUPPORT_SCREEN_CAPTURE      1
*           Allow automatic screen capture of current screen pressing F12, defined in KeyCallback()
*
*       #define SUPPORT_COMPRESSION_API     1
*           Support CompressData() and DecompressData() functions, those functions use zlib implementation
*           provided by stb_image and stb_image_write libraries, so, those libraries must be enabled on textures module
*           for linkage
*
*   DEPENDENCIES:
*       raymath  - 3D math functionality (Vector2, Vector3, Matrix, Quaternion)
*       camera   - Multiple 3D camera modes (free, orbital, 1st person, 3rd person)
*       gestures - Gestures system for touch-ready devices (or simulated from mouse inputs)
*
*
*   LICENSE: zlib/libpng
*
*   Copyright (c) 2013-2026 Ramon Santamaria (@raysan5) and contributors
*
*   This software is provided "as-is", without any express or implied warranty. In no event
*   will the authors be held liable for any damages arising from the use of this software.
*
*   Permission is granted to anyone to use this software for any purpose, including commercial
*   applications, and to alter it and redistribute it freely, subject to the following restrictions:
*
*     1. The origin of this software must not be misrepresented; you must not claim that you
*     wrote the original software. If you use this software in a product, an acknowledgment
*     in the product documentation would be appreciated but is not required.
*
*     2. Altered source versions must be plainly marked as such, and must not be misrepresented
*     as being the original software.
*
*     3. This notice may not be removed or altered from any source distribution.
*
**********************************************************************************************/

//----------------------------------------------------------------------------------
// Feature Test Macros required for this module
//----------------------------------------------------------------------------------
#if (defined(__linux__) || defined(PLATFORM_WEB) || defined(PLATFORM_WEB_RGFW)) && (_XOPEN_SOURCE < 500)
    #undef _XOPEN_SOURCE
    #define _XOPEN_SOURCE 500       // Required for: readlink if compiled with c99 without GNU extensions
#endif

#if (defined(__linux__) || defined(PLATFORM_WEB) || defined(PLATFORM_WEB_RGFW)) && (_POSIX_C_SOURCE < 199309L)
    #undef _POSIX_C_SOURCE
    #define _POSIX_C_SOURCE 199309L // Required for: CLOCK_MONOTONIC if compiled with c99 without GNU extensions
#endif

#include "raylib.h"                 // Declares module functions

#include "config.h"                 // Defines module configuration flags

#include "raylib_context.h"         // CoreData, RaylibContext, and platform interface

#include <stdlib.h>                 // Required for: srand(), rand(), exit()
#include <stdio.h>                  // Required for: FILE, fopen(), fseek(), ftell(), fread(), fwrite(), fprintf(), vprintf(), fclose(), sprintf() [Used in OpenURL()]
#include <string.h>                 // Required for: strlen(), strncpy(), strcmp(), strrchr(), memset(), strcat()
#include <stdarg.h>                 // Required for: va_list, va_start(), va_end() [Used in TraceLog()]
#include <time.h>                   // Required for: time() [Used in InitTimer()]
#include <math.h>                   // Required for: math operations

#if defined(PLATFORM_MEMORY) || defined(PLATFORM_WEB)
    #define SW_GL_FRAMEBUFFER_COPY_BGRA false
#endif
#include "rlgl.h"                   // OpenGL abstraction layer to OpenGL 1.1, 3.3+ or ES2

#include "raymath.h"                // Vector2, Vector3, Quaternion and Matrix functionality

#if SUPPORT_GESTURES_SYSTEM
    #define RGESTURES_IMPLEMENTATION
    #include "rgestures.h"          // Gestures detection functionality
#endif

#if SUPPORT_CAMERA_SYSTEM
    #define RCAMERA_IMPLEMENTATION
    #include "rcamera.h"            // Camera system functionality
#endif

#if SUPPORT_COMPRESSION_API
    #define SINFL_IMPLEMENTATION
    #define SINFL_NO_SIMD
    #include "external/sinfl.h"     // Deflate (RFC 1951) decompressor

    #define SDEFL_IMPLEMENTATION
    #include "external/sdefl.h"     // Deflate (RFC 1951) compressor
#endif

#if SUPPORT_RPRAND_GENERATOR
    #define RPRAND_IMPLEMENTATION
    #include "external/rprand.h"
#endif

#if defined(__linux__) && !defined(_GNU_SOURCE)
    #define _GNU_SOURCE
#endif

// Platform specific defines to handle GetApplicationDirectory()
#if defined(_WIN32)
    #if !defined(MAX_PATH)
        #define MAX_PATH 260
    #endif

    struct HINSTANCE__;
    #if defined(__cplusplus)
    extern "C" {
    #endif
    __declspec(dllimport) unsigned long __stdcall GetModuleFileNameA(struct HINSTANCE__ *hModule, char *lpFilename, unsigned long nSize);
    __declspec(dllimport) unsigned long __stdcall GetModuleFileNameW(struct HINSTANCE__ *hModule, wchar_t *lpFilename, unsigned long nSize);
    __declspec(dllimport) int __stdcall WideCharToMultiByte(unsigned int cp, unsigned long flags, const wchar_t *widestr, int cchwide, char *str, int cbmb, const char *defchar, int *used_default);
    __declspec(dllimport) unsigned int __stdcall timeBeginPeriod(unsigned int uPeriod);
    __declspec(dllimport) unsigned int __stdcall timeEndPeriod(unsigned int uPeriod);
    #if defined(__cplusplus)
    }
    #endif
#elif defined(__linux__)
    #include <unistd.h>
#elif defined(__FreeBSD__)
    #include <sys/types.h>
    #include <sys/sysctl.h>
    #include <unistd.h>
#elif defined(__APPLE__)
    #include <sys/syslimits.h>
    #include <mach-o/dyld.h>
#endif



//----------------------------------------------------------------------------------
// Defines and Macros
//----------------------------------------------------------------------------------
#ifndef MAX_TRACELOG_MSG_LENGTH
    #define MAX_TRACELOG_MSG_LENGTH      256        // Max length of one trace-log message
#endif

#ifndef MAX_FILEPATH_CAPACITY
    #define MAX_FILEPATH_CAPACITY       8192        // Maximum capacity for filepath
#endif
#ifndef MAX_FILEPATH_LENGTH
    #if defined(_WIN32)
        #define MAX_FILEPATH_LENGTH      256        // On Win32, MAX_PATH = 260 (limits.h) but Windows 10, Version 1607 enables long paths...
    #else
        #define MAX_FILEPATH_LENGTH     4096        // On Linux, PATH_MAX = 4096 by default (limits.h)
    #endif
#endif

#ifndef MAX_DECOMPRESSION_SIZE
    #define MAX_DECOMPRESSION_SIZE        64        // Maximum size allocated for decompression in MB
#endif

// Flags bitwise operation macros
// NOTE: FLAG_SET, FLAG_CLEAR, FLAG_IS_SET are now defined in platform.h
// (included via raylib_context.h)
#define FLAG_TOGGLE(n, f) ((n) ^= (f))

//----------------------------------------------------------------------------------
// Types and Structures Definition
//----------------------------------------------------------------------------------
// CoreData struct is defined in platform.h
// (shared between rcore.c and platform backends)

//----------------------------------------------------------------------------------
// Global Variables Definition
//----------------------------------------------------------------------------------
RLAPI const char *raylib_version = RAYLIB_VERSION;  // raylib version exported symbol, required for some bindings

// Raylib global context instance
// All global state lives in this context, accessed via GetRaylibContext().
// The CORE macro (from raylib_context.h) provides backward-compatible access.
static RaylibContext raylibCtxDefault = { 0 };
static RaylibContext *raylibCtxActive = NULL;

// Get the current raylib context
RaylibContext *GetRaylibContext(void)
{
    return (raylibCtxActive != NULL) ? raylibCtxActive : &raylibCtxDefault;
}

// Set the current raylib context (pointer-based, no shallow copy)
// The caller is responsible for allocating and freeing the context.
// Pass NULL to reset to the default internal context.
void SetRaylibContext(RaylibContext *context)
{
    raylibCtxActive = context;
}

static int logTypeLevel = LOG_INFO;                 // Minimum log type level

static TraceLogCallback traceLog = NULL;            // TraceLog callback function pointer
// File system callback functions have been moved to rcore_fs.c

#if SUPPORT_SCREEN_CAPTURE
static int screenshotCounter = 0;                   // Screenshots counter
#endif

//-----------------------------------------------------------------------------------

//----------------------------------------------------------------------------------
// Module Functions Declaration
// NOTE: Those functions are common for all platforms!
//----------------------------------------------------------------------------------
#if SUPPORT_MODULE_RTEXT
extern void LoadFontDefault(void);      // [Module: text] Loads default font on InitWindow()
extern void UnloadFontDefault(void);    // [Module: text] Unloads default font from GPU memory
#endif

void InitTimer(void);                                // Initialize timer, hi-resolution if available (required by InitPlatform())
void SetupViewport(int width, int height);           // Set viewport for a provided width and height

// InitPlatform() and ClosePlatform() are declared in platform.h

// File system scanning functions have been moved to rcore_fs.c

#if defined(_WIN32) && !defined(PLATFORM_DESKTOP_RGFW)
// NOTE: Declaring Sleep() function symbol to avoid including windows.h (kernel32.lib linkage required)
__declspec(dllimport) void __stdcall Sleep(unsigned long msTimeout); // Required for: WaitTime()
#endif

#if !SUPPORT_MODULE_RTEXT
const char *TextFormat(const char *text, ...); // Formatting of text with variables to 'embed'
#endif

// NOTE: PLATFORM_DESKTOP defaults to GLFW backend
#if defined(PLATFORM_DESKTOP)
    #define PLATFORM_DESKTOP_GLFW
#endif

// Include platform API header (platform backends are compiled separately)


//----------------------------------------------------------------------------------
// Module Functions Definition: Window and Graphics Device
//----------------------------------------------------------------------------------

// NOTE: Functions with a platform-specific implementation on rcore_<platform>.c
//bool WindowShouldClose(void)
//void ToggleFullscreen(void)
//void ToggleBorderlessWindowed(void)
//void MaximizeWindow(void)
//void MinimizeWindow(void)
//void RestoreWindow(void)

//void SetWindowState(unsigned int flags)
//void ClearWindowState(unsigned int flags)

//void SetWindowIcon(Image image)
//void SetWindowIcons(Image *images, int count)
//void SetWindowTitle(const char *title)
//void SetWindowPosition(int x, int y)
//void SetWindowMonitor(int monitor)
//void SetWindowMinSize(int width, int height)
//void SetWindowMaxSize(int width, int height)
//void SetWindowSize(int width, int height)
//void SetWindowOpacity(float opacity)
//void SetWindowFocused(void)
//void *GetWindowHandle(void)
//Vector2 GetWindowPosition(void)
//Vector2 GetWindowScaleDPI(void)

//int GetMonitorCount(void)
//int GetCurrentMonitor(void)
//int GetMonitorWidth(int monitor)
//int GetMonitorHeight(int monitor)
//int GetMonitorPhysicalWidth(int monitor)
//int GetMonitorPhysicalHeight(int monitor)
//int GetMonitorRefreshRate(int monitor)
//Vector2 GetMonitorPosition(int monitor)
//const char *GetMonitorName(int monitor)

//void SetClipboardText(const char *text)
//const char *GetClipboardText(void)

//void ShowCursor(void)
//void HideCursor(void)
//void EnableCursor(void)
//void DisableCursor(void)

// Initialize window and OpenGL context
void InitWindow(int width, int height, const char *title)
{
    TRACELOG(LOG_INFO, "Initializing raylib %s", RAYLIB_VERSION);

#if defined(PLATFORM_DESKTOP_GLFW)
    TRACELOG(LOG_INFO, "Platform backend: DESKTOP (GLFW)");
#elif defined(PLATFORM_DESKTOP_SDL)
    TRACELOG(LOG_INFO, "Platform backend: DESKTOP (SDL)");
#elif defined(PLATFORM_DESKTOP_RGFW)
    TRACELOG(LOG_INFO, "Platform backend: DESKTOP (RGFW)");
#elif defined(PLATFORM_DESKTOP_WIN32)
    TRACELOG(LOG_INFO, "Platform backend: DESKTOP (WIN32)");
#elif defined(PLATFORM_WEB_RGFW)
    TRACELOG(LOG_INFO, "Platform backend: WEB (RGFW) (HTML5)");
#elif defined(PLATFORM_WEB)
    TRACELOG(LOG_INFO, "Platform backend: WEB (HTML5)");
#elif defined(PLATFORM_DRM)
    TRACELOG(LOG_INFO, "Platform backend: NATIVE DRM");
#elif defined(PLATFORM_MEMORY)
    TRACELOG(LOG_INFO, "Platform backend: MEMORY (No OS)");
#else
    // TODO: Include your custom platform backend!
    // i.e software rendering backend or console backend!
    TRACELOG(LOG_INFO, "Platform backend: CUSTOM");
#endif

    TRACELOG(LOG_INFO, "Supported raylib modules:");
    TRACELOG(LOG_INFO, "    > rcore:..... loaded (mandatory)");
    TRACELOG(LOG_INFO, "    > rlgl:...... loaded (mandatory)");
#if SUPPORT_MODULE_RSHAPES
    TRACELOG(LOG_INFO, "    > rshapes:... loaded (optional)");
#else
    TRACELOG(LOG_INFO, "    > rshapes:... not loaded (optional)");
#endif
#if SUPPORT_MODULE_RTEXTURES
    TRACELOG(LOG_INFO, "    > rtextures:. loaded (optional)");
#else
    TRACELOG(LOG_INFO, "    > rtextures:. not loaded (optional)");
#endif
#if SUPPORT_MODULE_RTEXT
    TRACELOG(LOG_INFO, "    > rtext:..... loaded (optional)");
#else
    TRACELOG(LOG_INFO, "    > rtext:..... not loaded (optional)");
#endif
#if SUPPORT_MODULE_RMODELS
    TRACELOG(LOG_INFO, "    > rmodels:... loaded (optional)");
#else
    TRACELOG(LOG_INFO, "    > rmodels:... not loaded (optional)");
#endif
#if SUPPORT_MODULE_RAUDIO
    TRACELOG(LOG_INFO, "    > raudio:.... loaded (optional)");
#else
    TRACELOG(LOG_INFO, "    > raudio:.... not loaded (optional)");
#endif

    // Initialize window data
    CORE.Window.screen.width = width;
    CORE.Window.screen.height = height;

    CORE.Window.eventWaiting = false;
    CORE.Window.screenScale = MatrixIdentity(); // No draw scaling required by default
    if ((title != NULL) && (title[0] != 0)) CORE.Window.title = title;

    // Initialize global input state
    memset(&CORE.Input, 0, sizeof(CORE.Input)); // Reset CORE.Input structure to 0
    CORE.Input.Keyboard.exitKey = KEY_ESCAPE;
    CORE.Input.Mouse.scale = (Vector2){ 1.0f, 1.0f };
    CORE.Input.Mouse.cursor = MOUSE_CURSOR_ARROW;
    CORE.Input.Gamepad.lastButtonPressed = GAMEPAD_BUTTON_UNKNOWN;

    // Initialize platform
    //--------------------------------------------------------------
    int result = InitPlatform();

    if (result != 0)
    {
        TRACELOG(LOG_WARNING, "SYSTEM: Failed to initialize platform");
        return;
    }

    // Initialize render dimensions for embedded platforms
    // NOTE: On desktop platforms (GLFW, SDL, etc.), CORE.Window.render.width/height are set during window creation
    // On embedded platforms with no window manager, InitPlatform() doesn't set these values, so they should be initialized
    // here from screen dimensions (which are set from the InitWindow parameters)
    if ((CORE.Window.render.width == 0) || (CORE.Window.render.height == 0))
    {
        CORE.Window.render.width = CORE.Window.screen.width;
        CORE.Window.render.height = CORE.Window.screen.height;
    }
    //--------------------------------------------------------------

    // Initialize rlgl default data (buffers and shaders)
    // NOTE: Current fbo size stored as globals in rlgl for convenience
    rlglInit(CORE.Window.render.width, CORE.Window.render.height);

    // Setup default viewport
    SetupViewport(CORE.Window.render.width, CORE.Window.render.height);

#if SUPPORT_MODULE_RTEXT
    // Load default font
    // WARNING: External function: Module required: rtext
    LoadFontDefault();
    #if SUPPORT_MODULE_RSHAPES
    // Use the rlgl default 1x1 white texture for shapes drawing.
    // NOTE: raylib upstream batches shapes with a white block glyph from the
    // default font (GetFontDefault().recs[95]); this project's default font is
    // a CJK font where codepoint 127 is NOT a white block, so every shapes draw
    // sampled an empty texel and rendered black. (diagnosing-bugs fix)
    Texture2D shapeTexture = { rlGetTextureIdDefault(), 1, 1, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8 };
    SetShapesTexture(shapeTexture, (Rectangle){ 0.0f, 0.0f, 1.0f, 1.0f });
    #endif
#else
    #if SUPPORT_MODULE_RSHAPES
    // Set default texture and rectangle to be used for shapes drawing
    // NOTE: rlgl default texture is a 1x1 pixel UNCOMPRESSED_R8G8B8A8
    Texture2D texture = { rlGetTextureIdDefault(), 1, 1, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8 };
    SetShapesTexture(texture, (Rectangle){ 0.0f, 0.0f, 1.0f, 1.0f });    // WARNING: Module required: rshapes
    #endif
#endif

    CORE.Time.frameCounter = 0;
    CORE.Window.shouldClose = false;

    // Initialize random seed
    SetRandomSeed((unsigned int)time(NULL));

    TRACELOG(LOG_INFO, "SYSTEM: Working Directory: %s", GetWorkingDirectory());
}

// Close window and unload OpenGL context
void CloseWindow(void)
{
#if SUPPORT_MODULE_RTEXT
    UnloadFontDefault();        // WARNING: Module required: rtext
#endif

    rlglClose();                // De-init rlgl

    // De-initialize platform
    //--------------------------------------------------------------
    ClosePlatform();
    //--------------------------------------------------------------

    CORE.Window.ready = false;
    TRACELOG(LOG_INFO, "Window closed successfully");
}

// Check if window has been initialized successfully
bool IsWindowReady(void)
{
    return CORE.Window.ready;
}

// Check if window is currently fullscreen
bool IsWindowFullscreen(void)
{
    return FLAG_IS_SET(CORE.Window.flags, FLAG_FULLSCREEN_MODE);
}

// Check if window is currently hidden
bool IsWindowHidden(void)
{
    return FLAG_IS_SET(CORE.Window.flags, FLAG_WINDOW_HIDDEN);
}

// Check if window has been minimized
bool IsWindowMinimized(void)
{
    return FLAG_IS_SET(CORE.Window.flags, FLAG_WINDOW_MINIMIZED);
}

// Check if window has been maximized
bool IsWindowMaximized(void)
{
    return FLAG_IS_SET(CORE.Window.flags, FLAG_WINDOW_MAXIMIZED);
}

// Check if window has the focus
bool IsWindowFocused(void)
{
    return !FLAG_IS_SET(CORE.Window.flags, FLAG_WINDOW_UNFOCUSED);
}

// Check if window has been resizedLastFrame
bool IsWindowResized(void)
{
    return CORE.Window.resizedLastFrame;
}

// Check if one specific window flag is enabled
bool IsWindowState(unsigned int flag)
{
    return FLAG_IS_SET(CORE.Window.flags, flag);
}

// Get current screen width
int GetScreenWidth(void)
{
    return CORE.Window.screen.width;
}

// Get current screen height
int GetScreenHeight(void)
{
    return CORE.Window.screen.height;
}

// Get current render width which is equal to screen width*dpi scale
int GetRenderWidth(void)
{
    int width = 0;

    if (CORE.Window.usingFbo) width = CORE.Window.currentFbo.width;
    else width = CORE.Window.render.width;

    return width;
}

// Get current screen height which is equal to screen height*dpi scale
int GetRenderHeight(void)
{
    int height = 0;

    if (CORE.Window.usingFbo) height = CORE.Window.currentFbo.height;
    else height = CORE.Window.render.height;

    return height;
}

// Enable waiting for events on EndDrawing(), no automatic event polling
void EnableEventWaiting(void)
{
    CORE.Window.eventWaiting = true;
}

// Disable waiting for events on EndDrawing(), automatic events polling
void DisableEventWaiting(void)
{
    CORE.Window.eventWaiting = false;
}

// Check if cursor is not visible
bool IsCursorHidden(void)
{
    return CORE.Input.Mouse.cursorHidden;
}

// Check if cursor is on the current screen
bool IsCursorOnScreen(void)
{
    return CORE.Input.Mouse.cursorOnScreen;
}

//----------------------------------------------------------------------------------
// Module Functions Definition: Screen Drawing
//----------------------------------------------------------------------------------

// Clear background (framebuffer) to color
void ClearBackground(Color color)
{
    rlClearColor(color.r, color.g, color.b, color.a);   // Set clear color
    rlClearScreenBuffers();                             // Clear current framebuffers
}

// Begin canvas (framebuffer) drawing
void BeginDrawing(void)
{
    // WARNING: Previously to BeginDrawing() other render textures drawing could happen,
    // consequently the measure for update vs draw is not accurate (only the total frame time is accurate)

    CORE.Time.current = GetTime();      // Number of elapsed seconds since InitTimer()
    CORE.Time.update = CORE.Time.current - CORE.Time.previous;
    CORE.Time.previous = CORE.Time.current;

    rlLoadIdentity();                   // Reset current matrix (modelview)
    rlMultMatrixf(MatrixToFloat(CORE.Window.screenScale)); // Apply screen scaling

    //rlTranslatef(0.375, 0.375, 0);    // HACK to have 2D pixel-perfect drawing on OpenGL 1.1
                                        // NOTE: Not required with OpenGL 3.3+
}

// End canvas (framebuffer) drawing and swap buffers (double buffering)
void EndDrawing(void)
{
    rlDrawRenderBatchActive();      // Update and draw internal render batch

// Forward declarations for functions no longer in public header
void SwapScreenBuffer(void);
void PollInputEvents(void);
void WaitTime(double seconds);

#if !SUPPORT_CUSTOM_FRAME_CONTROL
    SwapScreenBuffer();                  // Copy back buffer to front buffer (screen)

    // Frame time control system
    CORE.Time.current = GetTime();
    CORE.Time.draw = CORE.Time.current - CORE.Time.previous;
    CORE.Time.previous = CORE.Time.current;

    CORE.Time.frame = CORE.Time.update + CORE.Time.draw;

    // Wait for some milliseconds...
    if (CORE.Time.frame < CORE.Time.target)
    {
        WaitTime(CORE.Time.target - CORE.Time.frame);

        CORE.Time.current = GetTime();
        double waitTime = CORE.Time.current - CORE.Time.previous;
        CORE.Time.previous = CORE.Time.current;

        CORE.Time.frame += waitTime;    // Total frame time: update + draw + wait
    }

    PollInputEvents();      // Poll user events (before next frame update)
#endif

#if SUPPORT_SCREEN_CAPTURE
    if (IsKeyPressed(KEY_F12))
    {
        TakeScreenshot(TextFormat("screenshot%03i.png", screenshotCounter));
        screenshotCounter++;
    }
#endif

    CORE.Time.frameCounter++;
}

// Initialize 2D mode with custom camera (2D)
void BeginMode2D(Camera2D camera)
{
    rlDrawRenderBatchActive();      // Update and draw internal render batch

    rlLoadIdentity();               // Reset current matrix (modelview)

    // Apply 2d camera transformation to modelview
    rlMultMatrixf(MatrixToFloat(GetCameraMatrix2D(camera)));
}

// End 2D mode with custom camera
void EndMode2D(void)
{
    rlDrawRenderBatchActive();      // Update and draw internal render batch

    rlLoadIdentity();               // Reset current matrix (modelview)

    if (rlGetActiveFramebuffer() == 0) rlMultMatrixf(MatrixToFloat(CORE.Window.screenScale)); // Apply screen scaling if required
}

// Initializes render texture for drawing
void BeginTextureMode(RenderTexture2D target)
{
    rlDrawRenderBatchActive();      // Update and draw internal render batch

    rlEnableFramebuffer(target.id); // Enable render target

    // Set viewport and RLGL internal framebuffer size
    rlViewport(0, 0, target.texture.width, target.texture.height);
    rlSetFramebufferWidth(target.texture.width);
    rlSetFramebufferHeight(target.texture.height);

    rlMatrixMode(RL_PROJECTION);    // Switch to projection matrix
    rlLoadIdentity();               // Reset current matrix (projection)

    // Set orthographic projection to current framebuffer size
    // NOTE: Configured top-left corner as (0, 0)
    rlOrtho(0, target.texture.width, target.texture.height, 0, 0.0f, 1.0f);

    rlMatrixMode(RL_MODELVIEW);     // Switch back to modelview matrix
    rlLoadIdentity();               // Reset current matrix (modelview)

    //rlScalef(0.0f, -1.0f, 0.0f);  // Flip Y-drawing (?)

    // Setup current width/height for proper aspect ratio
    // calculation when using BeginTextureMode()
    CORE.Window.currentFbo.width = target.texture.width;
    CORE.Window.currentFbo.height = target.texture.height;
    CORE.Window.usingFbo = true;
}

// End drawing to render texture
void EndTextureMode(void)
{
    rlDrawRenderBatchActive();      // Update and draw internal render batch

    rlDisableFramebuffer();         // Disable render target (fbo)

    // Set viewport to default framebuffer size
    SetupViewport(CORE.Window.render.width, CORE.Window.render.height);

    // Go back to the modelview state from BeginDrawing, back to the main framebuffer
    rlMatrixMode(RL_MODELVIEW);     // Switch back to modelview matrix
    rlLoadIdentity();               // Reset current matrix (modelview)
    rlMultMatrixf(MatrixToFloat(CORE.Window.screenScale)); // Apply screen scaling if required

    // Reset current fbo to screen size
    CORE.Window.currentFbo.width = CORE.Window.render.width;
    CORE.Window.currentFbo.height = CORE.Window.render.height;
    CORE.Window.usingFbo = false;
}

// Begin blending mode (alpha, additive, multiplied, subtract, custom)
// NOTE: Blend modes supported are enumerated in BlendMode enum
void BeginBlendMode(int mode)
{
    rlSetBlendMode(mode);
}

// End blending mode (reset to default: alpha blending)
void EndBlendMode(void)
{
    rlSetBlendMode(BLEND_ALPHA);
}

// Begin scissor mode (define screen area for following drawing)
// NOTE: Scissor rec refers to bottom-left corner, changing it to upper-left
void BeginScissorMode(int x, int y, int width, int height)
{
    rlDrawRenderBatchActive();      // Update and draw internal render batch

    rlEnableScissorTest();

#if defined(__APPLE__)
    if (!CORE.Window.usingFbo)
    {
        Vector2 scale = GetWindowScaleDPI();
        rlScissor((int)(x*scale.x), (int)(GetScreenHeight()*scale.y - (((y + height)*scale.y))), (int)(width*scale.x), (int)(height*scale.y));
    }
#else
    if (!CORE.Window.usingFbo && FLAG_IS_SET(CORE.Window.flags, FLAG_WINDOW_HIGHDPI))
    {
        Vector2 scale = GetWindowScaleDPI();
        rlScissor((int)(x*scale.x), (int)(CORE.Window.currentFbo.height - (y + height)*scale.y), (int)(width*scale.x), (int)(height*scale.y));
    }
#endif
    else
    {
        rlScissor(x, CORE.Window.currentFbo.height - (y + height), width, height);
    }
}

// End scissor mode
void EndScissorMode(void)
{
    rlDrawRenderBatchActive();      // Update and draw internal render batch
    rlDisableScissorTest();
}

// Get camera 2d transform matrix
Matrix GetCameraMatrix2D(Camera2D camera)
{
    Matrix matTransform = { 0 };
    // The camera in world-space is set by
    //   1. Move it to target
    //   2. Rotate by -rotation and scale by (1/zoom)
    //      When setting higher scale, it's more intuitive for the world to become bigger (= camera become smaller),
    //      not for the camera getting bigger, hence the invert. Same deal with rotation
    //   3. Move it by (-offset);
    //      Offset defines target transform relative to screen, but since effectively "moving" screen (camera)
    //      it needs to be moved into opposite direction (inverse transform)

    // Having camera transform in world-space, inverse of it gives the modelview transform
    // Since (A*B*C)' = C'*B'*A', the modelview is
    //   1. Move to offset
    //   2. Rotate and Scale
    //   3. Move by -target
    Matrix matOrigin = MatrixTranslate(-camera.target.x, -camera.target.y, 0.0f);
    Matrix matRotation = MatrixRotate((Vector3){ 0.0f, 0.0f, 1.0f }, camera.rotation*DEG2RAD);
    Matrix matScale = MatrixScale(camera.zoom, camera.zoom, 1.0f);
    Matrix matTranslation = MatrixTranslate(camera.offset.x, camera.offset.y, 0.0f);

    matTransform = MatrixMultiply(MatrixMultiply(matOrigin, MatrixMultiply(matScale, matRotation)), matTranslation);

    return matTransform;
}

// Get screen space position for a 2d camera world space position
Vector2 GetWorldToScreen2D(Vector2 position, Camera2D camera)
{
    Matrix matCamera = GetCameraMatrix2D(camera);
    Vector3 transform = Vector3Transform((Vector3){ position.x, position.y, 0 }, matCamera);

    return (Vector2){ transform.x, transform.y };
}

// Get world space position for a 2d camera screen space position
Vector2 GetScreenToWorld2D(Vector2 position, Camera2D camera)
{
    Matrix invMatCamera = MatrixInvert(GetCameraMatrix2D(camera));
    Vector3 transform = Vector3Transform((Vector3){ position.x, position.y, 0 }, invMatCamera);

    return (Vector2){ transform.x, transform.y };
}

//----------------------------------------------------------------------------------
// Module Functions Definition: Timing
//----------------------------------------------------------------------------------

// NOTE: Functions with a platform-specific implementation on rcore_<platform>.c
//double GetTime(void)

// Set target FPS (maximum)
void SetTargetFPS(int fps)
{
    if (fps < 1) CORE.Time.target = 0.0;
    else CORE.Time.target = 1.0/(double)fps;

    TRACELOG(LOG_INFO, "TIMER: Target time per frame: %02.03f milliseconds", (float)CORE.Time.target*1000.0f);
}

// Get current FPS
// NOTE: Calculating an average framerate
int GetFPS(void)
{
    int fps = 0;

#if !SUPPORT_CUSTOM_FRAME_CONTROL
    #define FPS_CAPTURE_FRAMES_COUNT    30      // 30 captures
    #define FPS_AVERAGE_TIME_SECONDS   0.5f     // 500 milliseconds
    #define FPS_STEP (FPS_AVERAGE_TIME_SECONDS/FPS_CAPTURE_FRAMES_COUNT)

    static int index = 0;
    static float history[FPS_CAPTURE_FRAMES_COUNT] = { 0 };
    static float average = 0, last = 0;
    float fpsFrame = GetFrameTime();

    // If reseting the window, reset the FPS info
    if (CORE.Time.frameCounter == 0)
    {
        average = 0;
        last = 0;
        index = 0;

        for (int i = 0; i < FPS_CAPTURE_FRAMES_COUNT; i++) history[i] = 0;
    }

    if (fpsFrame != 0)
    {
        if ((GetTime() - last) > FPS_STEP)
        {
            last = (float)GetTime();
            index = (index + 1)%FPS_CAPTURE_FRAMES_COUNT;
            average -= history[index];
            history[index] = fpsFrame/FPS_CAPTURE_FRAMES_COUNT;
            average += history[index];
        }

        fps = (int)roundf(1.0f/average);
    }
    else fps = 0;
#endif

    return fps;
}

// Get time in seconds for last frame drawn (delta time)
float GetFrameTime(void)
{
    return (float)CORE.Time.frame;
}

//----------------------------------------------------------------------------------
// Module Functions Definition: Custom frame control
//----------------------------------------------------------------------------------

// NOTE: Functions with a platform-specific implementation on rcore_<platform>.c
// (forward declarations are provided at the usage site in EndDrawing)

// Wait for some time (stop program execution)
// NOTE: Sleep() granularity could be around 10 ms, it means, Sleep() could
// take longer than expected... for that reason a busy wait loop is used
// REF: http://stackoverflow.com/questions/43057578/c-programming-win32-games-sleep-taking-longer-than-expected
// REF: http://www.geisswerks.com/ryan/FAQS/timing.html --> All about timing on Win32!
void WaitTime(double seconds)
{
    if (seconds < 0) return;    // Security check

#if SUPPORT_BUSY_WAIT_LOOP || SUPPORT_PARTIALBUSY_WAIT_LOOP
    double destinationTime = GetTime() + seconds;
#endif

#if SUPPORT_BUSY_WAIT_LOOP
    while (GetTime() < destinationTime) { }
#else
    #if SUPPORT_PARTIALBUSY_WAIT_LOOP
        double sleepSeconds = seconds - seconds*0.05;  // NOTE: Reserve a percentage of the time for busy waiting
    #else
        double sleepSeconds = seconds;
    #endif

    // System halt functions
    #if defined(_WIN32)
        Sleep((unsigned long)(sleepSeconds*1000.0));
    #endif
    #if defined(__linux__) || defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__EMSCRIPTEN__)
        struct timespec req = { 0 };
        time_t sec = sleepSeconds;
        long nsec = (sleepSeconds - sec)*1000000000L;
        req.tv_sec = sec;
        req.tv_nsec = nsec;

        // NOTE: Use nanosleep() on Unix platforms... usleep() it's deprecated
        while (nanosleep(&req, &req) == -1) continue;
    #endif
    #if defined(__APPLE__)
        usleep(sleepSeconds*1000000.0);
    #endif

    #if SUPPORT_PARTIALBUSY_WAIT_LOOP
        while (GetTime() < destinationTime) { }
    #endif
#endif
}

//----------------------------------------------------------------------------------
// Module Functions Definition: Misc
//----------------------------------------------------------------------------------

// NOTE: Functions with a platform-specific implementation on rcore_<platform>.c
//void OpenURL(const char *url)

// Set the seed for the random number generator
void SetRandomSeed(unsigned int seed)
{
#if SUPPORT_RPRAND_GENERATOR
    rprand_set_seed(seed);
#else
    srand(seed);
#endif
}

// Get a random value between min and max included
int GetRandomValue(int min, int max)
{
    int value = 0;

    if (min > max)
    {
        int tmp = max;
        max = min;
        min = tmp;
    }

#if SUPPORT_RPRAND_GENERATOR
    value = rprand_get_value(min, max);
#else
    // WARNING: Ranges higher than RAND_MAX will return invalid results
    // More specifically, if (max - min) > INT_MAX there will be an overflow,
    // and otherwise if (max - min) > RAND_MAX the random value will incorrectly never exceed a certain threshold
    // NOTE: Depending on the library it can be as low as 32767
    if ((unsigned int)(max - min) > (unsigned int)RAND_MAX)
    {
        TRACELOG(LOG_WARNING, "Invalid GetRandomValue() arguments, range should not be higher than %i", RAND_MAX);
    }

    // NOTE: This one-line approach produces a non-uniform distribution,
    // as stated by Donald Knuth in the book The Art of Programming, so
    // using below approach for more uniform results
    //value = (rand()%(abs(max - min) + 1) + min);

    // More uniform range solution
    int range = (max - min) + 1;

    // Degenerate/overflow case: fall back to min (same behavior as "always min" instead of UB)
    // Also covers ranges larger than RAND_MAX+1, where rejection sampling would loop forever (D7)
    if ((range <= 0) || ((unsigned long)range > (unsigned long)RAND_MAX + 1UL)) value = min;
    else
    {
        // Rejection sampling to get a uniform integer in [min, max]
        unsigned long c = (unsigned long)RAND_MAX + 1UL; // Number of possible results
        unsigned long m = (unsigned long)range;          // Size of the target interval
        unsigned long t = c - (c%m);                     // Largest multiple of m <= c
        unsigned long r = 0;

        for (;;)
        {
            r = (unsigned long)rand();
            if (r < t) break;   // Only accept values within the fair region
        }

        value = min + (int)(r%m);
    }
#endif

    return value;
}



// Takes a screenshot of current screen
// NOTE: Provided fileName should not contain paths, saving to working directory
void TakeScreenshot(const char *fileName)
{
#if SUPPORT_MODULE_RTEXTURES
    // Security check to (partially) avoid malicious code
    if (strchr(fileName, '\'') != NULL) { TRACELOG(LOG_WARNING, "SYSTEM: Provided fileName could be potentially malicious, avoid [\'] character"); return; }

    // Apply content scaling if required
    Vector2 scale = { 1.0f, 1.0f };
    if (FLAG_IS_SET(CORE.Window.flags, FLAG_WINDOW_HIGHDPI)) scale = GetWindowScaleDPI();

    // Flush the pending batch before reading the framebuffer: this function may
    // be called inside BeginDrawing()/EndDrawing(), where draw calls issued so
    // far this frame have not been submitted yet (would yield an all-black PNG).
    rlDrawRenderBatchActive();

    unsigned char *imgData = rlReadScreenPixels((int)((float)CORE.Window.render.width*scale.x), (int)((float)CORE.Window.render.height*scale.y));
    Image image = { imgData, (int)((float)CORE.Window.render.width*scale.x), (int)((float)CORE.Window.render.height*scale.y), 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8 };

    char path[MAX_FILEPATH_LENGTH] = { 0 };
    strncpy(path, TextFormat("%s/%s", CORE.Storage.basePath, fileName), MAX_FILEPATH_LENGTH - 1);

    ExportImage(image, path); // WARNING: Module required: rtextures
    RL_FREE(imgData);

    if (FileExists(path)) TRACELOG(LOG_INFO, "SYSTEM: [%s] Screenshot taken successfully", path);
    else TRACELOG(LOG_WARNING, "SYSTEM: [%s] Screenshot could not be saved", path);
#else
    TRACELOG(LOG_WARNING,"IMAGE: ExportImage() requires module: rtextures");
#endif
}

// Set up window configuration flags (view FLAGS)
// NOTE: This function is expected to be called before window creation,
// because it sets up some flags for the window creation process
// To configure window states after creation, use SetWindowState()
void SetConfigFlags(unsigned int flags)
{
    if (CORE.Window.ready) TRACELOG(LOG_WARNING, "WINDOW: SetConfigFlags called after window initialization, Use \"SetWindowState\" to set flags instead");

    // Selected flags are set but not evaluated at this point,
    // flag evaluation happens at InitWindow() or SetWindowState()
    FLAG_SET(CORE.Window.flags, flags);
}

// void OpenURL(const char *url);   // Defined per platform

//----------------------------------------------------------------------------------
// Module Functions Definition: Logging system
//----------------------------------------------------------------------------------
// Set the current threshold (minimum) log level
void SetTraceLogLevel(int logType) { logTypeLevel = logType; }

// Show trace log messages (LOG_INFO, LOG_WARNING, LOG_ERROR, LOG_DEBUG)
void TraceLog(int logType, const char *text, ...)
{
#if SUPPORT_TRACELOG
    // Message has level below current threshold, don't emit
    if ((logType < logTypeLevel) || (text == NULL)) return;

    va_list args;
    va_start(args, text);

    if (traceLog)
    {
        traceLog(logType, text, args);
        va_end(args);
        return;
    }

    char buffer[MAX_TRACELOG_MSG_LENGTH] = { 0 };

    switch (logType)
    {
        case LOG_TRACE: strncpy(buffer, "TRACE: ", 8); break;
        case LOG_DEBUG: strncpy(buffer, "DEBUG: ", 8); break;
        case LOG_INFO: strncpy(buffer, "INFO: ", 7); break;
        case LOG_WARNING: strncpy(buffer, "WARNING: ", 10); break;
        case LOG_ERROR: strncpy(buffer, "ERROR: ", 8); break;
        case LOG_FATAL: strncpy(buffer, "FATAL: ", 8); break;
        default: break;
    }

    unsigned int textLength = (unsigned int)strlen(text);
    memcpy(buffer + strlen(buffer), text, (textLength < (MAX_TRACELOG_MSG_LENGTH - 12))? textLength : (MAX_TRACELOG_MSG_LENGTH - 12));
    strcat(buffer, "\n");
    vprintf(buffer, args);
    fflush(stdout);

    va_end(args);

    if (logType == LOG_FATAL) exit(EXIT_FAILURE);  // If fatal logging, exit program
#endif
}

// Set custom trace log
void SetTraceLogCallback(TraceLogCallback callback)
{
    traceLog = callback;
}

// Internal: report the currently installed callback (NULL when the default
// logger is active). Lets the AI adapter chain instead of overwrite it (D1).
TraceLogCallback GetCurrentTraceLogCallback(void)
{
    return traceLog;  // NULL means raylib's built-in logger is active
}

//----------------------------------------------------------------------------------
// Module Functions Definition: Memory management
//----------------------------------------------------------------------------------
// Internal memory allocator
// NOTE: Initializes to zero by default
void *MemAlloc(unsigned int size)
{
    void *ptr = RL_CALLOC(size, 1);
    return ptr;
}

// Internal memory reallocator
void *MemRealloc(void *ptr, unsigned int size)
{
    void *ret = RL_REALLOC(ptr, size);
    return ret;
}

// Internal memory free
void MemFree(void *ptr)
{
    RL_FREE(ptr);
}

//----------------------------------------------------------------------------------
// Module Functions Definition: Timer and Viewport
//----------------------------------------------------------------------------------

// Initialize hi-resolution timer
void InitTimer(void)
{
    // Setting a higher resolution can improve the accuracy of time-out intervals in wait functions
    // However, it can also reduce overall system performance, because the thread scheduler switches tasks more often
    // High resolutions can also prevent the CPU power management system from entering power-saving modes
    // Setting a higher resolution does not improve the accuracy of the high-resolution performance counter
#if defined(_WIN32) && SUPPORT_WINMM_HIGHRES_TIMER && !SUPPORT_BUSY_WAIT_LOOP && !defined(PLATFORM_DESKTOP_SDL)
    timeBeginPeriod(1); // Setup high-resolution timer to 1ms (granularity of 1-2 ms)
#endif

#if defined(__linux__) || defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__EMSCRIPTEN__)
    struct timespec now = { 0 };

    if (clock_gettime(CLOCK_MONOTONIC, &now) == 0) // Success
    {
        CORE.Time.base = (unsigned long long)now.tv_sec*1000000000LLU + (unsigned long long)now.tv_nsec;
    }
    else TRACELOG(LOG_WARNING, "TIMER: Hi-resolution timer not available");
#endif

    CORE.Time.previous = GetTime(); // Get time as double
}

// Set viewport for a provided width and height
void SetupViewport(int width, int height)
{
    CORE.Window.render.width = width;
    CORE.Window.render.height = height;

    // Set viewport width and height
    rlViewport(CORE.Window.renderOffset.x/2, CORE.Window.renderOffset.y/2, CORE.Window.render.width, CORE.Window.render.height);

    rlMatrixMode(RL_PROJECTION);        // Switch to projection matrix
    rlLoadIdentity();                   // Reset current matrix (projection)

    // Set orthographic projection to current framebuffer size
    // NOTE: Configured top-left corner as (0, 0)
    rlOrtho(0, CORE.Window.render.width, CORE.Window.render.height, 0, 0.0f, 1.0f);

    rlMatrixMode(RL_MODELVIEW);         // Switch back to modelview matrix
    rlLoadIdentity();                   // Reset current matrix (modelview)
}

//----------------------------------------------------------------------------------
// Module Functions Definition: Drop files
//----------------------------------------------------------------------------------

// Check if file has been dropped into window
bool IsFileDropped(void)
{
    bool result = false;

    if (CORE.Window.dropFileCount > 0) result = true;

    return result;
}

// Load dropped filepaths
FilePathList LoadDroppedFiles(void)
{
    FilePathList files = { 0 };

    files.count = CORE.Window.dropFileCount;
    files.paths = CORE.Window.dropFilepaths;

    return files;
}

// Unload dropped filepaths
void UnloadDroppedFiles(FilePathList files)
{
    // WARNING: files pointers are the same as internal ones

    if (files.count > 0)
    {
        for (unsigned int i = 0; i < files.count; i++) RL_FREE(files.paths[i]);

        RL_FREE(files.paths);

        CORE.Window.dropFileCount = 0;
        CORE.Window.dropFilepaths = NULL;
    }
}

//----------------------------------------------------------------------------------
// Module Functions Definition: Compression and Encoding
//----------------------------------------------------------------------------------

// Compress data (DEFLATE algorithm)
unsigned char *CompressData(const unsigned char *data, int dataSize, int *compDataSize)
{
    #define COMPRESSION_QUALITY_DEFLATE  8

    unsigned char *compData = NULL;

#if SUPPORT_COMPRESSION_API
    // Compress data and generate a valid DEFLATE stream
    struct sdefl *sdefl = (struct sdefl *)RL_CALLOC(1, sizeof(struct sdefl));   // WARNING: Possible stack overflow, struct sdefl is almost 1MB
    int bounds = sdefl_bound(dataSize);
    compData = (unsigned char *)RL_CALLOC(bounds, 1);

    *compDataSize = sdeflate(sdefl, compData, data, dataSize, COMPRESSION_QUALITY_DEFLATE);   // Compression level 8, same as stbiw
    RL_FREE(sdefl);

    TRACELOG(LOG_INFO, "SYSTEM: Compress data: Original size: %i -> Comp. size: %i", dataSize, *compDataSize);
#endif

    return compData;
}

// Decompress data (DEFLATE algorithm)
unsigned char *DecompressData(const unsigned char *compData, int compDataSize, int *dataSize)
{
    unsigned char *data = NULL;

#if SUPPORT_COMPRESSION_API
    // Decompress data from a valid DEFLATE stream
    unsigned char *data0 = (unsigned char *)RL_CALLOC(MAX_DECOMPRESSION_SIZE*1024*1024, 1);
    int size = sinflate(data0, MAX_DECOMPRESSION_SIZE*1024*1024, compData, compDataSize);

    // WARNING: RL_REALLOC can make (and leave) data copies in memory,
    // that can be a security concern in case of compression of sensitive data
    // So, using a second buffer to copy data manually, wiping original buffer memory
    data = (unsigned char *)RL_CALLOC(size, 1);
    memcpy(data, data0, size);
    memset(data0, 0, MAX_DECOMPRESSION_SIZE*1024*1024); // Wipe memory, is memset() safe?
    RL_FREE(data0);

    TRACELOG(LOG_INFO, "SYSTEM: Decompress data: Comp. size: %i -> Original size: %i", compDataSize, size);

    *dataSize = size;
#endif

    return data;
}

// File system functions (LoadFileData, SaveFileData, ExportDataAsCode, etc.)
// have been moved to rcore_fs.c for better module separation.

// File system functions (LoadFileText, SaveFileText, FileExists, IsFileExtension,
// DirectoryExists, GetFileLength, GetFileModTime, GetFileExtension, GetFileName,
// GetFileNameWithoutExt, GetDirectoryPath, GetPrevDirectoryPath, GetWorkingDirectory,
// GetApplicationDirectory, LoadDirectoryFiles, LoadDirectoryFilesEx, UnloadDirectoryFiles,
// ChangeDirectory, IsPathFile, IsFileNameValid, GetDirectoryFileCount,
// GetDirectoryFileCountEx, ScanDirectoryFiles, etc.)
// have been moved to rcore_fs.c for better module separation.



#if !SUPPORT_MODULE_RTEXT
// Formatting of text with variables to 'embed'
// WARNING: String returned will expire after this function is called MAX_TEXTFORMAT_BUFFERS times
const char *TextFormat(const char *text, ...)
{
#ifndef MAX_TEXTFORMAT_BUFFERS
    #define MAX_TEXTFORMAT_BUFFERS      4        // Maximum number of static buffers for text formatting
#endif
#ifndef MAX_TEXT_BUFFER_LENGTH
    #define MAX_TEXT_BUFFER_LENGTH   1024        // Maximum size of static text buffer
#endif

    // Define an array of buffers, so strings don't expire until MAX_TEXTFORMAT_BUFFERS invocations
    static char buffers[MAX_TEXTFORMAT_BUFFERS][MAX_TEXT_BUFFER_LENGTH] = { 0 };
    static int index = 0;

    char *currentBuffer = buffers[index];
    memset(currentBuffer, 0, MAX_TEXT_BUFFER_LENGTH);   // Clear buffer before using

    if (text != NULL)
    {
        va_list args;
        va_start(args, text);
        int requiredByteCount = vsnprintf(currentBuffer, MAX_TEXT_BUFFER_LENGTH, text, args);
        va_end(args);

        // If requiredByteCount is larger than the MAX_TEXT_BUFFER_LENGTH, then overflow occurred
        if (requiredByteCount >= MAX_TEXT_BUFFER_LENGTH)
        {
            // Inserting "..." at the end of the string to mark as truncated
            char *truncBuffer = buffers[index] + MAX_TEXT_BUFFER_LENGTH - 4; // Adding 4 bytes = "...\0"
            snprintf(truncBuffer, 4, "...");
        }

        index += 1; // Move to next buffer for next function call
        if (index >= MAX_TEXTFORMAT_BUFFERS) index = 0;
    }

    return currentBuffer;
}
#endif
