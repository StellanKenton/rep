---
doc_role: module-spec
layer: example
module: example
status: active
portability: standalone
public_headers:
  - example.h
core_files:
  - example.c
port_files: []
debug_files: []
depends_on: []
forbidden_depends_on: []
required_hooks: []
optional_hooks: []
common_utils: []
copy_minimal_set:
  - example/example.md
  - example/manager/manager.md
  - example/system/system.md
read_next:
  - ../rule/rule.md
  - ../rule/projectrule.md
  - ./manager/manager.md
  - ./system/system.md
---

# Example 主文档样例

这是 `example/` 目录的权威入口文档。

它既是仓库主文档推荐模板样例，也是当前仓库所有项目绑定内容的统一示例入口。后续新项目若要参考 `manager`、`system`、服务生命周期或任务接线，默认都从这里进入。

## 1. 模块定位

`example/` 不承担仓库顶层公共层职责，它只承载“项目怎么组织”的参考实现与文档样例。

本目录当前负责两类内容：

- 用 `example.md` 示范主文档应该如何回答边界、接口、hook 和改动落点。
- 用 `manager/`、`system/` 子目录承载当前工程相关的项目示例，供后续新项目参考。

## 2. 目录内文件职责

| 文件 | 职责 |
| --- | --- |
| `example.md` | `example/` 总入口，说明示例层用途和阅读顺序 |
| `manager/` | 项目服务编排与生命周期示例 |
| `system/` | 项目系统模式、任务接线与调试示例 |

## 3. 对外公共接口

本目录对外不提供可复用 C API，只提供项目组织示例入口：

- 新建项目时先读 `example/example.md`。
- 需要服务编排示例时读 `example/manager/manager.md`。
- 需要系统模式和任务接线示例时读 `example/system/system.md`。

## 4. 配置、状态与生命周期

`example/` 本身不定义统一生命周期；其子目录按项目示例分别定义：

- `example/manager/` 负责 service 生命周期与状态持有方式。
- `example/system/` 负责系统模式、任务启动与命令接线方式。

## 5. 依赖白名单与黑名单

允许依赖：`rule/` 下规则文档，以及 `example/manager/`、`example/system/` 之间为说明关系建立的文档引用。

禁止依赖：把 `example/` 误当成仓库顶层可复用公共层，或在其他文档里继续引用不存在的顶层 `manager/`、`system/` 入口。

## 6. 函数指针 / port / assembly 契约

`example/` 根目录本身没有平台 hook。项目绑定 contract 应写在具体示例目录内，而不是继续堆在总入口。

## 7. 公共函数使用契约

本目录不调用公共代码函数，只有文档阅读关系。

## 8. 改动落点矩阵

| 需求 | 应改文档 |
| --- | --- |
| 调整仓库级项目示例入口说明 | `example/example.md` |
| 调整服务编排或生命周期示例 | `example/manager/manager.md` 及对应 service 文档 |
| 调整系统模式、任务接线或 system 调试示例 | `example/system/system.md` |

## 9. 复制到其他工程的最小步骤

后续新项目参考本目录时，建议最少按下面顺序读取：

1. `example/example.md`
2. `example/manager/manager.md`
3. `example/system/system.md`

是否直接复用代码由具体项目决定，但目录结构、边界划分和接线方式优先参考这里。

## 10. 验证清单

检查点：

- 仓库级文档不再引用顶层 `manager/`、`system/`。
- 新项目相关说明都能从 `example/example.md` 进入。
- `example/manager/` 与 `example/system/` 的边界说明一致。
