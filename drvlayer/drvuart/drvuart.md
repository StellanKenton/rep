# DrvUart 模块设计说明

本文档说明 `drvuart` 模块当前的真实分层方式，重点不是“怎么生成一个 UART 文件”，而是明确 `bsp_uart.c/.h` 需要怎样实现，才能满足 `drvuart.c` 的数据路径与接口契约。

## 1. 模块定位

`drvuart` 提供一个带公共接收缓存的 UART 驱动层。它不是简单把 BSP 发收函数透传给上层，而是额外做了两件事:

- 维护每个逻辑 UART 的公共层 ring buffer。
- 把 BSP 原始接收缓冲里的数据同步到公共层 ring buffer。

因此这个模块的 BSP 设计重点不只是“能不能收发”，还包括“BSP 的接收数据如何被公共层稳定搬运”。

## 2. 目录内文件职责

- `drvuart.h`: 定义 BSP 钩子表、公共 API、日志辅助接口。
- `drvuart.c`: 负责参数校验、初始化流程、接收同步和公共 ring buffer 读写。
- `drvuart_port.h`: 定义逻辑 UART 枚举、公共缓存容量宏和功能开关宏。
- `drvuart_port.c`: 绑定 BSP 钩子，并准备公共层 ring buffer 存储区。
- `drvuart_debug.c/.h`: 可选调试或 console 子模块。
- `bsp_uart.c/.h`: 负责具体 UART、GPIO、DMA 和中断实现。

## 3. 当前模块的两层接收缓冲模型

`drvuart` 当前不是单缓冲模型，而是两层缓冲:

1. BSP 自己维护原始接收缓冲，通常是 DMA 环形缓冲或中断缓存。
2. 公共层再把 BSP 中待消费的数据搬到自己的 ring buffer。

数据流如下:

1. BSP 接收硬件数据。
2. `bspUartGetDataLen()` 告诉公共层当前有多少原始数据可读。
3. `bspUartReceive()` 从 BSP 原始缓冲中取走指定字节数。
4. `drvUartSyncRxData()` 把这批数据写入公共层 ring buffer。
5. 上层通过 `drvUartReceive()`、`drvUartGetDataLen()` 或 `drvUartGetRingBuffer()` 访问公共层缓冲。

因此 BSP 绝不能直接操作 `ringbuffer` 模块，也不能绕过公共层对上暴露自己的 DMA 缓冲。

## 4. core 层依赖的 BSP 接口

`drvuart.c` 实际依赖下面这组接口:

```c
typedef struct stDrvUartBspInterface {
    drvUartBspInitFunc init;
    drvUartBspTransmitFunc transmit;
    drvUartBspTransmitItFunc transmitIt;
    drvUartBspTransmitDmaFunc transmitDma;
    drvUartBspGetDataLenFunc getDataLen;
    drvUartBspReceiveFunc receive;
    uint8_t *Buffer;
} stDrvUartBspInterface;
```

其中:

- `init`、`transmit`、`getDataLen`、`receive` 是当前初始化必需钩子。
- `transmitIt` 和 `transmitDma` 是可选钩子，缺失时公共层返回 `DRV_STATUS_UNSUPPORTED`。
- `Buffer` 不是 BSP DMA 缓冲，而是公共层 ring buffer 的底层存储区。

## 5. `bsp_uart.c` 必须满足的契约

### 5.1 `bspUartInit(eDrvUartPortMap uart)`

职责:

- 初始化对应逻辑 UART 的外设、GPIO、DMA 和中断。
- 让后续轮询发送、接收统计和接收取数可用。

实现要求:

- 对无效逻辑 UART 返回明确错误。
- 如果使用 DMA 接收，初始化时就应让 DMA 接收通道进入可工作状态。
- 重复初始化行为必须可预期。

### 5.2 `bspUartTransmit(eDrvUartPortMap uart, const uint8_t *buffer, uint16_t length, uint32_t timeoutMs)`

职责:

- 以同步方式发送数据。

实现要求:

- 需要真实处理 `timeoutMs`。
- 若 DMA 发送正在占用硬件，而当前实现不能并发，则返回明确忙状态。
- 不能假设调用方总是提前做过所有检查。

