# 架构优化计划

## 1. 结论

从重度依赖程序架构的嵌入式工程视角看，这套架构已经具备继续深挖的价值，但还没有到“以后只想用这一套”的程度。

当前最强的部分是：

- `drvlayer` 和 `module` 已经基本建立了 `core / port` 分层意识。
- 公共头文件整体上开始收敛为稳定语义，不再像传统工程那样直接把板级细节暴露给业务层。
- `manager/` 已经被定义为服务编排层，方向是正确的。
- `console` 已经通过 `console_port.c` 做了项目级命令接线，这是很好的“装配层”雏形。

当前最明显的短板是：

- `system/systask_port.c` 仍然直接绑定具体模块、具体 demo 和 MCU 头文件。
- “模块可拆卸”主要在驱动/模块层成立，在系统编排层还没有完全闭环。
- 板级资源清单没有真正落成单一 manifest，默认绑定仍分散在多个 `*_port.c` 中。
- 生命周期、健康状态、错误传播、并发约束还没有形成全仓库统一 contract。

一句话判断：

这套架构已经脱离了“堆功能式工程”，但还没有进化成“长期复用、持续扩张仍不失控”的体系。

## 2. 当前是否满足“每个模块都可拆卸”

### 2.1 已经基本满足的部分

以下层次已经比较接近“可拆卸”：

- `drvlayer/*` 大多遵循“core 只表达公共语义，port 负责项目绑定”的结构。
- `module/*` 的公共头文件普遍只暴露模块语义和配置，不直接 include 本模块 `_port.h`。
- `console` 的项目接线已经集中在 `console_port.c`，而不是散落在各业务文件中。
- `manager` 子模块当前只依赖公共头文件，没有直接跨层引用 `_port.h`。

从这几项看，当前仓库已经不是“任何模块都焊死在板子上”的状态了。

### 2.2 还不满足的部分

“完全可拆卸”目前还不能下结论成立，主要原因有四个：

1. `system` 仍然握着具体模块和平台实现

- `system/systask_port.c` 直接 include `appcomm/appcomm.h`、`Rep/module/w25qxxx/w25qxxx.h`、`gd32f4xx_gpio.h`。
- `sensorTaskCallback()` 内部直接运行 W25Qxxx flash demo。
- 这意味着 system 不是纯编排层，而是同时承担了 demo 容器、模块接线层、平台任务层三种职责。

2. 模块 port 仍然跨模块依赖其他 port

- `module/w25qxxx/w25qxxx_port.c` 直接 include `drvspi_port.h`。
- `module/mpu6050/mpu6050_port.c` 直接 include `drvanlogiic_port.h` 和 `drviic_port.h`。
- `module/pca9535/pca9535_port.c`、`module/tm1651/tm1651_port.c` 也存在类似模式。

这说明 core 边界虽然已经比过去干净，但“port 只能依赖下层公共接口”的原则还没有完全落实。

3. 默认资源绑定没有收敛到单点清单

- 当前 workspace 里没有实际落地的 `board/board_config.c`。
- 模块默认配置仍主要散落在各自的 `*_port.c` 中。
- 任务默认参数也仍在 system 侧组织，而不是完全来自单一 board manifest。

这会让“换板子只改一个地方”的目标难以真正实现。

4. 模块生命周期虽然开始统一，但 contract 还不完整

- 目前已经大量采用 `GetDefCfg / GetCfg / SetCfg / Init / IsReady` 形态。
- 但 active service、recover path、热重配、错误恢复、停止语义还没有统一到每个模块。

所以准确判断应该是：

当前架构已经具备“模块可以被拆下来”的方向，但还没有达到“拆下来以后不会牵出 system、board、任务编排、默认绑定的一串连锁修改”的程度。

## 3. 从架构角度，当前还需要做什么优化

### 3.1 把 `system` 还原成纯编排层

这是当前第一优先级。

`system` 现在还混着四类东西：

- 模式推进
- RTOS 任务创建
- console/appcomm 初始化
- flash demo 和模块验证

正确目标应该是：

- `system.c` 只维护 mode 语义。
- `systask_port.c` 只维护系统任务创建和调度钩子。
- 具体服务初始化、自检聚合、demo 场景，全部下沉到 `manager/`。

