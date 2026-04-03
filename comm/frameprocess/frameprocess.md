# frameprocess 模块计划

## 1. 文档目标

`frameprocess` 用于替代当前 `appcomm` 模块，统一管理协议帧接收、结构化解析、发送调度、优先级队列以及 ACK 超时重发。

本模块的核心设计目标是把“协议解析、业务数据缓存、发送流程控制”拆分为职责清晰的多个文件，避免把接收、处理、发送和重发逻辑集中在单一实现中，降低后续维护成本。

建议按以下分层组织：

- `frameprocess`：核心流程层，负责实例管理、收发状态机、ACK 处理和发送调度。
- `frameprocess_data`：数据转换层，负责命令结构体定义以及结构体与 payload 之间的双向转换。
- `frameprocess_pack`：业务处理层，负责命令相关的业务动作生成。
- `frameprocess_port`：端口适配层，负责绑定 `frameparser`、UART、tick、默认缓冲区和默认协议配置。

按此划分后：

- 新增命令时，优先修改 `frameprocess_data` 或 `frameprocess_pack`。
- 调整板级连接或默认链路时，优先修改 `frameprocess_port`。
- 调整 ACK、实例流程或调度策略时，只修改 `frameprocess` 核心层。

## 2. 替换范围

本模块计划替换当前 `USER/appcomm/appcomm.h` 与 `USER/appcomm/appcomm.c` 的职责，并继续复用以下基础组件：

- `Rep/comm/frameparser`：负责接收帧解析和发送组包。
- `Rep/ringbuffer`：负责双发送队列的底层字节缓冲。
- `Rep/drvlayer/drvuart`：负责底层 UART 收发。
- `Rep/system/systask.c`：继续保留任务创建位置，任务回调改为调用 `frameprocess` 对外接口。

建议分阶段完成替换：

1. 第一阶段仅替换 `appcomm` 逻辑，不立即清理 `frameparser_port` 中已有的 `AppComm` 协议常量。
2. 确认编译和运行稳定后，再进行命名统一和遗留符号清理。

这样可以先保证迁移正确，再处理命名一致性问题。

## 3. 建议文件划分

推荐目录结构如下：

```text
frameprocess/
    frameprocess.h
    frameprocess.c
    frameprocess_data.h
    frameprocess_data.c
    frameprocess_port.h
    frameprocess_port.c
    frameprocess.md
```

各文件职责建议如下。

### 3.1 frameprocess.h

负责定义：

- 逻辑实例枚举 `eFrmProcMapType`
- 命令字枚举 `eFrmProcCmdType`
- 模块状态码 `eFrmProcStatus`
- 收发标志位联合体
- 双发送队列配置结构
- ACK 配置结构和 ACK 运行态结构
- 实例配置结构 `stFrmProcCfg`
- 实例上下文结构 `stFrmProcCtx`
- 对外公共接口声明

### 3.2 frameprocess.c

负责实现：

- 默认配置加载
- 多实例上下文数组管理
- 接收轮询 `frmProcProcessRx()`
- 发送轮询 `frmProcProcessTx()`
- ACK 立即回复路径
- ACK 等待、超时、重发和失败退出
- 双环形缓冲区入队、出队和优先级调度
- 对外发送接口

### 3.3 frameprocess_data.h

负责定义协议相关的数据结构：

- 各命令的接收结构体 `stFrmDataRxXxx`
- 各命令的发送结构体 `stFrmDataTxXxx`
- 接收标志位联合体 `unFrmDataRxFlags`
- 发送标志位联合体 `unFrmDataTxFlags`
- 接收数据存储结构 `stFrmDataRxStore`
- 发送数据存储结构 `stFrmDataTxStore`

### 3.4 frameprocess_data.c

负责实现：

- 命令字到结构体的 payload 解析
- 结构体到 payload 的编码
- 各命令 payload 长度校验
- 接收完成后写入 `rx store`
- 发送前从 `tx store` 读取并序列化

### 3.5 frameprocess_port.h

