# WiFi 命令流解析设计




































---
doc_role: module-spec
layer: comm
module: flowparser
status: active
portability: layer-dependent
public_headers:
    - flowparser_stream.h
    - flowparser_tokenizer.h
core_files:
    - flowparser_stream.c
    - flowparser_tokenizer.c
port_files: []
debug_files: []
depends_on:
    - ../../tools/ringbuffer/ringbuffer.md
forbidden_depends_on:
    - tokenizer 或 stream core 直连 UART 私有实现
    - 在 core 内硬编码具体 AT 命令词或业务状态机
required_hooks:
    - flowparserStreamSendFunc
    - flowparserStreamGetTickFunc
optional_hooks:
    - flowparserStreamMatchFunc
    - flowparserStreamLineFunc
    - flowparserStreamDoneFunc
common_utils:
    - tools/ringbuffer
copy_minimal_set:
    - flowparser_stream.h
    - flowparser_stream.c
    - flowparser_tokenizer.h
    - flowparser_tokenizer.c
    - ../../tools/ringbuffer/ringbuffer.h
    - ../../tools/ringbuffer/ringbuffer.c
read_next:
    - ../comm.md
    - ../../tools/ringbuffer/ringbuffer.md
---

# FlowParser 模块说明

这是当前目录的权威入口文档。

## 1. 模块定位

`flowparser` 面向 AT / 命令事务流，而不是固定帧协议。它拆成两层：

- `tokenizer` 从 `ringbuffer` 中逐字节取数据，切出 `LINE`、`PROMPT`、`OVERFLOW` 三类词元。
- `stream` 在词元之上维护单事务状态机，处理命令发送、提示符等待、最终结果匹配、URC 分流和超时。

它适合“发一条命令，等若干行响应，必要时等 `>` 再发 payload，最后等 `OK/ERROR`”这一类串行事务，不适合多命令并发复用同一个流。

## 2. 目录内文件职责

| 文件 | 职责 |
| --- | --- |
| `flowparser_tokenizer.h/.c` | tokenizer 配置、状态和 `LINE/PROMPT/OVERFLOW` 切词逻辑 |
| `flowparser_stream.h/.c` | 单事务状态机、命令提交、payload 发送、终态匹配、URC 分流、超时检查 |
| `flowparser.md` | 当前目录 contract |

## 3. 对外公共接口

稳定公共头文件：`flowparser_stream.h`、`flowparser_tokenizer.h`

稳定 API：

- `flowparserTokIsCfgValid()` / `flowparserTokInit()` / `flowparserTokReset()` / `flowparserTokGet()`
- `flowparserStreamIsCfgValid()` / `flowparserStreamInit()` / `flowparserStreamReset()`
- `flowparserStreamSubmit()` / `flowparserStreamProc()`
- `flowparserStreamIsBusy()` / `flowparserStreamGetStage()`

推荐调用顺序：

1. 调用方先准备 `ringbuffer`、line buffer、command buffer、payload buffer。
2. 填好 `stFlowParserStreamCfg`，至少提供 `send` 和 `getTick`。
3. 调用 `flowparserStreamInit()`。
4. UART/链路接收路径把字节写入 `ringbuffer`。
5. 业务层通过 `flowparserStreamSubmit()` 发送命令并建立一个活动事务。
6. 任务上下文周期调用 `flowparserStreamProc()`，直到 `flowparserStreamIsBusy()` 变为 `false`。

注意：`flowparserStreamSubmit()` 会立即调用 `send` 发送命令，不会等到 `flowparserStreamProc()` 才真正下发。

## 4. 配置、状态与生命周期

### 4.1 tokenizer

- tokenizer 不拥有 `ringbuffer` 和 `lineBuf`，只保存外部指针。
- `LINE` 在读到 `\r\n` 或单独 `\n` 时产出；行内容里不包含换行符。
- 若上一字节是 `\r` 且后一字节不是 `\n`，这个 `\r` 会按普通字符并入当前行。
- 只有当 `>` 出现在一条新 token 的开头，也就是当前 `lineLen == 0` 时，才会产出 `PROMPT`。
- 当行缓存装不下时，tokenizer 进入 overflow 丢弃模式，直到消费到本行结束，再产出一个 `OVERFLOW` token。
- `flowparserTokReset()` 只清内部状态，不清外部 `ringbuffer` 内容。

