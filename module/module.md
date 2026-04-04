# Module 层文档总说明

## 1. 文档定位

`USER/Rep/module/module.md` 只做一件事：指导如何在本工程生成一个新的 module。

它回答的是通用问题：

- 为什么新模块必须拆成 core 和 port。
- 新模块应该有哪些文件。
- 对外 API、配置结构、状态码、实例管理应该怎么组织。
- port 层需要向 core 层提供什么最小能力。
- 新模块从建目录到可编译，建议按什么顺序推进。
- 寄存器也应该放在core的h文件下，port层不应该直接看到寄存器地址和命令，而是通过接口表提供更抽象的访问能力。

它不负责描述某个具体模块的寄存器、命令时序或适配细节。那些内容应写在各自模块目录里的 `<module>.md` 中。

## 2. 文档分工

module 目录下的文档按下面方式分工：

- `module.md`：新建模块的通用生成规范。
- `<module>/<module>.md`：该模块内部文件如何分工、core 需要什么、port 应如何链接 drv 来满足 core。

判断原则如下：

- 只要这段内容对大多数新模块都成立，写进 `module.md`。
- 只要这段内容依赖某个具体模块的寄存器、命令、传输形式或初始化流程，写进对应模块自己的 md。

## 3. 标准目录结构

新模块默认采用下面的目录结构：

```text
<module>/
    <module>.h
    <module>.c
    <module>_port.h
    <module>_port.c
    <module>_debug.h
    <module>_debug.c
    <module>.md
```

各文件职责固定如下：

- `<module>.h`：公共宏、状态码、枚举、结构体、公共函数声明。
- `<module>.c`：模块核心逻辑，只表达模块语义，不直接依赖 bsp。
- `<module>_port.h`：port 绑定结构、port 接口表、平台相关钩子声明。
- `<module>_port.c`：默认总线映射、drv 适配函数、延时函数、板级连接方式。
- `<module>_debug.h/.c`：可选 debug / console 注册接口与调试命令实现，必须受宏开关控制。
- `<module>.md`：本模块内部设计说明，重点写 core 和 port 的契约。

## 4. 分层原则

### 4.1 core 层负责什么

`<module>.c/.h` 负责：

- 模块公共语义。
- 参数校验。
- 默认配置装载触发。
- 初始化流程编排。
- 寄存器访问、协议读写、业务读写逻辑。
- 运行时状态维护，例如 `isReady`、缓存、探测结果、统计信息。

`<module>.c/.h` 不负责：

- GPIO、bus 对应哪一路外设。
- 使用硬件 IIC、软件 IIC、硬件 SPI 还是其他板级资源。
- FreeRTOS 与裸机差异。
- 直接调用 bsp 层函数。

### 4.2 port 层负责什么

`<module>_port.c/.h` 负责：

- 默认绑定和默认配置。
- 把 drv 公共接口适配成 core 所需的最小动作集合。
- 把逻辑 bus 编号转换成 drv 枚举。
- 延时、等待、平台差异处理。
- 当前板子的连接方式。

`<module>_port.c/.h` 不负责：

- 寄存器语义。
- 业务状态机。
- 初始化顺序本身。
- 面向上层的业务 API。

## 5. 生成新模块时先确定的四件事

开始写代码前，先明确这四件事：

1. 这个模块对上真正要暴露哪些稳定能力。
2. 这个模块对下最少需要哪些 drv 动作。
3. 哪些默认值属于“当前硬件怎么接的”，因此应放在 port 层。
4. 哪些字段属于运行态，因此应放在内部 `Device/Ctx` 而不是 `Cfg`。

这四件事明确后，文件结构和接口设计基本就固定了。

## 6. 推荐的数据结构

### 6.1 逻辑设备编号

如果模块对应固定数量的板级器件，优先使用逻辑设备编号：

```c
typedef enum eXxxDevMap {
    XXX_DEV0 = 0,
    XXX_DEV1,
    XXX_DEV_MAX,
} eXxxMapType;
```

这样可以统一：

- 上层入口。
- 默认映射。
- 内部上下文数组。

### 6.2 配置与运行态分离

推荐至少拆成下面两层：

```c
typedef struct stXxxCfg {
    stXxxPortBind bind;
} stXxxCfg;

typedef struct stXxxDevice {
    stXxxCfg cfg;
    bool isReady;
} stXxxDevice;
```

