# RTOS 服务移植规范

本文档的目标不是解释“为什么要抽象 RTOS”，而是直接说明在当前仓库里，怎样写出一份合格的 `User/port/rtos_port.c`。

如果读者照着本文实现，应该能够完成以下事情：

- 让 `rep/` 和 `User/` 上层代码不直接依赖具体 RTOS 头文件
- 为当前项目提供统一的延时、时钟、互斥锁、队列、任务创建等能力
- 在切换到新的 RTOS 时，把改动限制在 `User/port/rtos_port.*`

## 1. 模块位置与职责

当前仓库中，RTOS 相关代码分成两层：

- `rep/service/rtos`
  - 公共层
  - 定义统一类型、统一状态码、统一 API
  - 不允许直接调用 FreeRTOS、CMSIS-RTOS、uC/OS 等原生接口
- `User/port/rtos_port.*`
  - 项目绑定层
  - 负责包含具体 RTOS 头文件
  - 负责把具体 RTOS 能力翻译成 `rep` 层需要的统一接口

边界要求如下：

- `rep/` 下只能面向 `repRtos*` 这组公共接口编程
- `User/system`、`User/manager`、`User/bsp` 也应优先使用 `repRtos*`
- 只有 `User/port/rtos_port.*` 可以直接出现具体 RTOS 头文件和原生 API

## 2. 调用关系

上层业务代码的调用链必须保持为：

1. `app` 或业务模块调用 `repRtos*`
2. `rep/service/rtos/rtos.c` 从 provider 取到 `stRepRtosOps`
3. `User/port/rtos_port.c` 提供这张 `ops` 表
4. `ops` 表里的函数再去调用具体 RTOS 原生接口

也就是说：

- 上层不直接调用 `xTaskCreate`、`vTaskDelay`、`xSemaphoreTake`
- 上层也不直接 include `FreeRTOS.h`、`cmsis_os.h`、`ucos_ii.h`
- 上层只知道 `repRtosTaskCreate()`、`repRtosDelayMs()` 这类统一接口

## 3. `rtos_port.c` 必须提供什么

项目侧必须提供一张长期有效的静态操作表，以及对应的获取入口。

最基本的形式如下：

```c
static const stRepRtosOps gRepRtosOps = {
    .getSchedulerState = xxx,
    .delayMs = xxx,
    .getTickMs = xxx,
    .yield = xxx,
    .enterCritical = xxx,
    .exitCritical = xxx,
    .mutexCreate = xxx,
    .mutexTake = xxx,
    .mutexGive = xxx,
    .queueCreate = xxx,
    .queueSend = xxx,
    .queueReceive = xxx,
    .queueReset = xxx,
    .taskCreate = xxx,
    .taskDelayUntilMs = xxx,
    .statsInit = xxx,
};

const stRepRtosOps *repRtosProviderGetOps(void)
{
    return &gRepRtosOps;
}
```

这里有两个硬性要求：

- `gRepRtosOps` 必须是静态长期有效对象，不能返回临时变量地址
- `stRepRtosOps` 中每一个成员都必须显式处理，不能靠“没填就是不支持”

如果某项能力当前不支持，应该提供一个函数并返回 `REP_RTOS_STATUS_UNSUPPORTED`，而不是把函数指针留空。

## 4. 公共状态码的使用规则

`eRepRtosStatus` 不是随便返回的，移植时要统一语义。

- `REP_RTOS_STATUS_OK`
  - 调用成功
- `REP_RTOS_STATUS_INVALID_PARAM`
  - 传入参数非法
  - 或对象状态不合法，例如锁还没创建就去 take
- `REP_RTOS_STATUS_NOT_READY`
  - provider 本身不存在
  - 或公共层还拿不到完整的 `ops` 表
- `REP_RTOS_STATUS_BUSY`
  - 非阻塞调用没有立刻成功
  - 例如超时参数是 `0`，但队列已满、锁已被占用
- `REP_RTOS_STATUS_UNSUPPORTED`
  - 当前 RTOS 或当前 port 明确不支持该能力
- `REP_RTOS_STATUS_TIMEOUT`
  - 阻塞等待超时
- `REP_RTOS_STATUS_ERROR`
  - 原生接口已经真正被调用，但返回了异常失败

移植时不要混淆这几个状态，尤其要区分：

