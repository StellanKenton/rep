/************************************************************************************
* @file     : tm1651_port.h
* @brief    : TM1651 project port-layer declarations.
* @details  : This file keeps the project-level TM1651 binding and convenience
*             wrappers separate from the reusable TM1651 core implementation.
***********************************************************************************/
#ifndef TM1651_PORT_H
#define TM1651_PORT_H

#include <stdbool.h>
#include <stdint.h>

#include "tm1651.h"
#include "tm1651_assembly.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef TM1651_CONSOLE_SUPPORT
#define TM1651_CONSOLE_SUPPORT               1
#endif

typedef stTm1651IicInterface stTm1651PortIicInterface;
typedef stTm1651AssembleCfg stTm1651PortAssembleCfg;

eDrvStatus tm1651PortGetDefAssembleCfg(eTm1651MapType device, stTm1651PortAssembleCfg *cfg);
eDrvStatus tm1651PortGetAssembleCfg(eTm1651MapType device, stTm1651PortAssembleCfg *cfg);
eDrvStatus tm1651PortSetAssembleCfg(eTm1651MapType device, const stTm1651PortAssembleCfg *cfg);
eDrvStatus tm1651PortAssembleSoftIic(stTm1651PortAssembleCfg *cfg, uint8_t iic);
bool tm1651PortIsValidAssembleCfg(const stTm1651PortAssembleCfg *cfg);
bool tm1651PortHasValidIicIf(const stTm1651PortAssembleCfg *cfg);
const stTm1651PortIicInterface *tm1651PortGetIicIf(const stTm1651PortAssembleCfg *cfg);
eDrvStatus tm1651PortInit(void);
bool tm1651PortIsReady(void);
eDrvStatus tm1651PortSetBrightness(uint8_t brightness);
eDrvStatus tm1651PortSetDisplayOn(bool isDisplayOn);
eDrvStatus tm1651PortDisplayDigits(uint8_t dig1, uint8_t dig2, uint8_t dig3, uint8_t dig4);
eDrvStatus tm1651PortClearDisplay(void);
eDrvStatus tm1651PortShowNone(void);
eDrvStatus tm1651PortShowNumber3(uint16_t value);
eDrvStatus tm1651PortShowError(uint16_t value);

#ifdef __cplusplus
}
#endif

#endif  // TM1651_PORT_H
/**************************End of file********************************/
