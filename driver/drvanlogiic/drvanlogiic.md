# DrvAnlogIic 模块设计说明

本文档说明 `drvanlogiic` 的软件 IIC core 如何依赖 port 与 BSP。重点是让后续实现者明确 `bspanlogiic.c/.h` 中的 GPIO 与延时函数需要满足什么条件，core 才能正常工作。

## 1. 模块定位

`drvanlogiic` 是一个 bit-bang IIC 主机驱动。与 `drviic` 不同，它把 IIC 时序本身放在公共层实现，把板级差异缩小为“如何控制 SCL/SDA 以及如何延时”。

公共层负责:

- 起始、停止、ACK/NACK、读位、写位、读字节、写字节。
- 组合事务流程。
- 总线恢复流程。
- 总线互斥与初始化状态管理。

BSP 层负责:

- SCL/SDA 的实际拉低与释放。
- 读取 SCL/SDA 当前电平。
- 微秒级延时。
- GPIO 模式配置与上拉条件准备。

## 2. 目录内文件职责

- `drvanlogiic.h`: 定义公共 API、事务结构体和 BSP 钩子接口。
- `drvanlogiic.c`: 实现完整的软件 IIC 协议流程。
- `drvanlogiic_port.h`: 定义逻辑总线枚举、默认半周期和恢复脉冲数。
- `drvanlogiic_port.c`: 绑定 BSP 钩子和默认时序参数。
- `bspanlogiic.h/.c`: 负责 GPIO 控制和延时实现。

说明:

- `drvanlogiic.h` 的公共 API 使用 `uint8_t iic` 表示逻辑总线编号。
- `eDrvAnlogIicPortMap` 只定义在 `drvanlogiic_port.h`，用于 port/BSP 层和工程内逻辑总线常量。

## 3. core 层真实依赖的 BSP 接口

`drvanlogiic.c` 依赖的接口结构如下:

```c
typedef struct stDrvAnlogIicBspInterface {
	drvAnlogIicBspInitFunc init;
	drvAnlogIicBspDriveLineFunc setScl;
	drvAnlogIicBspDriveLineFunc setSda;
	drvAnlogIicBspReadLineFunc readScl;
	drvAnlogIicBspReadLineFunc readSda;
	drvAnlogIicBspDelayUsFunc delayUs;
	uint16_t halfPeriodUs;
	uint8_t recoveryClockCount;
} stDrvAnlogIicBspInterface;
```

这里所有函数钩子都是必需项。少任意一个，软件 IIC core 都无法维持正确时序。

## 4. BSP 函数的关键语义

### 4.1 `bspAnlogIicInit(uint8_t iic)`

职责:

- 初始化逻辑总线对应的 SCL/SDA GPIO。
- 建立开漏输出或等价的释放高电平策略。
- 确保初始总线空闲态可恢复为高电平。

### 4.2 `bspAnlogIicSetScl(uint8_t iic, bool releaseHigh)`

职责:

- `releaseHigh == false` 时拉低 SCL。
- `releaseHigh == true` 时释放 SCL 为高电平。

实现要求:

- 这里的“高”应该是释放，不应主动推挽输出高电平，除非硬件设计明确允许且不会破坏开漏语义。
- 如果存在时钟拉伸，`readScl()` 必须能读到真实线电平。

### 4.3 `bspAnlogIicSetSda(uint8_t iic, bool releaseHigh)`

职责和要求与 SCL 相同，只是作用对象换成 SDA。

### 4.4 `bspAnlogIicReadScl(uint8_t iic)`

职责:

- 读取当前 SCL 真实线电平。

实现要求:

- 不能只返回最近一次软件写入状态，必须反映真实硬件引脚状态。
- 总线恢复流程依赖它判断 SCL 是否真正被释放为高。

### 4.5 `bspAnlogIicReadSda(uint8_t iic)`

职责:

- 读取当前 SDA 真实线电平。

实现要求:

- ACK/NACK 判断、读位流程和恢复流程都依赖它。

### 4.6 `bspAnlogIicDelayUs(uint16_t delayUs)`

职责:

- 提供微秒级延时。

实现要求:

- 精度越稳定越好。
- 软件 IIC 全部时序都依赖它，误差过大会直接影响通讯可靠性。

## 5. `stDrvAnlogIicTransfer` 的语义

公共层事务结构与硬件 IIC 基本一致:

- 第一段写。
- 可选第二段写。
- 可选最后读。

但这里事务完全由 `drvanlogiic.c` 自己驱动，因此 BSP 只需要提供最底层的线控制能力，不需要理解 repeated start 或地址方向位。地址左移、R/W 位拼接、ACK/NACK 处理都在公共层完成。

## 6. port 层应该如何写

`drvanlogiic_port.h` 只定义:

- 逻辑总线枚举。
- `DRVANLOGIIC_DEFAULT_HALF_PERIOD_US`
- `DRVANLOGIIC_DEFAULT_RECOVERY_CLOCKS`
- 开关宏。

公共头不再暴露这些逻辑总线枚举；上层若需要命名常量，应显式包含 `drvanlogiic_port.h`。

`drvanlogiic_port.c` 负责为每个逻辑总线绑定一组底层 GPIO/延时函数，并给出默认时序参数，例如:

```c
stDrvAnlogIicBspInterface gDrvAnlogIicBspInterface[DRVANLOGIIC_MAX] = {
	[DRVANLOGIIC_PCA] = {
		.init = bspAnlogIicInit,
		.setScl = bspAnlogIicSetScl,
		.setSda = bspAnlogIicSetSda,
		.readScl = bspAnlogIicReadScl,
		.readSda = bspAnlogIicReadSda,
		.delayUs = bspAnlogIicDelayUs,
		.halfPeriodUs = DRVANLOGIIC_DEFAULT_HALF_PERIOD_US,
		.recoveryClockCount = DRVANLOGIIC_DEFAULT_RECOVERY_CLOCKS,
	},
};
```

## 7. 当前工程里 port 层真正可调的内容

后续常见调整点通常是:

- 换逻辑总线绑定。
- 调整 `halfPeriodUs` 以改变等效速度。
- 调整 `recoveryClockCount` 以适配恢复策略。

这些都应该在 port 层完成，而不是改 `drvanlogiic.c` 的核心协议逻辑。

## 8. 修改 BSP 时必须注意的问题

- SCL/SDA 必须满足开漏或等价释放语义。
- `readScl()` 和 `readSda()` 要读真实线电平，不能只读软件缓存。
- `delayUs()` 的精度会直接影响设备兼容性。
- `drvAnlogIicTransferTimeout()` 当前只是接口兼容包装，timeout 参数不会真正改变软件 IIC 流程，不要在文档中误导成“支持精确超时控制”。

## 9. 联调检查项

- 空闲态时 SCL/SDA 是否都能被释放为高。
- 起始、停止和 ACK 时序是否符合目标器件要求。
- 强制拉低 SDA 后，`drvAnlogIicRecoverBus()` 是否能恢复总线。
- 调整 `halfPeriodUs` 后，目标设备是否仍然稳定通信。
- `readScl()` 是否能正确反映时钟拉伸或硬件占线情况。