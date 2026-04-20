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
    - ../log/log.md
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
    - ../log/log.md
---

# Update Service Guide

这是 `rep/service/update/` 目录的权威入口文档。

## 1. 本层目标和边界

`update` 负责把“升级文件存放在哪里、当前设备要改写哪个逻辑区、失败时是否回滚、成功后何时跳转”统一收敛成一套可复用的升级状态机。

本层负责：

- 维护统一的升级状态机。
- 定义镜像头、升级记录、错误码、状态码等稳定数据结构。
- 把升级流程建立在“逻辑存储设备 + 逻辑区域”的抽象之上，而不是写死某个 Flash 芯片。
- 统一升级请求、镜像校验、备份、编程、回滚和结果落盘顺序。
- 对上提供 `init/process/getStatus` 这类稳定编排接口。
- 只沉淀可跨工程复用的通用能力，例如通用状态机、通用升级状态、通用镜像结构、通用 header / record 调度策略。

本层不负责：

- 蓝牙、串口、CAN 等升级包传输协议。
- 具体内部 Flash 或外部 Flash 驱动细节。
- 具体跳转 App 前的芯片寄存器清理实现细节。
- 上位机升级包格式设计。
- 当前工程的具体升级流程装配、业务触发时机、项目模式切换和复位跳转策略；这些内容应放在 `User/` 目录下的项目实现层。

一句话约束：`update core` 只理解“逻辑镜像”和“逻辑区域”，不直接理解 `GD25Q32_MEM`、`SPI1`、`0x08020000` 这类项目细节。

进一步约束：`rep/service/update/` 只负责通用升级框架，不承载当前工程的升级 manager。`User/` 目录下的 update manager 负责把具体工程的升级流程串起来，例如进入升级模式、接收升级请求、驱动 Boot/App 切换、决定何时调用通用 `update` 接口；而 `rep/service/update/` 只负责这些项目实现会复用的公共状态机、公共升级状态、公共镜像结构和公共 header 调度语义。

## 2. 推荐目录拆分

为了把通用性做出来，建议 `rep/service/update/` 采用下面的 core-port-debug 三层拆分。

| 文件 | 层级 | 职责 |
| --- | --- | --- |
| `update.h` | core | 对外稳定 API、状态枚举、错误码、镜像头、升级记录、逻辑区域枚举 |
| `update.c` | core | 升级状态机、镜像校验流程、回滚决策、公共读写编排 |
| `update_port.h` | port | 存储设备枚举、逻辑区域映射、port hook、默认绑定结构 |
| `update_port.c` | port | 把当前工程的内部 Flash、外部 Flash1、外部 Flash2 绑定到逻辑区域 |
| `update_debug.h/.c` | debug | 状态切换打印、进度打印、诊断 dump，可按需裁剪 |

如果后续状态机继续变复杂，再把 `update.c` 拆成两个私有实现文件：

- `update_storage.c`：区域读写、镜像头读写、升级记录落盘。
- `update_state.c`：状态机迁移和失败回滚逻辑。

## 3. 核心抽象模型

### 3.1 逻辑存储设备

port 层先声明“设备”，用于区分不同物理存储介质。

推荐枚举：

| 枚举值 | 含义 | 典型绑定 |
| --- | --- | --- |
| `E_UPDATE_STORAGE_INTERNAL_FLASH` | MCU 内部 Flash | `drvmcuflash` |
| `E_UPDATE_STORAGE_EXT_FLASH1` | 外部 Flash 1 | `gd25qxxx dev0` |
| `E_UPDATE_STORAGE_EXT_FLASH2` | 外部 Flash 2 | 第二颗 SPI NOR / EEPROM |
| `E_UPDATE_STORAGE_MAX` | 设备上限 | 仅内部使用 |

设备只表达“在哪颗存储上”，不表达“这个区域的业务语义是什么”。

### 3.2 逻辑区域

core 层再声明“区域”，用于表达升级流程中的稳定语义。

推荐枚举：

| 枚举值 | 含义 | 是否必须 |
| --- | --- | --- |
| `E_UPDATE_REGION_BOOT_RECORD` | 升级记录区，保存请求标志、错误码、序列号 | 必须 |
| `E_UPDATE_REGION_RUN_APP` | 当前运行 App 区 | 必须 |
| `E_UPDATE_REGION_STAGING_APP` | 待升级镜像暂存区 | 必须 |
| `E_UPDATE_REGION_STAGING_APP_HEADER` | 待升级镜像头记录 | 必须 |
| `E_UPDATE_REGION_BACKUP_APP` | 当前 App 备份区 | 推荐 |
| `E_UPDATE_REGION_BACKUP_APP_HEADER` | 当前 App 备份区头记录 | 推荐 |
| `E_UPDATE_REGION_BOOT_IMAGE` | Boot 镜像区 | 可选 |
| `E_UPDATE_REGION_FACTORY_APP` | 出厂保底镜像区 | 可选 |
| `E_UPDATE_REGION_MAX` | 区域上限 | 仅内部使用 |

