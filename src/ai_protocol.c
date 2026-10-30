/**********************************************************************************************
*
*   ai_protocol.c - AI adapter command protocol subsystem (M4 split from raylib_ai_adapter.c)
*
*   Owns: the stdin reader thread, the command queue, command parsing/execution
*   (AIProcessCommands) and the <<AI_ERROR>> structured error printer. Depends on
*   the scene/input/audio subsystems through their public APIs.
*
*   LICENSE: zlib/libpng
*
**********************************************************************************************/

#include "ai_adapter_internal.h"
#include "raylib.h"          // Required for: raylib API used by commands (GetRaylibContext, etc.)
#include "raylib_context.h"  // Required for: RaylibContext, GetRaylibContext()
#include "config.h"

#include <stdlib.h>          // Required for: atoi()
#include <string.h>          // Required for: strcmp(), strlen(), strncpy()
#include <stdio.h>           // Required for: printf(), fgets(), snprintf()
#include <stdarg.h>          // Required for: va_list, va_start, va_end

static DWORD WINAPI AICommandThreadFunc(LPVOID data)
{
    AIAdapter *adapter = (AIAdapter *)data;
    char line[AI_COMMAND_MAX_LEN] = { 0 };

    while (adapter->commandThreadRunning)
    {
        if (fgets(line, sizeof(line), stdin) == NULL) break;  // stdin closed

        // Strip trailing newline
        size_t len = strlen(line);
        while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r'))
        {
            line[--len] = '\0';
        }
        if (len == 0) continue;  // Skip empty lines

        // Enqueue
        AI_COMMAND_LOCK(adapter);
        if (adapter->commandQueueCount < MAX_AI_COMMANDS)
        {
            strncpy(adapter->commandQueue[adapter->commandQueueTail].text, line, AI_COMMAND_MAX_LEN - 1);
            adapter->commandQueue[adapter->commandQueueTail].text[AI_COMMAND_MAX_LEN - 1] = '\0';
            adapter->commandQueueTail = (adapter->commandQueueTail + 1) % MAX_AI_COMMANDS;
            adapter->commandQueueCount++;
        }
        AI_COMMAND_UNLOCK(adapter);
    }

    return 0;
}

void AIStartCommandLoop(AIAdapter *adapter)
{
    if (adapter == NULL || adapter->commandLoopStarted) return;

#if defined(_WIN32)
    InitializeCriticalSection(&adapter->commandMutex);
    adapter->commandQueueHead = 0;
    adapter->commandQueueTail = 0;
    adapter->commandQueueCount = 0;
    adapter->commandThreadRunning = true;

    adapter->commandThread = CreateThread(NULL, 0, AICommandThreadFunc, adapter, 0, NULL);
    if (adapter->commandThread == NULL)
    {
        adapter->commandThreadRunning = false;
        DeleteCriticalSection(&adapter->commandMutex);
        TRACELOG(LOG_WARNING, "AIADAPTER: Failed to start command loop thread");
        return;
    }

    adapter->commandLoopStarted = true;
    TRACELOG(LOG_INFO, "AIADAPTER: Command loop started (reading from stdin)");
#else
    TRACELOG(LOG_WARNING, "AIADAPTER: Command loop not supported on this platform");
#endif
}

// Emit one protocol output line through the injected sink (spec M5).
// Falls back to printf(stdout) when the deps table has no emitProtocol hook,
// so existing console-driven AI tooling keeps working unchanged.
static void AIPrintOutput(AIAdapter *adapter, const char *format, ...)
{
    // NOTE: scene/state/diff JSON can be far larger than a command line, so the
    // output buffer must be the full JSON size, not AI_COMMAND_MAX_LEN (512 B),
    // otherwise large scenes get truncated mid-stream (losing '<<AI_SCENE_END>>').
    char buffer[MAX_JSON_STRING];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    if (adapter != NULL && adapter->deps != NULL && adapter->deps->emitProtocol != NULL)
    {
        adapter->deps->emitProtocol(buffer);
    }
    else
    {
        printf("%s", buffer);
        fflush(stdout);
    }
}

static void AIPrintError(AIAdapter *adapter, AIErrorCode code, const char *format, ...)
{
    char buffer[AI_COMMAND_MAX_LEN];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    AIPrintOutput(adapter, "<<AI_ERROR>> [code:%d] %s\n", (int)code, buffer);
}

