# T9 修复与验收结果（`:check_toolchain`）

> 对应交接：`docs/t9-bat-e2e-handoff.md`（2026-11-11 修订版）
> 唯一改动文件：`D:\barelib\compile_game.bat`
> 结论：缺陷**已复现并修复**；§5 的 7 项验收中 **6 项 PASS，1 项因环境无 `bash` 无法执行**（如实记录，未假装通过）。

---

## 1. 缺陷复现（修复前）

按交接 §3 的人造「半成功 vcvars」方案，不依赖 `reg.exe` 策略。

`build\tmp\t9\fake_vcvars.cmd`（CRLF）：

```bat
@echo off
set "INCLUDE=D:\barelib\src"
set "LIB=D:\barelib\bin\Debug"
set "PATH=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64;%PATH%"
```

`build\tmp\t9\run_repro.cmd`（CRLF）：

```bat
@echo off
set "INCLUDE="
set "LIB="
set "VCVARS=D:\barelib\build\tmp\t9\fake_vcvars.cmd"
call "D:\barelib\compile_game.bat" D:\barelib\build\tmp\t9\t9_main.c D:\barelib\build\tmp\t9\t9_helper.c -o D:\barelib\build\tmp\t9\t9game_repro.exe build
echo BAT_EXIT=%ERRORLEVEL%
exit /b %ERRORLEVEL%
```

执行 `& "D:\barelib\build\tmp\t9\run_repro.cmd"`：

```
[compile_game] Sources:  "...\t9_main.c" "...\t9_helper.c"
[compile_game] Output:  ...\t9game_repro.exe

t9_main.c
t9_helper.c
正在生成代码...
LINK : fatal error LNK1181: 无法打开输入文件“opengl32.lib”

[compile_game] ERROR: Compilation failed.
BAT_EXIT=1
RUNNER_EXIT=1
```

- 无 `WARNING: vcvars did not produce a usable toolchain.`、无 `Trying vswhere.exe...` —— 诊断被跳过。
- 与交接 §2 描述的机制完全一致：`INCLUDE` 与 `cl` 可用，`LIB` 缺 SDK `um\x64`，`:check_toolchain` 误判为可用。

---

## 2. 修复内容

只改 `:check_toolchain`（其余逻辑、label、行尾、`/MD`、TEMP 修复、参数解析全部保持原样）：

```bat
:check_toolchain
set "TOOLCHAIN_OK=0"
if not defined INCLUDE exit /b 0
if not defined LIB exit /b 0
where cl >nul 2>&1
if errorlevel 1 exit /b 0
REM vcvars64.bat calls reg.exe. If reg.exe is blocked, INCLUDE and cl can
REM still be set while the Windows SDK Lib\<ver>\um\x64 path is missing ->
REM LNK1181 "cannot open opengl32.lib" with no readable root cause. Require
REM the SDK import-lib directory before trusting vcvars.
set "LIB_NO_UM=!LIB:um\x64=!"
if "!LIB_NO_UM!"=="!LIB!" exit /b 0
set "TOOLCHAIN_OK=1"
exit /b 0
```

判定失败后自动落入原有 `:diagnose` → `:try_fallback` → 二次 `:check_toolchain` → 明确报错 `exit /b 1` 的流程，fallback 本身未改动。

---

## 3. §5 验收逐项