这个设计的关键点是：

- `RUN_APP` 表示“最终要跳转的 App”。
- `STAGING_APP` 表示“已经下载但尚未写入运行区的新镜像”。
- `STAGING_APP_HEADER` 表示“待升级镜像头记录”，用于保存镜像头和事务元数据。
- `BACKUP_APP` 表示“升级前对旧镜像做的回滚快照”。
- `BACKUP_APP_HEADER` 表示“回滚快照区头记录”，用于保存备份镜像头和事务元数据。
- `BOOT_RECORD` 表示“升级事务元数据”，不属于镜像区。

core 永远按逻辑区域工作，不允许上层直接传裸地址。

### 3.3 逻辑区域描述符

port 层应给每个逻辑区域提供一份区域描述。推荐结构如下：

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

字段含义：

- `storageId`: 该逻辑区域落在哪个物理存储上。
- `startAddress`: 该区域在对应存储中的起始地址。
- `size`: 区域总长度。
- `eraseUnit`: 最小擦除粒度。
- `progUnit`: 最小编程粒度。
- `headerReserveSize`: 当镜像头和镜像正文共用同一区域时，表示区域起始处预留给镜像头的字节数；若已经拆分 `*_HEADER` 独立逻辑区，正文区通常设为 `0`，头区大小直接由对应 `*_HEADER` 区域的 `size` 描述。
- `isReadable/isWritable/isExecutable`: 约束该区域的访问语义。

## 4. 推荐公共数据结构

### 4.1 镜像头

每个镜像头都采用下面的结构。

推荐结构：

```c
typedef enum eUpdateImageState {
    E_UPDATE_IMAGE_STATE_EMPTY = 0,
    E_UPDATE_IMAGE_STATE_RECEIVING,
    E_UPDATE_IMAGE_STATE_READY,
    E_UPDATE_IMAGE_STATE_INVALID,
} eUpdateImageState;

typedef struct stUpdateImageHeader {
    uint32_t magic;
    uint32_t headerVersion;
    uint32_t imageType;
    uint32_t imageVersion;
    uint32_t imageSize;
    uint32_t imageCrc32;
    uint32_t writeOffset;
    uint32_t imageState;
    uint32_t reserved[7];
    uint32_t headerCrc32;
} stUpdateImageHeader;
```

镜像头至少要回答下面问题：

- 这是不是一个有效镜像。
- 当前镜像属于 Boot、App 还是工厂镜像。
- 镜像实际长度是多少。
- 镜像是否已经完整接收。
- 断点续传应该从哪里继续。

说明：

- `stUpdateImageHeader` 建议继续作为“业务载荷”结构使用，不直接承担掉电安全落盘控制字段。
- 若镜像头会频繁更新 `writeOffset`、`imageState` 等字段，建议把它包进元数据槽日志中追加写入，而不是每次都原地擦除重写同一地址。

### 4.2 升级记录

升级记录区不要只放一个字节标志位，而要显式保存事务状态。

推荐结构：

```c
typedef enum eUpdateRequestFlag {
    E_UPDATE_REQUEST_IDLE = 0,
    E_UPDATE_REQUEST_PROGRAM_APP,
    E_UPDATE_REQUEST_PROGRAM_BOOT,
    E_UPDATE_REQUEST_BACKUP_DONE,
    E_UPDATE_REQUEST_PROGRAM_DONE,
    E_UPDATE_REQUEST_RUN_APP,
    E_UPDATE_REQUEST_FAILED,
} eUpdateRequestFlag;

typedef struct stUpdateBootRecord {
    uint32_t magic;
    uint32_t requestFlag;
    uint32_t lastError;
    uint32_t targetRegion;
    uint32_t stagingCrc32;
    uint32_t backupCrc32;
    uint32_t imageSize;
    uint32_t sequence;
    uint32_t reserved[7];
    uint32_t recordCrc32;
} stUpdateBootRecord;
```

设计原则：

- `sequence` 用于区分新旧事务，避免掉电后重复消费脏请求。
- `targetRegion` 用于表达本次升级目标是 App、Boot 还是其他镜像。
- `lastError` 用于掉电后诊断失败点。
- `stUpdateBootRecord` 建议作为“事务载荷”使用，真正落盘时再包一层槽元数据，避免先擦后写导致整条记录丢失。

### 4.3 运行时状态

core 内部建议维护一份只驻留 RAM 的运行时状态：

