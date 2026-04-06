# Rep 文档编排改造计划

## 1. 改造目标

本计划只处理文档编排，不直接改代码。目标是让 AI 在进入任意目录后，能够基于少量固定入口文档快速得到稳定结论，不再因为文档风格不统一、职责边界不清、函数指针契约缺失而跑偏。

本次文档改造要同时满足下面四个结果：

- AI 读完入口文档和目标目录主文档后，能明确该改哪些文件、不该改哪些文件。
- AI 能从目录主文档中直接看出函数指针需要哪些适配函数、这些函数由谁实现、失败时该返回什么。
- AI 能从目录主文档中直接看出当前目录允许使用哪些公共函数、这些公共函数的调用顺序、前置条件和错误处理规则。
- 当把某个目录单独复制到其他没有当前架构的项目时，维护者可以只根据该目录下的主文档补齐外部依赖和装配层，而不需要先理解整个仓库。

## 2. 当前主要问题

结合现有文档，当前最影响 AI 稳定写代码的不是“文档数量不够”，而是“文档槽位不统一”。主要问题如下：

- 同一层级的文档命名和目标不统一，有的是“设计说明”，有的是“计划”，有的是“架构说明”，AI 很难判断哪一份是权威入口。
- 很多文档已经说明了模块用途，但没有固定写出“公共 API / port 契约 / 公共函数使用方式 / 外部项目移植步骤”。
- 函数指针和适配层需求大多写在散文里，没有统一表格，AI 不容易稳定补出缺失函数。
- 目录复制到外部项目时，缺少“最小依赖集”和“必须实现的外部符号”说明。
- 一部分目录没有自己的父级入口文档，例如 `comm/` 和 `tools/`。
- 一部分文档存在命名或路径漂移，例如 `rule/map.md` 中仍有与当前目录结构不完全一致的引用；`tools/` 下的部分文档名不规则，容易让 AI 误判。
- 有些文档更像阶段性方案，例如 `comm/frameprocess/frameprocess.md` 当前更偏“模块计划”，还不是最终可复用 contract。

## 3. 目标文档架构

### 3.1 三层入口模型

后续所有代码目录的 md 统一按三层组织：

1. 仓库级入口文档
   位置：`rule/`
   作用：定义阅读顺序、分层规则、命名规则、文档模板规则。

2. 目录级总文档
   位置：每个父目录的主 md，例如 `drvlayer/drvrule.md`、`module/module.md`。
   作用：定义这一层的共性、允许依赖关系、下级目录应该如何写自己的主 md。

3. 叶子目录主文档
   位置：每个具体代码目录下，文件名尽量与目录同名，例如 `drvuart/drvuart.md`、`mpu6050/mpu6050.md`。
   作用：给出该目录真正的接口契约、装配契约、公共函数使用契约、改动落点矩阵和移植步骤。

### 3.2 每个代码目录只保留一个权威主文档

后续每个代码目录只保留一个“AI 首读主文档”。其他文档可以存在，但必须降级为补充文档。

统一规则如下：

- 主文档文件名尽量与目录同名。
- 主文档开头必须明确写出“这是当前目录的权威入口文档”。
- 如果目录里还有 `architecture.md`、`design.md`、`plan.md`、`migration.md` 这类补充文档，主文档必须在前部列出“补充阅读文档”，并说明各自作用。
- AI 默认先读主文档，不直接把补充文档当成权威 contract。

### 3.3 目录可复制性分级

每个叶子目录主文档必须声明自己的可复制等级，避免 AI 默认认为所有目录都能单独迁移。

建议统一成下面三级：

| 等级 | 含义 | 文档要求 |
| --- | --- | --- |
| `standalone` | 基本可单独复制，只需很少外部依赖 | 写清楚依赖文件、外部宏、初始化顺序 |
| `layer-dependent` | 可以复用，但依赖下层公共接口或通用工具目录 | 写清楚依赖目录、必须调用的公共函数和 port 适配项 |
| `project-bound` | 强依赖当前工程任务编排、系统模式或板级启动流程 | 写清楚不可脱离的部分，以及外部项目需要替换的接线点 |

这样 AI 在“复制目录到其他项目”场景下，先看可复制等级，再决定是直接复用、补 port、还是只复用 core 思路。

## 4. 主文档顶部统一增加文档头

为了让 AI 更稳地抽取关键信息，建议每份目录主文档在最前面增加一个固定文档头，优先使用 YAML front matter，字段尽量保持统一。

建议字段如下：

