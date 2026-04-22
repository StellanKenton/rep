---
doc_role: module-spec
layer: module
module: sdcard
status: active
portability: layer-dependent
public_headers:
  - sdcard.h
core_files:
  - sdcard.c
  - sdcard_assembly.h
port_files: []
debug_files: []
depends_on:
  - ../../rep.h
  - ../../lib/fatfs/fatfs.md
forbidden_depends_on:
  - 在 core 中直连具体 SDIO/SPI BSP 或项目私有 diskio.c
required_hooks:
  - sdcardLoadPlatformDefaultCfg
  - sdcardGetPlatformInterface
  - sdcardPlatformIsValidCfg
optional_hooks: []
common_utils: []
copy_minimal_set:
  - sdcard.h
  - sdcard.c
  - sdcard_assembly.h
read_next:
  - ../module.md
  - ../../lib/fatfs/fatfs.md
---

# SDCard 模块说明

这是当前目录的权威入口文档。

## 1. 模块定位

`sdcard` 是块设备语义模块，对上提供配置、初始化、卡状态查询、块读写、同步和 trim 接口；对下通过 assembly hook 绑定实际 SDIO 或 SPI 控制器。

这个模块故意不在 core 内部实现 SD 协议细节，而是把控制器/协议差异留给项目侧 transport provider，这样可以复用在不同 MCU、不同控制器和不同板级接法上。

## 2. 目录内文件职责

| 文件 | 职责 |
| --- | --- |
| `sdcard.h` | 公共状态码、配置、卡信息、块读写 API |
| `sdcard.c` | 设备实例管理、默认配置、状态刷新、范围校验、通用读写流程 |
| `sdcard_assembly.h` | 项目侧最小 transport 契约和 ioctl 命令定义 |
| `sdcard.md` | 当前目录 contract |

## 3. 对外公共接口

稳定公共头文件：`sdcard.h`

稳定 API：

- `sdcardGetDefCfg()`
- `sdcardGetCfg()`
- `sdcardSetCfg()`
- `sdcardInit()`
- `sdcardIsReady()` / `sdcardGetInfo()` / `sdcardGetStatus()`
- `sdcardReadBlocks()` / `sdcardWriteBlocks()`
- `sdcardSync()` / `sdcardTrim()`

推荐调用顺序：

1. `sdcardGetDefCfg()`。
2. 如需更换控制器 linkId 或超时，调用 `sdcardSetCfg()`。
3. `sdcardInit()`。
4. 通过 `sdcardIsReady()` 或 `sdcardGetInfo()` 确认可用后进行块访问。
5. 上层 `diskio.c` 或文件系统 glue 通过 `sdcardReadBlocks()` / `sdcardWriteBlocks()` / `sdcardSync()` 转接到 FatFs。

## 4. 配置、状态与生命周期

`sdcard` 属于 `passive module`：

- `GetDefCfg()` 只写稳定默认配置。
- `SetCfg()` 写入配置快照并清空 ready 与缓存信息。
- `Init()` 负责控制器 bring-up、介质在线检测和卡容量信息刷新。
- ready 条件：配置合法、provider 完整、介质在线且 `GET_INFO` 返回有效 `blockCount`。

当前模块的 `info` 字段专门面向块设备语义：

- `blockSize`
- `blockCount`
- `eraseBlockSize`
- `capacityBytes`
- `isPresent`
- `isWriteProtected`
- `isHighCapacity`

## 5. 函数指针 / port / assembly 契约

