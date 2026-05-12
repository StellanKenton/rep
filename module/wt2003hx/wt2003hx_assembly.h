/************************************************************************************
* @file     : wt2003hx_assembly.h
* @brief    : WT2003HX assembly-time contract shared by core and port.
* @details  : Declares transport and control hooks used by the reusable WT2003HX
*             module core.
* @author   : 
* @date     : 2026-04-30
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef WT2003HX_ASSEMBLY_H
#define WT2003HX_ASSEMBLY_H

#include <stdbool.h>
#include <stdint.h>

#include "wt2003hx.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef eDrvStatus (*wt2003hxTransportInitFunc)(uint8_t linkId);
typedef eDrvStatus (*wt2003hxTransportWriteFunc)(uint8_t linkId, const uint8_t *buffer, uint16_t length, uint32_t timeoutMs);
typedef uint16_t (*wt2003hxTransportGetRxLenFunc)(uint8_t linkId);
typedef eDrvStatus (*wt2003hxTransportReadFunc)(uint8_t linkId, uint8_t *buffer, uint16_t length);
typedef uint32_t (*wt2003hxTransportGetTickMsFunc)(void);
typedef void (*wt2003hxControlSetEnableFunc)(uint8_t enablePin, bool enabled);
typedef void (*wt2003hxControlDelayMsFunc)(uint32_t delayMs);

typedef struct stWt2003hxTransportInterface {
    wt2003hxTransportInitFunc init;
    wt2003hxTransportWriteFunc write;
    wt2003hxTransportGetRxLenFunc getRxLen;
    wt2003hxTransportReadFunc read;
    wt2003hxTransportGetTickMsFunc getTickMs;
} stWt2003hxTransportInterface;

typedef struct stWt2003hxControlInterface {
    wt2003hxControlSetEnableFunc setEnable;
    wt2003hxControlDelayMsFunc delayMs;
} stWt2003hxControlInterface;

typedef struct stWt2003hxOps {
    void (*loadDefaultCfg)(eWt2003hxMapType device, stWt2003hxCfg *cfg);
    const stWt2003hxTransportInterface *(*getTransportInterface)(const stWt2003hxCfg *cfg);
    const stWt2003hxControlInterface *(*getControlInterface)(eWt2003hxMapType device);
    bool (*isValidCfg)(const stWt2003hxCfg *cfg);
} stWt2003hxOps;

#ifdef __cplusplus
}
#endif

#endif  // WT2003HX_ASSEMBLY_H
/**************************End of file********************************/
