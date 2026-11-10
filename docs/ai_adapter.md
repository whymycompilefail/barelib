# AI Adapter — Let AI Agents See Your Game

> **Give LLMs structured scene descriptions so they can understand, test, and play your raylib games — without a screen.**

---

## What is this?

The AI Adapter (`raylib_ai_adapter.h` / `raylib_ai_adapter.c`) is a layer that turns your game's visual state into **structured JSON text**. An AI model can read this JSON to know exactly what's on screen — shapes, text, textures, colors — frame by frame.

It also supports **input injection**, **command loop** (stdin-driven control), **record/playback**, and **scene snapshots/diffing**, closing the loop from observation to action.

### Design principles

- **What the human sees, the AI sees**: AIDrawXXX wrappers call the real raylib draw functions and register the same elements the human user sees on screen.
- **Objective data only**: The adapter returns raw JSON (element list, positions, colors). No interpretation, no hallucination.
- **Two observability paths**: (1) explicit — AIDrawXXX wrappers register elements; (2) non-invasive (spec M3) — the engine's draw observer (`raylib_observability.h`) hooks the *convergence points* of the DrawXXX families (DrawRectanglePro / DrawCircleV / DrawTextEx) so plain native draw calls become scene elements with no game-code changes. Enable via `AISetAutoSceneCapture()`, mutually exclusive with the wrappers.
- **Text-only protocol**: All communication is via stdin/stdout text. No multimodal dependencies.

---

## Quick Start

```c
#include "raylib.h"
#include "raylib_ai_adapter.h"

int main(void)
{
    InitWindow(800, 600, "AI Game");
    AIAdapter *ai = AIInit();
    AIStartCommandLoop(ai);  // listen for stdin commands

    while (!WindowShouldClose())
    {
        BeginDrawing();
        AIClearBackground(RAYWHITE);              // ClearBackground + reset scene buffer
        AIDrawRectangle(10, 10, 100, 40, BLUE);   // DrawRectangle + register element
        AIDrawText("Score: 100", 10, 60, 20, BLACK);
        EndDrawing();
        AIUpdate(ai);  // process commands, advance playback, auto-output JSON
    }

    AIClose(ai);
    CloseWindow();
    return 0;
}
```

### How AIDrawXXX wrappers work

Each `AIDrawXXX` function:
1. Calls the corresponding raylib `DrawXXX` function (rendering is unaffected)
2. Registers the element with the first active AIAdapter
3. If no adapter exists, still draws normally (graceful degradation)

```c
void AIDrawRectangle(int posX, int posY, int width, int height, Color color)
{
    DrawRectangle(posX, posY, width, height, color);  // real draw
    AIAdapter *adapter = AIGetActiveAdapter();
    if (adapter) {
        AIDrawElement elem = { .type=AI_DRAW_RECT, .x=posX, .y=posY,
                               .w=width, .h=height, .color=color };
        AIAddElement(adapter, &elem);
    }
}
```

### Key difference from raw DrawXXX

| Raw raylib | AI Adapter | Effect |
|------------|------------|--------|
| `ClearBackground(c)` | `AIClearBackground(c)` | Clears screen + resets scene element buffer |
| `DrawRectangle(...)` | `AIDrawRectangle(...)` | Draws + registers element |
| `DrawText(...)` | `AIDrawText(...)` | Draws + registers element |
| `DrawCircle(...)` | `AIDrawCircle(...)` | Draws + registers element |
| `LoadTexture(...)` | `AILoadTexture(...)` | Loads + records file path for JSON |

> If you use raw `DrawXXX`, the element won't appear in the scene JSON. AI-authored games should use `AIDrawXXX` wrappers exclusively.

---

## Command Loop (stdin-driven AI control)

`AIStartCommandLoop(adapter)` starts a background thread that reads commands from stdin. `AIUpdate()` processes queued commands each frame. This lets an AI agent control the game via stdin/stdout pipes.

### Commands

| Command | Description |
|---------|-------------|
| `version` | Print protocol version (see `AI_PROTOCOL_VERSION`) |
| `scene` | Print current scene JSON to stdout |
| `input <action>` | Inject input (press/click/move/scroll/type) |
| `pause` | Pause game loop |
| `resume` | Resume game loop |
| `step [n]` | Step n frames (default: 1) |
| `state [cat]` | Print state JSON (window/input/time/audio) |
| `errors` | Print error log JSON |