负责声明：

- 默认实例配置
- `frameparser` 默认协议配置获取接口
- UART 发送适配接口
- tick 获取接口
- 默认 ring buffer 存储区获取接口
- 默认 `urgent` 和 `normal` 队列容量配置

### 3.6 frameprocess_port.c

负责实现：

- 复用 `frmPsrPortGetDefProtoCfg()` 加载默认协议配置
- 绑定调试 UART 接收缓冲
- 提供 UART 发送适配函数
- 提供毫秒 tick 获取函数
- 提供各实例的 `urgent` 和 `normal` 发送缓冲区

## 4. 多实例模型

建议通过枚举管理多个 `frameprocess` 实例，例如：

```c
typedef enum eFrmProcMap {
    FRAME_PROC0 = 0,
    FRAME_PROC1,
    FRAME_PROC_MAX,
} eFrmProcMapType;
```

每个实例维护一套独立上下文，至少包含：

- 一个 `frameparser` 实例
- 一个接收数据存储区
- 一个发送数据存储区
- 一个 `urgent` 发送 ring buffer
- 一个 `normal` 发送 ring buffer
- 一个 ACK 运行态对象
- 一个当前发送槽

这样当链路从单路调试 UART 扩展到蓝牙串口或第二路串口时，只需要在 `port` 层补充默认绑定，不需要重写核心流程。

## 5. 协议数据层设计

### 5.1 命令枚举

建议统一使用命令字枚举，不在流程代码中直接使用裸值：

```c
typedef enum eFrmProcCmd {
    FRM_PROC_CMD_HANDSHAKE = 0x01,
    FRM_PROC_CMD_HEARTBEAT = 0x03,
    FRM_PROC_CMD_DISCONNECT = 0x04,
    FRM_PROC_CMD_SELF_CHECK = 0x05,
    FRM_PROC_CMD_GET_DEVICE_INFO = 0x11,
    FRM_PROC_CMD_GET_BLE_INFO = 0x13,
    FRM_PROC_CMD_CPR_DATA = 0x31,
} eFrmProcCmdType;
```

### 5.2 接收结构体和发送结构体分离

建议按数据方向拆分结构体，避免同一个结构体同时表示“收到的数据”和“待发送的数据”。

示例：

```c
typedef struct stFrmDataRxHandshake {
    uint8_t cipher[16];
} stFrmDataRxHandshake;

typedef struct stFrmDataTxHandshake {
    uint8_t macString[12];
} stFrmDataTxHandshake;
```

其他命令建议保持同样风格，例如：

- `stFrmDataRxHeartbeat`
- `stFrmDataTxHeartbeat`
- `stFrmDataRxDisconnect`
- `stFrmDataTxDisconnect`
- `stFrmDataTxSelfCheck`
- `stFrmDataTxDeviceInfo`
- `stFrmDataTxBleInfo`
- `stFrmDataTxCprData`

### 5.3 数据存储区

建议在 `frameprocess_data.h` 中集中定义接收与发送数据存储区：

```c
typedef struct stFrmDataRxStore {
    unFrmDataRxFlags flags;
    stFrmDataRxHandshake handshake;
    stFrmDataRxHeartbeat heartbeat;
    stFrmDataRxDisconnect disconnect;
    stFrmDataRxGetDeviceInfo getDeviceInfo;
    stFrmDataRxGetBleInfo getBleInfo;
} stFrmDataRxStore;

typedef struct stFrmDataTxStore {
    unFrmDataTxFlags flags;
    stFrmDataTxHandshake handshake;
    stFrmDataTxHeartbeat heartbeat;
    stFrmDataTxDisconnect disconnect;
    stFrmDataTxSelfCheck selfCheck;
    stFrmDataTxDeviceInfo deviceInfo;
    stFrmDataTxBleInfo bleInfo;
    stFrmDataTxCprData cprData;
} stFrmDataTxStore;
```

建议遵循以下约束：