// Process all pending commands from the queue (called from AIUpdate)
void AIProcessCommands(AIAdapter *adapter)
{
    if (adapter == NULL || !adapter->commandLoopStarted) return;

    char cmd[AI_COMMAND_MAX_LEN];
    while (true)
    {
        // Dequeue one command
        AI_COMMAND_LOCK(adapter);
        if (adapter->commandQueueCount == 0)
        {
            AI_COMMAND_UNLOCK(adapter);
            break;
        }
        strncpy(cmd, adapter->commandQueue[adapter->commandQueueHead].text, AI_COMMAND_MAX_LEN - 1);
        cmd[AI_COMMAND_MAX_LEN - 1] = '\0';
        adapter->commandQueueHead = (adapter->commandQueueHead + 1) % MAX_AI_COMMANDS;
        adapter->commandQueueCount--;
        AI_COMMAND_UNLOCK(adapter);

        // Parse and execute
        char action[32] = { 0 };
        const char *args = AIParseToken(cmd, action, sizeof(action));
        if (action[0] == '\0') continue;

        if (strcmp(action, "version") == 0)
        {
            AIPrintOutput(adapter, "<<AI_OK>> protocol %s (barelib AI adapter)\n", AI_PROTOCOL_VERSION);
        }
        else if (strcmp(action, "scene") == 0)
        {
            const char *json = AIGetSceneJSON(adapter);
            // Emit markers on their own lines: the JSON may already fill the
            // whole output buffer, and appending the end marker in the same
            // format string would truncate it (breaking the AI stream parser).
            AIPrintOutput(adapter, "<<AI_SCENE_BEGIN>>\n");
            AIPrintOutput(adapter, "%s\n", json);
            AIPrintOutput(adapter, "<<AI_SCENE_END>>\n");
        }
        else if (strcmp(action, "input") == 0)
        {
            AIInjectInput(adapter, args);
            if (adapter->lastErrorCode != AI_ERR_NONE)
            {
                AIPrintError(adapter, adapter->lastErrorCode, "%s", adapter->lastErrorMessage);
            }
            else
            {
                AIPrintOutput(adapter, "<<AI_OK>> input: %s\n", args);
            }
        }
        else if (strcmp(action, "inputmode") == 0)
        {
            // Switch the Windows input simulation mode at runtime (spec R6):
            // sendinput (default, requires foreground window) or postmessage
            // (background-safe — lets an unattended AI tester actually drive
            // the game window)
            char modeName[32] = { 0 };
            AIParseToken(args, modeName, sizeof(modeName));
            if (strcmp(modeName, "postmessage") == 0)
            {
                AISetInputMode(adapter, AI_SIMULATE_POSTMESSAGE);
                AIPrintOutput(adapter, "<<AI_OK>> input mode: postmessage (background-safe)\n");
            }
            else if (strcmp(modeName, "sendinput") == 0 || modeName[0] == '\0')
            {
                AISetInputMode(adapter, AI_SIMULATE_SENDINPUT);
                AIPrintOutput(adapter, "<<AI_OK>> input mode: sendinput (requires foreground window)\n");
            }
            else
            {
                AIPrintError(adapter, AI_ERR_BAD_ARGUMENT, "inputmode: unknown mode '%s' (sendinput|postmessage)", modeName);
            }
        }
        else if (strcmp(action, "pause") == 0)
        {
            AIPauseGame(adapter);
            AIPrintOutput(adapter, "<<AI_OK>> paused\n");
        }
        else if (strcmp(action, "resume") == 0)
        {
            AIResumeGame(adapter);
            AIPrintOutput(adapter, "<<AI_OK>> resumed\n");
        }
        else if (strcmp(action, "step") == 0)
        {
            int frames = atoi(args);
            if (frames <= 0) frames = 1;
            AIStepFrame(adapter, frames);
            AIPrintOutput(adapter, "<<AI_OK>> stepping %d frame(s)\n", frames);
        }
        else if (strcmp(action, "state") == 0)
        {
            char category[32] = { 0 };
            AIParseToken(args, category, sizeof(category));
            const char *json = NULL;
            if (strcmp(category, "window") == 0) json = AIGetWindowStateJSON(adapter);
            else if (strcmp(category, "input") == 0) json = AIGetInputStateJSON(adapter);
            else if (strcmp(category, "time") == 0) json = AIGetTimeStateJSON(adapter);
            else if (strcmp(category, "audio") == 0) json = AIGetAudioStateJSON(adapter);
            else json = AIGetWindowStateJSON(adapter);  // Default: window
            AIPrintOutput(adapter, "<<AI_STATE_BEGIN>>\n");
            AIPrintOutput(adapter, "%s\n", json);
            AIPrintOutput(adapter, "<<AI_STATE_END>>\n");
        }
        else if (strcmp(action, "errors") == 0)
        {
            AIPrintOutput(adapter, "<<AI_ERRORS_BEGIN>>\n");
            AIPrintOutput(adapter, "%s\n", AIGetErrorsJSON(adapter));
            AIPrintOutput(adapter, "<<AI_ERRORS_END>>\n");
        }
        else if (strcmp(action, "quiet") == 0)
        {
            AISetOutputMode(adapter, AI_OUTPUT_MEMORY);
            AIPrintOutput(adapter, "<<AI_OK>> quiet mode (memory)\n");
        }
        else if (strcmp(action, "loud") == 0)
        {
            AISetOutputMode(adapter, AI_OUTPUT_STDOUT);
            AIPrintOutput(adapter, "<<AI_OK>> loud mode (stdout)\n");
        }
        else if (strcmp(action, "elements") == 0)
        {
            AIPrintOutput(adapter, "<<AI_OK>> elements: %d\n", AIGetElementCount(adapter));
        }
        else if (strcmp(action, "record") == 0)
        {
            char sub[16] = { 0 };
            AIParseToken(args, sub, sizeof(sub));
            if (strcmp(sub, "stop") == 0)
            {
                AIStopRecording(adapter);
                AIPrintOutput(adapter, "<<AI_OK>> recording stopped (%d entries)\n", AIGetRecordCount(adapter));
            }
            else
            {
                AIStartRecording(adapter);
                AIPrintOutput(adapter, "<<AI_OK>> recording started\n");
            }
        }
        else if (strcmp(action, "playback") == 0)
        {
            char sub[16] = { 0 };
            AIParseToken(args, sub, sizeof(sub));
            if (strcmp(sub, "stop") == 0)
            {
                AIStopPlayback(adapter);
                AIPrintOutput(adapter, "<<AI_OK>> playback stopped\n");
            }
            else
            {
                if (AIGetRecordCount(adapter) == 0)
                {
                    AIPrintError(adapter, AI_ERR_NO_DATA, "no recorded entries to play back");
                }
                else
                {
                    AIStartPlayback(adapter);
                    AIPrintOutput(adapter, "<<AI_OK>> playback started (%d entries)\n", AIGetRecordCount(adapter));
                }
            }
        }
        else if (strcmp(action, "snapshot") == 0)
        {
            char sub[16] = { 0 };
            AIParseToken(args, sub, sizeof(sub));
            if (strcmp(sub, "clear") == 0)
            {
                AIClearSnapshots(adapter);
                AIPrintOutput(adapter, "<<AI_OK>> snapshots cleared\n");
            }
            else
            {
                AISaveSnapshot(adapter);
                AIPrintOutput(adapter, "<<AI_OK>> snapshot saved (elements: %d, total snapshots: %d)\n",
                       AIGetElementCount(adapter), AIGetSnapshotCount(adapter));
            }
        }
        else if (strcmp(action, "diff") == 0)
        {
            const char *json = AIGetSnapshotDiff(adapter);
            AIPrintOutput(adapter, "<<AI_DIFF_BEGIN>>\n");
            AIPrintOutput(adapter, "%s\n", json);
            AIPrintOutput(adapter, "<<AI_DIFF_END>>\n");
        }
        else if (strcmp(action, "help") == 0)
        {
            AIPrintOutput(adapter, "<<AI_HELP>>\n");
            AIPrintOutput(adapter, "Commands:\n");
            AIPrintOutput(adapter, "  version            - Print protocol version\n");
            AIPrintOutput(adapter, "  scene              - Get current scene JSON\n");
            AIPrintOutput(adapter, "  input <action>     - Inject input (press/click/move/scroll/type)\n");
            AIPrintOutput(adapter, "  pause              - Pause game loop\n");
            AIPrintOutput(adapter, "  resume             - Resume game loop\n");
            AIPrintOutput(adapter, "  step [n]           - Step n frames (default: 1)\n");
            AIPrintOutput(adapter, "  state [cat]        - Get state JSON (window/input/time/audio)\n");
            AIPrintOutput(adapter, "  errors             - Get error log JSON\n");
            AIPrintOutput(adapter, "  quiet              - Stop auto-stdout scene output\n");
            AIPrintOutput(adapter, "  loud               - Start auto-stdout scene output\n");
            AIPrintOutput(adapter, "  elements           - Get element count\n");
            AIPrintOutput(adapter, "  record             - Start recording input actions\n");
            AIPrintOutput(adapter, "  record stop        - Stop recording\n");
            AIPrintOutput(adapter, "  playback           - Replay recorded input\n");
            AIPrintOutput(adapter, "  playback stop      - Stop playback\n");
            AIPrintOutput(adapter, "  snapshot [save]    - Save current scene snapshot\n");
            AIPrintOutput(adapter, "  snapshot clear     - Clear all snapshots\n");
            AIPrintOutput(adapter, "  diff               - Diff current scene vs latest snapshot\n");
            AIPrintOutput(adapter, "  help               - Show this help\n");
            AIPrintOutput(adapter, "  quit               - Close the game\n");
            AIPrintOutput(adapter, "<<AI_HELP_END>>\n");
        }
        else if (strcmp(action, "quit") == 0)
        {
            // Signal the game to close via the injected requestClose hook (spec M5)
            if (adapter->deps != NULL && adapter->deps->requestClose != NULL)
            {
                adapter->deps->requestClose();
            }
            AIPrintOutput(adapter, "<<AI_OK>> quitting\n");
        }
        else
        {
            AIPrintError(adapter, AI_ERR_UNKNOWN_COMMAND, "unknown command: %s (try 'help')", action);
        }
    }
}
