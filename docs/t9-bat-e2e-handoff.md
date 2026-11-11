# T9 交接：`compile_game.bat` 工具链判定缺陷（已闭环）

> 状态：**缺陷已修复，§5 的 7 项验收全部通过**。修复由外部 AI 完成，只改 `compile_game.bat`（+7 行），结果记录见 `docs/t9-bat-e2e-result.md`；本会话另做了独立复验，见 §0。
> §1–§4 保留为缺陷分析与修复方案的历史记录，**不再代表当前状态**；§9 已改为实际验收结论。
> 项目 `D:\barelib`，最后复核 2026-11-11 23:1x。四批次累计改动仍**未 git commit / 未推送**。

---

## 0. 闭环状态与本会话独立复验

外部 AI 落地的是 §4 的最小补丁：`:check_toolchain` 增加 `if not defined LIB` 与 `!LIB:um\x64=!` 子串判定（现 bat 第 195-208 行）。`git diff --stat` 从 +1361/−144 变为 **+1368/−144**，恰好只多 7 行，其余 22 个文件未被改动。

**独立复验（不采信结果文档自述，全部实跑）**

| 复验项 | 实测结果 |
|---|---|
| **场景 A：上一轮 FAIL 的同一条命令**（真实 vcvars 路径，多文件 + `-o` + `build`） | 退出 **0**；`WARNING: vcvars did not produce a usable toolchain` → `Resolved toolchain values` → `Trying vswhere` → `Fallback toolchain assembled` → `Compilation succeeded`，exe 生成。上一轮是 `LNK1181` + 退出 1 |
| 半成功人造复现 `run_repro.cmd` | 退出 0，5 段诊断齐全，无 `LNK1181`/`C1083`，exe 生成 |
| 双失败 `run_d.cmd` | 退出 1，明确报「reg.exe 被策略拦截，不是缺 CRT」，无 exe |
| **完整工具链不误伤**（本会话新增，外部未测）：手动注入完整 `INCLUDE`/`LIB`/`PATH`，`VCVARS` 指向不存在文件 | 退出 0，**无 WARNING、无 fallback**，直接编译成功 → 补丁没把正常路径逼进兜底 |
| **反事实对照**（本会话新增）：临时副本 `compile_nopatch.bat` 摘除那 3 行补丁，同一 `fake_vcvars.cmd` fixture | `LNK1181: 无法打开 opengl32.lib`、**无诊断**、退出 1、无 exe → 证明是补丁本身起了作用，不是环境变了 |
| 场景 B / E | B：默认名 `t9_marker.exe`、`Running...`、`T9_RAN` 打印，退出 0；E：无参数退出 1、`-o` 缺值退出 1，均未调 cl |
| 静态 | `CRLF=265 / bareLF=0`；8 label、0 缺失、0 未引用；公开 API 仍 **752** |
| **`smoke.sh`**（外部结果文档标 BLOCKED「本机无 bash」） | 本会话 Git Bash 补跑：`EXIT=0`，10 关状态机全 ok，全流程连打 `ENDING reached` → **PASS，不再是缺口** |
| `ctest` | `ctest -C Debug` **15/15**、`-LE gpu` **12/12** |

**口径纠正（旧结论作废）**：§4.3 与本会话工作日志曾写「`run_d.cmd` 的 runner 进程退出码恒为 0，打印的 1 只是 bat 的退出码」。实测 runner 现以 `exit /b %ERRORLEVEL%` 收尾，PowerShell `$LASTEXITCODE` = **1**，该判断不成立。`:diagnose` 在失败路径被调用两次、`Resolved toolchain values` 打印两遍，仍属实（非阻塞）。

**本次真实触发过一次 `reg.exe` 拦截提示**（场景 A 走真实 vcvars 路径所致），未重试、未绕过；后续验证一律用 §3 的人造半成功方案或 F 场景的完整工具链注入。

**残留缺陷（不属本次委托，已实测确认，供后续排期）**：`-o` 目标不带 `.exe` 时，`cl` 实际生成了 `xxx.exe`，但 `if not exist "%EXE%"` 判定失败并报 `Compilation failed.`、退出 1。外部结果文档 §6 另列 `LNK4098` CRT 警告、运行模式吞掉游戏退出码、`.obj` 落在调用者当前目录，均保持原样。

---

## 1. 原始任务，以及哪里没做好

**T9 原始现象**（`docs/engine-fixes-handoff.md` §T9）：`call vcvars64.bat` 内部会调 `reg.exe`，受限环境被拦 → `INCLUDE` 为空 → cl 报 `fatal error C1083: 无法打开 "stdio.h"`。**根因与 C Runtime 无关，但报错只给下游症状，排查成本极高**。另外一次只能编译一个 `.c`。

