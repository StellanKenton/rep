---
doc_role: repo-lib-leaf
layer: leaf
module: fatfs
status: active
portability: reusable
public_headers:
  - ff.h
  - ffconf.h
  - diskio.h
  - integer.h
core_files:
  - ff.c
  - ff.h
  - ffconf.h
  - diskio.h
  - integer.h
  - option/unicode.c
  - option/cc932.c
  - option/cc936.c
  - option/cc949.c
  - option/cc950.c
  - option/ccsbcs.c
port_files: []
debug_files: []
depends_on:
  - ../lib.md
forbidden_depends_on:
  - 直接放入项目专用 diskio.c
  - 直接放入示例 exfuns 或测试程序
required_hooks:
  - disk_initialize
  - disk_status
  - disk_read
  - disk_write
  - disk_ioctl
optional_hooks:
  - ff_memalloc
  - ff_memfree
common_utils: []
copy_minimal_set:
  - fatfs/ff.c
  - fatfs/ff.h
  - fatfs/ffconf.h
  - fatfs/diskio.h
  - fatfs/integer.h
  - fatfs/option/
read_next: []
---

# fatfs 目录文档

这是当前目录的权威入口文档。

## 1. 目录定位

该目录保存 FatFs 第三方库本体和通用头文件，不包含具体设备 glue。

当前目录从 `example/FATFS/src` 收敛了可复用库文件；示例、测试和平台绑定文件没有进入本目录。

## 2. 当前保留内容

| 路径 | 作用 |
| --- | --- |
| `ff.c/.h` | FatFs 核心实现与公共 API |
| `ffconf.h` | 当前仓库使用的 FatFs 配置头 |
| `diskio.h` | 底层磁盘接口声明 |
| `integer.h` | FatFs 基本整数类型定义 |
| `option/unicode.c` | LFN/编码转换入口 |
| `option/cc*.c` | 对应代码页转换表 |

## 3. 明确保留在项目侧的内容

下面这些内容是项目绑定或示例层，不在本目录：

- `diskio.c`：具体磁盘设备 glue，应该落在 `User/port/` 或项目侧适配层。
- `option/syscall.c`：OS/heap 相关示例实现，按目标工程自行适配。
- `exfuns/`、`fattester.*`、`doc/`：示例、测试、说明材料，不属于库本体。

## 4. 接入边界

- FatFs core 只依赖 `diskio.h` 声明的磁盘接口，不应直接包含具体 SD、Flash、USB 设备头。
- 若 `_USE_LFN == 3`，则需要项目侧提供 `ff_memalloc/ff_memfree`。
- 若开启 `_FS_REENTRANT`，同步对象类型和互斥函数也必须由项目侧提供。

## 5. 复制到其他工程的最小步骤

1. 复制当前目录中的核心文件和 `option/` 编码文件。
2. 在目标工程实现 `diskio.c` 或等价 glue 文件。
3. 按目标介质与 OS 能力调整 `ffconf.h`。