/***********************************************************************************
* @file     : vfs_fatfs.c
* @brief    : FatFs backend adapter for vfs.
* @details  : Maps FatFs mount, stat, directory, and file operations to the
*             generic vfs backend contract.
**********************************************************************************/
#include "vfs_fatfs.h"

#include <string.h>

struct stVfsFatfsFileHandle {
    FIL file;
    bool isOpen;
};

static bool vfsFatfsBuildPath(const stVfsFatfsContext *context, const char *path, char *fatfsPath, uint32_t capacity);
static eVfsResult vfsFatfsMapError(FRESULT result);
static bool vfsFatfsFillNodeInfo(const FILINFO *fatfsInfo, stVfsNodeInfo *info);
static bool vfsFatfsMount(void *backendContext, bool isReadOnly, eVfsResult *error);
static bool vfsFatfsUnmount(void *backendContext, eVfsResult *error);
static bool vfsFatfsFormat(void *backendContext, eVfsResult *error);
static bool vfsFatfsGetSpaceInfo(void *backendContext, stVfsSpaceInfo *info, eVfsResult *error);
static bool vfsFatfsStat(void *backendContext, const char *path, stVfsNodeInfo *info, eVfsResult *error);
static bool vfsFatfsListDir(void *backendContext,
                            const char *path,
                            uint32_t startIndex,
                            stVfsNodeInfo *entries,
                            uint32_t entryCapacity,
                            uint32_t *entryCount,
                            bool *hasMore,
                            eVfsResult *error);
static bool vfsFatfsMkdir(void *backendContext, const char *path, eVfsResult *error);
static bool vfsFatfsRemove(void *backendContext, const char *path, eVfsResult *error);
static bool vfsFatfsRename(void *backendContext, const char *oldPath, const char *newPath, eVfsResult *error);
static bool vfsFatfsFileOpen(void *backendContext, const char *path, uint32_t flags, void *fileContext, eVfsResult *error);
static bool vfsFatfsFileGetSize(void *backendContext, void *fileContext, uint32_t *size, eVfsResult *error);
static bool vfsFatfsFileRead(void *backendContext, void *fileContext, void *buffer, uint32_t bufferSize, uint32_t *actualSize, eVfsResult *error);
static bool vfsFatfsFileWrite(void *backendContext, void *fileContext, const void *data, uint32_t size, uint32_t *actualSize, eVfsResult *error);
static bool vfsFatfsFileClose(void *backendContext, void *fileContext, eVfsResult *error);

static const stVfsBackendOps gVfsFatfsOps = {
    .mount = vfsFatfsMount,
    .unmount = vfsFatfsUnmount,
    .format = vfsFatfsFormat,
    .getSpaceInfo = vfsFatfsGetSpaceInfo,
    .stat = vfsFatfsStat,
    .listDir = vfsFatfsListDir,
    .mkdir = vfsFatfsMkdir,
    .remove = vfsFatfsRemove,
    .rename = vfsFatfsRename,
    .fileOpen = vfsFatfsFileOpen,
    .fileGetSize = vfsFatfsFileGetSize,
    .fileRead = vfsFatfsFileRead,
    .fileWrite = vfsFatfsFileWrite,
    .fileClose = vfsFatfsFileClose,
};

static char gVfsFatfsFileContextSizeCheck[(sizeof(struct stVfsFatfsFileHandle) <= VFS_FILE_CONTEXT_SIZE) ? 1 : -1];

bool vfsFatfsInitContext(stVfsFatfsContext *context, const stVfsFatfsCfg *cfg)
{
    if ((context == NULL) || (cfg == NULL) || (cfg->physicalDrive > 9U)) {
        return false;
    }

    (void)memset(context, 0, sizeof(*context));
    context->cfg = *cfg;
    context->drivePath[0] = (char)('0' + cfg->physicalDrive);
    context->drivePath[1] = ':';
    context->drivePath[2] = '\0';
    context->isConfigured = true;
    return true;
}

