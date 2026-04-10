---
doc_role: module-spec
layer: module
module: lsm6
status: active
portability: layer-dependent
public_headers:
    - lsm6.h
core_files:
    - lsm6.c
port_files:
    - lsm6_assembly.h
debug_files:
    - lsm6_debug.h
    - lsm6_debug.c
depends_on:
    - ../../driver/drviic/drviic.md
    - ../../driver/drvanlogiic/drvanlogiic.md
forbidden_depends_on:
    - 在 core 中直连具体 IIC 驱动私有绑定
required_hooks:
    - lsm6LoadPlatformDefaultCfg
    - lsm6GetPlatformIicInterface
    - lsm6PlatformIsValidAssemble
    - lsm6PlatformDelayMs
optional_hooks:
    - lsm6PlatformGetResetDelayMs
    - lsm6PlatformGetResetPollDelayMs
common_utils: []
copy_minimal_set:
    - lsm6.h
    - lsm6.c
    - lsm6_assembly.h
read_next:
    - ../module.md
    - ../mpu6050/mpu6050.md
---

# LSM6 模块说明

这是当前目录的权威入口文档。

## 1. 模块定位

`lsm6` 是一个面向 LSM6 系列 6 轴 IMU 的通用模块，对上提供初始化、寄存器访问、原始温度/陀螺/加速度读取能力；对下通过 assembly hook 绑定项目里的 IIC 实现。

当前实现按 ST `LSM6DSO` 数据手册和同族公共寄存器布局落地，兼容常见 `WHO_AM_I` 值：

- `0x69` `LSM6DS3`
- `0x6A` `LSM6DSL`
- `0x6B` `LSM6DSM`
- `0x6C` `LSM6DSO`

## 2. 目录内文件职责

| 文件 | 职责 |
| --- | --- |
| `lsm6.h` | 公共配置、寄存器常量、稳定 API |
| `lsm6.c` | 默认配置、bring-up 流程、寄存器读写和数据读取 |
| `lsm6_assembly.h` | 平台默认配置、IIC 接口 provider、延时 hook |
| `lsm6_debug.h/.c` | 可选调试占位 |
| `lsm6.md` | 当前目录 contract |

## 3. 对外公共接口

稳定公共头文件：`lsm6.h`

稳定 API：

- `lsm6GetDefCfg()`
- `lsm6GetCfg()`
- `lsm6SetCfg()`
- `lsm6Init()`
- `lsm6IsReady()`
- `lsm6ReadId()`
- `lsm6ReadReg()` / `lsm6WriteReg()`
- `lsm6ReadStatus()`
- `lsm6ReadRaw()` / `lsm6ReadTempCdC()`

调用顺序：

1. 先取默认 cfg。
2. 如需修改地址、ODR、量程等，调用 `SetCfg()`。
3. 再 `Init()`。
4. 用 `IsReady()` 确认可用后再读写寄存器或采样。

## 4. 配置、状态与生命周期

`lsm6` 属于 `passive module`：

- `GetDefCfg()` 只写调用者缓冲。
- `SetCfg()` 写入模块快照并清理 ready / 缓存。
- `Init()` 基于当前 cfg 完成整套 bring-up。
- ready 条件：总线初始化成功、`WHO_AM_I` 合法、软复位完成、`CTRL1_XL/CTRL2_G/CTRL3_C` 配置完成。

## 5. assembly 契约

core 只依赖最小 IIC 接口：

- `init(bus)`
- `writeReg(bus, address, regBuf, regLen, buffer, length)`
- `readReg(bus, address, regBuf, regLen, buffer, length)`

推荐由 port 层决定默认地址、链路号、软件或硬件 IIC、以及复位等待时序。

## 6. 当前默认初始化内容

- 读 `WHO_AM_I`
- 写 `CTRL3_C.SW_RESET`
- 等待复位完成
- 配置 `BDU` 和 `IF_INC`
- 配置 `CTRL1_XL` 和 `CTRL2_G`

当前实现故意保持最小，不在模块层提前绑定 FIFO、中断和嵌入功能。

## 7. 后续扩展点

- FIFO 配置与读取
- 中断路由
- 量程换算为物理单位
- step counter / wake-up / 6D 等嵌入功能