# frameprocess 模块计划

## 1. 文档目标

`frameprocess` 用于替代当前 `appcomm` 模块，统一管理协议帧接收、结构化解析、发送调度、优先级队列以及 ACK 超时重发。

该模块的核心目标是把“协议解析、业务数据缓存、发送流程控制”拆分为职责清晰的多个文件，避免把接收、处理、发送和重发逻辑集中在单一实现中，降低后续维护成本。

建议按以下分层组织：

- `frameprocess`：核心流程层，负责实例管理、收发状态机、ACK 处理和发送调度。
- `frameprocess_data`：数据转换层，负责命令结构体定义以及结构体与 payload 之间的双向转换。
- `frameprocess_pack`：业务封装层，负责命令相关的业务动作生成。
- `frameprocess_port`：端口适配层，负责绑定 `frameparser`、UART、tick、默认缓冲区和默认协议配置。

M1 阶段边界：

- `frameprocess.h` 只依赖 `frameparser` 的 core 公共类型，禁止 include `framepareser_port.h`。
- `frameprocess.c` 只处理协议流程和 core 初始化接口，默认协议、UART、tick、缓冲区等项目绑定通过 `frmProcLoadPlatformDefaultCfg()`、`frmProcPlatformInit()`、`frmProcEnsurePlatformFmt()`、`frmProcBuildPlatformPkt()` 等通用平台钩子下沉到 `frameprocess_port.*`。
- 所有板级默认协议、链路资源和项目绑定都只放在 `frameprocess_port.*`，不要在 `frameprocess` 公共头文件里暴露端口层细节。

按这个边界划分后：

- 新增命令时，优先修改 `frameprocess_data` 和 `frameprocess_pack`。
- 调整板级连接或默认链路时，优先修改 `frameprocess_port`。
- 调整 ACK、实例流程或调度策略时，优先修改 `frameprocess` 核心层。

---
doc_role: service-spec
layer: comm
module: frameprocess
status: active
portability: layer-dependent
public_headers:
    - frameprocess.h
    - frameprocess_data.h
core_files:
    - frameprocess.c
    - frameprocess_data.c
    - frameprocess_pack.c
port_files: []
debug_files: []
depends_on:
    - ../frameparser/frameparser.md
    - ../../tools/ringbuffer/ringbuffer.md
forbidden_depends_on:
    - frameprocess core 直连 UART 或 parser 私有绑定
required_hooks:
    - stFrmProcCfg.getTick
    - stFrmProcCfg.txFrame
    - stFrmProcCfg.protoCfg.getRingBuf
    - stFrmProcCfg.protoCfg.pktLenFunc
    - stFrmProcCfg.protoCfg.crcCalcFunc
optional_hooks:
    - stFrmProcCfg.protoCfg.headLenFunc
    - stFrmProcCfg.protoCfg.getTick
common_utils:
    - tools/ringbuffer
read_next:
    - ../comm.md
    - ../frameparser/frameparser.md
---

# FrameProcess 模块说明

这是当前目录的权威入口文档。

补充阅读文档：当前仓库历史上有把本目录写成“迁移计划”的内容，后续一律以本文件为最终 contract。

## 1. 模块定位

`frameprocess` 负责完整链路的帧接收、payload 结构化解析、发送排队、优先级调度和 ACK 超时重发。它建立在 `frameparser` 之上，但不直接持有底层 UART 或 BSP 细节。

## 2. 目录内文件职责

| 文件 | 职责 |
| --- | --- |
| `frameprocess.h` | 实例枚举、命令枚举、状态码、cfg、ctx、公共 API |
| `frameprocess.c` | RX/TX 主流程、ACK 状态机、双队列调度 |
| `frameprocess_data.h/.c` | payload 与结构体的解析 / 编码 |
| `frameprocess_pack.h/.c` | 业务封装辅助 |
| `frameprocess.md` | 当前目录 contract |

## 3. 对外公共接口

稳定公共头文件：`frameprocess.h`、`frameprocess_data.h`

稳定 API：

- `frmProcGetDefCfg()`
- `frmProcSetCfg()`
- `frmProcInit()`
- `frmProcIsReady()`
- `frmProcProcess()`
- `frmProcPostSelfCheck()`
- `frmProcPostDisconnect()`
- `frmProcPostCprData()`
- `frmProcGetRxStore()` / `frmProcClearRxFlags()`

调用顺序：

1. 先取默认 cfg 并完成必要覆写。
2. `SetCfg()` 后 `Init()`。
3. 周期调用 `frmProcProcess()`。
4. 上层通过 `Post*` 接口写入待发业务数据，通过 `GetRxStore()` 读取接收结果。

## 4. 配置、状态与生命周期