补充原则：

- `Cfg` 只放可配置项。
- `Device/Ctx` 持有 `Cfg`，并补充所有运行态。
- 缓存、探测结果、统计值不要混进 `Cfg`。

### 6.3 最小 port 接口表

core 不应该直接看到完整 drv API，而应该只看到自己真正需要的最小动作。

例如：

```c
typedef eDrvStatus (*xxxPortInitFunc)(uint8_t bus);
typedef eDrvStatus (*xxxPortReadRegFunc)(uint8_t bus, ...);
typedef eDrvStatus (*xxxPortWriteRegFunc)(uint8_t bus, ...);

typedef struct stXxxPortInterface {
    xxxPortInitFunc init;
    xxxPortReadRegFunc readReg;
    xxxPortWriteRegFunc writeReg;
} stXxxPortInterface;
```

接口表设计规则：

- 只保留 core 真正调用的动作。
- 函数参数直接服务于 core 的访问语义。
- 不把 drv 私有结构体、bsp 句柄或硬件细节泄露给 core。

## 7. port 链接函数怎么设计

port 层最重要的内容不是默认映射，而是“链接函数”如何把 drv 接到 core。

这里的链接函数，指的就是 `*_port.c` 里那组 adapter，例如：

- `xxxPortHardIicInitAdpt()`
- `xxxPortHardIicReadRegAdpt()`
- `xxxPortHardIicWriteRegAdpt()`
- `xxxPortHardSpiTransferAdpt()`

这些函数必须满足以下要求：

1. 函数签名由 core 需要的最小动作决定，而不是照搬 drv 全接口。
2. 函数内部先校验 `bus` 和关键参数，再调用底层 drv。
3. 返回值统一使用 `eDrvStatus` 或模块约定的兼容状态。
4. 不在 adapter 里混入业务语义，例如寄存器初始化顺序、容量推导、数据解析。
5. 如果底层 drv 需要组包，组包动作可以放在 adapter 中，但组包后的语义仍然要保持通用。

换句话说，adapter 只做“翻译”，不做“决策”。

## 8. core 对 port 的典型需求

新模块设计时，可以先从 core 反推 port 需要提供什么能力。常见模式如下：

### 8.1 寄存器类模块

适用于传感器、控制器、编解码器等：

- `init(bus)`
- `readReg(bus, address, regBuf, regLen, buffer, length)`
- `writeReg(bus, address, regBuf, regLen, buffer, length)`
- `delayMs(delayMs)`

### 8.2 流式传输类模块

适用于 SPI Flash、显示器、某些总线外设：

- `init(bus)`
- `transfer(bus, writeBuffer, writeLength, secondWriteBuffer, secondWriteLength, readBuffer, readLength, readFillData)`
- `delayMs(delayMs)`

### 8.3 查询状态类模块

如果模块初始化或业务流程要轮询底层状态，可以在 core 中组织轮询逻辑，port 只提供一次原子访问能力。不要把轮询策略写进 port。

## 9. 推荐 API 形态

通用情况下，建议公共 API 按下面顺序组织：

- `<module>GetDefCfg(device, &cfg)`
- `<module>GetCfg(device, &cfg)`
- `<module>SetCfg(device, &cfg)`
- `<module>Init(device)`
- `<module>IsReady(device)`
- `<module>GetInfo(device)` 或 `<module>GetState(device)`
- `<module>ReadReg()` / `<module>WriteReg()` / `<module>ReadId()`
- `<module>ReadXxx()` / `<module>WriteXxx()` / `<module>SetXxx()` / `<module>EraseXxx()`

不是每个模块都要全部具备，但顺序建议尽量统一。

## 10. 生命周期分类与 cfg ownership

建议把模块生命周期统一成下面三类：

- passive module：`GetDefCfg/GetCfg/SetCfg + Init + API`，不需要周期 `Process()`。
- active service：`GetDefCfg/GetCfg/SetCfg + Init + Start + Process/Task + Stop`。
- recoverable service：在 active service 基础上补 `Fault/GetLastError/Recover`。

P1 阶段至少要把下面几条变成仓库事实：

