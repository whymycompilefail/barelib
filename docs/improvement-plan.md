# barelib 提交前改进清单

> 审查基线：本地工作区（对应 GitHub `cbf14af7`，外加 8/13 的 `ai_protocol.c` 修复）。
> 本清单只列**已核实**的问题；每一项都写明改动位置与验收方式。
> Android 支持已决定下线，见阶段 A。

---

## 总览

| 阶段 | 内容 | 目标 |
|---|---|---|
| A | 移除 Android 支持 | 平台范围与 spec 一致：仅 Windows 桌面 |
| B | P0 崩溃 / 核心功能失效 | 不再有"发个命令就崩""相机渲染错"的问题 |
| C | P1 AI 可观测能力补齐 | 文本/旋转等变化对 AI 可见，JSON 始终合法 |
| D | P2 健壮性与性能 | 线程、回调、字体渲染、图元细节 |
| E | 回归测试补齐 | 每个 P0/P1 修复都有对应测试，防止回退 |
| F | 文档口径统一 + 提交 | 数字一致、Android 表述清除、正式推送到自己的 fork |

验收总闸门：`cmake --build build --config Debug` 通过，`ctest -C Debug` 全绿（现为 13/13，新增测试后保持 100%）。

---

## A. 移除 Android 支持

原则：只删"Android 专用路径"，不删跨平台公共代码（`raylib_ai_input_stubs.c` 等非 Windows 桩保留，因为 AI 协议测试依赖它）。

- [x] **A1** 删除平台实现文件
  - `src/platform_android.c`
- [x] **A2** 清理构建系统
  - `src/CMakeLists.txt`：L63–64 的 `Android` 分支、L120 的 `Android` 条件
  - `CMakeOptions.txt` L5：`enum_option(PLATFORM "Desktop;Win32;Android" ...)` → 去掉 `Android`
  - 确认 `build.zig` / `build.zig.zon` 是否仍被使用；若纯属上游遗留，整体删除（本项目只走 CMake + MSVC）
- [x] **A3** 清理核心代码中的 Android 分支
  - `src/rcore.c`：L32 注释、L349–350 平台日志分支、L1086–1095 `__android_log_vprint` 分支
- [x] **A4** 删除 Android 工程与 CI
  - `.github/workflows/build_android.yml`
  - `projects/VS2019-Android/`（整个目录）
- [x] **A5** 修正文档表述
  - `README.md`：features 中 "Android is a supported target" → 仅 Windows
  - `spec.txt` §4.1 平台范围、§8 N3 平台要求
  - `CONTEXT.md` 中 `platform_android.c` 的保留说明
  - `docs/ai_adapter.md` 各头文件 "Multiplatform: ... Android ..." 注释统一改为 Windows-only
- [x] **A6** 保留不动（避免破坏 API）
  - `raylib_core.h` 的 `KEY_BACK/KEY_MENU/KEY_VOLUME_UP/KEY_VOLUME_DOWN` 枚举值保留，仅改注释为"保留值，不依赖 Android"

验收：**已完成（2026-10-30）**。全仓库 Android 引用仅剩 CHANGELOG 历史记录、第三方库（miniaudio/glad/glfw）与 Web 遗留 HTML 注释；Windows 构建通过，`ctest -C Debug` 13/13 通过。

---

## B. P0 — 崩溃 / 核心功能失效

- [x] **B1 未初始化音频导致崩溃（最高优先级）**
  - 位置：`src/raudio.c` L390–392（`AUDIO` 宏解引用 `audioState`）、L547–550（`IsAudioDeviceReady`）
  - 触发：游戏没调 `InitAudioDevice`，AI 发 `state`（默认快照会调用 `IsAudioDeviceReady`）→ 空指针
  - 修法：`IsAudioDeviceReady`/`GetMasterVolume`/`CloseAudioDevice` 等入口先判 `GetRaylibContext()->audioState == NULL`；空时返回安全值（false / 0.0f / 直接返回），不要分配隐式设备
  - 验收：新增测试 T1

