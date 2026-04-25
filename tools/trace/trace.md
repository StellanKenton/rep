---
doc_role: tool-spec
layer: tools
module: trace
status: active
portability: standalone
public_headers:
  - trace.h
core_files:
  - trace.c
port_files: []
debug_files: []
depends_on:
  - ../tools.md
forbidden_depends_on:
  - 直接依赖 RTT、UART、console 等输出实现
  - 在模块内部进入死循环或重启系统
required_hooks: []
optional_hooks: []
common_utils: []
copy_minimal_set:
  - trace.h
  - trace.c
read_next: []
---

# Trace 模块说明

这是当前目录的权威入口文档。

## 1. 模块定位

`trace` 提供 Cortex-M fault 处理能力，目标是在异常入口处把“异常入口汇编包装 + 当前堆栈帧采集 + fault 寄存器采集 + 默认格式化输出 + 停机策略”收敛为一个稳定模块，供项目层只补 transport 或持久化 hook。

当前模块默认负责：

- HardFault / MemManage / BusFault / UsageFault 的 Cortex-M 异常入口封装。
- 寄存器快照采集。
- 十六进制文本格式化输出。
- 默认停机。

项目层通常只需要覆盖 transport hook，或在需要时覆盖停机策略。

## 2. 目录内文件职责

| 文件 | 职责 |
| --- | --- |
| `trace.h` | fault 类型、堆栈帧与快照结构体、公共 API、platform hook 声明 |
| `trace.c` | Cortex-M fault handler、寄存器采集、默认文本输出和弱符号 hook |
| `trace.md` | 当前目录主文档 |

## 3. 对外公共接口

稳定公共头文件：`trace.h`

稳定 API：

- `traceFaultCapture()`
- `traceFaultHandle()`
- `HardFault_Handler()` / `MemManage_Handler()` / `BusFault_Handler()` / `UsageFault_Handler()`

## 4. 配置、状态与生命周期

- fault handler 通过 `LR bit[2]` 自动选择 MSP 或 PSP，并把 `EXC_RETURN` 传给 C 层。
- 模块不持有静态快照状态，采集缓冲由内部栈变量或调用方对象提供。
- 当 `frame == NULL` 时，仍会采集 fault 状态寄存器、MSP 和 PSP，并把 `hasStackFrame` 置为 `false`。
- 当前实现不依赖具体设备头，直接读取 Cortex-M 架构固定的 System Control Space 地址，因此对 STM32F1/F4/H7、GD32F1/F4/F7 以及同类 Cortex-M3/M4/M7 设备可直接复用。

## 5. 依赖白名单与黑名单

- 允许依赖 `rep_config.h` 和编译器提供的基础 intrinsic/inline assembly 能力。
- 禁止依赖具体芯片头、BSP、RTOS、console、manager 或项目日志输出。
- 禁止在采集路径中引入格式化库、堆分配或复杂分支。

## 6. 函数指针 / port / assembly 契约

| 类型 | 当前要求 |
| --- | --- |
| assembly 入口 | 模块内部已提供 ARM Cortex-M fault handler 包装，当前实现适配 Keil ARMCC 风格异常汇编 |
| platform hook | 项目层可覆盖 `traceFaultPlatformTransportInit()`、`traceFaultPlatformTransportWrite()`、`traceFaultPlatformHalt()` |
| 输出绑定 | 默认通过弱符号 transport hook 注入，不能把具体 RTT/UART 绑定写死回当前模块 |

## 7. 公共函数使用契约

| 函数 | 调用方 | 使用约束 |
| --- | --- | --- |
| `traceFaultCapture()` | 异常入口 C 层、故障诊断路径 | `snapshot` 不能为空；允许在中断/异常上下文调用；只负责采集 |
| `traceFaultHandle()` | 自定义异常入口、项目侧主动转发 | 允许在异常上下文调用；会关闭中断、输出快照并进入停机 hook |
| `traceFaultPlatformTransportWrite()` | 项目层 override | 必须无阻塞风险可控，且能在 fault 上下文使用 |

## 8. 改动落点矩阵

| 需求 | 应改文件 | 不该改的文件 |
| --- | --- | --- |
| 新增采集字段 | `trace.h`、`trace.c` | 项目层输出函数 |
| 改异常入口汇编风格以适配其他编译器 | `trace.c` | 项目层业务文件 |
| 改输出格式 | `trace.c` | 项目层业务文件 |
| 改输出介质或停机策略 | 项目层 hook 实现 | `trace.c` 核心采集逻辑 |

## 9. 复制到其他工程的最小步骤

最小依赖集：`trace.h/.c`。

迁移时需要额外满足：

- 目标芯片属于 Cortex-M3/M4/M7，或具备相同的 fault 状态寄存器布局。
- 若不是 Keil ARMCC，需要把 `trace.c` 中的异常汇编包装改成目标编译器写法。
- 项目层按需覆盖 transport/halt hook。

## 10. 验证清单

- `frame != NULL` 时，`pc/lr/psr` 与异常现场一致。
- `frame == NULL` 时，SCB fault 寄存器和 MSP/PSP 仍可读出。
- 模块不直接依赖 RTT/UART/LOG。
- STM32F1、STM32F4、STM32H7、GD32F1、GD32F4、GD32F7 等 Cortex-M3/M4/M7 设备无需替换设备头即可复用采集逻辑。