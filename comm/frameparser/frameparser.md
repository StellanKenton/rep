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
    - frmPsrTxHeadBuildFunc
    - frmPsrTxPktFinFunc
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

`frameparser` 负责从字节流 ring buffer 中重组一帧完整协议包，并在发送侧按格式对象生成完整数据包。它不解释业务命令，也不直接管理链路驱动。

## 2. 目录内文件职责

| 文件 | 职责 |
| --- | --- |
| `framepareser.h` | 解析状态、格式对象、运行配置和公共 API |
| `framepareser.c` | 包头搜索、长度判断、CRC 校验、组包 |
| `frameparser.md` | 当前目录 contract |

## 3. 对外公共接口

稳定公共头文件：`framepareser.h`

稳定 API：

- `frmPsrInit()` / `frmPsrInitByProtoCfg()` / `frmPsrInitFmt()`
- `frmPsrReset()` / `frmPsrProc()`
- `frmPsrSelFmt()` / `frmPsrGetFmt()`
- `frmPsrMkPkt()` / `frmPsrMkPktByFmt()`
- `frmPsrHasPkt()` / `frmPsrGetPkt()` / `frmPsrFreePkt()`

调用顺序：

1. 先准备 ring buffer、格式对象和运行配置。
2. `Init` 后周期调用 `frmPsrProc()`。
3. 成功取到包后，业务层消费完成再 `frmPsrFreePkt()`。

## 4. 配置、状态与生命周期

- 运行态包含 ring buffer、当前格式、pending 包状态和 ready 状态。
- `frmPsrProc()` 在没有完整包时返回 `EMPTY` 或 `NEED_MORE_DATA`，不应破坏上层流程状态。
- 上层在未释放当前 ready 包前，不应继续消费下一包。

## 5. 依赖白名单与黑名单

- 允许依赖：`ringbuffer`。
- 禁止依赖：在 parser core 中直连 UART、tick 私有实现或业务协议逻辑。
- 禁止做法：把发送和接收格式硬编码到核心流程中。

## 6. 函数指针 / port / assembly 契约

| 名称 | 必需/可选 | 由谁实现 | 在哪里被调用 | 原型摘要 | 成功语义 | 失败语义 | 前置条件 | 备注 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `frmPsrPktLenFunc` | 必需 | 协议 format provider | 接收解析流程 | `uint32_t (*)(buf, headLen, availLen, userCtx)` | 返回合法总包长 | 返回 `0` 视为不可判定或非法 | 包头已基本命中 | 长度必须包含头、负载、CRC |
| `frmPsrCrcCalcFunc` | 必需 | 协议 format provider | CRC 校验与发包 | `uint32_t (*)(buf, len, userCtx)` | 返回 CRC 值 | 错误时由上层格式判定失败 | CRC 范围合法 | 接收与发送可共用 |
| `frmPsrHeadLenFunc` | 可选 | 协议 format provider | 变长包头判断 | `uint32_t (*)(buf, availLen, userCtx)` | 返回合法包头长度 | 返回 `0` 视为仍需更多字节 | 协议存在变长头 | 固定头协议可省略 |
| `frmPsrGetTickFunc` | 可选 | 运行配置 provider | 等待完整包超时控制 | `uint32_t (*)(void)` | 返回单调 tick | 缺失则不支持等待超时 | 配置需要等待超时 | 与 `waitPktToutMs` 联动 |

## 7. 公共函数使用契约

| 来源模块 | 公共函数 | 允许在哪些文件调用 | 用途 | 调用前提 | 典型调用顺序 | 返回值处理 | 禁止做法 |
| --- | --- | --- | --- | --- | --- | --- | --- |
| `ringbuffer` | `ringBufferPeek/Read/Discard/GetUsed` | `framepareser.c` | 搜索包头、读取完整包、丢弃无效字节 | ring buffer 已初始化 | `Peek -> Validate -> Read/Discard` | 不足数据时返回等待态 | 绕过 API 直接改 head/tail |

## 8. 改动落点矩阵

| 需求 | 应改文件 | 不该改的文件 |
| --- | --- | --- |
| 改包头搜索、CRC、长度处理 | `framepareser.c/.h` | 上层链路流程 |
| 改具体协议格式 | format / protoCfg provider | parser core 通用流程 |
| 改发送字段回填 | Tx format 回调 | 接收解析主流程 |

## 9. 复制到其他工程的最小步骤

最小依赖集：`framepareser.h/.c`、`ringbuffer`、外部协议格式回调实现。

外部项目必须补齐：包长函数、CRC 计算函数；若协议是变长头或有超时，再补头长函数和 tick hook。

## 10. 验证清单

- 包头错位时能稳定重同步。
- `NEED_MORE_DATA` 不会破坏已缓存字节流。
- CRC 失败只丢弃最小必要字节。
- ready 包在 `frmPsrFreePkt()` 前不会被下一包覆盖。