```c
typedef struct stUpdateStatus {
    eUpdateState state;
    uint32_t lastTickMs;
    uint32_t currentOffset;
    uint32_t totalSize;
    uint32_t activeCrc32;
    bool isUpdateRequested;
    bool isRollbackActive;
} stUpdateStatus;
```

这份状态不替代升级记录，只负责当前上电周期中的状态推进和进度统计。

## 5. port 层契约

### 5.1 存储操作接口

不同存储设备应统一成一份读写擦接口表。推荐结构：

```c
typedef struct stUpdateStorageOps {
    bool (*init)(void);
    bool (*read)(uint32_t address, uint8_t *buffer, uint32_t length);
    bool (*write)(uint32_t address, const uint8_t *buffer, uint32_t length);
    bool (*erase)(uint32_t address, uint32_t length);
    bool (*isRangeValid)(uint32_t address, uint32_t length);
} stUpdateStorageOps;
```

约束如下：

- `read/write/erase` 都以“该设备内部地址”作为入参。
- core 通过 `region.startAddress + offset` 计算实际地址，但不关心底层是 MCU Flash 还是 SPI NOR。
- port 必须保证 `erase` 能覆盖完整擦除单元。
- 若设备只支持 sector/block 擦除，port 负责向上适配。

### 5.2 默认绑定加载

port 层至少要提供三类接口：

- `updatePortLoadDefaultCfg()`：装载默认区域映射。
- `updatePortGetStorageOps(storageId)`：返回某个存储设备的操作表。
- `updatePortGetRegionMap(regionId, *cfg)`：返回某个逻辑区域的绑定配置。

建议 `updateInit()` 的顺序是：

1. 读取默认配置。
2. 初始化所有被引用到的存储设备。
3. 校验关键区域是否存在且不重叠。
4. 再进入 `CHECK_REQUEST` 状态。

### 5.3 区域合法性检查

在 `updateInit()` 阶段建议统一做下面检查：

- `RUN_APP`、`STAGING_APP`、`BOOT_RECORD` 必须存在。
- `RUN_APP` 必须 `isExecutable = true`。
- `STAGING_APP` 和 `BACKUP_APP` 必须 `isWritable = true`。
- 所有采用“头体同区”布局的区域长度都必须大于其 `headerReserveSize`；若已拆分 `*_HEADER` 独立区域，则正文区可令 `headerReserveSize = 0`。
- 同一设备上的多个区域不允许地址重叠。
- 若启用回滚，`BACKUP_APP.size` 必须不小于 `RUN_APP.size`。

### 5.4 长耗时状态与看门狗

升级流程里最容易触发看门狗超时的，不是普通状态切换，而是 Flash 擦除和整镜像搬运这类长耗时动作。`ERASE_TARGET`、`PREPARE_BACKUP`、`BACKUP_RUN_APP`、`PROGRAM_TARGET`、`ROLLBACK_ERASE_TARGET`、`ROLLBACK_PROGRAM_BACKUP` 这几类状态一旦实现成“单次调用里连续擦完整区/拷完整镜像”，就可能长时间占住状态机，导致系统在升级中途反复复位。

因此建议把 `updatePortFeedWatchdog()` 视为长耗时路径上的标准可选 hook，而不是可有可无的调试接口。约束如下：

- 只要工程启用了硬件 watchdog，core 在执行 sector/page 粒度的擦除、备份、编程、回滚循环时，都应在每个可恢复的分块之间调用一次 `updatePortFeedWatchdog()`。
- `ERASE_TARGET` 和 `ROLLBACK_ERASE_TARGET` 至少要在每个擦除单元完成后喂狗一次，不能等整片目标区擦完再统一喂狗。
- `BACKUP_RUN_APP`、`PROGRAM_TARGET`、`ROLLBACK_PROGRAM_BACKUP` 至少要在每个搬运块完成并通过本块读回/边界检查后喂狗一次，避免整镜像复制期间超过系统 watchdog 窗口。
- 若 `PREPARE_BACKUP` 同时包含元数据区擦除和镜像头落盘，也应在每个擦除单元或每次记录提交之间喂狗，避免元数据区轮转时卡死。
- core 只调用 `updatePortFeedWatchdog()`，不允许直接 include 具体 IWDG/WWDG 驱动，也不负责决定 watchdog 的启动时机；这些仍由项目层系统管理逻辑决定。
- 若工程没有提供 `updatePortFeedWatchdog()`，则项目层必须保证 watchdog 超时时间大于最坏单状态执行时间，或者在进入升级模式前采用其他等价策略；不能默认假设 Flash 擦除一定足够快。

实现上的推荐做法是：把长耗时状态拆成“有限分块 + 每块完成后返回/续跑”或“有限分块 + 块间喂狗”的形式，而不是在一次 `updateProcess()` 中完成整段耗时操作。这样既更容易满足 watchdog 时序，也更容易做进度统计和异常恢复。

