#!/usr/bin/env python3
"""
ai_protocol_check.py - AI-side protocol conformance check for the barelib AI Adapter.

Launches a game executable that uses the AI Adapter command loop, drives it over
stdin/stdout pipes like an AI agent would, and asserts the wire protocol:
  - `version`            -> <<AI_OK>> with protocol version
  - `scene`              -> <<AI_SCENE_BEGIN>> ... <<AI_SCENE_END>> wrapping valid
                            JSON that carries protocol/screen/elements fields
  - unknown command      -> <<AI_ERROR>> [code:1]   (AI_ERR_UNKNOWN_COMMAND)
  - bad input argument   -> <<AI_ERROR>> [code:2]   (AI_ERR_BAD_ARGUMENT)
  - playback with no data-> <<AI_ERROR>> [code:4]   (AI_ERR_NO_DATA)
  - `help`               -> <<AI_HELP>> listing the version command
  - `quit`               -> clean process exit

Usage:
    python tests/ai_protocol_check.py [path/to/game.exe]

Requires a GUI/display (the game opens a window) and a game built with the AI
Adapter. Build the default harness with:
    cmake --build build --target ai_protocol_harness --config Debug
(e.g. examples/ai_game_template.exe also works). Not part of the default ctest run.

Exit code 0 = all checks passed, 1 = any check failed.
"""

import json
import os
import re
import subprocess
import sys
import threading
import time

# Expected error codes — must match AIErrorCode in src/raylib_ai_adapter.h
ERR_UNKNOWN_COMMAND = 1
ERR_BAD_ARGUMENT = 2
ERR_NO_DATA = 4

DEFAULT_EXE = os.path.join("bin", "Debug", "ai_protocol_harness.exe")


def read_protocol_version(header_path=None):
    """Parse AI_PROTOCOL_VERSION from the header so the check never drifts.
    Resolves relative to this file, so it works regardless of the CWD
    (e.g. when ctest runs from the build directory)."""
    if header_path is None:
        header_path = os.path.join(
            os.path.dirname(os.path.abspath(__file__)), "..", "src", "raylib_ai_adapter.h")
    pattern = re.compile(r'#define\s+AI_PROTOCOL_VERSION\s+"([^"]+)"')
    with open(header_path, "r", encoding="utf-8") as f:
        for line in f:
            m = pattern.search(line)
            if m:
                return m.group(1)
    raise RuntimeError("AI_PROTOCOL_VERSION not found in raylib_ai_adapter.h")


class GameDriver:
    """Spawns the game and manages the stdin/stdout protocol session."""

    def __init__(self, exe_path):
        self.proc = subprocess.Popen(
            [exe_path],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            encoding="utf-8",
            errors="replace",
            bufsize=1,
        )
        self.lock = threading.Lock()
        self.output = []          # all lines seen so far
        self._reader = threading.Thread(target=self._read_loop, daemon=True)
        self._reader.start()

    def _read_loop(self):
        for line in self.proc.stdout:
            with self.lock:
                self.output.append(line.rstrip("\n"))

    def send(self, cmd):
        """Write one command line to the game's stdin."""
        self.proc.stdin.write(cmd + "\n")
        self.proc.stdin.flush()

    def wait_for(self, marker, timeout=8.0):
        """Block until a line containing `marker` appears; return that line."""
        deadline = time.time() + timeout
        while time.time() < deadline:
            with self.lock:
                for line in self.output:
                    if marker in line:
                        return line
            time.sleep(0.05)
        return None

    def send_and_wait(self, cmd, marker, timeout=8.0):
        """Send a command, wait for a matching response line, then consume all
        output seen so far so stale lines never match a later expectation."""
        self.send(cmd)
        line = self.wait_for(marker, timeout)
        self.take_lines()
        return line

    def take_lines(self):
        """Drain and return all lines collected so far."""
        with self.lock:
            lines, self.output = list(self.output), []
            return lines

    def recent_lines(self, n=8):
        """Return up to n most recent output lines without consuming them.
        Used to capture failure context for machine-readable reports."""
        with self.lock:
            return list(self.output[-n:])

    def close(self):
        if self.proc.poll() is None:
            self.proc.terminate()
        try:
            self.proc.wait(timeout=3)
        except subprocess.TimeoutExpired:
            self.proc.kill()