> **Protocol versioning (spec M2):** every scene JSON carries a top-level
> `"protocol"` field (e.g. `"1.0"`) so AI agents can adapt to engine evolution.
> The `version` command reports the same value. Within a major version the
> command set and JSON fields remain backward compatible.
| `quiet` | Stop auto-stdout scene output |
| `loud` | Start auto-stdout scene output |
| `elements` | Print element count |
| `record` | Start recording input actions |
| `record stop` | Stop recording |
| `playback` | Replay recorded input |
| `playback stop` | Stop playback |
| `snapshot [save]` | Save current scene snapshot |
| `snapshot clear` | Clear all snapshots |
| `diff` | Diff current scene vs latest snapshot |
| `help` | Print available commands |
| `quit` | Close the game |

### Output markers

```
<<AI_SCENE_BEGIN>>  ... <<AI_SCENE_END>>      — scene JSON
<<AI_DIFF_BEGIN>>   ... <<AI_DIFF_END>>       — diff JSON
<<AI_STATE_BEGIN>>  ... <<AI_STATE_END>>      — state JSON
<<AI_ERRORS_BEGIN>> ... <<AI_ERRORS_END>>     — error log JSON
<<AI_OK>> ...                                  — command acknowledgment
<<AI_ERROR>> [code:N] message                  — command error (N = AIErrorCode)
<<AI_HELP>> ... <<AI_HELP_END>>               — help text
<<AI_SHUTDOWN>>                                — adapter closing
```

**Error model (spec M2):** command failures are structured as
`<<AI_ERROR>> [code:N] message` where N is an `AIErrorCode`:

| Code | Name | Meaning |
|------|------|---------|
| 0 | `AI_ERR_NONE` | No error |
| 1 | `AI_ERR_UNKNOWN_COMMAND` | Unknown command (see `help`) |
| 2 | `AI_ERR_BAD_ARGUMENT` | Missing or invalid argument |
| 3 | `AI_ERR_BUFFER_FULL` | Internal buffer overflow (scene elements) |
| 4 | `AI_ERR_NO_DATA` | No available data (e.g. nothing recorded) |
| 5 | `AI_ERR_NOT_SUPPORTED` | Operation not supported on this platform |
| 6 | `AI_ERR_INTERNAL` | Internal error |

Invalid `input` actions also set the adapter's last error, readable via
`AIGetLastError()` (returns `AI_ERR_NONE` on the next valid call).

### Example session

```
> scene
<<AI_SCENE_BEGIN>>
{"protocol":"1.0","screen":{"width":800,"height":600},"elements":[{"type":"rect","x":10,"y":10,"w":100,"h":40,"color":[0,121,241,255]},...]}
<<AI_SCENE_END>>
> input press notarealkey
<<AI_ERROR>> [code:2] input: unknown key 'notarealkey'
> input press space
<<AI_OK>> input: press space
> snapshot
<<AI_OK>> snapshot saved (elements: 3, total snapshots: 1)
> step 5
<<AI_OK>> stepped 5 frames
> diff
<<AI_DIFF_BEGIN>>
{"snapshot_elements":3,"current_elements":4,"added":1,"removed":0,"changed":1,"changes":[...]}
<<AI_DIFF_END>>
> record
<<AI_OK>> recording started
> input press right
<<AI_OK>> input: press right
> input press right up
<<AI_OK>> input: press right up
> record stop
<<AI_OK>> recording stopped (2 entries)
> playback
<<AI_OK>> playback started (2 entries)
> quit
<<AI_SHUTDOWN>>
```

---

## Record / Playback System

Record input action sequences and replay them — useful for reproducing bugs or testing timing-sensitive gameplay.

- **Recording** captures every input command injected via `AIInjectInput()` along with the frame number.
- **Playback** replays them at the correct frames. During playback, re-injected actions are not re-recorded.
- Recording stops automatically when the buffer is full (1024 entries).

```c
AIStartRecording(adapter);           // begin recording
AIInjectInput(adapter, "press right");
AIInjectInput(adapter, "press right up");
AIStopRecording(adapter);            // stop (2 entries recorded)

AIStartPlayback(adapter);            // replay the 2 entries at their original frames
// ... playback runs automatically in AIUpdate() ...
AIStopPlayback(adapter);             // stop early if needed
```

### API

