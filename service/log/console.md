---
doc_role: layer-guide
layer: console
module: console
status: active
portability: layer-dependent
public_headers:
  - log.h
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
  - log/console.h
  - log/console.c
  - log/log.h
read_next:
  - log.md
---

# Console 架构设计

这是 `service/log/` 目录中 console 子模块的权威入口文档。

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

稳定公共头文件：`log.h`

稳定 API：

- `logInit()`
- `logRegisterConsole()`
- `ConsoleBackGournd()`

兼容说明：

- `console.h` 保留为目录内和 debug 子模块的兼容头，不再作为系统装配层首选入口。
- `consoleReply()` 等名字只建议在目录内兼容使用；系统装配层应以 `log.h` 为准。

内置保留命令：

- `help`：由 `console` 核心直接处理，打印当前已注册命令的分组和入口命令。

调用顺序建议：

1. 先完成 `logInit()`。
2. 再由各模块调用 `logRegisterConsole()` 注册自己的命令。
3. 在任务上下文周期调用 `ConsoleBackGournd()`。

## 3. 目录内文件职责

| 文件 | 职责 |
| --- | --- |
| `console.h` | console 内部声明与兼容宏；对外首选 `log.h` |
| `console.c` | session 轮询、拆行、分词、查表分发、回复 |
| `console.md` | console contract |
| `log.h/.c` | 对外统一入口，负责 log core、console 装配和后台轮询 |

## 4. 依赖白名单与黑名单

- 允许依赖：`log` 提供的输入枚举、输入缓冲和定向写接口。
- 允许依赖：`ringbuffer` 作为输入缓冲的底层容器。
- 禁止依赖：`bspgpio.h`、MCU SDK 头、业务驱动私有结构。

## 5. 命令注册 contract

命令必须由所属模块注册，而不是由 `console` 核心硬编码。

保留字约束：

- 业务模块不得注册 `help`，该命令名保留给 `console` 核心。

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
| `log` | `logRegisterConsole` | manager / system / debug 注册文件 | 统一注册命令 | `logInit()` 后 | `Init -> Register -> ConsoleBackGournd` | `false` 表示注册失败或重名不一致 | 外部继续直接依赖 `consoleRegisterCommand()` |

## 7. 改动落点矩阵

| 需求 | 应改文件 | 不该改的文件 |
| --- | --- | --- |
| 新增命令框架能力 | `console.h/.c` | 业务驱动 core |
| 新增某模块命令 | 该模块 `*_debug.*` 或注册文件 | `console.c` 中硬编码业务 |
| 调整回复路径 | `log.h/.c`、`console.c` | 业务模块公共 API |
| 调整系统侧初始化顺序 | `log.c`、系统装配文件 | 各 debug 模块里手工补 `consoleInit()` |

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
- `ConsoleBackGournd()` 单轮处理量有界。