def extract_block(lines, begin, end):
    """Return the content between <<begin>> and <<end>> markers, if present."""
    text = "\n".join(lines)
    m = re.search(re.escape(begin) + r"\n(.*?)\n" + re.escape(end), text, re.S)
    return m.group(1) if m else None


def main():
    exe = sys.argv[1] if len(sys.argv) > 1 else DEFAULT_EXE
    if not os.path.exists(exe):
        print(f"FAIL: game executable not found: {exe}")
        print("      Build it first, e.g. cmake --build build --target ai_game_template")
        return 1

    version = read_protocol_version()
    checks = []
    failed = []

    def check(name, ok, detail=""):
        checks.append((name, ok))
        status = "PASS" if ok else "FAIL"
        print(f"  [{status}] {name}" + (f"  ({detail})" if detail and not ok else ""))
        if not ok:
            failed.append(name)

    print(f"Driving {exe} (protocol {version}) ...")
    game = GameDriver(exe)

    try:
        # Let the game initialize and its command loop come up
        time.sleep(1.0)

        # 1) version command
        resp = game.send_and_wait("version", "<<AI_OK>>")
        check("version reports current protocol",
              resp is not None and version in (resp or ""), resp or "no response")

        # 2) scene command -> valid JSON with protocol/screen/elements
        game.send("scene")
        resp = game.wait_for("<<AI_SCENE_END>>")
        lines = game.take_lines()
        body = extract_block(lines, "<<AI_SCENE_BEGIN>>", "<<AI_SCENE_END>>")
        if body is None and resp:
            # fall back to everything since last drain
            body = extract_block(lines + [resp], "<<AI_SCENE_BEGIN>>", "<<AI_SCENE_END>>")
        ok = False
        detail = ""
        if body:
            try:
                data = json.loads(body)
                ok = (data.get("protocol") == version
                      and "screen" in data and "elements" in data)
                detail = "json fields ok" if ok else f"missing fields in: {body[:120]}"
            except json.JSONDecodeError as e:
                detail = f"invalid JSON: {e}"
        check("scene returns valid protocol JSON", ok, detail or "no scene block")

        # 3) unknown command -> structured error code 1
        resp = game.send_and_wait("frobnicate", "<<AI_ERROR>>")
        check("unknown command -> [code:1]",
              resp is not None and f"[code:{ERR_UNKNOWN_COMMAND}]" in (resp or ""),
              resp or "no error line")

        # 4) invalid input argument -> structured error code 2
        resp = game.send_and_wait("input press definitelynotakey123", "<<AI_ERROR>>")
        check("bad input arg -> [code:2]",
              resp is not None and f"[code:{ERR_BAD_ARGUMENT}]" in (resp or ""),
              resp or "no error line")

        # 5) playback with nothing recorded -> structured error code 4
        resp = game.send_and_wait("playback", "<<AI_ERROR>>")
        check("playback with no data -> [code:4]",
              resp is not None and f"[code:{ERR_NO_DATA}]" in (resp or ""),
              resp or "no error line")

        # 6) help lists the version command
        game.send("help")
        game.wait_for("<<AI_HELP_END>>")
        lines = game.take_lines()
        check("help lists version command",
              any("version" in ln for ln in lines), "; ".join(lines[:3]))

        # 7) quit -> process exits
        game.send("quit")
        try:
            game.proc.wait(timeout=8)
            exited = True
        except subprocess.TimeoutExpired:
            exited = False
        check("quit terminates the game", exited)

    finally:
        game.close()

    print("-" * 50)
    passed = sum(1 for _, ok in checks if ok)
    print(f"{passed}/{len(checks)} protocol checks passed")
    return 0 if not failed else 1


if __name__ == "__main__":
    sys.exit(main())
