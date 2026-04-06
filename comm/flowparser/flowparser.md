# WiFi 命令流解析设计




















































































- 控制用简化滤波器与标准一阶滤波器边界清晰。- 连续调用时历史输入输出推进正确。- 初始化后状态量正确清零或设为指定初值。## 10. 验证清单最小依赖集：`filtterfisrt.h/.c`。## 9. 复制到其他工程的最小步骤| 改示例或历史说明 | `filtterfisrt.md` | `filter1st.md` 主 contract || 改一阶滤波差分方程 | `filtterfisrt.c/.h` | 上层控制流程 || --- | --- | --- || 需求 | 应改文件 | 不该改的文件 |## 8. 改动落点矩阵当前目录不调用其他公共模块函数。## 7. 公共函数使用契约当前目录无平台 hook。## 6. 函数指针 / port / assembly 契约- 禁止在不同采样周期下复用同一组系数却不重新标注语义。- 不依赖 RTOS、BSP 或业务模块。## 5. 依赖白名单与黑名单- 状态重置只能通过显式 reset 接口完成。- 初始化后按固定采样周期重复调用更新接口。- 过滤器对象由调用方持有。## 4. 配置、状态与生命周期稳定能力：初始化、参数设置、状态复位、单步更新。稳定公共头文件：`filtterfisrt.h`## 3. 对外公共接口| `filtterfisrt.md` | 历史补充说明 || `filter1st.md` | 权威主文档 || `filtterfisrt.c` | 差分方程更新和状态推进 || `filtterfisrt.h` | 一阶滤波对象、参数和公共 API || --- | --- || 文件 | 职责 |## 2. 目录内文件职责`filter1st` 提供一阶离散滤波器和控制用一阶滤波器对象，用于固定周期采样的平滑和低通处理。## 1. 模块定位补充阅读文档：`filtterfisrt.md` 保留为历史说明；最终 contract 以本文件为准。这是当前目录的权威入口文档。# Filter1st 模块说明---  - filtterfisrt.mdread_next:  - filtterfisrt.c  - filtterfisrt.hcopy_minimal_set:common_utils: []optional_hooks: []required_hooks: []forbidden_depends_on: []depends_on: []debug_files: []port_files: []  - filtterfisrt.ccore_files:  - filtterfisrt.hpublic_headers:portability: standalonestatus: activemodule: filter1stlayer: tools## 1. 问题背景

WiFi 模块，尤其是常见的 AT 命令流。

它更像一种“命令交互流”或者“事务流”，典型特征是：

- 有些响应是按行返回的，比如 `\r\nOK\r\n`、`\r\nERROR\r\n`。
- 有些响应不是完整一行，而是一个单独提示符，比如 `>`。
- 有些命令很快返回，有些命令要几百毫秒甚至几秒。
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
    - tools/ringbuffer/
read_next:
    - ../comm.md
    - ../../tools/ringbuffer/ringbuffer.md
---

# FlowParser 模块说明

这是当前目录的权威入口文档。

## 1. 模块定位

`flowparser` 面向 AT / 命令事务流，而不是固定帧协议。它由 tokenizer 和 stream 两部分组成：tokenizer 把字节流切成 `LINE` / `PROMPT` / `OVERFLOW` 词元，stream 用阶段状态机驱动单命令事务。

## 2. 目录内文件职责

| 文件 | 职责 |
| --- | --- |
| `flowparser_tokenizer.h/.c` | 从 ring buffer 产出 `LINE` / `PROMPT` / `OVERFLOW` 词元 |
| `flowparser_stream.h/.c` | 单命令事务状态机、请求提交、超时、URC 分流 |
| `flowparser.md` | 当前目录 contract |

## 3. 对外公共接口

稳定公共头文件：`flowparser_stream.h`、`flowparser_tokenizer.h`

稳定 API：

- `flowparserTokInit()` / `flowparserTokGet()` / `flowparserTokReset()`
- `flowparserStreamInit()` / `flowparserStreamSubmit()` / `flowparserStreamProc()` / `flowparserStreamReset()`
- `flowparserStreamIsBusy()` / `flowparserStreamGetStage()`

