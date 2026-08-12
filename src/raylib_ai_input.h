/**********************************************************************************************
*
*   raylib_ai_input.h - Platform input simulation abstraction for AI adapter
*
*   This header defines the default platform-specific input simulation functions
*   used by the AI adapter when no custom AIDeps are provided. Each platform
*   backend implements these functions in its own compilation unit.
*
*   FEATURES:
*       - Platform-agnostic input simulation interface
*       - Default implementations use platform APIs (SendInput on Windows)
*       - Separated from AI adapter core logic for testability
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

#ifndef RAYLIB_AI_INPUT_H
#define RAYLIB_AI_INPUT_H

#if defined(__cplusplus)
extern "C" {            // Prevents name mangling of functions
#endif

//----------------------------------------------------------------------------------
// Default Input Simulation Functions
//----------------------------------------------------------------------------------
// These functions provide platform-specific implementations for simulating
// keyboard and mouse input. They are used as defaults in AIAdapter's AIDeps
// when no custom implementations are provided via AIInitEx().

void DefaultSimulateKeyDown(int key);
void DefaultSimulateKeyUp(int key);
void DefaultSimulateMouseMove(int x, int y);
void DefaultSimulateMouseDown(int button);
void DefaultSimulateMouseUp(int button);
void DefaultSimulateMouseWheel(int deltaX, int deltaY);

//----------------------------------------------------------------------------------
// Input Simulation Mode (spec R6: background-window injection)
//----------------------------------------------------------------------------------
// SendInput (default) posts to the OS input queue and therefore requires the
// game window to be in the FOREGROUND; a background AI session's key events go
// to whatever window has focus (they silently do nothing to the game).
// PostMessage posts WM_* messages directly to the game window, which works with
// the window in the background (no focus needed) — this is what makes an AI
// tester truly able to operate a headless/background game session.
typedef enum AISimulateMode {
    AI_SIMULATE_SENDINPUT = 0,   // OS input queue; requires foreground window
    AI_SIMULATE_POSTMESSAGE      // Direct window messages; background-safe
} AISimulateMode;

// Select the mode used by the DefaultSimulate* functions (Windows only;
// ignored on other platforms). The `inputmode` command (see AI Adapter)
// calls this at runtime.
void AISetInputSimulationMode(AISimulateMode mode);

#if defined(__cplusplus)
}
#endif

#endif // RAYLIB_AI_INPUT_H