- [x] **B2 `rlMatrixMultiply` 与 raymath / 上游不一致**
  - 位置：`src/rlgl.c` L4275–4299
  - 问题：索引用法实际算反序乘积，注释"matches raymath"不成立；影响 `rlTranslatef/rlRotatef/rlScalef/rlMultMatrixf/matMVP`，`BeginMode2D` 在非单位相机下变换错误
  - 修法：改回 `raymath.h` L1505–1520 的公式（`left.m0*right.m0 + left.m1*right.m4 + ...`），并更新注释
  - 验收：新增测试 T2（像素级相机断言，不再只看"非空像素"）

- [x] **B3 AI 程序化音频暂停立刻被回收**
  - 位置：`src/ai_audio.c` L253–265
  - 问题：`IsSoundPlaying()` 对暂停缓冲返回 false（`raudio.c` L2709–2713），下一帧 `AIAudioUpdate` 直接卸载；`AIPauseAudio`/`AIResumeAudio` 实际不可用
  - 修法：slot 增加 `paused` 状态；回收条件改为"未暂停且不再播放"；暂停/恢复同步更新该状态
  - 验收：新增测试 T4（无音频设备时至少验证状态机逻辑；有设备时验证 resume 可播）

- [x] **B4 大场景 JSON 的结束标记仍会被截断**
  - 位置：`src/ai_protocol.c` L85–105（64KB 缓冲），配合 L147–151 `scene` 命令
  - 问题：场景 JSON 本身可占满 64KB，再拼 `<<AI_SCENE_BEGIN/END>>` 就溢出，AI 端解析流会卡住；8/13 修复只把缓冲放大，没有解决"JSON + 标记"共用同一容量
  - 修法：`scene`/`state`/`diff`/`errors` 输出改为"标记单独一行输出 + JSON 单独输出"（或缓冲按 `MAX_JSON_STRING + 64` 分配并单独校验）
  - 验收：新增测试 T5（1024 元素满载时标记完整）

- [x] **B5 `AIGetSceneJSON` 丢字段（rotation / text 细节）**
  - 位置：`src/ai_scene.c` L230–234（rect 无 rotation）、L289–295（texture 无 rotation）
  - 修法：与 `AIDrawElement` 实际记录字段对齐输出
  - 验收：并入 T3

---

## C. P1 — AI 可观测能力补齐

- [x] **C1 diff 看不到文本变化**
  - 位置：`src/ai_scene.c` L740–753（只比 type/x/y/w/h/radius/color）
  - 影响：分数 100→101、状态文案变化对 AI 不可见，快照/回归测试形同虚设
  - 修法：diff 增加 `text`、`fontSize`、`rotation`、`sides`、`source` 比较；输出里带 old/new 文本
  - 验收：新增测试 T3

- [x] **C2 X-macro 的 `field_flags` 是死代码，注释与实际不符**
  - 位置：`src/ai_scene.c` L39–55
  - 问题：声明"驱动序列化和 diff"，但序列化与 diff 都是手写 switch，正是 C1/B5 漂移的根因
  - 修法（已采用）：`AIElementFieldFlags` 由 X-macro 生成，`AIElementChanged` 按 flags 决定比较 `text/fontSize/rotation/sides/source/points`；序列化仍按类型显式输出（各类型键布局不同），注释已改为准确描述
  - 验收：新增字段只需改 X-macro 一处，且测试覆盖

