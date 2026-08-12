/**********************************************************************************************
*
*   ai_protocol_harness.c - Hidden-window protocol test harness
*
*   A minimal AI game for exercising the stdin/stdout AI protocol without
*   desktop interference (spec N5 test discipline). The window is created with
*   FLAG_WINDOW_HIDDEN, so the game runs headless-friendly while the command
*   loop and scene JSON still work — this lets tests/ai_protocol_check.py and
*   tests/ai_scenario_check.py run on servers and CI-like desktop sessions.
*
*   Built as the `ai_protocol_harness` CMake target (tests/CMakeLists.txt).
*
*   LICENSE: zlib/libpng
*
**********************************************************************************************/

#include "raylib.h"
#include "raylib_ai_adapter.h"

int main(void)
{
    SetConfigFlags(FLAG_WINDOW_HIDDEN);   // No visible window; protocol only
    InitWindow(640, 480, "AI Protocol Harness (hidden)");

    AIAdapter *adapter = AIInit();
    if (adapter == NULL)
    {
        CloseWindow();
        return 1;
    }

    // Enable stdin-driven command loop (processed inside AIUpdate)
    AIStartCommandLoop(adapter);

    while (!WindowShouldClose())
    {
        BeginDrawing();
        AIClearBackground(RAYWHITE);
        AIDrawRect(10, 10, 100, 40, 0, BLUE);
        AIDrawText(GetFontDefault(), "harness", 10, 60, 20, 1, BLACK);
        EndDrawing();

        // Process queued stdin commands and advance playback/audio state
        AIUpdate(adapter);
    }

    AIClose(adapter);
    CloseWindow();

    return 0;
}
