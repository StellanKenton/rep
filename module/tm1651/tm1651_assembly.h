/************************************************************************************
* @file     : tm1651_assembly.h
* @brief    : TM1651 assembly-time contract shared by core and port.
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef TM1651_ASSEMBLY_H
#define TM1651_ASSEMBLY_H

#include <stdint.h>

#include "tm1651.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef eDrvStatus (*tm1651IicInitFunc)(uint8_t bus);
typedef eDrvStatus (*tm1651WriteFrameFunc)(uint8_t bus, const uint8_t *buffer, uint8_t length);

typedef struct stTm1651IicInterface {
    tm1651IicInitFunc init;
    tm1651WriteFrameFunc writeFrame;
} stTm1651IicInterface;

typedef struct stTm1651AssembleCfg {
    uint8_t linkId;
} stTm1651AssembleCfg;

void tm1651LoadPlatformDefaultCfg(eTm1651MapType device, stTm1651Cfg *cfg);
const stTm1651IicInterface *tm1651GetPlatformIicInterface(eTm1651MapType device);
bool tm1651PlatformIsValidAssemble(eTm1651MapType device);
uint8_t tm1651PlatformGetLinkId(eTm1651MapType device);

#ifdef __cplusplus
}
#endif

#endif  // TM1651_ASSEMBLY_H
/**************************End of file********************************/