要求：vcvars 失败时打印可读诊断、尝试 `vswhere.exe` + SDK 兜底、仍失败则给明确根因；并支持多源文件。

**已经做好的部分**（不用重做）：多源文件解析、`-o name.exe`、任意位置的 `build` 模式、2022/2019 × 4 edition × 32/64 位 vcvars 探测、`:diagnose` 打印已解析变量、`:try_fallback` 用 vswhere + SDK 手拼 INCLUDE/LIB、非 ASCII TEMP 修复、`/MD` 说明、非 `build` 模式运行产物、纯 CRLF。

**没做好的部分（本次要修的）**：`compile_game.bat` 第 195-201 行的 `:check_toolchain` 只判

```bat
if not defined INCLUDE exit /b 0
where cl >nul 2>&1
if errorlevel 1 exit /b 0
set "TOOLCHAIN_OK=1"
```

它**完全不检查 `LIB`**。当 `reg.exe` 被拦时 `vcvars64.bat` 会「半成功」——`INCLUDE` 设上了、`cl.exe` 进了 PATH，但 Windows SDK 的 `Lib\<ver>\um\x64` 没拼进 `LIB`。于是 `:check_toolchain` 误判工具链可用、`goto :toolchain_ready` **跳过 `:try_fallback`** 直接编译：编译期通过，链接期报

```
LINK : fatal error LNK1181: 无法打开输入文件"opengl32.lib"
[compile_game] ERROR: Compilation failed.
```

这跟原来的 `C1083: 无法打开 stdio.h` 是**同一类下游症状换了个文件名**。也就是说，T9 最核心的验收——「vcvars 不可用时输出可读根因，而不是只丢一个看似无关的缺文件错误」——在真正被拦的环境里**没有达成**。

> 附带说明：`docs/t9-bat-e2e-result.md` 是外部 AI 在 `reg.exe` 未被拦、vcvars 完整成功的环境里跑出的 5/5 PASS，并据此写了「bat 无需修改」。那份结果可以留作宽松环境的回归记录，**但不能作为本任务的验收依据**；它的结论已作废。

---

## 2. 缺陷的精确机制

```
call "%VCVARS%"  (第 107 行)
   └─ vcvars64.bat 内部调 reg.exe 查 SDK 路径
        └─ reg.exe 被安全策略拦 → SDK Lib 路径没拼上
             INCLUDE = 有值（MSVC 部分）
             PATH    = 含 cl.exe
             LIB     = 只有 MSVC lib\x64，缺 SDK um\x64  ← 关键
:check_toolchain (第 109 行)
   └─ INCLUDE defined ✓ / where cl ✓ → TOOLCHAIN_OK=1
        └─ goto :toolchain_ready → 跳过 :diagnose 和 :try_fallback
cl.exe ... (第 164 行)
   └─ 编译 OK（夹具无头文件依赖）
   └─ 链接找 opengl32.lib / gdi32.lib / user32.lib → 全在 SDK um\x64 → LNK1181
```

`opengl32.lib` 等确实存在于 `C:\Program Files (x86)\Windows Kits\10\Lib\10.0.26100.0\um\x64\opengl32.lib`，所以缺的是**搜索路径**，不是文件。

---

## 3. 可复现的最小实验（推荐首选，不依赖 reg.exe 策略）

不要靠「碰巧有安全策略拦 reg.exe」来复现——接手方环境未必有。用一个人造的「半成功 vcvars」精确模拟，最可控，也不会触发任何被禁程序。

**步骤 1**：新建 `D:\barelib\build\tmp\t9\fake_vcvars.cmd`（CRLF）：

```bat
@echo off
set "INCLUDE=D:\barelib\src"
set "LIB=D:\barelib\bin\Debug"
set "PATH=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64;%PATH%"
```

> 故意**只给 `LIB` 一个不含 SDK `um\x64` 的目录**，模拟 reg.exe 被拦的结果。MSVC 版本号按本机实际目录调整（可从 `C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\` 下取）。夹具 `t9_main.c` / `t9_helper.c` 不含任何 `#include`，所以 `INCLUDE` 只需非空。

**步骤 2**：新建 `run_repro.cmd`（CRLF）：

```bat
@echo off
set "INCLUDE="
set "LIB="
set "VCVARS=D:\barelib\build\tmp\t9\fake_vcvars.cmd"
call "D:\barelib\compile_game.bat" D:\barelib\build\tmp\t9\t9_main.c D:\barelib\build\tmp\t9\t9_helper.c -o D:\barelib\build\tmp\t9\t9game_repro.exe build
echo BAT_EXIT=%ERRORLEVEL%
```

