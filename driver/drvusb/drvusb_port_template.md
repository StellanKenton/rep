# drvusb Port Templates

这份模板给出三类常见平台的 `drvusb` 对接骨架，目标是把差异压缩在 `User/port` 或项目绑定层。

## 通用映射原则

- `drvUsbInit`：完成控制器和 class/stack 的一次性初始化。
- `drvUsbStart` / `drvUsbStop`：启动或停止 USB 控制器。
- `drvUsbConnect` / `drvUsbDisconnect`：控制 D+ 上拉、软连接或控制器连接状态。
- `drvUsbOpenEndpoint` / `drvUsbCloseEndpoint` / `drvUsbFlushEndpoint`：直接映射到底层 endpoint API。
- `drvUsbTransmit` / `drvUsbReceive`：优先接 class 层稳定接口；如果没有 class 层接口，再退回 endpoint 原语。
- `drvUsbIsConfigured`：尽量用 USB 设备栈状态机，而不是本地布尔变量。

## STM32 HAL PCD + Cube CDC

适用场景：STM32 FS/HS Device，使用 Cube 生成的 `USB_DEVICE` 和 `usbd_cdc_if.c`。

关键映射：

- 控制器句柄：`USBD_HandleTypeDef hUsbDeviceFS`
- 低层控制器：`PCD_HandleTypeDef *pcd = (PCD_HandleTypeDef *)hUsbDeviceFS.pData`
- 启停：`USBD_Start()` / `USBD_Stop()`
- 软连接：`HAL_PCD_DevConnect()` / `HAL_PCD_DevDisconnect()`
- 端点：`HAL_PCD_EP_Open()` / `HAL_PCD_EP_Close()` / `HAL_PCD_EP_Flush()`
- CDC 发送：`CDC_Transmit_FS()`
- CDC 接收：在 `CDC_Receive_FS()` 用户代码区把数据写入环形缓冲

推荐骨架：

```c
static eDrvStatus drvUsbPortTransmit(uint8_t usb, uint8_t ep, const uint8_t *buffer, uint16_t length, uint32_t timeoutMs)
{
    uint32_t startTick = HAL_GetTick();

    if (ep != CDC_IN_EP) {
        return DRV_STATUS_UNSUPPORTED;
    }

    while ((HAL_GetTick() - startTick) < timeoutMs) {
        uint8_t status = CDC_Transmit_FS((uint8_t *)buffer, length);
        if (status == USBD_OK) {
            return DRV_STATUS_OK;
        }
        if (status != USBD_BUSY) {
            return DRV_STATUS_ERROR;
        }
        (void)repRtosDelayMs(1U);
    }

    return DRV_STATUS_TIMEOUT;
}
```

## GD32 USBFS Device

适用场景：GD32F3/F4 等带 USBFS Device 库的平台。

关键映射：

- 控制器初始化：`usbd_init()` 或厂商 USB Device 初始化入口
- 启停：通常由 `usbd_init()` + 中断使能启动；停止时关闭中断、复位 core
- 端点打开关闭：`usbd_ep_setup()`、`usbd_ep_clear()` 或同类 API
- IN 发送：`usbd_ep_send()`
- OUT 接收：在 `DataOut` 回调里把数据推入本地 ring buffer
- 状态查询：使用 GD32 device core 的 `cur_status`、`cur_addr`、`cur_config` 等状态字段

推荐做法：

- 不要把 `usb_core_driver` 或 `usb_dev` 对象暴露给 `rep/driver`
- 在 port 文件里维护 `gDrvUsbDeviceContext[]`
- 中断回调只做搬运，复杂解析留在 class 层

示例骨架：

```c
static eDrvStatus drvUsbPortOpenEndpoint(uint8_t usb, const stDrvUsbEndpointConfig *config)
{
    gd32_usb_device_t *device = drvUsbPortGetDevice(usb);

    if ((device == NULL) || (config == NULL)) {
        return DRV_STATUS_INVALID_PARAM;
    }

    usbd_ep_setup(device,
                  config->endpointAddress,
                  drvUsbPortMapEpType(config->type),
                  config->maxPacketSize);
    return DRV_STATUS_OK;
}
```

## ESP32 TinyUSB / ESP-IDF

适用场景：ESP32-S2/S3/P4 等使用 TinyUSB Device 栈，或者 ESP-IDF USB Serial/JTAG、TinyUSB 组件。

关键映射：

- 初始化：`tinyusb_driver_install()` 或 `tusb_init()`
- 主循环：`tud_task()`，或者由 TinyUSB 任务托管
- 连接态：`tud_connected()`
- 配置态：`tud_mounted()`
- CDC 发送：`tud_cdc_n_write()` + `tud_cdc_n_write_flush()`
- CDC 接收：`tud_cdc_n_available()` + `tud_cdc_n_read()`，或在 `tud_cdc_rx_cb()` 中转存
- 端点原语：TinyUSB 更推荐 class API；只有做 vendor class 时再直接走 endpoint 层

推荐做法：

- 如果项目已经采用 TinyUSB，`drvusb` 的 `transmit/receive` 优先包一层 class API
- 把 `tud_task()` 的调用位置放在系统任务，不要塞进 `rep/driver`
- 对多 CDC 口场景，用 `usb` 逻辑编号映射到 `itf` 或 `cdc_n`

示例骨架：

```c
static eDrvStatus drvUsbPortReceive(uint8_t usb, uint8_t ep, uint8_t *buffer, uint16_t length, uint16_t *actualLength, uint32_t timeoutMs)
{
    uint32_t startTick = drvUsbPortGetTickMs();
    uint8_t itf = drvUsbPortGetCdcInterface(usb);

    if (ep != DRVUSB_ENDPOINT_DIRECTION_OUT + 1U) {
        return DRV_STATUS_UNSUPPORTED;
    }

    while ((drvUsbPortGetTickMs() - startTick) < timeoutMs) {
        uint32_t available = tud_cdc_n_available(itf);
        if (available > 0U) {
            uint32_t readLen = tud_cdc_n_read(itf, buffer, length);
            if (actualLength != NULL) {
                *actualLength = (uint16_t)readLen;
            }
            return DRV_STATUS_OK;
        }
        vTaskDelay(pdMS_TO_TICKS(1U));
    }

    return DRV_STATUS_TIMEOUT;
}
```

## 建议边界

- `drvusb` 保持控制器抽象，不绑定具体 CDC/MSC/HID 描述符。
- class 描述符、VID/PID、字符串描述符、端点号常量放在 port 或 USB stack 配置层。
- 如果平台没有稳定的通用 endpoint API，优先保证 CDC 类路径跑通，再逐步补通用 endpoint 能力。