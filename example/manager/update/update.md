# Update Manager 说明

## 1. 模块定位

`manager/update` 是升级服务的运行态容器。

当前它先解决的是“升级状态不要继续散落在 system 里”，而不是一次性把 OTA、Flash 写入、镜像校验全部塞进来。

## 2. 当前职责

- 提供 `updateInit()` 初始化入口。
- 提供 `updateStart()` / `updateStop()` 运行态切换入口。
- 提供 `updateProcess()` 周期处理入口。
- 提供 `updateRequestStart()` / `updateRequestCancel()` 请求接口。
- 提供 `updateFault()` / `updateRecover()` 恢复接口。
- 提供 `updateGetState()`、`updateGetLastError()`、`updateGetStatus()` 状态快照接口。

## 3. 生命周期约定

- 类型：recoverable service
- cfg ownership：无外部 cfg，升级运行态由 `update` 自持
- ready 时机：`updateInit()` 成功后进入 ready
- repeat init：允许，且保持幂等
- process 前提：必须先 `updateStart()`
- recover path：内部故障通过 `updateFault()` 进入 fault，再由 `updateRecover()` 回到 ready

## 4. 当前边界

- 不直接操作 Flash 驱动。
- 不直接依赖升级协议实现。
- 不在 `system` 中维护升级服务内部状态。

## 5. 后续演进方向

后续可逐步补入：

- 镜像接收与分片状态机
- 校验与回滚策略
- 升级错误码
- 与 system mode、health、console 的联动