| 字段 | 含义 |
| --- | --- |
| `doc_role` | `repo-rule`、`layer-guide`、`module-spec`、`service-spec`、`tool-spec` |
| `layer` | `rule`、`console`、`drvlayer`、`module`、`manager`、`comm`、`system`、`tools` |
| `module` | 当前目录名或模块名 |
| `status` | `draft`、`active`、`needs-refresh` |
| `portability` | `standalone`、`layer-dependent`、`project-bound` |
| `public_headers` | 上层允许直接 include 的头文件 |
| `core_files` | 核心语义所在文件 |
| `port_files` | 绑定层或 assembly 层文件 |
| `debug_files` | debug/console 相关文件 |
| `depends_on` | 允许依赖的其他目录或公共模块 |
| `forbidden_depends_on` | 明确禁止直接依赖的对象 |
| `required_hooks` | 必需函数指针或外部适配函数 |
| `optional_hooks` | 可选函数指针或可选扩展点 |
| `common_utils` | 允许直接使用的公共函数模块 |
| `copy_minimal_set` | 迁移到其他工程至少要一起带走的文件或目录 |
| `read_next` | 建议继续阅读的依赖文档 |

文档头只解决“AI 快速抓主信息”的问题，详细 contract 仍然写在正文里。

## 5. 叶子目录主文档的固定章节

以后所有叶子目录主文档统一使用同一组标题，尽量不要增删。这样 AI 进入任何目录后，都能按相同位置找信息。

建议固定章节如下：

1. 模块定位
   说明本目录解决什么问题，不解决什么问题。

2. 目录内文件职责
   用表格列出每个 `.h/.c/.md` 文件的职责边界。

3. 对外公共接口
   列出公共头文件、稳定 API、调用顺序、返回值语义。

4. 配置、状态与生命周期
   写清楚 `GetDefCfg/GetCfg/SetCfg/Init`、`Start/Process/Stop/Recover` 等真实 contract。

5. 依赖白名单与黑名单
   明确允许依赖哪些公共层接口，禁止直接 include 哪些 `_port.h`、BSP 文件、外部私有结构。

6. 函数指针 / port / assembly 契约
   用固定表格写清楚需要哪些适配函数、谁来实现、何时调用、失败语义、是否必需。

7. 公共函数使用契约
   用固定表格列出允许调用的公共函数、调用前提、返回值处理、是否允许在 ISR 或任务上下文使用。

8. 改动落点矩阵
   把“想改什么需求，应该改哪些文件”直接写成表格，防止 AI 改错层。

9. 复制到其他工程的最小步骤
   写清楚最小依赖集、必须补齐的外部符号、可删除的当前项目绑定项。

10. 验证清单
    写清楚编译检查、日志检查、硬件联调检查和回归风险点。

## 6. 两张必须补齐的 contract 表

### 6.1 函数指针 / 适配函数契约表

这是最关键的一张表。以后凡是目录内部存在 port 适配、函数指针注入、弱符号平台钩子、assembly 接口，必须统一写成表，不再只写散文。

建议列如下：

| 名称 | 必需/可选 | 由谁实现 | 在哪里被调用 | 原型摘要 | 成功语义 | 失败语义 | 前置条件 | 备注 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |

这张表解决三个问题：

- AI 能直接看出少了哪些函数。
- AI 能直接看出该函数应该写在 core、port、debug 还是外部 BSP。
- AI 能直接看出失败时该返回什么，而不是乱造状态码。

### 6.2 公共函数使用契约表

这是第二张必须补齐的表。凡是目录会调用其他公共模块的函数，不要只写“依赖 drviic / ringbuffer / log”，必须写到函数级别。

建议列如下：

| 来源模块 | 公共函数 | 允许在哪些文件调用 | 用途 | 调用前提 | 典型调用顺序 | 返回值处理 | 禁止做法 |
| --- | --- | --- | --- | --- | --- | --- | --- |

这张表可以直接解决“AI 不知道如何正确使用公共函数”的问题。

例如：

- `module/mpu6050` 不只写“依赖 drviic”，而是写清楚 adapter 里实际调用哪些 `drvIic*` / `drvAnlogIic*` 接口。
- `drvuart` 不只写“依赖 ringbuffer”，而是写清楚哪个函数负责写、哪个函数负责读、谁拥有底层存储区。

## 7. 父目录总文档的固定章节

父目录总文档不需要写具体函数指针表，但必须统一覆盖下面内容：

1. 本层目标和边界。
2. 本层允许依赖哪些下层，禁止碰哪些更低层细节。
3. 下级目录主文档必须包含哪些章节。
4. 本层通用命名模式，例如 `core / port / debug / assembly`。
5. 本层常见错误写法和反例。
6. 本层目录复制到其他工程时，一般如何处理 port 和 assembly。
7. 本层 AI 推荐阅读顺序。

## 8. 各目录具体修改计划

### 8.1 P0：先修规则入口文档

这一步最重要，因为它决定 AI 会先看哪些文档、相信哪些文档。

#### `rule/rule.md`

