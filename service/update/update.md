---
doc_role: layer-guide
layer: service
module: update
status: active
portability: layer-dependent
public_headers:
    - update.h
    - update_port.h
core_files:
    - update.c
    - update.h
    - update.md
port_files:
    - update_port.c
    - update_port.h
debug_files:
    - update_debug.c
    - update_debug.h
depends_on:
    - ../../driver/drvmcuflash/drvmcuflash.md
    - ../console/log.md
forbidden_depends_on:
    - core 直接依赖具体 GD25QXXX 或 HAL Flash 句柄
    - 业务层直接操作升级区地址和标志位地址
required_hooks:
    - updatePortLoadDefaultCfg
    - updatePortGetStorageOps
    - updatePortGetRegionMap
optional_hooks:
    - updatePortGetTickMs
    - updatePortFeedWatchdog
    - updatePortJumpToRegion
    - updateDbgOnStateChanged
common_utils:
    - drvmcuflash
    - log
copy_minimal_set:
    - service/update/update.h
    - service/update/update.c
    - service/update/update_port.h
    - service/update/update_port.c
read_next:
    - ../../driver/drvmcuflash/drvmcuflash.md
    - ../console/log.md
---

# Update Service Guide

这是 `rep/service/update/` 目录的权威入口文档。

## 1. 本层目标和边界

`update` 负责把“升级文件存放在哪里、当前设备要改写哪个逻辑区、失败时是否回滚、成功后何时交给项目层跳转”收敛成一套可复用的升级状态机。

本层负责：

- 维护统一的升级状态机。
- 定义镜像头、升级记录、错误码、状态码等稳定数据结构。
- 基于“逻辑存储设备 + 逻辑区域”抽象进行镜像校验、备份、编程与回滚。
- 为掉电恢复提供双槽元数据记录的基线实现。
- 在长耗时擦写路径之间调用 watchdog hook，而不是直接碰具体 IWDG 驱动。

本层不负责：

- 蓝牙、串口、CAN 等升级包传输协议。
- 具体内部 Flash 或外部 Flash 驱动细节。
- 当前工程的升级入口、模式切换、复位时机和实际跳转汇编清理细节。

一句话约束：`update core` 只理解逻辑镜像和逻辑区域，不直接理解 `GD25Q32_MEM`、`SPI1`、`0x08020000` 这类项目细节。

## 2. 目录拆分与当前文件职责

| 文件 | 层级 | 职责 |
| --- | --- | --- |
| `update.h` | core | 对外稳定 API、状态枚举、错误码、镜像头、升级记录、元数据结构 |
| `update.c` | core | 升级状态机、元数据双槽读写、镜像校验、备份/编程/回滚编排 |
| `update_port.h` | port | 存储设备枚举、区域映射、存储操作表、watchdog/tick/jump hook 契约 |
| `update_port.c` | port | 当前工程默认把 MCU App 区和 GD25Q32 staging/backup/record 区绑定到逻辑区域 |
| `update_debug.h/.c` | debug | 状态名和状态切换通知桥接，便于项目层做日志或诊断 |

## 3. 核心抽象模型

### 3.1 逻辑存储设备

`update_port.h` 使用 `eUpdateStorageId` 把物理介质抽象成稳定枚举：

| 枚举值 | 含义 | 当前默认绑定 |
| --- | --- | --- |
| `E_UPDATE_STORAGE_INTERNAL_FLASH` | MCU 内部 Flash | `drvmcuflash` |
| `E_UPDATE_STORAGE_EXT_FLASH1` | 外部 Flash 1 | `gd25qxxx` |
| `E_UPDATE_STORAGE_EXT_FLASH2` | 外部 Flash 2 | 预留 |

### 3.2 逻辑区域

`update.h` 使用 `eUpdateRegion` 定义升级流程中的稳定语义区：

| 枚举值 | 含义 | 当前默认情况 |
| --- | --- | --- |
| `E_UPDATE_REGION_BOOT_RECORD` | 升级事务记录区 | 已绑定 |
| `E_UPDATE_REGION_RUN_APP` | 运行 App 区 | 已绑定 |
| `E_UPDATE_REGION_STAGING_APP` | 待升级镜像正文区 | 已绑定 |
| `E_UPDATE_REGION_STAGING_APP_HEADER` | 待升级镜像头区 | 已绑定 |
| `E_UPDATE_REGION_BACKUP_APP` | 运行镜像备份正文区 | 已绑定 |
| `E_UPDATE_REGION_BACKUP_APP_HEADER` | 运行镜像备份头区 | 已绑定 |
| `E_UPDATE_REGION_BOOT_IMAGE` | Boot 镜像区 | 预留 |
| `E_UPDATE_REGION_FACTORY_APP` | 工厂镜像区 | 预留 |

