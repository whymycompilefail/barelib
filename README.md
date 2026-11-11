**raylib** — 2D-focused fork, built for AI game agents.

A stripped-down, 2D-focused fork of [raylib](https://github.com/raysan5/raylib), with an **AI Adapter** that lets LLMs observe, understand, and interact with your game in real time.

No 3D, no shaders, no VR. Just 2D drawing, input, audio, and a JSON scene description that an AI can read frame by frame.

---

features
--------

**Core 2D engine**
  - Windows (desktop) — the only supported platform (authoring, testing and runtime); other platforms out of scope
  - OpenGL hardware accelerated (1.1, 2.1, 3.3, ES 2.0, ES 3.0)
  - Software renderer backend (no GPU required)
  - 2D shapes, textures, text, and audio
  - Gradient rectangles (`DrawRectangleGradientV/H`) — four-vertex colour interpolation for skies, fog bands and vignettes
  - Embedded CJK font baked at a requested size (`LoadFontEmbedded(1..64)`); the default font stays 16px so existing games are unchanged
  - Render textures for offscreen rendering
  - Camera2D with world-to-screen and screen-to-world transforms
  - Input: keyboard, mouse, gamepad, touch
  - Written in C99, zero external dependencies at build time

**AI Adapter** (`raylib_ai_adapter.h`)
  - **Versioned protocol**: scene JSON carries a `protocol` field (`AI_PROTOCOL_VERSION`), `version` command reports it — AI agents can adapt to engine evolution
  - **Structured error model**: `<<AI_ERROR>> [code:N]` with `AIErrorCode` (unknown command / bad argument / buffer full / no data / not supported), plus `AIGetLastError()`
  - **Non-invasive observability**: `AISetAutoSceneCapture()` — stock games using
    plain DrawXXX (no wrappers, no code changes) automatically appear in the
    scene JSON via an engine-level draw hook (rect/circle/text families)
  - **Explicit capacity**: `MAX_AI_ELEMENTS` is configurable; when the scene buffer overflows, the JSON reports an `overflow` count instead of silently truncating
  - **Scene observation**: AIDrawXXX wrapper functions register each drawn element; `AIGetSceneJSON()` produces structured JSON
  - **Command loop**: stdin-driven control — `version`, `scene`, `input`, `pause`, `resume`, `step`, `state`, `errors`, `elements`, `record`, `playback`, `snapshot`, `diff`, `help`, `quit`
  - **Input injection**: key press, mouse click, scroll, text type via `AIInjectInput()` or `input` command.
    Two modes (`inputmode` command / `AISetInputMode()`): `sendinput` (default, OS input queue,
    requires the game window in the foreground) and `postmessage` (WM_* messages posted straight to
    the game window — works in the background, so an unattended AI tester can drive the game)
  - **Game loop control**: pause / resume / step-by-frame for frame-by-frame debugging
  - **Record / playback**: record input action sequences and replay them for bug reproduction
  - **Scene snapshots**: save scene state, diff against current frame to detect changes
  - **Log interception**: capture raylib TraceLog output with timestamps
  - **Procedural audio**: AIPlayTone / AIPlayNoise with 5 waveforms (sine, square, triangle, sawtooth, noise)
  - **Pixel sprites**: `AILoadSpriteSheet()` reads Pixel Loom / Aseprite sheet JSON, owns tag playback
    (forward / reverse / pingpong, per-frame durations) and records the live tag + frame in the scene JSON
    (`AI_DRAW_SPRITE`); atlases load with nearest filtering; `AISpriteCloneState()` gives each actor an
    independent clock, `AISpriteDrawEx()` adds anchors, sub-1 scales and additive hit-flash in one element
  - **Backend state query**: window, input, time, audio state as JSON
  - **Dependency injection**: mock AIDeps for testing without a window
  - No network, no external AI runtime — just data your agent can consume

**Designed for testing**
  - 14 CTest suites, 100% passing (unit + GPU-hidden-window + protocol scenario)
  - GPU tests run in hidden window, no desktop interference
  - Pure CPU functions tested without window or GPU
  - All tests runnable via a single `ctest` command

**CJK text support**
  - Embedded 13MB Sarasa Gothic SC font with full GB2312 coverage
  - Zero external font files needed — Chinese text works out of the box
  - Subset includes ASCII, Latin-1, CJK ideographs, symbols, and more

what's removed
--------------
Compared to upstream raylib, this fork removes everything 3D and GPU-programmable:

  - No 3D shapes, models, or billboards
  - No materials or PBR
  - No skeletal animation
  - No shader API (loading, custom shaders, postprocessing)
  - No VR stereo rendering
  - No compute shaders or SSBOs
  - No automation events recording/playback

quick start
-----------

```c
#include "raylib.h"

int main(void)
{
    InitWindow(800, 600, "Hello");

    while (!WindowShouldClose())
    {
        BeginDrawing();
        ClearBackground(RAYWHITE);
        DrawText("Hello from raylib 2D!", 10, 10, 20, DARKGRAY);
        EndDrawing();
    }

    CloseWindow();
    return 0;
}
```

With AI Adapter (AIDrawXXX wrappers + command loop):

```c
#include "raylib.h"
#include "raylib_ai_adapter.h"

int main(void)
{
    InitWindow(800, 600, "AI Game");
    AIAdapter *ai = AIInit();
    AIStartCommandLoop(ai);  // listen for stdin commands from AI agent

    while (!WindowShouldClose())
    {
        BeginDrawing();
        AIClearBackground(RAYWHITE);              // ClearBackground + reset scene buffer
        AIDrawRect(10, 10, 100, 40, 0, BLUE);     // DrawRect + register element
        AIDrawText(GetFontDefault(), "Score: 100", 10, 60, 20, 1, BLACK);  // DrawText + register element
        EndDrawing();
        AIUpdate(ai);  // process commands, advance playback, auto-output JSON
    }

    AIClose(ai);
    CloseWindow();
    return 0;
}
```

AI agents interact via stdin/stdout pipes:

```
> version            # get protocol version
< <<AI_OK>> protocol 1.0 (barelib AI adapter)
> scene              # get current scene as JSON
< {"protocol":"1.0","screen":{"width":800,"height":600},"elements":[...]}
> input press space  # inject a key press
> snapshot           # save current scene
> step 5             # advance 5 frames
> diff               # compare current scene to snapshot
< {"changed":2,"changes":[...]}
> record             # start recording inputs
> input press right
> record stop        # stop recording
> playback           # replay recorded inputs
> quit               # close the game
```

build
-----

```bash
cmake -B build -S . -G "Visual Studio 17 2022" -DBUILD_EXAMPLES=OFF
cmake --build build --target raylib --config Debug
```

Output goes to `bin/Debug/`.

- **Static library only** (spec P7: exe-only engine, no DLL builds) — link
  `raylib.lib`, no runtime DLL needed.

compile a game
--------------

Use the included compile script (Windows / MSVC):

```bat
compile_game.bat my_game.c           :: compile and run
compile_game.bat my_game.c build     :: compile only
```

The script auto-configures Visual Studio (probes 2022/2019 install paths) and
fixes non-ASCII TEMP path issues. See `examples/` for complete starters
(AI-observable game loops, AIDrawXXX wrappers, command loop, procedural audio).

run tests
---------

```bash
cmake -B build -S . -DBUILD_EXAMPLES=OFF
cmake --build build --config Debug
cd build && ctest -C Debug
```

The test suite includes:

- **Unit tests** (no window/GPU needed): AI adapter (123 assertions: protocol
  version, element overflow, input error feedback, scene JSON truncation safety,
  record/playback, state snapshot JSON, observer ownership), raymath, rtext,
  rtextures, raudio, rcore, rshapes — run headless.
- **GPU tests** (hidden window): `test_gpu`, plus `ai_protocol_scenario` — an
  end-to-end protocol conformance regression that drives the live game over
  stdin/stdout pipes (8 steps: version, scene JSON, error codes, help, quit).
  Both carry the `gpu` label; run them with `ctest -L gpu`.

Protocol regression tooling (requires a GUI/GPU desktop):

```bash
cmake --build build --target ai_protocol_harness --config Debug
python tests/ai_protocol_check.py              # 7 quick protocol smoke checks
python tests/ai_scenario_check.py tests/scenarios/basic.json --report out.json
python tests/ai_tester_session.py              # end-to-end: inject a key in the
                                               # background window (postmessage mode)
                                               # and verify via scene JSON that the
                                               # game actually moved
# --report writes a machine-readable JSON report for the AI test->report->fix loop
```

project structure
-----------------

```
src/              — Library source (raylib + AI Adapter)
  raylib.h              — Public API umbrella header
  raylib_core.h         — Window, input, filesystem, timing
  raylib_shapes.h       — 2D shape drawing
  raylib_textures.h     — Image/texture loading and drawing
  raylib_text.h         — Font loading, text rendering
  raylib_audio.h        — Audio device, Sound/Music/AudioStream
  raymath.h             — Vector2/Vector3/Vector4/Matrix math
  rlgl.h                — OpenGL abstraction (low-level public API)
  rcore.c               — Core implementation
  rcore_fs.c            — File system implementation
  rcore_input.c         — Input state implementation
  rshapes.c             — Shape drawing implementation
  rtextures.c           — Texture implementation
  rtext.c               — Text implementation
  raudio.c              — Audio implementation
  raymath.c             — Math implementation
  raylib_ai_adapter.h   — AI Adapter public API
  raylib_ai_adapter.c   — AI Adapter facade (init/update/close, state JSON)
  ai_scene.c            — Scene elements, 16 AIDrawXXX wrappers, JSON/diff
  ai_audio.c            — Procedural audio (waveforms, tone slots)
  ai_sprite.c           — Pixel-art sheets (Pixel Loom / Aseprite JSON), tag playback, sprite scene elements
  raylib_ai_sprite.h    — Sprite sheet public API
  ai_input.c            — Input injection, key names, record/playback
  ai_protocol.c         — stdin command loop
  raylib_ai_input.h     — Platform input simulation interface
  raylib_ai_input_win32.c — Windows SendInput implementation
  raylib_ai_input_stubs.c — Non-Windows input stubs
  raylib_context.h      — RaylibContext (global state container)
  raudio_observer.h     — Audio buffer observer (miniaudio-free)
  cjk_font_data.h/.c    — Embedded CJK font data (~81MB generated; the .c is git-ignored,
                          CMake regenerates it offline from src/fonts/cjk_subset.ttf with
                          scripts/embed_font_data.py — Python 3 stdlib only, no network/pip)
  fonts/cjk_subset.ttf  — Tracked subset font (Sarasa Gothic SC Regular, SIL OFL 1.1);
                          re-subset with scripts/prepare_cjk_font.py (needs network + pip)
examples/         — Engine feature examples (upstream raylib samples removed)
  ai_music_demo.c    — Feature showcase: AI-observable music visualizer (AIDrawXXX, CJK HUD, procedural audio)
  ai_shapes_demo.c   — Feature showcase: AI-observable 2D shape gallery (all primitives incl. rounded rect + sector)
  ai_replay_demo.c   — Feature showcase: record/playback protocol + input injection (AI operability)
tests/            — Unit tests (test_ai_adapter.c + others)
docs/
  ai_adapter.md         — AI Adapter full API reference
compile_game.bat  — Compile script for AI-authored games
```

docs
----

- [AI Adapter](docs/ai_adapter.md) — full API reference, command loop, record/playback, JSON format
- [Spec](spec.txt) — platform scope, API principles, module layout, hygiene rules
- [Contributing](CONTRIBUTING.md) — how to build, test and contribute
- [Security](SECURITY.md) — vulnerability reporting and project security notes

license
-------

zlib/libpng. See [LICENSE](LICENSE).

This is a fork of [raylib](https://github.com/raysan5/raylib) by Ramon Santamaria (@raysan5), used under the same license.
