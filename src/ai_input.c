/**********************************************************************************************
*
*   ai_input.c - AI adapter input subsystem (M4 split from raylib_ai_adapter.c)
*
*   Owns: key-name parsing, input action injection (AIInjectInput), and the
*   record/playback system. The Windows SendInput/PostMessage implementations
*   live in raylib_ai_input_win32.c behind the AIDeps table.
*
*   LICENSE: zlib/libpng
*
**********************************************************************************************/

#include "ai_adapter_internal.h"
#include "raylib.h"          // Required for: raylib keycodes
#include "raylib_ai_input.h" // Required for: DefaultSimulateKeyDown(), etc.
#include "config.h"

#include <string.h>          // Required for: strcmp(), strlen()
#include <stdio.h>           // Required for: snprintf()

//----------------------------------------------------------------------------------
// Key Name to Keycode Lookup
//----------------------------------------------------------------------------------

static int AIKeyNameToKeycode(const char *name)
{
    if (name == NULL || *name == '\0') return -1;

    // Single character: letter or digit
    if (name[1] == '\0')
    {
        char c = name[0];
        if (c >= 'a' && c <= 'z') return (int)c - 32; // KEY_A = 65, etc.
        if (c >= 'A' && c <= 'Z') return (int)c;       // Already uppercase
        if (c >= '0' && c <= '9') return (int)c;       // KEY_ZERO = 48, etc.
        return -1;
    }

    // Lookup table for named keys
    // See raylib_core.h for the full keycode enum
    static const struct { const char *name; int code; } keyTable[] = {
        // Special keys
        { "space",      32 },
        { "escape",    256 },
        { "enter",     257 },
        { "tab",       258 },
        { "backspace", 259 },
        { "insert",    260 },
        { "delete",    261 },
        { "right",     262 },
        { "left",      263 },
        { "down",      264 },
        { "up",        265 },
        { "pageup",    266 },
        { "pagedown",  267 },
        { "home",      268 },
        { "end",       269 },
        // Modifiers
        { "shift",     340 },
        { "ctrl",      341 },
        { "alt",       342 },
        // F-keys
        { "f1",        290 }, { "f2",  291 }, { "f3",  292 },
        { "f4",        293 }, { "f5",  294 }, { "f6",  295 },
        { "f7",        296 }, { "f8",  297 }, { "f9",  298 },
        { "f10",       299 }, { "f11", 300 }, { "f12", 301 },
        // Keypad
        { "kp0",       320 }, { "kp1", 321 }, { "kp2", 322 },
        { "kp3",       323 }, { "kp4", 324 }, { "kp5", 325 },
        { "kp6",       326 }, { "kp7", 327 }, { "kp8", 328 },
        { "kp9",       329 },
        { "kpdecimal", 330 }, { "kpdivide", 331 },
        { "kpmultiply", 332 }, { "kpsubtract", 333 },
        { "kpadd",     334 }, { "kpenter", 335 },
        { "kpequal",   336 },
    };

    // Case-insensitive comparison
    for (int i = 0; i < (int)(sizeof(keyTable)/sizeof(keyTable[0])); i++)
    {
        const char *a = name;
        const char *b = keyTable[i].name;
        bool match = true;
        while (*a && *b)
        {
            char ca = (*a >= 'A' && *a <= 'Z') ? *a + 32 : *a;
            char cb = (*b >= 'A' && *b <= 'Z') ? *b + 32 : *b;
            if (ca != cb) { match = false; break; }
            a++; b++;
        }
        if (match && *a == *b) return keyTable[i].code;
    }

    return -1;
}

// Parse the next token from a string, returns pointer after the token and whitespace
// (cross-unit: used by ai_protocol.c command parsing)
const char *AIParseToken(const char *input, char *token, int tokenSize)
{
    if (input == NULL || token == NULL || tokenSize <= 0) return NULL;
    int i = 0;
    while (*input == ' ') input++;
    while (*input && *input != ' ' && i < tokenSize - 1) token[i++] = *input++;
    token[i] = '\0';
    while (*input == ' ') input++;
    return input;
}