`frameprocess` 是链路 service：

- `Init()` 建立 parser、双队列和 ACK 状态对象。
- `Process()` 负责 `RX -> BuildPendingTx -> AckTimeout -> TX`。
- `unFrmProcRunFlags` 只描述流程状态，不承载业务 payload。
- 当前优先级固定为：`immediateAck -> urgentTxRb -> normalTxRb`。

## 5. 依赖白名单与黑名单

- 允许依赖：`frameparser`、`ringbuffer`、`frameprocess_data`。
- 禁止依赖：在 `frameprocess.c` 中直接 include UART 私有头或 parser 私有绑定头。
- 禁止做法：让 `Post*` 接口直接驱动 UART 发送。

## 6. 函数指针 / port / assembly 契约

| 名称 | 必需/可选 | 由谁实现 | 在哪里被调用 | 原型摘要 | 成功语义 | 失败语义 | 前置条件 | 备注 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `stFrmProcCfg.getTick` | 必需 | 当前工程链路 provider | ACK 超时与流程调度 | `uint32_t (*)(void)` | 返回单调 tick | 缺失则无法初始化 | cfg 已准备 | 用于 ACK 和链路超时 |
| `stFrmProcCfg.txFrame` | 必需 | 当前工程链路 provider | TX 主流程 | `eFrmProcStatus (*)(proc, frameBuf, frameLen)` | 完整帧交给底层发送 | 返回 busy/timeout/error | cfg 已准备 | `Post*` 不直接调用它 |
| `stFrmProcCfg.protoCfg.getRingBuf` | 必需 | 当前工程链路 provider | parser 初始化 | `stRingBuffer *(*)(void *userCtx)` | 返回 RX ring buffer | `NULL` 视为未装配 | protoCfg 已准备 | RX ownership 在 provider 侧 |
| `stFrmProcCfg.protoCfg.pktLenFunc` | 必需 | 协议 provider | parser 解析 | 见 `frameparser` | 返回合法总包长 | `0` 视为不可判定或非法 | 格式合法 | 协议相关逻辑 |
| `stFrmProcCfg.protoCfg.crcCalcFunc` | 必需 | 协议 provider | parser 解析与组包 | 见 `frameparser` | CRC 计算成功 | 结果不匹配则解析失败 | 格式合法 | 接收与发送共用 |

## 7. 公共函数使用契约

| 来源模块 | 公共函数 | 允许在哪些文件调用 | 用途 | 调用前提 | 典型调用顺序 | 返回值处理 | 禁止做法 |
| --- | --- | --- | --- | --- | --- | --- | --- |
| `frameparser` | `frmPsrInitByProtoCfg` / `frmPsrProc` / `frmPsrFreePkt` / `frmPsrMkPkt` | `frameprocess.c` | RX 解析和 TX 组包 | cfg / protoCfg 已准备 | `Init -> Proc -> Free` | 解析失败进入错误或等待态 | 在业务层直接管理 parser 状态 |
| `ringbuffer` | `ringBufferWrite/Read/Peek` | `frameprocess.c` | urgent / normal 队列管理 | 队列已初始化 | `Build -> Enqueue -> Dequeue` | 无空间时返回 `NO_SPACE` 或降级策略 | 直接写裸 payload 不加帧长前缀 |
| `frameprocess_data` | `frmProcDataParseRx` / `frmProcDataBuildTx` | `frameprocess.c` | payload 与结构体转换 | cmd 合法 | `ParseRx` 或 `BuildTx` | `false` 视为 parse/build 失败 | 在 `frameprocess.c` 手写 payload 字节拷贝 |

## 8. 改动落点矩阵

| 需求 | 应改文件 | 不该改的文件 |
| --- | --- | --- |
| 改 ACK、队列调度、实例流程 | `frameprocess.c/.h` | `frameprocess_data.c` payload 细节 |
| 新增命令结构体和 payload 编解码 | `frameprocess_data.*` | `frameprocess.c` 主流程 |
| 改默认协议、tick、发送链路绑定 | 当前工程 cfg / provider | `frameprocess.c` 核心状态机 |

## 9. 复制到其他工程的最小步骤

最小依赖集：`frameprocess.h/.c`、`frameprocess_data.h/.c`、`frameparser`、`ringbuffer`。

外部项目必须补齐：tick hook、完整帧发送函数、parser protoCfg、RX ring buffer provider。若协议不同，还要重写 `frameprocess_data` 的编解码。

## 10. 验证清单

- 单实例下 `RX -> ACK -> TX` 顺序稳定。
- `urgent` 队列不会被 `normal` 队列抢占。
- ACK 超时和重发次数符合配置。
- `Post*` 接口只写数据，不直接驱动底层发送。

