/***********************************************************************************
* @file     : vfs_littlefs.c
* @brief    : littlefs backend adapter for vfs.
* @details  : Maps littlefs mount, stat, directory, and file operations to the
*             generic vfs backend contract.
* @author   : 
* @date     : 
* @version  : 
* @copyright: Copyright (c) 2050
**********************************************************************************/
#include "vfs_littlefs.h"

#include <string.h>

#include "../log/log.h"

#define VFS_LITTLEFS_LOG_TAG "vfs_lfs"

struct stVfsLittlefsFileHandle {
    lfs_file_t file;
    struct lfs_file_config fileCfg;
    uint8_t fileBuffer[VFS_LITTLEFS_CACHE_SIZE];
    bool isOpen;
};

static int vfsLittlefsRead(const struct lfs_config *cfg, lfs_block_t block, lfs_off_t off, void *buffer, lfs_size_t size);
static int vfsLittlefsProg(const struct lfs_config *cfg, lfs_block_t block, lfs_off_t off, const void *buffer, lfs_size_t size);
static int vfsLittlefsErase(const struct lfs_config *cfg, lfs_block_t block);
static int vfsLittlefsSync(const struct lfs_config *cfg);
static eVfsResult vfsLittlefsMapError(int error);
static uint32_t vfsLittlefsGetAddress(const stVfsLittlefsContext *context, lfs_block_t block, lfs_off_t off);
static bool vfsLittlefsProbeBlank(const stVfsLittlefsContext *context);
static bool vfsLittlefsFillNodeInfo(const struct lfs_info *lfsInfo, stVfsNodeInfo *info);
static bool vfsLittlefsMount(void *backendContext, bool isReadOnly, eVfsResult *error);
static bool vfsLittlefsUnmount(void *backendContext, eVfsResult *error);
static bool vfsLittlefsFormat(void *backendContext, eVfsResult *error);
static bool vfsLittlefsGetSpaceInfo(void *backendContext, stVfsSpaceInfo *info, eVfsResult *error);
static bool vfsLittlefsStat(void *backendContext, const char *path, stVfsNodeInfo *info, eVfsResult *error);
static bool vfsLittlefsListDir(void *backendContext, const char *path, pfVfsDirVisitor visitor, void *visitorContext, uint32_t *entryCount, eVfsResult *error);
static bool vfsLittlefsMkdir(void *backendContext, const char *path, eVfsResult *error);
static bool vfsLittlefsRemove(void *backendContext, const char *path, eVfsResult *error);
static bool vfsLittlefsRename(void *backendContext, const char *oldPath, const char *newPath, eVfsResult *error);
static bool vfsLittlefsFileOpen(void *backendContext, const char *path, uint32_t flags, void *fileContext, eVfsResult *error);
static bool vfsLittlefsFileGetSize(void *backendContext, void *fileContext, uint32_t *size, eVfsResult *error);
static bool vfsLittlefsFileRead(void *backendContext, void *fileContext, void *buffer, uint32_t bufferSize, uint32_t *actualSize, eVfsResult *error);
static bool vfsLittlefsFileWrite(void *backendContext, void *fileContext, const void *data, uint32_t size, uint32_t *actualSize, eVfsResult *error);
static bool vfsLittlefsFileClose(void *backendContext, void *fileContext, eVfsResult *error);

static const stVfsBackendOps gVfsLittlefsOps = {
    .mount = vfsLittlefsMount,
    .unmount = vfsLittlefsUnmount,
    .format = vfsLittlefsFormat,
    .getSpaceInfo = vfsLittlefsGetSpaceInfo,
    .stat = vfsLittlefsStat,
    .listDir = vfsLittlefsListDir,
    .mkdir = vfsLittlefsMkdir,
    .remove = vfsLittlefsRemove,
    .rename = vfsLittlefsRename,
    .fileOpen = vfsLittlefsFileOpen,
    .fileGetSize = vfsLittlefsFileGetSize,
    .fileRead = vfsLittlefsFileRead,
    .fileWrite = vfsLittlefsFileWrite,
    .fileClose = vfsLittlefsFileClose,
};

