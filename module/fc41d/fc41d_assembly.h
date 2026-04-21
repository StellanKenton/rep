/************************************************************************************
* @file     : fc41d_assembly.h
* @brief    : FC41D assembly-time contract shared by core and port.
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef FC41D_ASSEMBLY_H
#define FC41D_ASSEMBLY_H

#include <stdint.h>

#include "fc41d.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef eDrvStatus (*fc41dTransportInitFunc)(uint8_t linkId);
typedef eDrvStatus (*fc41dTransportWriteFunc)(uint8_t linkId, const uint8_t *buffer, uint16_t length, uint32_t timeoutMs);
typedef uint16_t (*fc41dTransportGetRxLenFunc)(uint8_t linkId);
typedef eDrvStatus (*fc41dTransportReadFunc)(uint8_t linkId, uint8_t *buffer, uint16_t length);
typedef uint32_t (*fc41dTransportGetTickMsFunc)(void);
typedef void (*fc41dControlInitFunc)(uint8_t resetPin);
typedef void (*fc41dControlSetResetLevelFunc)(uint8_t resetPin, bool isActive);

typedef struct stFc41dTransportInterface {
    fc41dTransportInitFunc init;
    fc41dTransportWriteFunc write;
    fc41dTransportGetRxLenFunc getRxLen;
    fc41dTransportReadFunc read;
    fc41dTransportGetTickMsFunc getTickMs;
} stFc41dTransportInterface;

typedef struct stFc41dControlInterface {
    fc41dControlInitFunc init;
    fc41dControlSetResetLevelFunc setResetLevel;
} stFc41dControlInterface;

void fc41dLoadPlatformDefaultCfg(eFc41dMapType device, stFc41dCfg *cfg);
const stFc41dTransportInterface *fc41dGetPlatformTransportInterface(const stFc41dCfg *cfg);
const stFc41dControlInterface *fc41dGetPlatformControlInterface(eFc41dMapType device);
bool fc41dPlatformIsValidCfg(const stFc41dCfg *cfg);

#ifdef __cplusplus
}
#endif

#endif  // FC41D_ASSEMBLY_H
/**************************End of file********************************/