**步骤 3**：用 PowerShell 直接调用（本环境 `cmd /c` 被工具层策略拒绝，`&` 直接调 .cmd 可用）：

```powershell
& "D:\barelib\build\tmp\t9\run_repro.cmd"
```

**修复前预期**：`LNK1181: 无法打开输入文件"opengl32.lib"`、`Compilation failed.`、`BAT_EXIT=1`、`t9game_repro.exe` 不存在，**且没有出现** `WARNING: vcvars did not produce a usable toolchain.` / `Trying vswhere.exe...`（这就是缺陷：本该走诊断却没走）。

**修复后预期**：见 §5 验收。

### 已跑过的真实环境复验（2026-11-11 22:18，供交叉印证）

| 场景 | 命令 | 结果 |
|---|---|---|
| A 正常 vcvars 路径 | `& compile_game.bat t9_main.c t9_helper.c -o ...\t9game_verify.exe build` | **FAIL** `LNK1181 opengl32.lib`，退出码 1，无 exe；末尾工具层报 `reg.exe` 被安全黑名单拦截 |
| C 坏 VCVARS → fallback | `& run_c.cmd` | **PASS** `Fallback toolchain assembled from vswhere + SDK.` → `Compilation succeeded.`，`t9game_c.exe` 生成 |
| D vcvars 与 fallback 双失败 | `& run_d.cmd` | 诊断可读、明确报 reg.exe 被拦、无 exe（这部分实现是对的） |

只读核查：父 shell `INCLUDE`/`LIB` 均为空（场景 A 的变量全来自 `call vcvars64.bat`）；SDK 的 `um\x64\opengl32.lib` 存在；`compile_game.bat` 未被改动（mtime 15:05:25，git diff +170/−30）。

C 的 PASS 很关键：它证明 `:try_fallback` 拼出的 `LIB` 是完整可用的，所以修法只需让 `:check_toolchain` 正确地把半成功判定为失败，不需要动 fallback 本身。

---

## 4. 修复要求

### 4.1 最小补丁（只改 `:check_toolchain`，修复前第 195-201 行，现 195-208 行；已落地）

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

- 脚本已 `setlocal enabledelayedexpansion`，子串替换必须用 `!VAR:old=new!`，不要用 `%VAR:old=new%`（在括号块/子程序里展开时机不对）。
- 桌面 Windows SDK 的导入库目录固定叫 `um\x64`，`opengl32.lib`/`gdi32.lib`/`user32.lib` 都在那里，本工程链接必需。
- `if not defined LIB` 单独不够：半成功时 `LIB` **非空**但缺 SDK 项，所以必须再加 `um\x64` 子串判定。
- 若接手方有更稳的判定（例如直接验证 `cl.exe` 能解析某个必需导入库），可以替换，但判定必须在**编译之前**完成，且失败时必须落到 `:diagnose` + `:try_fallback`。

### 4.2 必须保持不变

- 纯 CRLF 行尾（改完检查 `bareLF == 0`）。
- `/MD`（第 164 行）；改成 `/MT` 会对 `raylib.lib` 报 LNK2019/LNK1120。
- 非 ASCII TEMP 修复（第 148-150 行，中文用户名下 cl 会报 D8018）。
- `:diagnose` → `:try_fallback` → 二次 `:check_toolchain` → 明确报错 `exit /b 1` 的整体顺序。
- 多源解析用 label + `goto` 逐行解析、`!SOURCES!` 延迟展开（不要改成括号块）。
- 8 个 label 的名称与调用关系。

### 4.3 顺带修正（可选，非阻塞）

- 失败路径下 `:diagnose` 被调用两次（`:check_toolchain` 之后、fallback 失败之后），输出里 `Resolved toolchain values` 出现两遍。可去重，但别把诊断信息删掉。**仍然属实**。
- ~~runner 最后一条是 `echo RUNNER_EXIT=%ERRORLEVEL%`，导致 runner 进程退出码恒为 0。~~ **此项已作废**：现 `run_c.cmd` / `run_d.cmd` 均以 `exit /b %ERRORLEVEL%` 收尾，实测 `$LASTEXITCODE=1`，打印值与进程退出码一致。见 §0 口径纠正。

---

## 5. 验收标准（硬指标，缺一不可）

**必须在「vcvars 半成功」环境跑**（§3 的人造 `fake_vcvars.cmd`，或真实 `reg.exe` 被拦的环境）。在 vcvars 完整成功的环境里跑出 PASS 不算数——这正是上一轮误判的原因。

