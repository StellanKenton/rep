# DrvIic 模块设计说明

本文档描述 `drviic` 当前的 core / port / BSP 分工，以及 `bsphardiic.c/.h` 需要提供怎样的行为，才能满足公共层的调用契约。

## 1. 模块定位

`drviic` 是硬件 IIC 主机驱动的公共层。它不直接关心 GD32、I2C0、PB6/PB7 这些板级信息，只关心“某个逻辑 IIC 总线能否完成一次完整事务”。

公共层负责:

- 参数校验。
- 初始化状态管理。
- 总线互斥与忙状态保护。
- 默认超时归一化。
- 通用读写和寄存器访问 helper。

BSP 层负责:

- 时钟、引脚、滤波、速度配置。
- 起始、停止、地址、读写方向位等控制器细节。
- 超时、NACK、总线错误到 `eDrvStatus` 的归一化返回。

## 2. 目录内文件职责

- `drviic.h`: 定义 `stDrvIicTransfer`、BSP 钩子类型和公共 API。
- `drviic.c`: 实现参数检查、互斥、事务封装和 helper 接口。
- `drviic_port.h`: 定义逻辑总线枚举和默认超时宏。
- `drviic_port.c`: 维护 `gDrvIicBspInterface` 默认绑定表。
- `bsphardiic.h/.c`: 硬件 IIC 控制器的实际实现。

## 3. core 层实际依赖的 BSP 接口

`drviic.c` 通过 `gDrvIicBspInterface[DRVIIC_MAX]` 访问 BSP。每个逻辑总线的接口项定义如下:

```c
typedef struct stDrvIicBspInterface {
	drvIicBspInitFunc init;
	drvIicBspTransferFunc transfer;
	drvIicBspRecoverBusFunc recoverBus;
	uint32_t defaultTimeoutMs;
} stDrvIicBspInterface;
```

其中:

- `init` 是必需钩子。
- `transfer` 是必需钩子。
- `recoverBus` 是可选钩子，没有时公共层返回 `DRV_STATUS_UNSUPPORTED`。
- `defaultTimeoutMs` 是 port 层默认值，调用 `drvIicTransfer()` 时会在 timeout 为 `0` 的情况下生效。

## 4. `stDrvIicTransfer` 的真实语义

公共层把一次事务抽象为一个 `stDrvIicTransfer`:

- `address`: 7 位从机地址。
- `writeBuffer` + `writeLength`: 第一段写。
- `secondWriteBuffer` + `secondWriteLength`: 第二段连续写。
- `readBuffer` + `readLength`: 最后一段读。

这意味着 BSP 的 `transfer()` 必须支持下面几种情况:

- 纯写。
- 纯读。
- 先写后读。
- 先写一段、再写一段、最后读。

对寄存器读这种常见场景，公共层通常会把“寄存器地址”放在第一段写，把数据放在读段。BSP 需要在写段和读段之间保持正确的 IIC 事务连续性，必要时使用 repeated start。

## 5. `bsphardiic.c` 必须满足的行为契约

### 5.1 `bspHardIicInit(eDrvIicPortMap iic)`

职责:

- 根据逻辑总线完成对应控制器初始化。
- 配置时钟、SCL/SDA 引脚、速率、ACK、滤波和控制器初始状态。

实现要求:

- 对无效逻辑总线返回明确错误。
- 可以重复调用，但重复调用行为必须可预期。
- 初始化成功后，该总线才能被 `drvIicTransfer*()` 使用。

### 5.2 `bspHardIicTransfer(eDrvIicPortMap iic, const stDrvIicTransfer *transfer, uint32_t timeoutMs)`

职责:

- 在一次总线占用期间完成完整事务。

实现要求:

- 不要在 BSP 内再次拆成多个彼此无关的事务，破坏 repeated start 语义。
- 当 `writeLength` 或 `secondWriteLength` 为 `0` 时，要正确跳过对应阶段。
- 当 `readLength` 为 `0` 时，不要强行进入读流程。
- `timeoutMs` 需要真正参与超时判断，而不是被忽略。
- 对 NACK、超时、总线忙、仲裁丢失或其他控制器错误，要返回明确的 `eDrvStatus`。

### 5.3 `bspHardIicRecoverBus(eDrvIicPortMap iic)`

职责:

- 在总线卡死时执行恢复动作。

实现要求:

- 如果控制器或 GPIO 回退方案支持恢复，则实现真实逻辑。
- 如果当前平台不支持，宁可在 port 层不绑定，也不要写伪恢复。

## 6. port 层应该如何写

`drviic_port.h` 应只定义逻辑资源与默认值，例如:

- `DRVIIC_BUS0`
- `DRVIIC_DEFAULT_TIMEOUT_MS`
- `DRVIIC_CONSOLE_SUPPORT`

`drviic_port.c` 的职责是为每个逻辑总线绑定一组 BSP 钩子和默认超时。例如当前结构:

```c
stDrvIicBspInterface gDrvIicBspInterface[DRVIIC_MAX] = {
	[DRVIIC_BUS0] = {
		.init = bspHardIicInit,
		.transfer = bspHardIicTransfer,
		.recoverBus = bspHardIicRecoverBus,
		.defaultTimeoutMs = DRVIIC_DEFAULT_TIMEOUT_MS,
	},
};
```

如果以后新增 `DRVIIC_BUS1`，应该先扩展枚举，再扩展这个绑定表，而不是改公共层逻辑。

## 7. 当前工程默认绑定

当前工程中:

- `DRVIIC_BUS0` 绑定到 `bspHardIicInit()`、`bspHardIicTransfer()`、`bspHardIicRecoverBus()`。
- 默认超时来自 `DRVIIC_DEFAULT_TIMEOUT_MS`，即 `100 ms`。

文档里不再固化更多板级细节。更具体的 SCL/SDA 引脚或速率，应该以 BSP 源码为准。

## 8. 修改 BSP 时必须注意的问题

- `address` 是 7 位地址，方向位由 BSP 自己处理。
- `drvIic.c` 已经做了大部分参数检查，BSP 不应假设输入永远合法，但也不需要重复造复杂上层逻辑。
- 公共层已经做了总线互斥，BSP 应避免再引入破坏时序的多层锁。
- `transfer()` 返回值必须稳定，否则上层 helper 会误判失败类型。

## 9. 联调检查项

- `drvIicInit()` 后是否能稳定访问已知从机。
- `drvIicReadRegister()` 是否使用了正确的 repeated start 行为。
- 超时路径是否真实可触发并返回 `DRV_STATUS_TIMEOUT`。
- 无设备应答时是否返回 `DRV_STATUS_NACK` 或明确失败状态。
- 总线被占用或卡死时，`drvIicRecoverBus()` 是否行为可预期。