如果这一步不做，后续模块越多，`systask_port.c` 一定会重新膨胀成总控垃圾场。

### 3.2 把跨模块 `port -> port` 依赖收口

当前最需要继续执行的边界整改不是 `core -> port`，而是：

- `module/*_port.c` 不再 include 下层 `drv*_port.h`
- module port 只依赖下层驱动公共头，例如 `drvspi.h`、`drviic.h`
- 下层驱动如果还缺稳定公共接口，就补公共接口，而不是继续开放 `drv*_port.h`

这个动作很关键，因为“可拆卸”不是只要求 core 干净，而是要求 port 的装配关系也能稳定迁移。

### 3.3 不引入统一 board manifest，改成模块内 assembly 收口

后续不再引入统一的 `board_config.*`。

默认装配信息继续放在各模块自己的 `*_port.c` 中，但要把职责收紧为“只负责本模块的 assembly 默认值和项目绑定”。

建议统一成下面的收口方式：

- 任务参数继续在 `system/manager` 侧维护，不抽成统一 board manifest
- 模块默认实例绑定放在各自 `*_port.c`
- transport / bus / link 选择通过各模块 `_port.h` 的 assembly API 修改
- 业务代码只调用公共稳定 API，不直接碰装配字段

这样做的目标不是“一个文件管全板子”，而是“每个模块的装配信息都有唯一入口，且不会泄漏到业务层”。

### 3.4 把公共 API 和装配 API 分开

现在模块 API 形态已经有统一趋势，但还差最后一步区分：

- Stable API：给 system、manager、业务代码调用
- Assembly API：只给 port、board、manager 初始化阶段调用

稳定 API 只暴露：

- 逻辑设备号
- 稳定配置
- 状态查询
- 数据操作

装配 API 负责：

- 默认装配配置加载
- 平台接口绑定
- 项目级合法性校验

这一步完成后，业务代码才不会继续被 transport 绑定语义牵着走。

### 3.5 统一生命周期模型

建议把模块明确分成三类：

- passive module：`GetDefCfg/GetCfg/SetCfg + Init + API`
- active service：`Init + Start + Process/Task + Stop`
- recoverable service：在 active 基础上补 `Fault/GetLastError/Recover`

每个模块至少要统一回答：

- cfg ownership 属于谁
- 什么时候 ready
- 是否允许重复 init
- 是否支持热重配
- 失败后怎么恢复

如果这一层不统一，模块一多，结构会重新碎掉。

### 3.6 给架构补可观测性和错误传播

现在有 log 和 console，但对长期维护来说还不够。

这一项现在已经落地最小可用版本，当前统一 contract 是：

- `power/update/selfcheck` 统一提供 `GetState()`
- `power/update/selfcheck` 统一提供 `GetLastError()`
- `power/update/selfcheck` 统一通过 `GetStatus()` 输出最小统计快照
- `managerGetHealthSummary()` 聚合 manager 级 health summary
- `system_debug` 通过 `health` 命令暴露自检结果和服务健康查询接口

这样架构就从“靠日志猜问题”进入了“能主动查询 system/manager 健康状态”的最小闭环。后续如果继续扩张，再把同一 contract 下推到更多 module 即可。

### 3.7 用自动化守护边界

如果希望以后一直用这套架构，就不能只靠人记规则。

至少需要自动化检查：

- 非 `port/debug` 文件不得 include `_port.h`
- `*_port.c` 不得跨模块 include 其他模块 `_port.h`
- 公共头文件不得暴露 `Port` 绑定概念
- system 不得直接 include module/driver 的 port 头
- 命名、路径、大小写一致性检查

架构一旦没有自动化护栏，就一定会退化。

## 4. 怎样才能让我只想用这套架构

如果要达到“以后都不想换别的架构”的程度，我看重的不是层数有多漂亮，而是下面六件事是否同时成立。

### 4.1 改板子时，改动面必须稳定

理想状态：

- 改板卡资源映射时，只改 `board_config.*`、`*_port.*`、BSP。
- 不改 `system`、`manager`、`module core`、`comm core`。

这是衡量“可移植”的第一标准。

### 4.2 新增模块时，不需要重新发明结构

理想状态：

