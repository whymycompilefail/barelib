/**********************************************************************************************
*
*   platform.h - Platform backends interface header for raylib
*
*   This header defines the interface between rcore.c and all platform backend implementations.
*   Platform backends (platform_*.c) implement these functions to provide window management,
*   input handling, and platform-specific functionality.
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

#ifndef PLATFORM_H
#define PLATFORM_H

#include "raylib.h"
#include "config.h"

//----------------------------------------------------------------------------------
// Flag manipulation macros
// NOTE: Defined here so both rcore.c and platform backends share them.
//----------------------------------------------------------------------------------
#define FLAG_SET(n, f) ((n) |= (f))
#define FLAG_CLEAR(n, f) ((n) &= ~(f))
#define FLAG_TOGGLE(n, f) ((n) ^= (f))
#define FLAG_IS_SET(n, f) (((n) & (f)) == (f))

// Point and Size types for platform data
typedef struct { int x; int y; } Point;
typedef struct { int width; int height; } Size;

// Core global state context data
typedef struct CoreData {
    struct {
        const char *title;                  // Window text title const pointer
        unsigned int flags;                 // Configuration flags (bit based), keeps window state
        bool ready;                         // Check if window has been initialized successfully
        bool shouldClose;                   // Check if window set for closing
        bool resizedLastFrame;              // Check if window has been resized last frame
        bool eventWaiting;                  // Wait for events before ending frame
        bool usingFbo;                      // Using FBO (RenderTexture) for rendering instead of default framebuffer

        Size display;  // Display width and height (monitor, device-screen, LCD, ...)
        Size screen;   // Screen current width and height
        Point position; // Window current position
        Size previousScreen;   // Screen previous width and height
        Point previousPosition; // Window previous position
        Size render;   // Screen framebuffer width and height
        Point renderOffset;     // Screen framebuffer render offset
        Size currentFbo;       // Current framebuffer render width and height
        Size screenMin;        // Screen minimum width and height
        Size screenMax;        // Screen maximum width and height
        Matrix screenScale;                 // Matrix to scale screen (framebuffer rendering)

        char **dropFilepaths;               // Store dropped files paths pointers
        unsigned int dropFileCount;         // Count dropped files strings
    } Window;
    struct {
        const char *basePath;               // Base path for data storage
    } Storage;
    struct {
        struct {
            int exitKey;                    // Default exit key
            char currentKeyState[MAX_KEYBOARD_KEYS]; // Registers current frame key state
            char previousKeyState[MAX_KEYBOARD_KEYS]; // Registers previous frame key state
            char keyRepeatInFrame[MAX_KEYBOARD_KEYS]; // Registers key repeats for current frame
            int keyPressedQueue[MAX_KEY_PRESSED_QUEUE]; // Input keys queue
            int keyPressedQueueCount;       // Input keys queue count
            int charPressedQueue[MAX_CHAR_PRESSED_QUEUE]; // Input characters queue (unicode)
            int charPressedQueueCount;      // Input characters queue count
        } Keyboard;
        struct {
            Vector2 offset;                 // Mouse offset
            Vector2 scale;                  // Mouse scaling
            Vector2 currentPosition;        // Mouse position on screen
            Vector2 previousPosition;       // Previous mouse position
            Vector2 lockedPosition;         // Mouse position when locked
            int cursor;                     // Tracks current mouse cursor
            bool cursorHidden;              // Track if cursor is hidden
            bool cursorLocked;              // Track if cursor is locked (disabled)
            bool cursorOnScreen;            // Tracks if cursor is inside client area
            char currentButtonState[MAX_MOUSE_BUTTONS]; // Registers current mouse button state
            char previousButtonState[MAX_MOUSE_BUTTONS]; // Registers previous mouse button state
            Vector2 currentWheelMove;       // Registers current mouse wheel variation
            Vector2 previousWheelMove;      // Registers previous mouse wheel variation
        } Mouse;
        struct {
            int pointCount;                                 // Number of touch points active
            int pointId[MAX_TOUCH_POINTS];                  // Point identifiers
            Vector2 position[MAX_TOUCH_POINTS];             // Touch position on screen
            Vector2 previousPosition[MAX_TOUCH_POINTS];     // Previous touch position on screen
            char currentTouchState[MAX_TOUCH_POINTS];       // Registers current touch state
            char previousTouchState[MAX_TOUCH_POINTS];      // Registers previous touch state
        } Touch;
        struct {
            int lastButtonPressed;          // Register last gamepad button pressed
            int axisCount[MAX_GAMEPADS];    // Register number of available gamepad axes
            bool ready[MAX_GAMEPADS];       // Flag to know if gamepad is ready
            char name[MAX_GAMEPADS][MAX_GAMEPAD_NAME_LENGTH];               // Gamepad name holder
            char currentButtonState[MAX_GAMEPADS][MAX_GAMEPAD_BUTTONS];     // Current gamepad buttons state
            char previousButtonState[MAX_GAMEPADS][MAX_GAMEPAD_BUTTONS];    // Previous gamepad buttons state
            float axisState[MAX_GAMEPADS][MAX_GAMEPAD_AXES];                // Gamepad axes state
        } Gamepad;
    } Input;
    struct {
        double current;                     // Current time measure (seconds)
        double previous;                    // Previous time measure (seconds)
        double update;                      // Time measure for frame update (seconds)
        double draw;                        // Time measure for frame draw (seconds)
        double frame;                       // Time measure for one frame (seconds)
        double target;                      // Desired time for one frame, if 0 not applied (seconds)
        unsigned long long base;            // Base time measure for hi-res timer (ticks or nanoseconds)
        unsigned int frameCounter;          // Frame counter (frames)
    } Time;
} CoreData;

// CoreData is defined above and accessed via GetRaylibContext()->core
// See raylib_context.h for context management and the CORE macro

//----------------------------------------------------------------------------------
// Internal platform helper functions
// NOTE: These functions are defined in rcore.c and used by platform backends
//----------------------------------------------------------------------------------
extern void InitTimer(void);
extern void SetupViewport(int width, int height);

//----------------------------------------------------------------------------------
// Platform function declarations (all platforms must implement these)
//----------------------------------------------------------------------------------
int InitPlatform(void);
void ClosePlatform(void);
bool WindowShouldClose(void);
void ToggleFullscreen(void);
void ToggleBorderlessWindowed(void);
void MaximizeWindow(void);
void MinimizeWindow(void);
void RestoreWindow(void);
void SetWindowState(unsigned int flags);
void ClearWindowState(unsigned int flags);
void SetWindowIcon(Image image);
void SetWindowIcons(Image *images, int count);
void SetWindowTitle(const char *title);
void SetWindowPosition(int x, int y);
void SetWindowMonitor(int monitor);
void SetWindowMinSize(int width, int height);
void SetWindowMaxSize(int width, int height);
void SetWindowSize(int width, int height);
void SetWindowOpacity(float opacity);
void SetWindowFocused(void);
void *GetWindowHandle(void);
int GetMonitorCount(void);
int GetCurrentMonitor(void);
int GetMonitorWidth(int monitor);
int GetMonitorHeight(int monitor);
int GetMonitorPhysicalWidth(int monitor);
int GetMonitorPhysicalHeight(int monitor);
int GetMonitorRefreshRate(int monitor);
const char *GetMonitorName(int monitor);
void SetClipboardText(const char *text);
const char *GetClipboardText(void);
void ShowCursor(void);
void HideCursor(void);
void EnableCursor(void);
void DisableCursor(void);
void SwapScreenBuffer(void);
double GetTime(void);
void OpenURL(const char *url);
int SetGamepadMappings(const char *mappings);
void SetGamepadVibration(int gamepad, float leftMotor, float rightMotor, float duration);
void SetMousePosition(int x, int y);
void SetMouseCursor(int cursor);
const char *GetKeyName(int key);
void PollInputEvents(void);

#endif // PLATFORM_H