const stVfsBackendOps *vfsFatfsGetBackendOps(void)
{
    return &gVfsFatfsOps;
}

static bool vfsFatfsBuildPath(const stVfsFatfsContext *context, const char *path, char *fatfsPath, uint32_t capacity)
{
    uint32_t lDriveLength;
    uint32_t lPathLength;

    if ((context == NULL) || (path == NULL) || (fatfsPath == NULL) || (capacity < 4U)) {
        return false;
    }

    lDriveLength = (uint32_t)strlen(context->drivePath);
    lPathLength = (uint32_t)strlen(path);
    if ((lDriveLength + lPathLength + 1U) > capacity) {
        return false;
    }

    (void)memcpy(fatfsPath, context->drivePath, lDriveLength);
    if ((lPathLength == 1U) && (path[0] == '/')) {
        fatfsPath[lDriveLength] = '/';
        fatfsPath[lDriveLength + 1U] = '\0';
        return true;
    }

    (void)memcpy(&fatfsPath[lDriveLength], path, lPathLength + 1U);
    return true;
}

static eVfsResult vfsFatfsMapError(FRESULT result)
{
    switch (result) {
        case FR_OK:
            return eVFS_OK;
        case FR_NO_FILE:
        case FR_NO_PATH:
        case FR_INVALID_DRIVE:
            return eVFS_NOT_FOUND;
        case FR_EXIST:
            return eVFS_ALREADY_EXISTS;
        case FR_INVALID_NAME:
        case FR_INVALID_PARAMETER:
            return eVFS_INVALID_PARAM;
        case FR_WRITE_PROTECTED:
            return eVFS_READ_ONLY;
        case FR_NOT_READY:
        case FR_NOT_ENABLED:
        case FR_INVALID_OBJECT:
            return eVFS_NOT_READY;
        case FR_NO_FILESYSTEM:
        case FR_MKFS_ABORTED:
            return eVFS_CORRUPT;
        case FR_NOT_ENOUGH_CORE:
            return eVFS_NO_SPACE;
        case FR_TIMEOUT:
        case FR_LOCKED:
        case FR_TOO_MANY_OPEN_FILES:
            return eVFS_BUSY;
        case FR_DENIED:
        case FR_DISK_ERR:
        case FR_INT_ERR:
        default:
            return eVFS_IO;
    }
}

static bool vfsFatfsFillNodeInfo(const FILINFO *fatfsInfo, stVfsNodeInfo *info)
{
    const char *lName;
    uint32_t lNameLength;

    if ((fatfsInfo == NULL) || (info == NULL)) {
        return false;
    }

    (void)memset(info, 0, sizeof(*info));
    info->type = ((fatfsInfo->fattrib & AM_DIR) != 0U) ? eVFS_NODE_DIR : eVFS_NODE_FILE;
    info->size = fatfsInfo->fsize;
#if _USE_LFN
    if ((fatfsInfo->lfname != NULL) && (fatfsInfo->lfname[0] != '\0')) {
        lName = fatfsInfo->lfname;
    } else {
        lName = fatfsInfo->fname;
    }
#else
    lName = fatfsInfo->fname;
#endif
    lNameLength = (uint32_t)strlen(lName);
    if (lNameLength > VFS_ENTRY_NAME_MAX) {
        lNameLength = VFS_ENTRY_NAME_MAX;
    }
    (void)memcpy(info->name, lName, lNameLength);
    info->name[lNameLength] = '\0';
    return true;
}

static bool vfsFatfsMount(void *backendContext, bool isReadOnly, eVfsResult *error)
{
    stVfsFatfsContext *lContext = (stVfsFatfsContext *)backendContext;
    FRESULT lResult;

    (void)isReadOnly;
    if ((lContext == NULL) || !lContext->isConfigured) {
        if (error != NULL) {
            *error = eVFS_INVALID_PARAM;
        }
        return false;
    }

    lResult = f_mount(&lContext->fatfs, lContext->drivePath, 1U);
    if (error != NULL) {
        *error = vfsFatfsMapError(lResult);
    }
    return lResult == FR_OK;
}

