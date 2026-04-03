# W25QXXX 模块设计说明

## 1. 模块目标

`w25qxxx` 是一个 SPI NOR Flash 模块。它对上提供 JEDEC 探测、容量信息查询、读、页编程、扇区擦除、64K 擦除和整片擦除接口；对下只依赖项目中的 `drvspi` 公共层。

这份文档关注的是当前目录内部文件如何配合，尤其是 `w25qxxx_port.c` 里的链接函数要如何设计，才能满足 `w25qxxx.c` 的 core 访问需求。

## 2. 文件分工

### 2.1 `w25qxxx.h`

负责定义：

- 逻辑设备编号 `eW25qxxxMapType`。
- Flash 命令相关常量和尺寸常量。
- 状态码别名与扩展状态码。
- SPI port 接口表和绑定结构。
- 配置结构 `stW25qxxxCfg`。
- 识别信息结构 `stW25qxxxInfo`。
- 运行上下文 `stW25qxxxDevice`。
- 对外公共 API。

这里把 `stW25qxxxPortSpiInterface` 放进头文件，是因为它本身就是 core 与 port 的正式契约。

### 2.2 `w25qxxx.c`

负责实现：

- 默认配置懒加载。
- 配置读取与回写。
- JEDEC ID 探测和容量解析。
- 范围检查、分页写入、分片读取。
- busy 轮询。
- 设备信息和 `isReady` 状态维护。

core 只依赖 port 提供的两个原子动作：

- `init`
- `transfer`

这说明 core 关心的是“发一个命令头，再带一段数据或收一段数据”，而不是底层 SPI 控制器细节。

### 2.3 `w25qxxx_port.h`

负责声明：

- 默认绑定和默认配置函数。
- 绑定修改函数。
- 绑定合法性检查函数。
- 接口表完整性检查函数。
- 获取当前绑定接口表的函数。
- 毫秒延时函数。

### 2.4 `w25qxxx_port.c`

负责实现：

- 默认设备映射 `gW25qxxxPortDefCfg`。
- `drvSpiInit()` 的 adapter。
- `drvSpiTransfer()` 的 adapter。
- `gW25qxxxPortSpiInterfaces` 接口表。
- 绑定校验和接口获取。
- FreeRTOS / 裸机共用延时函数。

这个文件的本质不是写 Flash 协议，而是把当前工程的 SPI drv 翻译成 W25QXXX core 需要的最小访问能力。

### 2.5 `w25qxxx_debug.h/.c`

负责实现：

- 可选 console 注册入口。
- `list`、`init`、`jedec`、`status`、`info`、`read`、`write`、`erase` 等调试命令。
- 保持命令轻量，只覆盖联调和定位问题所需的关键动作。

## 3. core 需要的最小底层能力

从 `w25qxxx.c` 可以直接看出，core 对 port 只有三类需求：

1. 初始化指定 bus。
2. 执行一次完整 SPI 传输。
3. 在 busy 轮询时提供毫秒等待。

因此接口表非常小：

```c
typedef struct stW25qxxxPortSpiInterface {
	w25qxxxPortSpiInitFunc init;
	w25qxxxPortSpiTransferFunc transfer;
} stW25qxxxPortSpiInterface;
```

这两个函数已经足够支撑：

- JEDEC 读 ID。
- 状态寄存器读取。
- 数据读取。
- 页编程。
- 扇区与块擦除。

如果将来 core 需要更多动作，应先确认现有 `transfer()` 是否真的不够，再决定是否扩接口，而不是提前暴露多余能力。

## 4. port 链接函数设计

### 4.1 当前模块需要的 adapter 形态

对 W25QXXX 而言，关键 adapter 只有两个：

```c
static eDrvStatus w25qxxxPortHardSpiInitAdpt(uint8_t bus);
static eDrvStatus w25qxxxPortHardSpiTransferAdpt(
	uint8_t bus,
	const uint8_t *writeBuffer,
	uint16_t writeLength,
	const uint8_t *secondWriteBuffer,
	uint16_t secondWriteLength,
	uint8_t *readBuffer,
	uint16_t readLength,
	uint8_t readFillData);
```

这里的 `transfer` 之所以有两段写缓冲，是因为 core 的很多命令天然就是“命令头 + 数据段”模式。例如：

