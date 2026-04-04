# Manager 层说明

## 1. 模块定位

`manager/` 是服务编排层，不承担底层驱动适配，也不直接承载系统模式状态。

它负责三类事情：

- 组织跨模块服务的初始化顺序。
- 聚合启动期自检结果。
- 为 `system` 提供明确的服务入口，避免把业务接线继续堆回 `systask_port.c`。

## 2. 当前目录结构

```text
manager/
    manager.h
    manager.c
    manager.md
    power/
        power.h
        power.c
        power.md
    selfcheck/
        selfcheck.h
        selfcheck.c
        selfcheck.md
    update/
        update.h
        update.c
        update.md
```

## 3. 当前职责划分

- `manager.h/.c`
  提供统一入口，负责把 `power`、`selfcheck`、`update` 串起来。
- `power/`
  承接电源服务运行态，供 `PowerTask` 调度。
- `selfcheck/`
  承接启动期自检结果聚合与状态快照。
- `update/`
  承接升级服务运行态与请求状态。

## 4. 现阶段设计原则

- 不引入 IOC、依赖图或动态注册框架。
- 优先采用显式初始化函数和固定调用序列。
- `system` 只决定何时进入自检、待机、升级模式，不直接维护具体服务内部状态。
- manager 子模块只依赖公共头文件，不跨层引用 `_port.h`。

## 5. 生命周期 contract

`manager/` 现在统一采用三类生命周期模型：

- passive module: `Init + Query/API`
- active service: `Init + Start + Process/Task + Stop`
- recoverable service: `Init + Start + Process/Task + Stop + Fault/GetLastError/Recover`

公共基线放在 `manager/service_lifecycle.*`，当前首批接入模块如下：

- `power/` 作为 active service 落地 `Init/Start/Process/Stop`
- `update/` 作为 recoverable service 落地 `Init/Start/Process/Stop/Fault/Recover`
- `selfcheck/` 作为 recoverable one-shot service 落地 `Start/Commit/GetLastError/Recover`

统一约束：

- `Init` 允许重复调用，语义为幂等 ready。
- `Start` 仅在 ready 且非 fault 状态下成功。
- `Process` 只允许在 started 状态下执行。
- `Stop` 负责退出运行态，不销毁模块实例。
- `Recover` 只用于 recoverable service，从 fault 返回 ready。

## 6. 当前落地范围

这次只完成 P2 的最小可用骨架：

- 启动期自检由 `managerRunStartupSelfCheck()` 聚合。
- `PowerTask` 通过 `managerPowerProcess()` 调度电源服务。
- `UPDATE_MODE` 通过 `managerUpdateProcess()` 调度升级服务。
- manager 子模块统一携带 lifecycle state、last error 和计数快照。

## 7. 可观测性 contract

当前仓库已经补齐 manager 层最小可用的 health/error contract：

- `power`、`update`、`selfcheck` 统一保留 `GetState()`、`GetLastError()` 和 `GetStatus()` 查询入口。
- `GetStatus()` 作为最小统计快照，稳定暴露 lifecycle 计数、ready/started/fault 标志和服务私有状态。
- `managerGetHealthSummary()` 负责聚合 `power/update/selfcheck` 的 lifecycle、错误、自检结果和总体健康等级。
- `system_debug` 通过 `health` console 命令暴露 manager 级状态查询，不再只依赖 log 侧推断问题。

后续可以继续往这里补：

- service health registry
- 启动依赖声明
- demo/场景服务编排
- 更细粒度的模块级 health registry
