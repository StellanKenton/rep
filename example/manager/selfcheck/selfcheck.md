# SelfCheck Manager 说明

## 1. 模块定位

`manager/selfcheck` 负责聚合启动期服务检查结果，不直接执行底层硬件检测动作。

它的目标不是替代具体模块的自检逻辑，而是给 `system` 和后续 console/health 查询提供统一的启动结果快照。

## 2. 当前职责

- 保存 console、自定义通信、电源服务、升级服务四类启动结果。
- 输出统一的 `PASS/FAIL` 汇总状态。
- 提供 `selfCheckGetSummary()`、`selfCheckGetState()`、`selfCheckGetLastError()` 作为查询接口。

## 3. 生命周期约定

- 类型：recoverable service
- cfg ownership：无外部 cfg，结果快照由 `selfcheck` 自持
- ready 时机：`selfCheckInit()` 成功后进入 ready
- repeat init：允许，且不清空既有结果
- start 语义：`selfCheckStart()` 开启一轮新的启动检查并重置结果
- fault/recover：`selfCheckCommit()` 失败时进入 fault，`selfCheckRecover()` 可回到 ready

## 4. 当前设计约束

- 结果写入由 manager 层编排完成，不做动态注册。
- 结果结构保持稳定，后续扩展优先新增字段，不轻易改已有语义。
- 当前只表达服务级自检，不表达底层硬件细节。

## 5. 后续可扩展项

- 增加更多 service item
- 接入 console 查询命令
- 映射到通信上报结构
- 引入错误码和失败原因
