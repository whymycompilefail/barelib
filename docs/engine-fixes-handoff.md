# barelib 引擎修复交接文档

> 来源：`D:\barelib_test\game\docs\ENGINE_REVIEW.md`（以「从零做 10 关横版动作游戏」为任务的实测评测）。
> 本文把评测中的**引擎侧**问题转成可执行任务，供接手的 AI 直接实施。
> 评测工程本身（`D:\barelib_test`）是只读参考，**不要修改**。

---

## 0. 背景

- 引擎仓库：`D:\barelib`（raylib v6.1-dev 深度裁剪的**纯 2D** 分支 + AI Adapter）
- 平台：Windows / MSVC 2022 / C99，静态库 `raylib.lib` + 公开头 `bin/include/`
- 现状：本地 14 个 CTest 套件全绿；CI 4 个工作流（AI Adapter Tests、Build raylib CMake、Build raylib Windows 矩阵、Build examples）全绿
- 公开 API 计数（2026-11 实测）：**747 = 引擎 672 + AI 75**。任何新增/删除公开函数的改动都要同步 `README.md`、`spec.txt`、`CONTEXT.md` 里的数字

### 构建与验证命令

```powershell
cd D:\barelib
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure          # 全部（含 GPU，需显卡）
ctest --test-dir build -C Debug -LE gpu --output-on-failure  # CI 模式（托管 runner 无 GL 3.3）
```

全新构建：

```powershell
cmake -B build -G "Visual Studio 17 2022" -DBUILD_EXAMPLES=OFF
```

评测工程复验（改动后建议跑一遍真实项目）：

```powershell
cd D:\barelib_test\game
./build.sh                      # 链 D:\barelib\bin\Debug\raylib.lib
./ashen_debug.exe --level 1 --start --bot --nosound --turbo --frames 26000 --log 1000
./tools/smoke.sh                # 编译 + 10 关 verify + 抽样试玩 + 全流程连打
```

---

## 1. 实施前必须遵守的约束

1. **纯 2D**：不要为修 bug 重新引入 3D/剔除/着色器相关能力。
2. **AI Adapter 双职责**：`AIDrawXXX` 是「绘制 + 上报」一体；直接调 `DrawXxx` 不会被场景 JSON 记录。新绘制 API 要么沿用这一模式，要么明确说明不参与上报。
3. **场景元素类型是 X-macro 单一数据源**（`src/ai_scene.c` 顶部 `AI_ELEMENT_TYPES`）。新增类型必须同步：
   - `AI_ELEMENT_TYPES(X)` 一行；
   - `AI_HAS_*` 字段 flag；
   - 协议输出前的类型范围检查（当前是 `e->type > AI_DRAW_SPRITE`）；
   - 序列化 `switch` 分支；
   - `AIElementChanged()` 的字段比较；
   - `tests/test_ai_adapter.c` 断言。
4. **公开头文件必须加入 `src/CMakeLists.txt` 的 `raylib_public_headers`**，否则不会拷贝到 `bin/include`。
5. **新增公开 API 需论证**（spec P6），并同步文档计数。计数命令：

   ```powershell
   (Select-String -Path src\*.h -Pattern '^\s*(RLAPI|RMAPI)' | Measure-Object).Count
   ```

6. **测试先行**：每个任务先在对应套件里写出会失败的断言，再改实现。GPU 像素断言放 `tests/test_gpu.c`（该套件带 `gpu` 标签，CI 会跳过）。
7. **不引入第三方依赖**，除非有明确收益并说明许可（当前仅 `src/external/jsmn.h`、stb 系列）。
8. 提交信息风格：`fix:` / `feat:` + 说明「为什么」，参考 `git log --oneline -10`。

---

## 2. 任务清单

### T1 · 关闭 2D 背面剔除（P0，最高优先）

**现象**：`DrawTriangle` 只在某一绕序下可见，另一绕序被静默丢弃；评测中玩家斗篷、Boss 长袍、地刺、飞行怪全部消失，控制台无任何提示。

**根因**：`src/rlgl.c:1492-1496` 在 `rlglInit()` 里启用了背面剔除（上游 raylib 为 3D 保留），而本引擎已无 3D。

