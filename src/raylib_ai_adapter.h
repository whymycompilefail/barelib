/**********************************************************************************************
*
*   raylib_ai_adapter.h - AI adapter engine for raylib
*
*   This module provides a text-based scene description layer for AI systems.
*   It captures error/log output and exposes the rendered scene as structured
*   JSON suitable for pure-text LLM consumption.
*
*   FEATURES:
*       - Manual scene registration: AI-authored games call AIDrawXXX wrapper
*         functions (which wrap raylib's DrawXXX + register the element) so the
*         adapter knows exactly what was drawn. AIGetSceneJSON() produces JSON
*         from the registered elements.
*       - Error/log interception: capture raylib TraceLog output with timestamps
*       - JSON output: format all data as structured JSON for AI parsing
*       - Dependency injection: support mock functions for testing without a window
*       - Input injection: AI can simulate keyboard/mouse input for game testing
*       - Game loop control: pause/resume/step for frame-by-frame debugging
*
*   USAGE:
*       #include "raylib_ai_adapter.h"
*
*       // After raylib initialization
*       AIAdapter *adapter = AIInit();
*
*       // In the game loop:
*       BeginDrawing();
*       AIClearBackground(RAYWHITE);      // clears scene + ClearBackground()
*       AIDrawRect(10, 10, 100, 40, 0, BLUE);  // DrawRect + register
*       AIDrawText(GetFontDefault(), "Hello", 10, 60, 20, 1, BLACK);  // DrawText + register
*       EndDrawing();
*
*       AIUpdate(adapter);
*       const char *json = AIGetSceneJSON(adapter);
*       // Send json to AI model...
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

#ifndef RAYLIB_AI_ADAPTER_H
#define RAYLIB_AI_ADAPTER_H

#include "raylib.h"
#include "raylib_ai_input.h"   // Required for: AISimulateMode (input simulation mode)

#if defined(__cplusplus)
extern "C" {            // Prevents name mangling of functions
#endif

//----------------------------------------------------------------------------------
// Types and Structures Definition
//----------------------------------------------------------------------------------

// AI communication protocol version (wire protocol identifier)
// Scene JSON output carries this version in its top-level "protocol" field, and
// the "version" command reports it. Within the same major version the command
// set and JSON fields remain backward compatible; a major bump signals breaking
// changes. Versioned protocol allows AI agents to adapt to engine evolution.
#define AI_PROTOCOL_VERSION "1.0"

// Structured error codes (error model)
// Command-layer errors are emitted as: <<AI_ERROR>> [code:<N>] <message>
// where N maps to this enum, letting AI agents classify failures programmatically
// instead of pattern-matching free text.
typedef enum AIErrorCode {
    AI_ERR_NONE = 0,            // No error
    AI_ERR_UNKNOWN_COMMAND,     // Unknown command (see 'help')
    AI_ERR_BAD_ARGUMENT,        // Missing or invalid argument
    AI_ERR_BUFFER_FULL,         // Internal buffer overflow (e.g. scene element limit)
    AI_ERR_NO_DATA,             // No available data (e.g. nothing recorded to replay)
    AI_ERR_BUSY,                // Resource already owned by another adapter (e.g. draw observer)
    AI_ERR_NOT_SUPPORTED,       // Operation not supported on this platform
    AI_ERR_INTERNAL             // Internal error
} AIErrorCode;

// Opaque snapshot of window/input/time state consumed by the state JSON generators.
// Full definition lives in src/ai_adapter_internal.h (internal). AIDeps::getStateSnapshot
// fills it; the default implementation reads the live raylib core state.
typedef struct AIStateSnapshot AIStateSnapshot;

// Dependency injection table for testing the AI adapter without a raylib window.
// Each field maps to a raylib function. When NULL is passed to AIInitEx(),
// the real raylib functions are used by default.
typedef struct AIDeps {
    int     (*getScreenWidth)(void);            // Maps to GetScreenWidth()
    int     (*getScreenHeight)(void);           // Maps to GetScreenHeight()
    double  (*getTime)(void);                   // Maps to GetTime()
    void    (*setTraceLogCallback)(TraceLogCallback callback);  // Maps to SetTraceLogCallback()

    // State snapshot injection: fills a copy of the current window/input/time state
    // for JSON serialization. Default implementation reads RaylibContext directly;
    // tests replace it to assert JSON structure without a window.
    void    (*getStateSnapshot)(AIStateSnapshot *out);

    // Input simulation functions (for AI-driven game testing)
    // Default implementations use platform APIs (SendInput on Windows).
    void    (*simulateKeyDown)(int key);        // Simulate key press (down)
    void    (*simulateKeyUp)(int key);          // Simulate key release (up)
    void    (*simulateMouseMove)(int x, int y); // Move mouse cursor
    void    (*simulateMouseDown)(int button);   // Mouse button down (0=left, 1=right, 2=middle)
    void    (*simulateMouseUp)(int button);     // Mouse button release
    void    (*simulateMouseWheel)(int deltaX, int deltaY); // Scroll wheel

    // Protocol command output sink (spec M5): each line the command protocol
    // would print to stdout is passed here. NULL (default) falls back to
    // printf/fflush(stdout). Tests capture output to assert command handling.
    void    (*emitProtocol)(const char *text);

    // Request the game window to close (spec M5, "quit" command). Default
    // implementation sets RaylibContext core.Window.shouldClose = true.
    void    (*requestClose)(void);
} AIDeps;

// AI adapter context handle
// Opaque pointer to the adapter's internal state.
typedef struct AIAdapter AIAdapter;

// Type of a scene element recorded by the adapter.
// Used by AIDrawElement and the AIDrawXXX wrapper functions.
typedef enum AIDrawElementType {
    AI_DRAW_CLEAR = 0,        // ClearBackground
    AI_DRAW_RECT,             // DrawRectangle / DrawRectangleRec / DrawRectanglePro
    AI_DRAW_RECT_LINES,       // DrawRectangleLines / DrawRectangleLinesEx
    AI_DRAW_RECT_ROUNDED,     // DrawRectangleRounded (+ optional outline via lineThick)
    AI_DRAW_CIRCLE,           // DrawCircle / DrawCircleSector / DrawCircleV
    AI_DRAW_CIRCLE_LINES,     // DrawCircleLines / DrawCircleSectorLines
    AI_DRAW_CIRCLE_SECTOR,    // DrawCircleSector / SectorLines (+ optional outline via lineThick)
    AI_DRAW_TRIANGLE,         // DrawTriangle
    AI_DRAW_TRIANGLE_LINES,   // DrawTriangleLines
    AI_DRAW_POLY,             // DrawPoly
    AI_DRAW_POLY_LINES,       // DrawPolyLines / DrawPolyLinesEx
    AI_DRAW_LINE,             // DrawLine / DrawLineV / DrawLineEx
    AI_DRAW_LINE_BEZIER,      // DrawLineBezier
    AI_DRAW_TEXT,             // DrawText / DrawTextEx
    AI_DRAW_TEXTURE,          // DrawTexture / DrawTexturePro / DrawTextureEx
    AI_DRAW_PIXEL,            // DrawPixel / DrawPixelV
    AI_DRAW_SPRITE            // AISpriteDraw (pixel-art sheet frame + animation state)
} AIDrawElementType;

// A single scene element recorded by the adapter.
// The AIDrawXXX wrappers populate this struct and add it to the adapter's
// element buffer. AIRegisterElement() lets advanced users add custom elements.
typedef struct AIDrawElement {
    AIDrawElementType type;   // Element type (determines which fields are used)
    float x, y;               // Position (rect corner, circle center, text origin, etc.)
    float w, h;               // Size (rect width/height, texture dest width/height)
    float x2, y2;             // Second point (line end, triangle v2)
    float x3, y3;             // Third point (triangle v3)
    float radius;             // Circle/poly radius
    float rotation;           // Polygon rotation (degrees)
    int sides;                // Polygon side count
    int fontSize;             // Text font size
    Color color;              // Element color/tint
    char text[128];           // Text content (AI_DRAW_TEXT only)
    char source[256];         // Texture file path (AI_DRAW_TEXTURE / AI_DRAW_SPRITE atlas)
    char tag[32];             // Active animation tag (AI_DRAW_SPRITE only, "" = whole sheet)
    char sheet[256];          // Sheet JSON path (AI_DRAW_SPRITE only)
    int frame;                // Current frame source index (AI_DRAW_SPRITE only)
    int frameCount;           // Frames in the sheet (AI_DRAW_SPRITE only)
    int loop;                 // Active tag loops: 0/1 (AI_DRAW_SPRITE only)
    float scale;              // Draw scale, may be < 1 (AI_DRAW_SPRITE only)
    bool flipX;               // Frame mirrored horizontally (AI_DRAW_SPRITE only)
    int anchor;               // AISpriteAnchor: which point x/y refers to (AI_DRAW_SPRITE only)
    int fx;                   // AI_SPRITE_FX_* bitmask (AI_DRAW_SPRITE only)
} AIDrawElement;

// Structure for a captured log/error message
typedef struct AILogEntry {
    int logLevel;               // LOG_INFO, LOG_WARNING, LOG_ERROR, etc.
    const char *message;        // Log message text
    double timestamp;           // Time since adapter init
} AILogEntry;

// Waveform types for procedural audio generation
typedef enum AIWaveform {
    AI_WAVEFORM_SINE = 0,       // Pure sine wave (smooth tone)
    AI_WAVEFORM_SQUARE,         // Square wave (buzzy, retro)
    AI_WAVEFORM_TRIANGLE,       // Triangle wave (soft, mellow)
    AI_WAVEFORM_SAWTOOTH,       // Sawtooth wave (sharp, bright)
    AI_WAVEFORM_NOISE           // White noise (percussion, effects)
} AIWaveform;

#define MAX_AI_AUDIO_SLOTS 64  // Max concurrent procedural audio sources

#define MAX_AI_RECORD_ENTRIES 1024  // Max recorded input entries
#define MAX_AI_SNAPSHOTS      16    // Max saved scene snapshots

// A recorded input entry (for record/playback system).
// Each entry stores an input action string and the frame number at which it occurred.
typedef struct AIRecordEntry {
    int frame;                  // Frame number relative to recording start
    char action[128];           // Input action text (e.g. "press space", "click 400 300")
} AIRecordEntry;

//----------------------------------------------------------------------------------
// AI Adapter Module Functions
//----------------------------------------------------------------------------------

// Initialize the AI adapter (uses real raylib functions)
// Must be called after InitWindow(). Returns NULL on failure.
RLAPI AIAdapter *AIInit(void);

// Initialize the AI adapter with custom dependencies
// Pass NULL for deps to use real raylib functions (same as AIInit()).
// Useful for testing without a raylib window — provide mock implementations.
RLAPI AIAdapter *AIInitEx(AIDeps *deps);

// Close the AI adapter and free all resources.
// On Windows the stdin reader thread is stopped with CancelSynchronousIo()
// (works for pipe/redirected stdin), so a blocked fgets() no longer forces a
// deferred free. Interactive console stdin is not always cancellable: in that
// case the thread is detached and the adapter memory is reclaimed at process
// exit, with a WARNING. To get a full release under an interactive console,
// close stdin (EOF) before calling AIClose().
RLAPI void AIClose(AIAdapter *adapter);

// Update the adapter's internal state (call once per frame after EndDrawing)
// Recycles finished audio slots and auto-outputs scene JSON in stdout mode.
// Does NOT clear the element buffer — use AIClearBackground() or AIClearElements()
// at the start of each frame's draw phase to reset the scene.
RLAPI void AIUpdate(AIAdapter *adapter);

//----------------------------------------------------------------------------------
// Scene Description Functions
//----------------------------------------------------------------------------------

// Register a scene element manually (advanced use).
// Most AI-authored games should use the AIDrawXXX wrappers instead, which call
// the corresponding raylib DrawXXX function and register the element in one step.
// element is copied by value into the adapter's internal buffer.
RLAPI void AIRegisterElement(AIAdapter *adapter, const AIDrawElement *element);

// Select the input simulation mode for this adapter (spec R6):
//   AI_SIMULATE_SENDINPUT (default) — OS input queue; requires the game window
//       to be in the foreground (background sessions lose key events).
//   AI_SIMULATE_POSTMESSAGE — WM_* messages posted straight to the game window;
//       works with the window in the background, so an unattended AI tester can
//       actually drive the game. Key presses are held one frame in this mode.
// The `inputmode` command wraps this.
RLAPI void AISetInputMode(AIAdapter *adapter, AISimulateMode mode);

// Enable/disable engine-level automatic scene capture (spec M3, non-invasive).
// When enabled, the engine's draw observer registers native DrawXXX calls
// (currently the rectangle family) into this adapter's scene, so STOCK games
// using plain DrawXXX become observable without any game-code changes.
// Mutually exclusive with the AIDrawXXX wrappers: use wrappers (explicit
// registration) OR auto capture (implicit registration) for a given game —
// enabling both double-registers elements. Pass false (or NULL observer path)
// to disable; auto capture also disables on AIClose().
RLAPI void AISetAutoSceneCapture(AIAdapter *adapter, bool enable);

// Clear all registered scene elements.
// Called automatically by AIClearBackground(); can also be called explicitly
// at the start of each frame's draw phase.
RLAPI void AIClearElements(AIAdapter *adapter);

// Get the number of registered scene elements for the current frame.
RLAPI int AIGetElementCount(AIAdapter *adapter);

// Get a JSON-formatted description of the current scene
// Returns a string that should NOT be freed by the caller (owned by the adapter).
// The JSON includes: screen dimensions and the list of elements registered
// this frame (via AIDrawXXX wrappers or AIRegisterElement).
// NOTE: the returned pointer points into the adapter's internal JSON buffer.
// It stays valid only until the next AIGet*JSON() call (scene/state/diff/errors
// share the same buffer), so copy the string if you need to keep it (D8).
RLAPI const char *AIGetSceneJSON(AIAdapter *adapter);

//----------------------------------------------------------------------------------
// Drawing Wrapper Functions
//----------------------------------------------------------------------------------
// These functions wrap raylib's draw calls and automatically register the
// scene element with the first registered AIAdapter. If no adapter exists,
// they still call the raylib draw function (rendering is unaffected).
//
// AI-authored games SHOULD use these instead of the raw DrawXXX functions
// so the adapter can produce an accurate scene description.
//----------------------------------------------------------------------------------

// Background
RLAPI void AIClearBackground(Color color);                    // ClearBackground + clear scene buffer

// Drawing wrappers (M4-slim: one function per AIDrawElementType, scalar params,
// superset semantics — see docs/ai_api_迁移映射表.md for the 33→16 migration)

// Rectangles
RLAPI void AIDrawRect(float x, float y, float w, float h, float rotation, Color color);
RLAPI void AIDrawRectLines(float x, float y, float w, float h, float lineThick, Color color);
RLAPI void AIDrawRectRounded(float x, float y, float w, float h, float roundness, float lineThick, Color color);

// Circles
RLAPI void AIDrawCircle(float x, float y, float radius, Color color);
RLAPI void AIDrawCircleLines(float x, float y, float radius, Color color);
RLAPI void AIDrawCircleSector(float x, float y, float radius, float startAngle, float endAngle, float lineThick, Color color);

// Triangles
RLAPI void AIDrawTriangle(float x1, float y1, float x2, float y2, float x3, float y3, Color color);
RLAPI void AIDrawTriangleLines(float x1, float y1, float x2, float y2, float x3, float y3, Color color);

// Polygons
RLAPI void AIDrawPoly(float x, float y, int sides, float radius, float rotation, Color color);
RLAPI void AIDrawPolyLines(float x, float y, int sides, float radius, float rotation, float lineThick, Color color);

// Lines
RLAPI void AIDrawLine(float x1, float y1, float x2, float y2, float thick, Color color);
RLAPI void AIDrawLineBezier(float x1, float y1, float x2, float y2, float thick, Color color);

// Pixels
RLAPI void AIDrawPixel(float x, float y, Color color);

// Text
// font: pass GetFontDefault() (or any font with texture.id == 0) for the default font; spacing 1 = default
RLAPI void AIDrawText(Font font, const char *text, float x, float y, float fontSize, float spacing, Color color);

// Textures
// NOTE: Use AILoadTexture() instead of LoadTexture() so the adapter can record
// the file path for the scene JSON. If you use LoadTexture(), the texture will
// still render but the scene JSON will show "unknown" as the source path.
// source: (Rectangle){0} = whole texture; w/h 0 = texture natural size; rotation 0 = none
RLAPI Texture2D AILoadTexture(const char *fileName);          // LoadTexture + record path
RLAPI void AIDrawTexture(Texture2D texture, Rectangle source, float x, float y, float w, float h, float rotation, Color tint);

//----------------------------------------------------------------------------------
// Error and Log Feedback Functions
//----------------------------------------------------------------------------------

// Get all captured log entries since the last call to AIClearLogs()
// Returns a pointer to an internal array; count is set to the number of entries.
RLAPI const AILogEntry *AIGetLogs(AIAdapter *adapter, int *count);

// Get log entry by logical index (0 = oldest retained). Returns NULL if out of range.
// Preferred over AIGetLogs(): after ring wrap-around, physical buffer order != logical order.
// NOTE: the entry pointer is only valid until the next log write / AIClearLogs().
RLAPI const AILogEntry *AIGetLogAt(AIAdapter *adapter, int index);

// Clear all captured log entries
RLAPI void AIClearLogs(AIAdapter *adapter);

// Get the most recent error(s) formatted as a JSON string
RLAPI const char *AIGetErrorsJSON(AIAdapter *adapter);

// Get the last error code reported by the adapter (AI_ERR_NONE if none).
// Set by AIInjectInput() on invalid action strings, among others; the command
// loop reports it as <<AI_ERROR>> [code:N] so AI agents can react to failures.
RLAPI AIErrorCode AIGetLastError(AIAdapter *adapter);

//----------------------------------------------------------------------------------
// Backend State Query Functions (for AI observability)
//----------------------------------------------------------------------------------

// Get the current window state as a JSON string.
// Returns window size, position, display, flags (fullscreen/resizable/undecorated/etc).
// Reads CORE directly; adapter is used only as the JSON buffer owner.
RLAPI const char *AIGetWindowStateJSON(AIAdapter *adapter);

// Get the current input state as a JSON string.
// Returns keyboard (keysDown, keysPressed, charsPressed), mouse (position, buttonsDown, wheel),
// touch (points), and gamepad (per-device buttonsDown + axes).
// Lets AI observe input results after AIInjectInput(), closing the inject-observe loop.
RLAPI const char *AIGetInputStateJSON(AIAdapter *adapter);

// Get the current timing state as a JSON string.
// Returns current time, frame/update/draw time, target, frameCounter, and FPS.
RLAPI const char *AIGetTimeStateJSON(AIAdapter *adapter);

// Get the current audio state as a JSON string.
// Returns device readiness, master volume, and all active audio buffers
// (usage=STATIC for Sound / STREAM for Music+AudioStream, with playing/paused/volume/pitch/pan/progress).
// Lets AI observe what audio is playing without direct audio API access.
RLAPI const char *AIGetAudioStateJSON(AIAdapter *adapter);

//----------------------------------------------------------------------------------
// Procedural Audio Functions (AI-generated sound effects)
//----------------------------------------------------------------------------------

// Play a procedural tone with given waveform. Returns audio handle (>0) or 0 on failure.
// Handle is valid until the tone finishes or AIStopAudio() is called.
// Fade in/out (10ms or 5% of duration) is applied automatically to avoid clicks.
// `seed` drives NOISE reproducibility (self-contained LCG, ignored for tonal
// waveforms); duration is clamped to ~22.7s at 44.1kHz (AI_MAX_AUDIO_SAMPLES).
RLAPI int AIPlayTone(AIAdapter *adapter, float frequency, float duration, float volume, int waveform, unsigned int seed);

// Play white noise burst. Returns audio handle (>0) or 0 on failure.
// Equivalent to AIPlayTone with AI_WAVEFORM_NOISE but clearer intent.
// Same `seed` (e.g. 0) yields the same noise — AI can reproduce sound effects.
RLAPI int AIPlayNoise(AIAdapter *adapter, float duration, float volume, unsigned int seed);

// Stop a procedural audio source by handle. Safe to call with invalid/finished handle.
RLAPI void AIStopAudio(AIAdapter *adapter, int handle);

// Pause a procedural audio source by handle.
RLAPI void AIPauseAudio(AIAdapter *adapter, int handle);

// Resume a paused procedural audio source by handle.
RLAPI void AIResumeAudio(AIAdapter *adapter, int handle);

// Set volume of a procedural audio source by handle (0.0 to 1.0).
RLAPI void AISetAudioVolume(AIAdapter *adapter, int handle, float volume);

// Set pitch of a procedural audio source by handle (1.0 = base frequency).
RLAPI void AISetAudioPitch(AIAdapter *adapter, int handle, float pitch);

// Check if a procedural audio source is still playing.
RLAPI bool IsAIAudioPlaying(AIAdapter *adapter, int handle);

//----------------------------------------------------------------------------------
// Input Injection Functions (AI-driven game testing)
//----------------------------------------------------------------------------------

// Inject an input action for AI-driven game testing.
// Supported action formats:
//   press <key>             - Press and release a key
//   press <key> down        - Press and hold a key
//   press <key> up          - Release a key
//   click <x> <y>           - Left mouse click at (x, y)
//   click <x> <y> right     - Right mouse click at (x, y)
//   click <x> <y> middle    - Middle mouse click at (x, y)
//   move <x> <y>            - Move mouse cursor to (x, y)
//   scroll <dx> <dy>        - Scroll mouse wheel
//   type <text>             - Type a string of text
//
// Key names (case-insensitive):
//   Letters: a-z, Numbers: 0-9
//   Special: space, enter, escape, tab, backspace, delete, insert,
//            home, end, pageup, pagedown
//   Arrows:  up, down, left, right
//   Modifiers: shift, ctrl, alt
//   Function: f1-f12
RLAPI void AIInjectInput(AIAdapter *adapter, const char *action);

//----------------------------------------------------------------------------------
// Output Mode Configuration
//----------------------------------------------------------------------------------

// Output mode for the AI adapter.
typedef enum AIOutputMode {
    AI_OUTPUT_MEMORY = 0,  // Default: store JSON in memory, caller fetches with AIGetSceneJSON()
    AI_OUTPUT_STDOUT,      // Auto-write JSON to stdout on each AIUpdate() call
} AIOutputMode;

// Set the output mode for the adapter.
// When set to AI_OUTPUT_STDOUT, AIUpdate() automatically writes scene JSON
// to stdout delimited by <<AI_SCENE_BEGIN>> / <<AI_SCENE_END>> markers,
// suitable for AI agents (Trae, Codex, etc.) to parse from console output.
// When set to AI_OUTPUT_MEMORY (default), the caller fetches JSON via AIGetSceneJSON().
RLAPI void AISetOutputMode(AIAdapter *adapter, AIOutputMode mode);

//----------------------------------------------------------------------------------
// Game Loop Control (for AI hot-reload runtime)
//----------------------------------------------------------------------------------

// Pause the game loop. When paused, GameUpdate is not called but GameDraw still
// renders the current state, so AI can observe the frozen scene.
RLAPI void AIPauseGame(AIAdapter *adapter);

// Resume the game loop after a pause.
RLAPI void AIResumeGame(AIAdapter *adapter);

// Allow GameUpdate() to run for N real frames while paused. This only sets a
// counter: frames advance one per host frame, not instantly. The adapter stays
// paused; the counter reaching 0 simply stops permitting updates.
// If the game is not paused, this has no effect.
RLAPI void AIStepFrame(AIAdapter *adapter, int frames);

// Query whether the game loop is currently paused.
RLAPI bool AIGamePaused(AIAdapter *adapter);

// Get remaining step frames (decremented by the host via AIConsumeStepFrame).
// Returns 0 when no stepping is in progress.
RLAPI int AIGetStepFrames(AIAdapter *adapter);

// Consume one step frame (decrement the counter). The host MUST call this
// once per real frame — the engine runtime never calls it. No-op when the
// counter is already 0. See docs/ai_adapter.md "Game loop control".
RLAPI void AIConsumeStepFrame(AIAdapter *adapter);

//----------------------------------------------------------------------------------
// Command Loop (stdin-driven AI control)
//----------------------------------------------------------------------------------
// Starts a background thread that reads commands from stdin and queues them
// for processing. AIUpdate() processes pending commands each frame.
// This allows an AI agent to control the game via stdin/stdout pipes.
//
// Commands (one per line):
//   version            - Print protocol version
//   inputmode <mode>   - Switch input simulation: sendinput (default) | postmessage (background-safe)
//   scene              - Print current scene JSON to stdout
//   input <action>     - Inject input (press/click/move/scroll/type)
//   pause              - Pause game loop
//   resume             - Resume game loop
//   step [n]           - Step n frames (default: 1)
//   state [category]   - Print state JSON (window/input/time/audio)
//   errors             - Print error log JSON
//   quiet              - Stop auto-stdout scene output (memory mode)
//   loud               - Start auto-stdout scene output (stdout mode)
//   elements           - Print element count
//   help               - Print available commands
//   quit               - Signal game to close
//
// Output markers:
//   <<AI_SCENE_BEGIN>> ... <<AI_SCENE_END>>      - Scene JSON
//   <<AI_STATE_BEGIN>> ... <<AI_STATE_END>>      - State JSON
//   <<AI_ERRORS_BEGIN>> ... <<AI_ERRORS_END>>    - Errors JSON
//   <<AI_OK>> <message>                          - Success confirmation
//   <<AI_ERROR>> <message>                       - Error message
//   <<AI_SHUTDOWN>>                              - Adapter closing
RLAPI void AIStartCommandLoop(AIAdapter *adapter);

//----------------------------------------------------------------------------------
// Record / Playback System
//----------------------------------------------------------------------------------
// The record/playback system lets an AI agent record a sequence of input actions
// and replay them later — useful for reproducing bugs or testing timing-sensitive
// gameplay. Recording captures every input command injected via AIInjectInput()
// along with the frame number. Playback replays them at the correct frames.
//
// Scene snapshots capture the element list at a point in time. The diff command
// compares the current scene against the most recent snapshot, reporting added,
// removed, and changed elements — useful for detecting unexpected state changes.
//
// Commands (via stdin command loop):
//   record             - Start recording input actions
//   record stop        - Stop recording
//   playback           - Start replaying recorded input
//   playback stop      - Stop playback
//   snapshot [save]    - Save current scene as a snapshot
//   snapshot clear     - Clear all saved snapshots
//   diff               - Compare current scene to latest snapshot (JSON output)
//
// Output markers:
//   <<AI_DIFF_BEGIN>> ... <<AI_DIFF_END>>  - Scene diff JSON

// Recording control
RLAPI void AIStartRecording(AIAdapter *adapter);   // Begin recording input actions
RLAPI void AIStopRecording(AIAdapter *adapter);    // Stop recording
RLAPI bool AIIsRecording(AIAdapter *adapter);      // Check if currently recording
RLAPI int  AIGetRecordCount(AIAdapter *adapter);   // Number of recorded entries

// Playback control
RLAPI void AIStartPlayback(AIAdapter *adapter);    // Begin replaying recorded input
RLAPI void AIStopPlayback(AIAdapter *adapter);     // Stop playback
RLAPI bool AIIsPlayingBack(AIAdapter *adapter);    // Check if currently playing back

// Scene snapshots and diffing
RLAPI void AISaveSnapshot(AIAdapter *adapter);             // Save current scene elements as snapshot
RLAPI void AIClearSnapshots(AIAdapter *adapter);           // Clear all saved snapshots
RLAPI int  AIGetSnapshotCount(AIAdapter *adapter);         // Number of saved snapshots
RLAPI const char *AIGetSnapshotDiff(AIAdapter *adapter);   // Diff current scene vs latest snapshot (JSON)

#if defined(__cplusplus)
}
#endif

#endif // RAYLIB_AI_ADAPTER_H