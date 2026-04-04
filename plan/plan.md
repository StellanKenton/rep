# 架构进阶计划

## 1. 结论

当前这套架构的方向是对的：已经有了 `core / port / BSP` 的基本思想，也开始强调逻辑资源、模块文档和平台隔离。

但如果目标不是“能用”，而是“让我以后都不想换别的架构”，那现在还差最后一段关键路程：

- 把分层边界从“设计目标”变成“仓库事实”。
- 把模块 API 从“可配置”提升为“稳定可装配”。
- 把 system 从“直接接线层”升级成“纯编排层”。
- 把并发、状态、错误、健康度做成统一 contract。
- 把规则交给脚本和测试守住，而不是靠人记。

换句话说，前期完成的 core-port 边界整改解决了"先把边界打正"的问题；本计划要解决的是"让这套架构具备长期复用、长期演进、长期可维护能力"。

## 2. 当前架构已经具备的优点

- `drvlayer / module / comm / system` 的职责划分方向是正确的。
- 文档密度已经明显高于很多嵌入式仓库，后续沉淀规则的成本不高。
- `ringbuffer` 已经开始按 `core / concurrency policy / integration` 三层思考，这是非常好的基础设施意识。
- `rep_config.h` 已经有平台、RTOS、日志级别这类全局配置入口，说明仓库具备继续做 profile 分层的基础。
- `manager/` 目录已经预留出来了，说明这套架构天然有空间补“服务装配层”。

这些优点说明：这套架构值得继续投入，不需要推翻重来。

## 3. 现在还不够强的关键点

### 3.1 Core-Port 边界还没有彻底收口

core 文件直接 include 自己的 `_port.h` 的问题已经基本消除（通过 weak 函数 + platform hook 模式），但跨模块 port 交叉引用仍然存在：

- `mpu6050_port.c` 直接 include `drvanlogiic_port.h` 和 `drviic_port.h`
- `pca9535_port.c` 直接 include `drvanlogiic_port.h`
- `tm1651_port.c` 直接 include `drvanlogiic_port.h`
- `w25qxxx_port.c` 直接 include `drvspi_port.h`
- `gd25qxxx_port.c` 直接 include `drvspi_port.h`

这说明尽管 core → port 的直接依赖已经收口，但 port → 其他模块 port 的跨层引用仍未解决，这意味着模块之间的板级耦合还在通过 port 层传递。

这类跨模块 port 引用是当前最高优先级的边界违规。如果不处理，后面再谈 manager、profile、测试，都很容易被旧依赖拉回去。

### 3.2 板级绑定语义通过 port 头泄漏到 system 层

当前模块的公共头文件（`xxx.h`）本身已经不再直接暴露绑定函数，但 port 头文件被当作事实上的公共 API 使用，导致板级绑定语义仍然泄漏到 system 层：

- `mpu6050_port.h` 暴露 `mpu6050PortSetSoftIic()` / `mpu6050PortSetHardIic()`
- `w25qxxx_port.h` 暴露 `w25qxxxPortSetHardSpi()`
- `pca9535_port.h` 暴露 `pca9535PortSetSoftIic()`
- `tm1651_port.h` 暴露 `tm1651PortSetSoftIic()`
- `gd25qxxx_port.h` 暴露 `gd25qxxxPortSetHardSpi()`

另外，配置结构里仍直接出现 `spi`、`iic`、`iicType` 这类底层连接语义。

这会带来三个问题：

- 上层必须知道底层接线细节。
- core API 的稳定性被板级资源绑定牵制。
- 模块移植时，业务层仍然很容易被迫修改。

真正成熟的架构里，板级绑定应该只存在于 assembly/port 层，不应该通过 port 头的“事实公开”泄漏到 system 层。

### 3.3 缺少真正的 Service / Manager 层

当前 `system/systask_port.c` 同时承担了：

- 任务创建
- console 初始化接线
- appcomm 初始化
- flash demo / 自检逻辑

这会让 `system` 同时扮演“启动器、接线层、demo 容器、系统服务容器”四个角色。

如果后续业务继续增长，`system` 很容易重新变成一个越来越大的总控文件。

空着的 `manager/` 目录正好可以用来承接这件事。当前最缺的不是再多一个模块，而是把跨模块编排从 `system` 中抽出去。

### 3.4 生命周期和配置所有权模式需要演进

