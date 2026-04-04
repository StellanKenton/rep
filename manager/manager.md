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

## 5. 当前落地范围

这次只完成 P2 的最小可用骨架：

- 启动期自检由 `managerRunStartupSelfCheck()` 聚合。
- `PowerTask` 通过 `managerPowerProcess()` 调度电源服务。
- `UPDATE_MODE` 通过 `managerUpdateProcess()` 调度升级服务。

后续可以继续往这里补：

- service health registry
- 启动依赖声明
- demo/场景服务编排
- 统一 console 查询接口
