---
doc_role: module-spec
layer: driver
module: drvuart
status: active
portability: layer-dependent
public_headers:
    - drvuart.h
core_files:
    - drvuart.c
port_files: []
debug_files:
    - drvuart_debug.h
    - drvuart_debug.c
depends_on:
    - ../../tools/ringbuffer/ringbuffer.md
forbidden_depends_on:
    - 在 core 中直连 UART BSP 或 DMA 私有结构
required_hooks:
    - drvUartBspInterface.init
    - drvUartBspInterface.transmit
    - drvUartBspInterface.getDataLen
    - drvUartBspInterface.receive
optional_hooks:
    - drvUartBspInterface.transmitIt
    - drvUartBspInterface.transmitDma
common_utils:
    - tools/ringbuffer
copy_minimal_set:
    - drvuart.h
    - drvuart.c
    - drvuart_debug.h
    - drvuart_debug.c
    - tools/ringbuffer/
read_next:
    - ../drvrule.md
    - ../../tools/ringbuffer/ringbuffer.md
---

# DrvUart 模块说明

这是当前目录的权威入口文档。

## 1. 模块定位

`drvuart` 提供带公共接收 ring buffer 的 UART 驱动抽象。它解决的是“逻辑 UART 的稳定收发和接收缓存归一化”，不解决具体 DMA、IRQ、GPIO 配置细节。

## 2. 目录内文件职责

| 文件 | 职责 |
| --- | --- |
| `drvuart.h` | 公共 API、BSP hook 类型、ring buffer 查询接口 |
| `drvuart.c` | 参数校验、初始化、接收同步、公共缓存访问 |
| `drvuart_debug.h/.c` | 可选 debug / console 能力 |
| `drvuart.md` | 当前目录 contract |

说明：当前目录没有独立 `_port.*` 文件，平台绑定通过 BSP hook 表和外部 provider 完成。

## 3. 对外公共接口

稳定公共头文件：`drvuart.h`

稳定 API：

- `drvUartInit()`
- `drvUartTransmit()`
- `drvUartTransmitIt()`
- `drvUartTransmitDma()`
- `drvUartReceive()`
- `drvUartGetDataLen()`
- `drvUartGetRingBuffer()`

调用顺序：

1. 先 `drvUartInit(uart)`。
2. 再调用发送 / 接收 API。
3. 上层若需要直接消费缓存，使用 `drvUartGetRingBuffer()`。

## 4. 配置、状态与生命周期

- `drvUartInit()` 负责校验 hook、初始化底层 UART，并建立公共 ring buffer 运行前提。
- `drvUartReceive()`、`drvUartGetDataLen()` 操作的是公共层 ring buffer，不是 BSP 原始 DMA 缓冲。
- `drvUartTransmitIt()` 和 `drvUartTransmitDma()` 属于可选能力，未绑定时必须返回 `DRV_STATUS_UNSUPPORTED`。

## 5. 依赖白名单与黑名单

- 允许依赖：`ringbuffer` 公共接口。
- 禁止依赖：在 `drvuart.c` 中直接包含 BSP 头、DMA 句柄、IRQ 细节。
- 禁止做法：让 BSP 直接操作公共层 ring buffer。

## 6. 函数指针 / port / assembly 契约

| 名称 | 必需/可选 | 由谁实现 | 在哪里被调用 | 原型摘要 | 成功语义 | 失败语义 | 前置条件 | 备注 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `init` | 必需 | 当前工程 UART BSP | `drvUartInit()` | `eDrvStatus (*)(uint8_t uart)` | 逻辑 UART 可收发 | 返回明确错误码 | uart 合法 | 可重复初始化，但行为必须可预期 |
| `transmit` | 必需 | 当前工程 UART BSP | `drvUartTransmit()` | `eDrvStatus (*)(uint8_t, const uint8_t *, uint16_t, uint32_t)` | 同步发送成功 | 超时/忙/错误 | 已初始化 | 必须真实处理 `timeoutMs` |
| `getDataLen` | 必需 | 当前工程 UART BSP | 接收同步流程 | `uint16_t (*)(uint8_t uart)` | 返回 BSP 原始缓冲可读字节数 | 失败时返回 `0` | 已初始化 | 不是公共 ring buffer 已用量 |
| `receive` | 必需 | 当前工程 UART BSP | 接收同步流程、`drvUartReceive()` | `eDrvStatus (*)(uint8_t, uint8_t *, uint16_t)` | 从原始缓冲读出数据 | 返回明确错误 | 已初始化 | 必须推进 BSP 消费状态 |
| `transmitIt` | 可选 | 当前工程 UART BSP | `drvUartTransmitIt()` | `eDrvStatus (*)(uint8_t, const uint8_t *, uint16_t)` | 启动中断发送 | 未实现时由公共层返回 `UNSUPPORTED` | 已初始化 | 不能伪成功 |
| `transmitDma` | 可选 | 当前工程 UART BSP | `drvUartTransmitDma()` | `eDrvStatus (*)(uint8_t, const uint8_t *, uint16_t)` | 启动 DMA 发送 | 未实现或忙时返回明确错误 | 已初始化 | 必须管理 DMA 忙状态 |

## 7. 公共函数使用契约

| 来源模块 | 公共函数 | 允许在哪些文件调用 | 用途 | 调用前提 | 典型调用顺序 | 返回值处理 | 禁止做法 |
| --- | --- | --- | --- | --- | --- | --- | --- |
| `ringbuffer` | `ringBufferInit/Write/Read/GetUsed` | `drvuart.c` | 维护公共接收缓存 | hook 已可用 | `Init -> Sync -> Read` | 失败时返回或截断同步 | 让 BSP 绕过公共层直接写 ring |

## 8. 改动落点矩阵

| 需求 | 应改文件 | 不该改的文件 |
| --- | --- | --- |
| 改发送 / 接收公共语义 | `drvuart.c/.h` | BSP 寄存器实现 |
| 改默认逻辑 UART 绑定或原始缓冲实现 | 当前工程 UART BSP / provider | `drvuart.c` 公共流程 |
| 改 console / debug 命令 | `drvuart_debug.*` | `drvuart.c` 主流程 |

## 9. 复制到其他工程的最小步骤

最小依赖集：`drvuart.h/.c`、`ringbuffer`、新的 UART BSP hook 实现。

外部项目必须补齐：`init`、`transmit`、`getDataLen`、`receive`，以及用于公共缓存的底层存储区。

若不需要 console / debug，可不带 `drvuart_debug.*`。

## 10. 验证清单

- 初始化后同步发送和接收都可用。
- `getDataLen` 与 `receive` 统计一致，不重复读、不丢读。
- 公共 ring buffer 数据不会与 BSP 原始缓冲混用。
- 可选 DMA / IT hook 缺失时返回值明确。
