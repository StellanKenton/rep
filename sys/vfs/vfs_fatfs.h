/************************************************************************************
* @file     : vfs_fatfs.h
* @brief    : FatFs backend adapter declarations for vfs.
* @details  : Binds a FatFs logical drive to the generic vfs backend contract
*             without leaking FatFs objects to project services.
***********************************************************************************/
#ifndef REP_SERVICE_VFS_FATFS_H
#define REP_SERVICE_VFS_FATFS_H

#include <stdbool.h>
#include <stdint.h>

#include "vfs.h"
#include "../../lib/fatfs/ff.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct stVfsFatfsCfg {
    uint8_t physicalDrive;
} stVfsFatfsCfg;

typedef struct stVfsFatfsContext {
    FATFS fatfs;
    stVfsFatfsCfg cfg;
    char drivePath[4];
    bool isConfigured;
} stVfsFatfsContext;

bool vfsFatfsInitContext(stVfsFatfsContext *context, const stVfsFatfsCfg *cfg);
const stVfsBackendOps *vfsFatfsGetBackendOps(void);

#ifdef __cplusplus
}
#endif

#endif  // REP_SERVICE_VFS_FATFS_H
/**************************End of file********************************/