### 5.3 `bspUartTransmitIt(eDrvUartPortMap uart, const uint8_t *buffer, uint16_t length)`

职责:

- 启动中断发送。

实现要求:

- 如果当前项目不需要该能力，可以不绑定这个钩子。
- 如果绑定了，就必须是真实现，不能伪成功。

### 5.4 `bspUartTransmitDma(eDrvUartPortMap uart, const uint8_t *buffer, uint16_t length)`

职责:

- 启动一次 DMA 发送。

实现要求:

- 必须管理 DMA 忙状态。
- DMA 结束或错误时必须清理发送状态。
- 若直接使用调用方缓冲区，需要保证 DMA 未完成前该缓冲区不可被改写的契约清晰一致。

### 5.5 `bspUartGetDataLen(eDrvUartPortMap uart)`

职责:

- 返回 BSP 原始接收缓冲中当前可读字节数。

实现要求:

- 返回值语义必须与 `bspUartReceive()` 完全一致。
- 返回的是“BSP 还能被读走多少字节”，不是公共层 ring buffer 里有多少字节。
- 无效或未初始化时返回 `0U`。

### 5.6 `bspUartReceive(eDrvUartPortMap uart, uint8_t *buffer, uint16_t length)`

职责:

- 从 BSP 原始接收缓冲里取出数据。

实现要求:

- 必须推进 BSP 自己的读指针或消费状态。
- 读取后要与 `bspUartGetDataLen()` 的统计保持一致。
- 不允许越界读取原始缓冲。
- 不要在这里写公共层 ring buffer。

## 6. `drvuart_port.c` 应承担的内容

当前 port 层除了绑定 BSP 钩子，还承担两件重要工作:

- 准备公共层 ring buffer 的存储区。
- 根据逻辑 UART 返回对应的 ring buffer 与容量配置。

这说明 UART 的 port 层不只是简单绑定层，它还负责“公共层缓存资源”这一项目级配置。

当前绑定结构应保持类似:

```c
stDrvUartBspInterface gDrvUartBspInterface[DRVUART_MAX] = {
    {
        .init = bspUartInit,
        .transmit = bspUartTransmit,
        .transmitIt = bspUartTransmitIt,
        .transmitDma = bspUartTransmitDma,
        .getDataLen = bspUartGetDataLen,
        .receive = bspUartReceive,
        .Buffer = gUartRxStorageDebug,
    }
};
```

## 7. 当前工程默认逻辑资源

根据 `drvuart_port.h`，当前工程只定义了一个逻辑 UART:

- `DRVUART_DEBUG`

它对应的公共层接收缓存容量由 `DRVUART_RECVLEN_DEBUGUART` 决定。以后如果新增其他逻辑串口，应该先扩展枚举、缓存和 port 绑定表，再补 BSP。

## 8. 日志层对 UART 的额外约束

当前日志接口会复用 `DRVUART_DEBUG`:

- `drvUartLogInit()`
- `drvUartLogWrite()`
- `drvUartLogGetInputBuffer()`

因此如果调整 `DRVUART_DEBUG` 的 BSP 实现，需要同步考虑:

- DMA 发送是否还能满足日志输出。
- 接收路径是否还能支持控制台输入。
- 关闭 console 功能时，是否只影响 port/debug 扩展，而不是破坏基础 UART 语义。

## 9. 修改 BSP 时必须注意的问题

- BSP 原始接收缓冲和公共层 ring buffer 是两层不同资源，不要混用。
- `drvUartSyncRxData()` 会分块搬运数据，`bspUartReceive()` 必须允许被多次小块读取。
- `bspUartGetDataLen()` 统计如果不稳定，会直接导致公共层丢包或重复读。
- 可选钩子不需要时可以不绑定，但绑定后必须真实可用。

## 10. 联调检查项

- `drvUartInit()` 后是否能稳定发送和接收。
- DMA 或中断接收时，`bspUartGetDataLen()` 与 `bspUartReceive()` 是否计数一致。
- 公共层 ring buffer 是否会重复收同一批数据。
- DMA 发送忙状态是否会在完成后正确释放。
- 日志输出和输入是否仍然工作正常。