### 4.2 stream

- stream 只允许单事务串行执行；活动事务未结束时再次 `Submit` 会返回 `FLOWPARSER_STREAM_BUSY`。
- `flowparserStreamSubmit()` 会把 `cmdBuf` 和 `payloadBuf` 复制到 `cfg.cmdBuf/payloadBuf`，因此原始命令和 payload 可以是短生命周期缓冲。
- `req->spec` 不会被复制，stream 只保存这个指针，所以 `stFlowParserSpec` 必须至少活到事务结束。
- `lineHandler` 只接收“非终态、非 URC”的普通行；命中 `errorPatterns`、`responseDonePatterns`、`finalDonePatterns` 的行不会再回调给它。
- `doneHandler` 在事务结束时同步调用一次，结束原因可能是 `OK`、`ERROR`、`TIMEOUT`、`OVERFLOW`、`SEND_FAIL`。
- `flowparserStreamReset()` 会重置 tokenizer 和事务状态，但不会主动清空底层 `ringbuffer`。

### 4.3 状态机

`eFlowParserStage` 的实际语义如下：

| 阶段 | 进入时机 | 退出条件 |
| --- | --- | --- |
| `FLOWPARSER_STAGE_IDLE` | 初始化后或事务结束后 | `Submit` 新请求 |
| `FLOWPARSER_STAGE_WAIT_RESPONSE` | `needPrompt == false` 的命令提交后 | 命中 response done / error / timeout / overflow |
| `FLOWPARSER_STAGE_WAIT_PROMPT` | `needPrompt == true` 的命令提交后 | 收到 `PROMPT`，或 timeout / overflow / send fail |
| `FLOWPARSER_STAGE_WAIT_FINAL` | `PROMPT` 后 payload 发送成功，或 `needPrompt` 流程后续阶段 | 命中 final done / error / timeout / overflow |

补充说明：

- 当 `needPrompt == true` 且 `payloadLen == 0` 时，收到 `PROMPT` 后不会再调用 `send`，而是直接切到 `WAIT_FINAL`。
- 如果某阶段的专用超时为 `0`，而 `totalToutMs` 非 `0`，实现会退回使用总超时值作为该阶段超时。
- `procBudget == 0` 时初始化会自动改成 `8`。

## 5. 依赖白名单与黑名单

- 允许依赖：`ringbuffer`、`rep_config.h` 中暴露的公共状态类型。
- 禁止依赖：UART 私有驱动头、MCU HAL、RTOS API、具体 AT 业务状态机。
- 禁止做法：
  - 在 tokenizer 或 stream core 里直接判断具体模块命令字。
  - 绕过 stream 状态机让多个调用方并发发送同一链路命令。
  - 把短生命周期的 `stFlowParserSpec` 栈对象直接提交给异步事务。

## 6. 函数指针 / port / assembly 契约

