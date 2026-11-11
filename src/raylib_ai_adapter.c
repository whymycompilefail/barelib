/**********************************************************************************************
*
*   raylib_ai_adapter.c - AI adapter engine implementation
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

#include "raylib_ai_adapter.h"
#include "raylib_ai_input.h"   // Required for: DefaultSimulateKeyDown(), etc.
#include "raylib_context.h"    // Required for: RaylibContext, GetRaylibContext()
#include "raudio_observer.h"   // Required for: AudioBufferInfo + iterator (miniaudio-free)
#include "raylib_observability.h" // Required for: AIDrawObserver engine hook (spec M3)
#include "ai_adapter_internal.h"  // Internal types/macros shared with the split units (M4)
#include "config.h"

#include <stdlib.h>     // Required for: malloc(), free(), realloc()
#include <string.h>     // Required for: strcmp(), strlen(), strdup()
#include <stdio.h>      // Required for: snprintf(), vsnprintf()
#include <stdarg.h>     // Required for: va_list, va_start, va_end
#include <stdbool.h>    // Required for: bool, true, false
#include <ctype.h>      // Required for: tolower(), isdigit()
#include <math.h>       // Required for: sinf(), fabsf(), floorf(), sqrtf()

// Windows threading for the command loop (stdin reader thread)
#if defined(_WIN32)
    #define WIN32_LEAN_AND_MEAN
    #define NOGDI
    #define NOMINMAX
    #define NODRAWTEXT             // Prevent DrawText/DrawTextEx macro conflicts
    #define CloseWindow  CloseWindow_WIN32
    #define ShowCursor   ShowCursor_WIN32
    #include <windows.h>
    #undef CloseWindow
    #undef ShowCursor
    // Also undef these in case NODRAWTEXT wasn't enough (some SDK versions)
    #ifdef DrawText
        #undef DrawText
    #endif
    #ifdef DrawTextEx
        #undef DrawTextEx
    #endif
#endif

//----------------------------------------------------------------------------------
// Defines and Macros
// Key Name to Keycode Lookup
//----------------------------------------------------------------------------------

// Convert a key name string to a raylib keycode (case-insensitive)
// Supports: letters a-z, numbers 0-9, and common named keys

//----------------------------------------------------------------------------------
// Default Input Simulation Implementations
// NOTE: Platform-specific implementations are now in separate compilation units:
//   - Windows:  raylib_ai_input_win32.c  (SendInput API)
//   - Other:    raylib_ai_input_stubs.c  (stubs)
// These are declared in raylib_ai_input.h and used by the default deps table
//----------------------------------------------------------------------------------

//----------------------------------------------------------------------------------
// Module Internal Functions Declaration
//----------------------------------------------------------------------------------

// Write a log entry to a single adapter's circular buffer
static void AIWriteLogEntry(AIAdapter *adapter, int logLevel, const char *message)
{
    // The audio thread also emits TraceLog entries, so every write is serialized (D2)
    AI_LOG_LOCK(adapter);

    AILogEntry *entry = &adapter->logBuffer[adapter->logHead];
    entry->logLevel = logLevel;

    // Free old message if overwriting (circular buffer wrap-around)
    if (adapter->logCount >= MAX_LOG_ENTRIES)
    {
        if (adapter->logBuffer[adapter->logHead].message != NULL)
        {
            RL_FREE((void *)adapter->logBuffer[adapter->logHead].message);
            adapter->logBuffer[adapter->logHead].message = NULL;
        }
    }

    entry->message = strdup(message);
    entry->timestamp = adapter->deps->getTime() - adapter->initTime;

    adapter->logHead = (adapter->logHead + 1) % MAX_LOG_ENTRIES;
    if (adapter->logCount < MAX_LOG_ENTRIES) adapter->logCount++;

    AI_LOG_UNLOCK(adapter);
}

// Custom TraceLog callback to capture logs for all registered adapters
static void AILogCallback(int logLevel, const char *text, va_list args)
{
    char message[1024];
    vsnprintf(message, sizeof(message), text, args);

    // Store in all registered adapters
    RaylibContext *ctx = GetRaylibContext();
    for (int i = 0; i < ctx->aiAdapterCount; i++)
    {
        AIWriteLogEntry(ctx->aiAdapters[i], logLevel, message);
    }

    // Forward to the original callback (saved before our hook was installed)
    if (ctx->aiSavedLogCallback != NULL)
    {
        ctx->aiSavedLogCallback(logLevel, text, args);
    }
}

//----------------------------------------------------------------------------------
// Module Functions Definition
//----------------------------------------------------------------------------------

// Initialize the AI adapter (uses real raylib functions)
AIAdapter *AIInit(void)
{
    return AIInitEx(NULL);
}

// Forward declaration: default state snapshot implementation (defined below with
// the state JSON generators); registered in defaultDeps so real games get it.
static void AIDefaultGetStateSnapshot(AIStateSnapshot *snap);
// Forward declaration: default quit handler (defined with the state generators);
// registered in defaultDeps so the protocol's "quit" command works by default.
static void AIDefaultRequestClose(void);

// Initialize the AI adapter with custom dependencies
// Pass NULL for deps to use real raylib functions
AIAdapter *AIInitEx(AIDeps *deps)
{
    AIAdapter *adapter = (AIAdapter *)RL_CALLOC(sizeof(AIAdapter), 1);
    if (adapter == NULL) return NULL;

    // Set per-instance dependency table (real raylib functions by default,
    // stored in-context so no module-level static state is needed)
    adapter->defaultDeps = (AIDeps){
        .getScreenWidth = GetScreenWidth,
        .getScreenHeight = GetScreenHeight,
        .getTime = GetTime,
        .setTraceLogCallback = SetTraceLogCallback,
        .getStateSnapshot = AIDefaultGetStateSnapshot,
        .simulateKeyDown = DefaultSimulateKeyDown,
        .simulateKeyUp = DefaultSimulateKeyUp,
        .simulateMouseMove = DefaultSimulateMouseMove,
        .simulateMouseDown = DefaultSimulateMouseDown,
        .simulateMouseUp = DefaultSimulateMouseUp,
        .simulateMouseWheel = DefaultSimulateMouseWheel,
        .requestClose = AIDefaultRequestClose,
        // .emitProtocol stays NULL: ai_protocol.c falls back to printf(stdout)
    };
    adapter->deps = (deps != NULL) ? deps : &adapter->defaultDeps;

    // Initialize state
    adapter->logCount = 0;
    adapter->logHead = 0;
    adapter->initTime = adapter->deps->getTime();
    adapter->outputMode = AI_OUTPUT_MEMORY;
    adapter->lastErrorCode = AI_ERR_NONE;
    adapter->lastErrorMessage[0] = '\0';
    adapter->inputMode = AI_SIMULATE_SENDINPUT;
    adapter->pendingKeyUp = -1;
    adapter->pendingMouseUp = -1;
#if defined(_WIN32)
    InitializeCriticalSection(&adapter->logMutex);
#endif

    // Register this adapter in the context's adapter list.
    // Single-adapter premise (candidate 2): the FIRST registered adapter becomes
    // the one served by AIDrawXXX wrappers; additional instances are allowed
    // only for multi-instance memory-isolation testing (they do not serve wrappers).
    RaylibContext *ctx = GetRaylibContext();
    if (ctx->aiAdapterCount > 0)
    {
        TRACELOG(LOG_WARNING, "AIADAPTER: additional adapter registered - only the first adapter serves AIDrawXXX wrappers (single-adapter premise)");
    }
    if (ctx->aiAdapterCount < MAX_AI_ADAPTERS)
    {
        ctx->aiAdapters[ctx->aiAdapterCount++] = adapter;
    }
    if (ctx->aiActiveAdapter == NULL)
    {
        ctx->aiActiveAdapter = adapter;
    }

    // Install custom log callback on first registration
    if (ctx->aiAdapterCount == 1)
    {
        // First adapter: chain the existing callback (which may be a user hook)
        // and install our logging hook on top instead of clobbering it (D1)
        ctx->aiSavedLogCallback = GetCurrentTraceLogCallback();
        adapter->deps->setTraceLogCallback(AILogCallback);
    }

    TRACELOG(LOG_INFO, "AIADAPTER: Initialized");

    return adapter;
}

// Close the AI adapter and free all resources
void AIClose(AIAdapter *adapter)
{
    if (adapter == NULL) return;

    // Stop the command loop thread if it was started
#if defined(_WIN32)
    if (adapter->commandLoopStarted)
    {
        adapter->commandThreadRunning = false;

        if (adapter->commandThread != NULL)
        {
            HANDLE thread = adapter->commandThread;

            // T6: cancel the synchronous ReadFile behind a blocked fgets().
            // CancelSynchronousIo() is Vista+ and lives in kernel32; resolve it
            // at runtime so we never add a hard SDK/loader dependency. For a
            // pipe-backed stdin (AI testers, `echo quit |`) this makes fgets()
            // return NULL so the reader exits and the adapter can be freed.
            // Console input is not always cancellable -> keep the detach path.
            typedef BOOL (WINAPI *CancelSynchronousIoFn)(HANDLE);
            CancelSynchronousIoFn cancelSyncIo = NULL;
            HMODULE kernel32 = GetModuleHandleW(L"kernel32.dll");
            if (kernel32 != NULL)
            {
                cancelSyncIo = (CancelSynchronousIoFn)GetProcAddress(kernel32, "CancelSynchronousIo");
            }

            // A thread that passed the while() check but has not entered
            // ReadFile yet reports ERROR_NOT_FOUND; retry briefly to close that
            // race before falling back.
            BOOL cancelled = FALSE;
            for (int attempt = 0; (attempt < 3) && (cancelSyncIo != NULL) && !cancelled; attempt++)
            {
                cancelled = cancelSyncIo(thread);
                if (!cancelled) Sleep(5);
            }

            DWORD waitRes = WaitForSingleObject(thread, 500);
            if (waitRes == WAIT_OBJECT_0)
            {
                CloseHandle(thread);
                adapter->commandThread = NULL;
                if (cancelled)
                {
                    // A cancelled ReadFile leaves the CRT stream in an error
                    // state; clear it so a later AIStartCommandLoop() in this
                    // process can read stdin again.
                    clearerr(stdin);
                    TRACELOG(LOG_DEBUG, "AIADAPTER: stdin thread stopped via CancelSynchronousIo");
                }
            }
            else
            {
                // Still blocked in fgets (typically interactive console input).
                // Do NOT free the adapter or delete the mutex: the thread still
                // references them. Detach and let the OS reclaim at exit.
                adapter->commandThread = NULL;   // detach: never CloseHandle a running thread
                adapter->deferFree = true;       // adapter memory deferred to process exit
                TRACELOG(LOG_WARNING, "AIADAPTER: stdin thread still blocked; adapter memory deferred to process exit");
            }
        }
        // Only destroy the mutex once the reader thread is confirmed gone
        if (!adapter->deferFree)
        {
            DeleteCriticalSection(&adapter->commandMutex);
        }
        adapter->commandLoopStarted = false;
    }
#endif

    // Flush errors to stderr BEFORE freeing log messages: the flush reads
    // logBuffer[].message pointers, and AIClose itself may have just appended a
    // warning (e.g. the deferred-thread note above) that must not become a
    // use-after-free or a false <<AI_CRASH>> report. Only LOG_ERROR/LOG_FATAL
    // trigger the crash marker; warnings are diagnostics, not crashes
    if (adapter->logCount > 0)
    {
        int hasErrors = 0;
        for (int i = 0; i < adapter->logCount; i++)
        {
            int idx = AI_LOG_INDEX(adapter, i);
            if (adapter->logBuffer[idx].logLevel >= LOG_ERROR)
            {
                hasErrors = 1;
                break;
            }
        }
        if (hasErrors)
        {
            fprintf(stderr, "<<AI_CRASH>>\n");
            for (int i = 0; i < adapter->logCount; i++)
            {
                int idx = AI_LOG_INDEX(adapter, i);
                if (adapter->logBuffer[idx].logLevel >= LOG_ERROR)
                {
                    fprintf(stderr, "[level %d] %s\n",
                        adapter->logBuffer[idx].logLevel,
                        adapter->logBuffer[idx].message ? adapter->logBuffer[idx].message : "");
                }
            }
            fprintf(stderr, "<<AI_CRASH_END>>\n");
            fflush(stderr);
        }
    }

    // Free log messages (no callback can fire after the adapter is unregistered;
    // the log mutex only needs to be destroyed for non-deferred teardown)
    AI_LOG_LOCK(adapter);
    for (int i = 0; i < adapter->logCount; i++)
    {
        int idx = AI_LOG_INDEX(adapter, i);
        if (adapter->logBuffer[idx].message != NULL)
        {
            RL_FREE((void *)adapter->logBuffer[idx].message);
        }
    }
    adapter->logCount = 0;
    AI_LOG_UNLOCK(adapter);

    // Stop and unload any active procedural audio slots (delegated to ai_audio.c)
    AIAudioShutdown(adapter);

    // Remove from context's adapter list
    RaylibContext *ctx = GetRaylibContext();
    for (int i = 0; i < ctx->aiAdapterCount; i++)
    {
        if (ctx->aiAdapters[i] == adapter)
        {
            ctx->aiAdapters[i] = ctx->aiAdapters[ctx->aiAdapterCount - 1];
            ctx->aiAdapterCount--;
            break;
        }
    }
    // If the served adapter closes, no adapter serves the wrappers anymore
    // (single-adapter premise — no promotion of a second instance).
    if (ctx->aiActiveAdapter == adapter)
    {
        ctx->aiActiveAdapter = NULL;
    }

    // Disable engine-level auto scene capture if this adapter owns the observer:
    // release only our own registration — never touch an observer owned by
    // game code or another adapter (single-adapter premise, ownership tracked).
    if (adapter->autoCaptureEnabled)
    {
        adapter->autoCaptureEnabled = false;
    }
    if (ctx->aiDrawObserverOwner == (void *)adapter)
    {
        SetAIDrawObserverOwned(NULL, NULL);
    }

    // Restore original callback if this was the last adapter
    if (ctx->aiAdapterCount == 0)
    {
        adapter->deps->setTraceLogCallback(ctx->aiSavedLogCallback);
        ctx->aiSavedLogCallback = NULL;
    }

    // Output clean shutdown marker
    if (adapter->outputMode == AI_OUTPUT_STDOUT)
    {
        printf("<<AI_SHUTDOWN>>\n");
        fflush(stdout);
    }

    if (!adapter->deferFree)
    {
#if defined(_WIN32)
        DeleteCriticalSection(&adapter->logMutex);
#endif
        RL_FREE(adapter);
    }
    else
    {
        // The stdin reader thread still references the adapter; leaking it
        // until process exit is safe (OS reclaims) and prevents use-after-free
        TRACELOG(LOG_WARNING, "AIADAPTER: adapter memory deferred to process exit (stdin thread active)");
    }
}

// Update the adapter (call once per frame after EndDrawing)
void AIUpdate(AIAdapter *adapter)
{
    if (adapter == NULL) return;

    // Flush the delayed key-up from a PostMessage press of the PREVIOUS frame:
    // the game's PollEvents has since handled the key-down, so releasing the key
    // now lets IsKeyPressed detect the press (spec R6). Must run before
    // processing new commands — a fresh press sets pendingKeyUp again this frame
    if (adapter->pendingKeyUp >= 0)
    {
        adapter->deps->simulateKeyUp(adapter->pendingKeyUp);
        adapter->pendingKeyUp = -1;
    }
    // Same one-frame delay for background mouse clicks: a same-frame down+up
    // pair is handled by one PollEvents batch and cancels itself out (C5).
    if (adapter->pendingMouseUp >= 0)
    {
        adapter->deps->simulateMouseUp(adapter->pendingMouseUp);
        adapter->pendingMouseUp = -1;
    }

    // Process pending commands from the stdin reader thread
    AIProcessCommands(adapter);

    // Playback: replay recorded input actions (loose frame alignment, auto-stop)
    AIPlaybackUpdate(adapter);

    // Auto-recycle finished procedural audio slots (delegated to ai_audio.c)
    AIAudioUpdate(adapter);

    // Auto-output to stdout if enabled
    if (adapter->outputMode == AI_OUTPUT_STDOUT)
    {
        const char *json = AIGetSceneJSON(adapter);
        if (json != NULL && json[0] != '\0')
        {
            // Markers on separate lines: the JSON can fill the whole buffer
            printf("<<AI_SCENE_BEGIN>>\n");
            printf("%s\n", json);
            printf("<<AI_SCENE_END>>\n");
            fflush(stdout);
        }
    }

    // Advance the recording frame counter
    if (adapter->isRecording)
    {
        adapter->recordFrame++;
    }
}

// Set the output mode for the adapter
void AISetOutputMode(AIAdapter *adapter, AIOutputMode mode)
{
    if (adapter == NULL) return;
    adapter->outputMode = mode;
}

//----------------------------------------------------------------------------------
// Scene Description Functions
//----------------------------------------------------------------------------------

// Engine-level draw observer (spec M3): bridges raylib_observability.h hooks
// into scene registration, so stock games using native DrawXXX (no AIDrawXXX
// wrappers) become observable without game-code changes. Registered with owner =
// the enabling adapter; AIAutoCaptureCallback services that owner directly (no
// aiAdapters[0] assumption — single-adapter premise enforced via AI_ERR_BUSY).
static void AIAutoCaptureCallback(AIObservedDraw type, float x, float y, float w, float h, float radius, float rotation, const char *text, Color color)
{
    (void)rotation;

    RaylibContext *ctx = GetRaylibContext();

    // The observer is owned by the adapter that registered it (or game code with
    // owner == NULL); service the owner directly instead of assuming aiAdapters[0].
    AIAdapter *adapter = (AIAdapter *)ctx->aiDrawObserverOwner;
    if (adapter == NULL || !adapter->autoCaptureEnabled) return;

    AIDrawElement elem = { 0 };
    switch (type)
    {
        case AI_OBS_RECT:
            elem.type = AI_DRAW_RECT;
            elem.x = x;
            elem.y = y;
            elem.w = w;
            elem.h = h;
            break;
        case AI_OBS_CIRCLE:
            elem.type = AI_DRAW_CIRCLE;
            elem.x = x;
            elem.y = y;
            elem.radius = radius;
            break;
        case AI_OBS_TEXT:
            if (text == NULL) return;
            elem.type = AI_DRAW_TEXT;
            elem.x = x;
            elem.y = y;
            elem.fontSize = (int)w;
            strncpy(elem.text, text, sizeof(elem.text) - 1);
            elem.text[sizeof(elem.text) - 1] = '\0';
            break;
        case AI_OBS_RECT_ROUNDED:
            elem.type = AI_DRAW_RECT_ROUNDED;
            elem.x = x;
            elem.y = y;
            elem.w = w;
            elem.h = h;
            elem.radius = radius;   // roundness rides the radius slot (same as AIDrawRectRounded)
            break;
        case AI_OBS_CIRCLE_SECTOR:
            elem.type = AI_DRAW_CIRCLE_SECTOR;
            elem.x = x;
            elem.y = y;
            elem.radius = radius;
            elem.rotation = rotation;  // startAngle (same as AIDrawCircleSector)
            elem.sides = (int)w;       // endAngle (same as AIDrawCircleSector)
            break;
        default:
            return;  // Hook kinds not yet bridged
    }
    elem.color = color;

    AIAddElement(adapter, &elem);
}

// Select the input simulation mode for this adapter (spec R6)
void AISetInputMode(AIAdapter *adapter, AISimulateMode mode)
{
    if (adapter == NULL) return;
    adapter->inputMode = mode;
    AISetInputSimulationMode(mode);
}

// Enable/disable engine-level automatic scene capture (spec M3)
void AISetAutoSceneCapture(AIAdapter *adapter, bool enable)
{
    if (adapter == NULL) return;

    RaylibContext *ctx = GetRaylibContext();

    if (enable)
    {
        if (adapter->autoCaptureEnabled) return;   // already ours

        // Ownership check: the observer must be free, or already owned by us.
        // A second adapter enabling auto-capture while owned by another must NOT
        // silently steal it — report AI_ERR_BUSY (single-adapter premise).
        if (ctx->aiDrawObserver != NULL && ctx->aiDrawObserverOwner != (void *)adapter)
        {
            AI_SET_ERROR(adapter, AI_ERR_BUSY,
                         "auto-scene-capture: draw observer is already owned by another adapter");
            TRACELOG(LOG_WARNING, "AIADAPTER: auto-scene-capture denied - draw observer owned by another adapter");
            return;
        }

        adapter->autoCaptureEnabled = true;
        SetAIDrawObserverOwned(AIAutoCaptureCallback, adapter);
    }
    else
    {
        adapter->autoCaptureEnabled = false;
        if (ctx->aiDrawObserverOwner == (void *)adapter)
        {
            SetAIDrawObserverOwned(NULL, NULL);
        }
    }
}

//----------------------------------------------------------------------------------
// Drawing Wrapper Functions
//----------------------------------------------------------------------------------
// Each wrapper calls the corresponding raylib DrawXXX function, then registers
// the element with the active adapter (first in context). If no adapter exists,
// the draw still happens — rendering is never blocked by the adapter
//----------------------------------------------------------------------------------

//----------------------------------------------------------------------------------
// M4-slim drawing wrappers (one function per AIDrawElementType, scalar params)
// Migration from the 33 legacy wrappers: docs/ai_api_迁移映射表.md
//----------------------------------------------------------------------------------

// Rectangles
// Error and Log Feedback Functions
//----------------------------------------------------------------------------------

// Get all captured log entries
const AILogEntry *AIGetLogs(AIAdapter *adapter, int *count)
{
    if (adapter == NULL || count == NULL) return NULL;

    *count = adapter->logCount;
    return adapter->logBuffer;
}

// Get the most recent log entry in logical order (0 = oldest retained)
const AILogEntry *AIGetLogAt(AIAdapter *adapter, int index)
{
    if (adapter == NULL || index < 0) return NULL;

    AI_LOG_LOCK(adapter);
    if (index >= adapter->logCount)
    {
        AI_LOG_UNLOCK(adapter);
        return NULL;
    }
    // Physical ring order differs from logical order after wrap-around (D3)
    const AILogEntry *entry = &adapter->logBuffer[AI_LOG_INDEX(adapter, index)];
    AI_LOG_UNLOCK(adapter);
    return entry;
}

// Clear all captured log entries
void AIClearLogs(AIAdapter *adapter)
{
    if (adapter == NULL) return;

    AI_LOG_LOCK(adapter);
    for (int i = 0; i < adapter->logCount; i++)
    {
        int idx = AI_LOG_INDEX(adapter, i);
        if (adapter->logBuffer[idx].message != NULL)
        {
            RL_FREE((void *)adapter->logBuffer[idx].message);
            adapter->logBuffer[idx].message = NULL;
        }
    }
    adapter->logCount = 0;
    adapter->logHead = 0;
    AI_LOG_UNLOCK(adapter);
}

// Symbolic name of a structured error code (T7). Kept next to AIGetErrorsJSON
// so the `errors` command reports the same classification as
// <<AI_ERROR>> [code:N] and AIGetLastError().
static const char *AIErrorName(AIErrorCode code)
{
    switch (code)
    {
        case AI_ERR_NONE:            return "AI_ERR_NONE";
        case AI_ERR_UNKNOWN_COMMAND: return "AI_ERR_UNKNOWN_COMMAND";
        case AI_ERR_BAD_ARGUMENT:    return "AI_ERR_BAD_ARGUMENT";
        case AI_ERR_BUFFER_FULL:     return "AI_ERR_BUFFER_FULL";
        case AI_ERR_NO_DATA:         return "AI_ERR_NO_DATA";
        case AI_ERR_BUSY:            return "AI_ERR_BUSY";
        case AI_ERR_NOT_SUPPORTED:   return "AI_ERR_NOT_SUPPORTED";
        case AI_ERR_INTERNAL:        return "AI_ERR_INTERNAL";
        default:                     return "AI_ERR_INTERNAL";
    }
}

// Get the most recent error(s) formatted as a JSON string
const char *AIGetErrorsJSON(AIAdapter *adapter)
{
    if (adapter == NULL) return "[]";

    char *json = adapter->jsonBuffer;
    int offset = 0;
    int remaining = sizeof(adapter->jsonBuffer);

    AI_LOG_LOCK(adapter);  // Snapshot the ring while no other thread appends (D2)

    AI_JSON_APPEND(json, remaining, offset, "[\n");

    int first = 1;

    // T7: AI_SET_ERROR writes lastErrorCode/lastErrorMessage, but the ring
    // below only sees TraceLog messages. Without this bridge, `errors` never
    // exposed AI_ERR_BUFFER_FULL (or any other structured code) even though
    // AIGetLastError() and <<AI_ERROR>> [code:N] both knew about it. Emit the
    // last structured error as a first-class JSON entry at WARNING level:
    // a dropped scene element is a diagnostic, not an AIClose <<AI_CRASH>>.
    // Keep headroom so truncation never leaves a half-written object.
    if ((adapter->lastErrorCode != AI_ERR_NONE) && (remaining - offset >= 1200))
    {
        char escapedMsg[1024];
        AIEscapeJSON(adapter->lastErrorMessage, escapedMsg, sizeof(escapedMsg));
        AI_JSON_APPEND(json, remaining, offset,
            "  {\"level\": %d, \"code\": %d, \"error\": \"%s\", \"message\": \"%s\", \"time\": %.3f}",
            LOG_WARNING, (int)adapter->lastErrorCode, AIErrorName(adapter->lastErrorCode),
            escapedMsg, adapter->deps->getTime() - adapter->initTime);
        first = 0;
    }

    for (int i = 0; i < adapter->logCount; i++)
    {
        int idx = AI_LOG_INDEX(adapter, i);
        AILogEntry *entry = &adapter->logBuffer[idx];

        if (entry->logLevel >= LOG_WARNING)
        {
            // Reserve headroom for one full entry so truncation never leaves a
            // half-written (invalid) JSON object; the array closes cleanly below
            if (remaining - offset < 1200) break;

            if (!first)
            {
                AI_JSON_APPEND(json, remaining, offset, ",\n");
            }
            char escapedMsg[1024];
            AIEscapeJSON(entry->message ? entry->message : "", escapedMsg, sizeof(escapedMsg));
            AI_JSON_APPEND(json, remaining, offset,
                "  {\"level\": %d, \"message\": \"%s\", \"time\": %.3f}",
                entry->logLevel, escapedMsg, entry->timestamp);
            first = 0;
        }
    }

    AI_JSON_APPEND(json, remaining, offset, "\n]");

    AI_LOG_UNLOCK(adapter);

    return json;
}

// Get the last error code reported by the adapter (AI_ERR_NONE if none)
AIErrorCode AIGetLastError(AIAdapter *adapter)
{
    if (adapter == NULL) return AI_ERR_NONE;
    return adapter->lastErrorCode;
}

//----------------------------------------------------------------------------------
// Backend State Query Functions
//----------------------------------------------------------------------------------

//----------------------------------------------------------------------------------
// State Snapshot (candidate 4: injectable state queries)
//----------------------------------------------------------------------------------

// Default request-close implementation (spec M5): the "quit" command signals the
// game loop to exit by setting raylib's shouldClose flag.
static void AIDefaultRequestClose(void)
{
    GetRaylibContext()->core.Window.shouldClose = true;
}

// Default state snapshot implementation: reads live raylib core state directly.
// Fills the same AIStateSnapshot struct that mock tests fill with known values,
// so the JSON generators are pure formatters over the snapshot.
static void AIDefaultGetStateSnapshot(AIStateSnapshot *snap)
{
    CoreData *core = &GetRaylibContext()->core;
    memset(snap, 0, sizeof(*snap));

    // window
    snap->title = core->Window.title;
    snap->ready = core->Window.ready;
    snap->shouldClose = core->Window.shouldClose;
    snap->resizedLastFrame = core->Window.resizedLastFrame;
    snap->screenW = core->Window.screen.width;
    snap->screenH = core->Window.screen.height;
    snap->posX = core->Window.position.x;
    snap->posY = core->Window.position.y;
    snap->dispW = core->Window.display.width;
    snap->dispH = core->Window.display.height;
    snap->renderW = core->Window.render.width;
    snap->renderH = core->Window.render.height;
    snap->flags = core->Window.flags;

    // keyboard
    for (int i = 0; i < MAX_KEYBOARD_KEYS && snap->keysDownCount < AI_STATE_MAX_KEYS; i++)
    {
        if (core->Input.Keyboard.currentKeyState[i]) snap->keysDown[snap->keysDownCount++] = i;
    }
    for (int i = 0; i < core->Input.Keyboard.keyPressedQueueCount && snap->keysPressedCount < AI_STATE_MAX_KEYS; i++)
    {
        snap->keysPressed[snap->keysPressedCount++] = core->Input.Keyboard.keyPressedQueue[i];
    }
    for (int i = 0; i < core->Input.Keyboard.charPressedQueueCount && snap->charsPressedCount < AI_STATE_MAX_KEYS; i++)
    {
        snap->charsPressed[snap->charsPressedCount++] = core->Input.Keyboard.charPressedQueue[i];
    }

    // mouse
    snap->mouseX = core->Input.Mouse.currentPosition.x;
    snap->mouseY = core->Input.Mouse.currentPosition.y;
    for (int i = 0; i < MAX_MOUSE_BUTTONS && snap->mouseButtonsDownCount < AI_STATE_MAX_MOUSE_BUTTONS; i++)
    {
        if (core->Input.Mouse.currentButtonState[i]) snap->mouseButtonsDown[snap->mouseButtonsDownCount++] = i;
    }
    snap->wheelX = core->Input.Mouse.currentWheelMove.x;
    snap->wheelY = core->Input.Mouse.currentWheelMove.y;

    // touch
    snap->touchCount = core->Input.Touch.pointCount;
    if (snap->touchCount > AI_STATE_MAX_TOUCH) snap->touchCount = AI_STATE_MAX_TOUCH;
    for (int i = 0; i < snap->touchCount; i++)
    {
        snap->touchX[i] = core->Input.Touch.position[i].x;
        snap->touchY[i] = core->Input.Touch.position[i].y;
    }

    // gamepad
    for (int g = 0; g < MAX_GAMEPADS; g++)
    {
        if (!core->Input.Gamepad.ready[g]) continue;
        int gi = snap->gamepadCount;
        if (gi >= AI_STATE_MAX_GAMEPADS) break;
        snap->gamepadReady[gi] = true;
        snap->gamepadName[gi] = core->Input.Gamepad.name[g];
        for (int b = 0; b < MAX_GAMEPAD_BUTTONS; b++)
        {
            if (core->Input.Gamepad.currentButtonState[g][b] &&
                snap->gamepadButtonsCount[gi] < AI_STATE_MAX_GAMEPAD_BUTTONS)
            {
                snap->gamepadButtonsDown[gi][snap->gamepadButtonsCount[gi]++] = b;
            }
        }
        snap->gamepadAxisCount[gi] = core->Input.Gamepad.axisCount[g];
        if (snap->gamepadAxisCount[gi] > AI_STATE_MAX_GAMEPAD_AXES) snap->gamepadAxisCount[gi] = AI_STATE_MAX_GAMEPAD_AXES;
        for (int a = 0; a < snap->gamepadAxisCount[gi]; a++)
        {
            snap->gamepadAxes[gi][a] = core->Input.Gamepad.axisState[g][a];
        }
        snap->gamepadCount++;
    }

    // time
    snap->timeCurrent = core->Time.current;
    snap->timePrevious = core->Time.previous;
    snap->timeUpdate = core->Time.update;
    snap->timeDraw = core->Time.draw;
    snap->timeFrame = core->Time.frame;
    snap->timeTarget = core->Time.target;
    snap->frameCounter = core->Time.frameCounter;

    // audio (candidate 3): walk the buffer list via the miniaudio-free iterator;
    // only touches device state when the audio device is actually ready.
    snap->audioDeviceReady = IsAudioDeviceReady();
    snap->audioMasterVolume = 0.0f;
    if (snap->audioDeviceReady)
    {
        snap->audioMasterVolume = GetMasterVolume();
        RaylibContext *ctx = GetRaylibContext();
        struct rAudioBuffer *buf = GetFirstAudioBuffer(ctx->audioState);
        while (buf != NULL && snap->audioBufferCount < AI_STATE_MAX_AUDIO_BUFFERS)
        {
            AudioBufferInfo info;
            if (GetAudioBufferInfo(buf, &info))
            {
                AIStateAudioBuffer *ab = &snap->audioBuffers[snap->audioBufferCount++];
                ab->usage = info.usage;
                ab->playing = info.playing;
                ab->paused = info.paused;
                ab->looping = info.looping;
                ab->volume = info.volume;
                ab->pitch = info.pitch;
                ab->pan = info.pan;
                ab->progress = info.progress;
            }
            buf = GetNextAudioBuffer(buf);
        }
    }
}

// Get the current window state as a JSON string
const char *AIGetWindowStateJSON(AIAdapter *adapter)
{
    if (adapter == NULL) return "{}";

    char *json = adapter->jsonBuffer;
    int offset = 0;
    int remaining = sizeof(adapter->jsonBuffer);

    AIStateSnapshot snap;
    if (adapter->deps->getStateSnapshot != NULL) adapter->deps->getStateSnapshot(&snap);
    else AIDefaultGetStateSnapshot(&snap);

    // Window titles are arbitrary user strings: escape before embedding (C3)
    char escapedTitle[1600] = { 0 };
    AIEscapeJSON(snap.title ? snap.title : "", escapedTitle, sizeof(escapedTitle));

    AI_JSON_APPEND(json, remaining, offset,
        "{\n"
        "  \"title\": \"%s\",\n"
        "  \"ready\": %s,\n"
        "  \"shouldClose\": %s,\n"
        "  \"resizedLastFrame\": %s,\n"
        "  \"screen\": {\"width\": %d, \"height\": %d},\n"
        "  \"position\": {\"x\": %d, \"y\": %d},\n"
        "  \"display\": {\"width\": %d, \"height\": %d},\n"
        "  \"render\": {\"width\": %d, \"height\": %d},\n"
        "  \"flags\": %u,\n"
        "  \"fullscreen\": %s,\n"
        "  \"resizable\": %s,\n"
        "  \"undecorated\": %s,\n"
        "  \"hidden\": %s,\n"
        "  \"minimized\": %s,\n"
        "  \"maximized\": %s\n"
        "}",
        snap.title ? escapedTitle : "",
        snap.ready ? "true" : "false",
        snap.shouldClose ? "true" : "false",
        snap.resizedLastFrame ? "true" : "false",
        snap.screenW, snap.screenH,
        snap.posX, snap.posY,
        snap.dispW, snap.dispH,
        snap.renderW, snap.renderH,
        snap.flags,
        (snap.flags & FLAG_FULLSCREEN_MODE) ? "true" : "false",
        (snap.flags & FLAG_WINDOW_RESIZABLE) ? "true" : "false",
        (snap.flags & FLAG_WINDOW_UNDECORATED) ? "true" : "false",
        (snap.flags & FLAG_WINDOW_HIDDEN) ? "true" : "false",
        (snap.flags & FLAG_WINDOW_MINIMIZED) ? "true" : "false",
        (snap.flags & FLAG_WINDOW_MAXIMIZED) ? "true" : "false");

    return json;
}

// Get the current input state as a JSON string
const char *AIGetInputStateJSON(AIAdapter *adapter)
{
    if (adapter == NULL) return "{}";

    char *json = adapter->jsonBuffer;
    int offset = 0;
    int remaining = sizeof(adapter->jsonBuffer);

    AIStateSnapshot snap;
    if (adapter->deps->getStateSnapshot != NULL) adapter->deps->getStateSnapshot(&snap);
    else AIDefaultGetStateSnapshot(&snap);

    AI_JSON_APPEND(json, remaining, offset, "{\n");

    // Keyboard: keys currently held, keys pressed this frame, chars pressed this frame
    AI_JSON_APPEND(json, remaining, offset, "  \"keyboard\": {\n    \"keysDown\": [");
    int first = 1;
    for (int i = 0; i < snap.keysDownCount && offset < remaining; i++)
    {
        AI_JSON_APPEND(json, remaining, offset, "%s%d", first ? "" : ", ", snap.keysDown[i]);
        first = 0;
    }
    AI_JSON_APPEND(json, remaining, offset, "],\n    \"keysPressed\": [");
    first = 1;
    for (int i = 0; i < snap.keysPressedCount && offset < remaining; i++)
    {
        AI_JSON_APPEND(json, remaining, offset, "%s%d", first ? "" : ", ", snap.keysPressed[i]);
        first = 0;
    }
    AI_JSON_APPEND(json, remaining, offset, "],\n    \"charsPressed\": [");
    first = 1;
    for (int i = 0; i < snap.charsPressedCount && offset < remaining; i++)
    {
        AI_JSON_APPEND(json, remaining, offset, "%s%d", first ? "" : ", ", snap.charsPressed[i]);
        first = 0;
    }
    AI_JSON_APPEND(json, remaining, offset, "]\n  },\n");

    // Mouse: position, buttons down, wheel
    AI_JSON_APPEND(json, remaining, offset,
        "  \"mouse\": {\n"
        "    \"x\": %.0f,\n"
        "    \"y\": %.0f,\n"
        "    \"buttonsDown\": [",
        snap.mouseX, snap.mouseY);
    first = 1;
    for (int i = 0; i < snap.mouseButtonsDownCount && offset < remaining; i++)
    {
        AI_JSON_APPEND(json, remaining, offset, "%s%d", first ? "" : ", ", snap.mouseButtonsDown[i]);
        first = 0;
    }
    AI_JSON_APPEND(json, remaining, offset,
        "],\n    \"wheel\": {\"x\": %.0f, \"y\": %.0f}\n"
        "  },\n",
        snap.wheelX, snap.wheelY);

    // Touch: active touch points
    AI_JSON_APPEND(json, remaining, offset,
        "  \"touch\": {\n"
        "    \"count\": %d,\n"
        "    \"points\": [",
        snap.touchCount);
    for (int i = 0; i < snap.touchCount && offset < remaining; i++)
    {
        AI_JSON_APPEND(json, remaining, offset,
            "%s{\"x\": %.0f, \"y\": %.0f}",
            i == 0 ? "" : ", ",
            snap.touchX[i], snap.touchY[i]);
    }
    AI_JSON_APPEND(json, remaining, offset, "]\n  },\n");

    // Gamepad: per-device buttons down + axes
    AI_JSON_APPEND(json, remaining, offset, "  \"gamepad\": [");
    first = 1;
    for (int g = 0; g < snap.gamepadCount && offset < remaining; g++)
    {
        if (snap.gamepadReady[g])
        {
            if (!first) AI_JSON_APPEND(json, remaining, offset, ",");
            first = 0;
            // Gamepad names are arbitrary driver strings: escape before embedding (C3)
            char escapedName[800] = { 0 };
            AIEscapeJSON(snap.gamepadName[g] ? snap.gamepadName[g] : "", escapedName, sizeof(escapedName));
            AI_JSON_APPEND(json, remaining, offset,
                "\n    {\"ready\": true, \"name\": \"%s\", \"buttonsDown\": [",
                escapedName);
            int firstBtn = 1;
            for (int b = 0; b < snap.gamepadButtonsCount[g] && offset < remaining; b++)
            {
                AI_JSON_APPEND(json, remaining, offset, "%s%d", firstBtn ? "" : ", ", snap.gamepadButtonsDown[g][b]);
                firstBtn = 0;
            }
            AI_JSON_APPEND(json, remaining, offset, "], \"axes\": [");
            for (int a = 0; a < snap.gamepadAxisCount[g] && offset < remaining; a++)
            {
                AI_JSON_APPEND(json, remaining, offset, "%s%.3f", a == 0 ? "" : ", ", snap.gamepadAxes[g][a]);
            }
            AI_JSON_APPEND(json, remaining, offset, "]}");
        }
    }
    AI_JSON_APPEND(json, remaining, offset, "]\n  }\n");

    AI_JSON_APPEND(json, remaining, offset, "}");

    return json;
}

// Get the current timing state as a JSON string
const char *AIGetTimeStateJSON(AIAdapter *adapter)
{
    if (adapter == NULL) return "{}";

    char *json = adapter->jsonBuffer;
    int offset = 0;
    int remaining = sizeof(adapter->jsonBuffer);

    AIStateSnapshot snap;
    if (adapter->deps->getStateSnapshot != NULL) adapter->deps->getStateSnapshot(&snap);
    else AIDefaultGetStateSnapshot(&snap);

    double fps = (snap.timeFrame > 0.0) ? (1.0 / snap.timeFrame) : 0.0;

    AI_JSON_APPEND(json, remaining, offset,
        "{\n"
        "  \"current\": %.6f,\n"
        "  \"previous\": %.6f,\n"
        "  \"update\": %.6f,\n"
        "  \"draw\": %.6f,\n"
        "  \"frame\": %.6f,\n"
        "  \"target\": %.6f,\n"
        "  \"frameCounter\": %u,\n"
        "  \"fps\": %.1f\n"
        "}",
        snap.timeCurrent,
        snap.timePrevious,
        snap.timeUpdate,
        snap.timeDraw,
        snap.timeFrame,
        snap.timeTarget,
        snap.frameCounter,
        fps);

    return json;
}

//----------------------------------------------------------------------------------
// Audio State Query Function
//----------------------------------------------------------------------------------

// Get the current audio state as a JSON string
// NOTE: pure formatter over AIStateSnapshot (candidate 3) — no direct CORE reads.
const char *AIGetAudioStateJSON(AIAdapter *adapter)
{
    if (adapter == NULL) return "{}";

    char *json = adapter->jsonBuffer;
    int offset = 0;
    int remaining = sizeof(adapter->jsonBuffer);

    AIStateSnapshot snap;
    if (adapter->deps->getStateSnapshot != NULL) adapter->deps->getStateSnapshot(&snap);
    else AIDefaultGetStateSnapshot(&snap);

    if (!snap.audioDeviceReady)
    {
        AI_JSON_APPEND(json, remaining, offset,
            "{\n  \"device\": {\"ready\": false},\n  \"activeBuffers\": [],\n  \"bufferCount\": 0\n}");
        return json;
    }

    AI_JSON_APPEND(json, remaining, offset,
        "{\n"
        "  \"device\": {\"ready\": true, \"masterVolume\": %.3f},\n"
        "  \"activeBuffers\": [",
        snap.audioMasterVolume);

    int first = 1;
    int count = 0;
    for (int i = 0; i < snap.audioBufferCount && offset < remaining; i++)
    {
        AIStateAudioBuffer *ab = &snap.audioBuffers[i];
        if (!first) AI_JSON_APPEND(json, remaining, offset, ",");
        first = 0;

        const char *usageStr = (ab->usage == 0) ? "STATIC" : "STREAM";

        AI_JSON_APPEND(json, remaining, offset,
            "\n    {\"usage\": \"%s\", \"playing\": %s, \"paused\": %s, \"looping\": %s, "
            "\"volume\": %.3f, \"pitch\": %.3f, \"pan\": %.3f, \"progress\": %.3f}",
            usageStr,
            ab->playing ? "true" : "false",
            ab->paused ? "true" : "false",
            ab->looping ? "true" : "false",
            ab->volume, ab->pitch, ab->pan, ab->progress);

        count++;
    }

    AI_JSON_APPEND(json, remaining, offset,
        "\n  ],\n"
        "  \"bufferCount\": %d\n"
        "}", count);

    return json;
}

//----------------------------------------------------------------------------------
// Procedural Audio Functions
// Input Injection Functions
//----------------------------------------------------------------------------------

// Parse the next token from a string, returns pointer after the token and whitespace

// Inject an input action for AI-driven game testing

//----------------------------------------------------------------------------------
// Game Loop Control Functions (for AI hot-reload runtime)
//----------------------------------------------------------------------------------

void AIPauseGame(AIAdapter *adapter)
{
    if (adapter == NULL) return;
    adapter->gamePaused = true;
}

void AIResumeGame(AIAdapter *adapter)
{
    if (adapter == NULL) return;
    adapter->gamePaused = false;
    adapter->stepFrames = 0;
}

void AIStepFrame(AIAdapter *adapter, int frames)
{
    if (adapter == NULL || frames <= 0) return;
    if (!adapter->gamePaused) return;  // Only step while paused
    adapter->stepFrames = frames;
}

bool AIGamePaused(AIAdapter *adapter)
{
    if (adapter == NULL) return false;
    return adapter->gamePaused;
}

int AIGetStepFrames(AIAdapter *adapter)
{
    if (adapter == NULL) return 0;
    return adapter->stepFrames;
}

void AIConsumeStepFrame(AIAdapter *adapter)
{
    if (adapter == NULL) return;
    if (adapter->stepFrames > 0) adapter->stepFrames--;
}

//----------------------------------------------------------------------------------
// Command Loop Functions (stdin-driven AI control)
//----------------------------------------------------------------------------------

// Platform-specific mutex helpers

// Stdin reader thread function: reads lines from stdin and queues them
#if defined(_WIN32)
#endif

//----------------------------------------------------------------------------------