- 接收解析成功后，先写 `rx store`，再置对应 `rx flag`
- 发送接口被调用后，先写 `tx store`，再置对应 `tx flag`
- `process` 层只根据标志决定流程，不直接处理具体 payload 字段拷贝

## 6. 标志位联合体设计

发送和接收都可以使用联合体 bit 位表示状态，建议分为“数据标志”和“流程标志”两类。

### 6.1 接收标志位

```c
typedef union unFrmDataRxFlags {
    uint32_t value;
    struct {
        uint32_t handshake : 1;
        uint32_t heartbeat : 1;
        uint32_t disconnect : 1;
        uint32_t getDeviceInfo : 1;
        uint32_t getBleInfo : 1;
        uint32_t ackFrame : 1;
        uint32_t reserved : 26;
    } bits;
} unFrmDataRxFlags;
```

### 6.2 发送标志位

```c
typedef union unFrmDataTxFlags {
    uint32_t value;
    struct {
        uint32_t handshake : 1;
        uint32_t heartbeat : 1;
        uint32_t disconnect : 1;
        uint32_t selfCheck : 1;
        uint32_t deviceInfo : 1;
        uint32_t bleInfo : 1;
        uint32_t cprData : 1;
        uint32_t ackPending : 1;
        uint32_t reserved : 24;
    } bits;
} unFrmDataTxFlags;
```

此外，建议增加流程运行标志位联合体，用于表示：

- `isInit`
- `isLinkUp`
- `isTxBusy`
- `isWaitingAck`
- `hasImmediateAck`

这样可以把业务数据标志和流程控制标志分开管理，减少状态耦合。

## 7. 接收流程设计

建议将接收流程固定为以下顺序：

1. `frmProcProcessRx()` 调用 `frmPsrProc()`，从 `frameparser` 获取完整帧
2. 读取完整帧中的 `cmd`、`payloadLen` 和原始帧缓存
3. 如果命令需要立即 ACK，则先生成 ACK 发送动作
4. 调用 `frameprocess_data.c` 的解析函数，将 payload 写入对应接收结构体
5. 置对应 `rx flag`
6. 根据命令触发业务处理或生成对应发送数据
7. 调用 `frmPsrFreePkt()` 释放当前包

这里的职责边界应保持明确：

- `process` 负责流程控制
- `data` 负责 payload 与结构体之间的转换
- 不应在 `process` 中手写各命令的字节拷贝细节

## 8. 发送流程设计

建议把发送流程拆成三个阶段。

### 8.1 上层写入发送结构体

例如：

- `frmProcPostSelfCheck()`
- `frmProcPostDisconnect()`
- `frmProcPostCprData()`

这些接口只负责三件事：

1. 校验参数
2. 将业务数据写入对应 `tx struct`
3. 置对应 `tx flag`，并指定优先级 `urgent` 或 `normal`

### 8.2 process 编码待发数据

`frmProcProcessTx()` 中建议执行以下动作：

1. 扫描待发 `tx flag`
2. 调用 `frameprocess_data.c` 将结构体编码为 payload
3. 调用 `frmPsrPortMkPkt()` 生成完整协议帧
4. 将完整帧写入 `urgent` 或 `normal` ring buffer

### 8.3 调度器发送完整帧

建议发送优先级固定为：

1. 立即 ACK 槽
2. `urgent` ring buffer
3. `normal` ring buffer

只要 `urgent` 队列中仍有待发帧，`normal` 队列就不应抢先发送。

## 9. 双环形缓冲区设计

### 9.1 使用双队列的原因

如果只保留单一发送队列，CPR 上报等普通数据可能阻塞握手回复、断开回复或 ACK 帧，导致上位机误判超时。因此发送路径需要显式区分高优先级和普通优先级。

### 9.2 建议队列模型

每个实例维护两个字节 ring buffer：

- `urgentTxRb`：容量较小，例如 256 或 512 字节
- `normalTxRb`：容量较大，例如 1024 或 2048 字节

两个队列都不直接存裸 payload，而是存完整帧记录：

