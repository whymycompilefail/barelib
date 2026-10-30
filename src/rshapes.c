/**********************************************************************************************
*
*   rshapes - Basic functions to draw 2d shapes and check collisions
*
*   ADDITIONAL NOTES:
*       Shapes can be draw using 3 types of primitives: LINES, TRIANGLES and QUADS
*       Some functions implement two drawing options: TRIANGLES and QUADS, by default TRIANGLES
*       are used but QUADS implementation can be selected with SUPPORT_QUADS_DRAW_MODE define
*
*       Some functions define texture coordinates (rlTexCoord2f()) for the shapes and use a
*       user-provided texture with SetShapesTexture(), the purpose of this implementation
*       is allowing to reduce draw calls when combined with a texture-atlas
*
*       By default, raylib sets the default texture and rectangle at InitWindow()[rcore] to one
*       white character of default font [rtext], this way, raylib text and shapes can be drawn with
*       a single draw call and it also allows users to configure it the same way with their own fonts
*
*   CONFIGURATION:
*       #define SUPPORT_MODULE_RSHAPES      1
*           rshapes module is included in the build
*
*       #define SUPPORT_QUADS_DRAW_MODE     1
*           Use QUADS instead of TRIANGLES for drawing when possible. Lines-based shapes still use LINES
*
*
*   LICENSE: zlib/libpng
*
*   Copyright (c) 2013-2026 Ramon Santamaria (@raysan5)
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

#include "raylib.h"     // Declares module functions

#include "config.h"     // Defines module configuration flags

#if SUPPORT_MODULE_RSHAPES

#include "rlgl.h"       // OpenGL abstraction layer to OpenGL 1.1, 2.1, 3.3+ or ES2
#include "raylib_observability.h"   // Required for: AIDrawObserver draw hook (spec M3)
#include "raylib_context.h"         // Required for: RaylibContext, GetRaylibContext() (observer owner)

#include <math.h>       // Required for: sinf(), asinf(), cosf(), acosf(), sqrtf(), fabsf()
#include <float.h>      // Required for: FLT_EPSILON
#include <stdlib.h>     // Required for: RL_FREE

//----------------------------------------------------------------------------------
// Defines and Macros
//----------------------------------------------------------------------------------
#ifndef SMOOTH_CIRCLE_ERROR_RATE
    // Define error rate to calculate how many segments are needed to draw a smooth circle
    // REF: https://stackoverflow.com/a/2244088
    #define SMOOTH_CIRCLE_ERROR_RATE    0.5f      // Circle error rate
#endif
#ifndef SPLINE_SEGMENT_DIVISIONS
    #define SPLINE_SEGMENT_DIVISIONS      24      // Spline segment divisions
#endif

//----------------------------------------------------------------------------------
// Types and Structures Definition
//----------------------------------------------------------------------------------
// Not here...

//----------------------------------------------------------------------------------
// Global Variables Definition
//----------------------------------------------------------------------------------
static Texture2D texShapes = { 1, 1, 1, 1, 7 };                // Texture used on shapes drawing (white pixel loaded by rlgl)
static Rectangle texShapesRec = { 0.0f, 0.0f, 1.0f, 1.0f };    // Texture source rectangle used on shapes drawing

// Draw observation hook state (spec M3: non-invasive observability probe)
// Registered observer receives semantic draw parameters from hooked DrawXXX
// functions; NULL (default) means one compare-and-branch per draw call.
// NOTE: observer + owner live in RaylibContext so ownership can be tracked
// across the engine↔adapter seam (single-adapter premise, AI_ERR_BUSY on conflict).
void SetAIDrawObserver(AIDrawObserver observer)
{
    SetAIDrawObserverOwned(observer, NULL);
}

void SetAIDrawObserverOwned(AIDrawObserver observer, void *owner)
{
    GetRaylibContext()->aiDrawObserver = observer;
    GetRaylibContext()->aiDrawObserverOwner = owner;
}

AIDrawObserver GetAIDrawObserver(void)
{
    return GetRaylibContext()->aiDrawObserver;
}

//----------------------------------------------------------------------------------
// Module Internal Functions Declaration
//----------------------------------------------------------------------------------
static float EaseCubicInOut(float t, float b, float c, float d);    // Cubic easing

//----------------------------------------------------------------------------------
// Module Functions Definition
//----------------------------------------------------------------------------------
// Set texture and rectangle to be used on shapes drawing
// NOTE: It can be useful when using basic shapes and one single font,
// defining a font char white rectangle would allow drawing everything in a single draw call
void SetShapesTexture(Texture2D texture, Rectangle rec)
{
    // Reset texture to default pixel if required
    // WARNING: Shapes texture should be probably better validated,
    // it can break the rendering of all shapes if misused
    if ((texture.id == 0) || (rec.width == 0) || (rec.height == 0))
    {
        texShapes = (Texture2D){ 1, 1, 1, 1, 7 };
        texShapesRec = (Rectangle){ 0.0f, 0.0f, 1.0f, 1.0f };
    }
    else
    {
        texShapes = texture;
        texShapesRec = rec;
    }
}

// Get texture that is used for shapes drawing
Texture2D GetShapesTexture(void)
{
    return texShapes;
}

// Get texture source rectangle that is used for shapes drawing
Rectangle GetShapesTextureRectangle(void)
{
    return texShapesRec;
}

// Draw a pixel
void DrawPixel(int posX, int posY, Color color)
{
    DrawPixelV((Vector2){ (float)posX, (float)posY }, color);
}

// Draw a pixel (Vector version)
void DrawPixelV(Vector2 position, Color color)
{
#if SUPPORT_QUADS_DRAW_MODE
    rlSetTexture(GetShapesTexture().id);
    Rectangle shapeRect = GetShapesTextureRectangle();

    rlBegin(RL_QUADS);

        rlNormal3f(0.0f, 0.0f, 1.0f);
        rlColor4ub(color.r, color.g, color.b, color.a);

        rlTexCoord2f(shapeRect.x/texShapes.width, shapeRect.y/texShapes.height);
        rlVertex2f(position.x, position.y);

        rlTexCoord2f(shapeRect.x/texShapes.width, (shapeRect.y + shapeRect.height)/texShapes.height);
        rlVertex2f(position.x, position.y + 1);

        rlTexCoord2f((shapeRect.x + shapeRect.width)/texShapes.width, (shapeRect.y + shapeRect.height)/texShapes.height);
        rlVertex2f(position.x + 1, position.y + 1);

        rlTexCoord2f((shapeRect.x + shapeRect.width)/texShapes.width, shapeRect.y/texShapes.height);
        rlVertex2f(position.x + 1, position.y);

    rlEnd();

    rlSetTexture(0);
#else
    rlBegin(RL_TRIANGLES);

        rlColor4ub(color.r, color.g, color.b, color.a);

        rlVertex2f(position.x, position.y);
        rlVertex2f(position.x, position.y + 1);
        rlVertex2f(position.x + 1, position.y);

        rlVertex2f(position.x + 1, position.y);
        rlVertex2f(position.x, position.y + 1);
        rlVertex2f(position.x + 1, position.y + 1);

    rlEnd();
#endif
}

// Draw a line (using gl lines)
void DrawLine(int startPosX, int startPosY, int endPosX, int endPosY, Color color)
{
    rlBegin(RL_LINES);
        rlColor4ub(color.r, color.g, color.b, color.a);
        rlVertex2f((float)startPosX, (float)startPosY);
        rlVertex2f((float)endPosX, (float)endPosY);
    rlEnd();
}

// Forward declaration for DrawTriangleStrip (used by DrawLineEx and DrawLineBezier)
static void DrawTriangleStrip(const Vector2 *points, int pointCount, Color color);

// Draw a line defining thickness
void DrawLineEx(Vector2 startPos, Vector2 endPos, float thick, Color color)
{
    Vector2 delta = { endPos.x - startPos.x, endPos.y - startPos.y };
    float length = sqrtf(delta.x*delta.x + delta.y*delta.y);

    if ((length > 0) && (thick > 0))
    {
        float scale = thick/(2*length);

        Vector2 radius = { -scale*delta.y, scale*delta.x };
        Vector2 strip[4] = {
            { startPos.x - radius.x, startPos.y - radius.y },
            { startPos.x + radius.x, startPos.y + radius.y },
            { endPos.x - radius.x, endPos.y - radius.y },
            { endPos.x + radius.x, endPos.y + radius.y }
        };

        DrawTriangleStrip(strip, 4, color);
    }
}

// Draw a line (using gl lines)
void DrawLineV(Vector2 startPos, Vector2 endPos, Color color)
{
    rlBegin(RL_LINES);
        rlColor4ub(color.r, color.g, color.b, color.a);
        rlVertex2f(startPos.x, startPos.y);
        rlVertex2f(endPos.x, endPos.y);
    rlEnd();
}

// Draw lines sequuence (using gl lines)
void DrawLineStrip(const Vector2 *points, int pointCount, Color color)
{
    if (pointCount < 2) return; // Security check

    rlBegin(RL_LINES);
        rlColor4ub(color.r, color.g, color.b, color.a);

        for (int i = 0; i < pointCount - 1; i++)
        {
            rlVertex2f(points[i].x, points[i].y);
            rlVertex2f(points[i + 1].x, points[i + 1].y);
        }
    rlEnd();
}

// Draw line using cubic-bezier spline, in-out interpolation, no control points
void DrawLineBezier(Vector2 startPos, Vector2 endPos, float thick, Color color)
{
    Vector2 previous = startPos;
    Vector2 current = { 0 };

    Vector2 points[2*SPLINE_SEGMENT_DIVISIONS + 2] = { 0 };

    for (int i = 1; i <= SPLINE_SEGMENT_DIVISIONS; i++)
    {
        // Cubic easing in-out
        // NOTE: Easing is calculated only for y position value
        current.y = EaseCubicInOut((float)i, startPos.y, endPos.y - startPos.y, (float)SPLINE_SEGMENT_DIVISIONS);
        current.x = previous.x + (endPos.x - startPos.x)/(float)SPLINE_SEGMENT_DIVISIONS;

        float dy = current.y - previous.y;
        float dx = current.x - previous.x;
        float size = 0.5f*thick/sqrtf(dx*dx+dy*dy);

        if (i == 1)
        {
            points[0].x = previous.x + dy*size;
            points[0].y = previous.y - dx*size;
            points[1].x = previous.x - dy*size;
            points[1].y = previous.y + dx*size;
        }

        points[2*i + 1].x = current.x - dy*size;
        points[2*i + 1].y = current.y + dx*size;
        points[2*i].x = current.x + dy*size;
        points[2*i].y = current.y - dx*size;

        previous = current;
    }

    DrawTriangleStrip(points, 2*SPLINE_SEGMENT_DIVISIONS + 2, color);
}

// Draw a dashed line
void DrawLineDashed(Vector2 startPos, Vector2 endPos, int dashSize, int spaceSize, Color color)
{
    // Calculate the vector and length of the line
    float dx = endPos.x - startPos.x;
    float dy = endPos.y - startPos.y;
    float lineLength = sqrtf(dx*dx + dy*dy);

    // If the line is too short for dashing or dash size is invalid, draw a solid thick line
    if ((lineLength < (dashSize + spaceSize)) || (dashSize <= 0))
    {
        DrawLineV(startPos, endPos, color);
        return;
    }

    // Calculate the normalized direction vector of the line
    float invLineLength = 1.0f/lineLength;
    float dirX = dx*invLineLength;
    float dirY = dy*invLineLength;

    Vector2 currentPos = startPos;
    float distanceTraveled = 0;

    rlBegin(RL_LINES);
        rlColor4ub(color.r, color.g, color.b, color.a);

        while (distanceTraveled < lineLength)
        {
            // Calculate the end of the current dash
            float dashEndDist = distanceTraveled + dashSize;
            if (dashEndDist > lineLength) dashEndDist = lineLength;

            Vector2 dashEndPos = { startPos.x + dashEndDist*dirX, startPos.y + dashEndDist*dirY };

            // Draw the dash segment
            rlVertex2f(currentPos.x, currentPos.y);
            rlVertex2f(dashEndPos.x, dashEndPos.y);

            // Update the distance traveled and move the current position for the next dash
            distanceTraveled = dashEndDist + spaceSize;
            currentPos.x = startPos.x + distanceTraveled*dirX;
            currentPos.y = startPos.y + distanceTraveled*dirY;
        }
    rlEnd();
}

// Draw a triangle
// NOTE: Vertex must be provided in counter-clockwise order
void DrawTriangle(Vector2 v1, Vector2 v2, Vector2 v3, Color color)
{
#if SUPPORT_QUADS_DRAW_MODE
    rlSetTexture(GetShapesTexture().id);
    Rectangle shapeRect = GetShapesTextureRectangle();

    rlBegin(RL_QUADS);
        rlNormal3f(0.0f, 0.0f, 1.0f);

        rlColor4ub(color.r, color.g, color.b, color.a);
        rlTexCoord2f(shapeRect.x/texShapes.width, shapeRect.y/texShapes.height);
        rlVertex2f(v1.x, v1.y);

        rlColor4ub(color.r, color.g, color.b, color.a);
        rlTexCoord2f(shapeRect.x/texShapes.width, (shapeRect.y + shapeRect.height)/texShapes.height);
        rlVertex2f(v2.x, v2.y);

        rlColor4ub(color.r, color.g, color.b, color.a);
        rlTexCoord2f((shapeRect.x + shapeRect.width)/texShapes.width, (shapeRect.y + shapeRect.height)/texShapes.height);
        rlVertex2f(v3.x, v3.y);

        rlColor4ub(color.r, color.g, color.b, color.a);
        rlTexCoord2f((shapeRect.x + shapeRect.width)/texShapes.width, shapeRect.y/texShapes.height);
        rlVertex2f(v3.x, v3.y);
    rlEnd();

    rlSetTexture(0);
#else
    rlBegin(RL_TRIANGLES);
        rlColor4ub(color.r, color.g, color.b, color.a);
        rlVertex2f(v1.x, v1.y);
        rlColor4ub(color.r, color.g, color.b, color.a);
        rlVertex2f(v2.x, v2.y);
        rlColor4ub(color.r, color.g, color.b, color.a);
        rlVertex2f(v3.x, v3.y);
    rlEnd();
#endif
}

// Draw a triangle using lines
// NOTE: Vertex must be provided in counter-clockwise order
void DrawTriangleLines(Vector2 v1, Vector2 v2, Vector2 v3, Color color)
{
    rlBegin(RL_LINES);
        rlColor4ub(color.r, color.g, color.b, color.a);
        rlVertex2f(v1.x, v1.y);
        rlVertex2f(v2.x, v2.y);

        rlVertex2f(v2.x, v2.y);
        rlVertex2f(v3.x, v3.y);

        rlVertex2f(v3.x, v3.y);
        rlVertex2f(v1.x, v1.y);
    rlEnd();
}

// Draw a triangle strip defined by points
// NOTE: Every new vertex connects with previous two
static void DrawTriangleStrip(const Vector2 *points, int pointCount, Color color)
{
    if (pointCount >= 3)
    {
        rlBegin(RL_TRIANGLES);
            rlColor4ub(color.r, color.g, color.b, color.a);

            for (int i = 2; i < pointCount; i++)
            {
                if ((i%2) == 0)
                {
                    rlVertex2f(points[i].x, points[i].y);
                    rlVertex2f(points[i - 2].x, points[i - 2].y);
                    rlVertex2f(points[i - 1].x, points[i - 1].y);
                }
                else
                {
                    rlVertex2f(points[i].x, points[i].y);
                    rlVertex2f(points[i - 1].x, points[i - 1].y);
                    rlVertex2f(points[i - 2].x, points[i - 2].y);
                }
            }
        rlEnd();
    }
}

// Draw a color-filled rectangle
void DrawRectangle(int posX, int posY, int width, int height, Color color)
{
    DrawRectangleV((Vector2){ (float)posX, (float)posY }, (Vector2){ (float)width, (float)height }, color);
}

// Draw a color-filled rectangle (Vector version)
// NOTE: On OpenGL 3.3 and ES2 using QUADS to avoid drawing order issues
void DrawRectangleV(Vector2 position, Vector2 size, Color color)
{
    DrawRectanglePro((Rectangle){ position.x, position.y, size.x, size.y }, (Vector2){ 0.0f, 0.0f }, 0.0f, color);
}

// Draw a color-filled rectangle
void DrawRectangleRec(Rectangle rec, Color color)
{
    DrawRectanglePro(rec, (Vector2){ 0.0f, 0.0f }, 0.0f, color);
}

// Draw a color-filled rectangle with pro parameters
void DrawRectanglePro(Rectangle rec, Vector2 origin, float rotation, Color color)
{
    // Non-invasive observability hook (spec M3): the rectangle family
    // (DrawRectangle/V/Rec) all converge here, so one hook covers them all.
    // NULL observer (default) costs one compare-and-branch per draw call.
    AIDrawObserver observer = GetAIDrawObserver();
    if (observer != NULL)
    {
        observer(AI_OBS_RECT, rec.x, rec.y, rec.width, rec.height, 0.0f, rotation, NULL, color);
    }

    Vector2 topLeft = { 0 };
    Vector2 topRight = { 0 };
    Vector2 bottomLeft = { 0 };
    Vector2 bottomRight = { 0 };

    // Only calculate rotation if needed
    if (rotation == 0.0f)
    {
        float x = rec.x - origin.x;
        float y = rec.y - origin.y;
        topLeft = (Vector2){ x, y };
        topRight = (Vector2){ x + rec.width, y };
        bottomLeft = (Vector2){ x, y + rec.height };
        bottomRight = (Vector2){ x + rec.width, y + rec.height };
    }
    else
    {
        float sinRotation = sinf(rotation*DEG2RAD);
        float cosRotation = cosf(rotation*DEG2RAD);
        float x = rec.x;
        float y = rec.y;
        float dx = -origin.x;
        float dy = -origin.y;

        topLeft.x = x + dx*cosRotation - dy*sinRotation;
        topLeft.y = y + dx*sinRotation + dy*cosRotation;

        topRight.x = x + (dx + rec.width)*cosRotation - dy*sinRotation;
        topRight.y = y + (dx + rec.width)*sinRotation + dy*cosRotation;

        bottomLeft.x = x + dx*cosRotation - (dy + rec.height)*sinRotation;
        bottomLeft.y = y + dx*sinRotation + (dy + rec.height)*cosRotation;

        bottomRight.x = x + (dx + rec.width)*cosRotation - (dy + rec.height)*sinRotation;
        bottomRight.y = y + (dx + rec.width)*sinRotation + (dy + rec.height)*cosRotation;
    }

#if SUPPORT_QUADS_DRAW_MODE
    rlSetTexture(GetShapesTexture().id);
    Rectangle shapeRect = GetShapesTextureRectangle();

    rlBegin(RL_QUADS);

        rlNormal3f(0.0f, 0.0f, 1.0f);
        rlColor4ub(color.r, color.g, color.b, color.a);

        rlTexCoord2f(shapeRect.x/texShapes.width, shapeRect.y/texShapes.height);
        rlVertex2f(topLeft.x, topLeft.y);

        rlTexCoord2f(shapeRect.x/texShapes.width, (shapeRect.y + shapeRect.height)/texShapes.height);
        rlVertex2f(bottomLeft.x, bottomLeft.y);

        rlTexCoord2f((shapeRect.x + shapeRect.width)/texShapes.width, (shapeRect.y + shapeRect.height)/texShapes.height);
        rlVertex2f(bottomRight.x, bottomRight.y);

        rlTexCoord2f((shapeRect.x + shapeRect.width)/texShapes.width, shapeRect.y/texShapes.height);
        rlVertex2f(topRight.x, topRight.y);

    rlEnd();

    rlSetTexture(0);
#else
    rlBegin(RL_TRIANGLES);

        rlColor4ub(color.r, color.g, color.b, color.a);

        rlVertex2f(topLeft.x, topLeft.y);
        rlVertex2f(bottomLeft.x, bottomLeft.y);
        rlVertex2f(topRight.x, topRight.y);

        rlVertex2f(topRight.x, topRight.y);
        rlVertex2f(bottomLeft.x, bottomLeft.y);
        rlVertex2f(bottomRight.x, bottomRight.y);

    rlEnd();
#endif
}

// Draw rectangle outline
// WARNING: All Draw*Lines() functions use RL_LINES for drawing,
// it implies flushing the current batch and changing draw mode to RL_LINES
// but it solves another issue: https://github.com/raysan5/raylib/issues/3884
void DrawRectangleLines(int posX, int posY, int width, int height, Color color)
{
    Matrix mat = rlGetMatrixTransform();
    float xOffset = 0.5f/mat.m0;
    float yOffset = 0.5f/mat.m5;

    rlBegin(RL_LINES);
        rlColor4ub(color.r, color.g, color.b, color.a);
        rlVertex2f((float)posX + xOffset, (float)posY + yOffset);
        rlVertex2f((float)posX + (float)width - xOffset, (float)posY + yOffset);

        rlVertex2f((float)posX + (float)width - xOffset, (float)posY + yOffset);
        rlVertex2f((float)posX + (float)width - xOffset, (float)posY + (float)height - yOffset);

        rlVertex2f((float)posX + (float)width - xOffset, (float)posY + (float)height - yOffset);
        rlVertex2f((float)posX + xOffset, (float)posY + (float)height - yOffset);

        rlVertex2f((float)posX + xOffset, (float)posY + (float)height - yOffset);
        rlVertex2f((float)posX + xOffset, (float)posY + yOffset);
    rlEnd();

/*
// Previous implementation, it has issues... but it does not require view matrix...
#if SUPPORT_QUADS_DRAW_MODE
    DrawRectangle(posX, posY, width, 1, color);
    DrawRectangle(posX + width - 1, posY + 1, 1, height - 2, color);
    DrawRectangle(posX, posY + height - 1, width, 1, color);
    DrawRectangle(posX, posY + 1, 1, height - 2, color);
#else
    rlBegin(RL_LINES);
        rlColor4ub(color.r, color.g, color.b, color.a);
        rlVertex2f((float)posX, (float)posY);
        rlVertex2f((float)posX + (float)width, (float)posY + 1);

        rlVertex2f((float)posX + (float)width, (float)posY + 1);
        rlVertex2f((float)posX + (float)width, (float)posY + (float)height);

        rlVertex2f((float)posX + (float)width, (float)posY + (float)height);
        rlVertex2f((float)posX + 1, (float)posY + (float)height);

        rlVertex2f((float)posX + 1, (float)posY + (float)height);
        rlVertex2f((float)posX + 1, (float)posY + 1);
    rlEnd();
#endif
*/
}

