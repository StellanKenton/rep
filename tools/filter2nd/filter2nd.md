---
doc_role: tool-spec
layer: tools
module: filter2nd
status: active
portability: standalone
public_headers:
  - filtersecd.h
core_files:
  - filtersecd.c
port_files: []
debug_files: []
depends_on: []
forbidden_depends_on: []
required_hooks: []
optional_hooks: []
common_utils: []
copy_minimal_set:
  - filtersecd.h
  - filtersecd.c
read_next:
  - filtersecd.md
---

# Filter2nd 模块说明

这是当前目录的权威入口文档。

补充阅读文档：`filtersecd.md` 保留为历史说明；最终 contract 以本文件为准。

## 1. 模块定位

`filter2nd` 提供二阶离散滤波器对象，适用于比一阶滤波更强的平滑、补偿或低通场景。

## 2. 目录内文件职责

| 文件 | 职责 |
| --- | --- |
| `filtersecd.h` | 二阶滤波对象、参数和公共 API |
| `filtersecd.c` | 差分方程更新与历史状态推进 |
| `filter2nd.md` | 权威主文档 |
| `filtersecd.md` | 历史补充说明 |

## 3. 对外公共接口

稳定公共头文件：`filtersecd.h`

稳定能力：对象初始化、分子 / 分母参数设置、初始条件设置和单步更新。

## 4. 配置、状态与生命周期

- 滤波器对象由调用方持有。
- 历史输入输出状态属于运行态，不应由外部直接改内存。
- 更新接口假设固定采样周期，若采样周期变化应重新校核系数。

## 5. 依赖白名单与黑名单

- 不依赖 RTOS、BSP 或业务模块。
- 禁止在未知采样周期下直接复用原有参数。

## 6. 函数指针 / port / assembly 契约

当前目录无平台 hook。

## 7. 公共函数使用契约

当前目录不调用其他公共模块函数。

## 8. 改动落点矩阵

| 需求 | 应改文件 | 不该改的文件 |
| --- | --- | --- |
| 改二阶滤波方程或状态推进 | `filtersecd.c/.h` | 上层业务逻辑 |
| 改历史说明 | `filtersecd.md` | `filter2nd.md` 主 contract |

## 9. 复制到其他工程的最小步骤

最小依赖集：`filtersecd.h/.c`。

## 10. 验证清单

- 初始化与初始条件设置后状态一致。
- 不同运行形式的输出与设计预期一致。
- 连续运行时历史状态推进正确。
