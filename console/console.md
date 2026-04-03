# Console 架构设计

## 1. 目标

在 `USER/Rep/console` 下新增一层独立于 `log` 的命令控制台模块，用于处理来自 RTT、UART 等输入通道的文本命令，并将命令分发到对应业务模块。


本设计需要满足以下目标：

- 支持从 `log` 输入通道接收命令行文本。
- 支持同时开启多个输入通道，并分别处理各自收到的命令。
- 命令采用注册式管理，避免 `console` 核心直接依赖具体驱动实现。
- 命令处理函数归属到各自模块，例如 GPIO 命令归 `DrvGpio`，版本信息命令归 `system`。
- 保持高内聚、低耦合，避免把驱动逻辑写进 `console` 核心。

## 2. 当前基础

现有 `log` 模块已经具备以下能力：

- `logGetInputBuffer(transport)` 可按 transport 获取输入 ring buffer。
- RTT 与 UART 都可以作为输入源。
- 每个 transport 自己维护输入缓冲区。

因此，`console` 不应直接并入 `log.c`，而应在 `log` 之上增加一层命令处理模块。

## 3. 设计原则

### 3.1 单一职责

- `log` 负责日志格式化、输出通道写入、输入 buffer 暴露。
- `console` 负责命令行接收、拆行、分词、查表、执行、回复。
- `DrvGpio`、`system` 等业务模块负责各自命令的注册和业务处理。

### 3.2 高内聚低耦合

- `console` 核心不直接调用 `bspGpio*`、`gd32f4xx_*` 或其他硬件细节函数。
- 命令归属模块只通过公开接口与 `console` 交互。
- 模块通过“注册函数”把命令接入 `console`，而不是由 `console` 主动了解每个驱动的内部实现。

### 3.3 多输入隔离

- 每个输入 transport 都有独立的会话上下文。
- 每个 transport 的接收缓存、当前命令行、解析状态、回复路径都彼此独立。
- 一个 transport 上的命令不能污染另一个 transport 的处理中间状态。

### 3.4 有界执行

- 命令接收和解析应保持轻量。
- 单次处理只消费有限字节，避免长时间占用任务上下文。
- ISR 不直接做命令解析；解析放在任务上下文执行。

## 4. 模块分层

建议后续按以下结构实现：

- `console.h`
  - 对外暴露 `consoleInit`、`consoleProcess`、`consoleRegisterCommand` 等接口。
- `console.c`
  - 实现 transport 会话管理、命令行拼装、参数切分、查表分发、定向回复。
- `log.c` / `log.h`
  - 维持 transport 输入输出抽象。
  - 后续补充少量接口，供 `console` 获取启用的输入 transport，并支持定向回复。
- `drvgpio.c` / `drvgpio.h`
  - 增加 GPIO 命令注册函数与 GPIO 命令处理函数。
- `system.c` / `system.h`
  - 增加版本类命令注册函数与版本查询命令处理函数。

其中 `console` 是命令框架层，不是业务层。

## 5. 核心职责划分

### 5.1 `console` 核心负责的内容

- 维护命令注册表。
- 维护每个 transport 的 console session。
- 从 `log` 输入 ring buffer 中取字节。
- 按 `\r`、`\n` 拼装完整命令行。
- 按空格拆分参数。
- 根据命令名查找处理函数。
- 把执行结果回复到命令来源的 transport。
- 对未知命令、参数错误、执行失败给出统一格式回复。

### 5.2 业务模块负责的内容

- 定义本模块命令名。
- 注册本模块命令。
- 校验本模块命令参数。
- 调用本模块现有公共接口完成实际动作。
- 生成与本模块相关的结果文本。

### 5.3 `log` 后续需要补充的内容

- 提供输入 transport 枚举能力。
- 提供按 transport 单播回复能力。

## 6. 命令注册模型

建议 `console` 使用静态注册表，不使用动态内存。

每条命令建议至少包含以下信息：

- `commandName`
- `helpText`
- `handler`
- `ownerTag`