```c
glCullFace(GL_BACK);
glFrontFace(GL_CCW);
glEnable(GL_CULL_FACE);      // ← 2D 场景下纯害无益
```

**修改**：
- `src/rlgl.c`：改为 `glDisable(GL_CULL_FACE);`，保留 `glCullFace/glFrontFace` 与注释，并注明「2D-only engine: 剔除只会让绕序错误变成静默丢图」。
- 可选（推荐但不强求）：`DrawTriangle` 内部按 `cross = (v2-v1)×(v3-v1)` 自动纠正绕序，使两种顺序都稳定，不依赖全局状态。
- 更新 `src/raylib_shapes.h:112` 注释：不再要求 CCW。

**验收**：
- 在 `tests/test_gpu.c` 增加用例：`BeginTextureMode(target)` 后分别画 `cross>0`、`cross<0` 两个三角形，`EndTextureMode` 后 `LoadImageFromTexture` + `GetImageColor` 断言两处像素均为绘制颜色（不是黑）。
- 复跑评测工程 `tools/tritest.c` 的最小复现（两种绕序都应出图）。
- `ctest` 全绿。

**风险**：极低。2D 引擎不需要 culling，且不改变任何公开 API。

---

### T2 · `TakeScreenshot` 在帧内调用得到全黑（P2）

**现象**：在 `BeginDrawing()`/`EndDrawing()` 之间调用，PNG 全黑；移到 `EndDrawing()` 之后正常。

**根因**：`src/rcore.c:1019-1043` 直接 `rlReadScreenPixels()`，没有先把挂起的渲染批次提交到帧缓冲。

**修改**：
- 在 `rlReadScreenPixels()` 之前调用 `rlDrawRenderBatchActive()`（`rlgl.h` 已公开）。
- `src/raylib_core.h:775` 注释补充：「可在帧内调用；内部会先 flush 当前批次。若截图需要包含本帧全部内容，仍在 `EndDrawing()` 之后调用更直观」。

**验收**：
- `tests/test_gpu.c`：`BeginDrawing(); ClearBackground(BLACK); DrawRectangle(0,0,16,16,RED); TakeScreenshot("shot_probe.png"); EndDrawing();` 然后 `LoadImage("shot_probe.png")`，断言 `(8,8)` 为红色、`(64,64)` 为黑色；测试结束删除文件。
- 注意测试工作目录与 `CORE.Storage.basePath` 的差异，统一用相对文件名。

**风险**：低。截图语义变清晰，不改变返回值（void）。

---

### T3 · 默认字体的字号策略（P1）

**现象**：默认字体按 baseSize=16 烘焙；用 12/18px 渲染会模糊，并刷警告
`FONT: [0x25ef] Glyph height is bigger than requested font size: 19 > 16`。

**根因**：
- `src/rtext.c` 的 `LoadFontDefault()` 固定 `LoadFontFromMemory(..., 16, ...)`；
- 警告来自 `src/rtext.c:624`，在烘焙阶段对「字形光栅高度 > 字号」的合法情况也打 WARNING。

**修改（建议方案）**：
1. 新增公开 API `RLAPI Font LoadFontEmbedded(int fontSize);`（`src/raylib_text.h` + `rtext.c`）：
   - 复用现有 Unicode 区间表（把 `LoadFontDefault()` 里的 `fontRanges[]` 抽成共享静态表）；
   - 从 `cjkFontData` 按**请求字号**烘焙，`LoadFontDefault()` 改为 `LoadFontEmbedded(16)`；
   - 文档注明内存代价：字号翻倍，图集面积约 4 倍（CJK 2 万字形，32px 需实测并写入文档）。
2. `rtext.c:624` 的警告降级为 `LOG_DEBUG`（或只在 `cpHeight > fontSize*1.5` 时告警），避免合法字形刷屏。
3. 同步 API 计数 747 → 748。