static char gVfsLittlefsFileContextSizeCheck[(sizeof(struct stVfsLittlefsFileHandle) <= VFS_FILE_CONTEXT_SIZE) ? 1 : -1];

static int vfsLittlefsRead(const struct lfs_config *cfg, lfs_block_t block, lfs_off_t off, void *buffer, lfs_size_t size)
{
    stVfsLittlefsContext *lContext = (stVfsLittlefsContext *)cfg->context;

    if ((lContext == NULL) || (buffer == NULL) || (lContext->cfg.blockDeviceOps == NULL) ||
        (lContext->cfg.blockDeviceOps->read == NULL) ||
        (block >= cfg->block_count) || (off > cfg->block_size) || (size > cfg->block_size) || ((off + size) > cfg->block_size)) {
        return LFS_ERR_INVAL;
    }

    return lContext->cfg.blockDeviceOps->read(lContext->cfg.blockDeviceContext,
                                              vfsLittlefsGetAddress(lContext, block, off),
                                              buffer,
                                              (uint32_t)size) ? LFS_ERR_OK : LFS_ERR_IO;
}

static int vfsLittlefsProg(const struct lfs_config *cfg, lfs_block_t block, lfs_off_t off, const void *buffer, lfs_size_t size)
{
    stVfsLittlefsContext *lContext = (stVfsLittlefsContext *)cfg->context;

    if ((lContext == NULL) || (buffer == NULL) || (lContext->cfg.blockDeviceOps == NULL) ||
        (lContext->cfg.blockDeviceOps->prog == NULL) ||
        (block >= cfg->block_count) || (off > cfg->block_size) || (size > cfg->block_size) || ((off + size) > cfg->block_size)) {
        return LFS_ERR_INVAL;
    }

    return lContext->cfg.blockDeviceOps->prog(lContext->cfg.blockDeviceContext,
                                              vfsLittlefsGetAddress(lContext, block, off),
                                              buffer,
                                              (uint32_t)size) ? LFS_ERR_OK : LFS_ERR_IO;
}

static int vfsLittlefsErase(const struct lfs_config *cfg, lfs_block_t block)
{
    stVfsLittlefsContext *lContext = (stVfsLittlefsContext *)cfg->context;

    if ((lContext == NULL) || (lContext->cfg.blockDeviceOps == NULL) || (lContext->cfg.blockDeviceOps->erase == NULL) || (block >= cfg->block_count)) {
        return LFS_ERR_INVAL;
    }

    return lContext->cfg.blockDeviceOps->erase(lContext->cfg.blockDeviceContext,
                                               vfsLittlefsGetAddress(lContext, block, 0U),
                                               lContext->cfg.blockSize) ? LFS_ERR_OK : LFS_ERR_IO;
}

static int vfsLittlefsSync(const struct lfs_config *cfg)
{
    stVfsLittlefsContext *lContext = (stVfsLittlefsContext *)cfg->context;

    if ((lContext == NULL) || (lContext->cfg.blockDeviceOps == NULL) || (lContext->cfg.blockDeviceOps->sync == NULL)) {
        return LFS_ERR_OK;
    }

    return lContext->cfg.blockDeviceOps->sync(lContext->cfg.blockDeviceContext) ? LFS_ERR_OK : LFS_ERR_IO;
}

static eVfsResult vfsLittlefsMapError(int error)
{
    switch (error) {
        case LFS_ERR_OK:
            return eVFS_OK;

        case LFS_ERR_NOENT:
            return eVFS_NOT_FOUND;

        case LFS_ERR_EXIST:
            return eVFS_ALREADY_EXISTS;

        case LFS_ERR_NOTDIR:
            return eVFS_NOT_DIR;

        case LFS_ERR_ISDIR:
            return eVFS_IS_DIR;

        case LFS_ERR_NOSPC:
            return eVFS_NO_SPACE;

        case LFS_ERR_CORRUPT:
            return eVFS_CORRUPT;

        case LFS_ERR_NAMETOOLONG:
            return eVFS_NAME_TOO_LONG;

        case LFS_ERR_INVAL:
            return eVFS_INVALID_PARAM;

        default:
            return eVFS_IO;
    }
}

