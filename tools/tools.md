---
doc_role: layer-guide
layer: tools
module: tools
status: active
portability: standalone
public_headers: []
core_files:
  - tools.md
port_files: []
debug_files: []
depends_on: []
forbidden_depends_on:
  - 工具模块直连业务或 BSP
required_hooks: []
optional_hooks: []
common_utils: []
copy_minimal_set:
  - tools/
read_next:
  - aes/aes.md
  - md5/md5.md
  - ringbuffer/ringbuffer.md
  - trace/trace.md
  - numfilter/numfilter.md
  - butterworthfilter/butterworthfilter.md
---

# Tools 层总文档

这是 `tools/` 的权威入口文档。

## 1. 本层目标和边界

`tools` 目录承载算法、滤波器和基础容器，目标是为其他层提供可复用、低耦合的基础能力。

本层负责：

- 通用容器，例如 `ringbuffer`。
- 数值算法、插值和统计工具。
- 滤波器与控制辅助算法。

本层不负责：

- 直接依赖业务流程。
- 隐式依赖 RTOS、BSP 或板级资源。

## 2. 子目录分工

| 目录 | 职责 |
| --- | --- |
| `aes/` | AES 分组加解密工具，支持 ECB/CBC 和 PKCS7 辅助处理 |
| `md5/` | MD5 摘要计算与 16/32 位十六进制字符串转换 |
| `ringbuffer/` | 字节环形缓冲区与并发约束 |
| `trace/` | Cortex-M fault 快照采集工具 |
| `numfilter/` | 数值算法与统计工具 |
| `butterworthfilter/` | 二阶 Butterworth 低通滤波器 |
| `filter1st/` | 一阶离散滤波器 |
| `filter2nd/` | 二阶离散滤波器 |

## 3. 下级目录主文档必须写清的内容

- 公共 API。
- 状态对象和 ownership。
- 线程安全 / ISR 使用限制。
- 可复制等级和最小依赖集。

## 4. 命名规则

- 目录主文档尽量与目录同名。
- 旧的 `filtterfisrt.md`、`filtersecd.md` 可保留为补充文档，但权威入口应切换为同名主文档。

## 5. 常见错误写法和反例

- 工具模块直接调用业务层接口。
- 不写清状态对象是否可跨任务共享。
- 只给示例代码，不给 API contract 和边界条件。

## 6. 复制到其他工程时如何处理

大多数工具目录属于 `standalone`：只要补齐头文件与实现并遵守调用约束，通常可以单独迁移。

## 7. AI 推荐阅读顺序

1. 先读本文件。
2. 需要容器时读 `ringbuffer/ringbuffer.md`。
3. 需要摘要或分组加密时先读 `md5/md5.md`、`aes/aes.md`。
4. 需要 Cortex-M fault 现场采集时读 `trace/trace.md`。
5. 需要滤波或数值算法时再进入对应目录主文档。
