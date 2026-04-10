/************************************************************************************
* @file     : lis2hh12_assembly.h
* @brief    : LIS2HH12 assembly-time contract shared by core and project port.
***********************************************************************************/
#ifndef LIS2HH12_ASSEMBLY_H
#define LIS2HH12_ASSEMBLY_H

#include <stdint.h>

#include "lis2hh12.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum eLis2hh12TransportType {
    LIS2HH12_TRANSPORT_TYPE_NONE = 0,
    LIS2HH12_TRANSPORT_TYPE_SOFTWARE,
    LIS2HH12_TRANSPORT_TYPE_HARDWARE,
    LIS2HH12_TRANSPORT_TYPE_MAX,
} eLis2hh12TransportType;

typedef eDrvStatus (*lis2hh12IicInitFunc)(uint8_t bus);
typedef eDrvStatus (*lis2hh12IicWriteRegFunc)(uint8_t bus, uint8_t address, const uint8_t *regBuf, uint16_t regLen, const uint8_t *buffer, uint16_t length);
typedef eDrvStatus (*lis2hh12IicReadRegFunc)(uint8_t bus, uint8_t address, const uint8_t *regBuf, uint16_t regLen, uint8_t *buffer, uint16_t length);

typedef struct stLis2hh12IicInterface {
    lis2hh12IicInitFunc init;
    lis2hh12IicWriteRegFunc writeReg;
    lis2hh12IicReadRegFunc readReg;
} stLis2hh12IicInterface;

typedef struct stLis2hh12AssembleCfg {
    eLis2hh12TransportType transportType;
    uint8_t linkId;
} stLis2hh12AssembleCfg;

void lis2hh12LoadPlatformDefaultCfg(eLis2hh12MapType device, stLis2hh12Cfg *cfg);
const stLis2hh12IicInterface *lis2hh12GetPlatformIicInterface(eLis2hh12MapType device);
bool lis2hh12PlatformIsValidAssemble(eLis2hh12MapType device);
uint8_t lis2hh12PlatformGetLinkId(eLis2hh12MapType device);
uint32_t lis2hh12PlatformGetRetryDelayMs(void);
uint32_t lis2hh12PlatformGetResetPollDelayMs(void);
void lis2hh12PlatformDelayMs(uint32_t delayMs);

#ifdef __cplusplus
}
#endif

#endif  // LIS2HH12_ASSEMBLY_H
/**************************End of file********************************/