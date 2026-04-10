---
doc_role: component-guide
layer: service
module: lifecycle
status: active
portability: reusable
public_headers:
  - lifecycle.h
core_files:
  - lifecycle.h
  - lifecycle.c
port_files: []
debug_files: []
depends_on:
  - ../../rule/projectrule.md
forbidden_depends_on:
  - board binding
  - manager/system project orchestration
  - any _port.h
required_hooks: []
optional_hooks: []
common_utils: []
copy_minimal_set:
  - service/lifecycle/
read_next: []
---

# Lifecycle Component

`rep/service/lifecycle/` 提供跨项目可复用的轻量生命周期内核。

## 1. 组件目标

本组件负责：

- 为 `passive module`、`active service`、`recoverable service` 提供统一的状态机基线。
- 统一维护 `Init/Start/Stop/Process/Fault/Recover` 的可观测状态和计数。
- 提供项目编排层可直接嵌入的最小运行态结构。

本组件不负责：

- 服务注册中心。
- 任务创建、线程调度、系统模式切换。
- 板级依赖、端口绑定或业务策略。

## 2. 公共契约

- `serviceLifecycleInit()`：幂等地进入 ready。
- `serviceLifecycleStart()`：仅在 ready 且非 fault 时进入 running。
- `serviceLifecycleStop()`：退出 running，进入 stopped。
- `serviceLifecycleNoteProcess()`：记录一次 process；主动服务必须已 started。
- `serviceLifecycleReportFault()`：进入 fault，并保留 fault error。
- `serviceLifecycleRecover()`：仅 recoverable service 可从 fault 回到 ready。

## 3. 接入方式

- 业务服务在自己的 status 结构中嵌入 `stServiceLifecycle lifecycle;`
- 在初始化静态值时设置 `classType`
- 由项目侧 manager 或 system 编排层决定何时调用公共生命周期 API

## 4. 复用边界

本组件是中性公共件，可以被多个项目直接复用。

调用方需要自行定义：

- 自己的业务状态枚举
- 故障触发条件
- 启停时序
- 恢复策略