### 3.3 逻辑区域描述符

`stUpdateRegionCfg` 用来描述每个逻辑区域的读写语义：

```c
typedef struct stUpdateRegionCfg {
    uint8_t storageId;
    uint32_t startAddress;
    uint32_t size;
    uint32_t eraseUnit;
    uint32_t progUnit;
    uint32_t headerReserveSize;
    bool isReadable;
    bool isWritable;
    bool isExecutable;
} stUpdateRegionCfg;
```

## 4. 公共数据结构与落盘语义

### 4.1 镜像头

镜像头采用 `stUpdateImageHeader`，记录镜像类型、版本、大小、CRC、写入偏移和状态。`headerCrc32` 只覆盖镜像头正文，不覆盖自己。

### 4.2 升级记录

升级记录采用 `stUpdateBootRecord`，显式保存：

- `requestFlag`：当前事务阶段。
- `lastError`：最近一次失败原因。
- `targetRegion`：本次升级目标逻辑区。
- `stagingCrc32` / `backupCrc32`：事务内关键镜像 CRC。
- `imageSize` / `sequence`：事务大小和业务序号。

### 4.3 元数据双槽提交

`update.c` 把 `BOOT_RECORD`、`STAGING_APP_HEADER`、`BACKUP_APP_HEADER` 统一包装成 `stUpdateMetaRecord`，默认按“双擦除单元轮转”提交：

1. 选择 `standby slot`。
2. 擦除该 slot。
3. 写入头字段和 payload，保留 `commitMarker = 0xFFFFFFFF`。
4. 回读检查头 CRC 和 payload CRC。
5. 最后单独写 `commitMarker = UPDATE_META_COMMIT_MARKER`。

恢复时只接受同时满足下面条件的记录：

- `recordMagic` 正确。
- `commitMarker` 已提交。
- `headerCrc32` 校验通过。
- `payloadCrc32` 校验通过。

## 5. port 层契约

### 5.1 存储操作接口

| 名称 | 必需/可选 | 由谁实现 | 在哪里被调用 | 原型摘要 | 成功语义 | 失败语义 | 前置条件 | 备注 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `init` | 必需 | port 存储适配器 | `updateInit()` | `bool (*)(void)` | 对应设备可访问 | `false` 表示设备不可用 | 底层总线已 ready | 每个已使用存储只初始化一次 |
| `read` | 必需 | port 存储适配器 | 元数据读取、CRC 校验、回滚 | `bool (*)(uint32_t, uint8_t *, uint32_t)` | 请求数据全部读出 | `false` 表示访问失败 | 地址范围合法 | 入参地址是设备内地址 |
| `write` | 必需 | port 存储适配器 | 写镜像头、升级记录和目标镜像 | `bool (*)(uint32_t, const uint8_t *, uint32_t)` | 请求数据全部写入 | `false` 表示写失败 | 目标已完成必要擦除 | 不隐式执行 erase |
| `erase` | 必需 | port 存储适配器 | 擦除 metadata/staging/backup/target | `bool (*)(uint32_t, uint32_t)` | 请求范围已满足擦除要求 | `false` 表示擦除失败 | 长度大于 0 | port 负责按底层擦除粒度对齐 |
| `isRangeValid` | 可选 | port 存储适配器 | init 检查和 I/O 保护 | `bool (*)(uint32_t, uint32_t)` | 范围合法 | `false` 表示越界 | 无 | 未提供时由 core 用 region 信息兜底 |

### 5.2 provider / hook 契约

| 名称 | 必需/可选 | 由谁实现 | 在哪里被调用 | 原型摘要 | 成功语义 | 失败语义 | 前置条件 | 备注 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `updatePortLoadDefaultCfg` | 必需 | port | `updateInit()` | `bool (*)(stUpdateCfg *)` | 默认配置已装入 | `false` 表示无合法配置 | `cfg != NULL` | 负责装配逻辑区域 |
| `updatePortGetStorageOps` | 必需 | port | `updateInit()`、运行期 I/O | `const stUpdateStorageOps *(*)(uint8_t)` | 返回非空操作表 | `NULL` 表示该设备未绑定 | `storageId < E_UPDATE_STORAGE_MAX` | core 不缓存底层私有句柄 |
| `updatePortGetRegionMap` | 必需 | port | 诊断或项目层查询 | `bool (*)(uint8_t, stUpdateRegionCfg *)` | 返回合法区域配置 | `false` 表示未绑定 | `cfg != NULL` | 当前默认实现直接回放默认 map |
| `updatePortGetTickMs` | 可选 | port | `updateProcess()` | `uint32_t (*)(void)` | 返回单调 tick | `0` 时由调用者参数兜底 | 系统时基已启动 | 默认实现走 `repRtosGetTickMs()` |
| `updatePortFeedWatchdog` | 可选 | port | 长耗时擦写状态 | `void (*)(void)` | 完成一次 watchdog 续期 | 空实现表示项目自担超时策略 | 无 | 每个分块完成后调用一次 |
| `updatePortJumpToRegion` | 可选 | port | `updateJumpToTargetIfValid()` | `bool (*)(uint8_t)` | 已完成目标跳转 | `false` 表示当前工程未接入跳转实现 | 目标向量表已通过校验 | 默认实现是 weak stub |
| `updateDbgOnStateChanged` | 可选 | debug | `updateSetState()` | `void (*)(eUpdateState, eUpdateState)` | 调试输出完成 | 空实现视为关闭调试 | 日志通道可用 | 建议仅做轻量诊断 |

