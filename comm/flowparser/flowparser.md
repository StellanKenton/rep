---
doc_role: module-spec
layer: comm
module: flowparser
status: active
portability: layer-dependent
public_headers:
  - flowparser.h
  - flowparser_stream.h
core_files:
  - flowparser.c
  - flowparser_stream.c
port_files: []
debug_files: []
depends_on:
  - ../comm.md
  - ../../tools/ringbuffer/ringbuffer.md
forbidden_depends_on:
  - core 直连 UART 或项目私有 AT 指令表
required_hooks:
  - flowparserStreamSendFunc
optional_hooks:
  - flowparserStreamGetTickFunc
  - flowparserStreamIsUrcFunc
common_utils:
  - tools/ringbuffer
copy_minimal_set:
  - flowparser.h
  - flowparser.c
  - flowparser_stream.h
  - flowparser_stream.c
  - ../../tools/ringbuffer/
read_next:
  - ../comm.md
  - ../../tools/ringbuffer/ringbuffer.md
---

# FlowParser 模块说明

这是当前目录的权威入口文档。

## 1. 模块定位

`flowparser` 负责把 AT / 文本命令交互整理成“命令事务 + 行回调 + 最终结果”模型。它不直接依赖 UART、DMA、RTOS 或具体 modem 指令表。

目录分两层：

- `flowparser.h/.c`：公共事务描述、模式匹配和结果枚举。
- `flowparser_stream.h/.c`：带 transport 绑定的流式执行器，负责发命令、收字节、分行、超时和 URC 分发。

## 2. 目录内文件职责

| 文件 | 职责 |
| --- | --- |
| `flowparser.h` | 公共结果枚举、阶段枚举、事务 spec 和请求结构 |
| `flowparser.c` | 文本模式匹配 helper |
| `flowparser_stream.h` | 流执行器配置、状态、transport hook 和公共 API |
| `flowparser_stream.c` | 发送、接收、分行、超时、prompt/final 判定 |
| `flowparser.md` | 当前目录 contract |

## 3. 对外公共接口

稳定公共头文件：`flowparser_stream.h`

稳定 API：

- `flowparserStreamInit()`
- `flowparserStreamReset()`
- `flowparserStreamFeed()`
- `flowparserStreamSubmit()`
- `flowparserStreamProc()`
- `flowparserStreamIsBusy()`
- `flowparserStreamGetStage()`
- `flowparserStreamSetUrcHandler()`
- `flowparserStreamSetUrcMatcher()`
- `flowparserStreamSetRawHook()`

## 4. 状态机约定

- `IDLE`：当前没有活跃事务。
- `WAIT_RESPONSE`：已发送命令，等待普通响应结束或直接结束。
- `WAIT_PROMPT`：等待 `>` prompt，收到后发送 payload。
- `WAIT_FINAL`：等待最终结束行，例如 `OK`、`SEND OK`、`CONNECT`。

执行规则：

1. `Submit` 只负责提交事务并发送命令头。
2. `Feed` 只负责把底层收到的字节灌入内部 ring buffer。
3. `Proc` 负责消费 ring buffer、按行切分、识别 prompt/结束行/错误行，并在命中时调用 done 回调。
4. 若配置了 `flowparserStreamIsUrcFunc`，则 URC 在事务进行中也会先走 URC 路径，不污染当前事务结果。
5. 若配置了 raw hook，则 `Proc` 会在逐行消费前先检查 ring buffer 头部是否命中完整 raw frame；命中后整帧直接交给 raw handler，后续文本 URC 仍继续按行处理。

## 5. 函数指针 / port / assembly 契约

| 名称 | 必需/可选 | 由谁实现 | 在哪里被调用 | 原型摘要 | 成功语义 | 失败语义 | 前置条件 | 备注 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `flowparserStreamSendFunc` | 必需 | transport 绑定层 | `flowparserStreamSubmit`、prompt 后 payload 发送 | `eFlowParserStrmSta (*)(userData, buf, len)` | 返回 `FLOWPARSER_STREAM_OK` | 返回其他状态视为发送失败 | transport 已初始化 | 不在 core 中直连 UART |
| `flowparserStreamGetTickFunc` | 可选 | transport / platform | 超时检查 | `uint32_t (*)(userData)` | 返回单调 tick | 缺失则禁用基于 tick 的超时 | 需要 timeout 语义 | 使用无符号减法处理回绕 |
| `flowparserStreamIsUrcFunc` | 可选 | 上层协议或平台 | `flowparserStreamProc` 分发前 | `bool (*)(userData, lineBuf, lineLen)` | 返回 `true` 视为 URC | 返回 `false` 视为普通响应 | 行已被完整切分 | 用于隔离异步事件 |
| `flowparserStreamRawMatchFunc` + `flowparserStreamRawFrameFunc` | 可选 | 上层协议或平台 | `flowparserStreamProc` 逐行解析前 | `match(userData, buf, availLen, frameLen)` + `handler(userData, frameBuf, frameLen)` | matcher 返回命中后 handler 收到整帧 | matcher 返回 `NEED_MORE` 时保留当前头部字节等待补齐 | raw frame 固定从 ring 头部开始匹配 | 适合 BLE 透传帧这类非文本字节流 |

## 6. 公共函数使用契约

| 来源模块 | 公共函数 | 允许在哪些文件调用 | 用途 | 调用前提 | 典型调用顺序 | 返回值处理 | 禁止做法 |
| --- | --- | --- | --- | --- | --- | --- | --- |
| `ringbuffer` | `ringBufferInit/Reset/PushByte/PopByte/IsEmpty` | `flowparser_stream.c` | 字节缓存和逐字节切行 | storage 已初始化 | `Init -> Feed -> Proc` | 满缓冲要返回 `OVERFLOW` | 外部直接改 `head/tail` |

## 7. 改动落点矩阵

| 需求 | 应改文件 | 不该改的文件 |
| --- | --- | --- |
| 改模式匹配语义 | `flowparser.c/.h` | transport hook |
| 改 prompt、超时、分行流程 | `flowparser_stream.c/.h` | 上层 modem 状态机 |
| 改 raw frame 识别或旁路转发 | `flowparser_stream.c/.h` + 项目侧 raw hook | 具体业务 manager |
| 改具体 UART / DMA / tick 绑定 | 项目侧 platform/assembly | `flowparser` core |

## 8. 复制到其他工程的最小步骤

最小依赖集：`flowparser.h/.c`、`flowparser_stream.h/.c`、`ringbuffer`。

外部工程至少要补齐发送 hook，并提供接收字节灌入入口；若需要超时或事务内 URC 分流，再补 tick hook 和 URC matcher。

## 9. 验证清单

- 普通 `AT -> ... -> OK` 事务能结束。
- `AT+QISEND` 这类 prompt 事务在收到 `>` 后能发送 payload 并等待最终结果。
- ring buffer 满时返回 `OVERFLOW`，而不是静默覆盖。
- 未配置 URC matcher 时，空闲态行能进入 URC handler；配置 matcher 后，事务内 URC 也能独立上报。
- 配置 raw hook 后，完整 raw frame 会先被转发，且 raw frame 后面紧跟的文本 URC 仍能继续被正常切行处理。