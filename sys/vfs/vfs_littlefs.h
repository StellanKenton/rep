/************************************************************************************
* @file     : vfs_littlefs.h
* @brief    : littlefs backend adapter declarations for vfs.
* @details  : Binds a raw flash-like block device to the generic vfs backend
*             contract without leaking littlefs objects to project services.
* @author   : 
* @date     : 
* @version  : 
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef REP_SERVICE_VFS_LITTLEFS_H
#define REP_SERVICE_VFS_LITTLEFS_H

#include <stdbool.h>
#include <stdint.h>

#include "vfs.h"
#include "../../lib/littlefs/lfs.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef VFS_LITTLEFS_CACHE_SIZE
#define VFS_LITTLEFS_CACHE_SIZE               256U
#endif

#ifndef VFS_LITTLEFS_LOOKAHEAD_SIZE
#define VFS_LITTLEFS_LOOKAHEAD_SIZE           32U
#endif

typedef struct stVfsLittlefsBlockDeviceOps {
    bool (*init)(void *deviceContext);
    bool (*read)(void *deviceContext, uint32_t address, void *buffer, uint32_t size);
    bool (*prog)(void *deviceContext, uint32_t address, const void *buffer, uint32_t size);
    bool (*erase)(void *deviceContext, uint32_t address, uint32_t size);
    bool (*sync)(void *deviceContext);
} stVfsLittlefsBlockDeviceOps;

typedef struct stVfsLittlefsCfg {
    const stVfsLittlefsBlockDeviceOps *blockDeviceOps;
    void *blockDeviceContext;
    uint32_t regionOffset;
    uint32_t regionSizeBytes;
    uint32_t readSize;
    uint32_t progSize;
    uint32_t blockSize;
    uint32_t cacheSize;
    uint32_t lookaheadSize;
    int32_t blockCycles;
} stVfsLittlefsCfg;

typedef struct stVfsLittlefsContext {
    lfs_t lfs;
    struct lfs_config lfsCfg;
    stVfsLittlefsCfg cfg;
    uint8_t readBuffer[VFS_LITTLEFS_CACHE_SIZE];
    uint8_t progBuffer[VFS_LITTLEFS_CACHE_SIZE];
    uint8_t lookaheadBuffer[VFS_LITTLEFS_LOOKAHEAD_SIZE];
    bool isConfigured;
} stVfsLittlefsContext;

bool vfsLittlefsInitContext(stVfsLittlefsContext *context, const stVfsLittlefsCfg *cfg);
const stVfsBackendOps *vfsLittlefsGetBackendOps(void);

#ifdef __cplusplus
}
#endif

#endif  // REP_SERVICE_VFS_LITTLEFS_H
/**************************End of file********************************/