static uint32_t vfsLittlefsGetAddress(const stVfsLittlefsContext *context, lfs_block_t block, lfs_off_t off)
{
    return context->cfg.regionOffset + ((uint32_t)block * context->cfg.blockSize) + (uint32_t)off;
}

static bool vfsLittlefsProbeBlank(const stVfsLittlefsContext *context)
{
    uint8_t lProbe[32];
    uint32_t lIndex;

    if ((context == NULL) || (context->cfg.blockDeviceOps == NULL) || (context->cfg.blockDeviceOps->read == NULL)) {
        return false;
    }

    if (!context->cfg.blockDeviceOps->read(context->cfg.blockDeviceContext,
                                           context->cfg.regionOffset,
                                           lProbe,
                                           (uint32_t)sizeof(lProbe))) {
        return false;
    }

    for (lIndex = 0U; lIndex < (uint32_t)sizeof(lProbe); ++lIndex) {
        if (lProbe[lIndex] != 0xFFU) {
            return false;
        }
    }

    return true;
}

static bool vfsLittlefsFillNodeInfo(const struct lfs_info *lfsInfo, stVfsNodeInfo *info)
{
    uint32_t lNameLength;

    if ((lfsInfo == NULL) || (info == NULL)) {
        return false;
    }

    (void)memset(info, 0, sizeof(*info));
    info->type = (lfsInfo->type == LFS_TYPE_DIR) ? eVFS_NODE_DIR : eVFS_NODE_FILE;
    info->size = (uint32_t)lfsInfo->size;
    lNameLength = (uint32_t)strlen(lfsInfo->name);
    if (lNameLength > VFS_ENTRY_NAME_MAX) {
        lNameLength = VFS_ENTRY_NAME_MAX;
    }

    (void)memcpy(info->name, lfsInfo->name, lNameLength);
    info->name[lNameLength] = '\0';
    return true;
}

static bool vfsLittlefsMount(void *backendContext, bool isReadOnly, eVfsResult *error)
{
    stVfsLittlefsContext *lContext = (stVfsLittlefsContext *)backendContext;
    int lResult;

    (void)isReadOnly;

    if ((lContext == NULL) || !lContext->isConfigured || (lContext->cfg.blockDeviceOps == NULL)) {
        if (error != NULL) {
            *error = eVFS_INVALID_PARAM;
        }
        return false;
    }

    if ((lContext->cfg.blockDeviceOps->init != NULL) && !lContext->cfg.blockDeviceOps->init(lContext->cfg.blockDeviceContext)) {
        if (error != NULL) {
            *error = eVFS_IO;
        }
        return false;
    }

    lResult = lfs_mount(&lContext->lfs, &lContext->lfsCfg);
    if (lResult == LFS_ERR_OK) {
        if (error != NULL) {
            *error = eVFS_OK;
        }
        return true;
    }

    if ((lResult != LFS_ERR_CORRUPT) && (lResult != LFS_ERR_INVAL)) {
        if (error != NULL) {
            *error = vfsLittlefsMapError(lResult);
        }
        return false;
    }

    if (vfsLittlefsProbeBlank(lContext)) {
        LOG_I(VFS_LITTLEFS_LOG_TAG, "littlefs region blank, format start");
    } else {
        LOG_W(VFS_LITTLEFS_LOG_TAG, "littlefs mount fail err=%d, format start", lResult);
    }

    lResult = lfs_format(&lContext->lfs, &lContext->lfsCfg);
    if (lResult != LFS_ERR_OK) {
        if (error != NULL) {
            *error = vfsLittlefsMapError(lResult);
        }
        return false;
    }

    lResult = lfs_mount(&lContext->lfs, &lContext->lfsCfg);
    if (error != NULL) {
        *error = vfsLittlefsMapError(lResult);
    }
    return lResult == LFS_ERR_OK;
}

