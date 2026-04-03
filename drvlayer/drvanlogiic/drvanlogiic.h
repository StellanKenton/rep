/************************************************************************************
* @file     : drvanlogiic.h
* @brief    : Reusable software IIC driver abstraction.
* @details  : This module exposes a stable bit-banged IIC interface for upper
*             modules while hiding board-specific GPIO operations behind hooks.
* @author   : GitHub Copilot
* @date     : 2026-04-01
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef DRVANLOGIIC_H
#define DRVANLOGIIC_H

#include <stdbool.h>
#include <stdint.h>

#include "rep_config.h"
#include "drvanlogiic_port.h"

#ifdef __cplusplus
extern "C" {
#endif


typedef void (*drvAnlogIicBspInitFunc)(eDrvAnlogIicPortMap iic);
typedef void (*drvAnlogIicBspDriveLineFunc)(eDrvAnlogIicPortMap iic, bool releaseHigh);
typedef bool (*drvAnlogIicBspReadLineFunc)(eDrvAnlogIicPortMap iic);
typedef void (*drvAnlogIicBspDelayUsFunc)(uint16_t delayUs);

typedef struct stDrvAnlogIicBspInterface {
    drvAnlogIicBspInitFunc init;
    drvAnlogIicBspDriveLineFunc setScl;
    drvAnlogIicBspDriveLineFunc setSda;
    drvAnlogIicBspReadLineFunc readScl;
    drvAnlogIicBspReadLineFunc readSda;
    drvAnlogIicBspDelayUsFunc delayUs;
    uint16_t halfPeriodUs;
    uint8_t recoveryClockCount;
} stDrvAnlogIicBspInterface;

typedef struct stDrvAnlogIicTransfer {
    uint8_t address;
    const uint8_t *writeBuffer;
    uint16_t writeLength;
    uint8_t *readBuffer;
    uint16_t readLength;
    const uint8_t *secondWriteBuffer;
    uint16_t secondWriteLength;
} stDrvAnlogIicTransfer;

typedef eDrvStatus (*drvAnlogIicBusActionFunc)(eDrvAnlogIicPortMap iic, stDrvAnlogIicBspInterface *bspInterface, void *context);

eDrvStatus drvAnlogIicInit(eDrvAnlogIicPortMap iic);
eDrvStatus drvAnlogIicRecoverBus(eDrvAnlogIicPortMap iic);
eDrvStatus drvAnlogIicBusAction(eDrvAnlogIicPortMap iic, drvAnlogIicBusActionFunc action, void *context);
eDrvStatus drvAnlogIicTransfer(eDrvAnlogIicPortMap iic, const stDrvAnlogIicTransfer *transfer);
eDrvStatus drvAnlogIicTransferTimeout(eDrvAnlogIicPortMap iic, const stDrvAnlogIicTransfer *transfer, uint32_t timeoutMs);
eDrvStatus drvAnlogIicWrite(eDrvAnlogIicPortMap iic, uint8_t address, const uint8_t *buffer, uint16_t length);
eDrvStatus drvAnlogIicWriteTimeout(eDrvAnlogIicPortMap iic, uint8_t address, const uint8_t *buffer, uint16_t length, uint32_t timeoutMs);
eDrvStatus drvAnlogIicRead(eDrvAnlogIicPortMap iic, uint8_t address, uint8_t *buffer, uint16_t length);
eDrvStatus drvAnlogIicReadTimeout(eDrvAnlogIicPortMap iic, uint8_t address, uint8_t *buffer, uint16_t length, uint32_t timeoutMs);
eDrvStatus drvAnlogIicWriteRegister(eDrvAnlogIicPortMap iic, uint8_t address, const uint8_t *registerBuffer, uint16_t registerLength, const uint8_t *buffer, uint16_t length);
eDrvStatus drvAnlogIicWriteRegisterTimeout(eDrvAnlogIicPortMap iic, uint8_t address, const uint8_t *registerBuffer, uint16_t registerLength, const uint8_t *buffer, uint16_t length, uint32_t timeoutMs);
eDrvStatus drvAnlogIicReadRegister(eDrvAnlogIicPortMap iic, uint8_t address, const uint8_t *registerBuffer, uint16_t registerLength, uint8_t *buffer, uint16_t length);
eDrvStatus drvAnlogIicReadRegisterTimeout(eDrvAnlogIicPortMap iic, uint8_t address, const uint8_t *registerBuffer, uint16_t registerLength, uint8_t *buffer, uint16_t length, uint32_t timeoutMs);

#ifdef __cplusplus
}
#endif

#endif  // DRVANLOGIIC_H
/**************************End of file********************************/

