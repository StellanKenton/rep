# DrvMcuFlash 模块设计说明

## 1. 模块定位

`drvmcuflash` 提供 MCU 片内 Flash 的最小稳定写存储接口。它对上暴露“绝对地址 + 长度 + bool 返回值”的访问方式，便于直接接到上层存储操作表；对下继续通过 port/BSP 维护可写区域和 sector 细节，避免业务层直接碰底层 Flash 控制器。

当前默认配置只开放 1MB Flash 的最后一个 128KB sector 作为用户区，便于存放参数、校准值或掉电保存数据。

## 2. 文件分工

- `drvmcuflash.h`: 定义公共 API、区域信息结构和 BSP 钩子表契约。
- `drvmcuflash.c`: 负责参数校验、范围保护、并发串行化和公共读写擦流程。
- `drvmcuflash_port.h`: 定义模块开关、锁等待时间和默认用户区范围。
- `drvmcuflash_port.c`: 绑定 `gDrvMcuFlashBspInterface`，并给出当前工程的默认区域映射。
- `drvmcuflash.md`: 说明 core 对 port/BSP 的依赖契约和默认配置。
- `bspmcuflash.h/.c`: 负责 GD32F4 FMC 相关的 sector 布局、擦除和编程实现。

## 3. 公共 API

当前公共层暴露以下接口：

- `drvMcuFlashInit()`
- `drvMcuFlashIsReady()`
- `drvMcuFlashGetAreaInfo()`
- `drvMcuFlashRead()`
- `drvMcuFlashWrite()`
- `drvMcuFlashErase()`
- `drvMcuFlashIsRangeValid()`

接口约束如下：

- 公共读写擦接口使用绝对地址；成功返回 `true`，失败返回 `false`。
- 逻辑区域枚举 `eDrvMcuFlashAreaMap` 和区域数量只保留在 `drvmcuflash_port.h/.c` 中，作为内部可写区白名单。
- `drvMcuFlashIsRangeValid()` 用于校验绝对地址范围是否完整落在某个已绑定区域内。
- 写操作不会隐式擦除 Flash，调用方应先根据需要执行 `drvMcuFlashErase()`。
- 写操作会先检查目标地址是否存在 `0 -> 1` 的位翻转需求，如存在则直接返回错误，避免静默写坏数据。

## 4. core 对 port/BSP 的依赖

公共层只依赖以下钩子：

- `init`
- `unlock`
- `lock`
- `eraseSector`
- `program`
- `getSectorInfo`

其中：

- `getSectorInfo` 必须能根据绝对地址返回所在 sector 编号、起始地址和大小。
- `eraseSector` 必须按 sector 编号执行一次完整擦除。
- `program` 必须支持按字节流编程，并把底层失败语义转换成 `eDrvStatus`。

只要这些钩子完整，公共层就不需要知道 GD32 的 FMC 寄存器细节。

## 5. 默认区域映射

当前 `drvmcuflash_port.h` 里默认定义：

- `DRVMCUFLASH_BOOT_RECORD_START_ADDR = 0x0801F000`
- `DRVMCUFLASH_BOOT_RECORD_SIZE = 0x00001000`
- `DRVMCUFLASH_APP_START_ADDR = 0x08020000`
- `DRVMCUFLASH_APP_SIZE = 0x00060000`

这对应当前 Bootloader 工程的两段内部 Flash 可写区：`BOOT_RECORD` 使用 `0x08020000` 之前最后两个 2KB page，`RUN_APP` 使用 `0x08020000` 之后的 App 区。之所以仍保留 port 层区域映射，是为了让公共层能在接受绝对地址时继续阻止越界擦写程序区。

如果以后需要把用户区改到别的 sector，只改 port 宏即可，公共层 API 不需要变化。

## 6. 并发与安全性

- 在 FreeRTOS 下，公共层使用互斥锁串行化全部读写擦操作。
- 在无 RTOS 配置下，公共层使用轻量 busy 标志和临界区保护，避免重入。
- 擦除和编程必须成对执行 `unlock/lock`，失败时也要确保最终重新上锁。

## 7. BSP 实现检查点

重写或迁移 `bspmcuflash.c` 时，至少检查下面几点：

- sector 表是否与目标芯片 Flash 容量一致。
- `getSectorInfo()` 是否覆盖所有可访问地址。
- `program()` 是否正确处理非对齐长度。
- 底层超时、保护错误和忙状态是否都映射成明确的 `eDrvStatus`。
- 改动默认用户区时，新的区域是否完整落在一个或多个可擦除 sector 内。