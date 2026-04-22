---
doc_role: repo-lib-leaf
layer: leaf
module: littlefs
status: active
portability: reusable
public_headers:
  - lfs.h
  - lfs_util.h
  - lfs_config.h
core_files:
  - lfs.c
  - lfs.h
  - lfs_util.c
  - lfs_util.h
  - lfs_config.h
port_files: []
debug_files: []
depends_on:
  - ../lib.md
forbidden_depends_on:
  - 直接依赖 GD25Qxxx 或项目 BSP
required_hooks:
  - struct lfs_config.read/prog/erase/sync
optional_hooks: []
common_utils: []
copy_minimal_set:
  - littlefs/lfs.c
  - littlefs/lfs.h
  - littlefs/lfs_util.c
  - littlefs/lfs_util.h
  - littlefs/lfs_config.h
read_next: []
---

# littlefs 目录文档

这是当前目录的权威入口文档。

## 1. 目录定位

该目录保存 littlefs 第三方库本体，以及当前仓库统一使用的 `lfs_config.h`。

## 2. 文件职责

| 文件 | 作用 |
| --- | --- |
| `lfs.c/.h` | littlefs 核心实现与公共 API |
| `lfs_util.c/.h` | littlefs 工具层实现 |
| `lfs_config.h` | 当前仓库统一的编译期裁剪配置 |

## 3. 接入边界

- 该目录不直接认识具体 flash 型号。
- `read/prog/erase/sync` 必须由上层通过 `struct lfs_config` 注入。
- 分区布局、容量裁剪、首次挂载恢复、日志策略应放在 `User/manager` 或 `User/port`。

## 4. 复制到其他工程的最小步骤

1. 复制本目录全部核心文件。
2. 在目标工程提供 `struct lfs_config` 的块读写擦接口。
3. 重新确认 `lfs_config.h` 中的裁剪宏是否符合目标工程。