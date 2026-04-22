---
doc_role: repo-rule
layer: rule
module: repo
status: active
portability: project-bound
public_headers: []
core_files:
	- rule.md
	- map.md
	- projectrule.md
	- coderule.md
	- memory.md
port_files: []
debug_files: []
depends_on: []
forbidden_depends_on:
	- 把补充文档当成权威主文档
	- 跳过 map.md 直接横扫整个仓库
required_hooks: []
optional_hooks: []
common_utils: []
copy_minimal_set:
	- rule/
read_next:
	- map.md
	- projectrule.md
	- coderule.md
---

# Rep 仓库入口规则

这是当前仓库的权威入口文档。

## 1. 文档定位

本文档只定义三件事：

- AI 和维护者进入仓库后的固定阅读顺序。
- 仓库内 md 的权威层级和命名规则。
- “复制目录到其他工程”场景下的最小阅读路径。

补充阅读文档：

- `map.md`：目录地图、任务入口矩阵、复制场景矩阵。
- `projectrule.md`：core-port 分层和文档硬约束。
- `coderule.md`：代码与文档命名风格。

## 2. 固定阅读顺序

任何新任务默认按下面顺序进入：

1. 先读 `rule/rule.md`。
2. 再读 `rule/map.md`，按任务类型缩小到目标目录。
3. 再读 `rule/coderule.md`，理解代码规则。
3. 进入目标父目录总文档，例如 `driver/drvrule.md`、`module/module.md`、`comm/comm.md`；若任务涉及项目绑定组织或新项目搭建，先转到 `example/example.md`。
4. 再读目标叶子目录主文档。
5. 最后再看对应 `.h/.c`。

禁止一开始无差别遍历整个仓库，也不要默认把历史计划文档、设计草稿或补充说明当成最终 contract。

## 3. 三层入口模型

仓库内 md 统一按三层组织：

1. 仓库级入口：`rule/` 下的规则与地图文档。
2. 父目录总文档：例如 `driver/drvrule.md`、`module/module.md`、`comm/comm.md`。
3. 叶子目录主文档：尽量与目录同名，例如 `drvuart/drvuart.md`、`frameprocess/frameprocess.md`。

项目绑定内容不再作为 `rep/` 顶层常驻层存在。凡是 `manager`、`system`、服务编排、任务接线这类当前工程相关内容，统一放在 `example/` 下作为参考示例，由 `example/example.md` 作为入口。

每个代码目录只允许一个“AI 首读主文档”。

## 4. 主文档统一要求

每份主文档都必须满足下面约束：

- 开头明确写出“这是当前目录的权威入口文档”。
- 使用统一的 YAML front matter，至少包含 `doc_role`、`layer`、`module`、`status`、`portability`、`public_headers`、`depends_on`、`forbidden_depends_on`、`required_hooks`、`common_utils`、`copy_minimal_set`、`read_next`。
- 叶子目录主文档必须包含固定章节，并补齐两张 contract 表：
	- `函数指针 / port / assembly 契约表`
	- `公共函数使用契约表`
- 父目录总文档必须写清本层目标、允许依赖、命名模式、反例、可复制性和推荐阅读顺序。

## 5. 复制到其他工程时的阅读顺序

当任务是“把某个目录复制到其他工程”时，默认顺序改成：

1. 先读本文件。
2. 再读 `rule/map.md`，确定依赖父目录。
3. 读目标父目录总文档。
4. 读目标叶子目录主文档中的 `复制到其他工程的最小步骤`。
5. 最后再补看源码和 assembly / debug 文件。

- 先看 `portability` 等级，再决定是直接复用、补 hook，还是只复用 core 设计。
- `*_design.md` 里也会有一些设计原则和示例，如果没有特别需要，通常不看。

## 6. 文档更新的最低验收标准

一份主文档如果不能直接回答下面问题，就视为未完成：

