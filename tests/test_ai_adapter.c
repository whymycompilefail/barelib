/**********************************************************************************************
*
*   test_ai_adapter.c - Unit tests for the AI adapter module
*
*   Tests core functionality using AIDeps mocks - no raylib window needed.
*
*   LICENSE: zlib/libpng
*
**********************************************************************************************/

#include "raylib_ai_adapter.h"
#include "raylib_ai_input.h"   // Required for: AISimulateMode
#include "raylib_observability.h"   // Required for: SetAIDrawObserver/GetAIDrawObserver (ownership tests)
#include "ai_adapter_internal.h"    // Required for: GenerateWaveData/AIAllocSlot pure-function tests
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

// Element buffer cap must match raylib_ai_adapter.c default; override both via
// the same compiler flag if the library is built with a custom MAX_AI_ELEMENTS.
#ifndef MAX_AI_ELEMENTS
    #define MAX_AI_ELEMENTS 1024
#endif

//----------------------------------------------------------------------------------
// Test Framework Macros
//----------------------------------------------------------------------------------

static int testsPassed = 0;
static int testsFailed = 0;

#define TEST_SECTION(name) \
    do { \
        printf("\n=== %s ===\n", name); \
    } while(0)

#define TEST(name, condition) \
    do { \
        if (!(condition)) { \
            printf("  FAIL: %s\n", name); \
            testsFailed++; \
        } else { \
            printf("  PASS: %s\n", name); \
            testsPassed++; \
        } \
    } while(0)

//----------------------------------------------------------------------------------
// Mock AIDeps
//----------------------------------------------------------------------------------

static int mockScreenWidth = 800;
static int mockScreenHeight = 600;
static double mockTime = 0.0;
static int mockLogCallbackCalled = 0;
static TraceLogCallback savedCallback = NULL;

static int mockGetScreenWidth(void) { return mockScreenWidth; }
static int mockGetScreenHeight(void) { return mockScreenHeight; }
static double mockGetTime(void) { return mockTime; }

static void mockSetTraceLogCallback(TraceLogCallback callback)
{
    mockLogCallbackCalled++;
    savedCallback = callback;
}

// Track input simulation calls
static int lastKeyDown = -1;
static int lastKeyUp = -1;
static int lastMouseX = -1;
static int lastMouseY = -1;
static int lastMouseButton = -1;
static int mouseUpCalls = 0;
static int lastWheelDX = 0;
static int lastWheelDY = 0;

// Injection sequence log (for playback tests)
static int keyDownLog[32];
static int keyDownLogCount = 0;

static void mockSimulateKeyDown(int key) { lastKeyDown = key; if (keyDownLogCount < 32) keyDownLog[keyDownLogCount++] = key; }
static void mockSimulateKeyUp(int key) { lastKeyUp = key; }
static void mockSimulateMouseMove(int x, int y) { lastMouseX = x; lastMouseY = y; }
static void mockSimulateMouseDown(int button) { lastMouseButton = button; }
static void mockSimulateMouseUp(int button) { lastMouseButton = button; mouseUpCalls++; }
static void mockSimulateMouseWheel(int dx, int dy) { lastWheelDX = dx; lastWheelDY = dy; }

// State snapshot mock (candidate 4): tests fill mockSnapshot with known values
// and assert the JSON generators format it faithfully.
static AIStateSnapshot mockSnapshot;
static void mockGetStateSnapshot(AIStateSnapshot *out) { *out = mockSnapshot; }

// Protocol output capture (spec M5): mock emitProtocol collects every line the
// command protocol emits; mock requestClose records quit requests.
static char gEmitBuf[MAX_JSON_STRING + 4096];
static int gEmitLen = 0;
static int gRequestCloseCalls = 0;

static void mockEmitProtocol(const char *text)
{
    int n = (int)strlen(text);
    if (gEmitLen + n < (int)sizeof(gEmitBuf) - 1)
    {
        memcpy(gEmitBuf + gEmitLen, text, n);
        gEmitLen += n;
        gEmitBuf[gEmitLen] = '\0';
    }
}
static void mockRequestClose(void) { gRequestCloseCalls++; }

static AIDeps mockDeps = {
    .getScreenWidth = mockGetScreenWidth,
    .getScreenHeight = mockGetScreenHeight,
    .getTime = mockGetTime,
    .setTraceLogCallback = mockSetTraceLogCallback,
    .getStateSnapshot = mockGetStateSnapshot,
    .simulateKeyDown = mockSimulateKeyDown,
    .simulateKeyUp = mockSimulateKeyUp,
    .simulateMouseMove = mockSimulateMouseMove,
    .simulateMouseDown = mockSimulateMouseDown,
    .simulateMouseUp = mockSimulateMouseUp,
    .simulateMouseWheel = mockSimulateMouseWheel,
    .emitProtocol = mockEmitProtocol,
    .requestClose = mockRequestClose,
};

// Helper to reset mock state between tests
static void ResetMocks(void)
{
    mockTime = 0.0;
    mockLogCallbackCalled = 0;
    savedCallback = NULL;
    lastKeyDown = -1;
    lastKeyUp = -1;
    lastMouseX = -1;
    lastMouseY = -1;
    lastMouseButton = -1;
    mouseUpCalls = 0;
    lastWheelDX = 0;
    lastWheelDY = 0;
    keyDownLogCount = 0;
    memset(&mockSnapshot, 0, sizeof(mockSnapshot));
    gEmitLen = 0;
    gEmitBuf[0] = '\0';
    gRequestCloseCalls = 0;
}

//----------------------------------------------------------------------------------
// Tests
//----------------------------------------------------------------------------------

// NOTE: Tests for scene element registration use the AIDrawXXX wrapper
// functions and AIRegisterElement/AIClearElements APIs. See raylib_ai_adapter.h.

static void TestSceneJSON(void)
{
    TEST_SECTION("Scene JSON Output");

    AIAdapter *adapter = AIInitEx(&mockDeps);

    // Scene JSON should at minimum contain screen info.
    // The elements array reflects whatever was captured by raylib's draw
    // functions this frame — without a window/GPU context, the buffer is
    // empty, so we only verify the screen block here.
    const char *json = AIGetSceneJSON(adapter);
    TEST("Scene JSON is not NULL", json != NULL);
    TEST("Scene JSON contains screen block", strstr(json, "\"screen\"") != NULL);
    TEST("Scene JSON contains width", strstr(json, "800") != NULL);
    TEST("Scene JSON contains height", strstr(json, "600") != NULL);
    TEST("Scene JSON contains elements array", strstr(json, "\"elements\"") != NULL);

    AIClose(adapter);
}