| 名称 | 必需/可选 | 由谁实现 | 在哪里被调用 | 原型摘要 | 成功语义 | 失败语义 | 前置条件 | 备注 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `flowparserStreamSendFunc` | 必需 | 当前工程链路 provider | `flowparserStreamSubmit()` 和 `PROMPT` 后 payload 下发路径 | `eDrvStatus (*)(const uint8_t *, uint16_t, void *)` | 数据已交给底层链路发送 | 返回非 `DRV_STATUS_OK`，命令提交失败或事务以 `SEND_FAIL` 结束 | stream 已初始化，缓冲已准备好 | 只表示发送请求被接受，不表示收到响应 |
| `flowparserStreamGetTickFunc` | 必需 | 当前工程平台 | `flowparserStreamSubmit()`、`flowparserStreamProc()` | `uint32_t (*)(void)` | 返回单调递增 tick | 缺失时无法初始化 | tick 基准在整个事务期间保持一致 | 实现按无符号减法比较超时 |
| `flowparserStreamMatchFunc` | 可选 | 当前工程 URC provider | `flowparserStreamHandleLine()` | `bool (*)(const uint8_t *, uint16_t, void *)` | 当前行应按 URC 路由 | 返回 `false` 表示不是 URC | tokenizer 已产出 `LINE` | 若提供，优先于 `urcPatterns` 前缀表 |
| `flowparserStreamLineFunc` | 可选 | 业务层或项目层 | 普通行分发、URC 分发、事务结束回调前 | `void (*)(void *, const uint8_t *, uint16_t)` | 上层收到一行文本 | 无返回值 | 对应 handler 已注册 | 不应长时间阻塞；URC 和普通行可能使用不同 userData |
| `flowparserStreamDoneFunc` | 可选 | 业务层 | `flowparserStreamFinish()` | `void (*)(void *, eFlowParserResult)` | 上层收到单次事务最终结果 | 无返回值 | 提交请求时传入 | 回调发生时事务已被标记为 idle |

## 7. 公共函数使用契约

| 来源模块 | 公共函数 | 允许在哪些文件调用 | 用途 | 调用前提 | 典型调用顺序 | 返回值处理 | 禁止做法 |
| --- | --- | --- | --- | --- | --- | --- | --- |
| `ringbuffer` | `ringBufferPopByte()` | `flowparser_tokenizer.c` | 逐字节消费接收流并切词元 | ring buffer 已完成初始化，写入方与读取方并发模型已由上层约束 | `UART/port push bytes -> flowparserTokGet -> token` | 读空时返回 `FLOWPARSER_TOK_EMPTY` | 在多个消费者之间共享同一个 tokenizer/ringbuffer 读口 |

## 8. 改动落点矩阵

| 需求 | 应改文件 | 不该改的文件 |
| --- | --- | --- |
| 改 `CR/LF` 切分、`>` 识别、overflow 规则 | `flowparser_tokenizer.*` | `flowparser_stream.*` |
| 改事务阶段推进、终态匹配顺序、超时策略 | `flowparser_stream.*` | `flowparser_tokenizer.*` |
| 改 URC 前缀表或自定义 matcher | 项目侧 stream cfg provider | tokenizer core |
| 改具体命令的 `OK/ERROR` 模式、payload/timeout 组合 | 调用方构造的 `stFlowParserSpec` | flowparser core |
| 改命令缓冲、payload 缓冲、ring buffer 大小 | 项目侧装配文件 | stream/tokenizer 核心逻辑 |

## 9. 复制到其他工程的最小步骤

最小依赖集：`flowparser_stream.*`、`flowparser_tokenizer.*`、`ringbuffer.h/.c`。

外部项目至少需要补齐：

1. 一个接收字节写入 `ringbuffer` 的链路。
2. `flowparserStreamSendFunc`。
3. `flowparserStreamGetTickFunc`。
4. command buffer、payload buffer、line buffer 的实际存储。

如果需要 URC 分流或异步事务回调，再额外补：

1. `urcPatterns` 或 `flowparserStreamMatchFunc`。
2. `urcHandler`。
3. 请求级 `lineHandler` / `doneHandler`。

## 10. 验证清单

- `\r\n`、单独 `\n`、孤立 `\r` 三种输入都能按源码语义稳定切分。
- 超长行只产出一次 `OVERFLOW`，并在下一行重新恢复解析。
- `needPrompt == true` 时，只有在收到 `PROMPT` 后才会发送 payload 或进入 `WAIT_FINAL`。
- `responseDonePatterns` 只在 `WAIT_RESPONSE` 生效，`finalDonePatterns` 只在 `WAIT_FINAL` 生效。
- 活动事务期间再次 `Submit` 会返回 `BUSY`，不会打断当前事务。
- URC 行不会进入事务 `lineHandler`；但若终态模式与 URC 模式重叠，终态匹配优先。
- 调用方若使用异步事务，`stFlowParserSpec` 生命周期覆盖整个事务。
