# Power Manager 说明

## 1. 模块定位

`manager/power` 是 manager 层里的电源服务占位实现。

它当前不直接控制 PMIC、ADC 或 GPIO，而是先把电源服务自己的运行态从 `system` 中剥离出来，提供后续可演进的承载点。

## 2. 当前职责

- 提供 `powerInit()` 初始化入口。
- 提供 `powerProcess()` 周期处理入口，供 `PowerTask` 调度。
- 提供 `powerRequestLowPower()` 低功耗请求接口。
- 提供 `powerGetStatus()` 状态快照接口。

## 3. 当前边界

- 不直接依赖 BSP。
- 不直接依赖任何 `_port.h`。
- 不在 `system` 中保存电源服务运行态。

## 4. 后续演进方向

后续可以逐步补入：

- 电池电压采样聚合
- 充放电状态管理
- 休眠/唤醒策略
- 电源健康状态上报
- 与通信层的心跳数据联动