## 6. 通用状态机

### 6.1 推荐状态定义

建议 core 层采用下面这套状态，基本覆盖 App 升级、Boot 升级和回滚三类场景。

| 状态 | 作用 |
| --- | --- |
| `E_UPDATE_STATE_UNINIT` | 未初始化 |
| `E_UPDATE_STATE_IDLE` | 空闲 |
| `E_UPDATE_STATE_CHECK_REQUEST` | 读取升级记录，判断是否存在升级事务 |
| `E_UPDATE_STATE_VALIDATE_STAGING` | 校验暂存镜像头和整镜像 CRC |
| `E_UPDATE_STATE_PREPARE_BACKUP` | 擦除备份区并写入初始镜像头 |
| `E_UPDATE_STATE_BACKUP_RUN_APP` | 将运行区内容备份到备份区 |
| `E_UPDATE_STATE_VERIFY_BACKUP` | 校验备份区镜像 |
| `E_UPDATE_STATE_ERASE_TARGET` | 擦除目标运行区 |
| `E_UPDATE_STATE_PROGRAM_TARGET` | 将暂存镜像写入目标运行区 |
| `E_UPDATE_STATE_VERIFY_TARGET` | 校验目标运行区镜像 |
| `E_UPDATE_STATE_COMMIT_RESULT` | 更新升级记录和镜像状态 |
| `E_UPDATE_STATE_ROLLBACK_ERASE_TARGET` | 回滚前擦除目标区 |
| `E_UPDATE_STATE_ROLLBACK_PROGRAM_BACKUP` | 将备份镜像恢复到目标区 |
| `E_UPDATE_STATE_VERIFY_ROLLBACK` | 校验回滚结果 |
| `E_UPDATE_STATE_JUMP_TARGET` | 跳转目标镜像 |
| `E_UPDATE_STATE_ERROR` | 停留错误态 |

### 6.2 主流程

推荐迁移顺序：

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

### 6.3 失败与回滚分层

建议把失败划成两段：

- 擦除目标区之前失败：不触发回滚，直接保留旧运行区。
- 擦除目标区之后失败：若备份有效，则必须走回滚链路。

回滚链路顺序：

1. `ROLLBACK_ERASE_TARGET`
2. `ROLLBACK_PROGRAM_BACKUP`
3. `VERIFY_ROLLBACK`
4. 成功则更新记录为 `FAILED` 并进入 `JUMP_TARGET`
5. 回滚失败则留在 `ERROR`

### 6.4 每个状态的最小动作

| 状态 | 最小动作 | 成功后去向 | 失败后去向 |
| --- | --- | --- | --- |
| `CHECK_REQUEST` | 读升级记录并检查请求标志 | `VALIDATE_STAGING` 或 `JUMP_TARGET` | `ERROR` |
| `VALIDATE_STAGING` | 校验镜像头和整镜像 CRC | `PREPARE_BACKUP` 或 `ERASE_TARGET` | `ERROR` |
| `PREPARE_BACKUP` | 分块擦备份区并写初始头，长耗时段按擦除单元喂狗 | `BACKUP_RUN_APP` | `ERROR` |
| `BACKUP_RUN_APP` | 分块备份运行区数据，块间喂狗 | `VERIFY_BACKUP` | `ERROR` |
| `VERIFY_BACKUP` | 校验备份 CRC | `ERASE_TARGET` | `ERROR` |
| `ERASE_TARGET` | 分块擦目标区，至少按擦除单元喂狗 | `PROGRAM_TARGET` | `ROLLBACK_ERASE_TARGET` 或 `ERROR` |
| `PROGRAM_TARGET` | 分块写目标区，块间喂狗 | `VERIFY_TARGET` | `ROLLBACK_ERASE_TARGET` 或 `ERROR` |
| `VERIFY_TARGET` | 校验目标区 CRC | `COMMIT_RESULT` | `ROLLBACK_ERASE_TARGET` 或 `ERROR` |
| `COMMIT_RESULT` | 清请求、写结果、更新镜像头 | `JUMP_TARGET` | `ERROR` |
| `ROLLBACK_ERASE_TARGET` | 分块擦目标区供回写，至少按擦除单元喂狗 | `ROLLBACK_PROGRAM_BACKUP` | `ERROR` |
| `ROLLBACK_PROGRAM_BACKUP` | 分块将备份回写到目标区，块间喂狗 | `VERIFY_ROLLBACK` | `ERROR` |
| `VERIFY_ROLLBACK` | 校验回滚镜像 | `JUMP_TARGET` | `ERROR` |

## 7. 镜像与记录的布局建议