- `BUSY` 和 `TIMEOUT`
- `UNSUPPORTED` 和 `ERROR`
- `INVALID_PARAM` 和 `NOT_READY`

## 5. 调度器状态的统一语义

`eRepRtosSchedulerState` 的语义如下：

- `REP_RTOS_SCHEDULER_STOPPED`
  - 内核未启动
- `REP_RTOS_SCHEDULER_READY`
  - 内核已初始化，但还没开始调度
- `REP_RTOS_SCHEDULER_RUNNING`
  - 调度器正在运行
- `REP_RTOS_SCHEDULER_LOCKED`
  - 内核运行中，但调度被锁住
- `REP_RTOS_SCHEDULER_SUSPENDED`
  - 调度被挂起
- `REP_RTOS_SCHEDULER_UNKNOWN`
  - 当前无法判断

如果目标 RTOS 没有完全对应的状态枚举，可以按最接近语义映射；不要为了“凑全状态”而编造不真实的状态。

## 6. `stRepRtosTaskConfig` 的约束

当前仓库里，任务配置结构的字段含义必须按下面理解：

- `name`
  - 必填
  - 任务名
- `entry`
  - 必填
  - 任务入口函数
- `argument`
  - 选填
  - 入口参数
- `stackBuffer`
  - 目前保留给将来的静态任务支持
  - 当前仓库里的 port 如果不支持静态栈，必须明确拒绝非 `NULL`
- `stackSize`
  - 必填
  - 单位不是字节
  - 单位是 `repRtosStackType` 的个数，也就是“栈深度”
- `priority`
  - 任务优先级
- `handle`
  - 可选输出
  - 用来把创建出的原生任务句柄回传给上层

这里最容易写错的是 `stackSize`：

- 上层如果以字节为起点，必须先换算成栈深度再传进来
- port 层不要把它当字节再次换算

## 7. 每个 `ops` 函数应该怎么写

这一节是本文最关键的部分。写 port 文件时，应该逐项对照实现。

### 7.1 `getSchedulerState`

职责：

- 查询当前调度器状态

要求：

- 必须返回统一后的 `eRepRtosSchedulerState`
- 如果 RTOS 原生状态不足以精确映射，返回最接近状态
- 实在无法判断时返回 `REP_RTOS_SCHEDULER_UNKNOWN`

### 7.2 `delayMs`

职责：

- 统一毫秒延时

要求：

- 调度器运行时，优先使用 RTOS 自身延时能力
- 调度器未运行时，可以退化到 `HAL_Delay()` 或等价裸机延时
- `delayMs(0)` 不能变成“完全不让出执行权”
- 只要是非零毫秒等待，就不能因为 tick 取整而退化成 `0 tick`

也就是说：

- `1 ms` 在低 tick 频率系统里，允许被向上取整成 `1 tick`
- 不允许被错误变成“不等待”

### 7.3 `getTickMs`

职责：

- 返回统一的毫秒时基

要求：

- 调度器运行时，优先返回 RTOS 时基对应的毫秒值
- 调度器未运行时，可以退化到 `HAL_GetTick()` 或等价硬件毫秒计数
- 返回值语义必须始终保持“毫秒”

### 7.4 `yield`

职责：

- 主动让出执行权

要求：

- 调度器运行时，调用原生 yield
- 调度器未运行时，可以直接空操作

### 7.5 `enterCritical` / `exitCritical`

职责：

- 进入和退出临界区

要求：

- 调度器运行时，优先使用 RTOS 原生临界区接口
- 调度器未运行时，退化到 MCU 中断屏蔽或等价裸机方案
- 如果裸机方案支持嵌套，必须保证 enter/exit 成对恢复

### 7.6 `mutexCreate`

职责：

- 创建互斥锁

要求：

- 对 `NULL` 参数返回 `INVALID_PARAM`
- 对已经创建过的对象，当前仓库允许直接返回 `OK`
- 如果当前 RTOS 不支持该能力，应明确返回 `UNSUPPORTED`

### 7.7 `mutexTake`

职责：

- 获取互斥锁

要求：

- 目标对象未创建时返回 `INVALID_PARAM`
- 非阻塞失败时返回 `BUSY`
- 阻塞超时失败时返回 `TIMEOUT`
- 原生接口异常失败时返回 `ERROR`

### 7.8 `mutexGive`

职责：

- 释放互斥锁

要求：