static void TestInputInjectionParsing(void)
{
    TEST_SECTION("Input Injection Parsing");

    AIAdapter *adapter = AIInitEx(&mockDeps);
    ResetMocks();

    // Test press (tap)
    AIInjectInput(adapter, "press space");
    TEST("press space: keyDown called", lastKeyDown >= 0);
    TEST("press space: keyUp called", lastKeyUp >= 0);

    // Test press with modifier
    ResetMocks();
    AIInjectInput(adapter, "press enter down");
    TEST("press enter down: keyDown called", lastKeyDown >= 0);

    ResetMocks();
    AIInjectInput(adapter, "press enter up");
    TEST("press enter up: keyUp called", lastKeyUp >= 0);

    // Test click
    ResetMocks();
    AIInjectInput(adapter, "click 400 300");
    TEST("click: mouse moved to 400", lastMouseX == 400);
    TEST("click: mouse moved to 300", lastMouseY == 300);

    // Test click right
    ResetMocks();
    AIInjectInput(adapter, "click 100 200 right");
    TEST("right click: mouse moved", lastMouseX == 100);

    // Test move
    ResetMocks();
    AIInjectInput(adapter, "move 800 600");
    TEST("move: mouse moved to 800", lastMouseX == 800);
    TEST("move: mouse moved to 600", lastMouseY == 600);

    // Test scroll
    ResetMocks();
    AIInjectInput(adapter, "scroll 0 -1");
    TEST("scroll: dy = -1", lastWheelDY == -1);

    // Test type
    ResetMocks();
    AIInjectInput(adapter, "type hello");
    TEST("type: keyDown called for 'h'", lastKeyDown >= 0);

    // Test NULL/empty safety
    AIInjectInput(NULL, "press space");   // Should not crash
    AIInjectInput(adapter, "");            // Should not crash
    AIInjectInput(adapter, NULL);          // Should not crash
    TEST("Empty action sets BAD_ARGUMENT", AIGetLastError(adapter) == AI_ERR_BAD_ARGUMENT);

    AIClose(adapter);
}

static void TestLogInterception(void)
{
    TEST_SECTION("Log Interception");

    // Reset to ensure clean state
    ResetMocks();

    AIAdapter *adapter = AIInitEx(&mockDeps);
    TEST("Log callback was installed on init", mockLogCallbackCalled >= 1);

    // Log a message through TraceLog
    // NOTE: Since we're mocking, the actual TraceLog won't call our hook.
    // We test the log buffer directly instead.
    TraceLog(LOG_INFO, "Test message %d", 42);

    int logCount = 0;
    const AILogEntry *logs = AIGetLogs(adapter, &logCount);
    TEST("AIGetLogs returns non-NULL", logs != NULL);
    TEST("AIGetLogs count/log consistency", ((logCount > 0 && logs != NULL) || logCount == 0));

    // Clear logs (should not crash)
    AIClearLogs(adapter);

    // Error JSON (should not crash)
    const char *errJson = AIGetErrorsJSON(adapter);
    TEST("AIGetErrorsJSON returns non-NULL", errJson != NULL);

    AIClose(adapter);
}

static void TestMultiInstance(void)
{
    TEST_SECTION("Multi-Instance Isolation");

    // Create two adapters with different mocks
    AIDeps depsA = mockDeps;
    AIDeps depsB = mockDeps;

    AIAdapter *adapterA = AIInitEx(&depsA);
    AIAdapter *adapterB = AIInitEx(&depsB);
    TEST("Two adapters created successfully", adapterA != NULL && adapterB != NULL);

    // Each adapter should produce its own JSON buffer (independent output)
    const char *jsonA = AIGetSceneJSON(adapterA);
    const char *jsonB = AIGetSceneJSON(adapterB);
    TEST("Adapter A JSON is non-NULL", jsonA != NULL);
    TEST("Adapter B JSON is non-NULL", jsonB != NULL);
    TEST("Adapter JSON buffers are independent pointers", jsonA != jsonB);

    // Closing one should not affect the other
    AIClose(adapterA);
    const char *jsonB2 = AIGetSceneJSON(adapterB);
    TEST("Adapter B JSON still valid after A is closed", jsonB2 != NULL);

    AIClose(adapterB);
}

// Protocol versioning (spec M2): scene JSON carries a top-level "protocol" field
static void TestProtocolVersion(void)
{
    TEST_SECTION("Protocol Version");

    AIAdapter *adapter = AIInitEx(&mockDeps);
    TEST("AI_PROTOCOL_VERSION is non-empty", AI_PROTOCOL_VERSION[0] != '\0');

    const char *json = AIGetSceneJSON(adapter);
    TEST("Scene JSON contains protocol field", strstr(json, "\"protocol\"") != NULL);
    TEST("Scene JSON reports current protocol version", strstr(json, AI_PROTOCOL_VERSION) != NULL);

    AIClose(adapter);
}

// Element buffer capacity (spec N2): overflow must be explicit, not silent
static void TestElementOverflow(void)
{
    TEST_SECTION("Element Buffer Overflow");

    AIAdapter *adapter = AIInitEx(&mockDeps);

    AIDrawElement elem = { 0 };
    elem.type = AI_DRAW_RECT;
    elem.x = 1.0f; elem.y = 2.0f; elem.w = 10.0f; elem.h = 20.0f;
    elem.color = (Color){ 255, 0, 0, 255 };

    // Register MAX_AI_ELEMENTS + 5: 5 elements must be dropped but reported
    int dropped = 5;
    for (int i = 0; i < MAX_AI_ELEMENTS + dropped; i++) AIRegisterElement(adapter, &elem);

    TEST("Element count capped at MAX_AI_ELEMENTS", AIGetElementCount(adapter) == MAX_AI_ELEMENTS);

    const char *json = AIGetSceneJSON(adapter);
    TEST("Scene JSON reports overflow field", strstr(json, "\"overflow\"") != NULL);
    TEST("Scene JSON reports exactly 5 dropped elements", (strstr(json, "\"overflow\": 5") != NULL));

    // Overflow resets when the element buffer is cleared (new frame)
    AIClearElements(adapter);
    const char *json2 = AIGetSceneJSON(adapter);
    TEST("Overflow cleared after AIClearElements", strstr(json2, "\"overflow\"") == NULL);

    AIClose(adapter);
}

// Scene JSON generation latency baseline (spec M2, §3.4)
// With a full element buffer (worst case), AIGetSceneJSON() must stay far below
// one frame (16ms) so AI observation never drags the game loop. Prints the
// measured average as the baseline; asserts a loose ceiling to catch
// catastrophic regressions without being flaky on slow/CI machines.
static void TestSceneJSONPerformance(void)
{
    TEST_SECTION("Scene JSON Performance Baseline");

    AIAdapter *adapter = AIInitEx(&mockDeps);

    // Fill the element buffer to its cap (worst case for JSON generation)
    AIDrawElement elem = { 0 };
    elem.type = AI_DRAW_TEXT;
    elem.x = 1.0f; elem.y = 2.0f; elem.fontSize = 20;
    strncpy(elem.text, "perf baseline", sizeof(elem.text) - 1);
    elem.color = (Color){ 255, 255, 255, 255 };
    for (int i = 0; i < MAX_AI_ELEMENTS; i++) AIRegisterElement(adapter, &elem);

    const int iters = 500;
    clock_t start = clock();
    const char *json = NULL;
    for (int i = 0; i < iters; i++) json = AIGetSceneJSON(adapter);
    clock_t elapsed = clock() - start;

    double avgMs = (double)elapsed * 1000.0 / (double)CLOCKS_PER_SEC / (double)iters;
    printf("  INFO: avg scene JSON generation: %.3f ms (%d elements, %d iters)\n",
           avgMs, AIGetElementCount(adapter), iters);

    TEST("Scene JSON generation avg < 16ms (one frame)", avgMs < 16.0);
    TEST("Scene JSON non-NULL under full load", json != NULL);

    // The 1024-element scene overflows the 64KB JSON buffer: the output must
    // stay structurally valid and report the truncation
    size_t plen = strlen(json);
    TEST("Perf JSON is structurally sound", json != NULL && json[0] == '{' && plen > 0 && json[plen - 1] == '}');
    TEST("Perf JSON reports truncation", strstr(json, "\"truncated\": true") != NULL);

    AIClose(adapter);
}