// Draw rectangle outline with line thickness
void DrawRectangleLinesEx(Rectangle rec, float thick, Color color)
{
    if ((thick > rec.width) || (thick > rec.height))
    {
        if (rec.width >= rec.height) thick = rec.height/2;
        else if (rec.width <= rec.height) thick = rec.width/2;
    }

    // When rec = { x, y, 8.0f, 6.0f } and thick = 2, the following
    // four rectangles are drawn ([T]op, [B]ottom, [L]eft, [R]ight):
    //
    //   TTTTTTTT
    //   TTTTTTTT
    //   LL    RR
    //   LL    RR
    //   BBBBBBBB
    //   BBBBBBBB
    //

    Rectangle top = { rec.x, rec.y, rec.width, thick };
    Rectangle bottom = { rec.x, rec.y - thick + rec.height, rec.width, thick };
    Rectangle left = { rec.x, rec.y + thick, thick, rec.height - thick*2.0f };
    Rectangle right = { rec.x - thick + rec.width, rec.y + thick, thick, rec.height - thick*2.0f };

    DrawRectangleRec(top, color);
    DrawRectangleRec(bottom, color);
    DrawRectangleRec(left, color);
    DrawRectangleRec(right, color);
}

// Draw rectangle with rounded edges (fill)
// NOTE: center-point triangle fan; four straight edges + four corner arcs
// Rounded shapes are solid-color: every vertex samples the same point of the
// shapes texture, so custom SetShapesTexture atlases also work (D6)
#define RL_ROUNDED_VERTEX(x, y) do { rlTexCoord2f(shapeU, shapeV); rlVertex2f((x), (y)); } while (0)
void DrawRectangleRounded(Rectangle rec, float roundness, int segments, Color color)
{
    if (roundness < 0.0f) roundness = 0.0f;
    if (roundness > 0.5f) roundness = 0.5f;
    if (segments <= 0) segments = 1;

    const float minSide = (rec.width < rec.height) ? rec.width : rec.height;
    const float radius = minSide*roundness;

    if (radius <= 0.0f) { DrawRectangleRec(rec, color); return; }

    // Observability hook (spec M3): rounded rect family converges here.
    // roundness rides the `radius` slot; engine has no rotation for this shape.
    // Degenerate roundness (0) already fell through to DrawRectangleRec above.
    AIDrawObserver observer = GetAIDrawObserver();
    if (observer != NULL)
    {
        observer(AI_OBS_RECT_ROUNDED, rec.x, rec.y, rec.width, rec.height, roundness, 0.0f, NULL, color);
    }

    const float x1 = rec.x, y1 = rec.y;
    const float x2 = rec.x + rec.width, y2 = rec.y + rec.height;
    const float cx = (x1 + x2)*0.5f, cy = (y1 + y2)*0.5f;

    // Bind the shapes texture (may be a custom atlas) and sample its center
    Texture2D shapeTex = GetShapesTexture();
    Rectangle shapeRect = GetShapesTextureRectangle();
    float shapeU = (shapeTex.width > 0)? ((shapeRect.x + shapeRect.width*0.5f)/(float)shapeTex.width) : 0.0f;
    float shapeV = (shapeTex.height > 0)? ((shapeRect.y + shapeRect.height*0.5f)/(float)shapeTex.height) : 0.0f;

    rlSetTexture(shapeTex.id);
    rlBegin(RL_TRIANGLES);
        rlColor4ub(color.r, color.g, color.b, color.a);

        // Four straight edges (center + edge endpoints, CCW winding like DrawCircleV)
        RL_ROUNDED_VERTEX(cx, cy); RL_ROUNDED_VERTEX(x2 - radius, y1); RL_ROUNDED_VERTEX(x1 + radius, y1);
        RL_ROUNDED_VERTEX(cx, cy); RL_ROUNDED_VERTEX(x2, y2 - radius); RL_ROUNDED_VERTEX(x2, y1 + radius);
        RL_ROUNDED_VERTEX(cx, cy); RL_ROUNDED_VERTEX(x1 + radius, y2); RL_ROUNDED_VERTEX(x2 - radius, y2);
        RL_ROUNDED_VERTEX(cx, cy); RL_ROUNDED_VERTEX(x1, y1 + radius); RL_ROUNDED_VERTEX(x1, y2 - radius);

        // Four corner arcs (center + two arc points per segment)
        // Top-left: arc center (x1+r, y1+r), angles 180..270
        for (int i = 0; i < segments; i++)
        {
            float a1 = PI + (PI/2.0f)*((float)i/segments);
            float a2 = PI + (PI/2.0f)*((float)(i + 1)/segments);
            RL_ROUNDED_VERTEX(cx, cy);
            RL_ROUNDED_VERTEX(x1 + radius + cosf(a2)*radius, y1 + radius + sinf(a2)*radius);
            RL_ROUNDED_VERTEX(x1 + radius + cosf(a1)*radius, y1 + radius + sinf(a1)*radius);
        }
        // Top-right: arc center (x2-r, y1+r), angles 270..360
        for (int i = 0; i < segments; i++)
        {
            float a1 = PI*1.5f + (PI/2.0f)*((float)i/segments);
            float a2 = PI*1.5f + (PI/2.0f)*((float)(i + 1)/segments);
            RL_ROUNDED_VERTEX(cx, cy);
            RL_ROUNDED_VERTEX(x2 - radius + cosf(a2)*radius, y1 + radius + sinf(a2)*radius);
            RL_ROUNDED_VERTEX(x2 - radius + cosf(a1)*radius, y1 + radius + sinf(a1)*radius);
        }
        // Bottom-right: arc center (x2-r, y2-r), angles 0..90
        for (int i = 0; i < segments; i++)
        {
            float a1 = (PI/2.0f)*((float)i/segments);
            float a2 = (PI/2.0f)*((float)(i + 1)/segments);
            RL_ROUNDED_VERTEX(cx, cy);
            RL_ROUNDED_VERTEX(x2 - radius + cosf(a2)*radius, y2 - radius + sinf(a2)*radius);
            RL_ROUNDED_VERTEX(x2 - radius + cosf(a1)*radius, y2 - radius + sinf(a1)*radius);
        }
        // Bottom-left: arc center (x1+r, y2-r), angles 90..180
        for (int i = 0; i < segments; i++)
        {
            float a1 = PI/2.0f + (PI/2.0f)*((float)i/segments);
            float a2 = PI/2.0f + (PI/2.0f)*((float)(i + 1)/segments);
            RL_ROUNDED_VERTEX(cx, cy);
            RL_ROUNDED_VERTEX(x1 + radius + cosf(a2)*radius, y2 - radius + sinf(a2)*radius);
            RL_ROUNDED_VERTEX(x1 + radius + cosf(a1)*radius, y2 - radius + sinf(a1)*radius);
        }
    rlEnd();
}