改造目标：

- 补成真正的仓库总入口。
- 明确“先读规则，再按 map 进入目标目录”的固定流程。
- 增加“目录主文档统一模板”和“补充文档降级规则”。
- 增加“复制目录到其他工程”场景下的阅读顺序。

#### `rule/map.md`

改造目标：

- 修正与当前实际目录不一致的路径说明。
- 补齐 `comm/`、`tools/`、`example/` 的真实入口关系。
- 增加“按任务类型找文档”的矩阵。
- 增加“按复制目标找依赖”的矩阵。

#### `rule/projectrule.md`

改造目标：

- 在现有 core-port 约束基础上，增加文档层面的硬约束。
- 规定每个叶子目录必须写“函数指针契约表”和“公共函数使用表”。
- 规定公共 API、assembly API、debug API 必须分开写。
- 规定复制到外部项目时，必须写“最小移植集”和“必须补齐的外部实现”。

#### `rule/coderule.md`

改造目标：

- 补充命名规范和缩写规则在文档层的对应写法。
- 给 adapter、hook、assemble、platform provider 等命名补统一建议。

#### `rule/memory.md`

改造目标：

- 不必写很多，只补一段“文档更新后哪些事实值得沉淀为 memory”。

### 8.2 P1：补齐父目录总文档

这一步负责把各层的“共性规则”固定下来。

#### `drvlayer/drvrule.md`

重点补充：

- `drvxxx.md` 的统一模板。
- `drvxxx.h`、`drvxxx.c`、`drvxxx_port.*`、`drvxxx_debug.*`、外部 BSP 的职责表。
- 必需钩子和可选钩子的文档写法。
- 驱动目录复制到外部项目时的最小移植模式。

#### `module/module.md`

重点补充：

- passive module 的固定 contract 写法。
- 公共 API 与 assembly API 的分离写法。
- `GetDefCfg/GetCfg/SetCfg/Init` 与 `PortAssemble*` 的固定说明方式。
- module 对下层 drv 公共函数的使用表模板。

#### `manager/manager.md`

重点补充：

- service 生命周期模板。
- `Init/Start/Process/Stop/Recover/GetStatus/GetLastError` 的文档槽位。
- manager 子模块复制到其他工程时，哪些是 project-bound，哪些可以保留。

#### `system/system.md`

重点补充：

- 标明 `system` 目录的可复制等级大概率是 `project-bound`。
- 明确 system 与 manager、console、task 创建、模式切换的接口边界。
- 增加“如果复制到外部项目，哪些内容只能参考，不能直接搬”的说明。

#### `console/console.md` 与 `console/log.md`

重点补充：

- console 的命令注册 contract。
- log 的输入输出、transport、单播/广播语义 contract。
- console 与 log 各自允许暴露给外部的公共函数表。

#### 新增 `comm/comm.md`

新增目标：

- 作为 `comm/` 父目录入口，说明 `flowparser`、`frameparser`、`frameprocess` 的分工。
- 规定 parser 类目录如何描述状态机、token、frame、platform hook。

#### 新增 `tools/tools.md`

新增目标：

- 作为 `tools/` 父目录入口，说明工具型目录和业务型目录的差异。
- 规定算法类、容器类工具文档如何写公共 API、状态、线程安全与可复制性。

#### 新增 `example/example.md`

新增目标：

- 做成一份“标准主文档样例”，后续新目录直接照着填。

### 8.3 P2：优先改高复用叶子目录主文档

优先级最高的是那些会被其他目录频繁依赖、并且最容易影响 AI 决策的目录。

#### 驱动层高优先级

- `drvlayer/drvuart/drvuart.md`
- `drvlayer/drviic/drviic.md`
- `drvlayer/drvspi/drvspi.md`
- `drvlayer/drvgpio/drvgpio.md`
- `drvlayer/drvanlogiic/drvanlogiic.md`
- `drvlayer/drvadc/drvadc.md`
- `drvlayer/drvmcuflash/drvmcuflash.md`

这些文档要重点补齐：

- 必需 BSP 钩子表。
- 可选钩子表。
- 逻辑资源和物理资源的映射位置。
- debug/console 是否属于可选能力。
- 迁移到外部项目时需要补的 BSP 最小函数集。

#### module 层高优先级

- `module/mpu6050/mpu6050.md`
- `module/pca9535/pca9535.md`
- `module/tm1651/tm1651.md`
- `module/w25qxxx/w25qxxx.md`
- `module/gd25qxxx/gd25qxxx.md`

这些文档要重点补齐：

- `GetDefCfg/GetCfg/SetCfg/Init` 的真实 contract。
- `PortAssemble*` 或其他 assembly 接口的职责表。
- 对下层 drv 公共函数的使用表。
- 复制到外部项目时，需要保留哪些 core 文件、哪些 port 文件需要重写。

