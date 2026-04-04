/************************************************************************************
* @file     : drvmcuflash.h
* @brief    : Generic MCU internal flash driver abstraction.
* @details  : This module exposes a safe writable-area API for on-chip flash while
*             leaving sector mapping and programming details in the BSP layer.
* @author   : GitHub Copilot
* @date     : 2026-04-02
* @version  : V1.0.0
***********************************************************************************/
#ifndef DRVMCUFLASH_H
#define DRVMCUFLASH_H

#include <stdbool.h>
#include <stdint.h>

#include "rep_config.h"
#include "drvmcuflash_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct stDrvMcuFlashAreaInfo {
    uint32_t startAddress;
    uint32_t size;
} stDrvMcuFlashAreaInfo;

typedef eDrvStatus (*drvMcuFlashBspInitFunc)(void);
typedef eDrvStatus (*drvMcuFlashBspUnlockFunc)(void);
typedef eDrvStatus (*drvMcuFlashBspLockFunc)(void);
typedef eDrvStatus (*drvMcuFlashBspEraseSectorFunc)(uint32_t sectorIndex);
typedef eDrvStatus (*drvMcuFlashBspProgramFunc)(uint32_t address, const uint8_t *buffer, uint32_t length);
typedef eDrvStatus (*drvMcuFlashBspGetSectorInfoFunc)(uint32_t address, uint32_t *sectorIndex, uint32_t *sectorStart, uint32_t *sectorSize);

typedef struct stDrvMcuFlashBspInterface {
    drvMcuFlashBspInitFunc init;
    drvMcuFlashBspUnlockFunc unlock;
    drvMcuFlashBspLockFunc lock;
    drvMcuFlashBspEraseSectorFunc eraseSector;
    drvMcuFlashBspProgramFunc program;
    drvMcuFlashBspGetSectorInfoFunc getSectorInfo;
} stDrvMcuFlashBspInterface;

eDrvStatus drvMcuFlashInit(void);
bool drvMcuFlashIsReady(void);
eDrvStatus drvMcuFlashGetAreaInfo(eDrvMcuFlashAreaMap area, stDrvMcuFlashAreaInfo *info);
eDrvStatus drvMcuFlashRead(eDrvMcuFlashAreaMap area, uint32_t offset, uint8_t *buffer, uint32_t length);
eDrvStatus drvMcuFlashWrite(eDrvMcuFlashAreaMap area, uint32_t offset, const uint8_t *buffer, uint32_t length);
eDrvStatus drvMcuFlashErase(eDrvMcuFlashAreaMap area, uint32_t offset, uint32_t length);

#ifdef __cplusplus
}
#endif

#endif  // DRVMCUFLASH_H
/**************************End of file********************************/