// Draw rectangle with rounded edges outline (with line thickness)
// NOTE: outer+inner contour strips; straight edges + corner arc segments
void DrawRectangleRoundedLines(Rectangle rec, float roundness, int segments, float lineThick, Color color)
{
    if (lineThick < 0.0f) lineThick = 0.0f;
    if (roundness < 0.0f) roundness = 0.0f;
    if (roundness > 0.5f) roundness = 0.5f;
    if (segments <= 0) segments = 1;

    const float minSide = (rec.width < rec.height) ? rec.width : rec.height;
    const float radius = minSide*roundness;

    if (radius <= 0.0f) { DrawRectangleLinesEx(rec, lineThick, color); return; }
    if (lineThick <= 0.0f) { DrawRectangleRounded(rec, roundness, segments, color); return; }

    // Observability hook (spec M3): rounded outline converges here (after the
    // degenerate branches so there is no double report). Same AI_OBS_RECT_ROUNDED
    // as the fill variant — the scene JSON does not distinguish fill/outline.
    AIDrawObserver observer = GetAIDrawObserver();
    if (observer != NULL)
    {
        observer(AI_OBS_RECT_ROUNDED, rec.x, rec.y, rec.width, rec.height, roundness, 0.0f, NULL, color);
    }

    const float x1 = rec.x, y1 = rec.y;
    const float x2 = rec.x + rec.width, y2 = rec.y + rec.height;
    // Inner radius must never go negative: an oversized line thickness would
    // otherwise produce inverted/invalid arcs (D5)
    const float rIn = (radius > lineThick) ? (radius - lineThick) : 0.0f;

    // Bind the shapes texture (may be a custom atlas) and sample its center
    Texture2D shapeTex = GetShapesTexture();
    Rectangle shapeRect = GetShapesTextureRectangle();
    float shapeU = (shapeTex.width > 0)? ((shapeRect.x + shapeRect.width*0.5f)/(float)shapeTex.width) : 0.0f;
    float shapeV = (shapeTex.height > 0)? ((shapeRect.y + shapeRect.height*0.5f)/(float)shapeTex.height) : 0.0f;

    rlSetTexture(shapeTex.id);
    rlBegin(RL_TRIANGLES);
        rlColor4ub(color.r, color.g, color.b, color.a);

        // Outline strips. Winding verified per-triangle so every triangle has a
        // negative cross product (visible side, same convention as DrawCircleV).
        // Top edge (outer y1, inner y1+th)
        RL_ROUNDED_VERTEX(x1 + radius, y1); RL_ROUNDED_VERTEX(x1 + radius, y1 + lineThick); RL_ROUNDED_VERTEX(x2 - radius, y1);
        RL_ROUNDED_VERTEX(x2 - radius, y1); RL_ROUNDED_VERTEX(x1 + radius, y1 + lineThick); RL_ROUNDED_VERTEX(x2 - radius, y1 + lineThick);
        // Bottom edge (outer y2, inner y2-th)
        RL_ROUNDED_VERTEX(x1 + radius, y2); RL_ROUNDED_VERTEX(x2 - radius, y2); RL_ROUNDED_VERTEX(x1 + radius, y2 - lineThick);
        RL_ROUNDED_VERTEX(x2 - radius, y2); RL_ROUNDED_VERTEX(x2 - radius, y2 - lineThick); RL_ROUNDED_VERTEX(x1 + radius, y2 - lineThick);
        // Left edge (outer x1, inner x1+th)
        RL_ROUNDED_VERTEX(x1, y1 + radius); RL_ROUNDED_VERTEX(x1, y2 - radius); RL_ROUNDED_VERTEX(x1 + lineThick, y1 + radius);
        RL_ROUNDED_VERTEX(x1 + lineThick, y1 + radius); RL_ROUNDED_VERTEX(x1 + lineThick, y2 - radius); RL_ROUNDED_VERTEX(x1, y2 - radius);
        // Right edge (outer x2, inner x2-th)
        RL_ROUNDED_VERTEX(x2, y1 + radius); RL_ROUNDED_VERTEX(x2 - lineThick, y1 + radius); RL_ROUNDED_VERTEX(x2, y2 - radius);
        RL_ROUNDED_VERTEX(x2, y2 - radius); RL_ROUNDED_VERTEX(x2 - lineThick, y1 + radius); RL_ROUNDED_VERTEX(x2 - lineThick, y2 - radius);

        // Corner arcs: per segment triangle (outer1, inner1, outer2) + (inner1, inner2, outer2)
        // Top-left (angles 180..270)
        for (int i = 0; i < segments; i++)
        {
            float a1 = PI + (PI/2.0f)*((float)i/segments);
            float a2 = PI + (PI/2.0f)*((float)(i + 1)/segments);
            float ox1 = x1 + radius + cosf(a1)*radius, oy1 = y1 + radius + sinf(a1)*radius;
            float ox2 = x1 + radius + cosf(a2)*radius, oy2 = y1 + radius + sinf(a2)*radius;
            float ix1 = x1 + rIn + cosf(a1)*rIn, iy1 = y1 + rIn + sinf(a1)*rIn;
            float ix2 = x1 + rIn + cosf(a2)*rIn, iy2 = y1 + rIn + sinf(a2)*rIn;
            RL_ROUNDED_VERTEX(ix1, iy1); RL_ROUNDED_VERTEX(ox1, oy1); RL_ROUNDED_VERTEX(ix2, iy2);
            RL_ROUNDED_VERTEX(ox1, oy1); RL_ROUNDED_VERTEX(ox2, oy2); RL_ROUNDED_VERTEX(ix2, iy2);
        }
        // Top-right (angles 270..360)
        for (int i = 0; i < segments; i++)
        {
            float a1 = PI*1.5f + (PI/2.0f)*((float)i/segments);
            float a2 = PI*1.5f + (PI/2.0f)*((float)(i + 1)/segments);
            float ox1 = x2 - radius + cosf(a1)*radius, oy1 = y1 + radius + sinf(a1)*radius;
            float ox2 = x2 - radius + cosf(a2)*radius, oy2 = y1 + radius + sinf(a2)*radius;
            float ix1 = x2 - rIn + cosf(a1)*rIn, iy1 = y1 + rIn + sinf(a1)*rIn;
            float ix2 = x2 - rIn + cosf(a2)*rIn, iy2 = y1 + rIn + sinf(a2)*rIn;
            RL_ROUNDED_VERTEX(ix1, iy1); RL_ROUNDED_VERTEX(ox1, oy1); RL_ROUNDED_VERTEX(ix2, iy2);
            RL_ROUNDED_VERTEX(ox1, oy1); RL_ROUNDED_VERTEX(ox2, oy2); RL_ROUNDED_VERTEX(ix2, iy2);
        }
        // Bottom-right (angles 0..90)
        for (int i = 0; i < segments; i++)
        {
            float a1 = (PI/2.0f)*((float)i/segments);
            float a2 = (PI/2.0f)*((float)(i + 1)/segments);
            float ox1 = x2 - radius + cosf(a1)*radius, oy1 = y2 - radius + sinf(a1)*radius;
            float ox2 = x2 - radius + cosf(a2)*radius, oy2 = y2 - radius + sinf(a2)*radius;
            float ix1 = x2 - rIn + cosf(a1)*rIn, iy1 = y2 - rIn + sinf(a1)*rIn;
            float ix2 = x2 - rIn + cosf(a2)*rIn, iy2 = y2 - rIn + sinf(a2)*rIn;
            RL_ROUNDED_VERTEX(ix1, iy1); RL_ROUNDED_VERTEX(ox1, oy1); RL_ROUNDED_VERTEX(ix2, iy2);
            RL_ROUNDED_VERTEX(ox1, oy1); RL_ROUNDED_VERTEX(ox2, oy2); RL_ROUNDED_VERTEX(ix2, iy2);
        }
        // Bottom-left (angles 90..180)
        for (int i = 0; i < segments; i++)
        {
            float a1 = PI/2.0f + (PI/2.0f)*((float)i/segments);
            float a2 = PI/2.0f + (PI/2.0f)*((float)(i + 1)/segments);
            float ox1 = x1 + radius + cosf(a1)*radius, oy1 = y2 - radius + sinf(a1)*radius;
            float ox2 = x1 + radius + cosf(a2)*radius, oy2 = y2 - radius + sinf(a2)*radius;
            float ix1 = x1 + rIn + cosf(a1)*rIn, iy1 = y2 - rIn + sinf(a1)*rIn;
            float ix2 = x1 + rIn + cosf(a2)*rIn, iy2 = y2 - rIn + sinf(a2)*rIn;
            RL_ROUNDED_VERTEX(ix1, iy1); RL_ROUNDED_VERTEX(ox1, oy1); RL_ROUNDED_VERTEX(ix2, iy2);
            RL_ROUNDED_VERTEX(ox1, oy1); RL_ROUNDED_VERTEX(ox2, oy2); RL_ROUNDED_VERTEX(ix2, iy2);
        }
    rlEnd();
}
#undef RL_ROUNDED_VERTEX

