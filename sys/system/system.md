---
doc_role: layer-guide
layer: sys
module: system
status: active
portability: reusable
public_headers:
    - system.h
core_files:
    - system.c
    - system.h
    - system.md
port_files: []
debug_files: []
depends_on: []
forbidden_depends_on:
    - system core 直接依赖 User/manager 或 User/system 的任务编排
    - system core 直接依赖 HAL、BSP 或具体 RTOS 句柄
required_hooks: []
optional_hooks: []
common_utils: []
copy_minimal_set:
    - sys/system/system.h
    - sys/system/system.c
read_next:
    - ../sys.md
---

# System Service Guide

这是 `rep/sys/system/` 目录的权威入口文档。

## 1. 本层目标和边界

`system` 负责沉淀跨项目可复用的最小系统态语义：

- 当前系统模式枚举与读写接口。
- 固件名、软件版本、硬件版本字符串。
- 模式到字符串的稳定映射。

本层不负责：

- 当前工程的任务创建、模式切换流程和 manager 调度。
- BSP 初始化、看门狗续期、外设上电、自检编排。
- console/debug 命令和项目日志策略。

一句话约束：`system core` 只保存“系统处于什么模式”和“版本字符串是什么”，不负责任何项目启动接线。

## 2. 目录拆分与当前文件职责

| 文件 | 层级 | 职责 |
| --- | --- | --- |
| `system.h` | core | 对外暴露系统模式枚举、模式访问函数，并从 `rep_config.h` 读取版本配置默认值 |
| `system.c` | core | 维护当前系统模式状态，并提供模式名和版本字符串返回 |

当前目录没有 `port` 文件，因为该核心实现不依赖项目绑定资源。

## 3. 公共函数使用契约表

| 公共函数 | 允许在哪些文件调用 | 用途 | 调用前提 | 返回值处理 | 禁止做法 |
| --- | --- | --- | --- | --- | --- |
| `systemGetMode` | `User/manager/*`、`User/system/*`、项目状态判断代码 | 读取当前系统模式 | 无 | 直接按枚举值判断 | 调用方直接读写内部静态变量 |
| `systemSetMode` | 项目模式切换点 | 切换系统模式 | `mode < eSYSTEM_MODE_MAX` | 非法枚举会被内部忽略 | 在 ISR 或底层驱动里混入业务模式跳转 |
| `systemGetModeString` | 日志、debug、状态上报 | 输出模式名 | 传入任意枚举值均可 | 未知值返回 `UNKNOWN_MODE` | 调用方自己复制一套模式字符串表 |
| `systemGetFirmwareName` | 启动日志、版本查询命令 | 获取固件名 | 无 | 只读使用 | 返回值当作可写缓冲区 |
| `systemGetFirmwareVersion` | 启动日志、版本查询命令 | 获取软件版本字符串 | 无 | 只读使用 | 在其他目录重复定义版本字符串 |
| `systemGetHardwareVersion` | 启动日志、版本查询命令 | 获取硬件版本字符串 | 无 | 只读使用 | 在其他目录重复定义版本字符串 |

## 4. 改动落点矩阵

| 想改什么 | 应修改文件 | 不应修改文件 |
| --- | --- | --- |
| 新增公共系统模式枚举 | `system.h` | `User/system/sysmgr.c` |
| 修改模式字符串显示 | `system.c` | `User/system/system_debug.c` |
| 修改当前项目启动流程 | `User/system/sysmgr.c`、`User/system/systask.c` | `system.c` |
| 修改项目任务优先级或周期 | `User/system/systask.h` | `system.h` |
| 修改项目版版本号 | `User/rep_config.h` | 各 manager 源文件 |

## 5. 复制到其他工程的最小步骤

1. 复制 `system.h` 和 `system.c`。
2. 把目录加入目标工程的 include path 和 source list。
3. 在项目自己的 `rep_config.h` 中覆盖 `SYSTEM_FIRMWARE_NAME`、`SYSTEM_FW_VER_*`、`SYSTEM_HW_VER_*`。
4. 在项目自己的 system manager 或 mode scheduler 中调用 `systemGetMode()` / `systemSetMode()`。

## 6. 当前工程中的边界说明

当前工程的 `User/system/sysmgr.c`、`User/system/systask.c`、`User/system/system_debug.c` 仍属于项目编排层，不在本目录内复用。

- `sysmgr` 负责 BSP 初始化、自检和模式切换流程。
- `systask` 负责项目任务创建与后台服务轮询。
- `system_debug` 负责 console/debug 命令。

这些文件后续若要继续抽象，应该拆成新的 `sys` 服务并补对应 `port` 契约，而不是继续塞进当前 `system` core。

当前版本约定：

- `rep/sys/system/system.h` 保留默认版本值，确保复制到新工程时可单独编译。
- `User/rep_config.h` 负责当前项目的固件名和软硬件版本覆盖。