建议的语义如下：

- `commandName`
  - 命令主字，例如 `verinfo`、`ledr`。
- `helpText`
  - 用于后续 `help` 命令或参数错误提示。
- `handler`
  - 命令执行函数，由所属模块提供。
- `ownerTag`
  - 标记命令归属模块，例如 `system`、`drvGpio`，便于排查和打印。

命令处理函数建议后续接收以下上下文信息：

- 来源 transport
- 参数个数 `argc`
- 参数列表 `argv`
- 回复函数或回复上下文

这样业务模块既能知道命令从哪个输入口进入，也不需要知道 `console` 内部 session 结构。

## 7. 注册方式设计

命令注册必须由命令所属模块发起，而不是由 `console` 核心硬编码。

建议模式如下：

- `system` 模块提供类似 `systemConsoleRegister()` 的注册函数。
- `drvgpio` 模块提供类似 `drvGpioDebugConsoleRegister()` 的注册函数。
- `consoleInit()` 完成核心初始化后，由上层初始化流程显式调用这些注册函数。

这样做的好处是：

- `console` 核心不依赖具体驱动头文件。
- 模块是否暴露 console 命令可以通过编译开关独立控制。
- 注册入口留在模块内，命令与业务逻辑天然靠近，维护成本更低。

不建议由 `console.c` 直接写成：

- 如果命令是 `ledr` 就调用 `drvGpioWrite`
- 如果命令是 `verinfo` 就调用 `systemGetFirmwareVersion`

这种写法会让 `console` 逐步膨胀为业务聚合点，耦合度会越来越高。

## 8. 多输入通道设计

### 8.1 结论

当开启 2 个或 3 个 log 输入时，不建议为每个输入单独创建一套不同的命令框架，而是建议：

- 只保留一个 `console` 核心。
- 为每个启用的 input transport 分配一个独立 session。
- 由一个统一的 `consoleProcess()` 轮询所有启用 transport。
- 回复的时候所有开启的输出通道都要收到回复。

这样能满足“分别运行对应的数据处理”，同时避免额外任务、额外栈和重复代码。

### 8.2 每个 session 需要保存的状态

每个 transport session 建议至少保存：

- `transport`
- `inputBuffer`
- `lineBuffer`
- `lineLength`
- `isLineOverflow`
- `lastActivityTick`

其中：

- `inputBuffer` 指向 `log` 暴露的 ring buffer。
- `lineBuffer` 用于缓存当前 transport 正在输入的一整行命令。
- `isLineOverflow` 用于标记单行过长时的丢弃状态。

### 8.3 处理原则

- RTT 输入的数据只进入 RTT 对应 session。
- UART 输入的数据只进入 UART 对应 session。
- 每个 session 独立拼装行、独立解析、独立执行。
- 同一时刻即使多个通道都在发送命令，也不会混成一行。

## 9. 回复路径设计

这是本设计里的关键点。

如果后续命令执行结果直接复用 `LOG_I`、`LOG_W`，在多输入开启时会出现以下问题：

- RTT 输入一条命令，UART 也会收到回复。
- UART 输入一条命令，RTT 也会收到回复。
- 多人调试或多工具连接时，交互信息会串线。

因此，`console` 的命令回复不应直接走“广播式日志输出”，而应支持“按 transport 定向输出”。

建议后续补充以下能力之一：

### 9.1 推荐方案

在 `log` 模块中新增按 transport 单播写接口，例如：

- `logWriteToTransport(transport, buffer, length)`

由 `console` 调用该接口，把命令回复发回命令来源通道。

### 9.2 可接受方案

把 `console` 回复能力单独抽象为 `consoleReplyWrite(transport, ...)`，其内部再映射到 `log` 的 transport hook。

无论选择哪种方式，都必须保证：

- 命令回复默认只回到来源 transport。
- 系统普通日志仍可继续广播到多个输出口。

## 10. 命令解析规则

建议后续采用以下最小规则：

