# MCU RingBuffer 架构设计

## 1. 设计目标

本文档定义一套适用于 MCU 项目的可复用环形缓冲区架构。设计重点面向嵌入式系统中的确定性行为、有界延迟、低内存开销以及清晰的并发边界。

核心目标如下：

- 安全：不允许隐式溢出，不允许所有权不清晰，错误行为必须显式可控。
- 高效：读写路径为 O(1)，运行时热路径不使用动态内存，尽量降低分支和取模成本。
- 可移植：不依赖具体 MCU、RTOS 或编译器特性。
- 可扩展：既支持字节流，也支持定长元素队列，并可兼容 ISR 与任务混合场景。
- 可测试：核心逻辑可脱离硬件，在主机环境完成边界和压力验证。

非目标如下：

- 基础版本不默认支持多生产者多消费者。
- 热路径不引入堆分配。
- 核心模块内部不实现阻塞等待或调度语义。

## 2. RingBuffer 文件夹定位

建议将 `RingBuffer` 目录视为一个独立的基础组件模块。

推荐后续文件划分：

- `ringbuffer_architecture.md`：架构设计与 API 合同。
- `ringbuffer.h`：对外公开接口与配置类型。
- `ringbuffer.c`：核心实现。
- `ringbuffer_port.h`：可选的平台适配钩子，例如临界区和内存屏障。


这样划分的好处是公共接口稳定、核心实现清晰、平台适配最小化且边界明确。

## 3. 总体架构分层

建议将环形缓冲区分为三层。

### 3.1 Core Layer

职责：

- 维护 head 与 tail 索引
- 计算已用空间与剩余空间
- 执行 push、pop、peek、discard 等基础操作
- 做边界检查并返回状态码

特性：

- 纯数据结构逻辑
- 不依赖 RTOS
- 不直接关中断
- 不在热路径中打印日志

### 3.2 Concurrency Policy Layer

职责：

- 定义实例是 SPSC、任务独占还是受保护共享模式
- 决定调用者是否需要在访问前进入临界区
- 提供可选的内存屏障或临界区钩子

特性：

- 对核心层做轻封装
- 把并发规则从数据结构逻辑中分离出去
- 避免对所有场景都强制施加高成本同步

### 3.3 Integration Layer

职责：

- 将 ring buffer 绑定到 UART、SPI、日志、协议解析或 DMA 流程
- 将模块返回值映射到系统级错误码
- 组织 ISR 生产者与任务消费者，或反向模式

特性：

- 可以包含事件通知、信号量唤醒等机制
- 不应绕过 ring buffer API 直接修改索引

## 4. 支持的使用模式

架构建议显式支持以下模式。

### 4.1 SPSC 字节流模式

单生产者、单消费者。典型场景是 UART RX 中断写入字节，任务上下文批量读取。

特点：

- 效率最高
- 在索引所有权严格划分时可做到无锁
- 建议作为默认模式

### 4.2 SPSC 定长元素队列模式

同样是单生产者、单消费者，但每个元素大小固定。

适用对象：

- 消息描述符
- 传感器样本
- 命令令牌

该模式可复用同一套索引引擎，但应对外暴露面向元素的 API。

### 4.3 外部保护共享模式

允许多个任务，或 ISR 加多个任务访问同一实例，但同步保护不放在核心层内部，而由上层显式处理。

特点：

- 仅在调用方明确串行化的前提下支持
- 推荐使用短临界区，或在非 ISR 场景使用互斥保护
- 核心模块应明确文档约束，而不是伪装成默认线程安全1

## 5. 核心数据模型

建议控制块如下：

```c
typedef struct stRingBuffer {
    uint8_t *buffer;
    uint32_t capacity;
    volatile uint32_t head;
    volatile uint32_t tail;
    uint32_t mask;
    uint8_t isPowerOfTwo;
} stRingBuffer;
```

字段语义：

- `buffer`：调用者提供的底层存储区。
- `capacity`：逻辑容量，单位可为字节或元素。
- `head`：下一个写位置对应的逻辑索引。
- `tail`：下一个读位置对应的逻辑索引。
- `mask`：容量为 2 的幂时用于快速回绕。
- `isPowerOfTwo`：标记使用快速路径还是通用取模路径。

