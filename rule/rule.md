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
- `memory.md`：哪些事实值得沉淀到 memory。

## 2. 固定阅读顺序

任何新任务默认按下面顺序进入：

1. 先读 `rule/rule.md`。
2. 再读 `rule/map.md`，按任务类型缩小到目标目录。
3. 再读 `rule/coderule.md`，理解代码规则。
3. 进入目标父目录总文档，例如 `drvlayer/drvrule.md`、`module/module.md`、`comm/comm.md`；若任务涉及项目绑定组织或新项目搭建，先转到 `example/example.md`。
4. 再读目标叶子目录主文档。
5. 最后再看对应 `.h/.c`。

禁止一开始无差别遍历整个仓库，也不要默认把历史计划文档、设计草稿或补充说明当成最终 contract。

## 3. 三层入口模型

仓库内 md 统一按三层组织：

1. 仓库级入口：`rule/` 下的规则与地图文档。
2. 父目录总文档：例如 `drvlayer/drvrule.md`、`module/module.md`、`comm/comm.md`。
3. 叶子目录主文档：尽量与目录同名，例如 `drvuart/drvuart.md`、`frameprocess/frameprocess.md`。

项目绑定内容不再作为 `rep/` 顶层常驻层存在。凡是 `manager`、`system`、服务编排、任务接线这类当前工程相关内容，统一放在 `example/` 下作为参考示例，由 `example/example.md` 作为入口。

每个代码目录只允许一个“AI 首读主文档”。如果目录里还有 `architecture.md`、`plan.md`、`migration.md` 等文件，主文档必须在前部明确写出这些文件是补充阅读而不是权威契约。

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

先看 `portability` 等级，再决定是直接复用、补 hook，还是只复用 core 设计。

## 6. 文档更新的最低验收标准

一份主文档如果不能直接回答下面问题，就视为未完成：

1. 当前目录的权威主文档是哪一份。
2. 上层允许直接 include 哪些头文件。
3. core 允许依赖哪些公共函数，禁止碰哪些 port/BSP 细节。
4. 缺少 hook 时应该补哪些函数、补在哪一层、失败时返回什么。
5. 改默认绑定、改业务流程、改状态机时分别该改哪些文件。
6. 复制到其他工程时最少要带走哪些文件，还要补哪些外部实现。

## 7. 文档更新顺序

修改文档时优先级固定如下：

1. 先修 `rule/`。
2. 再修缺失的父目录总文档。
3. 再修高复用叶子目录主文档。
4. 最后再修 `example/` 下的项目示例文档。

这样可以保证新的叶子文档总是挂在稳定的上层规则之下。
