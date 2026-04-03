# Core-Port 分层整改计划

## 1. 目标

本轮整改的最终目标只有一个：

- core 只能依赖 core 公共头文件和更底层模块的 core 公共接口。
- port 可以依赖本模块 core，也可以依赖其他模块的 port。
- 迁移项目时，原则上只需要修改各模块的 port 文件和 BSP 绑定，不需要改 core。

换句话说，依赖方向必须固定为：

- 上层业务 -> 模块 core / 驱动 core
- port -> core
- 禁止 core -> port

## 2. 验收标准

计划执行完成后，需要同时满足下面几条：

1. 所有非 port、非 debug 的 .c/.h 文件中，不再直接 include 本模块或其他模块的 _port.h。
2. 所有 core 公共头文件中，不再暴露 PortBinding、PortInterface、PortType 这类 port 概念。
3. 所有 core 实现文件中，不再直接调用 XxxPort* 函数，也不再通过 extern 方式偷偷依赖 port 符号。
4. port 成为唯一的项目绑定层，默认配置、总线映射、延时、缓冲区、平台差异全部收敛到 port。
5. 至少形成一套可重复套用的改造模板，后续新模块按同一规则新增。

## 3. 优先级总览

### P0. 先把规则和边界定死

这是第一优先级，不先定规则，后续改造会反复回退。

需要做的事情：

1. 在 rule 和模块说明文档里补一条强约束：core 不允许 include _port.h，不允许暴露 Port 类型，不允许调用 XxxPort*。
2. 明确允许的例外只有 port/debug 文件；system 和业务层如果需要项目绑定信息，也必须通过 core 公共接口拿，不能直接下钻到模块 port。
3. 定义统一重构模式，后面所有模块都按同一模板改，不要每个模块各搞一套。

建议产出：

- 一份统一的分层规则补充说明。
- 一份“模块改造模板”。

完成标志：

- 团队对什么算违规、什么不算违规有统一判断。

### P1. 先清掉跨模块反向依赖

这是最优先要动代码的部分，因为它直接破坏模块边界。

当前已确认的问题：

1. comm/frameprocess/frameprocess.h 直接引用了 comm/frameparser/framepareser_port.h。
2. system/systask_port.c 直接引用了 module/w25qxxx/w25qxxx_port.h。

需要做的事情：

1. 先改 frameprocess 与 frameparser 的边界。
2. 再改 system 对 w25qxxx 的直接 port 依赖。
3. 改完后重新扫全仓，确认没有新的跨模块 core -> port 引用。

建议改法：

1. frameprocess 只依赖 frameparser 的 core 公共类型。
2. 如果 frameprocess 目前用到的是 frameparser_port 中的配置结构，就把真正应该稳定暴露的那部分下沉到 frameparser.h，或者在 frameprocess 自己的 core 头里定义一个与 port 解耦的配置结构，再由 frameprocess_port 去做转换。
3. systask 不能直接 include w25qxxx_port.h；如果系统层需要默认设备/总线信息，就通过 w25qxxx 的 core API、公共枚举或新增一个 core helper 获取，具体绑定仍留在 w25qxxx_port.c。

完成标志：

1. 不再存在跨模块 core 直接 include 外部 _port.h 的情况。
2. system、comm 这些上层模块只看得到下层 core 接口。

### P2. 清理公共头文件里的 port 泄漏

这一层优先级次于跨模块问题，但比改 .c 文件更重要，因为公共头文件会把污染扩散到所有调用方。

当前重点对象：

1. drvlayer/drvadc/drvadc.h
2. drvlayer/drvanlogiic/drvanlogiic.h
3. drvlayer/drvgpio/drvgpio.h
4. drvlayer/drviic/drviic.h
5. drvlayer/drvmcuflash/drvmcuflash.h
6. drvlayer/drvspi/drvspi.h
7. drvlayer/drvuart/drvuart.h
8. module/mpu6050/mpu6050.h
9. module/pca9535/pca9535.h
10. module/tm1651/tm1651.h
11. module/w25qxxx/w25qxxx.h
12. module/gd25qxxx/gd25qxxx.h
13. comm/frameparser/framepareser.h

需要做的事情：

1. 所有公共头文件移除对 _port.h 的直接 include。
2. 把 PortType、PortInterface、PortBinding 这类类型从公共头文件移出去。
3. core 对外只保留稳定语义，比如设备号、运行时配置、状态码、操作接口。
4. 与项目绑定相关的 bus、hook、默认宏、接口表，全部转移到 _port.h / _port.c。

建议重构模式：

1. 公共头只保留“抽象配置”。
2. 如果模块初始化确实需要底层能力，改成 core 内部私有上下文持有一份适配表，适配表由 port 在初始化或默认配置加载阶段注入。
3. 对外公开的 cfg 结构不要再出现 spiBind、iicBind 这种 port binding 字段，改为更稳定的逻辑设备号、逻辑通道号，或完全由 GetDefCfg/Init 在内部装配。

完成标志：

1. 任何上层 include 模块公共头时，都不会被迫看到 port 类型。
2. 公共 API 对迁移目标板是稳定的，不随 port 变化而变化。

### P3. 清理 core .c 对本模块 port 的直接依赖

这一层工作量最大，建议按模块批次推进。

当前已确认的重点文件：