1. `run_repro.cmd`：`BAT_EXIT=0`、`t9game_repro.exe` 生成、直接运行退出 0。
2. 输出中**必须**出现 `[compile_game] WARNING: vcvars did not produce a usable toolchain.`、`[compile_game] Resolved toolchain values:`（含 `INCLUDE`/`LIB` 实值）、`[compile_game] Trying vswhere.exe + Windows SDK fallback...`、`[compile_game] Fallback toolchain assembled from vswhere + SDK.`、`[compile_game] Compilation succeeded.`。
3. 输出中**不得**出现 `LNK1181`、`C1083` 作为唯一线索；若出现，必须同时有上面第 2 条的可读根因。
4. 回归：`run_c.cmd`（PASS，退出 0）、`run_d.cmd`（明确报错，bat 退出码 1，不产出 exe）、场景 A 多文件 + `-o` + `build`（退出 0、不出现 `Running...`）、场景 B 默认输出名 + 运行模式（出现 `Running...`，用 `t9_marker.c` 打印 `T9_RAN` 做硬证据）、场景 E 参数错误（退出 1、不调用 cl）。
5. 静态检查：8 个 label 与全部 goto/call 目标配对（0 缺失、0 未引用）；文件纯 CRLF。
6. `D:\barelib_test\game\tools\smoke.sh` 复验 PASS（编译 ok、10 关状态机全 ok、全流程连打 ENDING）。
7. `ctest -C Debug` 15/15、`ctest -C Debug -LE gpu` 12/12（bat 不在 ctest 内，但改动后应确认无连带影响）。

静态 label 检查命令：

```
python -c "import re;s=open('D:/barelib/compile_game.bat',encoding='ascii').read();L=set(re.findall(r'(?m)^:([A-Za-z_][A-Za-z0-9_]*)',s));G=set(re.findall(r'goto\s+:([A-Za-z_][A-Za-z0-9_]*)',s,re.I));C=set(re.findall(r'call\s+:([A-Za-z_][A-Za-z0-9_]*)',s,re.I));print('missing goto',G-L,'missing call',C-L,'unused',L-G-C)"
```

---

## 6. 只读边界（务必遵守）

