---
doc_role: layer-guide
layer: console
module: console
status: active
portability: layer-dependent
public_headers:
  - console.h
core_files:
  - console.c
  - console.md
port_files: []
debug_files: []
depends_on:
  - log.md
forbidden_depends_on:
  - console 核心直连业务驱动
required_hooks:
  - logGetInputCount
  - logGetInputTransport
  - logGetInputBuffer
  - logWriteToTransport
optional_hooks: []
common_utils:
  - tools/ringbuffer
copy_minimal_set:
  - console/console.h
  - console/console.c
  - console/log.h
read_next:
  - log.md
---

# Console 架构设计

这是 `console/` 目录中 console 子模块的权威入口文档。

## 1. 本层目标和边界

`console` 是建立在 `log` 之上的命令分发层，负责 transport 级输入会话、命令注册、参数切分和定向回复。

本层负责：

- 维护命令注册表。
- 维护每个输入 transport 的 session。
- 从 `log` 输入 ring buffer 取字节并组装命令行。
- 把回复定向写回命令来源 transport。

本层不负责：

- 直接包含业务模块内部实现。
- 直接解释 GPIO 极性、系统版本格式等业务语义。

## 2. 对外公共接口

稳定公共头文件：`console.h`

稳定 API：

- `consoleInit()` / `consoleInitDefault()`
- `consoleRegisterCommand()`
- `consoleProcess()`
- `consoleReply()`

调用顺序建议：

1. 先完成 `logInit()`。
2. 再初始化 `console` 核心。
3. 再由各模块注册自己的命令。
4. 在任务上下文周期调用 `consoleProcess()`。

## 3. 目录内文件职责

| 文件 | 职责 |
| --- | --- |
| `console.h` | 公共命令、session、handler 类型与 API |
| `console.c` | session 轮询、拆行、分词、查表分发、回复 |
| `console.md` | console contract |
| `log.h/.c` | transport 输入输出抽象，不负责命令业务 |

## 4. 依赖白名单与黑名单

- 允许依赖：`log` 提供的输入枚举、输入缓冲和定向写接口。
- 允许依赖：`ringbuffer` 作为输入缓冲的底层容器。
- 禁止依赖：`bspgpio.h`、MCU SDK 头、业务驱动私有结构。

## 5. 命令注册 contract

命令必须由所属模块注册，而不是由 `console` 核心硬编码。

推荐模式：

- `systemDebugConsoleRegister()` 注册 system 命令。
- `drvGpioDebugConsoleRegister()` 注册 GPIO 命令。
- 其他模块按同样模式暴露注册入口。

## 6. 公共函数使用契约

| 来源模块 | 公共函数 | 允许在哪些文件调用 | 用途 | 调用前提 | 典型调用顺序 | 返回值处理 | 禁止做法 |
| --- | --- | --- | --- | --- | --- | --- | --- |
| `log` | `logGetInputCount` | `console.c` | 枚举启用输入 transport | `logInit()` 后 | `Init -> Process` | 0 表示当前无输入 | 在业务模块中重复做 transport 枚举 |
| `log` | `logGetInputTransport` | `console.c` | 获取 transport 标识 | index 合法 | `Process` 中遍历 | 非法值直接跳过 | 把 transport 当业务 ID |
| `log` | `logGetInputBuffer` | `console.c` | 取得输入 ring buffer | transport 已启用 | `Init/Process` | `NULL` 视为该口不可用 | 让业务模块直接消费输入缓冲 |
| `log` | `logWriteToTransport` | `console.c` | 定向回复 | transport 合法 | handler 执行后 | 失败返回错误回复或丢弃 | 用广播日志替代命令回复 |

## 7. 改动落点矩阵

| 需求 | 应改文件 | 不该改的文件 |
| --- | --- | --- |
| 新增命令框架能力 | `console.h/.c` | 业务驱动 core |
| 新增某模块命令 | 该模块 `*_debug.*` 或注册文件 | `console.c` 中硬编码业务 |
| 调整回复路径 | `log.h/.c`、`console.c` | 业务模块公共 API |

## 8. 复制到其他工程的最小步骤

最小依赖集：

- `console.h/.c`
- `log.h/.c` 暴露的 transport 抽象
- `ringbuffer` 模块

外部项目至少要补齐：

- 输入 transport 枚举能力
- 每个 transport 的输入缓冲 provider
- transport 定向写接口

## 9. 验证清单

- 多 transport 输入时 session 不串线。
- 未知命令、参数错误和 transport 不可用时返回明确回复。
- 回复默认只回到来源 transport。
- `consoleProcess()` 单轮处理量有界。