调用顺序：

1. 先准备 ring buffer、line buffer、send hook、tick hook。
2. 初始化 tokenizer 和 stream。
3. 业务层通过 `flowparserStreamSubmit()` 提交一个请求。
4. 周期调用 `flowparserStreamProc()` 驱动事务。

## 4. 配置、状态与生命周期

- tokenizer 只负责切词元，不负责判断命令是否成功。
- stream 保持单事务串行模型，任意时刻只允许一个活动命令。
- `eFlowParserStage` 决定当前等待的是普通响应、提示符还是最终结果。
- `eFlowParserResult` 只描述当前事务结果，不代表底层发送动作一定完成。

## 5. 依赖白名单与黑名单

- 允许依赖：`ringbuffer`。
- 禁止依赖：在 tokenizer 或 stream core 中直连 UART 私有实现、MCU SDK 或业务协议解析。
- 禁止做法：让多个任务直接并发发送 AT 命令而绕过 stream 状态机。

## 6. 函数指针 / port / assembly 契约

| 名称 | 必需/可选 | 由谁实现 | 在哪里被调用 | 原型摘要 | 成功语义 | 失败语义 | 前置条件 | 备注 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `flowparserStreamSendFunc` | 必需 | 当前工程链路 provider | `flowparserStreamSubmit/Proc` | `eDrvStatus (*)(const uint8_t *, uint16_t, void *)` | 命令或 payload 已交给底层链路 | 返回错误或忙 | stream 已初始化 | 只表示底层接受发送请求，不代表事务完成 |
| `flowparserStreamGetTickFunc` | 必需 | 当前工程平台 | stream 超时判定 | `uint32_t (*)(void)` | 返回单调 tick | 缺失则无法初始化 | stream 运行 | 用于 total / response / prompt / final 超时 |
| `flowparserStreamMatchFunc` | 可选 | 当前工程 URC provider | URC 判断 | `bool (*)(lineBuf, lineLen, userCtx)` | 当前行匹配 URC | `false` 表示不是 URC | tokenizer 已产出 `LINE` | 可替代简单前缀表 |
| `flowparserStreamLineFunc` | 可选 | 业务层 | 行级回调 | `void (*)(userData, lineBuf, lineLen)` | 上层收到中间行或 URC | 无返回值 | 对应 handler 已注册 | 不得阻塞太久 |
| `flowparserStreamDoneFunc` | 可选 | 业务层 | 事务结束通知 | `void (*)(userData, result)` | 上层收到最终结果 | 无返回值 | 提交请求时传入 | 可用于异步结果归档 |

## 7. 公共函数使用契约

| 来源模块 | 公共函数 | 允许在哪些文件调用 | 用途 | 调用前提 | 典型调用顺序 | 返回值处理 | 禁止做法 |
| --- | --- | --- | --- | --- | --- | --- | --- |
| `ringbuffer` | `ringBufferRead/Peek/GetUsed` | `flowparser_tokenizer.c` | 组装行、识别 prompt | ring buffer 已初始化 | `Peek/Read -> Tokenize` | 空缓冲返回 `EMPTY` | 在 ISR 中做复杂字符串匹配 |

## 8. 改动落点矩阵

| 需求 | 应改文件 | 不该改的文件 |
| --- | --- | --- |
| 改行切分或 prompt 识别 | `flowparser_tokenizer.*` | stream 事务状态机 |
| 改事务阶段、超时、URC 分流 | `flowparser_stream.*` | tokenizer 基础切分 |
| 改具体命令词、超时策略 | 请求 spec / provider | tokenizer core |

## 9. 复制到其他工程的最小步骤

最小依赖集：`flowparser_stream.*`、`flowparser_tokenizer.*`、`ringbuffer`。

外部项目必须补齐：发送 hook、tick hook；若需要 URC 分流或异步回调，再补 line/done/URC matcher。

## 10. 验证清单

- `LINE`、`PROMPT`、`OVERFLOW` 三类词元识别稳定。
- 单事务串行语义成立，新请求不会打断活动命令。
- `>` 提示后才发送 payload。
- URC 不会污染当前命令结果。
## 4. 推荐状态机模型