// Inject an input action for AI-driven game testing
void AIInjectInput(AIAdapter *adapter, const char *action)
{
    if (adapter == NULL) return;
    if (action == NULL)
    {
        AI_SET_ERROR(adapter, AI_ERR_BAD_ARGUMENT, "input: null action");
        return;
    }

    // Reset last error; set below on invalid action strings (error model feedback)
    adapter->lastErrorCode = AI_ERR_NONE;
    adapter->lastErrorMessage[0] = '\0';

    if (*action == '\0')
    {
        AI_SET_ERROR(adapter, AI_ERR_BAD_ARGUMENT, "input: empty action");
        return;
    }

    // Record the raw input action if recording is active
    if (adapter->isRecording && adapter->recordCount < MAX_AI_RECORD_ENTRIES)
    {
        AIRecordEntry *entry = &adapter->recordBuffer[adapter->recordCount];
        entry->frame = adapter->recordFrame;
        strncpy(entry->action, action, sizeof(entry->action) - 1);
        entry->action[sizeof(entry->action) - 1] = '\0';
        adapter->recordCount++;
    }

    // Parse the command word
    char cmd[32] = { 0 };
    action = AIParseToken(action, cmd, sizeof(cmd));

    if (strcmp(cmd, "press") == 0)
    {
        // press <key> [down|up]  (default: tap = down + up)
        char keyName[32] = { 0 };
        action = AIParseToken(action, keyName, sizeof(keyName));
        int key = AIKeyNameToKeycode(keyName);
        if (key < 0)
        {
            AI_SET_ERROR(adapter, AI_ERR_BAD_ARGUMENT, "press: unknown key '%s'", keyName);
            return;
        }

        char mod[16] = { 0 };
        AIParseToken(action, mod, sizeof(mod));

        if (strcmp(mod, "down") == 0)
        {
            adapter->deps->simulateKeyDown(key);
        }
        else if (strcmp(mod, "up") == 0)
        {
            adapter->deps->simulateKeyUp(key);
        }
        else
        {
            // Tap: press and release. In PostMessage mode (background-safe
            // injection) the up event is delayed one frame (flushed by AIUpdate)
            // so the game's IsKeyPressed sees the press — a same-frame down+up
            // pair is processed by one PollEvents batch and cancels itself out.
            adapter->deps->simulateKeyDown(key);
            if (adapter->inputMode == AI_SIMULATE_POSTMESSAGE)
            {
                adapter->pendingKeyUp = key;
            }
            else
            {
                adapter->deps->simulateKeyUp(key);
            }
        }
        return;
    }
    else if (strcmp(cmd, "click") == 0)
    {
        // click <x> <y> [button]  (button: left|right|middle, default left)
        char xStr[16] = { 0 };
        char yStr[16] = { 0 };
        action = AIParseToken(action, xStr, sizeof(xStr));
        action = AIParseToken(action, yStr, sizeof(yStr));
        if (xStr[0] == '\0' || yStr[0] == '\0')
        {
            AI_SET_ERROR(adapter, AI_ERR_BAD_ARGUMENT, "click: missing x/y arguments");
            return;
        }
        int x = atoi(xStr);
        int y = atoi(yStr);

        char btnStr[16] = { 0 };
        AIParseToken(action, btnStr, sizeof(btnStr));
        int button = 0;  // 0 = left
        if (strcmp(btnStr, "right") == 0) button = 1;
        else if (strcmp(btnStr, "middle") == 0) button = 2;

        adapter->deps->simulateMouseMove(x, y);
        adapter->deps->simulateMouseDown(button);
        if (adapter->inputMode == AI_SIMULATE_POSTMESSAGE)
        {
            // Delay the release one frame so IsMouseButtonPressed still sees it
            adapter->pendingMouseUp = button;
        }
        else
        {
            adapter->deps->simulateMouseUp(button);
        }
        return;
    }
    else if (strcmp(cmd, "move") == 0)
    {
        // move <x> <y>
        char xStr[16] = { 0 };
        char yStr[16] = { 0 };
        action = AIParseToken(action, xStr, sizeof(xStr));
        action = AIParseToken(action, yStr, sizeof(yStr));
        adapter->deps->simulateMouseMove(atoi(xStr), atoi(yStr));
        return;
    }
    else if (strcmp(cmd, "scroll") == 0)
    {
        // scroll <dx> <dy>
        char dxStr[16] = { 0 };
        char dyStr[16] = { 0 };
        action = AIParseToken(action, dxStr, sizeof(dxStr));
        action = AIParseToken(action, dyStr, sizeof(dyStr));
        adapter->deps->simulateMouseWheel(atoi(dxStr), atoi(dyStr));
        return;
    }
    else if (strcmp(cmd, "type") == 0)
    {
        // type <text>  — simulate each character as a key press
        const char *p = action;
        while (*p == ' ') p++;
        while (*p != '\0')
        {
            // Map literal characters to raylib keycodes
            int key = -1;
            if (*p >= 'a' && *p <= 'z') key = (int)(*p) - 32;  // KEY_A = 65
            else if (*p >= 'A' && *p <= 'Z') key = (int)(*p);
            else if (*p >= '0' && *p <= '9') key = (int)(*p);
            else if (*p == ' ') key = 32;
            else if (*p == '\n') key = 257;  // enter
            else if (*p == '\t') key = 258;  // tab
            else if (*p == '\x1b') key = 256; // escape
            else if ((unsigned char)(*p) >= 0x80)
            {
                // Non-ASCII input is not supported: injecting the raw UTF-8 byte as
                // a keycode would produce garbage keystrokes (C6).
                AI_SET_ERROR(adapter, AI_ERR_BAD_ARGUMENT, "type: non-ASCII input is not supported");
                return;
            }
            else key = (int)(*p);

            if (key >= 0)
            {
                adapter->deps->simulateKeyDown(key);
                adapter->deps->simulateKeyUp(key);
            }
            p++;
        }
        return;
    }

    AI_SET_ERROR(adapter, AI_ERR_BAD_ARGUMENT, "input: unknown command '%s' (press|click|move|scroll|type)", cmd);
}

