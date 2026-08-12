/**********************************************************************************************
*
*   raylib_ai_input_win32.c - Windows input simulation (SendInput + PostMessage)
*
*   Implements the default input simulation functions declared in raylib_ai_input.h.
*   Two modes (AISetInputSimulationMode):
*     - AI_SIMULATE_SENDINPUT (default): posts to the OS input queue via SendInput.
*       Requires the game window to be in the foreground — background AI sessions
*       silently lose keyboard events to whatever window has focus.
*     - AI_SIMULATE_POSTMESSAGE: posts WM_* messages directly to the game window
*       (found by its GLFW window class). Works with the window in the background,
*       which is what makes an unattended AI tester able to actually operate the
*       game (spec R6).
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

#include "raylib_ai_input.h"
#include <stdbool.h>

// Platform-specific headers for input simulation
// NOTE: We define conflicting names to avoid collisions with raylib's
// CloseWindow(), ShowCursor(), and Rectangle.
#if defined(_WIN32)
    #define WIN32_LEAN_AND_MEAN
    #define NOGDI                  // Prevent GDI Rectangle conflict
    #define NOMINMAX
    #define CloseWindow  CloseWindow_WIN32   // Avoid conflict with raylib
    #define ShowCursor   ShowCursor_WIN32    // Avoid conflict with raylib
    #include <windows.h>
    #undef CloseWindow
    #undef ShowCursor
#else
    #error "This file is for Windows only. Include raylib_ai_input_stubs.c for other platforms."
#endif

//----------------------------------------------------------------------------------
// Module State
//----------------------------------------------------------------------------------

// Active simulation mode; AI_SIMULATE_SENDINPUT keeps the historical behavior
static AISimulateMode aiSimulateMode = AI_SIMULATE_SENDINPUT;

//----------------------------------------------------------------------------------
// Key Code Conversion
//----------------------------------------------------------------------------------

// Convert a raylib keycode to a Windows virtual key code
static WORD AIKeyToVK(int key)
{
    // Letters and numbers: raylib codes match ASCII/VK
    if (key >= '0' && key <= '9') return (WORD)key;
    if (key >= 'A' && key <= 'Z') return (WORD)key;

    // Special keys mapping
    switch (key)
    {
        case 32:   return VK_SPACE;      // space
        case 256:  return VK_ESCAPE;     // escape
        case 257:  return VK_RETURN;     // enter
        case 258:  return VK_TAB;        // tab
        case 259:  return VK_BACK;       // backspace
        case 260:  return VK_INSERT;     // insert
        case 261:  return VK_DELETE;     // delete
        case 262:  return VK_RIGHT;      // right
        case 263:  return VK_LEFT;       // left
        case 264:  return VK_DOWN;       // down
        case 265:  return VK_UP;         // up
        case 266:  return VK_PRIOR;      // pageup
        case 267:  return VK_NEXT;       // pagedown
        case 268:  return VK_HOME;       // home
        case 269:  return VK_END;        // end
        case 340:  return VK_LSHIFT;     // shift
        case 341:  return VK_LCONTROL;   // ctrl
        case 342:  return VK_LMENU;      // alt
        // F-keys
        case 290: return VK_F1;  case 291: return VK_F2;
        case 292: return VK_F3;  case 293: return VK_F4;
        case 294: return VK_F5;  case 295: return VK_F6;
        case 296: return VK_F7;  case 297: return VK_F8;
        case 298: return VK_F9;  case 299: return VK_F10;
        case 300: return VK_F11; case 301: return VK_F12;
        default: return 0;
    }
}

//----------------------------------------------------------------------------------
// PostMessage Helpers (background-safe injection)
//----------------------------------------------------------------------------------

// Locate the game window by GLFW's Win32 window class name.
// GLFW 3.x registers its windows under "GLFW30".
static HWND AIGameWindow(void)
{
    return FindWindowW(L"GLFW30", NULL);
}

// Navigation keys are "extended" keys (0xE0 prefix): GLFW resolves keys by
// scancode table, and arrow/Ins/Home/PgUp etc. require the KF_EXTENDED bit in
// lParam or they map to the wrong entry (e.g. plain 0x48 != Up's 0x148).
static bool AIKeyIsExtended(WORD vk)
{
    switch (vk)
    {
        case VK_UP: case VK_DOWN: case VK_LEFT: case VK_RIGHT:
        case VK_HOME: case VK_END: case VK_PRIOR: case VK_NEXT:
        case VK_INSERT: case VK_DELETE:
            return true;
        default:
            return false;
    }
}

// Build a WM_KEYDOWN/WM_KEYUP lParam: repeat count + scan code (+ extended bit)
// + transition state (bit 31 = KF_UP; GLFW checks HIWORD(lParam) & KF_UP)
static LPARAM AIKeyLParam(WORD vk, bool keyUp)
{
    UINT scan = MapVirtualKeyW(vk, MAPVK_VK_TO_VSC);
    LPARAM lp = 1 | ((LPARAM)scan << 16);
    if (AIKeyIsExtended(vk)) lp |= ((LPARAM)KF_EXTENDED << 16);
    if (keyUp) lp |= (1 << 31);          // bit 31: transition state (key up)
    return lp;
}

// Convert screen coordinates (the AI protocol coordinate space, matching
// SendInput/SetCursorPos) to the game window's client coordinates, which is
// what WM_MOUSEMOVE/WM_LBUTTONDOWN expect.
static bool AIScreenToClient(int screenX, int screenY, POINT *outClient)
{
    HWND hwnd = AIGameWindow();
    if (hwnd == NULL) return false;
    POINT pt = { screenX, screenY };
    if (!ScreenToClient(hwnd, &pt)) return false;
    *outClient = pt;
    return true;
}

//----------------------------------------------------------------------------------
// SendInput Path (foreground-required)
//----------------------------------------------------------------------------------

static void SendInputKeyDown(int key)
{
    WORD vk = AIKeyToVK(key);
    if (vk == 0) return;
    INPUT ip = { 0 };
    ip.type = INPUT_KEYBOARD;
    ip.ki.wVk = vk;
    SendInput(1, &ip, sizeof(INPUT));
}

static void SendInputKeyUp(int key)
{
    WORD vk = AIKeyToVK(key);
    if (vk == 0) return;
    INPUT ip = { 0 };
    ip.type = INPUT_KEYBOARD;
    ip.ki.wVk = vk;
    ip.ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(1, &ip, sizeof(INPUT));
}

//----------------------------------------------------------------------------------
// PostMessage Path (background-safe)
//----------------------------------------------------------------------------------

static void PostMessageKeyDown(int key)
{
    HWND hwnd = AIGameWindow();
    if (hwnd == NULL) return;
    WORD vk = AIKeyToVK(key);
    if (vk == 0) return;
    PostMessageW(hwnd, WM_KEYDOWN, vk, AIKeyLParam(vk, false));
}

static void PostMessageKeyUp(int key)
{
    HWND hwnd = AIGameWindow();
    if (hwnd == NULL) return;
    WORD vk = AIKeyToVK(key);
    if (vk == 0) return;
    PostMessageW(hwnd, WM_KEYUP, vk, AIKeyLParam(vk, true));
}

static void PostMessageMouseMove(int x, int y)
{
    POINT client;
    if (!AIScreenToClient(x, y, &client)) return;
    PostMessageW(AIGameWindow(), WM_MOUSEMOVE, 0, MAKELPARAM(client.x, client.y));
}

static void PostMessageMouseButton(int button, bool down)
{
    HWND hwnd = AIGameWindow();
    if (hwnd == NULL) return;
    POINT cursor;
    GetCursorPos(&cursor);               // reuse current position as the client coords
    POINT client = cursor;
    ScreenToClient(hwnd, &client);
    UINT msg = 0;
    if (button == 0) msg = down ? WM_LBUTTONDOWN : WM_LBUTTONUP;
    else if (button == 1) msg = down ? WM_RBUTTONDOWN : WM_RBUTTONUP;
    else if (button == 2) msg = down ? WM_MBUTTONDOWN : WM_MBUTTONUP;
    else return;
    WPARAM wparam = down ? MK_LBUTTON : 0;
    PostMessageW(hwnd, msg, wparam, MAKELPARAM(client.x, client.y));
}

static void PostMessageMouseWheel(int deltaX, int deltaY)
{
    (void)deltaX;                        // horizontal wheel unsupported via WM_MOUSEWHEEL
    HWND hwnd = AIGameWindow();
    if (hwnd == NULL) return;
    WPARAM wparam = (WPARAM)((int)deltaY * WHEEL_DELTA);
    PostMessageW(hwnd, WM_MOUSEWHEEL, wparam, 0);
}

//----------------------------------------------------------------------------------
// Public Interface Implementation
//----------------------------------------------------------------------------------

void AISetInputSimulationMode(AISimulateMode mode)
{
    aiSimulateMode = mode;
}

void DefaultSimulateKeyDown(int key)
{
    if (aiSimulateMode == AI_SIMULATE_POSTMESSAGE) PostMessageKeyDown(key);
    else SendInputKeyDown(key);
}

void DefaultSimulateKeyUp(int key)
{
    if (aiSimulateMode == AI_SIMULATE_POSTMESSAGE) PostMessageKeyUp(key);
    else SendInputKeyUp(key);
}

void DefaultSimulateMouseMove(int x, int y)
{
    if (aiSimulateMode == AI_SIMULATE_POSTMESSAGE) PostMessageMouseMove(x, y);
    else SetCursorPos(x, y);
}

void DefaultSimulateMouseDown(int button)
{
    if (aiSimulateMode == AI_SIMULATE_POSTMESSAGE)
    {
        PostMessageMouseButton(button, true);
        return;
    }
    INPUT ip = { 0 };
    ip.type = INPUT_MOUSE;
    DWORD flags = 0;
    if (button == 0) flags = MOUSEEVENTF_LEFTDOWN;
    else if (button == 1) flags = MOUSEEVENTF_RIGHTDOWN;
    else if (button == 2) flags = MOUSEEVENTF_MIDDLEDOWN;
    else return;
    ip.mi.dwFlags = flags;
    SendInput(1, &ip, sizeof(INPUT));
}

void DefaultSimulateMouseUp(int button)
{
    if (aiSimulateMode == AI_SIMULATE_POSTMESSAGE)
    {
        PostMessageMouseButton(button, false);
        return;
    }
    INPUT ip = { 0 };
    ip.type = INPUT_MOUSE;
    DWORD flags = 0;
    if (button == 0) flags = MOUSEEVENTF_LEFTUP;
    else if (button == 1) flags = MOUSEEVENTF_RIGHTUP;
    else if (button == 2) flags = MOUSEEVENTF_MIDDLEUP;
    else return;
    ip.mi.dwFlags = flags;
    SendInput(1, &ip, sizeof(INPUT));
}

void DefaultSimulateMouseWheel(int deltaX, int deltaY)
{
    if (aiSimulateMode == AI_SIMULATE_POSTMESSAGE)
    {
        PostMessageMouseWheel(deltaX, deltaY);
        return;
    }
    INPUT ip = { 0 };
    ip.type = INPUT_MOUSE;
    ip.mi.dwFlags = MOUSEEVENTF_WHEEL;
    ip.mi.mouseData = (DWORD)(deltaY * WHEEL_DELTA);
    SendInput(1, &ip, sizeof(INPUT));
}