static bool vfsLittlefsUnmount(void *backendContext, eVfsResult *error)
{
    stVfsLittlefsContext *lContext = (stVfsLittlefsContext *)backendContext;
    int lResult;

    if (lContext == NULL) {
        if (error != NULL) {
            *error = eVFS_INVALID_PARAM;
        }
        return false;
    }

    lResult = lfs_unmount(&lContext->lfs);
    if (error != NULL) {
        *error = vfsLittlefsMapError(lResult);
    }
    return (lResult == LFS_ERR_OK) || (lResult == LFS_ERR_INVAL);
}

static bool vfsLittlefsFormat(void *backendContext, eVfsResult *error)
{
    stVfsLittlefsContext *lContext = (stVfsLittlefsContext *)backendContext;
    int lResult;

    if (lContext == NULL) {
        if (error != NULL) {
            *error = eVFS_INVALID_PARAM;
        }
        return false;
    }

    (void)lfs_unmount(&lContext->lfs);
    lResult = lfs_format(&lContext->lfs, &lContext->lfsCfg);
    if (lResult == LFS_ERR_OK) {
        lResult = lfs_mount(&lContext->lfs, &lContext->lfsCfg);
    }

    if (error != NULL) {
        *error = vfsLittlefsMapError(lResult);
    }
    return lResult == LFS_ERR_OK;
}

static bool vfsLittlefsGetSpaceInfo(void *backendContext, stVfsSpaceInfo *info, eVfsResult *error)
{
    stVfsLittlefsContext *lContext = (stVfsLittlefsContext *)backendContext;
    lfs_ssize_t lUsedBlocks;

    if ((lContext == NULL) || (info == NULL)) {
        if (error != NULL) {
            *error = eVFS_INVALID_PARAM;
        }
        return false;
    }

    lUsedBlocks = lfs_fs_size(&lContext->lfs);
    if (lUsedBlocks < 0) {
        if (error != NULL) {
            *error = vfsLittlefsMapError((int)lUsedBlocks);
        }
        return false;
    }

    info->totalSize = lContext->cfg.regionSizeBytes;
    info->usedSize = (uint32_t)lUsedBlocks * lContext->cfg.blockSize;
    if (info->usedSize > info->totalSize) {
        info->usedSize = info->totalSize;
    }
    info->freeSize = info->totalSize - info->usedSize;
    if (error != NULL) {
        *error = eVFS_OK;
    }
    return true;
}

static bool vfsLittlefsStat(void *backendContext, const char *path, stVfsNodeInfo *info, eVfsResult *error)
{
    stVfsLittlefsContext *lContext = (stVfsLittlefsContext *)backendContext;
    struct lfs_info lInfo;
    int lResult;

    if ((lContext == NULL) || (path == NULL) || (info == NULL)) {
        if (error != NULL) {
            *error = eVFS_INVALID_PARAM;
        }
        return false;
    }

    if ((path[0] == '/') && (path[1] == '\0')) {
        (void)memset(info, 0, sizeof(*info));
        info->type = eVFS_NODE_DIR;
        info->name[0] = '/';
        info->name[1] = '\0';
        if (error != NULL) {
            *error = eVFS_OK;
        }
        return true;
    }

    lResult = lfs_stat(&lContext->lfs, path, &lInfo);
    if (error != NULL) {
        *error = vfsLittlefsMapError(lResult);
    }
    return (lResult == LFS_ERR_OK) && vfsLittlefsFillNodeInfo(&lInfo, info);
}