```text
[frameLen high][frameLen low][frame bytes...]
```

这样出队时无需重新组包，ACK 重发时也可以直接复用原始帧缓存。

### 9.3 入队规则

- 握手回复、心跳回复、断开回复和 ACK 帧默认进入 `urgent`
- CPR 数据、自检上报、设备信息等默认进入 `normal`
- 上层可根据接口参数显式指定优先级

### 9.4 出队规则

调度器每次发送时按以下顺序检查：

1. 是否存在立即 ACK
2. 是否存在 `urgentTxRb` 数据
3. 是否存在 `normalTxRb` 数据

每次只发送一帧，待 UART 空闲后再继续发送下一帧，避免多个缓存源并发争用同一发送通道。

## 10. ACK 设计

### 10.1 ACK 基本规则

ACK 建议分为“接收端立即回复 ACK”和“发送端等待 ACK”两部分。

接收端规则：

- 当收到的帧满足 ACK 条件时，立即回复一帧匹配的 ACK 数据

发送端规则：

- 首次发送后进入 `waitingAck`
- 若 100 ms 内未收到匹配 ACK，则重发一次
- 再过 100 ms 仍未收到，则再重发一次
- 总发送次数达到 3 次后仍未收到 ACK，则记失败并退出等待状态

### 10.2 ACK 运行态

建议单独定义 ACK 运行态结构：

```c
typedef struct stFrmProcAckState {
    uint8_t frameBuf[FRM_PROC_MAX_PKT_LEN];
    uint16_t frameLen;
    uint32_t sendTickMs;
    uint8_t retryCount;
    uint8_t maxRetryCount;
    uint16_t timeoutMs;
    bool isWaiting;
} stFrmProcAckState;
```

该结构只保存当前等待 ACK 的单帧信息。对于单串口链路，这样的模型更简单，也更容易保证状态一致性。

### 10.3 ACK 匹配约束

这里需要先明确 ACK 判定条件。

当前协议文档中：

- `Byte0~1` 为帧头
- `Byte2` 为版本号，当前固定为 `0x01`
- `Byte3` 为命令字

因此，如果直接按“第 3 个 byte 为 `0x01` 时回复 ACK”理解，现有协议会出现语义冲突，因为版本字段本身固定就是 `0x01`。

建议当前方案按以下前提执行：

- ACK 判定条件以 `cmd == 0x01` 为准，即握手命令需要 ACK

如果 ACK 条件确实需要按 `Byte2` 判定，则应先修订协议字段定义，否则无法准确区分普通帧和需 ACK 帧。

### 10.4 ACK 立即回复路径

为满足立即回复要求，建议不要让 ACK 回复帧完全走普通排队路径，而是增加 `immediateAck` 槽：

- 收到需 ACK 的帧后，先把原始帧复制到 `immediateAckBuf`
- `frmProcProcessTx()` 每轮优先检查 `immediateAckBuf`
- UART 空闲时直接发送
- UART 忙时降级写入 `urgentTxRb`

这样既能保证 ACK 优先级，又能兼容底层 DMA 忙碌场景。

## 11. 建议公共 API

对外接口建议保持统一风格，例如：

```c
eFrmProcStatus frmProcGetDefCfg(eFrmProcMapType proc, stFrmProcCfg *cfg);
eFrmProcStatus frmProcSetCfg(eFrmProcMapType proc, const stFrmProcCfg *cfg);
eFrmProcStatus frmProcInit(eFrmProcMapType proc);
bool frmProcIsReady(eFrmProcMapType proc);
void frmProcProcess(eFrmProcMapType proc);

eFrmProcStatus frmProcPostSelfCheck(eFrmProcMapType proc, const stFrmDataTxSelfCheck *data, bool isUrgent);
eFrmProcStatus frmProcPostDisconnect(eFrmProcMapType proc, bool isUrgent);
eFrmProcStatus frmProcPostCprData(eFrmProcMapType proc, const stFrmDataTxCprData *data, bool isUrgent);

const stFrmDataRxStore *frmProcGetRxStore(eFrmProcMapType proc);
void frmProcClearRxFlags(eFrmProcMapType proc, uint32_t flags);
```