// spec M2/N2: when the scene JSON exceeds its buffer, output must remain valid
// JSON (no trailing comma, properly closed) and report truncation explicitly
static void TestSceneJSONTruncation(void)
{
    TEST_SECTION("Scene JSON Truncation Safety");

    AIAdapter *adapter = AIInitEx(&mockDeps);

    // Long text elements make the JSON exceed the 64KB buffer (worst case)
    AIDrawElement elem = { 0 };
    elem.type = AI_DRAW_TEXT;
    elem.x = 1.0f; elem.y = 2.0f; elem.fontSize = 20;
    memset(elem.text, 'A', sizeof(elem.text) - 1);
    elem.text[sizeof(elem.text) - 1] = '\0';
    elem.color = (Color){ 255, 255, 255, 255 };
    for (int i = 0; i < MAX_AI_ELEMENTS; i++) AIRegisterElement(adapter, &elem);

    const char *json = AIGetSceneJSON(adapter);
    size_t len = strlen(json);

    TEST("Truncated JSON starts with '{'", json != NULL && json[0] == '{');
    TEST("Truncated JSON ends with '}'", len > 0 && json[len - 1] == '}');
    TEST("No trailing comma before ']'", strstr(json, ",\n  ]") == NULL);
    TEST("Elements array is closed", strstr(json, "\n  ]") != NULL);
    TEST("Truncation reported explicitly", strstr(json, "\"truncated\": true") != NULL);
    TEST("Overflow field present (buffer full vs dropped)", strstr(json, "\"overflow\": 0") != NULL);

    AIClose(adapter);
}

// Scene snapshot/diff regression (engine usage report #3): the report found the
// scene diff degraded to empty/all-removed with no test coverage. These tests
// exercise the snapshot→mutate→diff path headlessly and verify the cleared-scene
// case still reports removals explicitly (not an empty/blank diff).
static void TestSnapshotDiff(void)
{
    TEST_SECTION("Scene Snapshot and Diff");

    AIAdapter *adapter = AIInitEx(&mockDeps);

    // Empty scene: snapshot of nothing; diff stays valid JSON
    AISaveSnapshot(adapter);
    TEST("Snapshot count increments", AIGetSnapshotCount(adapter) == 1);
    const char *emptyDiff = AIGetSnapshotDiff(adapter);
    TEST("Empty diff is valid JSON", emptyDiff != NULL && strstr(emptyDiff, "\"changes\": [") != NULL);

    // Register two elements, snapshot, then mutate the scene
    AIDrawElement rect = { 0 };
    rect.type = AI_DRAW_RECT;
    rect.x = 10.0f; rect.y = 20.0f; rect.w = 100.0f; rect.h = 40.0f;
    rect.color = (Color){ 0, 0, 255, 255 };
    AIRegisterElement(adapter, &rect);

    AIDrawElement circle = { 0 };
    circle.type = AI_DRAW_CIRCLE;
    circle.x = 5.0f; circle.y = 5.0f; circle.radius = 10.0f;
    circle.color = (Color){ 255, 0, 0, 255 };
    AIRegisterElement(adapter, &circle);

    AISaveSnapshot(adapter);
    TEST("Snapshot captured 2 elements", AIGetSnapshotCount(adapter) == 2);

    // Mutate: move the rect, replace the circle with a text element
    AIClearElements(adapter);
    rect.x = 30.0f;                              // moved
    AIRegisterElement(adapter, &rect);
    AIDrawElement text = { 0 };
    text.type = AI_DRAW_TEXT;
    text.x = 1.0f; text.y = 2.0f; text.fontSize = 20;
    strncpy(text.text, "hi", sizeof(text.text) - 1);
    text.color = (Color){ 0, 0, 0, 255 };
    AIRegisterElement(adapter, &text);

    const char *diff = AIGetSnapshotDiff(adapter);
    TEST("Diff reports changed elements", strstr(diff, "\"changed\": 2") != NULL);
    TEST("Diff reports added 0", strstr(diff, "\"added\": 0") != NULL);
    TEST("Diff reports removed 0", strstr(diff, "\"removed\": 0") != NULL);
    TEST("Diff details include changed actions", strstr(diff, "\"action\": \"changed\"") != NULL);
    TEST("Diff JSON closes with changes array", strstr(diff, "\"changes\": [") != NULL);

    // Regression (report #3): after the scene is cleared, the diff must still
    // report the removals explicitly instead of degrading to a blank diff
    AIClearElements(adapter);
    const char *clearDiff = AIGetSnapshotDiff(adapter);
    TEST("Cleared scene diff reports all removed", strstr(clearDiff, "\"removed\": 2") != NULL);
    TEST("Cleared scene diff is valid JSON", clearDiff != NULL && clearDiff[0] == '{' && strstr(clearDiff, "}") != NULL);

    AIClose(adapter);
}

// Input simulation mode (spec R6): in PostMessage mode the key-up is delayed
// one frame (flushed by AIUpdate) so IsKeyPressed sees the press; SendInput
// mode keeps the immediate down+up behavior.
static void TestPostMessageKeyPressDelay(void)
{
    TEST_SECTION("Input Mode: PostMessage Press Delay");

    AIAdapter *adapter = AIInitEx(&mockDeps);

    // Default: SendInput mode — press releases immediately
    AIInjectInput(adapter, "press space");
    TEST("SendInput mode: keyDown called", lastKeyDown == KEY_SPACE);
    TEST("SendInput mode: keyUp called immediately", lastKeyUp == KEY_SPACE);

    // PostMessage mode: keyUp must be delayed until AIUpdate flushes it
    AISetInputMode(adapter, AI_SIMULATE_POSTMESSAGE);
    lastKeyDown = lastKeyUp = -1;
    AIInjectInput(adapter, "press space");
    TEST("PostMessage mode: keyDown called", lastKeyDown == KEY_SPACE);
    TEST("PostMessage mode: keyUp NOT yet called", lastKeyUp == -1);

    AIUpdate(adapter);
    TEST("PostMessage mode: AIUpdate flushes keyUp", lastKeyUp == KEY_SPACE);

    AIClose(adapter);
}

// Error model (spec M2): invalid input actions must be observable via AIGetLastError
static void TestInputErrorFeedback(void)
{
    TEST_SECTION("Input Error Feedback");

    AIAdapter *adapter = AIInitEx(&mockDeps);
    TEST("Last error is NONE initially", AIGetLastError(adapter) == AI_ERR_NONE);

    AIInjectInput(adapter, "press unknownkey123");
    TEST("Unknown key yields BAD_ARGUMENT", AIGetLastError(adapter) == AI_ERR_BAD_ARGUMENT);

    AIInjectInput(adapter, "click");
    TEST("Missing click args yield BAD_ARGUMENT", AIGetLastError(adapter) == AI_ERR_BAD_ARGUMENT);

    AIInjectInput(adapter, "press space");
    TEST("Valid input resets error to NONE", AIGetLastError(adapter) == AI_ERR_NONE);

    AIClose(adapter);
}