- 对未创建对象返回 `INVALID_PARAM`
- 原生释放失败时返回 `ERROR`

### 7.9 `queueCreate`

职责：

- 创建队列

要求：

- `queue == NULL`、`itemSize == 0`、`capacity == 0` 都应视为非法参数
- 如果已创建，可按当前仓库策略直接返回 `OK`
- 如果 RTOS 不支持，返回 `UNSUPPORTED`

### 7.10 `queueSend`

职责：

- 发送一个队列元素

要求：

- `queue`、`item` 非法时返回 `INVALID_PARAM`
- 非阻塞发送失败返回 `BUSY`
- 有限时等待失败返回 `TIMEOUT`

### 7.11 `queueReceive`

职责：

- 接收一个队列元素

要求：

- 与 `queueSend` 的状态语义保持一致

### 7.12 `queueReset`

职责：

- 清空或重置队列

要求：

- 对未创建队列返回 `INVALID_PARAM`
- 不支持时返回 `UNSUPPORTED`

### 7.13 `taskCreate`

职责：

- 创建任务

要求：

- `config == NULL`、`entry == NULL`、`name == NULL`、`stackSize == 0` 都必须视为非法参数
- 如果当前 port 不支持静态栈，而 `stackBuffer != NULL`，必须明确返回 `UNSUPPORTED`
- 如果 `handle` 非空且上层已经保存了有效句柄，当前仓库允许把这次创建视为幂等请求并直接返回 `OK`
- 真正创建失败时返回 `ERROR`

### 7.14 `taskDelayUntilMs`

职责：

- 提供周期性任务延时

要求：

- `lastWakeTimeMs == NULL` 时返回 `INVALID_PARAM`
- 调度器运行时，优先映射到 RTOS 原生的“按周期唤醒”能力
- 调度器未运行时，允许退化成普通 delay，再回写新的毫秒时基
- 非零周期等待同样不能因为 tick 换算变成 `0 tick`

### 7.15 `statsInit`

职责：

- 初始化运行时统计功能，或者显式声明当前不支持

要求：

- 这项函数必须存在于 `ops` 表中
- 如果当前系统没有统计功能，返回 `REP_RTOS_STATUS_UNSUPPORTED`
- 不要把函数指针留空

## 8. 关于 `WAIT_FOREVER`

`REP_RTOS_WAIT_FOREVER` 的语义是“无限等待”。

port 层在映射时要注意：

- 如果目标 RTOS 有原生无限等待值，直接映射到原生值
- 如果目标 RTOS 没有等价语义，就要在文档里明确说明，并做出一致处理

不要出现以下情况：

- 上层传了 `WAIT_FOREVER`
- 底层却被错误映射成 `0`
- 最终变成一次非阻塞调用

## 9. 调度器未启动时允许怎样退化

当前仓库允许部分 API 在调度器未启动时使用裸机回退行为，但必须有边界。

通常允许退化的能力：

- `delayMs`
- `getTickMs`
- `enterCritical`
- `exitCritical`

通常不建议静默退化的能力：

- `queueCreate`
- `queueSend`
- `queueReceive`
- `taskCreate`

对于这些对象类能力，要么：

- 明确支持一套可维护的退化逻辑

要么：

- 明确返回 `UNSUPPORTED`

不要既不支持，又悄悄做出半成品行为。

## 10. port 文件最小骨架示例

下面给出一份最小骨架，读者可以从这里起步：

