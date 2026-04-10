/************************************************************************************
* @file     : drviic.h
* @brief    : Reusable hardware IIC driver abstraction.
* @details  : This module exposes a stable master-mode IIC interface for upper
*             modules while hiding board-specific controller details behind hooks.
* @author   : GitHub Copilot
* @date     : 2026-04-01
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef DRVIIC_H
#define DRVIIC_H

#include <stdint.h>

#include "rep_config.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef DRVIIC_LOG_SUPPORT
#define DRVIIC_LOG_SUPPORT                    1
#endif

#ifndef DRVIIC_CONSOLE_SUPPORT
#define DRVIIC_CONSOLE_SUPPORT                1
#endif

#ifndef DRVIIC_MAX
#define DRVIIC_MAX                            1U
#endif

#ifndef DRVIIC_LOCK_WAIT_MS
#define DRVIIC_LOCK_WAIT_MS                   5U
#endif

#ifndef DRVIIC_DEFAULT_TIMEOUT_MS
#define DRVIIC_DEFAULT_TIMEOUT_MS             100U
#endif

typedef struct stDrvIicTransfer {
    uint8_t address;
    const uint8_t *writeBuffer;
    uint16_t writeLength;
    uint8_t *readBuffer;
    uint16_t readLength;
    const uint8_t *secondWriteBuffer;
    uint16_t secondWriteLength;
} stDrvIicTransfer;

typedef eDrvStatus (*drvIicBspInitFunc)(uint8_t iic);
typedef eDrvStatus (*drvIicBspTransferFunc)(uint8_t iic, const stDrvIicTransfer *transfer, uint32_t timeoutMs);
typedef eDrvStatus (*drvIicBspRecoverBusFunc)(uint8_t iic);

typedef struct stDrvIicBspInterface {
    drvIicBspInitFunc init;
    drvIicBspTransferFunc transfer;
    drvIicBspRecoverBusFunc recoverBus;
    uint32_t defaultTimeoutMs;
} stDrvIicBspInterface;

eDrvStatus drvIicInit(uint8_t iic);
eDrvStatus drvIicRecoverBus(uint8_t iic);
eDrvStatus drvIicTransfer(uint8_t iic, const stDrvIicTransfer *transfer);
eDrvStatus drvIicTransferTimeout(uint8_t iic, const stDrvIicTransfer *transfer, uint32_t timeoutMs);
eDrvStatus drvIicWrite(uint8_t iic, uint8_t address, const uint8_t *buffer, uint16_t length);
eDrvStatus drvIicWriteTimeout(uint8_t iic, uint8_t address, const uint8_t *buffer, uint16_t length, uint32_t timeoutMs);
eDrvStatus drvIicRead(uint8_t iic, uint8_t address, uint8_t *buffer, uint16_t length);
eDrvStatus drvIicReadTimeout(uint8_t iic, uint8_t address, uint8_t *buffer, uint16_t length, uint32_t timeoutMs);
eDrvStatus drvIicWriteRegister(uint8_t iic, uint8_t address, const uint8_t *registerBuffer, uint16_t registerLength, const uint8_t *buffer, uint16_t length);
eDrvStatus drvIicWriteRegisterTimeout(uint8_t iic, uint8_t address, const uint8_t *registerBuffer, uint16_t registerLength, const uint8_t *buffer, uint16_t length, uint32_t timeoutMs);
eDrvStatus drvIicReadRegister(uint8_t iic, uint8_t address, const uint8_t *registerBuffer, uint16_t registerLength, uint8_t *buffer, uint16_t length);
eDrvStatus drvIicReadRegisterTimeout(uint8_t iic, uint8_t address, const uint8_t *registerBuffer, uint16_t registerLength, uint8_t *buffer, uint16_t length, uint32_t timeoutMs);

const stDrvIicBspInterface *drvIicGetPlatformBspInterfaces(void);

#ifdef __cplusplus
}
#endif

#endif  // DRVIIC_H
/**************************End of file********************************/

