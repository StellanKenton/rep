# drvusb

`drvusb` 是 `rep/driver` 下的通用 USB 控制器抽象层，用来把上层业务代码和具体 MCU USB 外设、HAL、USB 中间件解耦。

## 设计目标

- 统一 `stm32`、`gd32`、`esp32` 等不同平台的 USB 控制器接入方式。
- 上层只依赖稳定的 `drvUsb*` 接口，不直接依赖 `HAL_PCD`、`TinyUSB`、`ESP-IDF USB` 等厂商 API。
- 平台差异通过 `drvUsbGetPlatformBspInterfaces()` 返回的 BSP hook 表实现。

## 接口范围

- 控制器生命周期：`drvUsbInit`、`drvUsbStart`、`drvUsbStop`
- 物理连接控制：`drvUsbConnect`、`drvUsbDisconnect`
- 端点管理：`drvUsbOpenEndpoint`、`drvUsbCloseEndpoint`、`drvUsbFlushEndpoint`
- 端点数据收发：`drvUsbTransmit`、`drvUsbReceive`
- 状态查询：`drvUsbIsConnected`、`drvUsbIsConfigured`、`drvUsbGetSpeed`、`drvUsbGetRole`

## 平台移植建议

在 `User/port/drvusb_port.c` 或其他项目绑定文件里实现：

```c
static const stDrvUsbBspInterface gDrvUsbInterfaces[DRVUSB_MAX] = {
    [DRVUSB_DEV0] = {
        .init = drvUsbPortInit,
        .start = drvUsbPortStart,
        .stop = drvUsbPortStop,
        .setConnect = drvUsbPortSetConnect,
        .openEndpoint = drvUsbPortOpenEndpoint,
        .closeEndpoint = drvUsbPortCloseEndpoint,
        .flushEndpoint = drvUsbPortFlushEndpoint,
        .transmit = drvUsbPortTransmit,
        .receive = drvUsbPortReceive,
        .isConnected = drvUsbPortIsConnected,
        .isConfigured = drvUsbPortIsConfigured,
        .getSpeed = drvUsbPortGetSpeed,
        .defaultTimeoutMs = DRVUSB_DEFAULT_TIMEOUT_MS,
        .role = DRVUSB_ROLE_DEVICE,
    },
};

const stDrvUsbBspInterface *drvUsbGetPlatformBspInterfaces(void)
{
    return gDrvUsbInterfaces;
}
```

## 分层约束

- `rep/driver/drvusb` 只保留通用抽象和参数校验，不放具体芯片寄存器或 HAL 代码。
- 具体板级、中间件、端点号和 class 绑定放在 `User/port` 或项目绑定层。
- 如果项目只需要 CDC、MSC、HID 中的某一种 class，也建议把 class 细节保留在 port 层，而不是写死进 `drvusb`。

## 现成模板

- 当前 STM32F103 工程可直接参考 `User/port/drvusb_port.c` 和 `User/port/drvusb_port.h`。
- 更标准的跨平台对接模板见 `drvusb_port_template.md`，里面分别给了 STM32 HAL PCD、GD32 USBFS、ESP32 TinyUSB/ESP-IDF 的接入建议。