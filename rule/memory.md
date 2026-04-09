---
doc_role: repo-rule
layer: rule
module: memory
status: active
portability: project-bound
public_headers: []
core_files:
	- memory.md
port_files: []
debug_files: []
depends_on:
	- rule.md
forbidden_depends_on: []
required_hooks: []
optional_hooks: []
common_utils: []
copy_minimal_set:
	- rule/memory.md
read_next: []
---

# Memory 规则

文档改造完成后，只有下面这些“稳定事实”值得沉淀为 memory：

- 某个目录的权威入口文档路径已经固定。
- 某层统一采用的生命周期模型、assembly 模式或 hook 模式已经稳定。
- 某个高复用目录的可复制等级、最小依赖集和禁止依赖项已经确认。
- 某个项目级边界已经被验证，例如 `system` 为 `project-bound`、`manager` 统一走 `service_lifecycle`。
- `example/` 已经固定为项目绑定内容和新项目参考结构的统一入口。

不要把下面这些内容写入 memory：

- 阶段性计划。
- 仍可能变化的草稿字段名。
- 只对单次任务有用的临时改写策略。

memory 记录的是“仓库事实”，不是“本次聊天过程”。
