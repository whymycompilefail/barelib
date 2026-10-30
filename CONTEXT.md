# raylib 魔改版 — 领域术语表

## 项目定位
基于 raylib 深度定制的 2D 游戏引擎，移除 3D 模块，原生适配 AI agent 开发场景。

## 核心概念

### API 层次
- **高层 API（raylib_*）**：面向游戏开发者的直接接口，包括窗口、输入、绘制、音频、文本等
- **底层 API（rlgl）**：OpenGL 抽象层，作为底层 API 公开导出，供需要直接控制渲染的高级用户使用；AI 适配层通过 raylib 高层 API 间接使用，不直接调用 rlgl
- **数学 API（raymath）**：向量/矩阵运算，其中 Vector2 是 2D 开发核心，Vector3/4/Quaternion 偏 3D
- **AI 适配层**：场景元素追踪（**M4 精简后：每 `AIDrawElementType` 一个包装函数（16 个 AIDrawXXX，标量超集参数），`AI_ELEMENT_TYPES` X-macro 清单统一枚举/JSON 类型名/序列化/diff**；`AIRegisterElement` 手动注册；**engine draw observer（M3 非侵入，覆盖 rect/circle/text 收敛点 + 深化图元 rect_rounded/circle_sector，≥360° 扇形兜底内联不双重报告）**；**单 adapter 前提（候选 2）：`ctx->aiActiveAdapter` 显式指针服务 AIDrawXXX 包装（第一个注册者），注册数组保留供多实例内存隔离测试，第二个实例注册时警告一次**）、命令循环（stdin 驱动 version/scene/input/inputmode/pause/resume/step/state/errors/quiet/loud/elements/clear/record/playback/snapshot/diff/help/quit 等命令）、输入注入（AIInjectInput，SendInput/PostMessage 双模式）、录制/回放系统、场景快照与差异比较、后端状态查询（窗口/输入/时间/音频）、程序化音频生成（AIPlayTone/AIPlayNoise + 5 种波形 + 淡入淡出 + handle 控制；**噪声经自含 LCG 按 seed 可复现（候选 6），duration 钳制到 AI_MAX_AUDIO_SAMPLES≈22.7s 防无界分配**；**Sound 静态缓冲实现**——每次调用 PCM → LoadSoundFromWave → PlaySound，播完即止；早期 looping 流式实现会卡死音频线程，已根治）、依赖注入（AIDeps 用于无窗口测试；**协议命令输出经 deps->emitProtocol 注入，NULL 回退 stdout；quit 命令经 deps->requestClose，不直写 CORE**）、stdout 协议输出；**M4 拆编译单元**：门面 `raylib_ai_adapter.c`（Init/Close/Update/日志/状态/捕获/循环控制）+ `ai_scene.c`（元素/JSON/diff）+ `ai_audio.c`（程序化音频）+ `ai_input.c`（注入/键名/回放）+ `ai_protocol.c`（命令循环），共享类型入 `ai_adapter_internal.h`

### 函数变体模式
- **基础函数**：如 `DrawRectangle`、`DrawText`、`LoadFont`
- **Ex 变体**：增加控制参数，如 `DrawTextEx`（支持 Font 和 spacing）
- **Pro 变体**：最完整的控制，如 `DrawTextPro`（支持旋转、原点）
- **V 变体**：用 Vector2 替代 x/y 参数，如 `DrawRectangleV`
- **Rec 变体**：用 Rectangle 结构体替代 x/y/w/h，如 `DrawRectangleRec`
- **Lines 变体**：只绘制轮廓，如 `DrawRectangleLines`