**验收**：
- `tests/test_text.c` 增加：`LoadFontEmbedded(12/16/32)` 均返回有效字体、`glyphCount > 0`、文字测量宽度单调递增；卸载不崩。
- 用评测工程复验：以 12px 画 HUD 不再刷 Glyph height 警告，且字明显更清晰（可用 `--shot` + `tools/pngview.py` 人工确认）。

**风险**：中。图集内存增长，必须量化后再定是否缓存多档；不要默认加载多档。

---

### T4 · 精灵层：实例、锚点、闪白、缩小（P1，工作量最大）

**现象（评测接入真美术后集中暴露）**：
1. 同屏同类敌人共用一份 `AISprite` = 共用一份时钟，整队同步踏步；只能靠 `AISpriteSeek()` 强设相位绕过。
2. 只有左上角锚点，每个调用方手算 `sy = pos.y + h - canvasHeight*scale`。
3. tint 是乘法，暗色精灵无法「受击闪白」；叠加再画一次会重复注册 `AI_DRAW_SPRITE`，污染场景 JSON。
4. `scale < 1` 被夹到 1，没有缩小路径。

**修改**：
- `src/raylib_ai_sprite.h`：
  ```c
  typedef enum AISpriteAnchor { AI_SPRITE_TOPLEFT = 0, AI_SPRITE_CENTER, AI_SPRITE_BOTTOM_CENTER } AISpriteAnchor;
  #define AI_SPRITE_FX_FLASH      (1 << 0)   // additive white overlay
  #define AI_SPRITE_FX_TINT_ADD   (1 << 1)   // additive tint instead of multiply

  RLAPI AISprite *AISpriteCloneState(const AISprite *src);  // 共享纹理，独立播放状态
  RLAPI void AISpriteDrawEx(AISprite *s, float x, float y, float scale,
                            AISpriteAnchor anchor, bool flipX, Color tint, int fx);
  ```
  `AISprite` 增加 `bool ownsTexture;`（`AILoadSpriteSheet` 置 true，clone 置 false）。
- `src/ai_sprite.c`：
  - `AIUnloadSpriteSheet()`：仅 `ownsTexture` 时 `UnloadTexture`；clone 只释放结构体。
  - `AISpriteCloneState()`：复制 frames/tags/canvas/sheet/image 路径与当前播放位置，`texture` 句柄共享。
  - 锚点换算：`TOPLEFT(0,0)`、`CENTER(-w/2,-h/2)`、`BOTTOM_CENTER(-w/2,-h)`，其中 `w/h = source.width/height * scale`。
  - `AI_SPRITE_FX_FLASH`：正常画一次后，用 `BeginBlendMode(BLEND_ADDITIVE)` 再画一次白色（alpha 可用 tint.a 控制），**只注册一次** `AI_DRAW_SPRITE`。
  - `scale` 允许 `< 1`，最小 0.01；保留 POINT 过滤，不做 mipmap。
  - 旧 `AISpriteDraw(s, x, y, int scale, bool flipX, Color tint)` 保留为兼容包装（内部转 `AISpriteDrawEx(..., TOPLEFT, ..., 0)`）。
- `src/raylib_ai_adapter.h` + `src/ai_scene.c`：
  - `AIDrawElement` 增加 `int anchor; int fx;`；
  - 场景 JSON 的 sprite 元素增加 `"anchor": "bottom_center"`、`"fx": ["flash"]`（或 `"fx": 1`）；
  - `AIElementChanged` 比较新字段；
  - 更新 `tests/test_ai_adapter.c::TestSpriteSceneElement`。

**验收**：
- `tests/test_sprite.c`（纯 CPU）：
  - clone 后推进 clone 的时钟，原实例帧号不变；反之亦然；
  - `AIUnloadSpriteSheet(clone)` 后原实例仍可用（`texture.id` 非 0）。
- `tests/test_gpu.c`：
  - 锚点：16×16 帧、scale=2、画在 `(64,64)`，断言 `BOTTOM_CENTER` 时脚部像素落在 `y≈128`、`CENTER` 时中心落在 `(64,64)`；
  - 闪白：深色精灵加 `FLASH` 后同位置像素亮度显著上升；
  - 场景 JSON 仍只出现一个 sprite 元素。