// M4-slim: the 16 AIDrawElementType values must serialize to their canonical
// JSON "type" name plus the type's distinguishing field (X-macro single source
// of truth — this table is the test-side expectation for the C-side list)
static void TestElementTypeTable(void)
{
    TEST_SECTION("Element Type Table (M4-slim)");

    AIAdapter *adapter = AIInitEx(&mockDeps);

    typedef struct TypeCase { AIDrawElementType type; const char *jsonName; const char *field; } TypeCase;
    static const TypeCase cases[] = {
        { AI_DRAW_CLEAR,          "clear",          "\"color\"" },
        { AI_DRAW_RECT,           "rect",           "\"w\": 10.0" },
        { AI_DRAW_RECT_LINES,     "rect_lines",     "\"w\": 10.0" },
        { AI_DRAW_RECT_ROUNDED,   "rect_rounded",   "\"radius\": 5.0" },
        { AI_DRAW_CIRCLE,         "circle",         "\"radius\": 5.0" },
        { AI_DRAW_CIRCLE_LINES,   "circle_lines",   "\"radius\": 5.0" },
        { AI_DRAW_CIRCLE_SECTOR,  "circle_sector",  "\"startAngle\": 30.0" },
        { AI_DRAW_TRIANGLE,       "triangle",       "\"v3\"" },
        { AI_DRAW_TRIANGLE_LINES, "triangle_lines", "\"v3\"" },
        { AI_DRAW_POLY,           "poly",           "\"sides\": 6" },
        { AI_DRAW_POLY_LINES,     "poly_lines",     "\"sides\": 6" },
        { AI_DRAW_LINE,           "line",           "\"x2\": 8.0" },
        { AI_DRAW_LINE_BEZIER,    "line_bezier",    "\"x2\": 8.0" },
        { AI_DRAW_TEXT,           "text",           "\"size\": 20" },
        { AI_DRAW_TEXTURE,        "texture",        "\"source\"" },
        { AI_DRAW_PIXEL,          "pixel",          "\"x\": 3.0" },
    };

    int total = (int)(sizeof(cases)/sizeof(cases[0]));
    for (int i = 0; i < total; i++)
    {
        AIClearElements(adapter);
        AIDrawElement elem = { 0 };
        elem.type = cases[i].type;
        elem.x = 3.0f; elem.y = 4.0f;
        elem.w = 10.0f; elem.h = 20.0f;
        elem.x2 = 8.0f; elem.y2 = 9.0f;
        elem.radius = 5.0f;
        elem.sides = 6;
        elem.rotation = 30.0f;   // sector startAngle
        elem.fontSize = 20;
        elem.color = (Color){ 1, 2, 3, 255 };
        strncpy(elem.text, "hi", sizeof(elem.text) - 1);
        strncpy(elem.source, "tex.png", sizeof(elem.source) - 1);
        AIRegisterElement(adapter, &elem);

        const char *json = AIGetSceneJSON(adapter);
        char expect[64];
        snprintf(expect, sizeof(expect), "\"type\": \"%s\"", cases[i].jsonName);
        char label[96];
        snprintf(label, sizeof(label), "Type %s serializes to JSON name + field", cases[i].jsonName);
        TEST(label, (strstr(json, expect) != NULL) && (strstr(json, cases[i].field) != NULL));
    }

    // 16 distinct canonical names (no duplicates, no gaps)
    const char *scene = AIGetSceneJSON(adapter);
    (void)scene;
    TEST("Element type table covers 16 types", total == 16);

    AIClose(adapter);
}

static void TestRecordPlayback(void)
{
    TEST_SECTION("Record/Playback (loose-frame replay)");

    AIAdapter *adapter = AIInitEx(&mockDeps);
    ResetMocks();

    // Record two actions at different frames
    AIStartRecording(adapter);
    AIInjectInput(adapter, "press a");       // recorded at frame 0
    AIUpdate(adapter);                        // recordFrame -> 1
    AIInjectInput(adapter, "press b");       // recorded at frame 1
    AIStopRecording(adapter);

    // Replay: A then B in order, then auto-stop (clear the recording-phase log first)
    ResetMocks();
    AIStartPlayback(adapter);
    AIUpdate(adapter);                        // playbackFrame 0 -> inject KEY_A
    AIUpdate(adapter);                        // playbackFrame 1 -> inject KEY_B
    AIUpdate(adapter);                        // playbackFrame 2 -> exhausted, auto-stop
    TEST("Playback injected KEY_A first", keyDownLogCount >= 1 && keyDownLog[0] == KEY_A);
    TEST("Playback injected KEY_B second", keyDownLogCount >= 2 && keyDownLog[1] == KEY_B);
    TEST("Playback injected exactly 2 keys", keyDownLogCount == 2);

    // After completion, further frames inject nothing (auto-stop, no loop)
    ResetMocks();
    AIUpdate(adapter);
    AIUpdate(adapter);
    TEST("Playback auto-stopped (no more injections)", keyDownLogCount == 0);

    // Recording restoration: playback pauses recording and restores it on finish.
    // NOTE: AIStartRecording clears the buffer, so this second session contains
    // only the action recorded AFTER playback finished (KEY_C) — that proves
    // recording was restored, while A/B stayed out of the new buffer.
    ResetMocks();
    AIStartRecording(adapter);              // recording active (clears buffer)
    AIStartPlayback(adapter);               // pauses recording
    for (int i = 0; i < 3; i++) AIUpdate(adapter);  // replay old A,B and finish
    AIInjectInput(adapter, "press c");      // recording restored -> entry #1 (new session)
    AIStopRecording(adapter);

    // Replay the new session: only KEY_C
    ResetMocks();
    AIStartPlayback(adapter);
    for (int i = 0; i < 8; i++) AIUpdate(adapter);
    TEST("Second replay injected 1 key (new session only)", keyDownLogCount == 1);
    TEST("Second replay injected KEY_C", keyDownLogCount >= 1 && keyDownLog[0] == KEY_C);

    AIClose(adapter);
}