void AIStartRecording(AIAdapter *adapter)
{
    if (adapter == NULL) return;
    adapter->isRecording = true;
    adapter->recordFrame = 0;
    adapter->recordCount = 0;
    TRACELOG(LOG_INFO, "AIADAPTER: Recording started");
}

void AIStopRecording(AIAdapter *adapter)
{
    if (adapter == NULL) return;
    if (adapter->isRecording)
    {
        adapter->isRecording = false;
        TRACELOG(LOG_INFO, "AIADAPTER: Recording stopped (%d entries)", adapter->recordCount);
    }
}

bool AIIsRecording(AIAdapter *adapter)
{
    return (adapter != NULL) ? adapter->isRecording : false;
}

int AIGetRecordCount(AIAdapter *adapter)
{
    return (adapter != NULL) ? adapter->recordCount : 0;
}

void AIStartPlayback(AIAdapter *adapter)
{
    if (adapter == NULL || adapter->recordCount == 0)
    {
        TRACELOG(LOG_WARNING, "AIADAPTER: Cannot start playback (no recorded entries)");
        return;
    }

    // Playback never re-records its own injections: pause recording now and
    // restore the previous state when playback finishes (grill decision).
    adapter->resumeRecordingAfterPlayback = adapter->isRecording;
    adapter->isRecording = false;

    adapter->isPlayingBack = true;
    adapter->playbackIndex = 0;
    adapter->playbackFrame = 0;
    TRACELOG(LOG_INFO, "AIADAPTER: Playback started (%d entries)", adapter->recordCount);
}

// Advance playback by one game frame (called from AIUpdate)
// Loose-frame replay: inject every recorded entry whose frame <= current playback
// frame, in recorded order; auto-stop when the buffer is exhausted.
void AIPlaybackUpdate(AIAdapter *adapter)
{
    if (adapter == NULL || !adapter->isPlayingBack) return;

    // Replay all entries that should have fired by now (loose alignment)
    while (adapter->playbackIndex < adapter->recordCount &&
           adapter->recordBuffer[adapter->playbackIndex].frame <= adapter->playbackFrame)
    {
        AIRecordEntry *entry = &adapter->recordBuffer[adapter->playbackIndex];
        AIInjectInput(adapter, entry->action);
        adapter->playbackIndex++;
    }

    adapter->playbackFrame++;

    // Finished: auto-stop and restore recording state
    if (adapter->playbackIndex >= adapter->recordCount)
    {
        adapter->isPlayingBack = false;
        adapter->playbackFrame = 0;
        adapter->playbackIndex = 0;
        adapter->isRecording = adapter->resumeRecordingAfterPlayback;
        adapter->resumeRecordingAfterPlayback = false;
        TRACELOG(LOG_INFO, "AIADAPTER: Playback finished");
    }
}

void AIStopPlayback(AIAdapter *adapter)
{
    if (adapter == NULL) return;
    if (adapter->isPlayingBack)
    {
        adapter->isPlayingBack = false;
        TRACELOG(LOG_INFO, "AIADAPTER: Playback stopped (at entry %d/%d)",
                 adapter->playbackIndex, adapter->recordCount);
    }
}

bool AIIsPlayingBack(AIAdapter *adapter)
{
    return (adapter != NULL) ? adapter->isPlayingBack : false;
}