- 文档：`docs/ai_adapter.md` 精灵章节补 `CloneState` / `DrawEx` / anchor / fx 示例；README 特性行同步；API 计数 747 → 750（Clone + DrawEx + 旧 API 不变）。

**风险**：中。注意纹理所有权与 `AISprite` 结构体体积（当前约 10KB/实例，clone 会按实例数增长，可接受但要说明）。

---

### T5 · `pause` + `step` 文档纠错（P2，只改文档）

**问题**：`docs/ai_adapter.md` 写 `AIConsumeStepFrame` 由 runtime 调用；实际必须**宿主每帧调用一次**，且 `step N` 是摊到 N 个真实帧，不是瞬间推进。

**修改**：
- 把措辞改成 "host must call `AIConsumeStepFrame()` once per frame"；
- 补最小宿主循环示例：
  ```c
  while (!WindowShouldClose()) {
      AIConsumeStepFrame(adapter);   // 消费一帧 step
      AIConsumeInput(adapter);
      UpdateGame();
      BeginDrawing(); DrawScene(); EndDrawing();
      AIUpdate(adapter);
  }
  ```
- 在 `raylib_ai_adapter.h` 的 `AIConsumeStepFrame` 注释同步一句。

**验收**：文档自洽；`tests/probe.py`（评测工程）行为不变。

---

### T6 · `AIClose` 延后释放（P2）

**现状**：`src/raylib_ai_adapter.c:224-258` 先等 stdin 读线程 500ms；线程仍阻塞在 `fgets` 时 detach，并置 `deferFree = true`，退出时打印
`AIADAPTER: adapter memory deferred to process exit (stdin thread active)`（`:357-369`）。

**可选修改**（按性价比从高到低）：
1. 用 `CancelSynchronousIo(adapter->commandThread)` 取消阻塞中的 `ReadFile`（`fgets` 底层），再 `WaitForSingleObject` + `CloseHandle` + 正常释放；失败仍回退到当前 detach 路径。
2. 或者把 stdin 读取改为 overlapped `ReadFile` + 事件，使线程可被可靠唤醒。
3. 至少：在 `AIClose` 文档里写明「交互式 stdin 下适配器内存延后到进程退出；要彻底释放请在 EOF（管道关闭）后退出」。

**验收**：
- `tests/test_ai_adapter.c` 增加 100 次 `AIInit`/`AIClose` 循环不崩、日志不无限增长；
- 管道输入 `echo quit | exe` 时不出现 defer 警告；
- 交互式场景允许仍出现警告，但行为有文档说明。

**风险**：中。多线程生命周期改动容易引入 use-after-free，务必保留现有「确认线程退出后才删 mutex/释放」的顺序。

---

### T7 · 场景溢出进 `errors`（P3，先验证再决定）

**注意**：`src/ai_scene.c:137-146` **已经**在溢出时调用
`AI_SET_ERROR(adapter, AI_ERR_BUFFER_FULL, ...)` 并计数 `elementOverflowCount`。
评测说的「溢出只有字段没有错误」可能已过时。

**先做**：用 >1024 个元素跑一遍 `scene` + `errors`，确认 `AI_ERR_BUFFER_FULL` 是否出现在 errors JSON。
- 若已出现：本任务关闭，只在 `state` 里补 `elements_dropped`（可选）。
- 若未出现：查 `AI_SET_ERROR` 的可见性/清除时机，修正后补测试。

**验收**：`tests/test_ai_adapter.c` 填满 1024 元素后再加 1 个，断言场景 JSON `overflow == 1` 且 `errors` 含 `AI_ERR_BUFFER_FULL`。

---

### T8 · 2D 渐变矩形（P2，能力补充）

**现象**：`raylib_shapes.h` 无 `DrawRectangleGradientV/H`，天空/暗角/雾带只能拼色带。

**修改**：
- `src/rshapes.c` + `src/raylib_shapes.h`：
  ```c
  RLAPI void DrawRectangleGradientV(int posX, int posY, int width, int height, Color top, Color bottom);
  RLAPI void DrawRectangleGradientH(int posX, int posY, int width, int height, Color left, Color right);
  ```
  实现用 `rlBegin(RL_QUADS)` + 逐顶点颜色 + `rlSetTexture(GetShapesTexture().id)`（参考同文件现有 QUADS 函数）。
