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
#include "drviic_types.h"

#ifdef __cplusplus
extern "C" {
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

typedef eDrvStatus (*drvIicBspInitFunc)(eDrvIicPortMap iic);
typedef eDrvStatus (*drvIicBspTransferFunc)(eDrvIicPortMap iic, const stDrvIicTransfer *transfer, uint32_t timeoutMs);
typedef eDrvStatus (*drvIicBspRecoverBusFunc)(eDrvIicPortMap iic);

typedef struct stDrvIicBspInterface {
    drvIicBspInitFunc init;
    drvIicBspTransferFunc transfer;
    drvIicBspRecoverBusFunc recoverBus;
    uint32_t defaultTimeoutMs;
} stDrvIicBspInterface;

eDrvStatus drvIicInit(eDrvIicPortMap iic);
eDrvStatus drvIicRecoverBus(eDrvIicPortMap iic);
eDrvStatus drvIicTransfer(eDrvIicPortMap iic, const stDrvIicTransfer *transfer);
eDrvStatus drvIicTransferTimeout(eDrvIicPortMap iic, const stDrvIicTransfer *transfer, uint32_t timeoutMs);
eDrvStatus drvIicWrite(eDrvIicPortMap iic, uint8_t address, const uint8_t *buffer, uint16_t length);
eDrvStatus drvIicWriteTimeout(eDrvIicPortMap iic, uint8_t address, const uint8_t *buffer, uint16_t length, uint32_t timeoutMs);
eDrvStatus drvIicRead(eDrvIicPortMap iic, uint8_t address, uint8_t *buffer, uint16_t length);
eDrvStatus drvIicReadTimeout(eDrvIicPortMap iic, uint8_t address, uint8_t *buffer, uint16_t length, uint32_t timeoutMs);
eDrvStatus drvIicWriteRegister(eDrvIicPortMap iic, uint8_t address, const uint8_t *registerBuffer, uint16_t registerLength, const uint8_t *buffer, uint16_t length);
eDrvStatus drvIicWriteRegisterTimeout(eDrvIicPortMap iic, uint8_t address, const uint8_t *registerBuffer, uint16_t registerLength, const uint8_t *buffer, uint16_t length, uint32_t timeoutMs);
eDrvStatus drvIicReadRegister(eDrvIicPortMap iic, uint8_t address, const uint8_t *registerBuffer, uint16_t registerLength, uint8_t *buffer, uint16_t length);
eDrvStatus drvIicReadRegisterTimeout(eDrvIicPortMap iic, uint8_t address, const uint8_t *registerBuffer, uint16_t registerLength, uint8_t *buffer, uint16_t length, uint32_t timeoutMs);

#ifdef __cplusplus
}
#endif

#endif  // DRVIIC_H
/**************************End of file********************************/