- `GetDefCfg(device, &cfg)` 只把默认值写到调用者提供的缓冲区，不直接修改模块内部运行态。
- `GetCfg(device, &cfg)` 返回模块当前持有的配置快照。
- `SetCfg(device, &cfg)` 把调用者配置拷贝进模块上下文，并清掉 `isReady` 与相关缓存。
- `Init(device)` 只消费模块当前持有的 cfg，不再承担“顺手生成默认值”的职责。
- hot reconfig 统一走 `GetCfg/GetDefCfg -> 修改 cfg -> SetCfg -> Init`。

对 passive module，文档至少要明确：

- cfg ownership：调用者持有临时 cfg，模块内部持有 `SetCfg()` 后的快照。
- ready 条件：哪些 bring-up 步骤成功后才允许 `isReady = true`。
- repeat init：允许重复 `Init()`，但应视为基于当前 cfg 的重初始化。
- hot reconfig：允许，但必须先 `SetCfg()`，再重新 `Init()`。
- recover path：失败后如何回到可用状态，通常是重新 `SetCfg() + Init()`。

## 11. 初始化流程模板

`Init()` 建议按下面顺序实现：

1. 获取设备上下文。
2. 校验设备编号是否合法。
3. 校验配置是否合法。
4. 校验绑定是否合法。
5. 校验 port 接口表是否完整。
6. 初始化底层总线。
7. 清理 `isReady` 和旧缓存。
8. 读取关键 ID 或在线状态。
9. 下发复位、时序、模式、容量等初始化配置。
10. 所有步骤成功后再置 `isReady = true`。

这样做的目的：

- 初始化失败时状态清晰。
- 旧缓存不会被误用。
- 调试时容易定位失败阶段。

## 12. 状态码规则

新增模块时优先复用 `eDrvStatus`。

建议规则：

- 参数错误直接返回 `DRV_STATUS_INVALID_PARAM`。
- 未初始化或 port 接口未就绪返回 `DRV_STATUS_NOT_READY`。
- 设备 ID 不匹配返回 `DRV_STATUS_ID_NOTMATCH`。
- 底层通信失败直接透传 drv 状态。
- 模块业务独有错误，例如越界、对齐错误、协议不支持，可从 `DRV_STATUS_ERROR + 1` 往后扩展。

## 13. 默认值放在哪里

下面这些通常放 port 层：

- 默认 bus。
- 默认总线类型。
- 默认地址。
- 默认片选。
- 平台相关延时常量。

下面这些通常放 core 层：

- 协议命令。
- 寄存器地址。
- 数据解析公式。
- 初始化流程语义。

判断标准只有一句话：

- 更像“器件怎么工作”的，放 core。
- 更像“这块板子怎么接”的，放 port。

## 13. 新模块落地步骤

建议按下面顺序生成新模块：

1. 建立目录和五个基础文件。
2. 在 `<module>.h` 中先定义状态码、设备编号、配置结构、上下文结构、公共 API。
3. 在 `<module>_port.h` 中定义绑定结构、port 接口表、默认配置函数、延时接口。
4. 在 `<module>_port.c` 中先写默认映射，再写 adapter，再写绑定校验函数。
5. 在 `<module>.c` 中实现默认配置装载、配置校验、初始化流程和业务接口。
6. 在 `<module>.md` 中补齐“内部文件分工”和“port 如何满足 core”的说明。

## 14. 每个模块自己的 md 应该写什么

每个 `<module>.md` 至少应包含下面内容：

```text
# <module> 模块设计说明

## 1. 模块目标
## 2. 文件分工
## 3. core 需要的最小底层能力
## 4. port 链接函数设计
## 5. 默认配置与默认映射
## 6. 初始化流程
## 7. 公共 API 使用顺序
## 8. 错误处理与 ready 约束
## 9. 后续扩展点
```

## 15. 自检清单

写完一个新模块后，用下面清单检查：

- core 是否仍然不依赖 bsp。
- port 接口表是否只保留最小动作。
- adapter 是否只负责翻译，不负责业务决策。
- `Cfg` 和运行态是否已分离。
- 默认映射是否放在 port 层。
- `Init()` 是否分阶段并在最后置 ready。
- 公共接口是否区分参数错误、未就绪、通信失败。
- 模块自己的 md 是否已经写清 core 和 port 的契约。

满足以上要求后，这个模块通常就符合当前工程的 module 层风格。