# Contributing to barelib

Hello contributors! Welcome to barelib — a stripped-down, 2D-focused fork of
raylib with an **AI Adapter** for building games that AI agents can observe and
drive in real time.

## Project orientation

Read these before contributing:

- **`spec.txt`** — the single source of truth: platform scope, API principles
  (P1-P7), module layout (M1-M4), AI protocol, non-functional requirements
  (N1-N3) and repo hygiene rules (R1-R4).
- **`CONTEXT.md`** — architectural context: module seams, key decisions and the
  engine↔adapter boundary.
- **`docs/ai_adapter.md`** — the AI adapter protocol and integration guide.

## Platform scope

- **Authoring and usage is Windows-only** (desktop). The build toolchain is
  Visual Studio + `compile_game.bat`, or CMake (`cmake -B build -S .`).
- Linux / macOS / Web / Android builds are out of scope (no CI, no guarantees).

## How to help

- **C programming** — write/review/test/improve the engine (rcore, rlgl,
  rshapes, rtext, raudio) or the AI adapter (raylib_ai_adapter, ai_scene,
  ai_audio, ai_input, ai_protocol).
- **Tests** — every behavior change must keep `ctest` green (Windows):
  `test_ai_adapter`, `test_gpu`, `ai_protocol_scenario` and the raylib unit
  tests.
- **Examples** — the `examples/` showcase (ai_music_demo, ai_shapes_demo,
  ai_replay_demo) demonstrates engine features through the AI protocol; new
  feature showcases should follow that pattern.
- **Documentation** — README, spec, docs/ and this file must stay truthful to
  the code (stale docs are a bug).

## Coding conventions

Follow **`CONVENTIONS.md`** (C coding style, inherited from raylib) plus the
API principles in spec P1-P6:

- 2D-first, minimal API — every public function must be highly practical and
  self-explanatory; new public API must be justified in `spec.txt`.
- Exe-only engine: no DLL builds, `RLAPI`/`RMAPI` are empty macros (spec P7).
- Keep the AI seam clean: the adapter observes/drives via the documented
  protocol; engine internals stay reachable only through sanctioned seams
  (AIDeps, draw observer, state snapshot).

## Licensing

The project is licensed under the zlib license (see `LICENSE`), the same
permissive license as raylib. By contributing you agree your changes are
released under it.

## Pull request checklist

- [ ] Builds clean on Windows (`cmake --build build` and/or `compile_game.bat`)
- [ ] `ctest` passes (100% green)
- [ ] Changes stay within spec scope (platform, API principles, hygiene rules)
- [ ] Docs updated if behavior/API changed
- [ ] No build artifacts committed (spec R4)