- API 计数 +2。
- `tests/test_gpu.c`：画 V 渐变后断言顶端/底端像素分别接近 top/bottom。

**验收**：像素断言通过；`examples/ai_shapes_demo.c` 可选加一段演示。

---

### T9 · `compile_game.bat` 工具链健壮性（P1，体验问题）

**现象**：`call vcvars64.bat` 内部调用 `reg.exe`，受限环境被拦 → `INCLUDE` 为空 → cl 报
`fatal error C1083: 无法打开 "stdio.h"`，根因与 CRuntime 无关，排查成本极高。另外一次只能编一个 `.c`。

**修改**（`D:\barelib\compile_game.bat`）：
1. `call vcvars64.bat` 之后检查 `if not defined INCLUDE` / `where cl`：
   - 失败时打印**已解析到的** `VCVARS`、`INCLUDE`、`LIB`；
   - 尝试 `vswhere.exe`（`%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe`）定位 `VC\Tools\MSVC\*\bin\Hostx64\x64` 与 Windows SDK，手工拼 `INCLUDE`/`LIB`；
   - 仍失败则给出明确错误：「vcvars 不可用（可能是 reg.exe 被安全策略拦截），请用 build.sh / cmake」。
2. 支持多源文件：`compile_game.bat a.c b.c c.c [build]`，输出名默认取第一个源文件；或新增 `-o name.exe`。
3. 保持现有非 ASCII `TEMP` 修复与 `/MD` 说明。

**验收**：两个 `.c` 的多文件工程一条命令编过；人为改错 `VCVARS` 路径时输出可读错误而非 `stdio.h` 缺失。

---

## 3. 建议执行顺序

| 批次 | 任务 | 说明 |
|---|---|---|
| 1 | T1 + T2 + T5 | 改动小、风险低，先消除两个「静默失败」 |
| 2 | T4 | 独立模块，测试可控，收益最大 |
| 3 | T3 + T8 | 各自独立，可并行 |
| 4 | T6 + T9 + T7 验证 | 工具链/生命周期，注意回归 |

每批次完成后：
1. `ctest -C Debug` 全绿（含 GPU）；
2. `ctest -C Debug -LE gpu` 全绿（对齐 CI）；
3. `D:\barelib_test\game\tools\smoke.sh` PASS；
4. 同步文档与 API 计数；
5. 单独提交（一个任务一个 commit 或一个批次一个 commit）。

推送后确认 4 个 workflow 全部 success：

```powershell
gh run list --repo whymycompilefail/barelib --limit 8
```

---

## 4. 明确不在本轮范围

- 瓦片地图 / 精灵批渲染（draw call 优化）
- 文本自动换行与测量缓存
- AI 采样音频 / pan / pitch 混音
- 2D 光照 / 遮罩
- 骨骼 + 部件式大体积角色（评测已论证整图集不适合 86×96 的 Boss）

这些属于新能力，另立 spec，不要塞进本次修复。

---

## 5. 参考位置速查

| 内容 | 位置 |
|---|---|
| 评测原文 | `D:\barelib_test\game\docs\ENGINE_REVIEW.md` |
| 评测工程待办 | `D:\barelib_test\game\docs\TODO.md` |
| 绕序最小复现 | `D:\barelib_test\game\tools\tritest.c` |
| 图元语义探针 | `D:\barelib_test\game\tools\primprobe.c` |
| 截图逐像素核对 | `D:\barelib_test\game\tools\png_probe.py` |
| 精灵实机用法 | `D:\barelib_test\game\src\render.c`（`AISpriteSeek` 相位写法） |
| 引擎测试 | `D:\barelib\tests\`（`test_sprite.c` / `test_ai_adapter.c` / `test_gpu.c`） |
| 场景元素 X-macro | `D:\barelib\src\ai_scene.c` 顶部 |
| 精灵实现 | `D:\barelib\src\ai_sprite.c` / `raylib_ai_sprite.h` |