## 6. 公共函数使用契约表

| 来源模块 | 公共函数 | 允许在哪些文件调用 | 用途 | 调用前提 | 典型调用顺序 | 返回值处理 | 禁止做法 |
| --- | --- | --- | --- | --- | --- | --- | --- |
| `update` | `updateInit` | Boot 初始化入口、升级 manager 装配层 | 加载默认绑定并建立状态机初态 | port 已链接到工程 | `updateInit -> updateProcess(loop)` | `false` 时不要继续调状态机 | 初始化后手工改 region 地址 |
| `update` | `updateProcess` | Boot 主循环或周期调度点 | 推进一次有限状态机 | `updateInit` 成功 | `updateInit -> updateProcess(loop)` | 不返回错误码，错误通过 `updateGetStatus()` 读 | 把整镜像搬运塞进一次不可中断调用 |
| `update` | `updateGetStatus` | 系统管理、诊断命令、debug | 查询当前状态和进度 | `updateInit` 后 | `updateProcess -> updateGetStatus` | 只读使用 | 调用方直接篡改内部对象 |
| `update` | `updateRequestProgramRegion` | App 侧升级准备层、项目 manager | 在 staging 已 ready 后落盘升级请求 | `STAGING_APP_HEADER` 已是 ready | `write staging -> verify -> request program` | `false` 时不得复位进 Boot | staging 未校验就直接写请求 |
| `update` | `updateReadBootRecord` | Boot 检查阶段、诊断命令 | 读取当前事务记录 | `BOOT_RECORD` 已绑定 | `init -> read record -> decide` | 无记录时返回默认 idle 记录 | 业务层直接读裸地址 |
| `update` | `updateJumpToTargetIfValid` | 项目层跳转入口 | 在目标向量表合法时交给 port 层执行跳转 | 工程已实现 `updatePortJumpToRegion` | `check request -> validate vector -> jump` | `false` 表示未接入或校验失败 | 在 core 里硬编码芯片寄存器清理 |
| `drvmcuflash` | `drvMcuFlashInit/Read/Write/Erase/IsRangeValid` | `update_port.c`、项目 manager | 直接实现内部 Flash 存储操作 | port 区域白名单已绑定 | `init -> erase -> write -> readback` | 返回 `false` 视为失败 | `update.c` 直接 include `drvmcuflash_port.h` |
| `gd25qxxx` | `gd25qxxxInit/Read/Write/EraseSector/EraseBlock64k` | 仅 `update_port.c` 内部适配器 | 实现外部 Flash 存储操作 | device 已绑定 | `init -> erase -> write -> read` | 非 `OK` 视为失败 | `update.c` 直接拿 `GD25Q32_MEM` 做业务判断 |

## 7. 当前实现的状态机与恢复策略

当前版本已经把 `BOOT_IMAGE`、`FACTORY_APP` 等逻辑区域枚举预留出来，但真正打通的编程链路只有 `RUN_APP`。后续若要支持 Boot 镜像升级，应在保持当前元数据与 port 抽象不变的前提下扩展目标区选择和跳转策略，而不是把 Boot 细节直接写回 core I/O 路径。

主流程：

1. `CHECK_REQUEST`
2. `VALIDATE_STAGING`
3. `PREPARE_BACKUP`
4. `BACKUP_RUN_APP`
5. `VERIFY_BACKUP`
6. `ERASE_TARGET`
7. `PROGRAM_TARGET`
8. `VERIFY_TARGET`
9. `COMMIT_RESULT`
10. `JUMP_TARGET`

失败策略：