当前所有模块的 `GetDefCfg / GetCfg / SetCfg / Init / IsReady` 实际上已经保持了一致的形态——都采用 weak 函数 + 内部状态修改的模式，这是好的。但这个模式本身存在问题：`GetDefCfg()` 直接修改模块内部实例，而不是把默认配置返回给调用者，这导致 cfg 所有权不明确。

当前缺少统一回答的几个问题：

- cfg 是由调用者持有，还是由模块内部长期持有。
- `Init()` 前后哪些字段允许修改。
- 是否支持 `ReInit()` 或热重配。
- 哪些模块需要 `Process()`，哪些只提供同步接口。
- 是否需要统一的 `Stop / Deinit / Recover` 语义。

如果这一层不统一，模块数量一多，架构的一致性就会被接口细节慢慢侵蚀掉。

### 3.5 并发与 ISR 边界还没有形成全仓库 contract

`ringbuffer` 文档已经开始强调 concurrency policy，但整个仓库还没有把这一套纪律推广出去。

现在仍缺少统一约束：

- 哪些 API 可以在 ISR 调用。
- 哪些 API 只能在任务上下文调用。
- 哪些模块内部自带保护，哪些要求外部串行化。
- 缓冲区、tick、重试状态、超时语义由谁持有。

这类问题在 `log + console + uart + frameparser + frameprocess` 叠加时最容易积累成隐性 bug。

### 3.6 observability 还停留在 log 层，不足以支撑长期维护

当前已经有 `log` 和 `console`，但还缺更高一级的运行期可观测性：

- 模块状态快照
- 模块统计信息
- 统一的最近错误记录
- 自检项注册表
- service health 视图
- 降级 / 恢复 / 重试计数

没有这些能力，架构规模越大，定位问题就越依赖人工经验和现场日志时序。

### 3.7 自动化护栏还没有形成体系

当前文档已经明确说明仓库还缺成熟自动化测试套件。

如果没有下面这些自动化护栏，再好的架构也会逐渐退化：

- 架构违规扫描
- host 侧纯逻辑测试
- fake port contract test
- 命名和 include path 一致性检查
- 文档模板和新模块模板

架构必须靠工具守住，而不是靠 review 时临时想起来。

### 3.8 缺少统一的错误传播策略

当前 `eDrvStatus` 提供了驱动层的错误码，但缺少模块层到服务层的错误映射和传播策略：

- 驱动层返回的错误码与模块层的错误码是否是同一套。
- 模块内部错误是就地处理还是向上传播。
- 是否支持错误链（底层错误码 + 模块上下文）。
- 多模块组合场景下的错误汇聚策略。

如果各层错误处理各自为政，随着模块数量增长，调试和故障定位的难度会指数级上升。

### 3.9 命名和路径债务已经开始影响可迁移性

当前仓库还能看到一些历史痕迹：

- `framepareser` / `frameparser` 命名混用
- `drvanlogiic` 这类历史命名继续扩散
- include path 大小写不一致

这类问题在 Windows 下不一定马上爆炸，但会持续影响：

- 工具链迁移
- Linux CI
- 代码生成脚本
- 文档索引
- 新人理解成本

这是架构可信度问题，不只是命名洁癖。

## 4. 目标状态

如果要把这套架构做到“长期唯一选项”，我建议把目标抬到下面这个层级。

### 4.1 五层稳定模型

1. `tools / core infra`
   只放 ringbuffer、filter、基础数据结构、纯算法，不带业务语义。
2. `drvlayer`
   只暴露逻辑资源驱动，不暴露 BSP 细节。
3. `module / comm core`
   只表达设备语义、协议语义和运行态，不表达板级接线。
4. `manager`
   负责跨模块编排、服务装配、自检注册、健康管理、场景流程。
5. `system`
   只负责模式切换、任务调度、启动阶段装载，不直接写业务接线细节。

### 4.2 两类 API 分离

- Stable Service API
  面向上层业务，只暴露逻辑设备、业务参数、状态、数据、事件。
- Assembly API
  只给 port / manager 使用，用于默认配置装配、资源绑定、平台钩子注入。

核心原则：

- 业务代码只调用 Stable Service API。
- 板级接线只通过 Assembly API 完成。

### 4.3 三类配置分离

- compile-time profile
  平台、RTOS、功能开关、容量上限。
- board binding
  bus、device、gpio、tick、queue storage、transport 绑定。
- runtime config
  超时、阈值、重试、业务参数、策略参数。

这三类配置不能再揉进一个 cfg 结构中。

建议演进方向：

- `rep_config.h` 保留最上层通用编译期开关。
- 新增 board / service / app profile 概念，把板卡差异和产品差异拆开。
- port 层只负责把 profile 和 manifest 组装成 core 可消费的 cfg / hook。