1. ringbuffer/ringbuffer.c
2. console/log.c
3. comm/frameprocess/frameprocess.c
4. module/mpu6050/mpu6050.c
5. module/pca9535/pca9535.c
6. module/tm1651/tm1651.c
7. module/w25qxxx/w25qxxx.c
8. module/gd25qxxx/gd25qxxx.c
9. drvlayer/drvuart/drvuart.c
10. drvlayer/drvmcuflash/drvmcuflash.c

需要做的事情：

1. 把 core 当前直接调用的 XxxPort* 函数收敛成“由 port 注入的适配表”或者“由 port 提供默认配置，core 只消费普通数据结构”。
2. 删除 core 中通过 extern 访问 port 符号的写法。
3. 把延时、tick、临界区、缓冲区、默认资源映射、底层传输动作全部交给 port 注入。

建议按下面顺序改：

1. ringbuffer、log
原因：体量相对可控，改造后能沉淀通用模板。
2. drvuart、drvmcuflash
原因：驱动层是很多上层模块的基础，先把 driver core/core-port 边界打正。
3. mpu6050、pca9535、tm1651
原因：都是典型外设模块，适合统一抽象一套 port 注入模式。
4. w25qxxx、gd25qxxx
原因：接口较多、流程更长，放到模板成熟后处理更稳。
5. frameprocess
原因：它同时牵涉 frameparser、uart、tick、buffer，耦合最复杂，建议最后集中收口。

完成标志：

1. 这些 core .c 文件不再 include 自己的 _port.h。
2. 这些 core .c 文件里不再出现 XxxPort* 调用。

### P4. 建立自动检查，防止回归

如果不加检查，后续新增模块还会继续把 port 依赖写回 core。

需要做的事情：

1. 增加一条仓库检查规则：扫描非 _port、非 _debug 文件中是否出现 _port.h include。
2. 增加一条仓库检查规则：扫描公共头文件中是否出现 PortBinding、PortInterface、PortType 等命名。
3. 增加一条仓库检查规则：扫描 core 中是否出现 XxxPort* 调用或 extern port 符号。
4. 把这组检查命令写进脚本或文档，作为每轮改造后的固定回归动作。

完成标志：

1. 新增违规能在提交前被发现。

## 4. 推荐改造模式

后续改每个模块时，统一按下面 5 步走：

1. 先从公共头开始，移除 port 暴露。
2. 再把 core .c 里直接依赖的 port 动作识别出来，归类为：默认配置、资源映射、底层 IO、时序/延时、缓冲区、临界区。
3. 把这些能力收敛成 core 私有适配结构，或者改成由 port 预组装后的普通配置数据。
4. 在 _port.c 中完成实际绑定，把 drv/core/BSP 串起来。
5. 改完一个模块就立刻扫描一次，确认没有回归，再进下一个模块。

## 5. 推荐执行顺序

建议按下面的里程碑推进，不要并行大面积改造：

### 里程碑 M1. 规则定稿 + 跨模块问题清零

范围：

1. 补规则文档。
2. 修 frameprocess.h。
3. 修 systask_port.c。

目标：

- 先把最明显的边界反转清掉。

### 里程碑 M2. 公共头文件去 port 化

范围：

1. drvlayer 公共头一批。
2. module 公共头一批。
3. frameparser 公共头。

目标：

- 把 port 污染从 API 面收口。

### 里程碑 M3. 驱动层 core 去 port 化

范围：

1. ringbuffer
2. log
3. drvuart
4. drvmcuflash

目标：

- 沉淀出稳定的 core/port 适配模板。

### 里程碑 M4. 模块层 core 去 port 化

范围：

1. mpu6050
2. pca9535
3. tm1651
4. w25qxxx
5. gd25qxxx

目标：

- 把典型 sensor / flash 模块全部切到新模式。

### 里程碑 M5. 通信流程模块收口

范围：

1. frameprocess
2. frameparser 相关边界

目标：

- 完成最复杂模块的分层闭环。

### 里程碑 M6. 回归检查固化

范围：

1. 增加扫描脚本或检查命令。
2. 更新开发文档和新增模块模板。

目标：

- 后续新增模块默认按新规则落地。

## 6. 本周最值得先做的具体事项

如果只做最有价值的一小段，建议按下面顺序开始：

1. 先修 comm/frameprocess/frameprocess.h 对 framepareser_port.h 的依赖。
2. 再修 system/systask_port.c 对 w25qxxx_port.h 的依赖。
3. 然后统一改 drvlayer 下所有公共头，先把 include _port.h 去掉。
4. 接着选 ringbuffer 和 log 做首批样板重构。
5. 样板稳定后，再批量推广到 drvuart、drvmcuflash、mpu6050、pca9535、tm1651、w25qxxx、gd25qxxx、frameprocess。

## 7. 风险和注意事项

1. 不要一开始就同时重构所有模块，否则很难定位回归来源。
2. 不要为了去 port 化，把 BSP 或项目绑定细节塞进新的 core 公共头里，那只是换个名字继续耦合。
3. 不要把 port 接口表直接公开给上层；注入点可以存在，但尽量保持在 core 私有实现或 port 内部。
4. 每改完一个模块，都要重新跑一次违规扫描，确认没有遗留 include、extern、XxxPort* 调用。

## 8. 最终判断标准

当你把一个模块改完时，问自己 3 个问题：

1. 如果我换一块板子，这个模块的 core 代码需要改吗。
2. 如果我只改 _port.c / _port.h 和 BSP，模块是否还能工作。
3. 如果上层 include 这个模块的公共头，是否还能看到任何 port 概念。

这 3 个问题只要有一个回答不是“可以”，说明这个模块还没有真正分层完成。