static void TestStateSnapshotJSON(void)
{
    TEST_SECTION("State snapshot JSON (injectable state queries)");

    AIAdapter *adapter = AIInitEx(&mockDeps);
    ResetMocks();

    // Fill a known snapshot: the generators must format it faithfully
    mockSnapshot.title = "Test Game";
    mockSnapshot.ready = true;
    mockSnapshot.shouldClose = false;
    mockSnapshot.screenW = 640; mockSnapshot.screenH = 480;
    mockSnapshot.posX = 100; mockSnapshot.posY = 50;
    mockSnapshot.dispW = 1920; mockSnapshot.dispH = 1080;
    mockSnapshot.renderW = 640; mockSnapshot.renderH = 480;
    mockSnapshot.flags = FLAG_FULLSCREEN_MODE | FLAG_WINDOW_RESIZABLE;
    mockSnapshot.keysDownCount = 2; mockSnapshot.keysDown[0] = KEY_A; mockSnapshot.keysDown[1] = KEY_B;
    mockSnapshot.keysPressedCount = 1; mockSnapshot.keysPressed[0] = KEY_SPACE;
    mockSnapshot.charsPressedCount = 1; mockSnapshot.charsPressed[0] = 'x';
    mockSnapshot.mouseX = 123.0f; mockSnapshot.mouseY = 45.0f;
    mockSnapshot.mouseButtonsDownCount = 1; mockSnapshot.mouseButtonsDown[0] = 0;
    mockSnapshot.wheelX = 1.0f; mockSnapshot.wheelY = -2.0f;
    mockSnapshot.touchCount = 1; mockSnapshot.touchX[0] = 10.0f; mockSnapshot.touchY[0] = 20.0f;
    mockSnapshot.gamepadCount = 1; mockSnapshot.gamepadReady[0] = true;
    mockSnapshot.gamepadName[0] = "MockPad";
    mockSnapshot.gamepadButtonsCount[0] = 1; mockSnapshot.gamepadButtonsDown[0][0] = 7;
    mockSnapshot.gamepadAxisCount[0] = 2; mockSnapshot.gamepadAxes[0][0] = 0.5f; mockSnapshot.gamepadAxes[0][1] = -0.25f;
    mockSnapshot.timeCurrent = 1.5; mockSnapshot.timeFrame = 1.0/60.0;
    mockSnapshot.frameCounter = 42;

    // Window JSON
    const char *w = AIGetWindowStateJSON(adapter);
    TEST("window: title", strstr(w, "\"title\": \"Test Game\"") != NULL);
    TEST("window: screen dims", strstr(w, "\"screen\": {\"width\": 640, \"height\": 480}") != NULL);
    TEST("window: fullscreen flag", strstr(w, "\"fullscreen\": true") != NULL);
    TEST("window: resizable flag", strstr(w, "\"resizable\": true") != NULL);

    // Input JSON
    const char *in = AIGetInputStateJSON(adapter);
    TEST("input: keysDown A,B", strstr(in, "\"keysDown\": [65, 66]") != NULL);
    TEST("input: keysPressed space", strstr(in, "\"keysPressed\": [32]") != NULL);
    TEST("input: charsPressed x", strstr(in, "\"charsPressed\": [120]") != NULL);
    TEST("input: mouse pos", strstr(in, "\"x\": 123") != NULL && strstr(in, "\"y\": 45") != NULL);
    TEST("input: mouse button", strstr(in, "\"buttonsDown\": [0]") != NULL);
    TEST("input: wheel", strstr(in, "\"wheel\": {\"x\": 1, \"y\": -2}") != NULL);
    TEST("input: touch count", strstr(in, "\"count\": 1") != NULL);
    TEST("input: gamepad name", strstr(in, "\"name\": \"MockPad\"") != NULL);
    TEST("input: gamepad button", strstr(in, "\"buttonsDown\": [7]") != NULL);

    // Time JSON
    const char *t = AIGetTimeStateJSON(adapter);
    TEST("time: frameCounter", strstr(t, "\"frameCounter\": 42") != NULL);
    TEST("time: fps from frame", strstr(t, "\"fps\": 60.0") != NULL);
    TEST("time: current", strstr(t, "\"current\": 1.500000") != NULL);

    // Audio JSON (candidate 3: snapshot-driven, no CORE reads)
    mockSnapshot.audioDeviceReady = true;
    mockSnapshot.audioMasterVolume = 0.5f;
    mockSnapshot.audioBufferCount = 1;
    mockSnapshot.audioBuffers[0].usage = 1;  // STREAM
    mockSnapshot.audioBuffers[0].playing = true;
    mockSnapshot.audioBuffers[0].looping = true;
    mockSnapshot.audioBuffers[0].volume = 0.35f;
    mockSnapshot.audioBuffers[0].pitch = 1.0f;
    mockSnapshot.audioBuffers[0].pan = 0.0f;
    mockSnapshot.audioBuffers[0].progress = 0.5f;
    const char *a = AIGetAudioStateJSON(adapter);
    TEST("audio: device ready", strstr(a, "\"ready\": true") != NULL);
    TEST("audio: master volume", strstr(a, "\"masterVolume\": 0.500") != NULL);
    TEST("audio: buffer usage STREAM", strstr(a, "\"usage\": \"STREAM\"") != NULL);
    TEST("audio: buffer playing+looping", strstr(a, "\"playing\": true") != NULL && strstr(a, "\"looping\": true") != NULL);
    TEST("audio: buffer volume", strstr(a, "\"volume\": 0.350") != NULL);
    TEST("audio: buffer progress", strstr(a, "\"progress\": 0.500") != NULL);
    TEST("audio: buffer count", strstr(a, "\"bufferCount\": 1") != NULL);
    // Device not ready -> empty buffers
    mockSnapshot.audioDeviceReady = false;
    const char *a2 = AIGetAudioStateJSON(adapter);
    TEST("audio: not ready -> empty", strstr(a2, "\"ready\": false") != NULL && strstr(a2, "\"activeBuffers\": []") != NULL && strstr(a2, "\"bufferCount\": 0") != NULL);

    AIClose(adapter);
}

// Spec M5: drive AIProcessCommands headless through the command queue and capture
// its output via deps.emitProtocol — every command and the AI_ERR_* mappings
// become unit-testable without a window or a stdin thread.
static void FeedCommand(AIAdapter *adapter, const char *line)
{
    strncpy(adapter->commandQueue[0].text, line, AI_COMMAND_MAX_LEN - 1);
    adapter->commandQueue[0].text[AI_COMMAND_MAX_LEN - 1] = '\0';
    adapter->commandQueueCount = 1;
    adapter->commandQueueHead = 0;
    adapter->commandQueueTail = 1;
    adapter->commandLoopStarted = true;
    gEmitLen = 0;
    gEmitBuf[0] = '\0';
    adapter->lastErrorCode = AI_ERR_NONE;
}

