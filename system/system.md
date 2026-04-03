# System 模块说明

## 1. 文档目的

本文档面向后续切换到本项目的 AI 或维护者，用于快速识别 `USER/Rep/system` 的职责边界、启动方式、任务架构和修改约束。

如果需要修改 system 相关代码，建议先阅读以下文件：

1. `USER/Rep/rule/rule.md`
2. `USER/Rep/rule/mem.md`
3. `USER/main.c`
4. `USER/Rep/system/system.h`
5. `USER/Rep/system/systask_port.h`
6. `USER/Rep/system/system_debug.h`

## 2. 模块定位

`system` 目录不是底层 BSP，也不是具体业务模块。它承担的是应用启动编排层职责，主要负责：

- 维护系统模式状态。
- 驱动系统启动阶段的模式切换。
- 创建并编排与系统级运行相关的 FreeRTOS 任务。
- 接入 system 相关调试命令，并在启动阶段注册 console 命令。

当前目录下文件职责如下：

- `system.h` / `system.c`
  负责系统模式、版本号和模式字符串转换。
- `systask_port.h` / `systask_port.c`
  负责任务参数定义、任务入口函数、启动期任务创建和 console 初始化接线。
- `system_debug.h` / `system_debug.c`
  负责 system 调试命令注册和 `ver`、`sys`、`top` 命令实现。

## 3. 启动链路

系统入口在 `USER/main.c`，当前启动顺序如下：

1. `BSP_Init()`
2. `logInit()`
3. 创建 `DefaultTask`
4. 创建 `SystemTask`
5. `vTaskStartScheduler()`

其中：

- `DefaultTask` 只做状态灯翻转，是最小心跳任务。
- `SystemTask` 是 system 模块的主编排任务，周期调用 `process()` 推进系统模式。

system 模块的启动状态机当前为：

`INIT -> SELF_CHECK -> STANDBY`

当前模式语义：

- `eSYSTEM_INIT_MODE`
  启动初始状态，仅做一次初始化完成提示，然后切到自检态。
- `eSYSTEM_SELF_CHECK_MODE`
  执行启动期资源接线，目前主要包括 console 初始化和系统级任务创建。
- `eSYSTEM_STANDBY_MODE`
  当前稳定停驻态。进入后不再自动切换到业务运行态。
- `eSYSTEM_NORMAL_MODE`
  预留给后续正常运行逻辑。
- `eSYSTEM_UPDATE_MODE`
  预留给升级逻辑。
- `eSYSTEM_DIAGNOSTIC_MODE`
  预留给诊断逻辑。

如果检测到未知模式，`systask_port.c` 会记录 warning，并把模式重置回 `eSYSTEM_INIT_MODE`。

## 4. 任务架构

当前 system 直接管理的任务如下。

| 任务名 | 入口函数 | 周期 | 职责 |
| --- | --- | --- | --- |
| DefaultTask | `defaultTaskCallback` | 500 ms | 翻转状态灯 |
| SystemTask | `systemTaskCallback` | 10 ms | 运行 system 状态机 |
| ConsoleTask | `consoleTaskCallback` | 20 ms | 轮询 console，会在 `consoleProcess()` 内部顺带刷新 log 输出 |
| SensorTask | `sensorTaskCallback` | 1000 ms | 传感器任务占位 |
| GuardTask | `guardTaskCallback` | 1000 ms | 守护任务占位 |
| PowerTask | `powerTaskCallback` | 1000 ms | 电源任务占位 |
| MemoryTask | `memoryTaskCallback` | 1000 ms | 内存任务占位 |
| TaskCpuMon | `systemDebugTaskUsageSampler` | 50 ms x 20 次 | `top` 命令临时拉起的 CPU 使用率采样任务 |

说明：

- `DefaultTask` 和 `SystemTask` 由 `main.c` 创建。
- 其余常驻任务由 `SystemTask` 在 `SELF_CHECK` 阶段调用 `createTasks()` 创建。
- `createTask()` 会先检查任务句柄是否为空，因此重复进入 `SELF_CHECK` 时不会重复创建同一任务。
- `SensorTask`、`GuardTask`、`PowerTask`、`MemoryTask` 当前仍是空循环占位，后续应在各自任务内填入真实职责，而不是把业务逻辑堆进 `SystemTask`。

## 5. Console 与调试命令接线

console 接线在 `systask_port.c` 的 `initializeConsole()` 中完成，当前顺序如下：

1. `consoleInit()`
2. `systemDebugConsoleRegister()`
3. `drvGpioDebugConsoleRegister()`
4. `drvUartDebugConsoleRegister()`