- [x] **C3 JSON 字符串未转义**
  - 位置：`src/raylib_ai_adapter.c` L757–775（窗口标题）、L875–877（手柄名）
  - 修法：统一走 `AIEscapeJSON`；同时对标题长度做截断保护
  - 验收：新增测试：标题含 `"`、`\`、换行时 `json.loads` 成功

- [x] **C4 纹理注册表溢出逻辑写错**
  - 位置：`src/ai_scene.c` L103–108
  - 问题：满 256 条后把 `count` 清零，等于清空全部旧映射（注释称"覆盖最旧"）；没有 `AIUnloadTexture`，id 复用会串路径
  - 修法：引入写指针做真正环形覆盖；提供 `AIUnloadTexture`（或在 `UnloadTexture` 场景由游戏调用）移除条目
  - 验收：单元测试：注册 257 个纹理后，第 257 个可查、被覆盖的那个返回 `unknown`

- [x] **C5 PostMessage 模式的 click 自我抵消**
  - 位置：`src/ai_input.c` L185–209
  - 问题：down+up 同批投递，和 `press` 的"一帧延迟 key-up"补救不一致，后台窗口点击可能检测不到
  - 修法：复用 `pendingKeyUp` 思路，增加 `pendingMouseUp`（按钮 + 一帧延迟）
  - 验收：`ai_tester_session.py` 增加后台点击用例

- [x] **C6 `type` 无法输入非 ASCII**
  - 位置：`src/ai_input.c` L231–257
  - 问题：非 ASCII 字符被当成 keycode 注入，中文输入无效
  - 修法：非 ASCII 走 `WM_CHAR` 投递路径（PostMessage 模式），或明确文档化"仅支持 ASCII 输入"
  - 验收：至少文档口径一致；有条件则补一条中文输入用例

---

## D. P2 — 健壮性与性能

- [x] **D1 TraceLog 回调覆盖用户回调**
  - 位置：`src/raylib_ai_adapter.c` L196–204
  - 问题：安装时假定"之前没有回调"，把 `aiSavedLogCallback` 硬置 NULL
  - 修法（已完成）：`GetCurrentTraceLogCallback()`（内部接口，非 RLAPI）读取当前回调，`aiSavedLogCallback` 不再硬置 NULL
- [x] **D2 日志缓冲跨线程无锁**
  - 位置：`src/raylib_ai_adapter.c` L83–123
  - 问题：音频线程也会 `TraceLog`，与主线程 `AIGetErrorsJSON` 并发读写 `logBuffer`
  - 修法（已完成）：新增 `logMutex`，`AIWriteLogEntry` / `AIGetErrorsJSON` / `AIClearLogs` / `AIGetLogAt` 全部加锁；`AIClose` 在非延迟释放时销毁
- [x] **D3 `AIGetLogs` 返回物理环形缓冲**
  - 位置：`src/raylib_ai_adapter.c` L536–542
  - 修法（已完成）：新增 `AIGetLogAt(adapter, logicalIndex)`（RLAPI），旧 `AIGetLogs` 保留并在注释中说明物理顺序限制
- [x] **D4 CJK 默认字体字形查找线性扫描**
  - 位置：`src/rtext.c` L1383–1409（结合 L165–209 的 23700 字形加载）
  - 修法（已完成）：ASCII 快路径——先按 `codepoint-32` 尝试，校验 `glyphs[i].value` 后再返回，未命中回落线性扫描
- [x] **D5 圆角矩形线框内半径为负**
  - 位置：`src/rshapes.c` L718
  - 修法（已完成）：`rIn = (radius > lineThick) ? radius - lineThick : 0`
- [x] **D6 重写的圆角函数丢纹理绑定 / UV**
  - 位置：`src/rshapes.c` L619–690、L694–800
  - 修法（已完成）：两个函数先 `rlSetTexture(GetShapesTexture().id)`，并计算 shapes 矩形中心的 UV，所有顶点统一使用该 UV（圆角图形为纯色，单点采样正确）；自定义 `SetShapesTexture` 图集因此生效，也不再受调用方遗留纹理影响
- [x] **D7 `GetRandomValue` 极值下可能死循环（低）**
  - 位置：`src/rcore.c` L982–1009；仅 `SUPPORT_RPRAND_GENERATOR=0` 构建触发（默认 =1，不受影响）
  - 修法（已完成）：范围超过 `RAND_MAX+1` 时直接返回 `min`，不再进入必然死循环的拒绝采样
- [x] **D8 所有 JSON getter 共用同一缓冲**
  - 位置：`src/raylib_ai_adapter.c` / `ai_scene.c` 的 `jsonBuffer` / `diffBuffer`
  - 修法（已完成）：`AIGetSceneJSON` 声明处补充生命周期说明，明确共享缓冲、下次任意 JSON getter 后失效

---

## E. 回归测试补齐

- [x] **T1 无音频设备的 state 查询**（对应 B1）
  - 在 `tests/test_ai_adapter.c`：不初始化音频，直接调用 `IsAudioDeviceReady` / `GetMasterVolume` / `SetMasterVolume`，断言返回安全值且不崩溃（覆盖 `AIDefaultGetStateSnapshot` 崩溃路径的根因）
- [x] **T2 Camera2D 像素级正确性**（对应 B2）
  - 在 `tests/test_gpu.c`：固定 camera（target/offset/zoom/rotation），渲染已知矩形到 RenderTexture，断言"目标像素着色 + 应空白处未着色"
- [x] **T3 文本 / 旋转 diff**（对应 B5、C1，已随 C1 完成）
- [x] **T4 暂停音频不被回收**（本机真实音频设备验证通过；无设备环境自动跳过）
- [x] **T5 满载场景 JSON 标记完整**（对应 B4）
  - 在 `tests/test_ai_adapter.c`：用 `AIAddElement` 填满 1024 个长文本元素，执行 `scene` 命令，断言输出 >60KB 且 `<<AI_SCENE_END>>` 完整（原缺陷会把结束标记截断）
- [x] **T6 JSON 转义**（对应 C3）
  - 窗口标题含 `"`、`\`、换行（通过 mock deps 注入），断言输出中对应字符已转义（`\\\"`、`\\\\`、`\\n`）