// Draw a polygon of n sides
void DrawPoly(Vector2 center, int sides, float radius, float rotation, Color color)
{
    if (sides < 3) sides = 3;
    float centralAngle = rotation*DEG2RAD;
    float angleStep = 360.0f/(float)sides*DEG2RAD;

#if SUPPORT_QUADS_DRAW_MODE
    rlSetTexture(GetShapesTexture().id);
    Rectangle shapeRect = GetShapesTextureRectangle();

    rlBegin(RL_QUADS);
        for (int i = 0; i < sides; i++)
        {
            rlColor4ub(color.r, color.g, color.b, color.a);
            float nextAngle = centralAngle + angleStep;

            rlTexCoord2f(shapeRect.x/texShapes.width, shapeRect.y/texShapes.height);
            rlVertex2f(center.x, center.y);

            rlTexCoord2f(shapeRect.x/texShapes.width, (shapeRect.y + shapeRect.height)/texShapes.height);
            rlVertex2f(center.x + cosf(centralAngle)*radius, center.y + sinf(centralAngle)*radius);

            rlTexCoord2f((shapeRect.x + shapeRect.width)/texShapes.width, shapeRect.y/texShapes.height);
            rlVertex2f(center.x + cosf(nextAngle)*radius, center.y + sinf(nextAngle)*radius);

            rlTexCoord2f((shapeRect.x + shapeRect.width)/texShapes.width, (shapeRect.y + shapeRect.height)/texShapes.height);
            rlVertex2f(center.x + cosf(centralAngle)*radius, center.y + sinf(centralAngle)*radius);

            centralAngle = nextAngle;
        }
    rlEnd();
    rlSetTexture(0);
#else
    rlBegin(RL_TRIANGLES);
        for (int i = 0; i < sides; i++)
        {
            rlColor4ub(color.r, color.g, color.b, color.a);

            rlVertex2f(center.x, center.y);
            rlVertex2f(center.x + cosf(centralAngle + angleStep)*radius, center.y + sinf(centralAngle + angleStep)*radius);
            rlVertex2f(center.x + cosf(centralAngle)*radius, center.y + sinf(centralAngle)*radius);

            centralAngle += angleStep;
        }
    rlEnd();
#endif
}