static void TestProtocolCommands(void)
{
    TEST_SECTION("Protocol commands (headless via emitProtocol sink)");

    AIAdapter *adapter = AIInitEx(&mockDeps);
    ResetMocks();
    TEST("Adapter initialized for protocol tests", adapter != NULL);
    if (adapter == NULL) return;

    // AIProcessCommands guards the queue with commandMutex, which is normally
    // initialized by AIStartCommandLoop (stdin thread). Headless here: init the
    // lock directly; AIClose leaves it untouched (OS reclaims at process exit).
    InitializeCriticalSection(&adapter->commandMutex);

    // version
    FeedCommand(adapter, "version");
    AIProcessCommands(adapter);
    TEST("version: OK + protocol marker", strstr(gEmitBuf, "<<AI_OK>> protocol") != NULL);

    // scene (empty scene in headless)
    FeedCommand(adapter, "scene");
    AIProcessCommands(adapter);
    TEST("scene: begin/end markers", strstr(gEmitBuf, "<<AI_SCENE_BEGIN>>") != NULL && strstr(gEmitBuf, "<<AI_SCENE_END>>") != NULL);

    // input: valid injection routes through the simulate mocks
    FeedCommand(adapter, "input press a");
    AIProcessCommands(adapter);
    TEST("input: OK echo", strstr(gEmitBuf, "<<AI_OK>> input: press a") != NULL);
    TEST("input: simulate called", lastKeyDown == KEY_A);

    // inputmode: valid + unknown mode error
    FeedCommand(adapter, "inputmode postmessage");
    AIProcessCommands(adapter);
    TEST("inputmode: postmessage OK", strstr(gEmitBuf, "postmessage") != NULL);
    FeedCommand(adapter, "inputmode bogus");
    AIProcessCommands(adapter);
    TEST("inputmode: unknown mode -> AI_ERR_BAD_ARGUMENT", strstr(gEmitBuf, "<<AI_ERROR>> [code:") != NULL && strstr(gEmitBuf, "unknown mode") != NULL);

    // pause/resume/step
    FeedCommand(adapter, "pause");
    AIProcessCommands(adapter);
    TEST("pause: OK", strstr(gEmitBuf, "<<AI_OK>> paused") != NULL);
    TEST("pause: game actually paused", AIGamePaused(adapter) == true);
    FeedCommand(adapter, "resume");
    AIProcessCommands(adapter);
    TEST("resume: OK", strstr(gEmitBuf, "<<AI_OK>> resumed") != NULL);
    FeedCommand(adapter, "step 5");
    AIProcessCommands(adapter);
    TEST("step: frames echoed", strstr(gEmitBuf, "stepping 5 frame(s)") != NULL);

    // state window (mock snapshot)
    FeedCommand(adapter, "state window");
    AIProcessCommands(adapter);
    TEST("state window: markers", strstr(gEmitBuf, "<<AI_STATE_BEGIN>>") != NULL && strstr(gEmitBuf, "<<AI_STATE_END>>") != NULL);

    // state audio (candidate 3: snapshot-driven, headless-testable)
    mockSnapshot.audioDeviceReady = true;
    mockSnapshot.audioMasterVolume = 0.5f;
    mockSnapshot.audioBufferCount = 1;
    mockSnapshot.audioBuffers[0].usage = 1;
    mockSnapshot.audioBuffers[0].playing = true;
    FeedCommand(adapter, "state audio");
    AIProcessCommands(adapter);
    TEST("state audio: markers + device", strstr(gEmitBuf, "<<AI_STATE_BEGIN>>") != NULL &&
         strstr(gEmitBuf, "\"ready\": true") != NULL && strstr(gEmitBuf, "\"masterVolume\": 0.500") != NULL);
    TEST("state audio: buffer serialized", strstr(gEmitBuf, "\"usage\": \"STREAM\"") != NULL && strstr(gEmitBuf, "\"playing\": true") != NULL);
    mockSnapshot.audioDeviceReady = false;
    mockSnapshot.audioBufferCount = 0;

    // errors log
    FeedCommand(adapter, "errors");
    AIProcessCommands(adapter);
    TEST("errors: markers", strstr(gEmitBuf, "<<AI_ERRORS_BEGIN>>") != NULL && strstr(gEmitBuf, "<<AI_ERRORS_END>>") != NULL);

    // quiet/loud
    FeedCommand(adapter, "quiet");
    AIProcessCommands(adapter);
    TEST("quiet: OK", strstr(gEmitBuf, "quiet mode") != NULL);
    FeedCommand(adapter, "loud");
    AIProcessCommands(adapter);
    TEST("loud: OK", strstr(gEmitBuf, "loud mode") != NULL);

    // elements
    FeedCommand(adapter, "elements");
    AIProcessCommands(adapter);
    TEST("elements: count 0", strstr(gEmitBuf, "elements: 0") != NULL);

    // record / playback without data -> AI_ERR_NO_DATA
    FeedCommand(adapter, "playback");
    AIProcessCommands(adapter);
    TEST("playback: no data -> AI_ERR_NO_DATA", strstr(gEmitBuf, "<<AI_ERROR>> [code:") != NULL && strstr(gEmitBuf, "no recorded entries") != NULL);
    FeedCommand(adapter, "record");
    AIProcessCommands(adapter);
    TEST("record: started", strstr(gEmitBuf, "recording started") != NULL);
    FeedCommand(adapter, "record stop");
    AIProcessCommands(adapter);
    TEST("record stop: stopped", strstr(gEmitBuf, "recording stopped") != NULL);

    // snapshot / diff
    FeedCommand(adapter, "snapshot");
    AIProcessCommands(adapter);
    TEST("snapshot: saved", strstr(gEmitBuf, "snapshot saved") != NULL);
    FeedCommand(adapter, "diff");
    AIProcessCommands(adapter);
    TEST("diff: markers", strstr(gEmitBuf, "<<AI_DIFF_BEGIN>>") != NULL && strstr(gEmitBuf, "<<AI_DIFF_END>>") != NULL);
    FeedCommand(adapter, "snapshot clear");
    AIProcessCommands(adapter);
    TEST("snapshot clear: cleared", strstr(gEmitBuf, "snapshots cleared") != NULL);

    // help
    FeedCommand(adapter, "help");
    AIProcessCommands(adapter);
    TEST("help: markers", strstr(gEmitBuf, "<<AI_HELP>>") != NULL && strstr(gEmitBuf, "<<AI_HELP_END>>") != NULL);
    TEST("help: lists commands", strstr(gEmitBuf, "Commands:") != NULL);

    // quit: routes through deps.requestClose (no direct CORE writes)
    FeedCommand(adapter, "quit");
    AIProcessCommands(adapter);
    TEST("quit: OK", strstr(gEmitBuf, "<<AI_OK>> quitting") != NULL);
    TEST("quit: requestClose called", gRequestCloseCalls == 1);

    // unknown command -> AI_ERR_UNKNOWN_COMMAND
    FeedCommand(adapter, "frobnicate");
    AIProcessCommands(adapter);
    TEST("unknown: AI_ERR_UNKNOWN_COMMAND", strstr(gEmitBuf, "<<AI_ERROR>> [code:") != NULL && strstr(gEmitBuf, "unknown command: frobnicate") != NULL);

    // empty line is skipped silently
    FeedCommand(adapter, "");
    AIProcessCommands(adapter);
    TEST("empty line: no output", gEmitLen == 0);

    // multiple queued commands processed in order
    gEmitLen = 0; gEmitBuf[0] = '\0';
    strncpy(adapter->commandQueue[0].text, "pause", AI_COMMAND_MAX_LEN - 1);
    strncpy(adapter->commandQueue[1].text, "elements", AI_COMMAND_MAX_LEN - 1);
    strncpy(adapter->commandQueue[2].text, "help", AI_COMMAND_MAX_LEN - 1);
    adapter->commandQueueCount = 3;
    adapter->commandQueueHead = 0;
    adapter->commandQueueTail = 3;
    AIProcessCommands(adapter);
    TEST("multi-command: processed in order", strstr(gEmitBuf, "<<AI_OK>> paused") != NULL &&
         strstr(gEmitBuf, "elements: 0") != NULL && strstr(gEmitBuf, "<<AI_HELP>>") != NULL);

    // B4/T5: the end marker must survive a scene payload that fills the JSON buffer
    AIClearElements(adapter);
    AIDrawElement big = { 0 };
    big.type = AI_DRAW_TEXT;
    big.fontSize = 20;
    big.color = (Color){ 255, 255, 255, 255 };
    for (int i = 0; i < 127; i++) big.text[i] = 'A';
    big.text[127] = '\0';
    for (int i = 0; i < MAX_AI_ELEMENTS; i++) AIAddElement(adapter, &big);
    FeedCommand(adapter, "scene");
    AIProcessCommands(adapter);
    TEST("scene large: payload fills JSON buffer", gEmitLen > 60000);
    TEST("scene large: end marker intact", strstr(gEmitBuf, "<<AI_SCENE_END>>") != NULL);

    // B5: rotation is part of the observable scene for rects and textures
    AIClearElements(adapter);
    AIDrawElement rect = { 0 };
    rect.type = AI_DRAW_RECT;
    rect.x = 10; rect.y = 10; rect.w = 20; rect.h = 20; rect.rotation = 45.0f;
    rect.color = (Color){ 255, 0, 0, 255 };
    AIAddElement(adapter, &rect);
    FeedCommand(adapter, "scene");
    AIProcessCommands(adapter);
    TEST("scene JSON: rect rotation serialized", strstr(gEmitBuf, "\"rotation\": 45.0") != NULL);

    AIClose(adapter);
}