- 页编程时，先发命令头和地址，再发待写数据。
- 读取时，先发命令头和地址，再收数据。

这个签名正好覆盖当前 core 的通用访问模型。

### 4.2 adapter 内部应该做什么

adapter 里应该做这些事：

- 校验 `bus` 是否越界。
- 把 `bus` 转成 `eDrvSpiPortMap`。
- 把 core 传入的多段缓冲整理成 `stDrvSpiTransfer`。
- 调用 `drvSpiInit()` 或 `drvSpiTransfer()`。
- 把底层返回值原样返回给 core。

### 4.3 adapter 内部不应该做什么

adapter 不应该做这些事：

- 不应该解析 JEDEC ID。
- 不应该推导容量大小和地址宽度。
- 不应该决定读命令用 3-byte 还是 4-byte。
- 不应该执行分页写逻辑。
- 不应该执行 busy 轮询。
- 不应该吞掉底层错误后伪装成功。

这些都属于 core 的职责。

## 5. 默认配置与默认映射

当前默认映射在 `gW25qxxxPortDefCfg` 中定义：

- `W25QXXX_DEV0 -> DRVSPI_BUS0`
- `W25QXXX_DEV1 -> DRVSPI_BUS1`

这表明以下内容被定义为工程级默认值：

- 哪个逻辑 Flash 设备默认挂在哪一路 SPI。
- 默认使用的 SPI类型。
- 默认读取填充值 `W25QXXX_PORT_READ_FILL_DATA`。

这些都放在 port 层是合理的，因为它们反映的是板级连接，而不是 Flash 协议本身。

## 6. 初始化流程

`w25qxxxInit()` 的流程体现了 core 与 port 的边界：

1. 获取设备上下文。
2. 校验设备和配置是否合法。
3. 校验绑定和接口表是否合法。
4. 调用 `spiIf->init()` 初始化底层 SPI。
5. 清理 `isReady` 和 `info`。
6. 读取 JEDEC ID。
7. 检查厂商 ID 是否匹配。
8. 检查容量 ID 是否支持。
9. 根据容量 ID 填充 `info`。
10. 全部成功后置 `isReady = true`。

这里可以看到：

- port 只提供 `init()` 和 `transfer()`。
- JEDEC 判定、容量推导、地址宽度选择都由 core 决定。
- delay 虽然由 port 提供，但 busy 轮询策略仍由 core 控制。

## 7. 公共 API 使用顺序

推荐按下面顺序使用：

1. 调用 `w25qxxxGetDefCfg(device)` 装载默认配置。
2. 如需换总线，先 `w25qxxxGetCfg()`，再修改 `cfg.spiBind`，最后 `w25qxxxSetCfg()`。
3. 调用 `w25qxxxInit(device)`。
4. 调用 `w25qxxxIsReady(device)` 或 `w25qxxxGetInfo(device)` 判断模块是否可用。
5. 再调用 `w25qxxxRead()`、`w25qxxxWrite()`、`w25qxxxEraseSector()` 等业务接口。

这个顺序对后续新增更多 Flash 派生模块也适用。

## 8. 错误处理与 ready 约束

当前模块的错误处理约束如下：

- 配置非法或绑定非法，返回 `INVALID_PARAM`。
- 绑定合法但接口表不完整，返回 `NOT_READY`。
- JEDEC 厂商 ID 不匹配，返回 `DEVICE_ID_MISMATCH`。
- 容量 ID 不支持，返回 `UNSUPPORTED`。
- 地址范围不合法或擦除地址未对齐，返回 `OUT_OF_RANGE`。
- 底层 SPI 失败，透传为映射后的 drv 状态。

并且只有在 JEDEC 探测和容量解析都成功后，`isReady` 才能置位。

## 9. 后续扩展点

如果后面需要扩展这个模块，建议沿当前契约扩展：

- 新增双线或四线传输实现时，优先在 port 层新增 adapter 和接口表项。
- 新增更复杂的读写命令选择时，优先扩 core 中的命令构建逻辑。
- 新增默认设备映射时，扩展 `gW25qxxxPortDefCfg`。
- 新增等待策略时，扩展 `w25qxxxPortDelayMs()`，但轮询策略仍留在 core。

核心原则保持不变：

- core 负责 Flash 协议和业务语义。
- port 负责把工程里的 SPI drv 链接成 core 可直接调用的最小动作集合。
