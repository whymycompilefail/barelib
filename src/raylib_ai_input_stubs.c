/**********************************************************************************************
*
*   raylib_ai_input_stubs.c - Non-platform input simulation stubs
*
*   Stub implementations for platforms without native input simulation support.
*   These are used as placeholders; actual input simulation on these platforms
*   requires platform-specific implementations (e.g., XTest on Linux).
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
*     1. The origin of this software must not be misinterpreted; you must not claim that you
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

// Stub implementations - no actual input simulation

void DefaultSimulateKeyDown(int key) { (void)key; }
void DefaultSimulateKeyUp(int key) { (void)key; }
void DefaultSimulateMouseMove(int x, int y) { (void)x; (void)y; }
void DefaultSimulateMouseDown(int button) { (void)button; }
void DefaultSimulateMouseUp(int button) { (void)button; }
void DefaultSimulateMouseWheel(int deltaX, int deltaY) { (void)deltaX; (void)deltaY; }

// Mode selection is Windows-only; other platforms have no injection at all
void AISetInputSimulationMode(AISimulateMode mode) { (void)mode; }