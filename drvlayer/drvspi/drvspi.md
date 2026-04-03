# DrvSpi 模块设计说明

本文档描述 `drvspi` 的公共层、port 层和 BSP 层如何配合，重点说明 `drvspi.c` 对 `drvspi_port.c` 与 `bspspi.c` 的真实依赖。

## 1. 模块定位

`drvspi` 是面向上层模块的硬件 SPI 主机抽象层。它把“总线数据传输”和“片选控制”拆成两套能力:

- SPI 控制器收发由 BSP 提供。
- CS 控制由 port 层绑定的 `csControl` 提供。

这样设计的目的，是让同一个 SPI 控制器可以灵活绑定不同的 GPIO 片选策略，而公共层仍然只暴露统一的 `drvSpi*` 接口。

## 2. 目录内文件职责

- `drvspi.h`: 定义传输结构体、CS 控制结构体、BSP 接口结构体和公共 API。
- `drvspi.c`: 负责参数检查、互斥、CS 拉低拉高时机和事务封装。
- `drvspi_port.h`: 定义逻辑总线枚举、默认超时和默认读填充值。
- `drvspi_port.c`: 维护每条逻辑总线的默认 BSP 绑定和默认 CS 配置。
- `bspspi.h/.c`: 实现控制器初始化、原始收发和默认 GPIO 片选辅助函数。

## 3. core 层依赖的关键结构

`drvspi.c` 依赖的核心结构如下:

```c
typedef struct stDrvSpiCsControl {
	drvSpiCsInitFunc init;
	drvSpiCsWriteFunc write;
	void *context;
} stDrvSpiCsControl;

typedef struct stDrvSpiBspInterface {
	drvSpiBspInitFunc init;
	drvSpiBspTransferFunc transfer;
	uint32_t defaultTimeoutMs;
	stDrvSpiCsControl csControl;
} stDrvSpiBspInterface;
```

这说明 `drvspi` 的 port 层不只是“绑定一个 BSP 函数”，还负责绑定默认 CS 行为。

## 4. `stDrvSpiTransfer` 的语义

一次 SPI 事务由 `stDrvSpiTransfer` 描述:

- 第一段写: `writeBuffer` + `writeLength`
- 第二段写: `secondWriteBuffer` + `secondWriteLength`
- 最后一段读: `readBuffer` + `readLength`
- 读时填充值: `readFillData`

公共层的执行顺序固定如下:

1. 片选拉有效。
2. 发送第一段写数据。
3. 如果有第二段写，继续发送。
4. 如果有读段，再执行读。
5. 片选释放。

因此 BSP 的 `transfer()` 不需要管理 CS，但必须保证单次调用能完整处理一段连续字节流。

## 5. `bspspi.c` 必须满足的契约

### 5.1 `bspSpiInit(eDrvSpiPortMap spi)`

职责:

- 初始化对应逻辑总线的 SPI 控制器。
- 配置 SCK/MISO/MOSI 复用、时钟、极性相位、位宽和速率。

实现要求:

- 对无效逻辑总线返回明确错误。
- 初始化成功后，后续 `transfer()` 才应可用。
- 不负责 CS 初始化，CS 初始化由 `csControl.init()` 负责。

### 5.2 `bspSpiTransfer(eDrvSpiPortMap spi, const uint8_t *txBuffer, uint8_t *rxBuffer, uint16_t length, uint8_t fillData, uint32_t timeoutMs)`

职责:

- 完成一段原始 SPI 收发。

实现要求:

- `txBuffer == NULL` 时，需要发送 `fillData` 来产生时钟。
- `rxBuffer == NULL` 时，只需要丢弃收到的数据。
- `length == 0` 时应快速返回，不做无意义操作。
- `timeoutMs` 需要真实参与等待逻辑。
- 对 TXE、RXNE、BSY 等底层等待失败要返回明确错误。

### 5.3 `bspSpiCsInit(void *context)`

职责:

- 初始化默认片选 GPIO。

实现要求:

- `context` 一般指向 port 层准备好的 CS 配置结构。
- 初始化完成后，应保证 CS 处于非选中状态。

### 5.4 `bspSpiCsWrite(void *context, bool isActive)`

职责:

- 根据 `isActive` 控制片选有效与无效。

实现要求:

- 极性转换应只留在 BSP 或其辅助函数里。
- 不要让上层知道该片选是高有效还是低有效。
- 即使以后更换 CS 引脚，也不应该修改 `drvspi.c`。

## 6. `drvspi_port.c` 应该如何组织

当前 port 层除了绑定 `init` 和 `transfer`，还定义了每个逻辑总线的默认 CS 上下文，例如 `gDrvSpiBus0CsPin`。这类数据属于“当前工程默认绑定”，应该放在 port 层，不应该进入公共层。

默认绑定模式应保持为:

```c
stDrvSpiBspInterface gDrvSpiBspInterface[DRVSPI_MAX] = {
	[DRVSPI_BUS0] = {
		.init = bspSpiInit,
		.transfer = bspSpiTransfer,
		.defaultTimeoutMs = DRVSPI_DEFAULT_TIMEOUT_MS,
		.csControl = {
			.init = bspSpiCsInit,
			.write = bspSpiCsWrite,
			.context = &gDrvSpiBus0CsPin,
		},
	},
};
```

## 7. 当前工程里 port 层真正可调的内容

当前 `drvspi_port.c` 里，后续最常变的通常不是公共逻辑，而是:

- 默认总线到 BSP 的映射。
- 默认 CS GPIO。
- CS 是否低有效。
- 默认超时。

如果只想换片选脚，优先改 port 层上下文，或者运行时调用 `drvSpiSetCsControl()`，而不是去改 `drvspi.c`。

## 8. 修改 BSP 时必须注意的问题

- `drvspi.c` 已经负责总线互斥，BSP 不应破坏其单事务串行语义。
- 公共层会在整个事务期间维持 CS，有些设备依赖这个行为，BSP 不能在内部擅自切换 CS。
- 读阶段默认填充值来自 `readFillData`，BSP 必须真正使用它。
- `drvSpiSetCsControl()` 允许运行时替换 CS 策略，新的策略必须与现有初始化流程兼容。

## 9. 联调检查项

- 逻辑总线初始化后，默认 CS 是否已进入非选中状态。
- `drvSpiWriteRead()` 期间 CS 是否一直保持有效。
- 纯读场景下是否真的发送了 `readFillData`。
- 超时或标志位异常时，BSP 是否返回了明确错误。
- 更换默认 CS 引脚后，上层调用是否无需改动。