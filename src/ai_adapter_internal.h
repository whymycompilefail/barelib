/**********************************************************************************************
*
*   ai_adapter_internal.h - AI adapter internal shared types (across compilation units)
*
*   M4 code-comprehensibility refactor: raylib_ai_adapter.c was a single 2755-line file
*   with 8 stacked subsystems. This header carries the types/macros/state shared by the
*   split units (ai_audio.c / ai_scene.c / ai_input.c / ai_protocol.c) and the facade.
*
*   LICENSE: zlib/libpng
*
**********************************************************************************************/

#ifndef AI_ADAPTER_INTERNAL_H
#define AI_ADAPTER_INTERNAL_H

#include "raylib_ai_adapter.h"      // Public API + public types (AIDrawElement, AILogEntry, ...)

// Windows types used by struct AIAdapter (command thread handle/mutex)
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
    #ifdef DrawText
        #undef DrawText
    #endif
    #ifdef DrawTextEx
        #undef DrawTextEx
    #endif
#endif

//----------------------------------------------------------------------------------
// Internal constants (shared by all AI adapter units)
//----------------------------------------------------------------------------------
#define MAX_LOG_ENTRIES         256         // Maximum number of buffered log entries
#define MAX_JSON_STRING         65536       // Maximum JSON output buffer size
// Maximum scene elements per frame (override via compiler flag or config.h)
#ifndef MAX_AI_ELEMENTS
    // Overridable capacity (compile-time extension point: pass -DMAX_AI_ELEMENTS=n
    // to shrink for memory-constrained builds or to exercise overflow paths in tests)
    #define MAX_AI_ELEMENTS     1024
#endif
#define MAX_AI_TEXTURE_REGISTRY 256         // Maximum tracked textures (id → path)
#define MAX_AI_COMMANDS         64          // Maximum queued stdin commands
#define AI_COMMAND_MAX_LEN      512         // Maximum command text length
// MAX_AI_ADAPTERS is now defined in raylib_context.h (shared with core)

//----------------------------------------------------------------------------------
// Shared macros
//----------------------------------------------------------------------------------

// Circular buffer index helper: map logical index i to physical slot
#define AI_LOG_INDEX(adapter, i) (((adapter)->logHead - (adapter)->logCount + (i) + MAX_LOG_ENTRIES) % MAX_LOG_ENTRIES)

// Record a structured error on the adapter for AI feedback (see AIGetLastError)
#define AI_SET_ERROR(adapter, code, ...) \
    do { \
        (adapter)->lastErrorCode = (code); \
        snprintf((adapter)->lastErrorMessage, sizeof((adapter)->lastErrorMessage), __VA_ARGS__); \
    } while (0)

// Append via snprintf into a JSON buffer, clamping `off` to the buffer size
// snprintf returns the would-be length, which can exceed the remaining space;
// without clamping, `off` overshoots the buffer and later writes go past the
// end (heap OOB). All JSON generators must use this macro. The last byte is
// always kept NUL so callers may safely pass the buffer to %s even when the
// output was truncated with no room left for the closing write
#define AI_JSON_APPEND(json, size, off, ...) \
    do { \
        if ((off) > (size)) (off) = (size); \
        int _n = snprintf((json) + (off), (size_t)((size) - (off)), __VA_ARGS__); \
        if (_n < 0) _n = 0; \
        (off) += _n; \
        if ((off) > (size)) (off) = (size); \
        (json)[(size) - 1] = '\0'; \
    } while (0)

//----------------------------------------------------------------------------------
// Internal types (defined here instead of raylib_ai_adapter.c so each split unit
// can use them without reaching into the facade's file)
//----------------------------------------------------------------------------------

// State snapshot capacity limits (JSON serialization input)
// NOTE: lists are capped generously; simultaneous keys/mouse/touch well below these.
#define AI_STATE_MAX_KEYS            64
#define AI_STATE_MAX_MOUSE_BUTTONS   8
#define AI_STATE_MAX_TOUCH           10
#define AI_STATE_MAX_GAMEPADS        4
#define AI_STATE_MAX_GAMEPAD_BUTTONS 32
#define AI_STATE_MAX_GAMEPAD_AXES    8

// Snapshot of window/input/time state consumed by the state JSON generators.
// Internal type: AIDeps::getStateSnapshot fills it (default implementation reads
// RaylibContext->core directly; tests fill it with known values to assert JSON
// structure without a window). The generators are pure formatters over this struct.

// Audio buffer snapshot entry (active game/procedural audio sources)
#define AI_STATE_MAX_AUDIO_BUFFERS 8
typedef struct AIStateAudioBuffer {
    bool playing, paused, looping;
    float volume, pitch, pan, progress;
    int usage;   // 0 = STATIC, 1 = STREAM
} AIStateAudioBuffer;