## 5. 建议优化路线

### P0. 完成现有 Core-Port 整改闭环

这是所有后续工作的前置条件。

core → port 的直接依赖已基本消除，当前最高优先级的是消灭跨模块 port 交叉引用。

必须达到：

- 任何 `*_port.c` 不得 include 其他模块的 `*_port.h`（如 `mpu6050_port.c` 不得 include `drvanlogiic_port.h`）。
- 模块 port 应该只通过对应驱动的公共头（如 `drviic.h`、`drvspi.h`）访问驱动能力。
- core `.c` 中不再直接调用 `XxxPort*`。
- `system` 和业务层不再跨层包含下层 `_port.h`。
- 公共头文件不再暴露 port / bsp 命名和绑定字段。

### P1. 统一生命周期模型并重做公共 API 的配置语义

> 注：原 P1（配置语义）和原 P3（生命周期模型）存在强耦合关系——在没有确定生命周期模型的前提下重做 GetDefCfg，很可能需要返工。因此合并为一个里程碑执行。

#### P1a. 建立统一生命周期模型

建议至少把模块统一分成三类：

- passive module：只需要 `Init + API` 调用
- active service：需要 `Init + Start + Process/Task + Stop`
- recoverable service：在 active 基础上再补 `Fault / Recover`

每个模块文档都应明确：

- cfg ownership
- ready 条件
- 是否允许重复 init
- 是否支持热重配
- 是否需要周期 process
- 失败后恢复路径

这样新增模块时不需要重新发明生命周期语义。

#### P1b. 重做公共 API 的配置语义

目标：

- 统一 `GetDefCfg(device, &cfg)` 形态，由调用者拿到默认 cfg。
- `SetCfg(device, &cfg)` 只接收稳定语义字段。
- `SetHardSpi / SetSoftIic / SetHardIic` 退出 module 公共 API，只保留在 port / assembly helper。
- cfg 中优先使用逻辑链路号、逻辑 profile 或逻辑 device，而不是直接暴露物理 transport 细节。

收益：

- 上层业务真正不关心板级接法。
- 模块的 public API 可以长期稳定。
- 多板卡、多产品线迁移成本显著降低。

### P2. 把 `manager/` 做成服务编排层

建议把 `manager` 定义成“跨模块服务装配层”，而不是新的杂项目录。

第一步只需要承载最核心的能力：

- 服务初始化顺序编排（把 `systask_port.c` 里的跨模块初始化抽出来）
- 领域级 service分组，例如 storage service、sensor service、comm service
- demo / 自检逻辑的统一容器

待稳定后再逐步补充：

- 启动依赖声明
- 自检项注册
- health registry

完成后：

- `system` 只调度 manager
- manager 再去组装 module / comm / log / console
- demo、板级自检、场景级流程不再直接堆进 `systask_port.c`

> 注意不要过早引入重型 IOC 或复杂的依赖图。对当前规模的嵌入式项目，一个显式的初始化函数序列就足够了。

### P3. 建立资源清单和平台配置

建议引入一套轻量的配置收口机制，核心是一个 per-board 的 `board_config.c / board_config.h`，把当前散落在多个 `_port.c` 和 `systask_port.c` 里的默认资源收口到一处。

重点收口内容：

- UART / SPI / IIC / GPIO 逻辑资源绑定
- ringbuffer 存储区
- tick provider
- console / log transport 选择
- task 栈大小、优先级、周期
- 默认协议格式和默认实例配置

目标不是抽象得更花，而是把默认资源收口到一套可查找、可覆写、可验证的清单里。

> 不建议一开始就拆成 board manifest / service manifest / resource manifest / storage manifest 四类。对当前规模，先用一个文件收口，等多板卡多产品线的需求真正出现时再拆分。

### P4. 全仓库补齐并发 contract

每个活跃模块至少补四类说明：

- callable context：task / ISR / both
- ownership：buffer / ctx / queue 谁持有
- lock policy：内部保护还是外部保护
- timing：是否阻塞、最大等待、超时语义

建议优先落地模块：

- `drvuart`
- `log`
- `console`
- `frameparser`
- `frameprocess`
- `ringbuffer`

这是把“能跑”升级成“长期稳定可控”的关键一步。

### P5. 把 observability 升级为 health architecture 并统一错误传播

分两步落地，避免一次性过度设计：

**第一步（必做）：**

