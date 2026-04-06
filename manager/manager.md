---
doc_role: layer-guide
layer: manager
module: manager
status: active
portability: project-bound
public_headers: []
core_files:
  - manager.md
  - manager.c
  - service_lifecycle.c
port_files: []
debug_files: []
depends_on:
  - ../rule/projectrule.md
  - ../system/system.md
forbidden_depends_on:
  - 子模块依赖下层 _port.h
required_hooks: []
optional_hooks: []
common_utils: []
copy_minimal_set:
  - manager/
read_next:
  - power/power.md
  - selfcheck/selfcheck.md
  - update/update.md
---

# Manager 层说明

这是 `manager/` 的权威入口文档。

## 1. 本层目标和边界

`manager` 是服务编排层，负责把 `power`、`selfcheck`、`update` 这类 service 组织成稳定入口，供 `system` 调度。

本层负责：

- 服务生命周期统一化。
- 启动期自检结果聚合。
- 健康摘要与 last-error 可观测性。

本层不负责：

- 底层驱动适配。
- 直接管理系统模式状态。
- 替代 `system` 承担任务创建。

## 2. 允许依赖与禁止依赖

- 允许依赖：公共头文件、`service_lifecycle.*`、更底层稳定接口。
- 禁止依赖：任何 `_port.h`、板级绑定细节和 BSP 结构。
- `system` 只能通过 manager 暴露的公共入口调度服务，不能跨层操纵子模块内部状态。

## 3. 下级目录主文档必须包含的内容

每个 service 文档必须明确：

- 自己属于 `active service` 还是 `recoverable service`。
- `Init/Start/Process/Stop/Recover/GetStatus/GetLastError` 的语义。
- 允许在哪个任务上下文调用。
- 哪些接线点是 `project-bound`。

## 4. 本层通用命名模式

- `manager.h/.c`：聚合入口与健康摘要。
- `service_lifecycle.h/.c`：统一生命周期状态机基线。
- 子目录 `power/`、`selfcheck/`、`update/`：各自持有运行态、last error 和统计信息。

## 5. 生命周期模板

当前统一基线如下：

- `Init`：允许重复调用，语义为幂等 ready。
- `Start`：只在 ready 且非 fault 状态下成功。
- `Process`：只允许在 started 状态下执行。
- `Stop`：退出运行态，不销毁实例。
- `Recover`：只用于 recoverable service，从 fault 回到 ready。

已确认落地：

- `power`：active service。
- `update`：recoverable service。
- `selfcheck`：recoverable one-shot service。

## 6. 常见错误写法和反例

- 在 `system` 中直接维护 service 内部状态，而不是走 manager 入口。
- 让 manager 子模块直接 include 下层 `_port.h`。
- 把 `GetStatus` 做成临时调试接口，而不是稳定统计快照。

## 7. 复制到其他工程时如何处理

`manager/` 默认属于 `project-bound`：

- `service_lifecycle.*` 和生命周期模式可参考或复用。
- 具体服务接线、启动时序和模式切换逻辑通常要重写。

## 8. AI 推荐阅读顺序

1. 先读本文件。
2. 再读 `service_lifecycle.*`。
3. 再读目标 service 文档。
4. 最后回到 `system/system.md` 查看调度关系。