typedef struct AIStateSnapshot {
    // ---- window ----
    const char *title;              // Window title (borrowed, do not free)
    bool ready, shouldClose, resizedLastFrame;
    int screenW, screenH, posX, posY, dispW, dispH, renderW, renderH;
    unsigned int flags;             // raylib window flags (FLAG_*)
    // ---- keyboard ----
    int keysDown[AI_STATE_MAX_KEYS]; int keysDownCount;
    int keysPressed[AI_STATE_MAX_KEYS]; int keysPressedCount;
    int charsPressed[AI_STATE_MAX_KEYS]; int charsPressedCount;
    // ---- mouse ----
    float mouseX, mouseY;
    int mouseButtonsDown[AI_STATE_MAX_MOUSE_BUTTONS]; int mouseButtonsDownCount;
    float wheelX, wheelY;
    // ---- touch ----
    int touchCount;
    float touchX[AI_STATE_MAX_TOUCH], touchY[AI_STATE_MAX_TOUCH];
    // ---- gamepad ----
    int gamepadCount;
    bool gamepadReady[AI_STATE_MAX_GAMEPADS];
    const char *gamepadName[AI_STATE_MAX_GAMEPADS];
    int gamepadButtonsDown[AI_STATE_MAX_GAMEPADS][AI_STATE_MAX_GAMEPAD_BUTTONS];
    int gamepadButtonsCount[AI_STATE_MAX_GAMEPADS];
    float gamepadAxes[AI_STATE_MAX_GAMEPADS][AI_STATE_MAX_GAMEPAD_AXES];
    int gamepadAxisCount[AI_STATE_MAX_GAMEPADS];
    // ---- time ----
    double timeCurrent, timePrevious, timeUpdate, timeDraw, timeFrame, timeTarget;
    unsigned int frameCounter;
    // ---- audio (candidate 3: moved off direct CORE reads) ----
    bool audioDeviceReady;
    float audioMasterVolume;
    AIStateAudioBuffer audioBuffers[AI_STATE_MAX_AUDIO_BUFFERS];
    int audioBufferCount;
} AIStateSnapshot;

// Internal: procedural audio slot for AI-generated sound effects
typedef struct AIAudioSlot {
    Sound sound;                // raylib Sound (static PCM buffer; plays once, no looping)
    bool active;                // Whether this slot is currently in use
    bool paused;                // Whether playback is paused (paused slots must not be recycled)
    float duration;             // Total duration in seconds
    float volume;               // Current volume (0.0-1.0)
    float pitch;                // Current pitch multiplier (1.0 = base)
    int waveform;               // AIWaveform type
} AIAudioSlot;

// Internal: texture registry entry (maps texture ID to file path)
typedef struct AITextureEntry {
    unsigned int id;            // Texture ID (texture.id)
    char path[256];             // File path passed to AILoadTexture()
} AITextureEntry;

// Internal: command queue entry for the stdin reader thread
typedef struct AICommand {
    char text[AI_COMMAND_MAX_LEN];  // Command text (one line from stdin)
} AICommand;

// Internal context for the AI adapter
struct AIAdapter {
    const AIDeps *deps;                     // Per-instance dependency table (points to defaultDeps or caller's table)
    AIDeps defaultDeps;                     // In-context default table (real raylib fns) — no global static
    AILogEntry logBuffer[MAX_LOG_ENTRIES];  // Circular log buffer
    int logCount;                           // Number of entries in log buffer
    int logHead;                            // Circular buffer head index
    double initTime;                        // Time when adapter was initialized
    char jsonBuffer[MAX_JSON_STRING];       // Reusable JSON output buffer

    // Scene element buffer (cleared each frame via AIClearBackground/AIClearElements)
    AIDrawElement elements[MAX_AI_ELEMENTS]; // Registered scene elements for this frame
    int elementCount;                       // Number of elements registered this frame
    int elementOverflowCount;               // Elements dropped due to buffer full (this frame)

    // Texture registry (persists across frames; populated by AILoadTexture)
    AITextureEntry textureRegistry[MAX_AI_TEXTURE_REGISTRY];
    int textureRegistryCount;
    int textureRegistryNext;    // Next slot to overwrite once the registry is full (circular)

    // Procedural audio slots (AI-generated sound effects)
    AIAudioSlot audioSlots[MAX_AI_AUDIO_SLOTS];

    // Output mode
    AIOutputMode outputMode;                // Output mode (memory or stdout)

    // Game loop control (for AI hot-reload runtime)
    bool gamePaused;                        // Whether the game loop is paused
    int stepFrames;                         // Remaining frames to step while paused

    // Command loop (stdin reader thread)
    AICommand commandQueue[MAX_AI_COMMANDS]; // Queued commands from stdin
    int commandQueueHead;                    // Queue read index
    int commandQueueTail;                    // Queue write index
    int commandQueueCount;                   // Number of pending commands
    volatile bool commandThreadRunning;      // Thread should keep running
    bool commandLoopStarted;                 // Whether the command loop was started
#if defined(_WIN32)
    HANDLE commandThread;                    // stdin reader thread handle
    CRITICAL_SECTION commandMutex;           // Protects commandQueue
    CRITICAL_SECTION logMutex;               // Protects logBuffer (audio thread also logs)
#endif