### 7.1 镜像区与镜像头区布局

当工程已经把镜像头单独划分为 `E_UPDATE_REGION_STAGING_APP_HEADER`、`E_UPDATE_REGION_BACKUP_APP_HEADER` 这类独立区域时，推荐按下面的方式建模：

| 逻辑区域 | 推荐内容 | 说明 |
| --- | --- | --- |
| `STAGING_APP_HEADER` | 一个 `stUpdateImageHeader` 或双槽镜像头记录 | 只保存待升级镜像的头部与事务元数据 |
| `STAGING_APP` | 纯镜像正文数据 | 不再在起始处内嵌镜像头 |
| `BACKUP_APP_HEADER` | 一个 `stUpdateImageHeader` 或双槽镜像头记录 | 只保存备份镜像头和回滚元数据 |
| `BACKUP_APP` | 纯备份镜像正文数据 | 不再额外切出头部保留段 |

如果工程仍沿用“头体同区”布局，也可以继续把镜像头放在 `STAGING_APP` / `BACKUP_APP` 起始处，再通过 `headerReserveSize` 描述保留空间；但一旦已经存在 `*_HEADER` 独立逻辑区，就不建议同时保留正文区头部预留，否则容易出现两套头信息漂移。

推荐访问约束：

- `VALIDATE_STAGING` 先读取 `STAGING_APP_HEADER`，再按 `STAGING_APP` 的正文范围做 CRC 校验。
- `PREPARE_BACKUP` 先擦写 `BACKUP_APP_HEADER`，再写入 `BACKUP_APP` 正文。
- `COMMIT_RESULT` 更新镜像状态时，应优先修改对应的 `*_HEADER` 区域，而不是去改正文区起始地址。

这样有三个好处：

- 镜像头更新不会影响正文区地址规划，适合后续扩展双槽头或附加事务字段。
- `STAGING_APP` 和 `BACKUP_APP` 都可以被视为“纯镜像数据区”，CRC 计算边界更直观。
- 当 Boot/App/Factory 镜像共存时，头部区和正文区可以独立调大或调小，port 映射更清晰。

### 7.2 升级记录区布局

`BOOT_RECORD`、`STAGING_APP_HEADER`、`BACKUP_APP_HEADER` 这类元数据区，不建议继续使用“单份结构体 + 每次整 sector 擦除后重写”的方式。默认推荐先采用更容易实现和验证的“双擦除单元轮转”方案；只有当 `writeOffset`、镜像头 checkpoint 这类字段更新频率明显偏高时，再升级为“单元内多 slot 追加写”。

推荐默认最小布局：

- 每个元数据逻辑区至少预留 `2` 个擦除单元，分别作为 `active sector` 和 `standby sector`。
- 每个擦除单元默认只保存 `1` 份完整元数据副本，不强制在单元内继续细分多个 `slot`。
- `BOOT_RECORD`、`STAGING_APP_HEADER`、`BACKUP_APP_HEADER` 共用同一套头部封装和恢复逻辑，只是 payload 分别是 `stUpdateBootRecord` 或 `stUpdateImageHeader`。

推荐记录封装：

```c
typedef struct stUpdateMetaRecord {
    uint32_t recordMagic;
    uint32_t sequence;
    uint32_t payloadLength;
    uint32_t payloadCrc32;
    uint32_t headerCrc32;
    uint8_t payload[UPDATE_META_PAYLOAD_MAX];
    uint32_t commitMarker;
} stUpdateMetaRecord;
```

字段约束：

- `recordMagic`：记录魔数，用于快速过滤脏数据。
- `sequence`：全局单调递增版本号，恢复时永远选择最新一份已提交记录。
- `payloadLength`：当前 payload 实际长度，便于同一记录格式复用到 bootrecord 和 image header。
- `payloadCrc32`：只覆盖 payload 自身，校验业务载荷是否完整。
- `headerCrc32`：覆盖记录头字段，不含 `commitMarker`。
- `commitMarker`：最后一个写入字，擦除态为 `0xFFFFFFFF`，提交成功后写成固定已提交值，例如 `0x00000000`。

推荐写入顺序：

1. 先读取 `active sector` 中当前生效记录，并决定下一次写入目标是继续使用空闲 `standby sector`，还是先擦除旧 `standby sector`。
2. 擦除目标 `standby sector`。
3. 写入 `recordMagic/sequence/payloadLength/payloadCrc32/headerCrc32/payload`，此时 `commitMarker` 保持擦除态。
4. 回读校验记录头 CRC 和 payload CRC。
5. 最后单独写入 `commitMarker`，把该记录标记为“已提交”。
6. 只有在新记录提交成功后，旧 `active sector` 才允许被擦除并转为新的 `standby sector`。

这个顺序的关键点是：