---

## F. 文档口径与提交

- [x] **F1 文档数字统一**
  - API 数量：`README.md`（732）、`CONTEXT.md`（732）、`spec.txt`（701/706）→ 以一次实测脚本统计为准，全量替换
  - README "13 CTest 套件" 与实际 `tests/CMakeLists.txt` 对齐
- [x] **F2 仓库卫生**
  - `src/cjk_font_data.c`（81.3MB 生成物）：加入 `.gitignore`，文档写明由 `scripts/prepare_cjk_font.py` 生成；确认 GitHub 仓库如何处理历史大文件（必要时后续单独处理，不在本轮强制改写历史）
  - 删除已入库的 `_test_compat/`、根目录 `*.obj`、`bin/Debug/*.obj`、`*.lib.bak`、`temp/` 旧报告、`tests/__pycache__/`
  - `examples/json具体内容.txt` 等临时文件清理
- [x] **F3 本地仓库接线到自己的 fork**
  - `origin` → `upstream`（raylib），新增 `origin` = `whymycompilefail/barelib`
  - fetch 后把本地分支对齐 `cbf14af7`（先备份本地 `ai_protocol.c`，因为它在远端之外）
  - 推送本轮修复（建议按 A/B/C/D/E/F 分 5–6 个提交，而不是一个大 commit）
- [x] **F4 提交信息与验收记录**
  - 每个提交附带对应测试项编号（如 `fix(audio): null audioState (B1, T1)`）
  - 提交前记录一份 `ctest` 输出作为基线

---

## 本轮明确不做

- 不重构引擎架构、不做新的绘制族自动捕获（lines/poly/texture 另立项）
- 不做网络化多 agent（与 spec §4.2 冲突）
- 不引入新依赖，不改公开 API 语义（只补字段、修 bug）
- 不改写 git 历史（81MB 字体文件的瘦身另案处理）
- 不做 rtext 字符串族 / 文件系统 OS 级 API 的 M4 第三阶段精简（等本清单验收后单独排期）

---

## 执行顺序建议

1. **A（移除 Android）**：独立、纯删除，先做，后续所有验证都只针对 Windows
2. **B1 + T1**：最高收益，先让 AI 测试循环不再崩
3. **B2 + T2**：相机正确性，避免示例"看起来能画但实际错位"
4. **B3/B4/B5 + T3/T4/T5**
5. **C1/C2/C3/C4**：AI 可观测能力补齐
6. **D 组 + E 剩余测试**
7. **F**：文档、卫生、接线、提交

每一步完成后跑：`cmake --build build --config Debug` + `ctest -C Debug`，保持 100% 通过率再进下一步。