// C1/T3: diff must surface text and rotation changes, not just geometry
static void TestSnapshotDiffFields(void)
{
    TEST_SECTION("Snapshot diff: text/rotation fields (C1/T3)");

    AIAdapter *adapter = AIInitEx(&mockDeps);
    ResetMocks();
    AIClearElements(adapter);

    AIDrawElement text = { 0 };
    text.type = AI_DRAW_TEXT;
    text.x = 10; text.y = 10; text.fontSize = 20;
    strcpy(text.text, "100");
    text.color = (Color){ 255, 255, 255, 255 };

    AIDrawElement rect = { 0 };
    rect.type = AI_DRAW_RECT;
    rect.w = 10; rect.h = 10;
    rect.color = (Color){ 255, 0, 0, 255 };

    AIAddElement(adapter, &text);
    AIAddElement(adapter, &rect);
    AISaveSnapshot(adapter);

    strcpy(adapter->elements[0].text, "101");
    adapter->elements[1].rotation = 45.0f;

    const char *diff = AIGetSnapshotDiff(adapter);
    TEST("diff: text + rotation counted as 2 changes", strstr(diff, "\"changed\": 2") != NULL);
    TEST("diff: old/new text visible", strstr(diff, "\"old_text\": \"100\"") != NULL &&
         strstr(diff, "\"new_text\": \"101\"") != NULL);
    TEST("diff: rotation visible", strstr(diff, "\"new_rotation\": 45.0") != NULL);

    AIClose(adapter);
}

// C4: registry is truly circular once full; old entries keep working
static void TestTextureRegistry(void)
{
    TEST_SECTION("Texture registry circular overwrite (C4)");

    AIAdapter *adapter = AIInitEx(&mockDeps);
    char path[64];
    for (int i = 1; i <= MAX_AI_TEXTURE_REGISTRY + 1; i++)
    {
        snprintf(path, sizeof(path), "tex%d.png", i);
        AIRegisterTexture(adapter, (unsigned int)i, path);
    }

    TEST("registry: newest entry kept", strcmp(AILookupTexturePath(adapter, MAX_AI_TEXTURE_REGISTRY + 1), "tex257.png") == 0);
    TEST("registry: oldest overwritten", strcmp(AILookupTexturePath(adapter, 1), "unknown") == 0);
    TEST("registry: second entry kept", strcmp(AILookupTexturePath(adapter, 2), "tex2.png") == 0);

    AIClose(adapter);
}

// C5/C6: postmessage click defers the release; non-ASCII typing is rejected
static void TestPostMessageInput(void)
{
    TEST_SECTION("PostMessage input (C5/C6)");

    AIAdapter *adapter = AIInitEx(&mockDeps);
    ResetMocks();
    adapter->inputMode = AI_SIMULATE_POSTMESSAGE;

    AIInjectInput(adapter, "click 10 20");
    TEST("postmessage click: down sent immediately", lastMouseButton == 0);
    TEST("postmessage click: up deferred one frame", adapter->pendingMouseUp == 0 && mouseUpCalls == 0);

    AIInjectInput(adapter, "type \xe4\xb8\xad\xe6\x96\x87");
    TEST("type: non-ASCII rejected", adapter->lastErrorCode == AI_ERR_BAD_ARGUMENT);
    TEST("type: no key injected", lastKeyDown == -1);

    AIClose(adapter);
}

// C3/T6: arbitrary window titles must not break the JSON document
static void TestStateJSONEscaping(void)
{
    TEST_SECTION("State JSON escaping (C3/T6)");

    AIAdapter *adapter = AIInitEx(&mockDeps);
    ResetMocks();
    mockSnapshot.title = "He said \"hi\" \\ path\nline2";

    const char *json = AIGetWindowStateJSON(adapter);
    TEST("title: quotes escaped", strstr(json, "\\\"hi\\\"") != NULL);
    TEST("title: backslash escaped", strstr(json, "\\\\ path") != NULL);
    TEST("title: newline escaped", strstr(json, "\\nline2") != NULL);

    AIClose(adapter);
}

// T4: a paused procedural sound must survive AIAudioUpdate's auto-recycle and
// keep its slot until resumed or stopped. Requires a real audio device: skips
// (passes trivially) on headless machines where InitAudioDevice fails.
static void TestPausedAudioRetention(void)
{
    TEST_SECTION("Paused audio slot retention (T4)");

    if (!IsAudioDeviceReady()) InitAudioDevice();
    if (!IsAudioDeviceReady())
    {
        TEST("skipped: no audio device available", true);
        return;
    }

    AIAdapter *adapter = AIInit();
    int handle = AIPlayTone(adapter, 440.0f, 1.0f, 0.5f, AI_WAVEFORM_SINE, 0);
    TEST("tone started with valid handle", handle > 0);
    TEST("slot is active", adapter->audioSlots[handle - 1].active);

    AIPauseAudio(adapter, handle);
    for (int i = 0; i < 30; i++) AIAudioUpdate(adapter);   // 30 frames paused

    TEST("paused slot survives 30 updates", adapter->audioSlots[handle - 1].active);
    TEST("paused flag retained", adapter->audioSlots[handle - 1].paused);
    TEST("paused sound reports not playing", IsAIAudioPlaying(adapter, handle) == false);

    AIResumeAudio(adapter, handle);
    TEST("resume keeps slot active", adapter->audioSlots[handle - 1].active && !adapter->audioSlots[handle - 1].paused);

    AIStopAudio(adapter, handle);
    AIClose(adapter);
    CloseAudioDevice();
}

// B1/T1: audio state is only allocated by InitAudioDevice(); every public entry
// point must be safe to call without it (AI `state` on audio-less games crashed)
static void TestAudioDeviceGuards(void)
{
    TEST_SECTION("Audio device guards (B1)");

    TEST("IsAudioDeviceReady without InitAudioDevice returns false", IsAudioDeviceReady() == false);
    TEST("GetMasterVolume without device returns 0", GetMasterVolume() == 0.0f);
    SetMasterVolume(0.5f);  // must be a no-op, not a NULL dereference
    TEST("SetMasterVolume without device does not crash", true);
}