static bool vfsFatfsUnmount(void *backendContext, eVfsResult *error)
{
    stVfsFatfsContext *lContext = (stVfsFatfsContext *)backendContext;
    FRESULT lResult;

    if ((lContext == NULL) || !lContext->isConfigured) {
        if (error != NULL) {
            *error = eVFS_INVALID_PARAM;
        }
        return false;
    }

    lResult = f_mount(NULL, lContext->drivePath, 1U);
    if (error != NULL) {
        *error = vfsFatfsMapError(lResult);
    }
    return lResult == FR_OK;
}

static bool vfsFatfsFormat(void *backendContext, eVfsResult *error)
{
    stVfsFatfsContext *lContext = (stVfsFatfsContext *)backendContext;
    FRESULT lResult;

    if ((lContext == NULL) || !lContext->isConfigured) {
        if (error != NULL) {
            *error = eVFS_INVALID_PARAM;
        }
        return false;
    }

    (void)f_mount(NULL, lContext->drivePath, 1U);
    lResult = f_mkfs(lContext->drivePath, 0U, 0U);
    if (lResult == FR_OK) {
        lResult = f_mount(&lContext->fatfs, lContext->drivePath, 1U);
    }
    if (error != NULL) {
        *error = vfsFatfsMapError(lResult);
    }
    return lResult == FR_OK;
}

static bool vfsFatfsGetSpaceInfo(void *backendContext, stVfsSpaceInfo *info, eVfsResult *error)
{
    stVfsFatfsContext *lContext = (stVfsFatfsContext *)backendContext;
    DWORD lFreeClusters;
    FATFS *lFatfs = NULL;
    uint32_t lSectorSize = _MAX_SS;
    uint32_t lTotalClusters;
    uint32_t lFreeSectors;
    FRESULT lResult;

    if ((lContext == NULL) || (info == NULL) || !lContext->isConfigured) {
        if (error != NULL) {
            *error = eVFS_INVALID_PARAM;
        }
        return false;
    }

    lResult = f_getfree(lContext->drivePath, &lFreeClusters, &lFatfs);
    if ((lResult != FR_OK) || (lFatfs == NULL)) {
        if (error != NULL) {
            *error = vfsFatfsMapError(lResult);
        }
        return false;
    }

#if _MAX_SS != _MIN_SS
    lSectorSize = lFatfs->ssize;
#endif
    lTotalClusters = lFatfs->n_fatent - 2UL;
    lFreeSectors = (uint32_t)lFreeClusters * (uint32_t)lFatfs->csize;
    info->totalSize = lTotalClusters * (uint32_t)lFatfs->csize * lSectorSize;
    info->freeSize = lFreeSectors * lSectorSize;
    info->usedSize = (info->totalSize >= info->freeSize) ? (info->totalSize - info->freeSize) : 0U;
    if (error != NULL) {
        *error = eVFS_OK;
    }
    return true;
}