- 如果在步骤 `2` 到 `4` 中断电，旧 `active sector` 中的已提交记录仍然存在。
- 如果在步骤 `5` 前断电，新记录因为 `commitMarker` 未提交会被恢复逻辑忽略。
- 如果在步骤 `5` 后断电，新记录已经自洽，恢复时可直接接管。

推荐恢复逻辑：

1. 扫描两个擦除单元中的记录头。
2. 仅接受同时满足下面条件的记录：
   - `recordMagic` 正确。
   - `commitMarker` 为已提交值。
   - `headerCrc32` 校验通过。
   - `payloadCrc32` 校验通过。
3. 在所有有效记录中选择 `sequence` 最大的一条作为当前生效记录。
4. 若两个擦除单元都没有有效记录，则回退到“无记录 / 空镜像头”语义。

推荐轮转逻辑：

- 正常情况下始终保持“一个 `active sector` + 一个 `standby sector`”。
- 每次更新都把新记录完整写到 `standby sector`，而不是先擦 `active sector`。
- 确认 `standby sector` 中的新记录可读后，再擦除旧 `active sector` 并交换角色。
- 整个过程中，必须保证“新记录提交成功前，旧 sector 中至少保留一条已提交记录”。

对高频字段的额外建议：

- `writeOffset` 不要每收到一个小包就落盘一次，建议按固定粒度 checkpoint，例如每 `4KB`、`8KB` 或每个完整传输包提交一次。
- `requestFlag`、`imageState` 这类状态字段只有在状态切换点才落盘，不要在轮询过程中重复写同值。
- 如果经过老化评估发现“双擦除单元轮转”仍然擦得太频繁，再把单个擦除单元内部细分为多个固定长度 `slot`，改成“单元内追加写 + 单元间换页”的增强方案；这应视为优化项，不再作为基础实现门槛。

这样收敛后的默认方案更适合作为 `rep/service/update` 的基线实现，因为它先解决了最关键的掉电一致性问题：

- 避免“刚擦完还没写回”时断电导致上一份记录消失。
- 保证恢复逻辑只需要在两个擦除单元之间选出最新一份已提交记录，复杂度更低。
- 在大多数升级状态切换频度不高的项目里，已经足够满足可靠性和寿命要求。

如果后续确认 `BOOT_RECORD` 或镜像头属于高频元数据，再在这个双单元轮转基线上增加“多 slot 追加写”，这样演进路径会更稳，port 层也更容易分阶段实现。

## 8. 当前工程可参考的绑定示例

当前工程已有一套项目侧升级实现，可作为 `rep/service/update` 的实例化参考：

- `User/manager/update/update.c`
- `User/manager/update/update.h`
- `User/port/drvmcuflash_port.c`
- `User/port/drvmcuflash_port.h`

这里要明确区分“项目实现”和“公共层”：

- `User/manager/update/` 负责当前工程的具体升级流程编排，是项目侧 update manager。
- `rep/service/update/` 负责可移植的通用升级能力，只输出公共状态机、公共状态定义、公共镜像头和升级记录结构，以及通用 header / record 调度规则。
- 如果需求是修改当前产品的升级入口、升级模式切换、与无线或串口流程的联动，优先改 `User/` 目录，不应把这些项目策略回灌到 `rep/service/update/`。
- 如果需求是抽象所有工程都会复用的升级语义，例如状态机状态、镜像结构、元数据落盘规则，才应修改 `rep/service/update/`。

现有绑定关系大致是：

| 逻辑区域 | 当前工程绑定 |
| --- | --- |
| `RUN_APP` | MCU 内部 Flash `0x08020000 ~ 0x0807FFFF` |
| `STAGING_APP_HEADER` | 外部 Flash 中 `app2` 对应的独立镜像头区 |
| `STAGING_APP` | 外部 Flash 中 `app2` 对应的镜像正文区 |
| `BACKUP_APP_HEADER` | 外部 Flash 中 `app1` 对应的独立备份镜像头区 |
| `BACKUP_APP` | 外部 Flash 中 `app1` 对应的备份镜像正文区 |
| `BOOT_RECORD` | 外部 GD25Q32 标志位 sector |

也就是说，当前工程已经不再推荐把 `stUpdateImageHeader` 内嵌在 `STAGING_APP` 或 `BACKUP_APP` 起始处，而是通过独立 `*_HEADER` 逻辑区域保存头信息。这类调整应落在 port 映射和布局文档，不应该反向污染 core 状态机语义。

这正好说明为什么 `rep/service/update` 应该抽出 port 层：

- 当前工程把升级记录放在外部 Flash。
- 下一个工程可能把升级记录放在内部 Flash 最后一个 page。
- 当前工程只有一颗外部 Flash。
- 下一个工程可能同时有外部 Flash1 和外部 Flash2。

