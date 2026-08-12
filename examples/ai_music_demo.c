/**********************************************************************************************
*
*   ai_music_demo.c - Engine feature showcase: AI-observable music visualizer
*
*   Demonstrates barelib's differentiators in one self-running demo:
*     - AI observability: every draw goes through AIDrawXXX, so the full scene
*       (visualizer bars, pulses, HUD) appears in the scene JSON via `scene`
*     - Procedural audio: AIPlayTone (5 waveforms, fade in/out) plays a melody,
*       AIPlayNoise makes a hi-hat; no audio files needed
*     - CJK out of the box: embedded font renders Chinese HUD text
*     - AI stdin protocol: while it runs, an AI tester can send
*         version / scene / state / elements / help / quit
*
*   Build:  compile_game.bat examples\ai_music_demo.c
*   Drive:  pipe commands into stdin, e.g. "scene\nquit\n"
*
**********************************************************************************************/

#include "raylib.h"
#include "raylib_ai_adapter.h"

#include <stdio.h>   // Required for: snprintf()
#include <math.h>    // Required for: sinf()

// Simple melodic pattern (frequencies in Hz), C major pentatonic
static const float kMelody[] = {
    262.0f, 330.0f, 392.0f, 523.0f, 392.0f, 330.0f, 294.0f, 262.0f,
    294.0f, 392.0f, 494.0f, 587.0f, 494.0f, 392.0f, 330.0f, 262.0f
};

int main(void)
{
    const int screenWidth = 800;
    const int screenHeight = 600;

    InitWindow(screenWidth, screenHeight, "barelib AI 音乐可视化演示");
    SetTargetFPS(60);
    InitAudioDevice();   // Required for procedural audio (AIPlayTone/AIPlayNoise)

    AIAdapter *adapter = AIInit();
    AIStartCommandLoop(adapter);   // AI can drive/observe via stdin

    // Visualizer state
    int noteIndex = 0;
    double lastBeatTime = 0.0;
    float pulse = 0.0f;            // 0..1 pulse driven by the last played note
    float currentFreq = 262.0f;
    int barCount = 32;

    while (!WindowShouldClose())
    {
        double now = GetTime();

        // --- Beat: play a melody note and pulse the visualizer (every 0.28s) ---
        if (now - lastBeatTime >= 0.28)
        {
            lastBeatTime = now;
            currentFreq = kMelody[noteIndex % (sizeof(kMelody)/sizeof(kMelody[0]))];
            noteIndex++;

            // Main note: sine wave with fade in/out (AIPlayTone handles this)
            AIPlayTone(adapter, currentFreq, 0.24f, 0.35f, AI_WAVEFORM_SINE, 0);
            // Hi-hat: short noise burst on every other beat
            if (noteIndex % 2 == 0) AIPlayNoise(adapter, 0.06f, 0.15f, 0);
            // Bass: square wave one octave down every 4th beat
            if (noteIndex % 4 == 0) AIPlayTone(adapter, currentFreq/2.0f, 0.2f, 0.2f, AI_WAVEFORM_SQUARE, 0);

            pulse = 1.0f;
        }

        // Pulse decays each frame
        pulse *= 0.96f;
        if (pulse < 0.02f) pulse = 0.0f;

        // ------------------------------------------------------------------
        BeginDrawing();
        AIClearBackground((Color){ 18, 18, 28, 255 });

        // Visualizer bars: height follows the current note frequency
        for (int i = 0; i < barCount; i++)
        {
            float phase = (float)i / (float)barCount;
            float h = 40.0f + 200.0f * (0.3f + 0.7f * (currentFreq/600.0f)) * (0.5f + 0.5f*sinf(now*4.0f + phase*6.28f));
            float x = 60.0f + phase * 680.0f;
            AIDrawRect(x, 420.0f - h, 16.0f, h, 0, (Color){ (int)(40+180*phase), (int)(80+120*(1-phase)), 200, 255 });
        }

        // Central pulse ring: scales with the beat
        float ringR = 60.0f + 60.0f * pulse;
        AIDrawCircle(screenWidth/2, 180, ringR, (Color){ 255, 200, 60, (int)(120 + 130*pulse) });
        AIDrawCircleLines(screenWidth/2, 180, ringR + 14.0f, (Color){ 255, 160, 40, (int)(80 + 150*pulse) });

        // Note label (Chinese HUD via embedded CJK font)
        AIDrawText(GetFontDefault(), "音符频率 (Hz)", 60, 40, 18, 1, LIGHTGRAY);
        char freqText[32];
        snprintf(freqText, sizeof(freqText), "%d", (int)currentFreq);
        AIDrawText(GetFontDefault(), freqText, 200, 40, 28, 1, GOLD);

        AIDrawText(GetFontDefault(), "AI 可观测演示：stdin 发送 scene / state / help / quit", 60, 560, 16, 1, DARKGRAY);
        AIDrawText(GetFontDefault(), "程序化音效：sine 主音 + noise 踩镲 + square 低音", 60, 100, 16, 1, GRAY);

        EndDrawing();

        // ------------------------------------------------------------------
        // AI Adapter Update: processes stdin commands, playback, audio slot recycle
        AIUpdate(adapter);
    }

    // NOTE: the command-loop thread is stopped by AIClose() below
    AIClose(adapter);
    CloseAudioDevice();

    CloseWindow();
    return 0;
}