static bool vfsLittlefsListDir(void *backendContext, const char *path, pfVfsDirVisitor visitor, void *visitorContext, uint32_t *entryCount, eVfsResult *error)
{
    stVfsLittlefsContext *lContext = (stVfsLittlefsContext *)backendContext;
    lfs_dir_t lDir;
    struct lfs_info lInfo;
    stVfsNodeInfo lNodeInfo;
    uint32_t lCount = 0U;
    int lResult;

    if ((lContext == NULL) || (path == NULL)) {
        if (error != NULL) {
            *error = eVFS_INVALID_PARAM;
        }
        return false;
    }

    lResult = lfs_dir_open(&lContext->lfs, &lDir, path);
    if (lResult != LFS_ERR_OK) {
        if (error != NULL) {
            *error = vfsLittlefsMapError(lResult);
        }
        return false;
    }

    while (true) {
        lResult = lfs_dir_read(&lContext->lfs, &lDir, &lInfo);
        if (lResult < LFS_ERR_OK) {
            (void)lfs_dir_close(&lContext->lfs, &lDir);
            if (error != NULL) {
                *error = vfsLittlefsMapError(lResult);
            }
            return false;
        }

        if (lResult == LFS_ERR_OK) {
            break;
        }

        if ((strcmp(lInfo.name, ".") == 0) || (strcmp(lInfo.name, "..") == 0)) {
            continue;
        }

        (void)vfsLittlefsFillNodeInfo(&lInfo, &lNodeInfo);
        lCount++;
        if ((visitor != NULL) && !visitor(visitorContext, &lNodeInfo)) {
            break;
        }
    }

    lResult = lfs_dir_close(&lContext->lfs, &lDir);
    if (entryCount != NULL) {
        *entryCount = lCount;
    }

    if (error != NULL) {
        *error = vfsLittlefsMapError(lResult);
    }
    return lResult == LFS_ERR_OK;
}

static bool vfsLittlefsMkdir(void *backendContext, const char *path, eVfsResult *error)
{
    stVfsLittlefsContext *lContext = (stVfsLittlefsContext *)backendContext;
    int lResult;

    if ((lContext == NULL) || (path == NULL)) {
        if (error != NULL) {
            *error = eVFS_INVALID_PARAM;
        }
        return false;
    }

    lResult = lfs_mkdir(&lContext->lfs, path);
    if (error != NULL) {
        *error = vfsLittlefsMapError(lResult);
    }
    return (lResult == LFS_ERR_OK) || (lResult == LFS_ERR_EXIST);
}

static bool vfsLittlefsRemove(void *backendContext, const char *path, eVfsResult *error)
{
    stVfsLittlefsContext *lContext = (stVfsLittlefsContext *)backendContext;
    int lResult;

    if ((lContext == NULL) || (path == NULL)) {
        if (error != NULL) {
            *error = eVFS_INVALID_PARAM;
        }
        return false;
    }

    lResult = lfs_remove(&lContext->lfs, path);
    if (error != NULL) {
        *error = vfsLittlefsMapError(lResult);
    }
    return lResult == LFS_ERR_OK;
}

static bool vfsLittlefsRename(void *backendContext, const char *oldPath, const char *newPath, eVfsResult *error)
{
    stVfsLittlefsContext *lContext = (stVfsLittlefsContext *)backendContext;
    int lResult;

    if ((lContext == NULL) || (oldPath == NULL) || (newPath == NULL)) {
        if (error != NULL) {
            *error = eVFS_INVALID_PARAM;
        }
        return false;
    }

    lResult = lfs_rename(&lContext->lfs, oldPath, newPath);
    if (error != NULL) {
        *error = vfsLittlefsMapError(lResult);
    }
    return lResult == LFS_ERR_OK;
}