static bool vfsFatfsStat(void *backendContext, const char *path, stVfsNodeInfo *info, eVfsResult *error)
{
    stVfsFatfsContext *lContext = (stVfsFatfsContext *)backendContext;
    FILINFO lFatfsInfo;
    char lFatfsPath[VFS_PATH_MAX + 4U];
    FRESULT lResult;
#if _USE_LFN
    char lLongName[VFS_ENTRY_NAME_MAX + 1U];
#endif

    if ((lContext == NULL) || (path == NULL) || (info == NULL) || !lContext->isConfigured || !vfsFatfsBuildPath(lContext, path, lFatfsPath, sizeof(lFatfsPath))) {
        if (error != NULL) {
            *error = eVFS_INVALID_PARAM;
        }
        return false;
    }

#if _USE_LFN
    (void)memset(lLongName, 0, sizeof(lLongName));
    lFatfsInfo.lfname = lLongName;
    lFatfsInfo.lfsize = sizeof(lLongName);
#endif
    lResult = f_stat(lFatfsPath, &lFatfsInfo);
    if (lResult != FR_OK) {
        if (error != NULL) {
            *error = vfsFatfsMapError(lResult);
        }
        return false;
    }

    if (!vfsFatfsFillNodeInfo(&lFatfsInfo, info)) {
        if (error != NULL) {
            *error = eVFS_IO;
        }
        return false;
    }
    if ((strcmp(path, "/") == 0) && (info->name[0] == '\0')) {
        info->type = eVFS_NODE_DIR;
        info->name[0] = '/';
        info->name[1] = '\0';
    }
    if (error != NULL) {
        *error = eVFS_OK;
    }
    return true;
}

static bool vfsFatfsListDir(void *backendContext,
                            const char *path,
                            uint32_t startIndex,
                            stVfsNodeInfo *entries,
                            uint32_t entryCapacity,
                            uint32_t *entryCount,
                            bool *hasMore,
                            eVfsResult *error)
{
    stVfsFatfsContext *lContext = (stVfsFatfsContext *)backendContext;
    DIR lDir;
    FILINFO lFatfsInfo;
    char lFatfsPath[VFS_PATH_MAX + 4U];
    FRESULT lResult;
    uint32_t lSeenCount = 0U;
    uint32_t lWriteCount = 0U;
#if _USE_LFN
    char lLongName[VFS_ENTRY_NAME_MAX + 1U];
#endif

    if ((lContext == NULL) || (path == NULL) || (entries == NULL) || (entryCapacity == 0U) || (entryCount == NULL) || (hasMore == NULL) || !lContext->isConfigured || !vfsFatfsBuildPath(lContext, path, lFatfsPath, sizeof(lFatfsPath))) {
        if (error != NULL) {
            *error = eVFS_INVALID_PARAM;
        }
        return false;
    }

    *entryCount = 0U;
    *hasMore = false;
    lResult = f_opendir(&lDir, lFatfsPath);
    if (lResult != FR_OK) {
        if (error != NULL) {
            *error = vfsFatfsMapError(lResult);
        }
        return false;
    }

    while (true) {
#if _USE_LFN
        (void)memset(lLongName, 0, sizeof(lLongName));
        lFatfsInfo.lfname = lLongName;
        lFatfsInfo.lfsize = sizeof(lLongName);
#endif
        lResult = f_readdir(&lDir, &lFatfsInfo);
        if (lResult != FR_OK) {
            (void)f_closedir(&lDir);
            if (error != NULL) {
                *error = vfsFatfsMapError(lResult);
            }
            return false;
        }

        if (lFatfsInfo.fname[0] == '\0') {
            break;
        }

        if ((strcmp(lFatfsInfo.fname, ".") == 0) || (strcmp(lFatfsInfo.fname, "..") == 0)) {
            continue;
        }

        if (lSeenCount++ < startIndex) {
            continue;
        }

        if (lWriteCount < entryCapacity) {
            if (!vfsFatfsFillNodeInfo(&lFatfsInfo, &entries[lWriteCount])) {
                (void)f_closedir(&lDir);
                if (error != NULL) {
                    *error = eVFS_IO;
                }
                return false;
            }
            lWriteCount++;
            continue;
        }

        *hasMore = true;
        break;
    }

    (void)f_closedir(&lDir);
    *entryCount = lWriteCount;
    if (error != NULL) {
        *error = eVFS_OK;
    }
    return true;
}