- 一行命令以 `\r`、`\n` 或 `\r\n` 结束。
- 连续空格按一个分隔符处理。
- 第一个 token 为命令名。
- 后续 token 为参数。
- 参数暂不支持引号和转义。

这套规则足够覆盖当前需求：

- `verinfo`
- `ledr 0`
- `ledr 1`

当后续出现复杂字符串参数，再决定是否扩展引号和转义规则。

## 11. 首批命令规划

### 11.1 `verinfo`

归属模块：`system`

建议行为：

- 返回 firmware version
- 返回 hardware version
- 必要时返回当前 system mode

建议调用：

- `systemGetFirmwareVersion()`
- `systemGetHardwareVersion()`

### 11.2 `ledr 0` / `ledr 1`

归属模块：`DrvGpio`

用户要求：

- `ledr 0` 点亮红灯
- `ledr 1` 熄灭红灯

由于当前 GPIO 逻辑层已经把低有效硬件语义归一化，因此 GPIO 命令处理函数内部需要做“用户参数到逻辑状态”的转换，而不是让 `console` 核心知道 LED 的极性语义。

建议处理规则：

- `0` 表示用户期望的 `on`
- `1` 表示用户期望的 `off`
- 具体映射由 `DrvGpio` 命令处理函数决定

建议命令处理函数内部最终调用：

- `drvGpioWrite(DRVGPIO_LEDR, ...)`

如果后续需要扩展，也可以保持同一模式：

- `ledg <0|1>`
- `ledb <0|1>`
- `key read`

## 12. 模块边界建议

### 12.1 `console` 不应该做的事

- 不直接 include `bspgpio.h`
- 不直接 include MCU SDK GPIO 头文件
- 不直接判断 `DRVGPIO_LEDR` 的极性
- 不直接访问驱动内部状态表

### 12.2 `DrvGpio` 命令层应该做的事

- 定义 GPIO 相关命令名。
- 解析 GPIO 命令参数。
- 把用户命令映射成 `drvGpioWrite`、`drvGpioRead`、`drvGpioToggle` 调用。
- 对外暴露一个注册入口给 `console`。

### 12.3 `system` 命令层应该做的事

- 定义版本信息命令。
- 调用 `systemGetFirmwareVersion()`、`systemGetHardwareVersion()`。
- 对外暴露一个注册入口给 `console`。

## 13. 初始化时序建议

建议后续初始化顺序如下：

1. 先初始化底层驱动，例如 RTT、UART、GPIO、system。
2. 初始化 `log`，确保输入输出 transport 可用。
3. 初始化 `console` 核心。
4. 由各模块显式调用自己的命令注册函数。
5. 在任务上下文周期性调用 `consoleProcess()`，或由单独 console task 调用。

这里建议优先采用“一个 console task 或一个周期调用点”的形式，而不是让各驱动自己起任务。

原因是：

- 更容易统一调度。
- 更容易限制栈占用。
- 更容易集中管理所有输入通道。

## 14. 错误处理建议

后续实现时建议统一输出以下几类回复：

- `OK`
- `ERROR: unknown command`
- `ERROR: invalid argument`
- `ERROR: command buffer overflow`
- `ERROR: transport unavailable`

业务模块如果执行失败，也应返回明确错误，而不是静默失败。

## 15. 后续接口建议

为了让设计可以落地，后续代码实现时建议补充以下接口。

### 15.1 `log` 层

- 枚举已启用输入 transport 的接口
- 按 transport 定向写出的接口

### 15.2 `console` 层

- `consoleInit()`
- `consoleProcess()`
- `consoleRegisterCommand()`
- `consoleReply()`

### 15.3 业务模块层

- `systemConsoleRegister()`
- `drvGpioDebugConsoleRegister()`

命令处理函数本身保持在所属模块内部，不暴露多余细节。

## 16. 推荐实现路线

后续真正写代码时，建议按下面顺序推进：