| 名称 | 必需/可选 | 由谁实现 | 在哪里被调用 | 原型摘要 | 成功语义 | 失败语义 | 前置条件 | 备注 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `sdcardLoadPlatformDefaultCfg` | 必需 | 项目侧 `User/port` | `sdcardGetDefCfg` | `void (*)(device, cfg)` | 写入默认 linkId 与初始化超时 | weak 默认值只占位 | `cfg != NULL` | 默认值应体现当前板级绑定 |
| `sdcardGetPlatformInterface` | 必需 | 项目侧 `User/port` | `sdcardInit`、读写流程 | `const stSdcardInterface *(*)(cfg)` | 返回完整 transport 接口 | 返回 `NULL` 视为未绑定 | cfg 已装载 | core 不直接 include SDIO/SPI 私有头 |
| `sdcardPlatformIsValidCfg` | 必需 | 项目侧 `User/port` | `sdcardSetCfg`、`sdcardInit` | `bool (*)(cfg)` | 返回 `true` | 返回 `false` 视为配置非法 | `cfg != NULL` | 用于限制 linkId 与超时范围 |
| `stSdcardInterface.init` | 必需 | 项目侧 transport provider | `sdcardInit` | `eDrvStatus (*)(bus, timeoutMs)` | 控制器可访问介质 | 返回明确底层错误 | cfg 合法 | 可内部完成卡初始化序列 |
| `stSdcardInterface.getStatus` | 必需 | 项目侧 transport provider | `sdcardInit`、`sdcardGetStatus` | `eDrvStatus (*)(bus, isPresent, isWriteProtected)` | 返回实时介质状态 | 返回底层状态 | provider 已装配 | 用于卡插拔和写保护检测 |
| `stSdcardInterface.readBlocks` | 必需 | 项目侧 transport provider | `sdcardReadBlocks` | `eDrvStatus (*)(bus, startBlock, buffer, blockCount)` | 指定块读取成功 | 返回底层状态 | 模块 ready | 块大小由 `GET_INFO` 给出 |
| `stSdcardInterface.writeBlocks` | 可选 | 项目侧 transport provider | `sdcardWriteBlocks` | `eDrvStatus (*)(bus, startBlock, buffer, blockCount)` | 指定块写入成功 | 缺失时 core 返回 `UNSUPPORTED` | 模块 ready 且非写保护 | 只读介质可以不实现 |
| `stSdcardInterface.ioctl` | 建议 | 项目侧 transport provider | `sdcardInit`、`sdcardSync`、`sdcardTrim` | `eDrvStatus (*)(bus, cmd, buffer)` | 命令执行成功 | 缺失时部分 API 返回 `UNSUPPORTED` | provider 已装配 | `GET_INFO` 是 ready 的关键依赖 |

## 6. ioctl 命令约定

`sdcard_assembly.h` 当前定义了 3 个标准命令：

- `eSDCARD_IOCTL_GET_INFO`：`buffer` 指向 `stSdcardInfo`。
- `eSDCARD_IOCTL_SYNC`：刷新写缓存，可传 `NULL`。
- `eSDCARD_IOCTL_TRIM`：`buffer` 指向 `stSdcardTrimRange`。

如果项目暂时不支持 `SYNC` 或 `TRIM`，provider 可以返回 `DRV_STATUS_UNSUPPORTED`。

## 7. 改动落点矩阵

| 需求 | 应改文件 | 不该改的文件 |
| --- | --- | --- |
| 改公共块设备 API、状态码、ready 规则 | `sdcard.h/.c` | 项目侧 SDIO/SPI 驱动 |
| 改默认 linkId、超时和控制器绑定 | 项目侧 `sdcard_port.*` 或等价 provider | `sdcard.c` |
| 接 FatFs `diskio.c` | 项目侧 glue 文件 | `rep/lib/fatfs` 库本体 |
| 改底层 SDIO/SPI 时序或 DMA 行为 | 项目侧 transport/provider | `sdcard.c` |

## 8. 复制到其他工程的最小步骤

最小依赖集：`sdcard.h`、`sdcard.c`、`sdcard_assembly.h`。

外部工程至少要补齐：

- 默认 cfg provider
- cfg 合法性校验
- `init/getStatus/readBlocks`
- 建议补齐 `writeBlocks/ioctl`

## 9. 验证清单

- `sdcardInit()` 后，插卡时能获得合法 `blockSize` 和 `blockCount`。
- 未插卡时返回 `SDCARD_STATUS_NO_MEDIUM`，且不会误置 ready。
- 越界块访问返回 `SDCARD_STATUS_OUT_OF_RANGE`。
- 写保护卡执行 `sdcardWriteBlocks()` / `sdcardTrim()` 时返回 `SDCARD_STATUS_WRITE_PROTECTED`。
- 项目侧 `diskio.c` 能直接用该模块承接 `disk_read/disk_write/disk_ioctl`。