    // Record / playback system
    AIRecordEntry recordBuffer[MAX_AI_RECORD_ENTRIES]; // Recorded input entries
    int recordCount;                         // Number of recorded entries
    int recordFrame;                         // Current frame counter (incremented in AIUpdate)
    bool isRecording;                        // Whether input recording is active
    bool isPlayingBack;                      // Whether playback is active
    bool resumeRecordingAfterPlayback;       // Restore recording state when playback finishes
    int playbackIndex;                       // Next entry to replay
    int playbackFrame;                       // Current playback frame counter

    // Scene snapshots (for diff)
    AIDrawElement snapshotElements[MAX_AI_ELEMENTS]; // Latest snapshot element list
    int snapshotElementCount;                // Number of elements in latest snapshot
    int snapshotCount;                       // Total snapshots saved (cumulative)
    char diffBuffer[MAX_JSON_STRING];        // Reusable diff JSON output buffer

    // Error model (spec M2): last error reported for AI feedback
    AIErrorCode lastErrorCode;               // Last error code (AI_ERR_NONE = none)
    char lastErrorMessage[256];              // Last error message (human-readable)

    // Thread lifecycle (spec R5): when the stdin reader thread cannot be
    // stopped in time, defer freeing the adapter (and mutex) to process exit
    // to avoid use-after-free
    bool deferFree;                          // Adapter memory deferred to process exit

    // Engine-level auto scene capture (spec M3): this adapter has registered
    // the draw observer and receives native DrawXXX calls as scene elements
    bool autoCaptureEnabled;                 // Draw observer feeds this adapter

    // Input simulation (spec R6): PostMessage mode delays key-up by one frame
    // (flushed in AIUpdate) so IsKeyPressed sees the press instead of a same-frame
    // down+up pair. -1 = no pending key
    AISimulateMode inputMode;                // Active simulation mode
    int pendingKeyUp;                        // Key to release next AIUpdate (-1 = none)
    int pendingMouseUp;                      // Mouse button to release next AIUpdate (-1 = none)
};

#if defined(_WIN32)
    #define AI_COMMAND_LOCK(adapter)   EnterCriticalSection(&(adapter)->commandMutex)
    #define AI_COMMAND_UNLOCK(adapter) LeaveCriticalSection(&(adapter)->commandMutex)
    #define AI_LOG_LOCK(adapter)       EnterCriticalSection(&(adapter)->logMutex)
    #define AI_LOG_UNLOCK(adapter)     LeaveCriticalSection(&(adapter)->logMutex)
#else
    #define AI_COMMAND_LOCK(adapter)   (void)(adapter)
    #define AI_COMMAND_UNLOCK(adapter) (void)(adapter)
    #define AI_LOG_LOCK(adapter)       (void)(adapter)
    #define AI_LOG_UNLOCK(adapter)     (void)(adapter)
#endif

//----------------------------------------------------------------------------------
// Cross-unit function declarations (internal, not public API)
//----------------------------------------------------------------------------------

// ai_scene.c — scene subsystem
void AIAddElement(AIAdapter *adapter, const AIDrawElement *elem);  // called by facade auto-capture
int AIEscapeJSON(const char *input, char *output, size_t outputSize); // used by facade AIGetErrorsJSON

// Texture registry (cross-unit: tests and facade may probe path lookup)
void AIRegisterTexture(AIAdapter *adapter, unsigned int textureId, const char *path);
const char *AILookupTexturePath(AIAdapter *adapter, unsigned int textureId);

// ai_input.c — input subsystem
const char *AIParseToken(const char *input, char *token, int tokenSize); // shared with ai_protocol.c
void AIPlaybackUpdate(AIAdapter *adapter);  // called from AIUpdate (loose-frame replay, auto-stop)

// ai_protocol.c — command protocol subsystem
void AIProcessCommands(AIAdapter *adapter);  // called from AIUpdate

// ai_audio.c — procedural audio subsystem
// Default synthesis sample rate + hard sample-count cap (candidate 6): a huge
// AI-requested duration can no longer allocate unbounded PCM (1M samples ≈
// 22.7s @ 44.1kHz, 2MB buffer).
#define AI_AUDIO_SAMPLE_RATE  44100
#define AI_MAX_AUDIO_SAMPLES  1000000
short *GenerateWaveData(int sampleRate, float frequency, float duration, float volume, int waveform, unsigned int seed, int *outSampleCount); // PURE PCM synthesis (unit-testable)
int AIAllocSlot(AIAdapter *adapter);       // PURE slot allocation (-1 = full)
int AIPlayTone(AIAdapter *adapter, float frequency, float duration, float volume, int waveform, unsigned int seed);
int AIPlayNoise(AIAdapter *adapter, float duration, float volume, unsigned int seed);
void AIStopAudio(AIAdapter *adapter, int handle);
void AIPauseAudio(AIAdapter *adapter, int handle);
void AIResumeAudio(AIAdapter *adapter, int handle);
bool IsAIAudioPlaying(AIAdapter *adapter, int handle);
void AIAudioUpdate(AIAdapter *adapter);     // Auto-recycle finished slots (called from AIUpdate)
void AIAudioShutdown(AIAdapter *adapter);   // Stop+unload all slots (called from AIClose)

#endif // AI_ADAPTER_INTERNAL_H
