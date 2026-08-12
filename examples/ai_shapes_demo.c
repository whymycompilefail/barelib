/**********************************************************************************************
*
*   ai_shapes_demo.c - Engine feature showcase: AI-observable 2D shape gallery
*
*   Demonstrates every 2D primitive the engine draws, all routed through the
*   AIDrawXXX wrappers so the full scene appears in the scene JSON via `scene`:
*     - rect / rect_rounded (fill + outline, real corner rounding)
*     - circle / circle_lines
*     - circle_sector (fill + outline)
*     - triangle / triangle_lines / poly / line / pixel / text (CJK)
*   The rounded-rect and sector primitives are engine-native since the
*   "shallow wrapper deepening" pass (candidate 6).
*
*   Build:  compile_game.bat examples\ai_shapes_demo.c
*   Drive:  pipe commands into stdin, e.g. "scene\nquit\n"
*
**********************************************************************************************/

#include "raylib.h"
#include "raylib_ai_adapter.h"

#include <math.h>    // Required for: sinf()

int main(void)
{
    const int screenWidth = 800;
    const int screenHeight = 600;

    InitWindow(screenWidth, screenHeight, "barelib 形状画廊演示");
    SetTargetFPS(60);

    AIAdapter *adapter = AIInit();
    AIStartCommandLoop(adapter);   // AI can observe/drive via stdin

    while (!WindowShouldClose())
    {
        float t = (float)GetTime();

        BeginDrawing();
        AIClearBackground((Color){ 20, 22, 30, 255 });

        // --- Row 1: rectangles -------------------------------------------------
        // 1. Plain rect, slowly rotating
        AIDrawRect(120, 80, 80, 50, t*30.0f, (Color){ 230, 90, 90, 255 });
        AIDrawText(GetFontDefault(), "矩形 Rect", 80, 40, 14, 1, LIGHTGRAY);

        // 2. Rounded rect, filled (engine-native DrawRectangleRounded)
        AIDrawRectRounded(360, 60, 100, 60, 0.3f, 0, (Color){ 90, 190, 230, 255 });
        AIDrawText(GetFontDefault(), "圆角矩形(实心)", 330, 40, 14, 1, LIGHTGRAY);

        // 3. Rounded rect outline — real corner rounding with line thickness
        AIDrawRectRounded(600, 60, 100, 60, 0.3f, 4.0f, (Color){ 90, 230, 150, 255 });
        AIDrawText(GetFontDefault(), "圆角矩形(描边)", 585, 40, 14, 1, LIGHTGRAY);

        // --- Row 2: circles ----------------------------------------------------
        // 4. Pulsing filled circle
        float pr = 28.0f + 10.0f*sinf(t*3.0f);
        AIDrawCircle(120, 190, pr, (Color){ 230, 200, 80, 255 });
        AIDrawText(GetFontDefault(), "圆 Circle", 80, 145, 14, 1, LIGHTGRAY);

        // 5. Circle outline
        AIDrawCircleLines(360, 190, 35, (Color){ 200, 120, 230, 255 });
        AIDrawText(GetFontDefault(), "圆环 CircleLines", 315, 145, 14, 1, LIGHTGRAY);

        // 6. Circle sector, fill — sweep angle animated
        float sweep = 60.0f + 200.0f*(0.5f + 0.5f*sinf(t*1.5f));
        AIDrawCircleSector(600, 190, 40, -90.0f, -90.0f + sweep, 0, (Color){ 120, 220, 220, 255 });
        AIDrawText(GetFontDefault(), "扇形 Sector", 575, 145, 14, 1, LIGHTGRAY);

        // --- Row 3: polygons ---------------------------------------------------
        // 7. Triangle
        AIDrawTriangle(60, 300, 150, 280, 120, 340, (Color){ 240, 140, 60, 255 });
        AIDrawText(GetFontDefault(), "三角形 Triangle", 75, 250, 14, 1, LIGHTGRAY);

        // 8. Triangle outline
        AIDrawTriangleLines(300, 300, 380, 280, 350, 340, (Color){ 140, 240, 120, 255 });
        AIDrawText(GetFontDefault(), "三角形(描边)", 310, 250, 14, 1, LIGHTGRAY);

        // 9. Rotating hexagon
        AIDrawPoly(600, 310, 6, 45.0f, t*40.0f, (Color){ 230, 160, 220, 255 });
        AIDrawText(GetFontDefault(), "多边形 Poly", 580, 250, 14, 1, LIGHTGRAY);

        // --- Row 4: line / pixel / text ---------------------------------------
        // 10. Thick line
        AIDrawLine(70, 400, 180, 430, 4.0f, (Color){ 200, 200, 220, 255 });
        AIDrawText(GetFontDefault(), "线 Line", 85, 370, 14, 1, LIGHTGRAY);

        // 11. Pixel dots
        for (int i = 0; i < 10; i++)
        {
            AIDrawPixel(340 + i*6, 410 + (i % 2)*8, (Color){ 255, 255, 255, 255 });
        }
        AIDrawText(GetFontDefault(), "像素 Pixel", 340, 370, 14, 1, LIGHTGRAY);

        // 12. CJK text (embedded font, no asset needed)
        AIDrawText(GetFontDefault(), "中文文本 Text", 590, 390, 22, 1, (Color){ 250, 250, 200, 255 });

        // HUD
        AIDrawText(GetFontDefault(), "AI 观测形状画廊：stdin 发送 scene 查看全部元素类型 / quit 退出", 40, 560, 15, 1, DARKGRAY);

        EndDrawing();

        // ------------------------------------------------------------------
        // AI Adapter Update: processes stdin commands, playback, audio slots
        AIUpdate(adapter);
    }

    AIClose(adapter);
    CloseWindow();
    return 0;
}
