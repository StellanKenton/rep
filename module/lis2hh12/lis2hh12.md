---
doc_role: module-spec
layer: module
module: lis2hh12
status: active
portability: layer-dependent
public_headers:
    - lis2hh12.h
core_files:
    - lis2hh12.c
port_files:
    - lis2hh12_assembly.h
debug_files:
    - lis2hh12_debug.h
    - lis2hh12_debug.c
depends_on:
    - ../../driver/drviic/drviic.md
    - ../../driver/drvanlogiic/drvanlogiic.md
forbidden_depends_on:
    - 在 core 中直连 HAL I2C 或具体 BSP 句柄
required_hooks:
    - lis2hh12LoadPlatformDefaultCfg
    - lis2hh12GetPlatformIicInterface
    - lis2hh12PlatformIsValidAssemble
    - lis2hh12PlatformDelayMs
optional_hooks:
    - lis2hh12PlatformGetRetryDelayMs
    - lis2hh12PlatformGetResetPollDelayMs
common_utils: []
copy_minimal_set:
    - lis2hh12.h
    - lis2hh12.c
    - lis2hh12_assembly.h
read_next:
    - ../module.md
    - ../../driver/drviic/drviic.md
---

# LIS2HH12 模块说明

这是当前目录的权威入口文档。

## 1. 模块定位

`lis2hh12` 是一个寄存器型三轴加速度计模块。对上提供默认配置、初始化、寄存器访问、单次原始采样和 FIFO 批量采样能力；对下通过 assembly hook 绑定到共享 IIC 驱动。

## 2. 目录内文件职责

| 文件 | 职责 |
| --- | --- |
| `lis2hh12.h` | 公共配置、寄存器常量、稳定 API |
| `lis2hh12.c` | 默认配置、bring-up 流程、FIFO 采样逻辑 |
| `lis2hh12_assembly.h` | 平台默认配置、IIC 接口 provider、延时 hook |
| `lis2hh12_debug.h/.c` | 可选调试入口占位 |
| `lis2hh12.md` | 当前目录 contract |

## 3. 对外公共接口

稳定公共头文件：`lis2hh12.h`

稳定 API：

- `lis2hh12GetDefCfg()`
- `lis2hh12GetCfg()`
- `lis2hh12SetCfg()`
- `lis2hh12Init()`
- `lis2hh12IsReady()`
- `lis2hh12ReadId()`
- `lis2hh12ReadReg()` / `lis2hh12WriteReg()`
- `lis2hh12ReadRaw()`
- `lis2hh12ReadFifoStatus()`
- `lis2hh12ReadFifoSamples()`

推荐调用顺序：

1. 先调用 `GetDefCfg()` 获取默认配置。
2. 如需调整地址、FIFO 阈值或滤波参数，调用 `SetCfg()`。
3. 再执行 `Init()`。
4. 只有在 `IsReady()` 为真后，才进行寄存器访问或 FIFO 采样。

## 4. 配置、状态与生命周期

`lis2hh12` 属于 `passive module`：

- `GetDefCfg()` 只向调用者写默认值。
- `SetCfg()` 写入配置快照并清掉 ready / 上次采样缓存。
- `Init()` 完成探测、软复位、滤波链配置和 FIFO 模式配置。
- ready 条件：总线初始化成功、`WHO_AM_I` 匹配、软复位完成、目标配置全部下发成功。

## 5. 与示例代码的对应关系

当前实现吸收了 `example/lis2hh12_cpr_data_retrieve.*` 的核心行为，但做了以下仓库化调整：

- 去掉了对 HAL `hi2c1` 和 RTOS `osDelay()` 的直接依赖。
- 不再复用示例中的全局 `stmdev_ctx_t`，而是改成模块上下文 + assembly hook。
- 保留 FIFO stream 模式、`WHO_AM_I` 校验、软复位轮询和批量取样语义。
- 将“应用特有的 2~3 个样本才算正常”策略留给上层，不固化在模块 core 内。

## 6. assembly / platform hook 契约

| 名称 | 必需/可选 | 作用 |
| --- | --- | --- |
| `lis2hh12LoadPlatformDefaultCfg` | 必需 | 提供默认地址、FIFO 阈值和初始化配置 |
| `lis2hh12GetPlatformIicInterface` | 必需 | 返回 `init/writeReg/readReg` 最小 IIC 接口 |
| `lis2hh12PlatformIsValidAssemble` | 必需 | 判断当前 transport / linkId 装配是否合法 |
| `lis2hh12PlatformDelayMs` | 必需 | 提供阻塞延时 |
| `lis2hh12PlatformGetRetryDelayMs` | 可选 | 探测重试间隔 |
| `lis2hh12PlatformGetResetPollDelayMs` | 可选 | 复位轮询间隔 |

## 7. 下层驱动使用边界

- 允许通过 assembly adapter 绑定 `drviic` 或 `drvanlogiic` 的公共 API。
- 禁止在 `lis2hh12.c` 中直接 include HAL、CubeMX、BSP 或 `i2c.h`。
- 禁止把 `hi2c1`、GPIO 端口号或板级中断脚等项目绑定信息暴露到公共头文件。

## 8. 错误处理与 ready 约束

- 参数错误返回 `DRV_STATUS_INVALID_PARAM`。
- assembly 未装配或未完成初始化返回 `DRV_STATUS_NOT_READY`。
- `WHO_AM_I` 不匹配返回 `DRV_STATUS_ID_NOTMATCH`。
- FIFO 为空或样本数量异常时，`lis2hh12ReadFifoSamples()` 返回 `LIS2HH12_STATUS_DATA_INVALID`。
- FIFO 样本数超过调用者缓冲区时，返回 `LIS2HH12_STATUS_BUFFER_TOO_SMALL`。

## 9. 后续扩展点

- 若后续需要温度转换、阈值中断或 self-test，可在 core 中继续增加寄存器语义。
- 若项目需要 console 联调，可在 `lis2hh12_debug.*` 中补充命令，而不改公共 API。