只要逻辑区域不变，状态机不需要跟着改。

## 9. 函数指针 / port / assembly 契约表

| 名称 | 必需/可选 | 由谁实现 | 在哪里被调用 | 原型摘要 | 成功语义 | 失败语义 | 前置条件 | 备注 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `updatePortLoadDefaultCfg` | 必需 | port | `updateInit()` | `bool (*)(stUpdateCfg *cfg)` | 默认配置已装入 | `false` 表示无合法配置 | `cfg != NULL` | 用于装配默认区域映射 |
| `updatePortGetStorageOps` | 必需 | port | `updateInit()`、区域读写路径 | `const stUpdateStorageOps *(*)(uint8_t storageId)` | 返回非空操作表 | `NULL` 表示该设备不可用 | `storageId < E_UPDATE_STORAGE_MAX` | core 不缓存私有驱动句柄 |
| `updatePortGetRegionMap` | 必需 | port | `updateInit()`、状态机执行期 | `bool (*)(uint8_t regionId, stUpdateRegionCfg *cfg)` | 返回合法区域配置 | `false` 表示未绑定 | `cfg != NULL` | 区域配置应稳定只读 |
| `storage.init` | 必需 | port 绑定的存储适配器 | `updateInit()` | `bool (*)(void)` | 存储可访问 | `false` 表示初始化失败 | 底层总线已 ready | 可为空设备裁剪 |
| `storage.read` | 必需 | port 绑定的存储适配器 | 镜像头读取、CRC 校验、回滚 | `bool (*)(uint32_t, uint8_t *, uint32_t)` | 成功读出请求数据 | `false` 表示访问失败 | 地址范围合法 | 不允许部分成功但返回成功 |
| `storage.write` | 必需 | port 绑定的存储适配器 | 写镜像头、写目标镜像、写升级记录 | `bool (*)(uint32_t, const uint8_t *, uint32_t)` | 请求数据全部写入 | `false` 表示写失败 | 目标范围可写 | 不隐式执行 erase |
| `storage.erase` | 必需 | port 绑定的存储适配器 | 预擦 staging/backup/target | `bool (*)(uint32_t, uint32_t)` | 请求范围已满足擦除要求 | `false` 表示擦除失败 | 长度大于 0 | port 负责对齐到擦除粒度 |
| `storage.isRangeValid` | 可选 | port 绑定的存储适配器 | 调试校验或 init 期 | `bool (*)(uint32_t, uint32_t)` | 范围合法 | `false` 表示越界 | 无 | 未提供时由 core 用 region 信息兜底 |
| `updatePortGetTickMs` | 可选 | port | `updateProcess()` | `uint32_t (*)(void)` | 返回单调 tick | `0` 由调用方自带参数兜底 | 系统时基已启动 | 用于统一日志时间戳 |
| `updatePortFeedWatchdog` | 可选 | port | `ERASE_TARGET`、`PREPARE_BACKUP`、`BACKUP_RUN_APP`、`PROGRAM_TARGET`、回滚长耗时状态 | `void (*)(void)` | 当前 watchdog 窗口已被续期 | 空实现仅适用于系统未启用 watchdog 或超时时间足够覆盖最坏耗时 | 无 | 不要在 core 里直接碰具体 watchdog 驱动；应在分块循环间调用，而不是整段操作结束后才调用 |
| `updateDbgOnStateChanged` | 可选 | debug | `updateSetState()` | `void (*)(eUpdateState from, eUpdateState to)` | 调试输出完成 | 空实现视为关闭调试 | 日志模块可用 | 建议做成可裁剪 debug hook |

## 10. 公共函数使用契约表