```c
#include "rtos_port.h"

static eRepRtosSchedulerState rtosPortGetSchedulerStateImpl(void)
{
    return REP_RTOS_SCHEDULER_UNKNOWN;
}

static eRepRtosStatus rtosPortDelayMsImpl(uint32_t delayMs)
{
    (void)delayMs;
    return REP_RTOS_STATUS_UNSUPPORTED;
}

static uint32_t rtosPortGetTickMsImpl(void)
{
    return 0U;
}

static void rtosPortYieldImpl(void)
{
}

static void rtosPortEnterCriticalImpl(void)
{
}

static void rtosPortExitCriticalImpl(void)
{
}

static eRepRtosStatus rtosPortMutexCreateImpl(stRepRtosMutex *mutex)
{
    (void)mutex;
    return REP_RTOS_STATUS_UNSUPPORTED;
}

static eRepRtosStatus rtosPortMutexTakeImpl(stRepRtosMutex *mutex, uint32_t timeoutMs)
{
    (void)mutex;
    (void)timeoutMs;
    return REP_RTOS_STATUS_UNSUPPORTED;
}

static eRepRtosStatus rtosPortMutexGiveImpl(stRepRtosMutex *mutex)
{
    (void)mutex;
    return REP_RTOS_STATUS_UNSUPPORTED;
}

static eRepRtosStatus rtosPortQueueCreateImpl(stRepRtosQueue *queue, uint32_t itemSize, uint32_t capacity)
{
    (void)queue;
    (void)itemSize;
    (void)capacity;
    return REP_RTOS_STATUS_UNSUPPORTED;
}

static eRepRtosStatus rtosPortQueueSendImpl(stRepRtosQueue *queue, const void *item, uint32_t timeoutMs)
{
    (void)queue;
    (void)item;
    (void)timeoutMs;
    return REP_RTOS_STATUS_UNSUPPORTED;
}

static eRepRtosStatus rtosPortQueueReceiveImpl(stRepRtosQueue *queue, void *item, uint32_t timeoutMs)
{
    (void)queue;
    (void)item;
    (void)timeoutMs;
    return REP_RTOS_STATUS_UNSUPPORTED;
}

static eRepRtosStatus rtosPortQueueResetImpl(stRepRtosQueue *queue)
{
    (void)queue;
    return REP_RTOS_STATUS_UNSUPPORTED;
}

static eRepRtosStatus rtosPortTaskCreateImpl(const stRepRtosTaskConfig *config)
{
    (void)config;
    return REP_RTOS_STATUS_UNSUPPORTED;
}

static eRepRtosStatus rtosPortTaskDelayUntilMsImpl(uint32_t *lastWakeTimeMs, uint32_t periodMs)
{
    (void)lastWakeTimeMs;
    (void)periodMs;
    return REP_RTOS_STATUS_UNSUPPORTED;
}

static eRepRtosStatus rtosPortStatsInitImpl(void)
{
    return REP_RTOS_STATUS_UNSUPPORTED;
}

static const stRepRtosOps gRepRtosOps = {
    .getSchedulerState = rtosPortGetSchedulerStateImpl,
    .delayMs = rtosPortDelayMsImpl,
    .getTickMs = rtosPortGetTickMsImpl,
    .yield = rtosPortYieldImpl,
    .enterCritical = rtosPortEnterCriticalImpl,
    .exitCritical = rtosPortExitCriticalImpl,
    .mutexCreate = rtosPortMutexCreateImpl,
    .mutexTake = rtosPortMutexTakeImpl,
    .mutexGive = rtosPortMutexGiveImpl,
    .queueCreate = rtosPortQueueCreateImpl,
    .queueSend = rtosPortQueueSendImpl,
    .queueReceive = rtosPortQueueReceiveImpl,
    .queueReset = rtosPortQueueResetImpl,
    .taskCreate = rtosPortTaskCreateImpl,
    .taskDelayUntilMs = rtosPortTaskDelayUntilMsImpl,
    .statsInit = rtosPortStatsInitImpl,
};

const stRepRtosOps *repRtosProviderGetOps(void)
{
    return &gRepRtosOps;
}
```

## 11. 写完 port 之后必须自查什么

完成一份新的 `rtos_port.c` 后，至少检查以下事项：

- `stRepRtosOps` 的所有成员是否都已填充
- 不支持的能力是否明确返回了 `UNSUPPORTED`
- `stackSize` 是否按“栈深度”处理，而不是按字节处理
- `stackBuffer` 的行为是否明确
- `WAIT_FOREVER` 是否正确映射
- 非零毫秒等待是否可能被错误退化成 `0 tick`
- 调度器未启动时，`delay/tick/critical` 的退化行为是否清晰
- `taskCreate` 对非法参数、重复创建、静态栈不支持等情况是否语义明确

## 12. 何时需要改公共接口

新增 RTOS 时，优先只修改 `User/port/rtos_port.*`。

只有在以下情况，才应该去改 `rep/service/rtos` 公共层：

- 新需求确实是公共能力，而不是某个 RTOS 的私有特性
- 现有 `stRepRtosOps` 无法表达该能力
- 能为这项能力定义清晰、稳定、可跨 RTOS 的统一语义

如果只是某个 RTOS 的特有函数，不要直接塞进公共层。