设计建议：

- `head` 和 `tail` 应采用单调递增无符号计数器，而不是始终限制在 `[0, capacity)`。
- 物理数组下标通过 `index & mask` 或 `index % capacity` 派生得到。
- 单调计数器可以简化 used/free 计算，并降低满/空歧义。

## 6. 满与空的判定策略

建议采用单调计数器差值法：

- `used = head - tail`
- `free = capacity - used`

约束条件：

- 仅当 `used <= capacity` 时定义有效
- 模块必须阻止任何导致 `used > capacity` 的写入行为

相比“预留一个空槽”的传统做法，这种设计更适合基础设施组件，因为：

- 容量语义更清晰
- 可以真正使用满额容量
- 边界检查更容易显式化

为保证算术安全，建议：

- 使用与平台宽度匹配的无符号整数类型，优先 `uint32_t`
- 保证生产者与消费者索引差永不超过 `capacity`
- 初始化时拒绝 `capacity == 0`
- 在通用约束中建议 `capacity <= UINT32_MAX / 2`

## 7. 内存布局与效率策略

### 7.1 存储区由调用者持有

底层 buffer 应由调用者显式提供。

常见形式：

- 长生命周期驱动使用静态全局数组
- 子系统内部使用文件作用域静态数组
- 与 RTOS 集成时使用静态区域

优势：

- 无碎片风险
- 启动后内存分布清晰可见
- 生命周期可控

### 7.2 2 的幂容量优化

当容量为 2 的幂时，物理索引可使用：

```c
physicalIndex = logicalIndex & (capacity - 1U);
```

这可以避免小 MCU 上高成本的取模运算。

推荐策略：

- 通用 API 支持任意容量
- 当检测到容量为 2 的幂时自动走快速路径
- 在文档中建议高吞吐场景优先选择 2 的幂容量

### 7.3 单字节与批量接口并存

模块应同时提供单字节和批量读写接口。

原因：

- 单字节接口适合 ISR 使用
- 批量接口可减少任务上下文中的函数调用开销
- 批量接口更容易一次处理跨回绕的两段内存

推荐内部拷贝流程：

1. 计算到缓冲区尾部的线性连续区长度
2. 复制第一段
3. 若发生回绕，再复制第二段
4. 复制完成后再提交索引

## 8. 安全性要求

### 8.1 初始化校验

初始化接口应拒绝以下输入：

- 控制块为空
- 存储区指针为空
- 容量为 0
- 容量超出算术约束上界
- 定长元素模式下，元素尺寸或模式配置不一致

### 8.2 边界保护

模块必须保证以下行为永不发生：

- 写越界到底层存储区之外
- 参数校验失败后仍推进 `head`
- 在可读数据不足时推进 `tail`

所有基于长度的操作都应显式裁剪或显式失败。只有当 API 合同明确声明支持部分传输时，才允许返回部分完成。

### 8.3 溢出策略必须显式区分

不要把“覆盖旧数据”的行为隐藏在普通写接口中。

建议写策略如下：

- `RINGBUFFER_WRITE_ALL_OR_FAIL`：空间不足则失败
- `RINGBUFFER_WRITE_PARTIAL`：尽可能写入并返回实际长度
- `RINGBUFFER_WRITE_OVERWRITE_OLDEST`：写前主动推进 `tail`，丢弃最旧数据

覆盖模式应独立成策略或专用 API，因为它会改变数据丢失语义。

### 8.4 中断上下文约束

文档与 API 必须明确说明：

- 哪些接口可在 ISR 中调用
- 哪些接口会执行多字节拷贝，从而拉长 ISR 时延
- 哪些接口需要外部临界区保护

推荐默认约束：

- ISR 安全：单字节 push、单字节 pop、以及在模型允许时的状态查询
- 任务上下文优先：批量 copy、reset、强制 discard、overwrite 辅助接口

## 9. 并发模型

### 9.1 默认并发合同

基础 ring buffer 实例应默认定义为 SPSC。

所有权规则：