// Draw a polygon outline of n sides
void DrawPolyLines(Vector2 center, int sides, float radius, float rotation, Color color)
{
    if (sides < 3) sides = 3;
    float centralAngle = rotation*DEG2RAD;
    float angleStep = 360.0f/(float)sides*DEG2RAD;

    rlBegin(RL_LINES);
        for (int i = 0; i < sides; i++)
        {
            rlColor4ub(color.r, color.g, color.b, color.a);

            rlVertex2f(center.x + cosf(centralAngle)*radius, center.y + sinf(centralAngle)*radius);
            rlVertex2f(center.x + cosf(centralAngle + angleStep)*radius, center.y + sinf(centralAngle + angleStep)*radius);

            centralAngle += angleStep;
        }
    rlEnd();
}

void DrawPolyLinesEx(Vector2 center, int sides, float radius, float rotation, float thick, Color color)
{
    if (sides < 3) sides = 3;
    float centralAngle = rotation*DEG2RAD;
    float exteriorAngle = 360.0f/(float)sides*DEG2RAD;
    float innerRadius = radius - (thick*cosf(DEG2RAD*exteriorAngle/2.0f));

#if SUPPORT_QUADS_DRAW_MODE
    rlSetTexture(GetShapesTexture().id);
    Rectangle shapeRect = GetShapesTextureRectangle();

    rlBegin(RL_QUADS);
        for (int i = 0; i < sides; i++)
        {
            rlColor4ub(color.r, color.g, color.b, color.a);
            float nextAngle = centralAngle + exteriorAngle;

            rlTexCoord2f(shapeRect.x/texShapes.width, (shapeRect.y + shapeRect.height)/texShapes.height);
            rlVertex2f(center.x + cosf(centralAngle)*radius, center.y + sinf(centralAngle)*radius);

            rlTexCoord2f(shapeRect.x/texShapes.width, shapeRect.y/texShapes.height);
            rlVertex2f(center.x + cosf(centralAngle)*innerRadius, center.y + sinf(centralAngle)*innerRadius);

            rlTexCoord2f((shapeRect.x + shapeRect.width)/texShapes.width, (shapeRect.y + shapeRect.height)/texShapes.height);
            rlVertex2f(center.x + cosf(nextAngle)*innerRadius, center.y + sinf(nextAngle)*innerRadius);

            rlTexCoord2f((shapeRect.x + shapeRect.width)/texShapes.width, shapeRect.y/texShapes.height);
            rlVertex2f(center.x + cosf(nextAngle)*radius, center.y + sinf(nextAngle)*radius);

            centralAngle = nextAngle;
        }
    rlEnd();
    rlSetTexture(0);
#else
    rlBegin(RL_TRIANGLES);
        for (int i = 0; i < sides; i++)
        {
            rlColor4ub(color.r, color.g, color.b, color.a);
            float nextAngle = centralAngle + exteriorAngle;

            rlVertex2f(center.x + cosf(nextAngle)*radius, center.y + sinf(nextAngle)*radius);
            rlVertex2f(center.x + cosf(centralAngle)*radius, center.y + sinf(centralAngle)*radius);
            rlVertex2f(center.x + cosf(centralAngle)*innerRadius, center.y + sinf(centralAngle)*innerRadius);

            rlVertex2f(center.x + cosf(centralAngle)*innerRadius, center.y + sinf(centralAngle)*innerRadius);
            rlVertex2f(center.x + cosf(nextAngle)*innerRadius, center.y + sinf(nextAngle)*innerRadius);
            rlVertex2f(center.x + cosf(nextAngle)*radius, center.y + sinf(nextAngle)*radius);

            centralAngle = nextAngle;
        }
    rlEnd();
#endif
}

