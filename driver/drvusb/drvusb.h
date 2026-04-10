/************************************************************************************
* @file     : drvusb.h
* @brief    : Reusable USB controller driver abstraction.
* @details  : This module exposes a stable USB device or host controller interface
*             for upper modules while hiding MCU- and stack-specific details behind
*             BSP hooks.
* @author   : GitHub Copilot
* @date     : 2026-04-10
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef DRVUSB_H
#define DRVUSB_H

#include <stdbool.h>
#include <stdint.h>

#include "rep_config.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef DRVUSB_LOG_SUPPORT
#define DRVUSB_LOG_SUPPORT                    1
#endif

#ifndef DRVUSB_CONSOLE_SUPPORT
#define DRVUSB_CONSOLE_SUPPORT                1
#endif

#ifndef DRVUSB_MAX
#define DRVUSB_MAX                            1U
#endif

#ifndef DRVUSB_LOCK_WAIT_MS
#define DRVUSB_LOCK_WAIT_MS                   5U
#endif

#ifndef DRVUSB_DEFAULT_TIMEOUT_MS
#define DRVUSB_DEFAULT_TIMEOUT_MS             100U
#endif

#define DRVUSB_ENDPOINT_DIRECTION_IN          0x80U
#define DRVUSB_ENDPOINT_ADDRESS_MASK          0x0FU

typedef enum eDrvUsbRole {
    DRVUSB_ROLE_UNKNOWN = 0,
    DRVUSB_ROLE_DEVICE,
    DRVUSB_ROLE_HOST,
    DRVUSB_ROLE_OTG,
} eDrvUsbRole;

typedef enum eDrvUsbSpeed {
    DRVUSB_SPEED_UNKNOWN = 0,
    DRVUSB_SPEED_LOW,
    DRVUSB_SPEED_FULL,
    DRVUSB_SPEED_HIGH,
    DRVUSB_SPEED_SUPER,
} eDrvUsbSpeed;

typedef enum eDrvUsbEndpointType {
    DRVUSB_ENDPOINT_TYPE_CONTROL = 0,
    DRVUSB_ENDPOINT_TYPE_ISOCHRONOUS,
    DRVUSB_ENDPOINT_TYPE_BULK,
    DRVUSB_ENDPOINT_TYPE_INTERRUPT,
} eDrvUsbEndpointType;

typedef struct stDrvUsbEndpointConfig {
    uint8_t endpointAddress;
    eDrvUsbEndpointType type;
    uint16_t maxPacketSize;
    uint8_t interval;
} stDrvUsbEndpointConfig;

typedef eDrvStatus (*drvUsbBspInitFunc)(uint8_t usb);
typedef eDrvStatus (*drvUsbBspStartFunc)(uint8_t usb);
typedef eDrvStatus (*drvUsbBspStopFunc)(uint8_t usb);
typedef eDrvStatus (*drvUsbBspSetConnectFunc)(uint8_t usb, bool isConnect);
typedef eDrvStatus (*drvUsbBspOpenEndpointFunc)(uint8_t usb, const stDrvUsbEndpointConfig *config);
typedef eDrvStatus (*drvUsbBspCloseEndpointFunc)(uint8_t usb, uint8_t endpointAddress);
typedef eDrvStatus (*drvUsbBspFlushEndpointFunc)(uint8_t usb, uint8_t endpointAddress);
typedef eDrvStatus (*drvUsbBspTransmitFunc)(uint8_t usb, uint8_t endpointAddress, const uint8_t *buffer, uint16_t length, uint32_t timeoutMs);
typedef eDrvStatus (*drvUsbBspReceiveFunc)(uint8_t usb, uint8_t endpointAddress, uint8_t *buffer, uint16_t length, uint16_t *actualLength, uint32_t timeoutMs);
typedef bool (*drvUsbBspGetStateFunc)(uint8_t usb);
typedef eDrvUsbSpeed (*drvUsbBspGetSpeedFunc)(uint8_t usb);

typedef struct stDrvUsbBspInterface {
    drvUsbBspInitFunc init;
    drvUsbBspStartFunc start;
    drvUsbBspStopFunc stop;
    drvUsbBspSetConnectFunc setConnect;
    drvUsbBspOpenEndpointFunc openEndpoint;
    drvUsbBspCloseEndpointFunc closeEndpoint;
    drvUsbBspFlushEndpointFunc flushEndpoint;
    drvUsbBspTransmitFunc transmit;
    drvUsbBspReceiveFunc receive;
    drvUsbBspGetStateFunc isConnected;
    drvUsbBspGetStateFunc isConfigured;
    drvUsbBspGetSpeedFunc getSpeed;
    uint32_t defaultTimeoutMs;
    eDrvUsbRole role;
} stDrvUsbBspInterface;

eDrvStatus drvUsbInit(uint8_t usb);
eDrvStatus drvUsbStart(uint8_t usb);
eDrvStatus drvUsbStop(uint8_t usb);
eDrvStatus drvUsbConnect(uint8_t usb);
eDrvStatus drvUsbDisconnect(uint8_t usb);
eDrvStatus drvUsbOpenEndpoint(uint8_t usb, const stDrvUsbEndpointConfig *config);
eDrvStatus drvUsbCloseEndpoint(uint8_t usb, uint8_t endpointAddress);
eDrvStatus drvUsbFlushEndpoint(uint8_t usb, uint8_t endpointAddress);
eDrvStatus drvUsbTransmit(uint8_t usb, uint8_t endpointAddress, const uint8_t *buffer, uint16_t length);
eDrvStatus drvUsbTransmitTimeout(uint8_t usb, uint8_t endpointAddress, const uint8_t *buffer, uint16_t length, uint32_t timeoutMs);
eDrvStatus drvUsbReceive(uint8_t usb, uint8_t endpointAddress, uint8_t *buffer, uint16_t length, uint16_t *actualLength);
eDrvStatus drvUsbReceiveTimeout(uint8_t usb, uint8_t endpointAddress, uint8_t *buffer, uint16_t length, uint16_t *actualLength, uint32_t timeoutMs);
bool drvUsbIsConnected(uint8_t usb);
bool drvUsbIsConfigured(uint8_t usb);
eDrvUsbSpeed drvUsbGetSpeed(uint8_t usb);
eDrvUsbRole drvUsbGetRole(uint8_t usb);

const stDrvUsbBspInterface *drvUsbGetPlatformBspInterfaces(void);

#ifdef __cplusplus
}
#endif

#endif  // DRVUSB_H
/**************************End of file********************************/