- 新增驱动有驱动模板。
- 新增 module 有 module 模板。
- 新增 service 有 manager/service 模板。
- 生命周期、错误语义、状态查询、配置 ownership 都是默认统一的。

一旦新增模块还要重新思考“这个模块到底放哪、Init 怎么做、状态怎么查”，说明架构还不够成熟。

### 4.3 system 永远不会再次膨胀

真正让我愿意长期使用的架构，不是今天干净，而是三个月后加十个功能仍然干净。

判断标准：

- 新功能优先进入 `manager` 或具体 service
- `system` 只负责模式和调度
- demo、自检、联调场景不再直接进入 `systask_port.c`

### 4.4 出问题时，不靠猜

理想状态：

- 每个服务都能查状态
- 每个模块都能查最后错误
- 自检结果能查询
- 关键统计信息能查询
- manager 能给出系统健康总览

能快速定位问题，比“目录分得漂亮”更能决定我会不会一直用它。

### 4.5 架构边界由脚本保证

真正成熟的架构，review 不是用来抓低级边界违规的。

边界违规应该在脚本阶段直接报错，例如：

- 非法 include `_port.h`
- 非法暴露平台绑定字段
- 非法跨层依赖

这样团队扩大后，架构才不会回退。

### 4.6 允许演进，但不允许失控

我会长期偏爱一套架构，前提是它既稳定又不僵硬。

这意味着：

- 可以增加新服务
- 可以切换新板卡
- 可以补新的 transport
- 可以引入新的观测接口

但这些变化都不会破坏既有层次，不会要求把旧模块推翻重来。

## 5. 分阶段优化路线

### P0. 收口 system 直接耦合

目标：

- 把 `systask_port.c` 里的 `appcomm` 初始化、自检细节、flash demo 下沉到 `manager/`
- `system` 只保留 mode 推进和任务调度

验收标准：

- `system/systask_port.c` 不再直接 include `w25qxxx.h`
- `system/systask_port.c` 不再承载具体模块 demo
- `system` 与具体 demo/业务模块解耦

### P1. 收口跨模块 port 依赖

目标：

- `module/*_port.c` 不再 include 下层 `drv*_port.h`
- port 层统一只依赖下层稳定公共接口

验收标准：

- 仓库扫描中，跨模块 `_port.h` 引用为 0

### P2. 落地模块内 assembly API

目标：

- 每个模块把稳定配置和装配配置拆开
- transport / bus / link 选择统一通过各自 `_port.h` 的 assembly API 修改
- 默认绑定仍保留在各自 `*_port.c`

验收标准：

- 公共头文件不再暴露 transport / bus / link 绑定字段
- 业务层只使用 stable API；装配期代码只使用 assembly API
- 默认装配信息不再混入公共 cfg

### P3. 统一模块生命周期 contract

目标：

- 给 passive module / active service / recoverable service 建立标准 API 形态
- 统一 `GetDefCfg/GetCfg/SetCfg/Init` 语义

验收标准：

- 每个模块文档都能回答 ownership、ready、repeat init、recover path

### P4. 建立 health 和 error contract

目标：

- 模块补 `GetState/GetLastError`
- manager 聚合 health summary
- 自检结果支持查询

验收标准：

- 现场问题可通过 console/status 接口主动定位，而不是只看 log

### P5. 补自动化护栏

目标：

- 增加架构扫描脚本和最小 host 侧测试
- 把规则从文档变成检查项

验收标准：

- 新增模块或修改边界时，脚本可以自动发现违规

## 6. 最终目标状态

当下面这些条件同时成立时，这套架构就足够强，可以成为长期默认选项：

1. 改板级连接时，只改相关模块的 `*_port.*`、BSP 和必要的 manager/system 装配点。
2. 新增模块时，只需套模板，不需重新设计生命周期。
3. `system` 永远不再承载 demo 和硬件细节。
4. `manager` 成为唯一的服务装配与自检聚合入口。
5. 每个模块都能查询状态、错误和最小统计信息。
6. 架构边界能被脚本自动验证，而不是靠口头约束。

## 7. 一句话建议

这套架构不需要推倒重来，真正该做的是继续完成“边界闭环”。

只要把 `system` 纯化、把 board manifest 落地、把生命周期和 health contract 统一、再用自动化守住边界，这套架构就会从“设计得不错”进化成“值得长期只用这一套”。