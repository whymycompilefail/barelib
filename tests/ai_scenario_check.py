#!/usr/bin/env python3
"""
ai_scenario_check.py - Scenario-driven protocol regression runner (spec M3).

Executes a JSON scenario script against a live game process, replaying a
recorded command/expectation sequence to catch protocol regressions — the
"scene script replay baseline" of the roadmap.

Scenario file format (JSON array of steps):
  {"cmd": "version", "expect": "<<AI_OK>>", "contains": ["1.0"]}
      send `cmd`, wait for a line containing `expect`, assert it contains every
      substring in `contains` (string or list).
  {"cmd": "scene", "block": {"begin": "...", "end": "...", "json_fields": [...]}}
      additionally capture the block between markers and assert it parses as
      JSON carrying every listed top-level field.
  {"cmd": "help", "lines_contain": ["version"]}
      assert at least one output line (since previous step) contains each
      substring.
  {"cmd": "quit", "expect_exit": true}
      assert the process exits after the command.
  {"cmd": "pause", "settle": 0.5}
      optional fixed wait after the command (no assertion).

Usage:
    python tests/ai_scenario_check.py <scenario.json> [path/to/game.exe] [--report out.json]

Default game exe: bin/Debug/ai_protocol_harness.exe (build with
`cmake --build build --target ai_protocol_harness --config Debug`).
Requires a GUI/display (hidden window). Exit code 0 = all steps passed.

--report <file> writes a machine-readable JSON report (per-step results with
failure context) for AI agents to consume in the test->report->fix loop.
"""

import json
import os
import re
import sys

# Reuse the live-game protocol driver from ai_protocol_check.py
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from ai_protocol_check import GameDriver, read_protocol_version, extract_block  # noqa: E402

DEFAULT_EXE = os.path.join("bin", "Debug", "ai_protocol_harness.exe")


def run_step(game, step):
    """Execute one scenario step; returns (ok, detail, observed).
    observed: recent output lines captured when the step failed."""
    cmd = step.get("cmd", "")
    if not cmd:
        return False, "step missing 'cmd'", game.recent_lines()

    game.send(cmd)
    ok, detail = True, ""

    if step.get("expect_exit"):
        try:
            game.proc.wait(timeout=8)
            return True, "process exited", []
        except Exception:
            return False, "process did not exit", game.recent_lines()

    expect = step.get("expect")
    if expect:
        resp = game.wait_for(expect)
        if resp is None:
            return False, f"no line matching '{expect}'", game.recent_lines()
        contains = step.get("contains", [])
        if isinstance(contains, str):
            contains = [contains]
        for sub in contains:
            if sub not in resp:
                ok, detail = False, f"response missing '{sub}': {resp.strip()}"
                break

    block = step.get("block")
    if ok and block:
        # Wait for the closing marker first: commands are processed once per
        # game frame, so the block may not be emitted yet right after send().
        if game.wait_for(block["end"]) is None:
            ok, detail = False, f"no closing marker {block['end']}"
        if ok:
            lines = game.take_lines()
            body = extract_block(lines, block["begin"], block["end"])
            if body is None:
                ok, detail = False, f"no block between {block['begin']} and {block['end']}"
            else:
                try:
                    data = json.loads(body)
                    missing = [f for f in block.get("json_fields", []) if f not in data]
                    if missing:
                        ok, detail = False, f"missing json fields: {missing}"
                except json.JSONDecodeError as e:
                    ok, detail = False, f"block is not valid JSON: {e}"

    if ok and step.get("lines_contain"):
        lines = game.take_lines()
        need = step["lines_contain"]
        if isinstance(need, str):
            need = [need]
        for sub in need:
            if not any(sub in ln for ln in lines):
                ok, detail = False, f"no output line contains '{sub}'"

    if step.get("settle"):
        import time
        time.sleep(step["settle"])

    # Drain any remaining output so stale lines never match later steps
    game.take_lines()
    observed = game.recent_lines() if not ok else []
    return ok, detail, observed


def main():
    args = sys.argv[1:]
    report_path = None
    if "--report" in args:
        i = args.index("--report")
        report_path = args[i + 1] if i + 1 < len(args) else None
        del args[i:i + 2]
    if len(args) < 1:
        print("Usage: python ai_scenario_check.py <scenario.json> [game.exe] [--report out.json]")
        return 2
    scenario_path = args[0]
    exe = args[1] if len(args) > 1 else DEFAULT_EXE

    if not os.path.exists(exe):
        print(f"FAIL: game executable not found: {exe}")
        return 1
    with open(scenario_path, "r", encoding="utf-8") as f:
        scenario = json.load(f)

    version = read_protocol_version()
    print(f"Replaying {scenario_path} against {exe} (protocol {version}) ...")
    game = GameDriver(exe)
    step_results = []
    try:
        import time
        import datetime
        time.sleep(1.0)  # let the command loop come up
        failed = []
        for i, step in enumerate(scenario, 1):
            ok, detail, observed = run_step(game, step)
            name = step.get("cmd", f"step {i}")
            status = "PASS" if ok else "FAIL"
            print(f"  [{status}] {name}" + (f"  ({detail})" if not ok else ""))
            result = {"cmd": name, "ok": ok}
            if not ok:
                result["detail"] = detail
                result["expected"] = step
                result["observed"] = observed
                failed.append(name)
            step_results.append(result)
    finally:
        game.close()

    total = len(scenario)
    passed = total - len(failed)
    print("-" * 50)
    print(f"{passed}/{total} scenario steps passed")

    if report_path:
        import datetime
        report = {
            "tool": "ai_scenario_check",
            "scenario": os.path.basename(scenario_path),
            "game": os.path.basename(exe),
            "protocol": version,
            "timestamp": datetime.datetime.now().isoformat(timespec="seconds"),
            "passed": passed,
            "total": total,
            "exit_ok": not failed,
            "steps": step_results,
        }
        with open(report_path, "w", encoding="utf-8") as f:
            json.dump(report, f, ensure_ascii=False, indent=2)
        print(f"Report written to {report_path}")

    return 0 if not failed else 1


if __name__ == "__main__":
    sys.exit(main())