1. 先补 `log` 的 transport 枚举能力与定向输出能力。
2. 再实现 `console` 核心的数据结构、注册表和 session 轮询。
3. 接着在 `system` 中注册 `verinfo`。
4. 再在 `DrvGpio` 中注册 `ledr`。
5. 最后再扩展 `help`、`ledg`、`ledb` 等命令。

这样能保证每一步都可独立验证，并且避免一次性把所有逻辑耦合到一起。

## 17. 最终结论

本项目的 `console` 应定义为“位于 `log` 之上的命令分发层”，而不是 `log` 的一部分，也不是驱动层的一部分。

推荐采用以下总体方案：

- `console` 统一负责命令接收、解析、查找、分发。
- 每个启用的 log 输入 transport 都有独立 session，分别处理自己的命令流。
- 命令回复必须按来源 transport 定向发送，不能直接复用广播式日志输出。
- 业务命令通过注册方式接入。
- GPIO 命令注册入口放在 `DrvGpio`，版本命令注册入口放在 `system`。

这套方案可以满足当前 `verinfo` 和 `ledr 0/1` 的需求，也便于后续扩展更多模块命令，而不会让 `console` 核心逐步演变成一个强耦合的大杂烩。


第一阶段先改异步输出框架。
说明：
把发送职责从 producer task 移到 ConsoleTask，但不一次性大改 API。
改动点：
log.h 增加异步输出配置和处理接口，例如 logProcessOutput、logGetStats。
log.c 增加发送队列、状态机、丢包统计。
systask.c:194-199 在 consoleProcess 后调用 logProcessOutput。
目标：
先把直接写接口的行为收口到 ConsoleTask，一步解决 UART busy 时业务任务直接失败的问题。

第二阶段处理调用者栈占用。
说明：
这是你这次目标里最关键的一步。
推荐做法：
把 log.c:289 这个局部 lBuffer 改成 log 模块内部的共享静态 scratch buffer，再配一个很短的临界区或互斥保护。
效果：
先把固定的 256 字节任务栈占用拿掉，通常这已经能明显降低小栈任务风险。
注意：
这一步并不等于“调用者完全不消耗格式化栈”，因为 vsnprintf 本身仍会有内部栈消耗，但会比现在小很多。

第三阶段决定队列模型。
推荐模型：
每个输出接口一个 2 KB ring。
原因：
RTT 和 UART 的发送节奏不同，独立 ring 最容易保证“不重复、不串线、不因为一条慢链路拖死另一条”。
不推荐模型：
一个共享 2 KB 裸字节 ring。
原因：
多消费者确认逻辑复杂，后面维护成本高。
如果必须共享：
把 ring 中每条日志存成完整 frame，至少包含长度字段，并维护 transport 发送位图和当前 offset。

第四阶段定义日志入队策略。
建议：
日志以“完整一行”为最小单位入队，不要用 overwrite 模式覆盖中间字节。
当前 RTT 输入在 bsp_rtt.c:39-48 用的是 overwrite，这适合输入缓存，不适合日志输出。
输出队列更合理的策略是：
空间不足时丢弃整条新日志。
增加 droppedLines 和 droppedBytes 统计。
必要时由 ConsoleTask 后续补发一条 dropped summary。
这样即使丢，也不会把多条日志拼坏。

第五阶段定义 ConsoleTask 的发送节流。
建议：
每轮循环按 transport 轮询，每个 transport 每轮只发固定预算，比如 64 到 256 字节。
UART 如果 busy，就保留当前发送位置，下轮重试。
RTT 如果短写，就按实际写入字节推进。
这样可以避免 log flush 抢占 console 命令处理时间，仍然保持 systask.h:31 这类周期任务的可控执行时间。

第六阶段梳理 transport 层职责。
建议：
底层 transport 的 write 继续保持“尽力发送多少就返回多少”的语义，不再让上层假设一次写完。
要重点适配：
drvuart_port.c:64-74
bsp_rtt.c:62-69
尤其 UART 现在是 DMA busy 就直接返回 0，这正适合改成“后台重试”的消费者模型。