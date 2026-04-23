/************************************************************************************
* @file     : sdcard_assembly.h
* @brief    : SD card assembly contract declarations.
* @details  : Defines the minimal platform hooks that bind the reusable sdcard
*             module core to a project-specific SDIO or SPI transport.
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef SDCARD_ASSEMBLY_H
#define SDCARD_ASSEMBLY_H

#include <stdbool.h>
#include <stdint.h>

#include "sdcard.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum eSdcardIoctlCmd {
    eSDCARD_IOCTL_GET_INFO = 0,
    eSDCARD_IOCTL_SYNC,
    eSDCARD_IOCTL_TRIM,
} eSdcardIoctlCmd;

typedef eDrvStatus (*sdcardInitFunc)(uint8_t bus, uint32_t timeoutMs);
typedef eDrvStatus (*sdcardGetStatusFunc)(uint8_t bus, bool *isPresent, bool *isWriteProtected);
typedef eDrvStatus (*sdcardReadBlocksFunc)(uint8_t bus, uint32_t startBlock, uint8_t *buffer, uint32_t blockCount);
typedef eDrvStatus (*sdcardWriteBlocksFunc)(uint8_t bus, uint32_t startBlock, const uint8_t *buffer, uint32_t blockCount);
typedef eDrvStatus (*sdcardIoctlFunc)(uint8_t bus, uint32_t command, void *buffer);

typedef struct stSdcardInterface {
    sdcardInitFunc init;
    sdcardGetStatusFunc getStatus;
    sdcardReadBlocksFunc readBlocks;
    sdcardWriteBlocksFunc writeBlocks;
    sdcardIoctlFunc ioctl;
} stSdcardInterface;

void sdcardLoadPlatformDefaultCfg(eSdcardMapType device, stSdcardCfg *cfg);
const stSdcardInterface *sdcardGetPlatformInterface(const stSdcardCfg *cfg);
bool sdcardPlatformIsValidCfg(const stSdcardCfg *cfg);

#ifdef __cplusplus
}
#endif

#endif  // SDCARD_ASSEMBLY_H
/**************************End of file********************************/
