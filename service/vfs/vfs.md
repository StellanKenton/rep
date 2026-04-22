---
doc_role: layer-guide
layer: service
module: vfs
status: active
portability: layer-dependent
public_headers:
    - vfs.h
    - vfs_littlefs.h
    - vfs_debug.h
core_files:
    - vfs.c
    - vfs.h
    - vfs.md
    - vfs_littlefs.c
    - vfs_littlefs.h
debug_files:
    - vfs_debug.c
    - vfs_debug.h
port_files: []
depends_on:
    - ../rtos/rtos.md
    - ../log/log.md
forbidden_depends_on:
    - core 直接依赖 User/ 下的项目头文件
    - core 直接写死具体 flash 型号、SPI 口号或分区地址
required_hooks:
    - 项目侧提供 mount 注册与 block device 绑定
optional_hooks:
    - 额外 backend，例如 fatfs
common_utils:
    - littlefs
    - log
    - rtos
copy_minimal_set:
    - service/vfs/vfs.h
    - service/vfs/vfs.c
    - service/vfs/vfs_littlefs.h
    - service/vfs/vfs_littlefs.c
read_next:
    - ../rtos/rtos.md
    - ../log/log.md
---

# VFS Service Guide

这是 `rep/service/vfs/` 目录的权威入口文档。

## 1. 目标与边界

`vfs` 负责统一绝对路径、挂载点和基础文件语义，对上暴露与具体文件系统实现解耦的轻量文件接口。

本层负责：

- 维护固定数量的挂载点表和最长前缀路径分发。
- 提供统一的 `mount`、`format`、`stat`、`ls`、`read/write`、`copy/move` 语义。
- 把 littlefs 这类具体库吸收到 backend 中。
- 提供可裁剪的 console 调试入口。

本层不负责：

- 具体 flash、SD、USB MSC 的板级驱动和地址布局。
- 当前工程的启动顺序、模式切换和业务流程。
- 凭空提供未接入的文件系统栈。

## 2. 当前实现范围

当前仓库已实现：

- `vfs` core。
- `littlefs` backend。
- 基于 console 的 `vfs_debug` shell。
- 项目侧 `/mem` littlefs 挂载绑定。

当前仓库未实现：

- FatFs backend。
- `/sd` 可运行绑定。

文档里允许规划这些能力，但代码 contract 以当前仓库真实实现为准。

## 3. 目录职责

| 文件 | 层级 | 职责 |
| --- | --- | --- |
| `vfs.h` | core | 公共类型、错误码、挂载配置、统一 API |
| `vfs.c` | core | 挂载表、路径规范化、路径分发、跨挂载点 copy/move |
| `vfs_littlefs.h` | backend | littlefs block device 配置和 backend 对外入口 |
| `vfs_littlefs.c` | backend | littlefs 错误码映射、mount/format/file/dir 适配 |
| `vfs_debug.h/.c` | debug | 面向单个挂载根目录的 shell 风格命令注册 |

## 4. 核心 contract

### 4.1 路径模型

- 只接受绝对路径。
- 使用 `/` 作为分隔符。
- 折叠重复 `/`，接受 `.`，支持受限的 `..` 归一化。
- `/` 是 VFS 根目录，用于枚举挂载点。
- 挂载点匹配使用最长前缀规则。

### 4.2 并发边界

- 所有公开 API 只允许在线程/任务上下文调用，不允许在 ISR 中调用。
- `vfs` 内部用单互斥保护挂载表和 backend 访问。
- `vfsListDir()` 在调用 visitor 前会释放内部互斥；visitor 可以再进入其他 VFS API，但目录内容若被并发修改，不保证枚举快照一致性。
- 调用者不能跨任务共享 backend 私有文件上下文。

### 4.3 失败语义

- `vfsRename()` 只保证同挂载点内重命名。
- `vfsMove()` 跨挂载点时退化为 `copy + delete`，不保证原子性。
- 当前实现只支持文件级 `copy/move`；目录复制或目录跨挂载搬运返回 `eVFS_UNSUPPORTED`。
- `vfsDelete()` 只删除单个节点；递归删除由 debug 或上层业务显式编排。

## 5. 项目侧接入方式

项目侧应在 `User/port/` 完成 backend 绑定，然后在系统启动中：

1. 调 `vfsInit()`。
2. 注册 `/mem` 等挂载点。
3. 对自动挂载点执行 mount。
4. 由 `memory.*` 这类兼容层把旧接口映射到具体挂载根，例如 `/mem`。

## 6. 公共函数使用契约表

| 公共函数 | 用途 | 允许调用层 | 调用前提 | 失败处理 |
| --- | --- | --- | --- | --- |
| `vfsInit` | 初始化 vfs 全局状态 | system、manager | RTOS 互斥可用 | 失败后不要继续注册挂载 |
| `vfsRegisterMount` | 注册挂载点 | system、port | `mountPath/backendOps/backendContext` 合法 | 返回 `false` 时视为该挂载不可用 |
| `vfsMount` | 挂载具体卷 | system、manager 兼容层 | 挂载已注册 | 失败后读取 `vfsGetStatus()->lastError` |
| `vfsGetInfo` | 查询节点属性 | manager、debug | 路径已归一化或可归一化 | 节点不存在时返回 `false` |
| `vfsListDir` | 枚举目录项 | manager、debug | 路径是目录 | visitor 返回 `false` 视为调用者主动停止 |
| `vfsReadFile` | 读取完整文件 | manager、debug | 调用方缓冲区足够 | 缓冲区不足返回 `false` |
| `vfsWriteFile` | 覆盖写完整文件 | manager、debug | 目标挂载非只读 | 写失败后由调用方决定重试 |
| `vfsCopy/vfsMove` | 跨挂载点搬运 | manager、debug | 源存在，目标路径合法 | 不保证事务性，失败后检查目标残留 |

## 7. 复制到其他工程的最小步骤

1. 复制 `rep/service/vfs/`。
2. 保留 littlefs 时同步复制 littlefs 依赖和对应 port 绑定。
3. 在目标工程的 `User/port/` 重新实现 block device 绑定。
4. 在目标工程启动序列里完成 `vfsInit + mount register + mount`。