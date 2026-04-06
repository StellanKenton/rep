---
doc_role: layer-guide
layer: console
module: log
status: active
portability: layer-dependent
public_headers:
    - log.h
core_files:
    - log.c
    - log.md
port_files: []
debug_files: []
depends_on:
    - ../tools/ringbuffer/ringbuffer.md
forbidden_depends_on:
    - 业务层直接操作 transport 私有缓冲
required_hooks:
    - logInitFunc
    - logOutputWriteFunc
    - logInputGetBufferFunc
optional_hooks: []
common_utils:
    - tools/ringbuffer
copy_minimal_set:
    - console/log.h
    - console/log.c
read_next:
    - console.md
---

# Log Hook Guide

这是 `console/` 目录中 log 子模块的权威入口文档。

## 1. 本层目标和边界

`log` 负责统一日志级别、transport 输入输出抽象、输入缓冲暴露和按 transport 定向写出。

本层负责：

- `LOG_E/W/I/D` 统一入口。
- transport 初始化、输出写接口、输入 ring buffer provider。
- 为 `console` 提供输入枚举和定向回复能力。

本层不负责：

- 命令解析。
- 业务命令分发。

## 2. 对外公共接口

稳定公共头文件：`log.h`

稳定 API：

- `logInit()`
- `logGetInputCount()`
- `logGetInputTransport()`
- `logGetInputBuffer()`
- `logWriteToTransport()`
- `logProcessOutput()`
- `logGetStats()`
- `logSetTimestampProvider()`

## 3. transport hook 契约

| 名称 | 必需/可选 | 由谁实现 | 在哪里被调用 | 原型摘要 | 成功语义 | 失败语义 | 前置条件 | 备注 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `init` | 可选 | transport provider | `logInit()` | `void (*)(void)` | transport 完成初始化 | `NULL` 表示无需初始化 | `logInit()` 前 | 输入和输出共享同一个 init |
| `write` | 输出侧必需 | transport provider | `logVWrite()`、`logWriteToTransport()` | `int32_t (*)(const uint8_t *, uint16_t)` | 返回实际写出字节数 | 返回 `0` 视为未发送 | transport 已启用输出 | 不追加额外前缀和换行 |
| `getBuffer` | 输入侧必需 | transport provider | `logGetInputBuffer()` | `stRingBuffer *(*)(void)` | 返回该 transport 输入缓冲 | 返回 `NULL` 视为无输入 | transport 已启用输入 | log 不拥有该 ring buffer 生命周期 |

## 4. 公共函数使用契约

| 来源模块 | 公共函数 | 允许在哪些文件调用 | 用途 | 调用前提 | 典型调用顺序 | 返回值处理 | 禁止做法 |
| --- | --- | --- | --- | --- | --- | --- | --- |
| `drvuart` | `drvUartInit` / `drvUartTransmit` / `drvUartGetRingBuffer` | transport provider | 实现 UART log 输入输出 | 对应逻辑 UART 已定义 | `init -> write/getBuffer` | 非 `OK` 视为发送失败 | 在业务层直接调用 UART 实现日志 |
| `ringbuffer` | `stRingBuffer` 及相关 API | transport provider | 承载输入缓存 | provider 自己拥有存储区 | `init -> getBuffer` | `NULL` 表示不可用 | 让 log 直接修改 ring 索引 |

## 5. 改动落点矩阵

| 需求 | 应改文件 | 不该改的文件 |
| --- | --- | --- |
| 新增 transport | transport provider + `log` 接口表 | `console.c` |
| 调整日志格式 | `log.c` | transport provider 中硬编码格式化 |
| 调整 console 回复通道 | `logWriteToTransport()` + `console.c` | 业务命令处理函数 |

## 6. 复制到其他工程的最小步骤

最小依赖集：

- `log.h/.c`
- 至少一个 transport provider
- `ringbuffer`（若需要输入）

外部项目必须补齐：

- transport 接口表
- 输出 write hook
- 若要支持 console，再补输入 `getBuffer` hook

## 7. 验证清单

- `logInit()` 后所有启用 transport 都能完成初始化。
- 广播日志能写到所有启用输出口。
- `logWriteToTransport()` 只写到目标 transport。
- 输入缓冲为空或 `NULL` 时行为可预期。