### 模块职责
- **rcore**：窗口管理、输入、文件系统、时序、系统功能
- **rlgl**：OpenGL 状态管理、着色器、纹理上传、渲染批处理（底层公开 API，供高级用户直接使用）
- **raymath**：Vector2/3/4、Matrix、Quaternion 数学运算
- **rshapes**：2D 图元绘制、碰撞检测
- **rtextures**：图像加载/保存/处理、纹理上传/绘制
- **rtext**：字体加载、文本绘制、字符串处理、Codepoint 编码
- **raudio**：音频设备、Sound/Wave/Music/AudioStream 管理
- **AI Adapter**：场景元素追踪（AIDrawXXX 包装函数自动注册 + AIRegisterElement 手动注册）、命令循环（stdin 线程读取 → 命令队列 → AIUpdate 处理，支持 version/scene/input/inputmode/pause/resume/step/state/errors/quiet/loud/elements/clear/record/playback/snapshot/diff/help/quit）、协议版本化（AI_PROTOCOL_VERSION + 场景 JSON `protocol` 字段 + version 命令）、结构化错误模型（AIErrorCode 枚举 + `<<AI_ERROR>> [code:N]` 输出 + AIGetLastError 查询最后错误）、容量显式溢出（MAX_AI_ELEMENTS 可配置 + 场景 JSON `overflow` 字段）、录制/回放系统（记录输入动作+帧号，按帧重放）、场景快照与差异比较（AISaveSnapshot/AIGetSnapshotDiff，比较 type/pos/size/radius/color 变化）、游戏循环控制（AIPauseGame/AIResumeGame/AIStepFrame）、输入注入（AIInjectInput，支持 press/click/move/scroll/type）、后端状态查询（AIGetWindowStateJSON/AIGetInputStateJSON/AIGetTimeStateJSON/**AIGetAudioStateJSON 全部经 AIStateSnapshot 纯格式化**——含 audio 设备/缓冲列表，候选 3 完成；快照由 deps->getStateSnapshot 注入，默认实现直读 CORE 与 raudio iterator）、程序化音频生成（AIPlayTone/AIPlayNoise + 5 种波形 + 淡入淡出 + handle 控制）、依赖注入（AIDeps 用于无窗口测试）、stdout 协议（<<AI_SCENE_BEGIN>>/<<AI_DIFF_BEGIN>> 等标记）

## API 精简状态（当前总计 733 个 RLAPI/RMAPI 函数：引擎 669 + AI 层 64，2026-10 实测）

| 模块 | 函数数 | 精简内容 |
|------|--------|----------|
| raylib_core.h | 165 | 移除 VR、自动化事件、哈希/编码、自定义帧控制、手势等冷门功能 |
| raylib_shapes.h | 48 | 移除样条线、渐变变体；候选 6 补回圆角矩形/扇形（DrawRectangleRounded/Lines、DrawCircleSector/Lines） |
| raylib_textures.h | 87 | 移除程序化图像生成、卷积、抖动、CubeMap、NPatch，合并 ImageDraw 变体 |
| raylib_text.h | 48 | 移除 Alloc 变体、命名转换、冷门字符串操作函数 |
| raylib_audio.h | 53 | 保留核心音频功能 |
| rlgl.h | 148 | 保留为底层公开 API（供高级用户直接使用） |
| raymath.h | 120 | 保留 Vector2/3/4/Matrix/Quaternion 类型与常用运算（3D 专用运算精简） |

## 已移除/内部化的功能
- **3D 模块**：rmodels 完全移除
- **Web / Android 后端**：platform_web.c 于 M4 清除；platform_android.c 于提交前清理（平台范围收敛为 Windows 桌面）

## 渲染管线修复（diagnosing-bugs，2026-08）
默认 framebuffer 顶点渲染全黑 + 音乐示例只显示背景的根因是 4 个叠加 bug（都在 rlgl/rcore/rshapes 魔改中引入）：
1. **rlgl.h 丢失 24 个 RLAPI 声明**（rlOrtho/rlVertex2f/rlBegin/rlMatrixMode 等被移到 rlgl_internal.h，但 rcore/rshapes 只 include rlgl.h）→ 隐式声明 → float/double ABI 错位 → rlOrtho 收到 0 → **NaN 投影矩阵**（候选 4 后 rlgl_internal.h 已删除：3 个渲染类型移入 rlgl.h，函数声明本就在 rlgl.h，双链消失）
2. **rlMatrixMultiply 用行优先公式**（raylib Matrix 是列优先）→ MVP 计算错误
3. **rlDrawRenderBatch 把 raylib 枚举直接当 GL 常量**（RL_TRIANGLES=1 ≠ GL_TRIANGLES=4）→ 三角形画成线
4. **texShapes 使用 CJK 默认字体的 recs[95]**（原版标准字体 127 位是白块，CJK 字体不是）→ 所有 shapes 绘制采样空白 texel → 黑（已改为 rlgl 默认 1x1 白色纹理）
修复后 test_gpu 98/98、示例柱子/圆环/文本正常渲染。
- **VR 渲染**：从 rcore 移除
- **自动化事件系统**：从 rcore 移除
- **自定义帧控制**：从 rcore 移除
- **手势识别**：精简
- **样条线绘制**：从 rshapes 移除
- **程序化图像生成**：从 rtextures 移除（保留 GenImageColor）
- **NPatch 九宫格**：从 rtextures 移除
- **Alloc 变体**：从 rtext 移除（TextReplace, TextInsert 等改为返回需 free 的内存）
- **ImageDrawPixel/ImageDrawLine**：改为内部静态函数（ImageClearBackground 等内部使用）

## 已知设计问题
- 大量 Ex/Pro/V/Rec/Lines 变体可通过统一参数合并
- Sound/Music/AudioStream 三组 API 模式重复
- 字符串管理（26 个函数）远超游戏开发核心需求
- 文件系统模块包含大量 OS 级操作