#### 通讯层高优先级

- `comm/frameparser/frameparser.md`
- `comm/flowparser/flowparser.md`
- `comm/frameprocess/frameprocess.md`

这些文档要重点补齐：

- 状态机或流式解析器的核心状态表。
- 平台钩子或 parser hook 的契约表。
- 输入输出缓存 ownership。
- 与 UART、tick、ringbuffer、协议默认值的关系。

其中 `frameprocess.md` 需要从“计划文档”升级成“最终模块 contract 文档”。

#### 工具层高优先级

- `tools/ringbuffer/ringbuffer_architecture.md`
- `tools/numfilter/numfilter.md`
- `tools/butterworthfilter/butterworthfilter.md`
- `tools/filter1st/filtterfisrt.md`
- `tools/filter2nd/filtersecd.md`

这里建议做两件事：

- 补齐一个与目录同名的主文档，或把现有文档改成主文档。
- 修正规则不统一的命名，避免 AI 在 `filter1st` / `filtterfisrt.md`、`filter2nd` / `filtersecd.md` 之间误判。

### 8.4 P3：再改项目绑定更强的目录文档

这些目录不一定适合“单独复制即用”，但仍然要把边界写清楚，避免 AI 改错层。

#### `manager/power/power.md`
#### `manager/selfcheck/selfcheck.md`
#### `manager/update/update.md`
#### `console/console.md`
#### `console/log.md`
#### `system/system.md`

这批文档重点补齐：

- 生命周期 contract。
- 任务上下文或启动上下文约束。
- project-bound 接线点。
- 只能参考不能直接迁移的部分。

## 9. 目录主文档的推荐正文模板

后续可以把下面这一套直接复制到每个叶子目录主文档里，再按模块填充内容：

```md
---
doc_role: module-spec
layer: module
module: xxx
status: active
portability: layer-dependent
public_headers:
  - xxx.h
core_files:
  - xxx.c
port_files:
  - xxx_port.h
  - xxx_port.c
depends_on:
  - drvlayer/yyy
forbidden_depends_on:
  - bsp_*.h
required_hooks:
  - xxxPortInit
optional_hooks:
  - xxxDebugRegister
common_utils:
  - tools/ringbuffer
copy_minimal_set:
  - xxx.h
  - xxx.c
  - xxx_port.h
  - xxx_port.c
read_next:
  - module/module.md
  - drvlayer/yyy/yyy.md
---

# xxx 模块说明

## 1. 模块定位
## 2. 目录内文件职责
## 3. 对外公共接口
## 4. 配置、状态与生命周期
## 5. 依赖白名单与黑名单
## 6. 函数指针 / port / assembly 契约
## 7. 公共函数使用契约
## 8. 改动落点矩阵
## 9. 复制到其他工程的最小步骤
## 10. 验证清单
```

## 10. 执行顺序建议

建议按下面顺序落地，而不是同时平铺修改所有 md：

1. 先改 `rule/rule.md`、`rule/map.md`、`rule/projectrule.md`。
2. 再补 `comm/comm.md`、`tools/tools.md`、`example/example.md`。
3. 再统一 `drvlayer/drvrule.md`、`module/module.md`、`manager/manager.md`。
4. 先改高复用目录：UART、IIC、SPI、ringbuffer、frameparser、frameprocess、mpu6050、w25qxxx。
5. 再改其余叶子目录。
6. 最后回头检查所有主文档的章节顺序、文档头字段名和表格列名是否完全统一。

## 11. 验收标准

当下面这些问题都能只靠主文档回答出来，本次改造才算真正完成：

1. 这个目录的权威入口 md 是哪一份。
2. 这个目录允许上层 include 哪些头文件。
3. 这个目录的 core 能直接依赖哪些公共函数，禁止直接依赖哪些 port/BSP 细节。
4. 这个目录缺失函数指针时，应该补哪些函数、补在哪一层、返回什么错误语义。
5. 如果要改“默认总线/默认资源/默认平台绑定”，应该改哪里。
6. 如果要改“寄存器流程/解析流程/业务状态机”，应该改哪里。
7. 如果把这个目录复制到其他工程，至少要一起复制哪些文件，还要补哪些外部实现。
8. 如果复制后不需要 debug/console，哪些文件可以不带走。

## 12. 最终原则

后续所有 md 改造都围绕一个原则展开：

不要让 AI 靠猜测补架构，而要让 AI 靠主文档直接读出边界、接口、依赖、函数指针契约和迁移步骤。

只要每个目录主文档都固定回答“我是谁、我依赖谁、我暴露什么、我要别人实现什么、我要怎么被移植”，AI 写代码时就会稳定很多，也更适合把单个目录复制到新项目继续复用。