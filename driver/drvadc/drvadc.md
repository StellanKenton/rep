# DrvAdc 模块设计说明

本文档说明 `drvadc` 的公共层如何依赖 port 与 BSP，并约束后续 `bspadc.c/.h` 应满足的契约。

## 1. 模块定位

`drvadc` 是 `driver` 下的 ADC 公共驱动层，负责把“逻辑采样通道”和“具体 MCU ADC 控制器实现”隔离开。

公共层负责：

- 参数校验。
- 初始化状态检查。
- 通道级互斥与错误语义统一。
- 原始采样值读取接口。
- 在 BSP 未提供毫伏接口时，根据参考电压和分辨率把原始值换算为毫伏。

BSP 层负责：

- ADC 控制器、GPIO 模拟输入、时钟与校准初始化。
- 指定逻辑通道的单次采样或等价采样流程。

## 2. 目录内文件职责

- `drvadc.h`: 定义公共 API 与 BSP 钩子接口。
- `drvadc.c`: 实现状态校验、互斥控制、超时选择与原始值到毫伏的回退换算。
- `drvadc_port.h`: 定义逻辑 ADC 通道枚举、默认超时、默认参考电压和默认分辨率。
- `drvadc_port.c`: 绑定当前工程里的逻辑通道到 BSP 钩子。
- `bspadc.h/.c`: 负责 MCU 相关的 ADC 外设、通道配置与采样实现。

说明：

- `drvadc.h` 的公共 API 入口使用 `uint8_t adc` 表示逻辑通道编号，不再在公共头里暴露逻辑通道枚举。
- `eDrvAdcPortMap` 只保留在 `drvadc_port.h`，供 port/BSP 层和需要逻辑通道常量的工程文件使用。

## 3. core 层真实依赖的 BSP 接口

`drvadc.c` 依赖的接口结构如下：

```c
typedef struct stDrvAdcBspInterface {
    drvAdcBspInitFunc init;
    drvAdcBspReadRawFunc readRaw;
    uint32_t defaultTimeoutMs;
    uint16_t referenceMv;
    uint8_t resolutionBits;
} stDrvAdcBspInterface;
```

其中：

- `init` 和 `readRaw` 是必需钩子。
- MCU/BSP 层只返回 `raw`，公共层统一使用 `referenceMv` 和 `resolutionBits` 换算 `mv` 并更新结果缓存。
- 这组函数指针在 port 层只绑定一次，不为每个逻辑通道重复保存一份。
- 默认超时、参考电压和分辨率也合并在同一个 `stDrvAdcBspInterface` 里统一维护一次。

每个逻辑通道只保留采样结果数据，通过 `stDrvAdcData` 维护：

- `raw`
- `mv`

## 4. BSP 函数语义

### 4.1 `bspAdcInit(uint8_t adc)`

职责：

- 初始化逻辑通道对应的 ADC 外设、时钟和输入引脚。
- 确保后续 `bspAdcReadRaw()` 可被重复调用。

失败语义：

- 通道未配置、硬件资源冲突或初始化失败时返回明确错误。

### 4.2 `bspAdcReadRaw(uint8_t adc, uint16_t *value, uint32_t timeoutMs)`

职责：

- 对指定逻辑通道执行一次采样。
- 将结果写入 `value`。

失败语义：

- 参数错误返回 `DRV_STATUS_INVALID_PARAM`。
- 采样资源忙返回 `DRV_STATUS_BUSY`。
- 超时返回 `DRV_STATUS_TIMEOUT`。
- 当前工程未完成硬件绑定时返回 `DRV_STATUS_UNSUPPORTED`。

## 5. port 层应该如何写

`drvadc_port.h` 只定义：

- 逻辑 ADC 通道枚举。
- `DRVADC_LOCK_WAIT_MS`
- `DRVADC_DEFAULT_TIMEOUT_MS`
- `DRVADC_DEFAULT_RESOLUTION_BITS`
- `DRVADC_DEFAULT_REFERENCE_MV`

`drvadc_port.c` 负责：

- 绑定单个 `gDrvAdcBspInterface`。
- 维护 `gDrvAdcData[DRVADC_MAX]` 作为每个通道的采样结果缓存。

也就是说：

- BSP 钩子只注册一次。
- 公共 API 只接收 `uint8_t adc`，通道差异通过 `drvadc_port.h` 里的逻辑枚举和对应硬件映射表达。
- BSP/port 内部再根据 `eDrvAdcPortMap` 或等价表项去查找实际硬件映射，一一完成初始化和采样。

## 6. 当前工程默认状态

当前提交已经生成 `drvadc` 模块及 `bspadc` 骨架，但 `bspadc.c` 仍是默认占位实现：

- `DRVADC_CH0`、`DRVADC_CH1`、`DRVADC_CH2` 已经预留。
- `bspAdcInit()` 和 `bspAdcReadRaw()` 默认返回 `DRV_STATUS_UNSUPPORTED`。
- `bspadc.c` 内部应按 `drvadc_port.h` 中的逻辑通道定义查表获取每个通道的绑定信息。

这意味着：

- 工程可以正常编译。
- 上层在真正完成 ADC 资源绑定前不会误以为 ADC 已可用。

## 7. 联调检查项

- 逻辑通道是否已经映射到正确的 ADC 控制器和输入脚。
- `bspAdcInit()` 是否完成时钟、GPIO 模拟模式和 ADC 校准。
- `bspAdcReadRaw()` 是否在超时和忙状态下返回明确错误码。
- `referenceMv` 和 `resolutionBits` 是否与实际 ADC 配置一致。
