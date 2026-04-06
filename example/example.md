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
  - example/example.h
  - example/example.c
  - example/example.md
read_next:
  - ../rule/rule.md
  - ../rule/projectrule.md
---

# Example 主文档样例

这是 `example/` 目录的权威入口文档，也是仓库主文档推荐模板样例。

## 1. 模块定位

本目录不承载真实业务，只用于示范“主文档应该如何回答边界、接口、hook 和改动落点”。

## 2. 目录内文件职责

| 文件 | 职责 |
| --- | --- |
| `example.h` | 示例公共头文件 |
| `example.c` | 示例实现 |
| `example.md` | 示例主文档 |

## 3. 对外公共接口

这里列稳定公共头文件、公开 API、调用顺序和返回值语义。

## 4. 配置、状态与生命周期

这里说明 `GetDefCfg/GetCfg/SetCfg/Init` 或 `Start/Process/Stop` 的真实 contract。

## 5. 依赖白名单与黑名单

这里列允许依赖的公共模块，以及禁止直接 include 的 `_port.h`、BSP 头和私有结构。

## 6. 函数指针 / port / assembly 契约

这里必须放固定表格，列出 required hooks、由谁实现、何时调用和失败语义。

## 7. 公共函数使用契约

这里必须放函数级调用表，而不是只写“依赖某模块”。

## 8. 改动落点矩阵

这里直接回答“改什么需求，应该改哪些文件”。

## 9. 复制到其他工程的最小步骤

这里写最小依赖集、必须补齐的外部实现和可裁剪文件。

## 10. 验证清单

这里写编译、日志、联调和回归风险检查项。
