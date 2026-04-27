/************************************************************************************
* @file     : ec800m_assembly.h
* @brief    : EC800M assembly-time contract shared by core and port.
* @author   : GitHub Copilot
* @date     : 2026-04-27
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef EC800M_ASSEMBLY_H
#define EC800M_ASSEMBLY_H

#include <stdint.h>

#include "ec800m.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef eDrvStatus (*ec800mTransportInitFunc)(uint8_t linkId);
typedef eDrvStatus (*ec800mTransportWriteFunc)(uint8_t linkId, const uint8_t *buffer, uint16_t length, uint32_t timeoutMs);
typedef uint16_t (*ec800mTransportGetRxLenFunc)(uint8_t linkId);
typedef eDrvStatus (*ec800mTransportReadFunc)(uint8_t linkId, uint8_t *buffer, uint16_t length);
typedef uint32_t (*ec800mTransportGetTickMsFunc)(void);
typedef void (*ec800mControlInitFunc)(uint8_t pwrkeyPin, uint8_t resetPin);
typedef void (*ec800mControlSetPinLevelFunc)(uint8_t pin, bool isActive);

typedef struct stEc800mTransportInterface {
    ec800mTransportInitFunc init;
    ec800mTransportWriteFunc write;
    ec800mTransportGetRxLenFunc getRxLen;
    ec800mTransportReadFunc read;
    ec800mTransportGetTickMsFunc getTickMs;
} stEc800mTransportInterface;

typedef struct stEc800mControlInterface {
    ec800mControlInitFunc init;
    ec800mControlSetPinLevelFunc setPwrkeyLevel;
    ec800mControlSetPinLevelFunc setResetLevel;
} stEc800mControlInterface;

void ec800mLoadPlatformDefaultCfg(eEc800mMapType device, stEc800mCfg *cfg);
const stEc800mTransportInterface *ec800mGetPlatformTransportInterface(const stEc800mCfg *cfg);
const stEc800mControlInterface *ec800mGetPlatformControlInterface(eEc800mMapType device);
bool ec800mPlatformIsValidCfg(const stEc800mCfg *cfg);

#ifdef __cplusplus
}
#endif

#endif  // EC800M_ASSEMBLY_H
/**************************End of file********************************/
