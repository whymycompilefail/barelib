"""T6 regression: AIClose must not defer teardown when stdin stays open.

Launches the hidden-window protocol harness, sends `quit` over an OPEN pipe
(never closes stdin), and waits for the process to exit. The stdin reader thread
is therefore still blocked in fgets() when AIClose() runs.

Before the T6 fix AIClose() could only detach that thread and leak the adapter
("adapter memory deferred to process exit"). Now it cancels the synchronous
ReadFile so the thread exits and the adapter is freed normally.

An EOF pipe (`echo quit | harness`) does NOT reproduce this: fgets() returns
NULL immediately and the thread is already gone.

Usage: python ai_close_check.py <path-to-ai_protocol_harness.exe>
"""

import subprocess
import sys

MARKER = "deferred to process exit"
TIMEOUT_SECONDS = 30


def main():
    if len(sys.argv) != 2:
        print("usage: ai_close_check.py <ai_protocol_harness.exe>")
        return 2

    exe = sys.argv[1]
    proc = subprocess.Popen(
        [exe],
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        encoding="utf-8",
        errors="replace",
    )

    # Ask the game to quit but deliberately keep the pipe open.
    proc.stdin.write("quit\n")
    proc.stdin.flush()

    try:
        rc = proc.wait(timeout=TIMEOUT_SECONDS)
    except subprocess.TimeoutExpired:
        proc.kill()
        proc.wait()
        print("FAIL: harness did not exit within %ds" % TIMEOUT_SECONDS)
        return 1

    stdout = proc.stdout.read()
    stderr = proc.stderr.read()
    proc.stdin.close()

    output = stdout + stderr
    defer_count = output.count(MARKER)

    print("returncode:      %d" % rc)
    print("defer warnings:  %d" % defer_count)
    if defer_count:
        for line in output.splitlines():
            if MARKER in line:
                print("  %s" % line.strip())

    if rc != 0:
        print("FAIL: harness exited with %d" % rc)
        print("stdout tail: %s" % stdout[-400:].replace("\n", " | "))
        print("stderr tail: %s" % stderr[-400:].replace("\n", " | "))
        return 1

    if defer_count:
        print("FAIL: stdin thread was not cancelled; adapter memory was deferred")
        return 1

    print("PASS: stdin thread cancelled, adapter released normally")
    return 0


if __name__ == "__main__":
    sys.exit(main())
