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

#ifdef __cplusplus
extern "C" {
#endif

#ifndef DRVANLOGIIC_LOG_SUPPORT
#define DRVANLOGIIC_LOG_SUPPORT                 1
#endif

#ifndef DRVANLOGIIC_CONSOLE_SUPPORT
#define DRVANLOGIIC_CONSOLE_SUPPORT             1
#endif

#ifndef DRVANLOGIIC_MAX
#define DRVANLOGIIC_MAX                         2U
#endif

#ifndef DRVANLOGIIC_LOCK_WAIT_MS
#define DRVANLOGIIC_LOCK_WAIT_MS                5U
#endif

#ifndef DRVANLOGIIC_DEFAULT_HALF_PERIOD_US
#define DRVANLOGIIC_DEFAULT_HALF_PERIOD_US      10U
#endif

#ifndef DRVANLOGIIC_DEFAULT_RECOVERY_CLOCKS
#define DRVANLOGIIC_DEFAULT_RECOVERY_CLOCKS     9U
#endif


typedef void (*drvAnlogIicBspInitFunc)(uint8_t iic);
typedef void (*drvAnlogIicBspDriveLineFunc)(uint8_t iic, bool releaseHigh);
typedef bool (*drvAnlogIicBspReadLineFunc)(uint8_t iic);
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

typedef eDrvStatus (*drvAnlogIicBusActionFunc)(uint8_t iic, stDrvAnlogIicBspInterface *bspInterface, void *context);

eDrvStatus drvAnlogIicInit(uint8_t iic);
eDrvStatus drvAnlogIicRecoverBus(uint8_t iic);
eDrvStatus drvAnlogIicBusAction(uint8_t iic, drvAnlogIicBusActionFunc action, void *context);
eDrvStatus drvAnlogIicTransfer(uint8_t iic, const stDrvAnlogIicTransfer *transfer);
eDrvStatus drvAnlogIicTransferTimeout(uint8_t iic, const stDrvAnlogIicTransfer *transfer, uint32_t timeoutMs);
eDrvStatus drvAnlogIicWrite(uint8_t iic, uint8_t address, const uint8_t *buffer, uint16_t length);
eDrvStatus drvAnlogIicWriteTimeout(uint8_t iic, uint8_t address, const uint8_t *buffer, uint16_t length, uint32_t timeoutMs);
eDrvStatus drvAnlogIicRead(uint8_t iic, uint8_t address, uint8_t *buffer, uint16_t length);
eDrvStatus drvAnlogIicReadTimeout(uint8_t iic, uint8_t address, uint8_t *buffer, uint16_t length, uint32_t timeoutMs);
eDrvStatus drvAnlogIicWriteRegister(uint8_t iic, uint8_t address, const uint8_t *registerBuffer, uint16_t registerLength, const uint8_t *buffer, uint16_t length);
eDrvStatus drvAnlogIicWriteRegisterTimeout(uint8_t iic, uint8_t address, const uint8_t *registerBuffer, uint16_t registerLength, const uint8_t *buffer, uint16_t length, uint32_t timeoutMs);
eDrvStatus drvAnlogIicReadRegister(uint8_t iic, uint8_t address, const uint8_t *registerBuffer, uint16_t registerLength, uint8_t *buffer, uint16_t length);
eDrvStatus drvAnlogIicReadRegisterTimeout(uint8_t iic, uint8_t address, const uint8_t *registerBuffer, uint16_t registerLength, uint8_t *buffer, uint16_t length, uint32_t timeoutMs);

const stDrvAnlogIicBspInterface *drvAnlogIicGetPlatformBspInterfaces(void);

#ifdef __cplusplus
}
#endif

#endif  // DRVANLOGIIC_H
/**************************End of file********************************/

