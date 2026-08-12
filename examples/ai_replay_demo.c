/**********************************************************************************************
*
*   ai_replay_demo.c - Engine feature showcase: record / playback + input injection
*
*   Self-running demo of the AI operability protocol. A "tester path" of key actions
*   (hold RIGHT, hold DOWN, hold LEFT, hold UP ...) is:
*     1. RECORD phase: injected via AIInjectInput while AIStartRecording is active —
*        the adapter stores each action with its frame number.
*     2. REPLAY phase: AIStartPlayback + AIUpdate re-inject the recorded actions frame
*        by frame, and the square follows the exact recorded path (driven by the
*        re-injected key state, exactly like a real AI tester driving the game).
*   While it runs, an AI tester can also send stdin commands (scene / state input /
*   record / playback / quit).
*
*   Build:  compile_game.bat examples\ai_replay_demo.c
*
**********************************************************************************************/

#include "raylib.h"
#include "raylib_ai_adapter.h"

#include <stdio.h>    // Required for: snprintf()

// "Tester path": hold each direction long enough to move, then release.
// Syntax: "press <key> down" / "press <key> up" (see ai_input.c AIInjectInput).
static const char *kPath[] = {
    "press right down", "press right down", "press right down",
    "press down down",  "press down down",
    "press left down",  "press left down",
    "press up down",    "press up down",
    "press right down", "press right down",
    "press right up",   "press down up", "press left up", "press up up"
};
static const int kPathLen = (int)(sizeof(kPath)/sizeof(kPath[0]));

int main(void)
{
    const int screenWidth = 800;
    const int screenHeight = 600;

    InitWindow(screenWidth, screenHeight, "barelib 录制回放演示");
    SetTargetFPS(60);

    AIAdapter *adapter = AIInit();
    AIStartCommandLoop(adapter);   // AI can drive/observe via stdin

    // Demo state machine
    enum { PHASE_RECORD, PHASE_REPLAY, PHASE_PAUSE } phase = PHASE_RECORD;
    double phaseStart = GetTime();
    int injectCount = 0;           // actions injected in the RECORD phase
    bool playbackStarted = false;

    float px = 400.0f, py = 300.0f;   // square position (starts at center)
    const float speed = 1.6f;         // px per frame at 60fps

    AIStartRecording(adapter);        // begin capturing the injected actions

    while (!WindowShouldClose())
    {
        double now = GetTime();

        // --- Phase transitions -------------------------------------------------
        if (phase == PHASE_RECORD)
        {
            // Inject one action every 0.3s (simulated AI tester input)
            if ((now - phaseStart) >= (injectCount + 1)*0.3 && injectCount < kPathLen)
            {
                AIInjectInput(adapter, kPath[injectCount]);
                injectCount++;
            }
            if (injectCount >= kPathLen)
            {
                AIStopRecording(adapter);     // freeze the recorded sequence
                phase = PHASE_REPLAY;
                phaseStart = now;
                playbackStarted = false;
            }
        }
        else if (phase == PHASE_REPLAY)
        {
            if (!playbackStarted)
            {
                AIStartPlayback(adapter);     // re-inject recorded actions frame by frame
                playbackStarted = true;
            }
            // Replay window: actions were recorded ~0.3s apart; give it enough time
            // to re-inject all of them, then pause and loop back to RECORD.
            if ((now - phaseStart) > 6.0)
            {
                phase = PHASE_PAUSE;
                phaseStart = now;
            }
        }
        else // PHASE_PAUSE
        {
            if ((now - phaseStart) > 1.5)
            {
                // Reset and loop: back to center, start recording a fresh sequence
                px = 400.0f; py = 300.0f;
                injectCount = 0;
                phase = PHASE_RECORD;
                phaseStart = now;
                AIStartRecording(adapter);
            }
        }

        // --- Square movement: only responds during REPLAY (the recorded path
        // is what's being demonstrated; RECORD just captures the actions).
        if (phase == PHASE_REPLAY)
        {
            if (IsKeyDown(KEY_RIGHT)) px += speed;
            if (IsKeyDown(KEY_LEFT))  px -= speed;
            if (IsKeyDown(KEY_DOWN))  py += speed;
            if (IsKeyDown(KEY_UP))    py -= speed;
        }
        if (px < 20.0f) px = 20.0f;
        if (px > 780.0f) px = 780.0f;
        if (py < 20.0f) py = 20.0f;
        if (py > 580.0f) py = 580.0f;

        // --- Draw --------------------------------------------------------------
        BeginDrawing();
        AIClearBackground((Color){ 18, 22, 26, 255 });

        // Movement trail (the path the square has walked this replay)
        if (phase == PHASE_REPLAY)
        {
            static float trail[256][2];
            static int trailCount = 0;
            if (trailCount < 256)
            {
                trail[trailCount][0] = px;
                trail[trailCount][1] = py;
                trailCount++;
            }
            for (int i = 0; i < trailCount; i++)
            {
                AIDrawPixel((int)trail[i][0], (int)trail[i][1], (Color){ 100, 220, 160, 255 });
            }
        }

        // The square (green during replay, dim gray while recording)
        if (phase == PHASE_REPLAY)
        {
            AIDrawRect(px - 20, py - 20, 40, 40, 0, (Color){ 80, 230, 130, 255 });
        }
        else
        {
            AIDrawRect(px - 20, py - 20, 40, 40, 0, (Color){ 110, 120, 130, 255 });
        }

        // HUD: phase, injected count, position
        const char *phaseName =
            (phase == PHASE_RECORD) ? "录制 RECORD（模拟 AI 注入方向键）" :
            (phase == PHASE_REPLAY) ? "回放 REPLAY（按帧重放注入的键）"   :
                                      "暂停 PAUSE";
        AIDrawText(GetFontDefault(), phaseName, 40, 40, 20, 1, (phase == PHASE_REPLAY) ? LIME : LIGHTGRAY);

        char info[128];
        snprintf(info, sizeof(info), "已注入 %d/%d 个动作，方块位置 (%.0f, %.0f)", injectCount, kPathLen, px, py);
        AIDrawText(GetFontDefault(), info, 40, 70, 15, 1, GRAY);

        AIDrawText(GetFontDefault(), "协议演示：record 录制输入 → playback 按帧重放 → AI 驱动闭环", 40, 560, 15, 1, DARKGRAY);
        AIDrawText(GetFontDefault(), "stdin 可发 scene / state input / record / playback / quit", 40, 578, 14, 1, DARKGRAY);

        EndDrawing();

        // ------------------------------------------------------------------
        // AI Adapter Update: processes stdin commands, advances playback
        AIUpdate(adapter);
    }

    AIClose(adapter);
    CloseWindow();
    return 0;
}