static bool vfsLittlefsFileOpen(void *backendContext, const char *path, uint32_t flags, void *fileContext, eVfsResult *error)
{
    stVfsLittlefsContext *lContext = (stVfsLittlefsContext *)backendContext;
    struct stVfsLittlefsFileHandle *lFileHandle = (struct stVfsLittlefsFileHandle *)fileContext;
    int lLfsFlags = 0;
    int lResult;

    if ((lContext == NULL) || (path == NULL) || (lFileHandle == NULL)) {
        if (error != NULL) {
            *error = eVFS_INVALID_PARAM;
        }
        return false;
    }

    if ((flags & VFS_FILE_FLAG_READ) != 0U) {
        lLfsFlags |= LFS_O_RDONLY;
    }

    if ((flags & VFS_FILE_FLAG_WRITE) != 0U) {
        lLfsFlags &= ~LFS_O_RDONLY;
        lLfsFlags |= ((flags & VFS_FILE_FLAG_READ) != 0U) ? LFS_O_RDWR : LFS_O_WRONLY;
    }

    if ((flags & VFS_FILE_FLAG_CREATE) != 0U) {
        lLfsFlags |= LFS_O_CREAT;
    }

    if ((flags & VFS_FILE_FLAG_TRUNC) != 0U) {
        lLfsFlags |= LFS_O_TRUNC;
    }

    if ((flags & VFS_FILE_FLAG_APPEND) != 0U) {
        lLfsFlags |= LFS_O_APPEND;
    }

    (void)memset(lFileHandle, 0, sizeof(*lFileHandle));
    lFileHandle->fileCfg.buffer = lFileHandle->fileBuffer;
    lFileHandle->fileCfg.attrs = NULL;
    lFileHandle->fileCfg.attr_count = 0U;

    lResult = lfs_file_opencfg(&lContext->lfs, &lFileHandle->file, path, lLfsFlags, &lFileHandle->fileCfg);
    if (error != NULL) {
        *error = vfsLittlefsMapError(lResult);
    }

    lFileHandle->isOpen = (lResult == LFS_ERR_OK);
    return lResult == LFS_ERR_OK;
}

static bool vfsLittlefsFileGetSize(void *backendContext, void *fileContext, uint32_t *size, eVfsResult *error)
{
    stVfsLittlefsContext *lContext = (stVfsLittlefsContext *)backendContext;
    struct stVfsLittlefsFileHandle *lFileHandle = (struct stVfsLittlefsFileHandle *)fileContext;
    lfs_soff_t lFileSize;

    if ((lContext == NULL) || (lFileHandle == NULL) || (size == NULL) || !lFileHandle->isOpen) {
        if (error != NULL) {
            *error = eVFS_INVALID_PARAM;
        }
        return false;
    }

    lFileSize = lfs_file_size(&lContext->lfs, &lFileHandle->file);
    if (lFileSize < 0) {
        if (error != NULL) {
            *error = vfsLittlefsMapError((int)lFileSize);
        }
        return false;
    }

    *size = (uint32_t)lFileSize;
    if (error != NULL) {
        *error = eVFS_OK;
    }
    return true;
}

static bool vfsLittlefsFileRead(void *backendContext, void *fileContext, void *buffer, uint32_t bufferSize, uint32_t *actualSize, eVfsResult *error)
{
    stVfsLittlefsContext *lContext = (stVfsLittlefsContext *)backendContext;
    struct stVfsLittlefsFileHandle *lFileHandle = (struct stVfsLittlefsFileHandle *)fileContext;
    lfs_ssize_t lReadSize;

    if ((lContext == NULL) || (lFileHandle == NULL) || ((buffer == NULL) && (bufferSize > 0U)) || (actualSize == NULL) || !lFileHandle->isOpen) {
        if (error != NULL) {
            *error = eVFS_INVALID_PARAM;
        }
        return false;
    }

    lReadSize = lfs_file_read(&lContext->lfs, &lFileHandle->file, buffer, (lfs_size_t)bufferSize);
    if (lReadSize < 0) {
        if (error != NULL) {
            *error = vfsLittlefsMapError((int)lReadSize);
        }
        return false;
    }

    *actualSize = (uint32_t)lReadSize;
    if (error != NULL) {
        *error = eVFS_OK;
    }
    return true;
}

static bool vfsLittlefsFileWrite(void *backendContext, void *fileContext, const void *data, uint32_t size, uint32_t *actualSize, eVfsResult *error)
{
    stVfsLittlefsContext *lContext = (stVfsLittlefsContext *)backendContext;
    struct stVfsLittlefsFileHandle *lFileHandle = (struct stVfsLittlefsFileHandle *)fileContext;
    lfs_ssize_t lWriteSize;

    if ((lContext == NULL) || (lFileHandle == NULL) || ((data == NULL) && (size > 0U)) || (actualSize == NULL) || !lFileHandle->isOpen) {
        if (error != NULL) {
            *error = eVFS_INVALID_PARAM;
        }
        return false;
    }

    lWriteSize = lfs_file_write(&lContext->lfs, &lFileHandle->file, data, (lfs_size_t)size);
    if (lWriteSize < 0) {
        if (error != NULL) {
            *error = vfsLittlefsMapError((int)lWriteSize);
        }
        return false;
    }

    *actualSize = (uint32_t)lWriteSize;
    if (error != NULL) {
        *error = (*actualSize == size) ? eVFS_OK : eVFS_IO;
    }
    return *actualSize == size;
}

