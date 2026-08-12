#!/usr/bin/env python3
"""
ai_tester_session.py - AI-tester end-to-end session (spec: does an AI tester work?)

Launches a game executable in the BACKGROUND (window not activated), switches the
input simulation to postmessage mode (background-safe injection), injects a key
via the AI Adapter protocol, and asserts through scene JSON that the game
actually responded — the observe -> act -> observe loop an AI tester relies on.

Why this exists (engine usage report #6 / R6): SendInput-based key injection
requires the game window in the foreground, so a background AI session's keys go
nowhere. The postmessage mode (inputmode command) posts WM_* messages straight
to the game window; this session verifies the fix end-to-end.

Timing note: an AI tester must wait for the game to respond before asserting.
The first frame of a CJK game loads the embedded font (tens of MB) and can block
the loop; assert on a later observation, not the first one after injection.

Usage:
    python tests/ai_tester_session.py [path/to/game.exe] [--key up] [--assert-dy -1]

Exit code 0 = injection took effect (AI tester loop works), 1 = it did not.
Requires a GUI/desktop (the game opens a hidden-usable window). Not part of ctest.
"""

import json
import os
import subprocess
import sys
import threading
import time

DEFAULT_EXE = os.path.join("bin", "Debug", "ai_snake_demo.exe")
OUT_TXT = open("tester_session_report.txt", "w", encoding="utf-8")


def log(msg=""):
    print(msg, file=OUT_TXT)
    OUT_TXT.flush()


def start(exe):
    p = subprocess.Popen([exe], stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                         text=True, encoding="utf-8", errors="replace", bufsize=1)
    out = []
    t = threading.Thread(target=lambda: [out.append(l.rstrip("\n")) for l in p.stdout])
    t.daemon = True
    t.start()
    return p, out


def send(p, out, cmd, wait=0.6):
    p.stdin.write(cmd + "\n")
    p.stdin.flush()
    time.sleep(wait)
    lines = list(out)
    out.clear()
    return "\n".join(lines)


def scene_of(text):
    if "<<AI_SCENE_BEGIN>>" not in text:
        return None
    body = text.split("<<AI_SCENE_BEGIN>>\n")[1].split("\n<<AI_SCENE_END>>")[0]
    return json.loads(body)


def snake_head(p, out):
    scene = scene_of(send(p, out, "scene"))
    if scene is None:
        return None
    for e in scene["elements"]:
        if e["type"] == "rect" and tuple(e["color"]) == (0, 100, 0, 255):
            return (e["x"] / 24, e["y"] / 24)
    return None


def main():
    args = sys.argv[1:]
    exe = args[0] if args and not args[0].startswith("--") else DEFAULT_EXE
    key = "up"
    if "--key" in args:
        key = args[args.index("--key") + 1]

    log("=" * 56)
    log("AI tester session: %s (background window, postmessage injection)" % exe)
    log("=" * 56)

    p, out = start(exe)
    time.sleep(1.2)

    r1 = send(p, out, "inputmode postmessage")
    log("[1] inputmode postmessage -> " + (r1.strip().splitlines()[-1] if r1.strip() else "(no resp)"))

    h0 = snake_head(p, out)
    log("[2] head before: %s" % (h0,))

    r3 = send(p, out, "input press %s" % key, wait=0.15)
    log("[3] input press %s -> %s" % (key, r3.strip().splitlines()[-1] if r3.strip() else "(no resp)"))

    time.sleep(0.5)          # let the game respond (font preload is done in InitWindow)
    h1 = snake_head(p, out)
    log("[4] head after (+0.5s): %s" % (h1,))

    dy = (h1[1] - h0[1]) if (h0 and h1) else None
    if dy is not None and dy < 0:
        verdict = "PASS"
        detail = "head y delta %d (upward): injected '%s' took effect — AI tester observe/act loop works" % (-dy, key)
    else:
        verdict = "FAIL"
        detail = "head moved %s -> %s: injection did not take effect" % (h0, h1)
    log("\n[5] verdict: %s" % verdict)
    log("    %s" % detail)

    send(p, out, "quit", wait=1.0)
    try:
        p.wait(timeout=5)
    except Exception:
        p.kill()
    return 0 if verdict == "PASS" else 1


if __name__ == "__main__":
    sys.exit(main())