- 每个模块补齐 `<module>GetLastError()` 和 `<module>GetState()`
- 统一错误传播策略：明确驱动层错误码与模块层错误码的映射关系，确定模块内部错误是就地处理还是向上传播
- console 统一 status / error 查询命令

**第二步（按需）：**

- `<module>GetStats()` 统计信息
- self-check item registry
- service health registry

完成后，现场定位问题时就不再只依赖串口日志，而是可以主动查询系统状态快照。

### P6. 建立自动化守护

至少补齐五类检查：

- 非 `port/debug` 文件是否 include `_port.h`
- `*_port.c` 是否跨模块 include 其他模块的 `*_port.h`
- 公共头文件是否暴露 port / bsp 命名
- module 公共 API 是否暴露 `SetHard* / SetSoft*` 这类绑定动作
- include path、命名、大小写是否一致

再补两类测试：

- host 侧纯逻辑测试：`ringbuffer / frameparser / frameprocess_data / console parser`
- fake port contract test：module 在假接口下验证 `Init / Read / Write / Timeout / Recover`

目标很明确：架构质量不靠人记忆维持。

### P7. 做一次命名和目录债务清算

建议单独拉一个里程碑处理：

- 统一 `framepareser` 命名
- 收敛 `drvanlogiic` 历史命名
- 统一 include path 大小写
- 同步更新地图文档和模块说明文档

这一步不直接增加功能，但会明显提高：

- 可迁移性
- 自动化可行性
- 仓库可信度
- 新成员接手速度

### P8. 把模板和脚本纳入架构本体

想长期只用这套架构，就不能让新增模块靠“记忆”完成。

建议补齐：

- drv 模块模板
- module 模块模板
- comm / service 模块模板
- architecture check script
- 新模块 checklist

最理想的状态是：新模块默认生成正确骨架，而不是靠 review 后补救。

## 6. 推荐执行顺序

### M1. Core-Port 收口 + 命名债务清算（P0 + P7）

目标：

- 消灭跨模块 port 交叉引用。
- 统一 `framepareser` → `frameparser`、`drvanlogiic` → `drvalogiic` 命名。
- 统一 include path 大小写。

> 命名债务放在第一步做，是因为后续每个里程碑都会涉及文件重命名和头路径变更，越晚做影响面越大。和 P0 一起做可以减少一次全量头文件扫描。

### M2. 统一生命周期 + 公共 API 形态（P1）

目标：

- 确定三类模块（passive / active / recoverable）的生命周期模型。
- 把 Stable API 和 Assembly API 分开。
- 统一 GetDefCfg 为调用者持有模式。

### M3. 启用 `manager` 层（P2）

目标：

- 把 `system` 从“直接接线 + demo 容器”还原成纯编排层。
- 先做最小可用的服务编排，不急于上 manifest。

### M4. 建立平台配置收口（P3）

目标：

- 把默认资源、启动装配、任务参数统一收口到 per-board 配置。

### M5. 并发 contract + 错误传播 + health 基础（P4 + P5）

目标：

- 每个活跃模块补齐并发 contract 文档。
- 统一错误传播策略。
- 补齐 GetLastError / GetState 基础 health 能力。

### M6. 自动化守护 + host test + 模板固化（P6 + P8）

目标：

- 架构质量可以稳定保持。
- 后续新增模块自然落在正确轨道上。

## 7. 最终验收标准

当下面这些条件同时成立时，这套架构就很接近“长期唯一选项”了：

1. 改板级连接时，只需要改 board_config、`*_port.*` 和 BSP，不需要改 core / module / manager。
2. 新增一个模块时，可以直接套模板，接口形态和生命周期不需要重新发明。
3. `system` 不再承载 demo 和硬件细节，只负责模式和调度。
4. 任意模块的并发边界、错误语义、默认配置所有权都能在文档和代码里一致找到。
5. 错误传播路径清晰：驱动层错误码 → 模块层错误码 → 服务层聚合，可追溯。
6. 现场问题可以通过 `GetState / GetLastError` 快速收敛，而不是只能靠日志猜。
7. 架构违规可以在脚本和测试阶段自动发现。
8. 仓库命名、路径、文档、模板保持一致，迁移工具链不会被大小写和历史命名拖垮。

## 8. 一句话判断

现在这套架构已经有“值得继续投资”的骨架了，但还没有进化到“足以长期复用且不想换”的程度。

真正还需要补的，不是再多拆几个 `*_port.c`，而是把"边界、装配、生命周期、并发、错误传播、可观测、自动化"这七件事做成完整体系。