- 生产者只负责推进 `head`
- 消费者只负责推进 `tail`
- 双方都可以读取两个索引的当前值

这是在 MCU 场景中兼顾可证明正确性和执行效率的最佳默认模型。

### 9.2 内存可见性与发布顺序

很多 MCU 上，自然对齐的 32 位读写已经足够接近原子，但架构文档仍应显式约束访问顺序。

必须满足：

- 生产者先写数据，再发布新的 `head`
- 消费者观测到新的 `head` 后再读取数据
- 消费者消费完成后再推进 `tail`

若目标 CPU 或编译器需要，port 层可提供：

- 编译器屏障
- 数据内存屏障
- 临界区进入与退出钩子

核心模块不应硬编码具体架构相关的屏障指令。

### 9.3 何时需要临界区

仅在以下场景建议使用临界区或等效同步：

- 存在多个生产者
- 存在多个消费者
- reset 或重配置与正常收发并发发生
- DMA 与 CPU 共同更新同一边界，而没有更强的同步机制

临界区应尽量只包裹最小元数据更新区，而不是整段大拷贝过程。

## 10. API 合同建议

建议状态码定义如下：

```c
typedef enum eRingBufferStatus {
    RINGBUFFER_OK = 0,
    RINGBUFFER_ERROR_PARAM,
    RINGBUFFER_ERROR_EMPTY,
    RINGBUFFER_ERROR_FULL,
    RINGBUFFER_ERROR_NO_SPACE,
    RINGBUFFER_ERROR_STATE
} eRingBufferStatus;
```

建议公开接口如下：

```c
eRingBufferStatus ringBufferInit(stRingBuffer *rb, uint8_t *storage, uint32_t capacity);
eRingBufferStatus ringBufferReset(stRingBuffer *rb);

uint32_t ringBufferGetUsed(const stRingBuffer *rb);
uint32_t ringBufferGetFree(const stRingBuffer *rb);
uint32_t ringBufferGetCapacity(const stRingBuffer *rb);
uint8_t ringBufferIsEmpty(const stRingBuffer *rb);
uint8_t ringBufferIsFull(const stRingBuffer *rb);

eRingBufferStatus ringBufferPushByte(stRingBuffer *rb, uint8_t data);
eRingBufferStatus ringBufferPopByte(stRingBuffer *rb, uint8_t *data);
eRingBufferStatus ringBufferPeekByte(const stRingBuffer *rb, uint8_t *data);

uint32_t ringBufferWrite(stRingBuffer *rb, const uint8_t *src, uint32_t length);
uint32_t ringBufferRead(stRingBuffer *rb, uint8_t *dst, uint32_t length);
uint32_t ringBufferPeek(const stRingBuffer *rb, uint8_t *dst, uint32_t length);
uint32_t ringBufferDiscard(stRingBuffer *rb, uint32_t length);

uint32_t ringBufferWriteOverwrite(stRingBuffer *rb, const uint8_t *src, uint32_t length);
```

接口约束建议：

- 单字节接口返回明确状态码
- 批量接口返回实际传输长度
- 不修改数据的接口尽量使用 `const` 指针
- `reset` 应标注为任务上下文接口，除非调用者能保证独占访问

## 11. 内部函数划分建议

为保证实现易审计、易测试、易维护，建议将内部逻辑拆为若干小函数。

推荐私有辅助函数：

- `ringBufferIsPowerOfTwo()`
- `ringBufferToPhysicalIndex()`
- `ringBufferGetUsedInternal()`
- `ringBufferGetLinearWriteSpan()`
- `ringBufferGetLinearReadSpan()`
- `ringBufferCopyIn()`
- `ringBufferCopyOut()`

这样做有利于隔离复杂算术逻辑，也更适合后续做单元测试与代码审查。

## 12. 错误处理与诊断策略

核心模块在正常路径中不应打印日志。热路径中的日志会破坏确定性，并可能直接破坏 ISR 安全性。

推荐诊断策略：

- 使用显式状态码
- 仅在调试构建中暴露诊断计数器
- 断言只用于保护编程假设，不替代运行时错误处理

可选调试计数器包括：