// Draw a color-filled circle
void DrawCircle(int centerX, int centerY, float radius, Color color)
{
    DrawCircleV((Vector2){ (float)centerX, (float)centerY }, radius, color);
}

// Draw a color-filled circle (Vector version)
// NOTE: On OpenGL 3.3 and ES2 using QUADS to avoid drawing order issues
void DrawCircleV(Vector2 center, float radius, Color color)
{
    // Non-invasive observability hook (spec M3): the circle family
    // (DrawCircle/V) converges here. NULL observer = one compare-and-branch.
    AIDrawObserver observer = GetAIDrawObserver();
    if (observer != NULL)
    {
        observer(AI_OBS_CIRCLE, center.x, center.y, 0.0f, 0.0f, radius, 0.0f, NULL, color);
    }

    rlBegin(RL_TRIANGLES);
        for (int i = 0; i < 360; i += 10)
        {
            rlColor4ub(color.r, color.g, color.b, color.a);
            rlVertex2f(center.x, center.y);
            rlVertex2f(center.x + cosf(DEG2RAD*(i + 10))*radius, center.y + sinf(DEG2RAD*(i + 10))*radius);
            rlVertex2f(center.x + cosf(DEG2RAD*i)*radius, center.y + sinf(DEG2RAD*i)*radius);
        }
    rlEnd();
}

// Draw circle outline
void DrawCircleLines(int centerX, int centerY, float radius, Color color)
{
    DrawCircleLinesV((Vector2){ (float)centerX, (float)centerY }, radius, color);
}

// Draw circle outline (Vector version)
void DrawCircleLinesV(Vector2 center, float radius, Color color)
{
    rlBegin(RL_LINES);
        rlColor4ub(color.r, color.g, color.b, color.a);

        // NOTE: Circle outline is drawn pixel by pixel every degree (0 to 360)
        for (int i = 0; i < 360; i += 10)
        {
            rlVertex2f(center.x + cosf(DEG2RAD*i)*radius, center.y + sinf(DEG2RAD*i)*radius);
            rlVertex2f(center.x + cosf(DEG2RAD*(i + 10))*radius, center.y + sinf(DEG2RAD*(i + 10))*radius);
        }
    rlEnd();
}

void DrawCircleLinesEx(Vector2 center, float radius, float thick, Color color)
{
    DrawRing(center, radius - thick, radius, 0.0f, 360.0f, 36, color);
}

// Draw a piece of a circle (fill)
// NOTE: center-point triangle fan over [startAngle, endAngle]
void DrawCircleSector(Vector2 center, float radius, float startAngle, float endAngle, int segments, Color color)
{
    if (radius <= 0.0f) return;
    if (segments < 2) segments = 2;
    if (endAngle < startAngle) endAngle += 360.0f;

    // Observability hook (spec M3): sector family converges here, before the
    // full-circle fallback, so >=360deg sectors report AI_OBS_CIRCLE_SECTOR once
    // (never degraded to AI_OBS_CIRCLE). startAngle rides `rotation`, endAngle `w`.
    AIDrawObserver observer = GetAIDrawObserver();
    if (observer != NULL)
    {
        observer(AI_OBS_CIRCLE_SECTOR, center.x, center.y, endAngle, 0.0f, radius, startAngle, NULL, color);
    }

    if ((endAngle - startAngle) >= 360.0f)
    {
        // Full-circle fallback inlined (instead of calling DrawCircleV) so the
        // observer reports AI_OBS_CIRCLE_SECTOR exactly once — DrawCircleV has
        // its own hook that would otherwise double-report.
        rlBegin(RL_TRIANGLES);
            for (int i = 0; i < 360; i += 10)
            {
                rlColor4ub(color.r, color.g, color.b, color.a);
                rlVertex2f(center.x, center.y);
                rlVertex2f(center.x + cosf(DEG2RAD*(i + 10))*radius, center.y + sinf(DEG2RAD*(i + 10))*radius);
                rlVertex2f(center.x + cosf(DEG2RAD*i)*radius, center.y + sinf(DEG2RAD*i)*radius);
            }
        rlEnd();
        return;
    }

    float startRad = startAngle*DEG2RAD;
    float step = (endAngle - startAngle)*DEG2RAD/segments;

    rlBegin(RL_TRIANGLES);
        rlColor4ub(color.r, color.g, color.b, color.a);
        for (int i = 0; i < segments; i++)
        {
            float a1 = startRad + i*step;
            float a2 = startRad + (i + 1)*step;
            rlVertex2f(center.x, center.y);
            rlVertex2f(center.x + cosf(a2)*radius, center.y + sinf(a2)*radius);
            rlVertex2f(center.x + cosf(a1)*radius, center.y + sinf(a1)*radius);
        }
    rlEnd();
}

// Draw a piece of a circle outline (two radius lines + arc)
void DrawCircleSectorLines(Vector2 center, float radius, float startAngle, float endAngle, int segments, Color color)
{
    if (radius <= 0.0f) return;
    if (segments < 2) segments = 2;
    if (endAngle < startAngle) endAngle += 360.0f;

    // Observability hook (spec M3): sector outline converges here (no fallback
    // path); same AI_OBS_CIRCLE_SECTOR as the fill variant.
    AIDrawObserver observer = GetAIDrawObserver();
    if (observer != NULL)
    {
        observer(AI_OBS_CIRCLE_SECTOR, center.x, center.y, endAngle, 0.0f, radius, startAngle, NULL, color);
    }

    float startRad = startAngle*DEG2RAD;
    float endRad = endAngle*DEG2RAD;
    float step = (endRad - startRad)/segments;

    rlBegin(RL_LINES);
        rlColor4ub(color.r, color.g, color.b, color.a);

        // Two radius lines
        rlVertex2f(center.x, center.y);
        rlVertex2f(center.x + cosf(startRad)*radius, center.y + sinf(startRad)*radius);
        rlVertex2f(center.x, center.y);
        rlVertex2f(center.x + cosf(endRad)*radius, center.y + sinf(endRad)*radius);

        // Arc segments
        for (int i = 0; i < segments; i++)
        {
            float a1 = startRad + i*step;
            float a2 = startRad + (i + 1)*step;
            rlVertex2f(center.x + cosf(a1)*radius, center.y + sinf(a1)*radius);
            rlVertex2f(center.x + cosf(a2)*radius, center.y + sinf(a2)*radius);
        }
    rlEnd();
}

// Draw ellipse
void DrawEllipse(int centerX, int centerY, float radiusH, float radiusV, Color color)
{
    DrawEllipseV((Vector2){ (float)centerX, (float)centerY }, radiusH, radiusV, color);
}

// Draw ellipse (Vector version)
void DrawEllipseV(Vector2 center, float radiusH, float radiusV, Color color)
{
    rlBegin(RL_TRIANGLES);
        for (int i = 0; i < 360; i += 10)
        {
            rlColor4ub(color.r, color.g, color.b, color.a);
            rlVertex2f(center.x,  center.y);
            rlVertex2f(center.x + cosf(DEG2RAD*(i + 10))*radiusH, center.y + sinf(DEG2RAD*(i + 10))*radiusV);
            rlVertex2f(center.x + cosf(DEG2RAD*i)*radiusH, center.y + sinf(DEG2RAD*i)*radiusV);
        }
    rlEnd();
}

// Draw ellipse outline
void DrawEllipseLines(int centerX, int centerY, float radiusH, float radiusV, Color color)
{
    DrawEllipseLinesV((Vector2){ (float)centerX, (float)centerY }, radiusH, radiusV, color);
}

// Draw ellipse outline
void DrawEllipseLinesV(Vector2 center, float radiusH, float radiusV, Color color)
{
    rlBegin(RL_LINES);
        for (int i = 0; i < 360; i += 10)
        {
            rlColor4ub(color.r, color.g, color.b, color.a);
            rlVertex2f(center.x + cosf(DEG2RAD*(i + 10))*radiusH, center.y + sinf(DEG2RAD*(i + 10))*radiusV);
            rlVertex2f(center.x + cosf(DEG2RAD*i)*radiusH, center.y + sinf(DEG2RAD*i)*radiusV);
        }
    rlEnd();
}