static bool vfsFatfsMkdir(void *backendContext, const char *path, eVfsResult *error)
{
    stVfsFatfsContext *lContext = (stVfsFatfsContext *)backendContext;
    char lFatfsPath[VFS_PATH_MAX + 4U];
    FRESULT lResult;

    if ((lContext == NULL) || (path == NULL) || !lContext->isConfigured || !vfsFatfsBuildPath(lContext, path, lFatfsPath, sizeof(lFatfsPath))) {
        if (error != NULL) {
            *error = eVFS_INVALID_PARAM;
        }
        return false;
    }

    lResult = f_mkdir(lFatfsPath);
    if (error != NULL) {
        *error = vfsFatfsMapError(lResult);
    }
    return lResult == FR_OK;
}

static bool vfsFatfsRemove(void *backendContext, const char *path, eVfsResult *error)
{
    stVfsFatfsContext *lContext = (stVfsFatfsContext *)backendContext;
    char lFatfsPath[VFS_PATH_MAX + 4U];
    FRESULT lResult;

    if ((lContext == NULL) || (path == NULL) || !lContext->isConfigured || !vfsFatfsBuildPath(lContext, path, lFatfsPath, sizeof(lFatfsPath))) {
        if (error != NULL) {
            *error = eVFS_INVALID_PARAM;
        }
        return false;
    }

    lResult = f_unlink(lFatfsPath);
    if (error != NULL) {
        *error = vfsFatfsMapError(lResult);
    }
    return lResult == FR_OK;
}

static bool vfsFatfsRename(void *backendContext, const char *oldPath, const char *newPath, eVfsResult *error)
{
    stVfsFatfsContext *lContext = (stVfsFatfsContext *)backendContext;
    char lOldFatfsPath[VFS_PATH_MAX + 4U];
    char lNewFatfsPath[VFS_PATH_MAX + 4U];
    FRESULT lResult;

    if ((lContext == NULL) || (oldPath == NULL) || (newPath == NULL) || !lContext->isConfigured ||
        !vfsFatfsBuildPath(lContext, oldPath, lOldFatfsPath, sizeof(lOldFatfsPath)) ||
        !vfsFatfsBuildPath(lContext, newPath, lNewFatfsPath, sizeof(lNewFatfsPath))) {
        if (error != NULL) {
            *error = eVFS_INVALID_PARAM;
        }
        return false;
    }

    lResult = f_rename(lOldFatfsPath, lNewFatfsPath);
    if (error != NULL) {
        *error = vfsFatfsMapError(lResult);
    }
    return lResult == FR_OK;
}

static bool vfsFatfsFileOpen(void *backendContext, const char *path, uint32_t flags, void *fileContext, eVfsResult *error)
{
    stVfsFatfsContext *lContext = (stVfsFatfsContext *)backendContext;
    struct stVfsFatfsFileHandle *lHandle = (struct stVfsFatfsFileHandle *)fileContext;
    char lFatfsPath[VFS_PATH_MAX + 4U];
    BYTE lMode = 0U;
    FRESULT lResult;

    if ((lContext == NULL) || (path == NULL) || (lHandle == NULL) || !lContext->isConfigured || !vfsFatfsBuildPath(lContext, path, lFatfsPath, sizeof(lFatfsPath))) {
        if (error != NULL) {
            *error = eVFS_INVALID_PARAM;
        }
        return false;
    }

    (void)memset(lHandle, 0, sizeof(*lHandle));
    if ((flags & VFS_FILE_FLAG_READ) != 0U) {
        lMode |= FA_READ;
    }
    if ((flags & VFS_FILE_FLAG_WRITE) != 0U) {
        lMode |= FA_WRITE;
    }
    if (((flags & VFS_FILE_FLAG_CREATE) != 0U) && ((flags & VFS_FILE_FLAG_TRUNC) != 0U)) {
        lMode |= FA_CREATE_ALWAYS;
    } else if ((flags & VFS_FILE_FLAG_TRUNC) != 0U) {
        lMode |= FA_CREATE_ALWAYS;
    } else if ((flags & VFS_FILE_FLAG_CREATE) != 0U) {
        lMode |= FA_OPEN_ALWAYS;
    }
    if ((flags & VFS_FILE_FLAG_APPEND) != 0U) {
        lMode |= FA_OPEN_ALWAYS;
        lMode |= FA_WRITE;
    }

    lResult = f_open(&lHandle->file, lFatfsPath, lMode);
    if ((lResult == FR_OK) && ((flags & VFS_FILE_FLAG_APPEND) != 0U)) {
        lResult = f_lseek(&lHandle->file, f_size(&lHandle->file));
    }
    if (lResult == FR_OK) {
        lHandle->isOpen = true;
    }
    if (error != NULL) {
        *error = vfsFatfsMapError(lResult);
    }
    return lResult == FR_OK;
}