接口约束建议如下：

- `frmProcProcess()` 内部分别调用 `frmProcProcessRx()` 和 `frmProcProcessTx()`
- `Post` 类接口只负责写入发送数据，不直接驱动 UART
- 上层如果只关心某类接收结果，可只读取 `RxStore` 和 `RxFlags`

## 12. process 内部主流程

建议 `frmProcProcess()` 按固定顺序组织：

```text
frmProcProcess()
    -> frmProcProcessRx()
    -> frmProcBuildPendingTx()
    -> frmProcProcessAckTimeout()
    -> frmProcProcessTx()
```

各步骤职责如下：

- `frmProcProcessRx()`：从 `frameparser` 获取完整帧并写入 `rx store`
- `frmProcBuildPendingTx()`：把 `tx store` 中待发数据编码并入队
- `frmProcProcessAckTimeout()`：处理 ACK 超时和重发
- `frmProcProcessTx()`：按 `immediateAck`、`urgent`、`normal` 顺序发包

## 13. 与现有 system 的对接方式

当前 `systask.c` 中已有独立的 `appCommTaskCallback()`。迁移到 `frameprocess` 后，建议按以下顺序对接：

1. 初始化阶段将 `initializeAppComm()` 替换为 `initializeFrameProcess()`
2. 任务回调中调用 `frmProcProcess(FRAME_PROC0)`
3. 逐步将 `appCommSendCprData()` 的调用点替换为 `frmProcPostCprData()`
4. 验证通过后再删除旧 `appcomm` 模块

这样可以把改动范围限制在对接层，降低迁移风险。

## 14. 实施顺序建议

建议按以下顺序落地，避免一次性改动过大。

### 第一步

先建立 `frameprocess.h`、`frameprocess.c`、`frameprocess_port.h`、`frameprocess_port.c`、`frameprocess_data.h`、`frameprocess_data.c` 基本骨架，先保证接口可编译。

### 第二步

将 `appcomm` 已有命令迁移到 `frameprocess_data`，优先覆盖以下命令：

- 握手
- 心跳
- 断开
- 自检
- 设备信息
- BLE 信息
- CPR 数据

### 第三步

实现双队列发送和 ACK 状态机，初期只接入一条链路 `FRAME_PROC0`，确认流程稳定后再扩展到多实例。

### 第四步

切换 `systask.c` 调用入口，从 `appCommProcess()` 改到 `frmProcProcess()`。

### 第五步

验证协议联调稳定后，再移除 `appcomm`。

## 15. 风险点

当前阶段最需要提前锁定的风险点如下：

- ACK 判定字段到底是 Byte2 还是 Byte3，需要先统一。
- ACK 回包是否“完全镜像原帧”，如果是，收到的 CRC 也应原样带回，不应重新组包。
- urgent 队列容量过小时，连续 ACK 或连续心跳回复可能顶满队列。
- 如果底层 UART DMA 是异步发送，需要明确“发送完成”判断接口，否则调度器可能重复发同一帧。
- `frameparser_port` 当前默认协议名和常量仍偏 `appcomm`，第一阶段可以复用，第二阶段建议重命名为通用命名。

## 16. 本计划的结论

`frameprocess` 最适合按“core + data + port”的方式落地，而不是继续做成单文件协议模块。这样既满足你提出的：

- 多实例
- 结构体化收发
- 基于 `frameparser` 的解析与组包
- 联合体 bit 标志位分发
- urgent/normal 双环形缓冲区
- ACK 立即回复与 100ms 重发

也符合当前仓库对模块分层、端口适配、数据结构定义位置和后续可扩展性的要求。

如果下一步开始实现，建议先从 `frameprocess.h`、`frameprocess_data.h` 和 `frameprocess_port.h` 三个头文件定契约，再写 `frameprocess.c` 主流程。这样可以先把边界定死，减少中途返工。