// Draw ring
void DrawRing(Vector2 center, float innerRadius, float outerRadius, float startAngle, float endAngle, int segments, Color color)
{
    if (startAngle == endAngle) return;

    // Function expects (outerRadius > innerRadius)
    if (outerRadius < innerRadius)
    {
        float tmp = outerRadius;
        outerRadius = innerRadius;
        innerRadius = tmp;

        if (outerRadius <= 0.0f) outerRadius = 0.1f;
    }

    // Function expects (endAngle > startAngle)
    if (endAngle < startAngle)
    {
        // Swap values
        float tmp = startAngle;
        startAngle = endAngle;
        endAngle = tmp;
    }

    int minSegments = (int)ceilf((endAngle - startAngle)/90);

    if (segments < minSegments)
    {
        // Calculate the maximum angle between segments based on the error rate (usually 0.5f)
        float th = acosf(2*powf(1 - SMOOTH_CIRCLE_ERROR_RATE/outerRadius, 2) - 1);
        segments = (int)ceilf((endAngle - startAngle)*(2*PI/th)/360.0f);

        if (segments <= 0) segments = minSegments;
    }

    // Not a ring
    if (innerRadius <= 0.0f)
    {
        // Draw as a filled sector directly
        float stepLength = (endAngle - startAngle)/(float)segments;
        float angle = startAngle;
        rlBegin(RL_TRIANGLES);
            for (int i = 0; i < segments; i++)
            {
                rlColor4ub(color.r, color.g, color.b, color.a);
                rlVertex2f(center.x, center.y);
                rlVertex2f(center.x + cosf(DEG2RAD*(angle + stepLength))*outerRadius, center.y + sinf(DEG2RAD*(angle + stepLength))*outerRadius);
                rlVertex2f(center.x + cosf(DEG2RAD*angle)*outerRadius, center.y + sinf(DEG2RAD*angle)*outerRadius);
                angle += stepLength;
            }
        rlEnd();
        return;
    }

    float stepLength = (endAngle - startAngle)/(float)segments;
    float angle = startAngle;

#if SUPPORT_QUADS_DRAW_MODE
    rlSetTexture(GetShapesTexture().id);
    Rectangle shapeRect = GetShapesTextureRectangle();

    rlBegin(RL_QUADS);
        for (int i = 0; i < segments; i++)
        {
            rlColor4ub(color.r, color.g, color.b, color.a);

            rlTexCoord2f(shapeRect.x/texShapes.width, (shapeRect.y + shapeRect.height)/texShapes.height);
            rlVertex2f(center.x + cosf(DEG2RAD*angle)*outerRadius, center.y + sinf(DEG2RAD*angle)*outerRadius);

            rlTexCoord2f(shapeRect.x/texShapes.width, shapeRect.y/texShapes.height);
            rlVertex2f(center.x + cosf(DEG2RAD*angle)*innerRadius, center.y + sinf(DEG2RAD*angle)*innerRadius);

            rlTexCoord2f((shapeRect.x + shapeRect.width)/texShapes.width, shapeRect.y/texShapes.height);
            rlVertex2f(center.x + cosf(DEG2RAD*(angle + stepLength))*innerRadius, center.y + sinf(DEG2RAD*(angle + stepLength))*innerRadius);

            rlTexCoord2f((shapeRect.x + shapeRect.width)/texShapes.width, (shapeRect.y + shapeRect.height)/texShapes.height);
            rlVertex2f(center.x + cosf(DEG2RAD*(angle + stepLength))*outerRadius, center.y + sinf(DEG2RAD*(angle + stepLength))*outerRadius);

            angle += stepLength;
        }
    rlEnd();

    rlSetTexture(0);
#else
    rlBegin(RL_TRIANGLES);
        for (int i = 0; i < segments; i++)
        {
            rlColor4ub(color.r, color.g, color.b, color.a);

            rlVertex2f(center.x + cosf(DEG2RAD*angle)*innerRadius, center.y + sinf(DEG2RAD*angle)*innerRadius);
            rlVertex2f(center.x + cosf(DEG2RAD*(angle + stepLength))*innerRadius, center.y + sinf(DEG2RAD*(angle + stepLength))*innerRadius);
            rlVertex2f(center.x + cosf(DEG2RAD*angle)*outerRadius, center.y + sinf(DEG2RAD*angle)*outerRadius);

            rlVertex2f(center.x + cosf(DEG2RAD*(angle + stepLength))*innerRadius, center.y + sinf(DEG2RAD*(angle + stepLength))*innerRadius);
            rlVertex2f(center.x + cosf(DEG2RAD*(angle + stepLength))*outerRadius, center.y + sinf(DEG2RAD*(angle + stepLength))*outerRadius);
            rlVertex2f(center.x + cosf(DEG2RAD*angle)*outerRadius, center.y + sinf(DEG2RAD*angle)*outerRadius);

            angle += stepLength;
        }
    rlEnd();
#endif
}

// Draw ring outline
void DrawRingLines(Vector2 center, float innerRadius, float outerRadius, float startAngle, float endAngle, int segments, Color color)
{
    if (startAngle == endAngle) return;

    // Function expects (outerRadius > innerRadius)
    if (outerRadius < innerRadius)
    {
        float tmp = outerRadius;
        outerRadius = innerRadius;
        innerRadius = tmp;

        if (outerRadius <= 0.0f) outerRadius = 0.1f;
    }

    // Function expects (endAngle > startAngle)
    if (endAngle < startAngle)
    {
        // Swap values
        float tmp = startAngle;
        startAngle = endAngle;
        endAngle = tmp;
    }

    int minSegments = (int)ceilf((endAngle - startAngle)/90);

    if (segments < minSegments)
    {
        // Calculate the maximum angle between segments based on the error rate (usually 0.5f)
        float th = acosf(2*powf(1 - SMOOTH_CIRCLE_ERROR_RATE/outerRadius, 2) - 1);
        segments = (int)ceilf((endAngle - startAngle)*(2*PI/th)/360.0f);

        if (segments <= 0) segments = minSegments;
    }

    if (innerRadius <= 0.0f)
    {
        // Draw as a sector outline directly
        float stepLength = (endAngle - startAngle)/(float)segments;
        float angle = startAngle;
        bool showCapLines = true;
        rlBegin(RL_LINES);
            if (showCapLines)
            {
                rlColor4ub(color.r, color.g, color.b, color.a);
                rlVertex2f(center.x, center.y);
                rlVertex2f(center.x + cosf(DEG2RAD*angle)*outerRadius, center.y + sinf(DEG2RAD*angle)*outerRadius);
            }

            for (int i = 0; i < segments; i++)
            {
                rlColor4ub(color.r, color.g, color.b, color.a);
                rlVertex2f(center.x + cosf(DEG2RAD*angle)*outerRadius, center.y + sinf(DEG2RAD*angle)*outerRadius);
                rlVertex2f(center.x + cosf(DEG2RAD*(angle + stepLength))*outerRadius, center.y + sinf(DEG2RAD*(angle + stepLength))*outerRadius);
                angle += stepLength;
            }

            if (showCapLines)
            {
                rlColor4ub(color.r, color.g, color.b, color.a);
                rlVertex2f(center.x, center.y);
                rlVertex2f(center.x + cosf(DEG2RAD*angle)*outerRadius, center.y + sinf(DEG2RAD*angle)*outerRadius);
            }
        rlEnd();
        return;
    }

    float stepLength = (endAngle - startAngle)/(float)segments;
    float angle = startAngle;
    bool showCapLines = true;

    rlBegin(RL_LINES);
        if (showCapLines)
        {
            rlColor4ub(color.r, color.g, color.b, color.a);
            rlVertex2f(center.x + cosf(DEG2RAD*angle)*outerRadius, center.y + sinf(DEG2RAD*angle)*outerRadius);
            rlVertex2f(center.x + cosf(DEG2RAD*angle)*innerRadius, center.y + sinf(DEG2RAD*angle)*innerRadius);
        }

        for (int i = 0; i < segments; i++)
        {
            rlColor4ub(color.r, color.g, color.b, color.a);

            rlVertex2f(center.x + cosf(DEG2RAD*angle)*outerRadius, center.y + sinf(DEG2RAD*angle)*outerRadius);
            rlVertex2f(center.x + cosf(DEG2RAD*(angle + stepLength))*outerRadius, center.y + sinf(DEG2RAD*(angle + stepLength))*outerRadius);

            rlVertex2f(center.x + cosf(DEG2RAD*angle)*innerRadius, center.y + sinf(DEG2RAD*angle)*innerRadius);
            rlVertex2f(center.x + cosf(DEG2RAD*(angle + stepLength))*innerRadius, center.y + sinf(DEG2RAD*(angle + stepLength))*innerRadius);

            angle += stepLength;
        }

        if (showCapLines)
        {
            rlColor4ub(color.r, color.g, color.b, color.a);
            rlVertex2f(center.x + cosf(DEG2RAD*angle)*outerRadius, center.y + sinf(DEG2RAD*angle)*outerRadius);
            rlVertex2f(center.x + cosf(DEG2RAD*angle)*innerRadius, center.y + sinf(DEG2RAD*angle)*innerRadius);
        }
    rlEnd();
}