这意味着 system 是当前 console 命令体系的启动接入点，但不是 console 核心实现本身。

当前 system 自带命令如下：

- `ver`
  返回 firmware version 和 hardware version。
- `sys`
  返回当前 system mode。
- `top`
  启动一个 1 秒采样窗口的任务 CPU 占用率监视任务。

调试开关由 `system_debug.h` 中的 `SYSTEM_DEBUG_CONSOLE_SUPPORT` 控制，默认值为 `1`。

## 6. 对外接口边界

### 6.1 `system.h` 暴露的公共能力

- `systemIsValidMode()`
- `systemGetMode()`
- `systemSetMode()`
- `systemGetModeString()`
- `systemGetFirmwareVersion()`
- `systemGetHardwareVersion()`

适合放在 `system.h/.c` 的内容：

- 与 system mode 本身直接相关的纯状态接口。
- 版本号、字符串、模式合法性等轻量逻辑。

不适合放在 `system.h/.c` 的内容：

- FreeRTOS 任务创建。
- console 注册。
- 硬件初始化细节。
- 大量平台相关宏。

### 6.2 `systask_port.h` 的职责

`systask_port.h` 当前承载的是 system 任务参数和任务入口声明，属于应用编排配置层。

如果要新增 system 级任务，优先在这里添加：

- 栈大小宏
- 优先级宏
- 周期宏
- 任务入口声明

不要把这些任务配置宏塞进 `system.h`，避免把“模式接口”和“RTOS 编排配置”混在一起。

## 7. 当前架构约束

后续 AI 修改该模块时，优先遵守以下约束：

- `system.c` 只维护 system mode 和版本语义，不承载任务编排。
- `systask_port.c` 是启动编排层，不要把具体驱动实现写进这里。
- `SystemTask` 应保持短周期、非阻塞，只推进状态机和调度初始化，不要塞长耗时业务。
- ISR、回调或高频路径相关逻辑不要下沉到 console 命令处理中。
- 任务占位逻辑后续应填充到对应任务中，不要为了省事把所有业务都挂到 `process()`。
- console 命令注册要由各自模块提供注册函数，再由 system 启动阶段统一接线。
- 新增 `LOG_*` 或复杂格式化逻辑时，要重新评估任务栈，不能默认沿用当前最小栈配置。
- `systemSetMode()` 已负责非法模式保护和日志输出，外部调用不应绕过它直接写全局状态。

## 8. AI 接手时的建议检查顺序

如果后续 AI 切换到本项目并需要修改 system 模块，建议按以下顺序建立上下文：

1. 阅读 `USER/main.c`，确认任务启动入口。
2. 阅读 `USER/Rep/system/systask_port.h`，确认当前任务参数和周期。
3. 阅读 `USER/Rep/system/systask_port.c`，确认任务创建路径和模式切换流程。
4. 阅读 `USER/Rep/system/system.h` 与 `system.c`，确认 mode API 和版本接口。
5. 阅读 `USER/Rep/system/system_debug.h` 与 `system_debug.c`，确认 console 命令和调试开关。
6. 如果变更涉及 console，再继续阅读 `USER/Rep/console/console.h` 与 `console.c`。
7. 如果变更涉及 GPIO/UART 调试命令，再继续阅读对应 `*_debug.c` 文件。

## 9. 常见改动落点

按需求选择文件，不要跨层误改：

- 新增或调整系统模式：改 `system.h` 和 `system.c`。
- 新增 system 级常驻任务：改 `systask_port.h` 和 `systask_port.c`。
- 新增启动阶段接线：优先改 `initializeConsole()` 或 `createTasks()`。
- 新增 system 调试命令：改 `system_debug.h` 和 `system_debug.c`。
- 调整任务周期、优先级、栈大小：改 `systask_port.h`。

## 10. 构建与验证

当前推荐验证方式：

- 使用 VS Code 任务 `Keil: Build` 或 `Keil: Rebuild`。
- Keil 工程文件位于 `Project/MDK-ARM/mcos-gd32.uvprojx`。

当前仓库没有成熟的自动化测试套件，因此 system 相关变更至少应完成：

1. 编译通过。
2. 启动日志和 console 初始化路径检查通过。
3. 若改动涉及任务调度、串口、RTT、GPIO 或模式切换，仍需板级硬件验证。

## 11. 一句话总结

`system` 是当前工程的系统编排层：`system.c` 管模式，`systask_port.c` 管任务和启动接线，`system_debug.c` 管 system 调试命令。后续扩展时保持这三层边界稳定，AI 才能持续按当前架构工作。