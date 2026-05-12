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
port_files:
    - ../../../User/port/frameparser_port.h
    - ../../../User/port/frameparser_port.c
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

`frameparser` core 只接受通用 `protocolId` 作为平台默认协议查询键，不在 `rep/` 内定义任何项目私有协议枚举；具体协议编号由外部工程的 port/provider 层通过 `stFrmPsrOps` 定义。

协议 provider 可以为一个协议配置最多 5 个包头模式；这些包头长度必须一致，只允许内容不同。解析器按统一包头长度匹配，并保留尾部半包头以等待后续字节补齐。

## 2. 目录内文件职责

| 文件 | 职责 |
| --- | --- |
| `framepareser.h` | 解析状态、协议配置和 4 个公共函数 |
| `framepareser.c` | 包头搜索、长度判断、CRC 校验和轮询解析 |
| `User/port/frameparser_port.h` | 项目侧 provider 头，声明 `frmPsrPortGetOps()` |
| `User/port/frameparser_port.c` | 项目侧 provider 实现，提供 `stFrmPsrOps`、默认 tick 和各协议 `protoCfg` 装配 |
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
| `stFrmPsrOps::loadDefaultProtoCfg` | 必需 | 项目侧 `frameparser_port.*` provider | `frmPsrInit()` | `void (*)(protocolId, protoCfg)` | 按协议号填充默认 parser 配置 | 保持零值表示该协议不可用 | `protoCfg != NULL` | 类似 `rtos` 的 ops provider，不再使用 weak hook |
| `stFrmPsrOps::getTickMs` | 可选 | 项目侧 `frameparser_port.*` provider | `frmPsrProcess()` 超时判断 | `uint32_t (*)(void)` | 返回单调 tick | 缺失则等待超时功能退化为关闭 | 配置了 `waitPktToutMs` | 平台时钟属于 ops，不再放进 `protoCfg` |
| `frmPsrPktLenFunc` | 必需 | 协议 provider | 接收解析流程 | `uint32_t (*)(buf, headLen, availLen, userCtx)` | 返回合法总包长 | 返回 `0` 视为不可判定或非法 | 包头已基本命中 | 长度必须包含头、负载、CRC |
| `frmPsrCrcCalcFunc` | 必需 | 协议 provider | CRC 校验 | `uint32_t (*)(buf, len, userCtx)` | 返回 CRC 值 | 错误时由上层协议判定失败 | CRC 范围合法 | |
| `frmPsrHeadLenFunc` | 可选 | 协议 provider | 变长包头判断 | `uint32_t (*)(buf, availLen, userCtx)` | 返回合法包头长度 | 返回 `0` 视为仍需更多字节 | 协议存在变长头 | 固定头协议可省略 |

### 6.1 最小 `frameparser_port.c` 骨架

下面这份骨架应当能作为项目侧 `User/port/frameparser_port.c` 的最小起点：

```c
#include "frameparser_port.h"

#include <string.h>

static uint32_t frameparserPortGetTickMsImpl(void);
static void frameparserPortLoadDefaultProtoCfgImpl(uint32_t protocolId, stFrmPsrProtoCfg *protoCfg);

static const stFrmPsrOps gFrameparserPortOps = {
    .getTickMs = frameparserPortGetTickMsImpl,
    .loadDefaultProtoCfg = frameparserPortLoadDefaultProtoCfgImpl,
};

static uint32_t frameparserPortGetTickMsImpl(void)
{
    return 0U;
}

static void frameparserPortLoadDefaultProtoCfgImpl(uint32_t protocolId, stFrmPsrProtoCfg *protoCfg)
{
    if (protoCfg == NULL) {
        return;
    }

    (void)memset(protoCfg, 0, sizeof(*protoCfg));

    switch (protocolId) {
        case DEMO_PROTOCOL_ID:
            protoCfg->headPatList[0] = gDemoHead;
            protoCfg->headPatCount = 1U;
            protoCfg->headPatLen = 2U;
            protoCfg->minHeadLen = 4U;
            protoCfg->minPktLen = 6U;
            protoCfg->maxPktLen = 64U;
            protoCfg->crcRangeStartOff = 0;
            protoCfg->crcRangeEndOff = -2;
            protoCfg->crcFieldOff = -1;
            protoCfg->crcFieldLen = 1U;
            protoCfg->crcFieldEnd = FRM_PSR_CRC_END_LITTLE;
            protoCfg->pktLenFunc = demoPktLen;
            protoCfg->crcCalcFunc = demoCrc;
            break;
        default:
            break;
    }
}

const stFrmPsrOps *frmPsrPortGetOps(void)
{
    return &gFrameparserPortOps;
}
```

使用要求：

- `gFrameparserPortOps` 必须是长期有效的静态对象，不能返回栈上临时变量。
- `frmPsrPortGetOps()` 必须始终返回同一张 `ops` 表或另一张同样长期有效的静态表。
- `loadDefaultProtoCfg()` 开头必须先清零 `*protoCfg`，未知 `protocolId` 直接保持零值返回。
- `getTickMs()` 若无法提供单调 tick，可返回 `0U`；这会让超时逻辑自动退化为关闭。