1. 当前目录的权威主文档是哪一份。
2. 上层允许直接 include 哪些头文件。
3. core 允许依赖哪些公共函数，禁止碰哪些 port/BSP 细节。
4. 缺少 hook 时应该补哪些函数、补在哪一层、失败时返回什么。
5. 改默认绑定、改业务流程、改状态机时分别该改哪些文件。
6. 复制到其他工程时最少要带走哪些文件，还要补哪些外部实现。
7. 新增的宏、`typedef`、枚举别名、函数指针类型应该放在哪个 `.h` 文件，而不是落在 `.c` 文件里。

## 7. 文档更新顺序

修改文档时优先级固定如下：

1. 先修 `rule/`。
2. 再修缺失的父目录总文档。
3. 再修高复用叶子目录主文档。
4. 最后再修 `example/` 下的项目示例文档。

这样可以保证新的叶子文档总是挂在稳定的上层规则之下。

## 8. 新增文件约束

1.当新增文件夹及对应的*.c 或*.h时，必须先新增对应的 md 主文档，md 主文档包含三个部分：1.该文件夹的概要设计；2.该文件夹的接口契约，比如port文件应该怎么设计、bsp应该包含哪些函数来绑定到port文件；3.该core文件的使用示例。
2.新增了文件夹和md文件后，应该对应修改上层父目录的总文档，补充新的子目录和主文档的介绍，以及新的依赖关系和命名规则。不能直接新增文件夹和md文件，而不修改父目录总文档。

## 9. 程序编写约束

- 硬规则：宏定义和所有 `typedef` 定义必须放在 `.h` 文件中，不能放在 `.c` 文件中。
- 上一条没有例外：即使该类型只在单个 `.c` 文件内部使用，也不能把 `typedef struct {...} Xxx;`、`typedef enum {...} Xxx;`、函数指针 `typedef` 或类型别名写在 `.c` 文件中；必须放到对应配套 `.h` 中。
- AI 或维护者在提交前必须做一次结构检查：只要本次改动新增了宏、`typedef`、枚举别名、函数指针类型，就必须逐项确认它们都位于 `.h` 文件；若出现在 `.c` 文件，视为直接违规，不接受“仅内部使用所以放 `.c`”这类理由。
- 代码中禁止直接使用 printf、puts、putchar 等标准库函数输出日志或调试信息，必须使用 LOG_I、LOG_W、LOG_E 等宏输出日志。
- 除 `User/port/rtos_port.*` 外，项目层与 `rep/` 复用层禁止直接调用原生 RTOS API；需要延时、tick、任务创建、统计初始化、互斥或队列能力时，统一通过 `rep/service/rtos/rtos.h` 暴露的 `repRtos*` 接口访问。
- 当新增功能时，除非明确要求，否则不要更改 rep 文件夹下的内容，尽量更改其他文件来适配 rep 文件夹下的内容。
- 非常重要：critical：当前项目是在MCU资源受限的环境下运行的，rep文件夹下的代码应该尽量保持轻量级和高效，避免引入过多的依赖和复杂的逻辑，以免影响系统性能和稳定性。critical' 复用性>高效>可读>资源占用--以最少的代码实现功能！！！'critical
- 非常重要：critical：程序编写时要简介明了，不能编写复杂的逻辑和过多的分支，尽量保持代码的简单性和可读性，以便后续维护和复用。
- 如果出现修改代码涉及参与编译文件的修改或者目录修改，要直接将参与编译的目录修改掉，而不是只创建一个兼容层来适配之前的代码。比如新增了一个模块，应该直接将该模块加入到编译目录中，而不是先创建一个兼容层来适配之前的代码。

## 10. 编码前自检

在开始写 `.c/.h` 之前，默认先检查下面四项：

1. 这次是否新增了宏、`typedef`、枚举别名、函数指针类型；如果有，先决定它们落在哪个 `.h` 文件。
2. 这次是否新增了目录或文件；如果有，先补主文档和上层父目录文档。
3. 这次是否会改编译参与文件；如果会，直接修改真实编译入口，不要额外做兼容层。
4. 这次是否误把项目绑定逻辑写进 `rep/`；如果是，优先回到 `User/` 或 `User/port/`。


