/**********************************************************************************************
*
*   raylib_observability.h - Draw observation hook (non-invasive observability probe)
*
*   spec M3: lets the ENGINE capture semantic draw calls WITHOUT game cooperation.
*   A stock game (using plain DrawXXX) becomes observable to AI systems by having
*   the engine notify a registered observer with the semantic parameters, instead
*   of requiring the game to call AIDrawXXX wrappers.
*
*   CURRENT SCOPE: hooks land on the convergence points of the rectangle family
*   (DrawRectangle/V/Rec/Pro), the circle family (DrawCircle/V), text
*   (DrawText/Ex/Pro via DrawTextEx), and the deepened primitives
*   (DrawRectangleRounded/Lines, DrawCircleSector/Lines — added with the
*   wrapper-deepening pass).
*   The observer is a single function pointer; NULL (default) disables the hook
*   with one compare-and-branch per draw call.
*
*   LICENSE: zlib/libpng
*
**********************************************************************************************/

#ifndef RAYLIB_OBSERVABILITY_H
#define RAYLIB_OBSERVABILITY_H

#include "raylib.h"

#if defined(__cplusplus)
extern "C" {
#endif

//----------------------------------------------------------------------------------
// Types and Structures Definition
//----------------------------------------------------------------------------------

// Draw operation kinds reported to the observer (extend as more DrawXXX hooks land)
typedef enum AIObservedDraw {
    AI_OBS_RECT = 0,             // Rectangle family (DrawRectangle/V/Rec/Pro)
    AI_OBS_CIRCLE,               // Circle family (DrawCircle/V)
    AI_OBS_TEXT,                 // Text (DrawText/Ex/Pro via DrawTextEx)
    AI_OBS_RECT_ROUNDED,         // Rounded rect (DrawRectangleRounded/Lines) — roundness in `radius`
    AI_OBS_CIRCLE_SECTOR         // Circle sector (DrawCircleSector/Lines) — startAngle in `rotation`, endAngle in `w`
} AIObservedDraw;

// Observer callback: receives the semantic draw parameters of each hooked DrawXXX.
// Called from the engine's draw path, so it must be cheap and must NOT call raylib
// draw functions (would recurse). radius/rotation/text are 0/NULL when not
// applicable; `text` is only valid for the duration of the call.
typedef void (*AIDrawObserver)(AIObservedDraw type, float x, float y, float w, float h, float radius, float rotation, const char *text, Color color);

//----------------------------------------------------------------------------------
// Module Functions
//----------------------------------------------------------------------------------

// Register (or clear with NULL) the draw observer. Thread-safe not required:
// call before the render loop starts.
RLAPI void SetAIDrawObserver(AIDrawObserver observer);

// Get the currently registered observer (NULL if none).
RLAPI AIDrawObserver GetAIDrawObserver(void);

// INTERNAL (used by the AI adapter): register observer WITH ownership. The owner
// pointer lets the adapter release only its own registration on close and detect
// conflicts (second adapter enabling auto-capture while owned by another -> AI_ERR_BUSY).
// Passing owner == NULL is equivalent to the public SetAIDrawObserver (no owner).
RLAPI void SetAIDrawObserverOwned(AIDrawObserver observer, void *owner);

#if defined(__cplusplus)
}
#endif

#endif // RAYLIB_OBSERVABILITY_H