| # | 验收项 | 命令 | 结果 | 判定 |
|---|---|---|---|---|
| 1 | 半成功环境可编过并运行 | `& run_repro.cmd` | `BAT_EXIT=0`、`RUNNER_EXIT=0`、`t9game_repro.exe` 生成、直接运行 `PROG_EXIT=0` | **PASS** |
| 2 | 必须出现的 5 段诊断 | 同上 | 见下方输出片段，5 段全部出现 | **PASS** |
| 3 | 不得只有 `LNK1181` / `C1083` | 同上 | 未出现任何 `LNK`/`C1083`，编译直接成功 | **PASS** |
| 4a | `run_c.cmd` 回归 | `& run_c.cmd` | `RUNNER_EXIT=0`，fallback 组装成功 | **PASS** |
| 4b | `run_d.cmd` 回归 | `& run_d.cmd` | `RUNNER_EXIT=1`，明确报错，无 exe | **PASS** |
| 4c | 场景 A：多文件 + `-o` + `build` | `& compile_game.bat t9_main.c t9_helper.c -o t9game.exe build` | 退出 0，无 `Running...` | **PASS** |
| 4d | 场景 B：默认输出名 + 运行 | `& compile_game.bat t9_marker.c` | 输出 `t9_marker.exe`，出现 `Running...` 与 `T9_RAN` | **PASS** |
| 4e | 场景 E：参数错误 | 无参数 / `-o` 缺值 | 退出 1 / 1，无 `cl` 调用 | **PASS** |
| 5 | 静态检查 + CRLF | 见 §5 | 8 labels，0 缺失，0 未引用；`CRLF=265, bareLF=0` | **PASS** |
| 6 | `tools/smoke.sh` | `bash D:/barelib_test/game/tools/smoke.sh` | **未执行**：本机无 `bash`（PATH 无，`C:\Program Files\Git`、`C:\msys64`、`C:\cygwin64` 均不存在） | **BLOCKED** |
| 7 | ctest | `ctest -C Debug` / `ctest -C Debug -LE gpu` | **15/15 通过** / **12/12 通过** | **PASS** |

### 修复后 `run_repro.cmd` 输出

```
[compile_game] WARNING: vcvars did not produce a usable toolchain.
[compile_game] Resolved toolchain values:
  VCVARS  = "D:\barelib\build\tmp\t9\fake_vcvars.cmd"
  INCLUDE = "D:\barelib\src"
  LIB     = "D:\barelib\bin\Debug"
  VSROOT  = ""
  MSVCROOT= ""
  SDKVER  = ""
  CL_BIN  = ""
[compile_game] Trying vswhere.exe + Windows SDK fallback...
[compile_game] Fallback toolchain assembled from vswhere + SDK.

[compile_game] Sources:  "...\t9_main.c" "...\t9_helper.c"
[compile_game] Output:  ...\t9game_repro.exe

t9_main.c
t9_helper.c
正在生成代码...

[compile_game] Compilation succeeded.
BAT_EXIT=0
```

---

## 4. `smoke.sh` 无法执行的具体证据

```
Get-Command bash  → 不存在
Test-Path 'C:\Program Files\Git'        → False
Test-Path 'C:\Program Files (x86)\Git'  → False
Test-Path 'C:\msys64'                   → False
Test-Path 'C:\cygwin64'                 → False
```

未尝试安装 shell 或改写脚本，按交接要求如实记录为未执行。

---

## 5. 静态检查

```
labels=8  CRLF=265  bareLF=0
missing_goto=  (空)
missing_call=  (空)
unused=        (空)
```

labels：`args_done` / `check_toolchain` / `diagnose` / `parse_args` / `toolchain_ready` / `try_fallback` / `try_fallback_start` / `vcvars_resolved`。

---

## 6. 已观察到但本次未改的问题（不属本次委托范围）

按 §4.2「必须保持不变」与「最小补丁」要求，以下现象保持原样，供后续评估：

1. **`/MD` 与 Debug 版 `raylib.lib` 的 CRT 不一致**：用真实 raylib 程序（引用 `Fade`/`GetRandomValue`）链接时会输出
   `LINK : warning LNK4098: 默认库"MSVCRTD"与其他库的使用冲突`。`bin\Debug\raylib.lib` 由 CMake Debug 构建（`/MDd`）。
2. **`-o` 不带扩展名会假失败**：`cl /Fe:noext` 实际产出 `noext.exe`，bat 的 `if not exist "%EXE%"` 判定为失败并退出 1。
3. **运行模式吞掉游戏退出码**：结尾 `exit /b 0` 恒定返回 0，游戏失败时调用方无法感知。
4. **`.obj` 落在调用者当前目录**：未指定 `/Fo`，多工程同名源文件可能互相覆盖。

以上 4 项均有可复现命令，若需要可另开一轮按同样流程修复与验收。
