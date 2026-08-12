# REMOVED_API.md — 本 fork 相对上游 raylib 的 API 变更清单（迁移指南）

> 裁剪 fork 删除了大量 API、结构体字段和变体。编译报错"不是 X 的成员"、
> "未定义的标识符"时，先查本表；未收录的用 `grep <符号> src/` 在源码里确认。
> 本清单源自 CONTEXT.md 模块级记录 + 引擎使用报告（Neon Rogue 08-04）实际踩坑
> 交叉整理，**持续维护，欢迎补充**。

## 排查方法（别猜，三分钟定位）

1. `grep -rn "函数名" src/` —— 存在则看签名，不存在则本表找替代
2. 结构体字段报错 → 查 `src/raylib_core.h` 中对应 `typedef struct`
3. 拿不准默认值/语义 → 看对应头文件的行内注释（本 fork 大量精简注释）

## 模块级删除（整族移除）

| 模块 | 原 API 族 | 说明 |
|---|---|---|
| rmodels | 全部 3D（LoadModel/Material/Bone/Skeleton/网格/动画） | 2D-only 定位，整模块移除 |
| VR 渲染 | SetStereoConfig / VR 相关 | 从 rcore 移除 |
| 自动化事件 | AutomationEvent* / 事件录制回放 | 从 rcore 移除（AI 侧有 record/playback 替代） |
| 手势识别 | SetGesturesEnabled / GetGestureXxx | 精简 |
| 样条线绘制 | DrawSplineLinear/Segment/Basis/CatmullRom | 从 rshapes 移除 |
| 程序化图像生成 | GenImageGradient / GenImagePerlinNoise / GenImageSpot / GenImageChecked 等 | 从 rtextures 移除，**仅保留 GenImageColor** |
| NPatch 九宫格 | NPatchInfo / DrawTextureNPatch / SetShapesTexture 相关 | 从 rtextures 移除 |
| raymath 3D 运算 | 无整族删除 | **Quaternion/Vector4 类型与运算保留**（24 个 Quaternion 函数：Add/Multiply/Lerp/Slerp/FromMatrix 等）；Vector4 无独立运算函数（Quaternion 是 Vector4 别名），2D 开发用 Vector2/Matrix |

## 已知结构体字段删除

| 结构 | 删除字段 | 现状与替代 |
|---|---|---|
| `Sound` | `sampleRate` 等公开音频参数 | 现仅 `AudioStream stream` + `unsigned int frameCount`；采样率由 AudioStream 内部管理，别再读字段 |
| （遇其他字段报错） | — | 查 `raylib_core.h` typedef，2D 定位下大量纹理/3D 字段已删 |

## 已知函数语义变化（存在但行为不同）

| 原 API | 变化 |
|---|---|
| `TextReplace` / `TextInsert` / `TextJoin` 等字符串函数 | 保留，但 **Alloc 变体语义变化**：返回的内存需要调用方 `free()`（不再是静态缓冲区） |
| `ImageDrawPixel` / `ImageDrawLine` | 内部化为静态函数，不再公开导出 |

## 使用报告补充的坑（Neon Rogue 08-04）

- `Sound` 无 `sampleRate`：直接读 `AudioStream` 相关 API，别猜字段
- 3D 相关函数一律不存在：写代码前先确认是 2D API
- 本表之外的删除项：`grep src/` 是唯一权威，避免"凭空猜测"（该纪律来自真实踩坑）

---

*维护：新增删除项时在此登记（原 API → 替代/原因），保持与 CONTEXT.md 一致。*