//----------------------------------------------------------------------------------
// Module Functions Definition - Collision Detection functions
//----------------------------------------------------------------------------------

// Check if point is inside rectangle
bool CheckCollisionPointRec(Vector2 point, Rectangle rec)
{
    bool collision = false;

    if ((point.x >= rec.x) && (point.x < (rec.x + rec.width)) && (point.y >= rec.y) && (point.y < (rec.y + rec.height))) collision = true;

    return collision;
}

// Check if point is inside circle
bool CheckCollisionPointCircle(Vector2 point, Vector2 center, float radius)
{
    bool collision = false;

    float distanceSquared = (point.x - center.x)*(point.x - center.x) + (point.y - center.y)*(point.y - center.y);

    if (distanceSquared <= radius*radius) collision = true;

    return collision;
}

// Check if point is inside a triangle defined by three points (p1, p2, p3)
bool CheckCollisionPointTriangle(Vector2 point, Vector2 p1, Vector2 p2, Vector2 p3)
{
    bool collision = false;

    float alpha = ((p2.y - p3.y)*(point.x - p3.x) + (p3.x - p2.x)*(point.y - p3.y)) /
                  ((p2.y - p3.y)*(p1.x - p3.x) + (p3.x - p2.x)*(p1.y - p3.y));

    float beta = ((p3.y - p1.y)*(point.x - p3.x) + (p1.x - p3.x)*(point.y - p3.y)) /
                 ((p2.y - p3.y)*(p1.x - p3.x) + (p3.x - p2.x)*(p1.y - p3.y));

    float gamma = 1.0f - alpha - beta;

    if ((alpha > 0) && (beta > 0) && (gamma > 0)) collision = true;

    return collision;
}

// Check if point is within a polygon described by array of vertices
// NOTE: Based on http://jeffreythompson.org/collision-detection/poly-point.php
bool CheckCollisionPointPoly(Vector2 point, const Vector2 *points, int pointCount)
{
    bool collision = false;

    if (pointCount > 2)
    {
        for (int i = 0, j = pointCount - 1; i < pointCount; j = i++)
        {
            if ((points[i].y > point.y) != (points[j].y > point.y) &&
                (point.x < (points[j].x - points[i].x)*(point.y - points[i].y)/(points[j].y - points[i].y) + points[i].x))
            {
                collision = !collision;
            }
        }
    }

    return collision;
}

// Check collision between two rectangles
bool CheckCollisionRecs(Rectangle rec1, Rectangle rec2)
{
    bool collision = false;

    if ((rec1.x < (rec2.x + rec2.width) && (rec1.x + rec1.width) > rec2.x) &&
        (rec1.y < (rec2.y + rec2.height) && (rec1.y + rec1.height) > rec2.y)) collision = true;

    return collision;
}

// Check collision between two circles
bool CheckCollisionCircles(Vector2 center1, float radius1, Vector2 center2, float radius2)
{
    bool collision = false;

    float dx = center2.x - center1.x;      // X distance between centers
    float dy = center2.y - center1.y;      // Y distance between centers

    float distanceSquared = dx*dx + dy*dy; // Distance between centers squared
    float radiusSum = radius1 + radius2;

    collision = (distanceSquared <= (radiusSum*radiusSum));

    return collision;
}

// Check collision between circle and rectangle
// NOTE: Reviewed version to take into account corner limit case
bool CheckCollisionCircleRec(Vector2 center, float radius, Rectangle rec)
{
    bool collision = false;

    float recCenterX = rec.x + rec.width/2.0f;
    float recCenterY = rec.y + rec.height/2.0f;

    float dx = fabsf(center.x - recCenterX);
    float dy = fabsf(center.y - recCenterY);

    if ((dx <= (rec.width/2.0f + radius)) && (dy <= (rec.height/2.0f + radius)))
    {
        if (dx <= (rec.width/2.0f)) collision = true;
        else if (dy <= (rec.height/2.0f)) collision = true;
        else
        {
            float cornerDistanceSq = (dx - rec.width/2.0f)*(dx - rec.width/2.0f) +
                (dy - rec.height/2.0f)*(dy - rec.height/2.0f);

            collision = (cornerDistanceSq <= (radius*radius));
        }
    }

    return collision;
}

// Check the collision between two lines defined by two points each, returns collision point by reference
// REF: https://en.wikipedia.org/wiki/Line–line_intersection#Given_two_points_on_each_line_segment
bool CheckCollisionLines(Vector2 startPos1, Vector2 endPos1, Vector2 startPos2, Vector2 endPos2, Vector2 *collisionPoint)
{
    bool collision = false;

    float rx = endPos1.x - startPos1.x;
    float ry = endPos1.y - startPos1.y;
    float sx = endPos2.x - startPos2.x;
    float sy = endPos2.y - startPos2.y;

    float div = rx*sy - ry*sx;

    if (fabsf(div) >= FLT_EPSILON)
    {
        float s12x = startPos2.x - startPos1.x;
        float s12y = startPos2.y - startPos1.y;

        float t = (s12x*sy - s12y*sx)/div;
        float u = (s12x*ry - s12y*rx)/div;

        if ((0.0f <= t) && (t <= 1.0f) && (0.0f <= u) && (u <= 1.0f))
        {
            if (collisionPoint)
            {
                collisionPoint->x = startPos1.x + t*rx;
                collisionPoint->y = startPos1.y + t*ry;
            }

            collision = true;
        }
    }

    return collision;
}

// Check if point belongs to line created between two points [p1] and [p2] with defined margin in pixels [threshold]
bool CheckCollisionPointLine(Vector2 point, Vector2 p1, Vector2 p2, int threshold)
{
    bool collision = false;

    float dxc = point.x - p1.x;
    float dyc = point.y - p1.y;
    float dxl = p2.x - p1.x;
    float dyl = p2.y - p1.y;
    float cross = dxc*dyl - dyc*dxl;

    if (fabsf(cross) < (threshold*fmaxf(fabsf(dxl), fabsf(dyl))))
    {
        if (fabsf(dxl) >= fabsf(dyl)) collision = (dxl > 0)? ((p1.x <= point.x) && (point.x <= p2.x)) : ((p2.x <= point.x) && (point.x <= p1.x));
        else collision = (dyl > 0)? ((p1.y <= point.y) && (point.y <= p2.y)) : ((p2.y <= point.y) && (point.y <= p1.y));
    }

    return collision;
}

// Check if circle collides with a line created between two points [p1] and [p2]
bool CheckCollisionCircleLine(Vector2 center, float radius, Vector2 p1, Vector2 p2)
{
    bool collision = false;

    float dx = p1.x - p2.x;
    float dy = p1.y - p2.y;

    if ((fabsf(dx) + fabsf(dy)) <= FLT_EPSILON)
    {
        collision = CheckCollisionCircles(p1, 0, center, radius);
    }
    else
    {
        float lengthSQ = ((dx*dx) + (dy*dy));
        float dotProduct = (((center.x - p1.x)*(p2.x - p1.x)) + ((center.y - p1.y)*(p2.y - p1.y)))/(lengthSQ);

        if (dotProduct > 1.0f) dotProduct = 1.0f;
        else if (dotProduct < 0.0f) dotProduct = 0.0f;

        float dx2 = (p1.x - (dotProduct*(dx))) - center.x;
        float dy2 = (p1.y - (dotProduct*(dy))) - center.y;
        float distanceSQ = ((dx2*dx2) + (dy2*dy2));

        if (distanceSQ <= radius*radius) collision = true;
    }

    return collision;
}

// Get collision rectangle for two rectangles collision
Rectangle GetCollisionRec(Rectangle rec1, Rectangle rec2)
{
    Rectangle overlap = { 0 };

    float left = (rec1.x > rec2.x)? rec1.x : rec2.x;
    float right1 = rec1.x + rec1.width;
    float right2 = rec2.x + rec2.width;
    float right = (right1 < right2)? right1 : right2;
    float top = (rec1.y > rec2.y)? rec1.y : rec2.y;
    float bottom1 = rec1.y + rec1.height;
    float bottom2 = rec2.y + rec2.height;
    float bottom = (bottom1 < bottom2)? bottom1 : bottom2;

    if ((left < right) && (top < bottom))
    {
        overlap.x = left;
        overlap.y = top;
        overlap.width = right - left;
        overlap.height = bottom - top;
    }

    return overlap;
}

//----------------------------------------------------------------------------------
// Module Internal Functions Definition
//----------------------------------------------------------------------------------

// Cubic easing in-out
// NOTE: Used by DrawLineBezier() only
static float EaseCubicInOut(float t, float b, float c, float d)
{
    float result = 0.0f;

    if ((t /= 0.5f*d) < 1) result = 0.5f*c*t*t*t + b;
    else
    {
        t -= 2;
        result = 0.5f*c*(t*t*t + 2.0f) + b;
    }

    return result;
}

#endif // SUPPORT_MODULE_RSHAPES