static bool vfsLittlefsFileClose(void *backendContext, void *fileContext, eVfsResult *error)
{
    stVfsLittlefsContext *lContext = (stVfsLittlefsContext *)backendContext;
    struct stVfsLittlefsFileHandle *lFileHandle = (struct stVfsLittlefsFileHandle *)fileContext;
    int lResult;

    if ((lContext == NULL) || (lFileHandle == NULL)) {
        if (error != NULL) {
            *error = eVFS_INVALID_PARAM;
        }
        return false;
    }

    if (!lFileHandle->isOpen) {
        if (error != NULL) {
            *error = eVFS_OK;
        }
        return true;
    }

    lResult = lfs_file_close(&lContext->lfs, &lFileHandle->file);
    lFileHandle->isOpen = false;
    if (error != NULL) {
        *error = vfsLittlefsMapError(lResult);
    }
    return lResult == LFS_ERR_OK;
}

bool vfsLittlefsInitContext(stVfsLittlefsContext *context, const stVfsLittlefsCfg *cfg)
{
    if ((context == NULL) || (cfg == NULL) || (cfg->blockDeviceOps == NULL) || (cfg->blockDeviceOps->read == NULL) ||
        (cfg->blockDeviceOps->prog == NULL) || (cfg->blockDeviceOps->erase == NULL) ||
        (cfg->regionSizeBytes == 0U) || (cfg->blockSize == 0U) || ((cfg->regionSizeBytes % cfg->blockSize) != 0U) ||
        (cfg->cacheSize == 0U) || (cfg->cacheSize > VFS_LITTLEFS_CACHE_SIZE) ||
        (cfg->lookaheadSize == 0U) || (cfg->lookaheadSize > VFS_LITTLEFS_LOOKAHEAD_SIZE)) {
        return false;
    }

    (void)memset(context, 0, sizeof(*context));
    context->cfg = *cfg;
    context->lfsCfg.context = context;
    context->lfsCfg.read = vfsLittlefsRead;
    context->lfsCfg.prog = vfsLittlefsProg;
    context->lfsCfg.erase = vfsLittlefsErase;
    context->lfsCfg.sync = vfsLittlefsSync;
    context->lfsCfg.read_size = cfg->readSize;
    context->lfsCfg.prog_size = cfg->progSize;
    context->lfsCfg.block_size = cfg->blockSize;
    context->lfsCfg.block_count = (lfs_size_t)(cfg->regionSizeBytes / cfg->blockSize);
    context->lfsCfg.block_cycles = cfg->blockCycles;
    context->lfsCfg.cache_size = cfg->cacheSize;
    context->lfsCfg.lookahead_size = cfg->lookaheadSize;
    context->lfsCfg.compact_thresh = 0U;
    context->lfsCfg.read_buffer = context->readBuffer;
    context->lfsCfg.prog_buffer = context->progBuffer;
    context->lfsCfg.lookahead_buffer = context->lookaheadBuffer;
    context->lfsCfg.name_max = 0U;
    context->lfsCfg.file_max = 0U;
    context->lfsCfg.attr_max = 0U;
    context->lfsCfg.metadata_max = 0U;
    context->lfsCfg.inline_max = 0U;
    context->isConfigured = true;
    return true;
}

const stVfsBackendOps *vfsLittlefsGetBackendOps(void)
{
    (void)gVfsLittlefsFileContextSizeCheck[0];
    return &gVfsLittlefsOps;
}

/**************************End of file********************************/