**只改**：`compile_game.bat`。测试产物统一放 `D:\barelib\build\tmp\t9\`（已存在）。可以更新 `docs/t9-bat-e2e-result.md` 或另写结果记录。

**不要做**：
- 不要 `git commit` / `git push`（四批次累计 23 个文件改动仍未提交，提交由用户另行决定）。
- 不要改 `D:\barelib_test\game`（评测工程只读）、`docs/engine-fixes-handoff.md`（原始需求文档，未跟踪）。
- 不要改 `src/`、`tests/`、`.workbuddy/`；不要删任何目录；不要对 Desktop / Downloads / 用户目录做清理或递归删除。
- 不要新增公开 API、不要动 API 计数（当前实测 **752**）。
- 本环境工具层明确要求「Do NOT retry `reg.exe`, launch it through another shell or script, or attempt an equivalent workaround」。请用 §3 的人造半成功方案，不要试图绕过或反复触发 `reg.exe`。
- `cmd /c` 在本环境被工具层拒绝（`The use of 'cmd /c' ... is blocked on Windows for safety. Please use PowerShell instead.`）；用 PowerShell `& "xxx.cmd"` 直接调用即可，不要加 wrapper 或换 shell 硬绕。

---

## 7. 环境坑

1. **代理变量大小写重复**：本机同时有 `HTTPS_PROXY` 与 `https_proxy`，MSBuild/CL 会报 `MSB6001: 已添加项。字典中的关键字:"HTTPS_PROXY"`。从 Git Bash 启动 `cmd.exe` 时先 `env -u https_proxy -u http_proxy -u all_proxy -u HTTPS_PROXY -u HTTP_PROXY -u ALL_PROXY ...`。这与 `cmd /c` 被拒、`reg.exe` 被拦是三件不同的事，别混。
2. **Git Bash 路径转换**：从 Git Bash 调 Windows 命令加 `MSYS2_ARG_CONV_EXCL="*"`，否则路径会被改写。推荐直接用 PowerShell。
3. **中文用户名**：cl.exe 会在 TEMP 写响应文件，非 ASCII 路径会报 D8018；bat 已把 `TEMP`/`TMP` 重定向到 `D:\barelib\temp`（已存在）。别删该目录。
4. **CRT 模式**：`/MD` 对应 CMake 构建的动态 CRT `raylib.lib`。

---

## 8. 实现速查（行号基于当前版本，改后自行复核）

| 行号 | 作用 |
|---|---|
| 26-33 | 空参数 → Usage + `exit /b 1` |
| 39-82 | 参数解析（label + `goto` 逐行、`!SOURCES!`、任意位置 `build` / `-o`） |
| 84-105 | `VCVARS` 候选探测；环境变量 `VCVARS` 可覆盖 |
| 107 | `if exist "%VCVARS%" call "%VCVARS%" >nul 2>&1` |
| 109-129 | `:check_toolchain` → `:diagnose` → `:try_fallback` → 二次检查 → 报错退出 1 |
| 133-143 | `src` 与 `bin\Debug\raylib.lib` 存在性检查 |
| 145-150 | 非 ASCII TEMP 修复 |
| 164-167 | `cl.exe /nologo /W3 /utf-8 /MD /Fe:"%EXE%" %SOURCES% /I... /link /LIBPATH... raylib.lib opengl32.lib gdi32.lib winmm.lib user32.lib shell32.lib` |
| 169-187 | `if not exist "%EXE%"` 判成败；非 `build` 模式 `pushd` 后运行 |
| **195-208** | **`:check_toolchain` —— 缺陷所在，已修复**（新增 `if not defined LIB` 与 `!LIB:um\x64=!` 子串判定；201-204 为机制注释） |
| 210-221 | `:diagnose`：打印 VCVARS / INCLUDE / LIB / VSROOT / MSVCROOT / SDKVER / CL_BIN |
| 223-265 | `:try_fallback`：vswhere → `VC\Tools\MSVC\*` → SDK Include 版本 → 手拼 INCLUDE/LIB 并把 cl 目录加进 PATH |

已知设计取舍（不是 bug）：`VCVARS` 已定义但路径无效时不回退到候选探测，直接走 `:check_toolchain` → fallback；这是 `run_c.cmd` / `run_d.cmd` 能触发诊断的前提。交互式 console 的 stdin 取消属 T6，与本 bat 无关。

---

## 9. 验收结果（§5 逐项，7/7 PASS）

| # | 验收项 | 结论 |
|---|---|---|
| 1 | 半成功环境 `run_repro.cmd` 编过并运行 | **PASS**：`BAT_EXIT=0`、`RUNNER_EXIT=0`、exe 生成 |
| 2 | 必须出现的 5 段诊断 | **PASS**：外部结果文档 §3 有完整输出片段；本会话场景 A 复现同样 5 段 |
| 3 | 不得只有 `LNK1181` / `C1083` | **PASS**：修复后未出现；反事实对照（摘除补丁）才出现 `LNK1181` 且无诊断 |
| 4 | 回归 run_c / run_d / A / B / E | **PASS**：run_c 退出 0、run_d 退出 1 无 exe、A 退出 0 无 `Running...`、B 有 `Running...` + `T9_RAN`、E 退出 1/1 |
| 5 | 静态 + CRLF | **PASS**：8 label / 0 缺失 / 0 未引用；`CRLF=265 bareLF=0` |
| 6 | `smoke.sh` | **PASS**：外部因无 `bash` 标 BLOCKED，本会话用 Git Bash 补跑，`EXIT=0` + `ENDING reached` |
| 7 | 两套 ctest | **PASS**：15/15、12/12 |

**关键因果证据**：同一环境、同一 `fake_vcvars.cmd` fixture，只差 `:check_toolchain` 那 3 行 —— 有补丁走 diagnose→fallback 成功，无补丁 `LNK1181` 且跳过诊断。这排除了「环境变了所以碰巧过」的替代解释。

相关文件：
- 已修：`D:\barelib\compile_game.bat`（唯一改动文件，+7 行）
- 原始需求：`D:\barelib\docs\engine-fixes-handoff.md` §T9
- 外部修复结果：`D:\barelib\docs\t9-bat-e2e-result.md`（其第 6 项 BLOCKED 已由本会话补跑；§6 列的 4 个残留缺陷不属本次委托）
- 夹具：`D:\barelib\build\tmp\t9\` 下 `t9_main.c`、`t9_helper.c`、`t9_marker.c`、`fake_vcvars.cmd`、`run_c.cmd`、`run_d.cmd`、`run_repro.cmd`、`compile_nopatch.bat`（反事实对照副本）
- 其余遗留（不属于本次委托）：四批次累计 23 个文件改动未提交/未推送、workflow success 未查、T8 可选 demo 跳过、T3 的 12px 字体未做像素级人工对比；`-o` 不带扩展名假失败等 4 项 bat 残留缺陷见 §0。