| 来源模块 | 公共函数 | 允许在哪些文件调用 | 用途 | 调用前提 | 典型调用顺序 | 返回值处理 | 禁止做法 |
| --- | --- | --- | --- | --- | --- | --- | --- |
| `update` | `updateInit` | 系统初始化或 Boot 模式入口 | 装载 port 配置并建立状态机初态 | port 已链接到工程 | `updateInit -> updateProcess` | `false` 直接进入错误处理或停留 Boot | 初始化后仍手工改全局区域地址 |
| `update` | `updateProcess` | 周期调度点 | 推进单步状态机 | `updateInit` 成功 | `updateInit -> updateProcess(loop)` | 只推进一步，错误态由 `status` 读取；长耗时状态需在分块推进期间调用 `updatePortFeedWatchdog` | 在 ISR 中长时间执行整镜像搬运，或在启用 watchdog 时把整片擦除放进一次调用里 |
| `update` | `updateGetStatus` | 系统管理、诊断命令、debug | 查询当前状态与进度 | `updateInit` 后 | `updateProcess -> updateGetStatus` | 只读使用，不修改内部对象 | 调用方直接改返回的状态对象 |
| `update` | `updateRequestProgramRegion` | App 侧升级准备层 | 写升级记录，请求 Boot 执行升级 | staging 镜像已 ready | `write staging -> verify -> request program` | 失败时不得复位到 Boot | 未校验 staging 就写请求标志 |
| `update` | `updateReadBootRecord` | Boot 检查阶段、诊断命令 | 读取升级事务记录 | `BOOT_RECORD` 已绑定 | `init -> read record -> decide` | 魔数或序列无效时视为无效记录 | 业务层直接读裸地址绕过 API |
| `update` | `updateJumpToTargetIfValid` | Boot 检查阶段 | 在满足条件时跳转运行镜像 | 目标向量表已校验 | `check request -> maybe jump` | 跳转失败应进入错误态 | 未清理中断和时基就直接跳转 |
| `drvmcuflash` | `drvMcuFlashRead/Write/Erase` | 仅 port 绑定适配器 | 实现内部 Flash 存储操作 | area 已合法映射 | `init -> erase -> write -> readback` | 非 `OK` 视为失败 | core 直接 include `drvmcuflash_port.h` 取得地址宏 |
| `log` | `LOG_I/W/E` | `update.c`、`update_debug.c` | 输出状态切换和失败原因 | 日志通道已 ready | `state change -> log` | 日志失败不影响状态机 | 用 `printf` 替代统一日志宏 |

## 11. 改动落点矩阵

| 需求 | 应改文件 | 不该改的文件 |
| --- | --- | --- |
| 新增外部 Flash2 作为备份区 | `update_port.h/.c` | `update.c` 状态机主流程 |
| 把升级记录从外部 Flash 改到内部 Flash | `update_port.h/.c` | App/Boot 状态枚举 |
| 新增 Boot 镜像升级 | `update.h`、`update.c`、必要的 port 默认映射 | 具体 SPI 驱动实现 |
| 调整失败后是否自动回滚 | `update.c` | port 的设备读写接口 |
| 调整日志打印粒度 | `update_debug.c` 或 `update.c` | 底层 Flash 驱动 |
| 调整当前产品的升级流程编排或升级入口 | `User/` 目录下的项目 update manager | `rep/service/update/` 的通用状态机语义 |

## 12. 复制到其他工程的最小步骤

最小依赖集：

- `update.h/.c`
- `update_port.h/.c`
- 至少一个内部 Flash 适配器
- 至少一个 staging 区可写存储适配器

外部项目必须补齐：

- 逻辑区域到物理存储的映射表
- 存储设备统一操作接口
- 目标镜像跳转前的平台清理逻辑
- 可选的 watchdog 和 debug hook

## 13. 验证清单

- `updateInit()` 后能检测出缺失的 `RUN_APP` / `STAGING_APP` / `BOOT_RECORD` 绑定。
- `STAGING_APP` 镜像未 ready 时不会写坏 `RUN_APP`。
- `BACKUP_APP` 关闭时，状态机能按“无回滚模式”运行。
- 擦除目标区后模拟写失败时，若备份有效能进入回滚链路。
- 系统启用 watchdog 时，`ERASE_TARGET`、`PREPARE_BACKUP`、`BACKUP_RUN_APP`、`PROGRAM_TARGET` 等长耗时状态不会因为单次处理时间过长而反复复位。
- `updatePortFeedWatchdog()` 缺失或为空实现时，项目层有明确的替代策略，且不会默认把超过 watchdog 窗口的整片擦除放进一次 `updateProcess()`。
- 同一份状态机可仅通过 `update_port.c` 切换内部 Flash、外部 Flash1、外部 Flash2 的绑定。
- 在 `BOOT_RECORD` 写 `standby sector` 到一半时断电，上电后仍能读回旧 `active sector` 中的已提交记录。
- 在 `STAGING_APP_HEADER` 或 `BACKUP_APP_HEADER` 写完 payload 但尚未写 `commitMarker` 时断电，上电后半写记录必须被忽略。
- 在 `standby sector` 刚提交成功、旧 `active sector` 尚未来得及擦除时断电，上电后至少能恢复出一条最新已提交记录。
- 在两个 sector 角色切换过程中断电，不能出现“两个 sector 都无有效记录”。
- 在长时间 Flash 擦除或整镜像备份过程中人为缩短 watchdog 超时窗口，仍能通过 `updatePortFeedWatchdog()` 保持升级流程继续推进，不出现中途重复进入 `CHECK_REQUEST` 的假故障。
- 若后续启用“单元内多 slot 追加写”优化，应额外验证半槽写入忽略、满槽换页和擦除次数下降这三类增强场景。