static bool vfsFatfsFileGetSize(void *backendContext, void *fileContext, uint32_t *size, eVfsResult *error)
{
    struct stVfsFatfsFileHandle *lHandle = (struct stVfsFatfsFileHandle *)fileContext;

    (void)backendContext;
    if ((lHandle == NULL) || (size == NULL) || !lHandle->isOpen) {
        if (error != NULL) {
            *error = eVFS_INVALID_PARAM;
        }
        return false;
    }

    *size = (uint32_t)f_size(&lHandle->file);
    if (error != NULL) {
        *error = eVFS_OK;
    }
    return true;
}

static bool vfsFatfsFileRead(void *backendContext, void *fileContext, void *buffer, uint32_t bufferSize, uint32_t *actualSize, eVfsResult *error)
{
    struct stVfsFatfsFileHandle *lHandle = (struct stVfsFatfsFileHandle *)fileContext;
    UINT lReadSize = 0U;
    FRESULT lResult;

    (void)backendContext;
    if ((lHandle == NULL) || (buffer == NULL) || (actualSize == NULL) || !lHandle->isOpen || (bufferSize > 0xFFFFUL)) {
        if (error != NULL) {
            *error = eVFS_INVALID_PARAM;
        }
        return false;
    }

    lResult = f_read(&lHandle->file, buffer, (UINT)bufferSize, &lReadSize);
    *actualSize = (uint32_t)lReadSize;
    if (error != NULL) {
        *error = vfsFatfsMapError(lResult);
    }
    return lResult == FR_OK;
}

static bool vfsFatfsFileWrite(void *backendContext, void *fileContext, const void *data, uint32_t size, uint32_t *actualSize, eVfsResult *error)
{
    struct stVfsFatfsFileHandle *lHandle = (struct stVfsFatfsFileHandle *)fileContext;
    UINT lWriteSize = 0U;
    FRESULT lResult;

    (void)backendContext;
    if ((lHandle == NULL) || (data == NULL) || (actualSize == NULL) || !lHandle->isOpen || (size > 0xFFFFUL)) {
        if (error != NULL) {
            *error = eVFS_INVALID_PARAM;
        }
        return false;
    }

    lResult = f_write(&lHandle->file, data, (UINT)size, &lWriteSize);
    *actualSize = (uint32_t)lWriteSize;
    if (error != NULL) {
        *error = vfsFatfsMapError(lResult);
    }
    return lResult == FR_OK;
}

static bool vfsFatfsFileClose(void *backendContext, void *fileContext, eVfsResult *error)
{
    struct stVfsFatfsFileHandle *lHandle = (struct stVfsFatfsFileHandle *)fileContext;
    FRESULT lResult;

    (void)backendContext;
    if ((lHandle == NULL) || !lHandle->isOpen) {
        if (error != NULL) {
            *error = eVFS_INVALID_PARAM;
        }
        return false;
    }

    lResult = f_close(&lHandle->file);
    lHandle->isOpen = false;
    if (error != NULL) {
        *error = vfsFatfsMapError(lResult);
    }
    return lResult == FR_OK;
}
/**************************End of file********************************/
