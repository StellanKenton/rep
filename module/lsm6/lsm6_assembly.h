/************************************************************************************
* @file     : lsm6_assembly.h
* @brief    : LSM6 assembly-time contract shared by core and port.
***********************************************************************************/
#ifndef LSM6_ASSEMBLY_H
#define LSM6_ASSEMBLY_H

#include <stdint.h>

#include "lsm6.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum eLsm6TransportType {
    LSM6_TRANSPORT_TYPE_NONE = 0,
    LSM6_TRANSPORT_TYPE_SOFTWARE,
    LSM6_TRANSPORT_TYPE_HARDWARE,
    LSM6_TRANSPORT_TYPE_MAX,
} eLsm6TransportType;

typedef eDrvStatus (*lsm6IicInitFunc)(uint8_t bus);
typedef eDrvStatus (*lsm6IicWriteRegFunc)(uint8_t bus, uint8_t address, const uint8_t *regBuf, uint16_t regLen, const uint8_t *buffer, uint16_t length);
typedef eDrvStatus (*lsm6IicReadRegFunc)(uint8_t bus, uint8_t address, const uint8_t *regBuf, uint16_t regLen, uint8_t *buffer, uint16_t length);

typedef struct stLsm6IicInterface {
    lsm6IicInitFunc init;
    lsm6IicWriteRegFunc writeReg;
    lsm6IicReadRegFunc readReg;
} stLsm6IicInterface;

typedef struct stLsm6AssembleCfg {
    eLsm6TransportType transportType;
    uint8_t linkId;
} stLsm6AssembleCfg;

void lsm6LoadPlatformDefaultCfg(eLsm6MapType device, stLsm6Cfg *cfg);
const stLsm6IicInterface *lsm6GetPlatformIicInterface(eLsm6MapType device);
bool lsm6PlatformIsValidAssemble(eLsm6MapType device);
uint8_t lsm6PlatformGetLinkId(eLsm6MapType device);
uint32_t lsm6PlatformGetResetDelayMs(void);
uint32_t lsm6PlatformGetResetPollDelayMs(void);
void lsm6PlatformDelayMs(uint32_t delayMs);

#ifdef __cplusplus
}
#endif

#endif  // LSM6_ASSEMBLY_H
/**************************End of file********************************/