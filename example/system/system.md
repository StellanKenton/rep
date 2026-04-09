---
doc_role: layer-guide
layer: system
module: system
status: active
portability: project-bound
public_headers: []
core_files:
  - system.md
  - system.c
  - system_debug.c
port_files:
  - systask_port.c
debug_files:
  - system_debug.c
depends_on:
  - ../manager/manager.md
  - ../console/console.md
forbidden_depends_on:
  - 直接在 system.c 中承载业务服务逻辑
required_hooks: []
optional_hooks: []
common_utils: []
copy_minimal_set:
  - system/
read_next:
  - ../manager/manager.md
  - ../console/console.md
---

# System 模块说明

这是 `example/system/` 的权威入口文档。

它描述的是“当前工程或后续新项目可参考的 system 层组织方式”，不是 `rep/` 顶层独立公共层入口。

## 1. 本层目标和边界

`example/system` 默认视为 `project-bound`。它负责系统模式、任务编排、启动接线和 system 调试命令，不适合作为仓库顶层可直接迁移的通用模块。

本层负责：

- system mode 与版本接口。
- 任务创建、周期和启动时序。
- console / debug 启动接线。
- 与 `manager` 的模式级协作。

本层不负责：

- 直接实现业务服务。
- 直接承担底层驱动适配。

## 2. 允许依赖与禁止依赖

- 允许依赖：`manager` 公共入口、`console` 公共入口、各模块暴露的注册函数。
- 禁止依赖：下层 `_port.h`、BSP 细节、把业务流程长期堆进 `SystemTask`。

## 3. 文件职责

| 文件 | 职责 |
| --- | --- |
| `system.h/.c` | system mode、版本号、模式字符串等稳定语义 |
| `systask_port.h/.c` | 任务参数、任务入口、启动接线和任务创建 |
| `system_debug.h/.c` | `ver`、`sys`、`top`、`health` 等调试命令 |

## 4. 接口边界

- `system.c` 只维护 mode 和版本语义。
- `systask_port.c` 只做编排和接线，不写底层驱动实现。
- console 命令注册由各自模块提供，再由 system 统一接线。
- service 生命周期由 `manager` 持有，system 只决定何时调度。

## 5. 常见错误写法和反例

- 在 `system.c` 中塞入任务创建或业务状态机。
- 让 `SystemTask` 直接替代 `PowerTask`、`UpdateTask` 等真实任务。
- 在 system 中直接 include 下层 debug 以外的私有头。

## 6. 复制到其他工程时如何处理

`example/system/` 的模式状态机、任务布局和 console 接线大多依赖当前工程启动流程。复制到外部项目时：

- `system.h/.c` 的 mode 语义可参考。
- `systask_port.*` 和具体任务接线通常必须重写。
- `system_debug.*` 可部分复用，但命令注册入口要按外部项目重接。

## 7. AI 推荐阅读顺序

1. 先读本文件。
2. 再读 `system.h/.c`。
3. 再读 `systask_port.h/.c`。
4. 再读 `system_debug.h/.c`。
5. 若涉及服务调度，再回读 `../manager/manager.md`。