# Log Hook Guide

## Overview

The log module uses one static interface array in `log_port.c`.

- `gLogPortInterfaces[]` defines all transport channels.
- `REP_LOG_OUTPUT_PORT` decides how many entries from the start of `gLogInterfaces[]` are active.
- Each valid and enabled output receives every formatted log line.
- Inputs are queried by `transport` through `logGetInputBuffer(transport)`.

Default transport IDs are declared in `log.h`:

- `LOG_TRANSPORT_NONE`
- `LOG_TRANSPORT_RTT`
- `LOG_TRANSPORT_UART`
- `LOG_TRANSPORT_CAN`

Default line buffer size is also declared in `log.h`:

- `LOG_LINE_BUFFER_SIZE`

## Output Hook Signature

Use this function type for log output:

```c
typedef int32_t (*logOutputWriteFunc)(const uint8_t *buffer, uint16_t length);
```

Requirements:

- Return the number of bytes actually sent.
- Return `0` when the input buffer is invalid or nothing is sent.
- Do not append extra prefixes, timestamps, or line endings in the hook.
- Keep the hook focused on transport only.

Optional init hook:

```c
typedef void (*logInitFunc)(void);
```

Use it for transport initialization that must happen when `logInit()` runs.

## Output Hook Example

UART output example:

```c
static void logUartInit(void)
{
    (void)drvUartInit(DRVUART_DEBUG);
}

static int32_t logUartWrite(const uint8_t *buffer, uint16_t length)
{
    if (buffer == NULL || length == 0U) {
        return 0;
    }

    if (drvUartTransmit(DRVUART_DEBUG, buffer, length, 100U) != DRVUART_STATUS_OK) {
        return 0;
    }

    return (int32_t)length;
}
```

CAN output example:

```c
static int32_t logCanWrite(const uint8_t *buffer, uint16_t length)
{
    if (buffer == NULL || length == 0U) {
        return 0;
    }

    return canLogFrameSend(buffer, length);
}
```

## Input Hook Signature

Use this function type for log input:

```c
typedef stRingBuffer *(*logInputGetBufferFunc)(void);
```

Requirements:

- Return the ring buffer instance owned by the transport.
- Return `NULL` when no input buffer is available.
- The log module does not allocate or manage this ring buffer.

Input and output share the same `init` hook. One transport only keeps one init function.

## Input Hook Example

UART input example:

```c
static void logUartInit(void)
{
    (void)drvUartInit(DRVUART_DEBUG);
}

static stRingBuffer *logUartGetBuffer(void)
{
    return drvUartGetRingBuffer(DRVUART_DEBUG);
}
```

## Interface Array Example

Add transport channels in `log_port.c` like this:

```c
static const stLogInterface gLogPortInterfaces[] = {
    {
        .transport = LOG_TRANSPORT_RTT,
        .init = bspRttLogInit,
        .write = bspRttLogWrite,
        .getBuffer = bspRttLogGetInputBuffer,
        .isOutputEnabled = true,
        .isInputEnabled = true,
    },
    {
        .transport = LOG_TRANSPORT_UART,
        .init = logUartInit,
        .write = logUartWrite,
        .getBuffer = logUartGetBuffer,
        .isOutputEnabled = true,
        .isInputEnabled = true,
    },
    {
        .transport = LOG_TRANSPORT_CAN,
        .init = NULL,
        .write = logCanWrite,
        .getBuffer = NULL,
        .isOutputEnabled = false,
        .isInputEnabled = false,
    },
};
```

If a transport has no output or no input, fill that side with `NULL`.

When you append a new entry, increase `REP_LOG_OUTPUT_PORT` so the new entry is included by `logInit()` and `logVWrite()`.

## Enable Rule

- `isOutputEnabled = true`: the output side is active and can send logs.
- `isOutputEnabled = false`: the output side is ignored even if `write` is present.
- `isInputEnabled = true`: the input side is active and can return a ring buffer.
- `isInputEnabled = false`: the input side is ignored even if `getBuffer` is present.

## Extension Notes

- Keep one transport implementation in one dedicated file when possible, such as `SEGGER/bsp_rtt.c`, `log_port.c`, or `log_can.c`.
- Do not put transport SDK details directly into `log.c`; keep them in `log_port.c` unless the module is only used on one platform.
- Reuse existing driver modules for UART and CAN instead of calling MCU SDK APIs directly from business logic.
- `SEGGER/bsp_rtt.c` now uses local default macros for RTT details: `BSP_RTT_LOG_OUTPUT_ENABLE`, `BSP_RTT_LOG_INPUT_ENABLE`, `BSP_RTT_UP_BUFFER_INDEX`, `BSP_RTT_DOWN_BUFFER_INDEX`, and `BSP_RTT_INPUT_BUFFER_SIZE`.