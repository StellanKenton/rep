---
doc_role: module-spec
layer: comm
module: frameparser
status: active
portability: layer-dependent
public_headers:
    - framepareser.h
core_files:
    - framepareser.c
port_files: []
debug_files: []
depends_on:
    - ../../tools/ringbuffer/ringbuffer.md
forbidden_depends_on:
    - parser core 直连 UART 或协议私有实现
required_hooks:
    - frmPsrPktLenFunc
    - frmPsrCrcCalcFunc
optional_hooks:
    - frmPsrHeadLenFunc
    - frmPsrGetTickFunc
common_utils:
    - tools/ringbuffer
copy_minimal_set:
    - framepareser.h
    - framepareser.c
    - tools/ringbuffer/
read_next:
    - ../comm.md
    - ../../tools/ringbuffer/ringbuffer.md
---

# FrameParser 模块说明

这是当前目录的权威入口文档。

## 1. 模块定位

`frameparser` 负责从字节流中重组一帧完整协议包。它不解释业务命令，也不直接管理链路驱动。

`frameparser` core 只接受通用 `protocolId` 作为平台默认协议查询键，不在 `rep/` 内定义任何项目私有协议枚举；具体协议编号由外部工程的 port/provider 层定义。

协议 provider 可以为一个协议配置最多 5 个包头模式；这些包头长度必须一致，只允许内容不同。解析器按统一包头长度匹配，并保留尾部半包头以等待后续字节补齐。

## 2. 目录内文件职责

| 文件 | 职责 |
| --- | --- |
| `framepareser.h` | 解析状态、协议配置和 4 个公共函数 |
| `framepareser.c` | 包头搜索、长度判断、CRC 校验和轮询解析 |
| `frameparser.md` | 当前目录 contract |

## 3. 对外公共接口

稳定公共头文件：`framepareser.h`

稳定 API：

- `frmPsrInit()`
- `frmPsrFeed()`
- `frmPsrProcess()`
- `frmPsrRelease()`：返回当前 ready 包的 `stFrmPsrPkt` 视图指针

调用顺序：

1. 先准备 `stFrmPsrCfg`，其中包括协议编号、输入流缓冲区和帧缓冲区。
2. `Init` 后通过 `frmPsrFeed()` 把收到的字节流写入 `stFrmPsr`。
3. 周期调用 `frmPsrProcess()`；当返回 `FRM_PSR_OK` 且 `psr->hasReadyPkt == true` 时，当前完整帧保存在 `psr->pkt`。
4. 业务层消费完成后调用 `frmPsrRelease()` 获取 `stFrmPsrPkt` 视图；该视图包含整包指针、包头指针、数据区指针、CRC 指针、命令字段指针、包长字段指针，以及包头长度、数据长度、CRC 长度、命令字段偏移/长度、包长字段偏移/长度、CRC 偏移和整包长度。调用后解析器会解除当前 ready 锁定，允许继续向后解析新数据。

## 4. 配置、状态与生命周期

- 运行态包含内嵌 ring buffer、当前协议配置、pending 包状态和 ready 状态。
- `frmPsrProcess()` 在没有完整包时返回 `EMPTY` 或 `NEED_MORE_DATA`，不应破坏上层流程状态。
- `cmdindex/cmdLen/packlenindex/packlenLen` 被视为包头内字段描述，配置时必须落在 `minHeadLen` 范围内；ready 包会把这两个字段映射到 `stFrmPsrPkt` 视图。
- 上层在未调用 `frmPsrRelease()` 前，不应继续消费下一包；`frmPsrRelease()` 返回的 `stFrmPsrPkt` 视图在下一次 `frmPsrProcess()` 产生新包前有效。

## 5. 依赖白名单与黑名单

- 允许依赖：`ringbuffer`。
- 禁止依赖：在 parser core 中直连 UART、tick 私有实现或业务协议逻辑。
- 禁止做法：把发送和接收格式硬编码到核心流程中。

## 6. 函数指针 / port / assembly 契约

| 名称 | 必需/可选 | 由谁实现 | 在哪里被调用 | 原型摘要 | 成功语义 | 失败语义 | 前置条件 | 备注 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `frmPsrPktLenFunc` | 必需 | 协议 provider | 接收解析流程 | `uint32_t (*)(buf, headLen, availLen, userCtx)` | 返回合法总包长 | 返回 `0` 视为不可判定或非法 | 包头已基本命中 | 长度必须包含头、负载、CRC |
| `frmPsrCrcCalcFunc` | 必需 | 协议 provider | CRC 校验 | `uint32_t (*)(buf, len, userCtx)` | 返回 CRC 值 | 错误时由上层协议判定失败 | CRC 范围合法 | |
| `frmPsrHeadLenFunc` | 可选 | 协议 provider | 变长包头判断 | `uint32_t (*)(buf, availLen, userCtx)` | 返回合法包头长度 | 返回 `0` 视为仍需更多字节 | 协议存在变长头 | 固定头协议可省略 |
| `frmPsrGetTickFunc` | 可选 | 协议 provider | 等待完整包超时控制 | `uint32_t (*)(void)` | 返回单调 tick | 缺失则不支持等待超时 | 配置需要等待超时 | 与 `waitPktToutMs` 联动 |

## 7. 公共函数使用契约

| 来源模块 | 公共函数 | 允许在哪些文件调用 | 用途 | 调用前提 | 典型调用顺序 | 返回值处理 | 禁止做法 |
| --- | --- | --- | --- | --- | --- | --- | --- |
| `ringbuffer` | `ringBufferPeek/Read/Discard/GetUsed` | `framepareser.c` | 搜索包头、读取完整包、丢弃无效字节 | ring buffer 已初始化 | `Peek -> Validate -> Read/Discard` | 不足数据时返回等待态 | 绕过 API 直接改 head/tail |

## 8. 改动落点矩阵

| 需求 | 应改文件 | 不该改的文件 |
| --- | --- | --- |
| 改包头搜索、CRC、长度处理 | `framepareser.c/.h` | 上层链路流程 |
| 改具体协议格式 | protoCfg provider | parser core 通用流程 |

## 9. 复制到其他工程的最小步骤

最小依赖集：`framepareser.h/.c`、`ringbuffer`、外部协议配置回调实现。

外部项目必须补齐：包长函数、CRC 计算函数；若协议是变长头或有超时，再补头长函数和 tick hook。

## 10. 验证清单

- 包头错位时能稳定重同步。
- `NEED_MORE_DATA` 不会破坏已缓存字节流。
- CRC 失败只丢弃最小必要字节。
- ready 包在 `frmPsrRelease()` 前不会被下一包覆盖。