- 擦除目标区之前失败：写 `FAILED` 记录并进入 `ERROR`。
- 擦除目标区之后失败且备份有效：进入 `ROLLBACK_*` 链路。
- 回滚成功：把升级记录更新为 `FAILED`，再进入 `JUMP_TARGET`。

当前实现的恢复重点是：

- `BOOT_RECORD`/`*_HEADER` 断电后能够从最新已提交 slot 恢复。
- `requestFlag = BACKUP_DONE / PROGRAM_DONE` 时，可在重启后继续主流程而不是从头擦写元数据。

## 8. 当前工程默认绑定

`update_port.c` 当前默认按本工程现有布局装配：

| 逻辑区域 | 绑定位置 |
| --- | --- |
| `RUN_APP` | MCU 内部 Flash `0x08020000 ~ 0x0807FFFF` |
| `BOOT_RECORD` | MCU 内部 Flash `0x0801F000 ~ 0x0801FFFF`，占用 `0x08020000` 之前最后两个 2KB page |
| `BACKUP_APP_HEADER` | GD25Q32 `0x00300000` 起的 8KB 元数据区 |
| `BACKUP_APP` | GD25Q32 `0x00302000 ~ 0x0037FFFF` |
| `STAGING_APP_HEADER` | GD25Q32 `0x00380000` 起的 8KB 元数据区 |
| `STAGING_APP` | GD25Q32 `0x00382000 ~ 0x003FFFFF` |

说明：

- `BOOT_RECORD` 改为内部 Flash 末两页，既满足双槽元数据提交所需的 2 个擦除单元，也尽量保留更大的 boot 区代码空间。
- 这里把 header 区和正文区显式拆开，避免正文区头部预留与独立头区并存。
- 外部 Flash 的 `*_HEADER` 区仍按双 4KB sector 组织；内部 Flash 的 `BOOT_RECORD` 区按双 2KB page 组织，二者都满足双槽提交的最小要求。
- 如果项目实际地址布局变化，应只改 `update_port.c/.h`，不要改 `update.c` 状态机。

## 9. 改动落点矩阵

| 需求 | 应改文件 | 不该改的文件 |
| --- | --- | --- |
| 把升级记录从外部 Flash 改到内部 Flash | `update_port.c/.h` | `update.c` 主状态机 |
| 新增外部 Flash2 作为 staging 或 backup | `update_port.c/.h` | `update.h` 的逻辑区域定义 |
| 调整回滚策略 | `update.c` | `update_port.c` 的设备读写接口 |
| 接入实际 jump 汇编清理 | 项目层实现 `updatePortJumpToRegion` | `update.c` 中硬编码 MCU 细节 |
| 调整日志或状态诊断粒度 | `update_debug.c` 或项目层 `updateDbgOnStateChanged` | 底层 Flash 驱动 |
| 调整当前产品升级入口和协议流程 | `User/` 目录下的项目 manager | `rep/service/update/` 的公共状态语义 |

## 10. 复制到其他工程的最小步骤

最小依赖集：

- `update.h/.c`
- `update_port.h/.c`
- 至少一个可执行目标区存储适配器
- 至少一个 staging 区可写存储适配器

外部项目必须补齐：

- 逻辑区域到物理存储的映射表
- 存储设备统一操作接口
- 目标镜像跳转实现，或在项目层自己处理跳转
- 可选的 watchdog 和 debug hook

## 11. 使用示例

```c
if (!updateInit()) {
    return;
}

for (;;) {
    updateProcess(0U);

    if (updateGetStatus()->state == E_UPDATE_STATE_JUMP_TARGET) {
        (void)updateJumpToTargetIfValid();
    }
}
```

App 侧在 staging 镜像与头区都写好后，可调用：

```c
(void)updateRequestProgramRegion(E_UPDATE_REGION_RUN_APP);
```

## 12. 验证清单

- `updateInit()` 后能检测缺失的 `RUN_APP` / `STAGING_APP` / `BOOT_RECORD` 绑定。
- `STAGING_APP_HEADER` 非 ready 或 CRC 不匹配时，不会擦写 `RUN_APP`。
- 启用回滚时，`BACKUP_APP` 不足以覆盖 `RUN_APP` 会在 init 阶段失败。
- `BOOT_RECORD`/`*_HEADER` 在新 slot 未提交前断电，旧 slot 仍可恢复。
- 长耗时状态 `PREPARE_BACKUP`、`BACKUP_RUN_APP`、`ERASE_TARGET`、`PROGRAM_TARGET`、回滚链路都会在分块之间调用 `updatePortFeedWatchdog()`。
- 项目未实现 `updatePortJumpToRegion()` 时，`updateJumpToTargetIfValid()` 明确返回失败，不在 core 中偷偷访问芯片寄存器。