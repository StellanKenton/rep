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
    - ../../../User/port/lis2hh12_port.h
    - ../../../User/port/lis2hh12_port.c
debug_files:
    - lis2hh12_debug.h
    - lis2hh12_debug.c
depends_on:
    - ../../driver/drviic/drviic.md
    - ../../driver/drvanlogiic/drvanlogiic.md
forbidden_depends_on:
    - 在 core 中直连 HAL I2C 或具体 BSP 句柄
required_hooks:
    - lis2hh12PortGetOps
optional_hooks:
    - stLis2hh12Ops::getRetryDelayMs
    - stLis2hh12Ops::getResetPollDelayMs
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

`lis2hh12` 是一个寄存器型三轴加速度计模块。对上提供默认配置、初始化、寄存器访问、单次原始采样和 FIFO 批量采样能力；对下通过项目侧 `ops/provider` 绑定到共享 IIC 驱动。

## 2. 目录内文件职责

| 文件 | 职责 |
| --- | --- |
| `lis2hh12.h` | 公共配置、寄存器常量、稳定 API |
| `lis2hh12.c` | 默认配置、bring-up 流程、FIFO 采样逻辑 |
| `lis2hh12_assembly.h` | `stLis2hh12Ops`、IIC 接口类型和 provider 契约 |
| `User/port/lis2hh12_port.h` | 项目侧 provider 入口声明，暴露 `lis2hh12PortGetOps()` |
| `User/port/lis2hh12_port.c` | 项目侧静态 `ops` 表实现，装配默认配置、IIC 访问和延时能力 |
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

- `GetDefCfg()` 只通过 `ops->loadDefaultCfg` 向调用者写默认值。
- `SetCfg()` 写入配置快照并清掉 ready / 上次采样缓存。
- `Init()` 完成探测、软复位、滤波链配置和 FIFO 模式配置。
- ready 条件：总线初始化成功、`WHO_AM_I` 匹配、软复位完成、目标配置全部下发成功。

## 5. 与示例代码的对应关系

当前实现吸收了 `example/lis2hh12_cpr_data_retrieve.*` 的核心行为，但做了以下仓库化调整：

- 去掉了对 HAL `hi2c1` 和 RTOS `osDelay()` 的直接依赖。
- 不再复用示例中的全局 `stmdev_ctx_t`，而是改成模块上下文 + assembly hook。
- 保留 FIFO stream 模式、`WHO_AM_I` 校验、软复位轮询和批量取样语义。
- 将“应用特有的 2~3 个样本才算正常”策略留给上层，不固化在模块 core 内。

## 6. ops / provider 契约

| 名称 | 必需/可选 | 由谁实现 | 作用 | 缺失时语义 |
| --- | --- | --- |
| `lis2hh12PortGetOps()` | 必需 | `User/port/lis2hh12_port.*` | 返回长期有效的静态 `stLis2hh12Ops` 表 | 返回 `NULL` 时，`GetDefCfg()` 返回 `NOT_READY`，`Init()` 返回 `INVALID_PARAM` |
| `stLis2hh12Ops::loadDefaultCfg` | 必需 | port `ops` | 提供默认地址、FIFO 阈值和初始化配置 | 保持零值时视为默认配置不可用 |
| `stLis2hh12Ops::getIicInterface` | 必需 | port `ops` | 返回 `init/writeReg/readReg` 最小 IIC 接口 | 返回 `NULL` 时初始化失败 |
| `stLis2hh12Ops::isValidAssemble` | 必需 | port `ops` | 判断当前 transport / linkId 装配是否合法 | 返回 `false` 时视为装配无效 |
| `stLis2hh12Ops::getLinkId` | 必需 | port `ops` | 返回设备映射到的下层 linkId | 缺失时 core 不会继续 IIC 访问 |
| `stLis2hh12Ops::delayMs` | 必需 | port `ops` | 提供阻塞延时 | 缺失时视为装配不完整 |
| `stLis2hh12Ops::getRetryDelayMs` | 可选 | port `ops` | 提供探测重试间隔 | 缺失时回退到 `10 ms` |
| `stLis2hh12Ops::getResetPollDelayMs` | 可选 | port `ops` | 提供复位轮询间隔 | 缺失时回退到 `1 ms` |

### 6.1 最小 `lis2hh12_port.c` 骨架

```c
static const stLis2hh12Ops gLis2hh12PortOps = {
    .loadDefaultCfg = lis2hh12PortLoadDefaultCfg,
    .getIicInterface = lis2hh12PortGetIicInterface,
    .isValidAssemble = lis2hh12PortIsValidAssemble,
    .getLinkId = lis2hh12PortGetLinkId,
    .getRetryDelayMs = lis2hh12PortGetRetryDelayMs,
    .getResetPollDelayMs = lis2hh12PortGetResetPollDelayMs,
    .delayMs = lis2hh12PortDelayMs,
};

const stLis2hh12Ops *lis2hh12PortGetOps(void)
{
    return &gLis2hh12PortOps;
}
```

### 6.2 必填成员检查表

| 成员 | 是否必填 | 约束 |
| --- | --- | --- |
| `loadDefaultCfg` | 必填 | 命中已知 device 时必须把 `cfg` 填到 `lis2hh12IsValidCfg()` 可通过的程度 |
| `getIicInterface` | 必填 | 返回的接口表中 `init/writeReg/readReg` 都必须非 `NULL` |
| `isValidAssemble` | 必填 | 未知 device 必须返回 `false` |
| `getLinkId` | 必填 | 仅对 `isValidAssemble()==true` 的 device 返回合法下层 bus/linkId |
| `delayMs` | 必填 | 必须是可重复调用的阻塞延时 |
| `getRetryDelayMs` | 可选 | 缺失时 core 用 `10 ms` |
| `getResetPollDelayMs` | 可选 | 缺失时 core 用 `1 ms` |

### 6.3 多 device 分发规则

| 场景 | 推荐做法 | 不推荐做法 |
| --- | --- | --- |
| 单设备工程 | 用静态数组或 `switch` 只保留一个映射 | 在 core 中硬编码 `DRVIIC_BUS0` |
| 多设备工程 | 在 port 层按 `device` 分发不同 `linkId` 或不同接口表 | 为每个 device 复制一份 core |
| 未知 device | `isValidAssemble` 返回 `false`，`loadDefaultCfg` 保持零值 | 默认回退到某个猜测 bus |

## 7. 下层驱动使用边界

- 允许通过 port `ops` 绑定 `drviic` 或 `drvanlogiic` 的公共 API。
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