### 6.2 `loadDefaultProtoCfg()` 必填字段检查表

`loadDefaultProtoCfg()` 只要命中了某个已知 `protocolId`，至少要把下面字段配置到可通过 `frmPsrIsProtoCfgValid()` 的程度：

| 字段 | 是否必填 | 约束 | 说明 |
| --- | --- | --- | --- |
| `headPatList[0..headPatCount-1]` | 必填 | 每项非 `NULL` | 指向包头模式字节数组 |
| `headPatCount` | 必填 | `1..FRM_PSR_HEAD_PAT_MAX` | 包头模式数量 |
| `headPatLen` | 必填 | `> 0` | 所有包头模式长度必须一致 |
| `minHeadLen` | 必填 | `>= headPatLen` | 固定头或最小头长 |
| `minPktLen` | 必填 | `>= minHeadLen` | 最小整包长度 |
| `maxPktLen` | 必填 | `>= minPktLen` | 最大整包长度，同时受 frameBuf 限制 |
| `pktLenFunc` | 必填 | 非 `NULL` | 从头部/已有字节判断整包长度 |
| `crcCalcFunc` | 必填 | 非 `NULL` | 计算 CRC 的函数 |
| `crcFieldLen` | 必填 | `1..4` | CRC 字段字节数 |
| `crcFieldOff` | 必填 | 必须能定位到整包内合法范围 | 支持负偏移 |
| `crcRangeStartOff` | 必填 | 必须能定位 | CRC 计算起点 |
| `crcRangeEndOff` | 必填 | 必须能定位且不早于 start | CRC 计算终点 |
| `crcFieldEnd` | 必填 | `LITTLE` 或 `BIG` | CRC 字段字节序 |
| `cmdindex/cmdLen` | 按需 | 若启用则必须落在 `minHeadLen` 内 | 用于 ready 包命令字段视图 |
| `packlenindex/packlenLen` | 按需 | 若启用则必须落在 `minHeadLen` 内 | 用于 ready 包长度字段视图 |
| `headLenFunc` | 可选 | 变长头时非 `NULL` | 固定头协议可省略 |
| `waitPktToutMs` | 可选 | `0` 表示关闭超时 | 依赖 `ops->getTickMs()` |
| `userCtx` | 可选 | 可为 `NULL` | 透传给协议函数 |

快速判断规则：

- 只填 `headPat` 和 `pktLenFunc` 还不够，缺 `crcCalcFunc` 或 CRC 字段配置会直接判无效。
- 若协议无命令字段或包长字段，可把对应长度保持为 `0`。
- 若协议是固定包头，`headLenFunc` 可以不填，core 会退回使用 `headPatLen`。

### 6.3 多 `protocolId` 分发规则

一个工程内允许多个协议共用同一个 `frameparser_port.c`，推荐按下面规则实现：

| 场景 | 推荐做法 | 不推荐做法 |
| --- | --- | --- |
| 单协议工程 | `switch` 只保留一个 `case`，未知协议清零返回 | 为单协议额外拆多张 `ops` 表 |
| 多协议工程 | 在 `loadDefaultProtoCfg()` 中按 `protocolId` 分发不同装配分支 | 在 `rep/` core 中写协议专属 `if/else` |
| 未知协议 | 保持 `protoCfg` 全零返回，让 `frmPsrInit()` 返回 `FRM_PSR_PROTO_INVALID` | 默认回退到某个“猜测协议” |
| 共用时钟 | 所有协议共用同一个 `getTickMs()` provider | 把 tick 回调塞回每个 `protoCfg` |

分发实现要求：

- 先统一清零 `*protoCfg`，再进入 `switch (protocolId)`。
- 每个 `case` 只负责本协议的字段装配，不要跨协议复用残留字段。
- 若多个协议共享同一套 CRC/长度函数，可以共享静态函数，但仍要在各自分支中显式填字段。
- 若协议数量变多，允许把每个协议的装配逻辑拆成 `frameparserPortLoadXxxProtoCfg()` 静态函数，但 `frmPsrPortGetOps()` 入口保持不变。

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

最小依赖集：`framepareser.h/.c`、`ringbuffer`、项目侧 `frameparser_port.*` provider。

外部项目必须补齐：`frmPsrPortGetOps()`、协议默认配置装配；若协议需要默认超时基准，再补 `getTickMs`。

项目侧落地文件最少应包含：`User/port/frameparser_port.h`、`User/port/frameparser_port.c`。

## 10. 验证清单

- 包头错位时能稳定重同步。
- `NEED_MORE_DATA` 不会破坏已缓存字节流。
- CRC 失败只丢弃最小必要字节。
- ready 包在 `frmPsrRelease()` 前不会被下一包覆盖。