- writeRejectCount
- overwriteCount
- underflowCount
- maxUsedWatermark

若目标平台内存紧张，这些计数器应可在 release 配置中裁剪掉。

## 13. DMA 与零拷贝扩展路径

基础架构建议保持“拷贝式接口”为主，但要为 DMA 或零拷贝模式预留干净扩展点。

建议未来扩展接口：

- 获取可写线性区
- 提交已生产长度
- 获取可读线性区
- 提交已消费长度

设计意图示例如下：

```c
eRingBufferStatus ringBufferAcquireWriteRegion(stRingBuffer *rb, uint8_t **ptr, uint32_t *length);
eRingBufferStatus ringBufferCommitWrite(stRingBuffer *rb, uint32_t produced);
eRingBufferStatus ringBufferAcquireReadRegion(stRingBuffer *rb, uint8_t **ptr, uint32_t *length);
eRingBufferStatus ringBufferCommitRead(stRingBuffer *rb, uint32_t consumed);
```

该扩展适合：

- UART DMA RX idle-line 场景
- 网络帧暂存
- 音频采样流处理

建议将其与基础 API 分离，以保证基础版本足够简单，不易误用。

## 14. 推荐集成模式

### 14.1 UART RX ISR 到任务解析

推荐模式：

- ISR 将接收到的字节写入 SPSC ring buffer
- 任务定期或被事件唤醒后批量读取
- 协议解析仅在任务上下文执行

优势：

- ISR 路径短
- 共享状态少
- 复杂解析逻辑被隔离在中断外

### 14.2 任务生产到 DMA 或 UART TX 消费

推荐模式：

- 任务将待发送数据写入 TX ring buffer
- 驱动在硬件空闲时取一段线性可读区发送
- ISR 或 DMA 完成回调负责更新发送状态并触发下一段

这种方式可以减少忙等待，并保持发送流水持续性。

### 14.3 日志缓冲区

若用于日志：

- 只有在允许丢弃旧日志时才建议使用 overwrite-oldest
- 格式化过程应放在 ISR 之外
- 在严重故障路径中不要无条件做大块同步 flush，除非系统架构明确支持

## 15. 安全与稳健性考虑

对 MCU 基础组件来说，安全的重点不是传统网络安全语义，而是内存安全、失败行为可预测、以及对错误调用输入的抗误用能力。

必须遵守的稳健性原则：

- 所有外部传入长度在 copy 前都必须校验
- 不隐式信任调用者的所有权假设
- reset 与重配置必须与正常数据流分离
- 明确约定 `length == 0` 时是否允许 `dst == NULL`
- 明确部分传输的语义，不留歧义

需要重点防止的误用包括：

- ISR 活跃期间调用 reset
- 多个生产者直接共享同一实例但不加保护
- 上层假设写接口一定全量成功
- 把内部索引暴露给高层直接操作

## 16. 验证策略

实现阶段应采用确定性测试进行验证。

最低建议覆盖：

- 非法参数初始化
- 空读与满写边界
- 正好写满到容量上限
- 跨回绕写入与读取
- partial write 与 partial read 行为
- overwrite-oldest 的正确性
- 若启用 watermark，验证峰值统计
- 长时间单调计数器压力测试
- 生产者消费者交错执行的 SPSC 仿真

在板级联调时，还建议验证：

- 高波特率 UART RX 中断持续灌入时的稳定性
- 回绕边界无数据损坏
- 当消费者持续慢于生产者时，系统行为仍可预测

## 17. 最终推荐方案

结合当前 MCU 项目特征，建议采用以下基线架构：

1. 一个通用的字节型 SPSC ring buffer 核心
2. 底层存储区由调用者静态提供
3. 容量支持任意值，但优先推荐 2 的幂优化
4. fail、partial、overwrite 三类写策略显式分离
5. ISR 场景使用单字节接口，任务场景使用批量接口
6. 临界区与内存屏障通过可选 port 钩子提供，避免污染核心实现

这套架构对 MCU 软件是较均衡的方案：核心足够小，行为可预测，边界清晰，安全性可审计，同时又保留了后续向 DMA、零拷贝和更高层通信模块扩展的空间。