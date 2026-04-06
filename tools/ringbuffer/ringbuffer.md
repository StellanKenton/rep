---
doc_role: tool-spec
layer: tools
module: ringbuffer
status: active
portability: standalone
public_headers:
  - ringbuffer.h
core_files:
  - ringbuffer.c
port_files: []
debug_files: []
depends_on: []
forbidden_depends_on:
  - 直接在外部修改 head/tail
required_hooks: []
optional_hooks: []
common_utils: []
copy_minimal_set:
  - ringbuffer.h
  - ringbuffer.c
read_next:
  - ringbuffer_architecture.md
---

# RingBuffer 模块说明

这是当前目录的权威入口文档。

补充阅读文档：`ringbuffer_architecture.md` 用于解释架构背景和设计取舍；最终 contract 以本文件为准。

## 1. 模块定位

`ringbuffer` 是字节导向的基础容器，负责单调索引、容量计算、单字节与批量读写，以及覆盖写策略。它不负责线程调度或阻塞等待。

## 2. 目录内文件职责

| 文件 | 职责 |
| --- | --- |
| `ringbuffer.h` | 状态码、控制块、公共 API |
| `ringbuffer.c` | 核心读写、peek、discard、overwrite |
| `ringbuffer_architecture.md` | 架构背景与设计说明 |
| `ringbuffer.md` | 权威主文档 |

## 3. 对外公共接口

稳定公共头文件：`ringbuffer.h`

稳定 API：

- `ringBufferInit()` / `ringBufferReset()`
- `ringBufferGetUsed()` / `ringBufferGetFree()` / `ringBufferGetCapacity()`
- `ringBufferIsEmpty()` / `ringBufferIsFull()`
- `ringBufferPushByte()` / `ringBufferPopByte()` / `ringBufferPeekByte()`
- `ringBufferWrite()` / `ringBufferRead()` / `ringBufferPeek()` / `ringBufferDiscard()`
- `ringBufferWriteOverwrite()`

## 4. 配置、状态与生命周期

- 存储区由调用方提供并持有。
- `head` / `tail` 为单调递增索引，不应由外部直接修改。
- 默认模型是调用方负责并发保护；只有在明确 SPSC 约束下才可无锁使用。

## 5. 依赖白名单与黑名单

- 不依赖 RTOS、BSP 或业务模块。
- 禁止外部直接改 `head` / `tail`。
- 禁止把 overwrite 语义隐藏在普通写接口里。

## 6. 函数指针 / port / assembly 契约

当前目录无必需 platform hook。若外部项目需要临界区或内存屏障，应在上层封装，不应污染当前公共 API。

## 7. 公共函数使用契约

当前目录为工具模块，本身不调用其他公共模块函数。

## 8. 改动落点矩阵

| 需求 | 应改文件 | 不该改的文件 |
| --- | --- | --- |
| 改容量、索引、覆盖写语义 | `ringbuffer.c/.h` | 上层驱动或 parser 流程 |
| 改线程安全策略 | 上层封装或调用方文档 | `ringbuffer.c` 热路径里硬编码 RTOS 逻辑 |

## 9. 复制到其他工程的最小步骤

最小依赖集：`ringbuffer.h/.c`。

若外部项目有 ISR/任务并发需求，需在调用方补齐临界区约束和 ownership 说明。

## 10. 验证清单

- 初始化参数非法时返回明确错误。
- 满、空、used/free 计算一致。
- overwrite 只在专用 API 中发生。
- SPSC 和外部保护共享两种使用模式边界清晰。