| Function | Description |
|----------|-------------|
| `AIStartRecording(adapter)` | Begin recording input actions |
| `AIStopRecording(adapter)` | Stop recording |
| `AIIsRecording(adapter)` | Check if currently recording |
| `AIGetRecordCount(adapter)` | Number of recorded entries |
| `AIStartPlayback(adapter)` | Begin replaying recorded input |
| `AIStopPlayback(adapter)` | Stop playback |
| `AIIsPlayingBack(adapter)` | Check if currently playing back |

---

## Scene Snapshots and Diffing

Save the current scene element list as a snapshot, then compare a later frame against it.

```c
AISaveSnapshot(adapter);             // save current elements as snapshot
// ... game advances ...
const char *diff = AIGetSnapshotDiff(adapter);  // compare current vs snapshot
```

The diff JSON reports:
- `snapshot_elements` / `current_elements` — element counts
- `added` / `removed` / `changed` — summary counts
- `changes` — detailed list (up to 32 entries) with per-element:
  - `action`: "changed" / "added" / "removed"
  - `old_type` / `new_type`, `old_pos` / `new_pos`, `old_size` / `new_size`
  - `old_radius` / `new_radius`, `old_color` / `new_color`

### API

| Function | Description |
|----------|-------------|
| `AISaveSnapshot(adapter)` | Save current scene elements as snapshot |
| `AIClearSnapshots(adapter)` | Clear all saved snapshots |
| `AIGetSnapshotCount(adapter)` | Number of snapshots saved (cumulative) |
| `AIGetSnapshotDiff(adapter)` | Diff current scene vs latest snapshot (JSON) |

---

## API Reference

### Lifecycle

| Function | When to call |
|----------|-------------|
| `AIInit()` | After `InitWindow()`. Returns NULL on failure. |
| `AIInitEx(deps)` | Same, but with custom AIDeps (for testing without a window) |
| `AIUpdate(adapter)` | Every frame, **after** `EndDrawing()` |
| `AIClose(adapter)` | Cleanup, **before** `CloseWindow()` |

### Output modes

```c
AISetOutputMode(adapter, AI_OUTPUT_MEMORY);   // default — get JSON via AIGetSceneJSON()
AISetOutputMode(adapter, AI_OUTPUT_STDOUT);   // auto-write JSON to stdout on AIUpdate()
```

### Scene description