static void TestObserverOwnership(void)
{
    TEST_SECTION("Observer ownership (single-adapter premise)");

    AIAdapter *a = AIInitEx(&mockDeps);
    AIAdapter *b = AIInitEx(&mockDeps);
    ResetMocks();

    // First adapter takes the observer
    AISetAutoSceneCapture(a, true);
    TEST("First adapter owns observer", GetAIDrawObserver() != NULL);
    TEST("First adapter enable: no error", AIGetLastError(a) == AI_ERR_NONE);

    // Second adapter must be rejected with AI_ERR_BUSY, not silently steal it
    AISetAutoSceneCapture(b, true);
    TEST("Second adapter rejected with AI_ERR_BUSY", AIGetLastError(b) == AI_ERR_BUSY);
    TEST("Observer still owned by first adapter", GetAIDrawObserver() != NULL);

    // Closing the owner releases its own registration
    AIClose(a);
    TEST("Closing owner releases observer", GetAIDrawObserver() == NULL);
    AISetAutoSceneCapture(b, true);
    TEST("Second adapter takes observer after close", GetAIDrawObserver() != NULL);
    AIClose(b);

    // AIClose must NOT clear an observer re-registered by game code (owner == NULL)
    AIAdapter *c = AIInitEx(&mockDeps);
    AISetAutoSceneCapture(c, true);
    AIDrawObserver custom = GetAIDrawObserver();
    SetAIDrawObserver(custom);               // re-register with owner == NULL (game code)
    AIClose(c);
    TEST("AIClose keeps game-owned observer", GetAIDrawObserver() == custom);
    SetAIDrawObserver(NULL);                 // cleanup
}

static void TestProceduralAudio(void)
{
    TEST_SECTION("Procedural audio (GenerateWaveData + slot allocation)");

    AIAdapter *adapter = AIInitEx(&mockDeps);
    ResetMocks();

    // ① Sample count correctness
    int count = 0;
    short *pcm = GenerateWaveData(44100, 440.0f, 0.5f, 0.5f, AI_WAVEFORM_SINE, 0, &count);
    TEST("Sample count = rate * duration", count == 22050);
    TEST("PCM buffer allocated", pcm != NULL);

    int badCount = -1;
    TEST("Invalid params yield no samples", (GenerateWaveData(0, 1.0f, 1.0f, 1.0f, AI_WAVEFORM_SINE, 0, &badCount) == NULL) && (badCount == 0));

    // ② Fade in/out + audibility
    int peak = 0;
    for (int i = 0; i < count; i++)
    {
        int a = abs(pcm[i]);
        if (a > peak) peak = a;
    }
    TEST("Fade-in starts near zero", abs(pcm[0]) <= 1);
    TEST("Fade-out ends near zero", abs(pcm[count - 1]) <= 50);
    TEST("Tone is audible (peak amplitude > 1000)", peak > 1000);
    TEST("Peak within 16-bit amplitude", peak <= 32767);

    // ③ Volume clamping: volume 0 => silence
    int c0 = 0;
    short *pcm0 = GenerateWaveData(44100, 440.0f, 0.1f, 0.0f, AI_WAVEFORM_SINE, 0, &c0);
    int nonzero0 = 0;
    for (int i = 0; i < c0; i++) if (pcm0[i] != 0) nonzero0++;
    TEST("Volume 0 yields all-zero PCM", nonzero0 == 0);
    RL_FREE(pcm0);

    // ④ Waveform distinction
    long long sum = 0;
    for (int i = 0; i < count; i++) sum += pcm[i];
    double mean = (double)sum / (double)count;
    TEST("Sine mean near zero", mean > -200.0 && mean < 200.0);
    RL_FREE(pcm);

    int sqCount = 0;
    short *sq = GenerateWaveData(44100, 440.0f, 0.1f, 1.0f, AI_WAVEFORM_SQUARE, 0, &sqCount);
    int extremes = 0;
    for (int i = 0; i < sqCount; i++) if (abs(sq[i]) > 30000) extremes++;
    TEST("Square is extreme-dominated", extremes > sqCount / 2);
    RL_FREE(sq);

    int nCount = 0;
    short *nz = GenerateWaveData(44100, 0.0f, 0.1f, 1.0f, AI_WAVEFORM_NOISE, 0, &nCount);
    long long nsum = 0, nsum2 = 0;
    for (int i = 0; i < nCount; i++) { nsum += nz[i]; nsum2 += (long long)nz[i] * nz[i]; }
    double nmean = (double)nsum / (double)nCount;
    double variance = (double)nsum2 / (double)nCount - nmean * nmean;
    TEST("Noise has nonzero variance", variance > 1000.0);
    RL_FREE(nz);

    // ⑤ Candidate 6: NOISE reproducible per seed (self-contained LCG)
    int na = 0, nb = 0, nc = 0;
    short *noiseA = GenerateWaveData(44100, 0.0f, 0.05f, 1.0f, AI_WAVEFORM_NOISE, 42, &na);
    short *noiseB = GenerateWaveData(44100, 0.0f, 0.05f, 1.0f, AI_WAVEFORM_NOISE, 42, &nb);
    short *noiseC = GenerateWaveData(44100, 0.0f, 0.05f, 1.0f, AI_WAVEFORM_NOISE, 43, &nc);
    TEST("Noise reproducible per seed", na == nb && memcmp(noiseA, noiseB, na*sizeof(short)) == 0);
    TEST("Different seed different noise", na == nc && memcmp(noiseA, noiseC, na*sizeof(short)) != 0);
    RL_FREE(noiseA); RL_FREE(noiseB); RL_FREE(noiseC);

    // ⑥ Candidate 6: sampleCount hard cap — huge duration cannot allocate unbounded PCM
    int bigCount = 0;
    short *big = GenerateWaveData(44100, 440.0f, 600.0f, 0.5f, AI_WAVEFORM_SINE, 0, &bigCount);
    TEST("Long duration clamped to AI_MAX_AUDIO_SAMPLES", bigCount == AI_MAX_AUDIO_SAMPLES);
    RL_FREE(big);

    // Slot allocation state machine (pure, no raylib)
    TEST("Alloc first free slot", AIAllocSlot(adapter) == 0);
    adapter->audioSlots[0].active = true;
    TEST("Alloc skips active slot", AIAllocSlot(adapter) == 1);
    adapter->audioSlots[1].active = true;
    adapter->audioSlots[0].active = false;   // released
    TEST("Alloc reuses freed slot", AIAllocSlot(adapter) == 0);
    TEST("Alloc NULL adapter is safe", AIAllocSlot(NULL) == -1);

    adapter->audioSlots[0].active = false;
    adapter->audioSlots[1].active = false;
    AIClose(adapter);
}

//----------------------------------------------------------------------------------
// Main
//----------------------------------------------------------------------------------

int main(void)
{
    printf("========================================\n");
    printf("  AI Adapter Unit Tests\n");
    printf("========================================\n");

    TestSceneJSON();
    TestInputInjectionParsing();
    TestLogInterception();
    TestMultiInstance();
    TestProtocolVersion();
    TestElementOverflow();
    TestInputErrorFeedback();
    TestSceneJSONPerformance();
    TestSceneJSONTruncation();
    TestSnapshotDiff();
    TestPostMessageKeyPressDelay();                         
    TestElementTypeTable();                                 
    TestRecordPlayback();                                   
    TestObserverOwnership();
    TestStateSnapshotJSON();
    TestProtocolCommands();
    TestSnapshotDiffFields();
    TestTextureRegistry();
    TestPostMessageInput();
    TestStateJSONEscaping();
    TestAudioDeviceGuards();
    TestPausedAudioRetention();
    TestProceduralAudio();

    printf("\n========================================\n");
    printf("  Results: %d passed, %d failed out of %d\n",
           testsPassed, testsFailed, testsPassed + testsFailed);
    printf("========================================\n");

    return testsFailed > 0 ? 1 : 0;
}