| Function | Description |
|----------|-------------|
| `AIRegisterElement(adapter, element)` | Manually register an AIDrawElement |
| `AIClearElements(adapter)` | Clear all elements (also done by AIClearBackground) |
| `AIGetElementCount(adapter)` | Number of registered elements |
| `AIGetSceneJSON(adapter)` | Get current scene as JSON |
| `AISetAutoSceneCapture(adapter, enable)` | Non-invasive capture: native DrawXXX calls (rect/circle/text families) become scene elements via the engine draw hook — stock games need no wrappers. Mutually exclusive with AIDrawXXX (don't enable both) |

### AIDrawXXX wrapper functions

All wrappers call the real raylib draw function + register the element. If no adapter is active, they still draw normally.

**Background**: `AIClearBackground(color)`

**Rectangles** (rotation/lineThick/roundness are scalar params, mirroring the scene JSON fields):
  - `AIDrawRect(x, y, w, h, rotation, color)`
  - `AIDrawRectLines(x, y, w, h, lineThick, color)`
  - `AIDrawRectRounded(x, y, w, h, roundness, lineThick, color)` — engine-native rounded rect (fill or outline)

**Circles**:
  - `AIDrawCircle(x, y, radius, color)`
  - `AIDrawCircleLines(x, y, radius, color)`
  - `AIDrawCircleSector(x, y, radius, startAngle, endAngle, lineThick, color)` — engine-native sector (fill or outline)

**Triangles**: `AIDrawTriangle(x1, y1, x2, y2, x3, y3, color)`, `AIDrawTriangleLines(...)`

**Polygons**: `AIDrawPoly(x, y, sides, radius, rotation, color)`, `AIDrawPolyLines(...)`

**Lines**: `AIDrawLine(x1, y1, x2, y2, thick, color)`, `AIDrawLineBezier(x1, y1, x2, y2, thick, color)`

**Pixels**: `AIDrawPixel(x, y, color)`

**Text**: `AIDrawText(font, text, x, y, fontSize, spacing, color)`

**Textures**: `AILoadTexture(fileName)`, `AIDrawTexture(texture, x, y, color)`

> These 16 wrappers (M4 table-driven) map 1:1 to `AIDrawElementType`; the
> engine-side primitives they call (including `DrawRectangleRounded` and
> `DrawCircleSector`, added in the wrapper-deepening pass) are native.

### Input injection

```c
AIInjectInput(adapter, "press space");
AIInjectInput(adapter, "click 100 200");
AIInjectInput(adapter, "type hello");
```

| Action | Format | Example |
|--------|--------|---------|
| Key press | `press <key> [down\|up]` | `press space`, `press enter down` |
| Mouse click | `click <x> <y> [left\|right\|middle]` | `click 100 200`, `click 100 200 right` |
| Mouse move | `move <x> <y>` | `move 400 300` |
| Scroll | `scroll <dx> <dy>` | `scroll 0 -1` |
| Type text | `type <text>` | `type hello world` |

Key names (case-insensitive): a-z, 0-9, space, enter, escape, tab, backspace, delete, insert, home, end, pageup, pagedown, up, down, left, right, shift, ctrl, alt, f1-f12.

`type` only supports ASCII input: non-ASCII characters (e.g. Chinese) are rejected with `AI_ERR_BAD_ARGUMENT` instead of injecting garbage keycodes.

In `postmessage` mode, `press` and `click` delay the release by one frame (flushed by `AIUpdate`) so the game's `IsKeyPressed` / `IsMouseButtonPressed` can observe the event.

### Game loop control

| Function | Description |
|----------|-------------|
| `AIPauseGame(adapter)` | Pause game loop (draw still renders frozen state) |
| `AIResumeGame(adapter)` | Resume game loop |
| `AIStepFrame(adapter, n)` | Step n frames while paused |
| `AIGamePaused(adapter)` | Check if paused |
| `AIGetStepFrames(adapter)` | Remaining step frames |
| `AIConsumeStepFrame(adapter)` | Decrement step counter (runtime calls this) |

### Error and log feedback

| Function | Description |
|----------|-------------|
| `AIGetLogs(adapter, &count)` | Get cached log entries (pointer to internal array) |
| `AIClearLogs(adapter)` | Clear log buffer |
| `AIGetErrorsJSON(adapter)` | Get recent warnings/errors as JSON |

### Backend state query

| Function | Returns |
|----------|---------|
| `AIGetWindowStateJSON(adapter)` | Window size, position, display, flags |
| `AIGetInputStateJSON(adapter)` | Keyboard, mouse, touch, gamepad state |
| `AIGetTimeStateJSON(adapter)` | Current time, frame time, FPS, frame counter |
| `AIGetAudioStateJSON(adapter)` | Audio device, volume, active buffers |

### Procedural audio

| Function | Description |
|----------|-------------|
| `AIPlayTone(adapter, freq, duration, volume, waveform, seed)` | Play a tone (returns handle; seed drives NOISE, 0 for tonal) |
| `AIPlayNoise(adapter, duration, volume, seed)` | Play reproducible white noise (same seed = same noise) |
| `AIStopAudio(adapter, handle)` | Stop audio by handle |
| `AIPauseAudio(adapter, handle)` | Pause audio |
| `AIResumeAudio(adapter, handle)` | Resume audio |
| `AISetAudioVolume(adapter, handle, vol)` | Set volume (0.0-1.0) |
| `AISetAudioPitch(adapter, handle, pitch)` | Set pitch (1.0 = base) |
| `IsAIAudioPlaying(adapter, handle)` | Check if still playing |

Waveforms: `AI_WAVEFORM_SINE`, `AI_WAVEFORM_SQUARE`, `AI_WAVEFORM_TRIANGLE`, `AI_WAVEFORM_SAWTOOTH`, `AI_WAVEFORM_NOISE`

### Pixel sprites (`raylib_ai_sprite.h`)

Reads sheet JSON exported by [Pixel Loom](https://github.com/yangsir2077/pixel-loom) (native) or
Aseprite (JSON hash / JSON array). The engine owns playback state, so the scene JSON always reports
the frame that is actually on screen.

```c
AISprite *hero = AILoadSpriteSheet("assets/hero.json");  // atlas PNG resolved next to the JSON
AISpritePlay(hero, "walk");                              // tags from the sheet; NULL = whole timeline

// game loop
AISpriteUpdate(hero, GetFrameTime());
AISpriteDraw(hero, 120, 80, 4, false, WHITE);            // integer scale 4x, no flip
```

| Function | Description |
|----------|-------------|
| `AILoadSpriteSheet(jsonPath)` | Parse sheet JSON, load atlas with `TEXTURE_FILTER_POINT` |
| `AIParseSpriteSheet(json, size, hint)` | Parse from memory without a texture (headless tests) |
| `AIUnloadSpriteSheet(sprite)` | Unload texture and free |
| `AISpritePlay(sprite, tag)` | Play a tag from its first frame; unknown tag returns false |
| `AISpriteStop(sprite)` | Pause on the current frame |
| `AISpriteUpdate(sprite, dt)` | Advance by `duration_ms`, honouring the tag direction |
| `AISpriteIsPlaying(sprite)` | True while advancing |
| `AISpriteCurrentFrame(sprite)` | Original timeline frame index, or -1 |
| `AISpriteFrameCount(sprite)` | Frames in the sheet |
| `AISpriteSeek(sprite, position)` | Jump to a step of the expanded playback order |
| `AISpriteDraw(sprite, x, y, scale, flipX, tint)` | Draw the current frame and record an `AI_DRAW_SPRITE` element |

Playback order follows Pixel Loom: `forward` = start..end, `reverse` = end..start,
`pingpong` = start..end then end-1..start+1. Native tags loop by default; an Aseprite tag with a
`repeat` field plays once and stops on its last frame. `scale` is an integer multiplier
(nearest filtering + integer scale keep pixel art crisp).

### Command loop

| Function | Description |
|----------|-------------|
| `AIStartCommandLoop(adapter)` | Start stdin reader thread |

### Dependency injection (for testing)

```c
AIDeps deps = {
    .getScreenWidth = mockScreenWidth,
    .getScreenHeight = mockScreenHeight,
    .getTime = mockTime,
    .setTraceLogCallback = mockLogCallback,
    .getStateSnapshot = mockGetStateSnapshot,  // fill an AIStateSnapshot for state JSON tests
    .simulateKeyDown = mockKeyDown,
    // ...
};
AIAdapter *ai = AIInitEx(&deps);  // use mocks instead of real raylib
```

`getStateSnapshot` (added in the state-snapshot pass) fills a copy of the
window/input/time state; the JSON generators (`AIGetWindowStateJSON` etc.) are
pure formatters over it, so state output can be asserted headless. The default
implementation reads the live core state.

---

## JSON Output Format

### Scene JSON

```json
{
  "protocol": "1.0",
  "screen": {"width": 800, "height": 600},
  "elements": [
    {"type": "clear", "color": [245, 245, 245, 255]},
    {"type": "rect", "x": 10, "y": 10, "w": 100, "h": 40, "color": [0, 121, 241, 255]},
    {"type": "text", "x": 10, "y": 60, "text": "Score: 100", "size": 20, "color": [0, 0, 0, 255]}
  ]
}
```

- `protocol` — the AI communication protocol version (`AI_PROTOCOL_VERSION`).
- `screen` — current window size in pixels.
- `elements` — every element registered this frame via `AIDrawXXX`/`AIRegisterElement`.
- `overflow` — **only present when elements were dropped** because the scene element
  buffer (`MAX_AI_ELEMENTS`, default 1024) filled up; the value is the number of
  dropped elements. AI agents must treat an overflowing scene as incomplete.
  The counter resets each frame via `AIClearBackground()`/`AIClearElements()`.

Element types: `clear`, `rect`, `rect_lines`, `circle`, `circle_lines`, `triangle`, `triangle_lines`, `poly`, `poly_lines`, `text`, `texture`, `sprite`, `line`, `pixel`

Sprite elements add `sheet` (JSON path), `tag`, `frame`, `frame_count`, `loop`, `scale` and `flip_x`:

```json
{"type": "sprite", "x": 120.0, "y": 80.0, "w": 128.0, "h": 128.0, "scale": 4,
 "source": "assets/hero.png", "sheet": "assets/hero.json", "tag": "walk",
 "frame": 2, "frame_count": 6, "loop": true, "flip_x": false, "tint": [255, 255, 255, 255]}
```

### Diff JSON

```json
{
  "snapshot_elements": 3,
  "current_elements": 4,
  "added": 1,
  "removed": 0,
  "changed": 1,
  "changes": [
    {"index": 2, "action": "changed", "old_type": "text", "new_type": "text",
     "old_pos": [10, 60], "new_pos": [10, 60],
     "old_size": [0, 0], "new_size": [0, 0],
     "old_radius": 0.0, "new_radius": 0.0,
     "old_color": [0, 0, 0, 255], "new_color": [255, 0, 0, 255]},
    {"index": 3, "action": "added", "type": "circle", "pos": [200, 200],
     "size": [0, 0], "color": [255, 200, 0, 255]}
  ]
}
```

---

## Multi-Agent Collaboration (spec M3)

barelib's collaboration model is **tool-layer, not network**: any number of AI
agents can drive and share a game session through the filesystem and the
stdin/stdout tools — no network channel, which spec §4.2 explicitly excludes.

- **Authoring agent** writes a scenario script (`tests/scenarios/*.json`) that
  captures the desired protocol behavior as replayable steps.
- **Testing agent** runs it against the live game and produces a machine-readable
  report:
  ```bash
  python tests/ai_scenario_check.py tests/scenarios/basic.json --report out.json
  # exit 0 = all steps passed; out.json has per-step ok/detail/expected/observed
  ```
- **Fixing agent** consumes `out.json` (failure detail + observed output), edits
  the game source, and re-runs the same scenario until green — the
  test → report → fix loop. No session locking is needed: each run is an
  independent, reproducible replay of the same scenario.

## How It Works Under the Hood

- **AIDrawXXX wrappers** call the real raylib draw function, then register an `AIDrawElement` with the first active adapter (via `AIGetActiveAdapter()` which reads `RaylibContext`).
- **Command loop** runs a background stdin reader thread. Commands are queued in a thread-safe circular buffer. `AIUpdate()` processes pending commands each frame.
- **Recording** stores each `AIInjectInput()` call's action string + frame number. **Playback** replays them at the correct frames inside `AIUpdate()`.
- **Snapshots** copy the element buffer to a separate buffer. **Diff** compares element-by-element (index-based matching) and reports type/position/size/radius/color changes.
- **Log interception** hooks into raylib's `TraceLog` callback, caches everything in a ring buffer.
- **JSON output** uses a reusable 64KB buffer per adapter. Strings are escaped for JSON safety.
- **Dependency injection** abstracts raylib calls (screen size, time, logging, input) for mock testing.
- **Input backchannel** uses platform APIs (Windows `SendInput`) to simulate keyboard/mouse.
- **Auto-output mode** (`AI_OUTPUT_STDOUT`) makes `AIUpdate()` auto-write JSON to stdout.

---

## Compilation

### Building raylib with AI adapter

```bash
cmake -B build -S . -G "Visual Studio 17 2022" -DBUILD_EXAMPLES=OFF
cmake --build build --target raylib --config Debug
```

The AI adapter source files are automatically included in the raylib library target.

### Compiling AI-authored games

Use the included compile script:

```bat
compile_game.bat my_game.c           :: compile and run
compile_game.bat my_game.c build     :: compile only
```

Or compile manually with MSVC:

```bat
cl.exe /utf-8 /Fe:my_game.exe my_game.c /I src /link /LIBPATH:bin\Debug raylib.lib opengl32.lib gdi32.lib winmm.lib user32.lib shell32.lib
```

See `examples/ai_music_demo.c` for a complete starter demonstrating AIDrawXXX wrappers, command loop, and game loop structure.

---

## Risk Summary

| Risk | Level | Mitigation |
|------|-------|------------|
| Performance | Low | Static buffers (1024 elements, 256 logs, 64KB JSON); diff is O(n) index-based |
| Memory | Low | All buffers are static; only strdup for log messages is dynamic |
| Log hook collision | Medium | Last hook wins. Restore on `AIClose()`. Keep adapter as the last hook installed. |
| Thread safety | Medium | Command queue is mutex-protected. Other state is single-threaded (raylib's main loop). |
| Input injection | Medium | `SendInput` targets the foreground window. Non-Windows builds use no-op stubs. |
| Misuse | Medium | Must call `AIInit()` after `InitWindow()`. Must call `AIClearBackground()` each frame. `AIUpdate()` goes after `EndDrawing()`. |

---

## Quick Checklist

- [ ] Call `AIInit()` **after** `InitWindow()`
- [ ] Call `AIStartCommandLoop()` if using stdin-driven AI control
- [ ] Call `AIClearBackground()` at the start of each frame's draw phase
- [ ] Use `AIDrawXXX` wrappers instead of raw `DrawXXX` for scene registration
- [ ] Use `AILoadTexture()` instead of `LoadTexture()` for texture path tracking
- [ ] Call `AIUpdate()` **after** `EndDrawing()`
- [ ] Call `AIClose()` **before** `CloseWindow()`
- [ ] Don't hold `AIGetSceneJSON()` pointers across frames — they're invalidated on the next call
