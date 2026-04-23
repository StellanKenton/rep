/***********************************************************************************
* @file     : vfs.c
* @brief    : Lightweight virtual filesystem service core.
* @details  : Maintains mount registration, path normalization, backend dispatch,
*             and basic cross-mount file copy and move semantics.
* @author   : 
* @date     : 
* @version  : 
* @copyright: Copyright (c) 2050
**********************************************************************************/
#include "vfs.h"

#include <string.h>

#include "../log/log.h"
#include "../rtos/rtos.h"

#define VFS_LOG_TAG "vfs"


static bool vfsLock(void);
static void vfsUnlock(void);
static void vfsSetStatus(eVfsState state, bool isReady, eVfsResult error);
static bool vfsValidateBackendOps(const stVfsBackendOps *backendOps);
static bool vfsCopyText(char *buffer, const char *text, uint32_t capacity);
static bool vfsFlagsRequireWriteAccess(uint32_t flags);
static bool vfsMountAllowsWriteInt(uint32_t mountIndex);
static bool vfsPathMatchesMount(const char *path, const char *mountPath);
static bool vfsFindMountByMountPathInt(const char *mountPath, uint32_t *mountIndex);
static bool vfsFindMountByPathInt(const char *path, uint32_t *mountIndex, char *backendPath, uint32_t backendPathCapacity);
static bool vfsEnsureMountedInt(uint32_t mountIndex);
static bool vfsMountInt(uint32_t mountIndex);
static bool vfsUnmountInt(uint32_t mountIndex);
static bool vfsOpenFileInt(const char *path, uint32_t flags, struct stVfsBackendFile *file);
static void vfsCloseFileInt(struct stVfsBackendFile *file);
static bool vfsCopyFileInt(const char *sourcePath, const char *targetPath);

static stRepRtosMutex gVfsMutex;
static stVfsStatus gVfsStatus = {
    .state = eVFS_STATE_UNINIT,
    .isReady = false,
    .lastError = eVFS_OK,
};
static struct stVfsMountEntry gVfsMounts[VFS_MAX_MOUNTS];
static uint8_t gVfsCopyBuffer[VFS_COPY_BUFFER_SIZE];

static bool vfsLock(void)
{
    return repRtosMutexTake(&gVfsMutex, REP_RTOS_WAIT_FOREVER) == REP_RTOS_STATUS_OK;
}

static void vfsUnlock(void)
{
    (void)repRtosMutexGive(&gVfsMutex);
}

static void vfsSetStatus(eVfsState state, bool isReady, eVfsResult error)
{
    gVfsStatus.state = state;
    gVfsStatus.isReady = isReady;
    gVfsStatus.lastError = error;
}

static bool vfsValidateBackendOps(const stVfsBackendOps *backendOps)
{
    if (backendOps == NULL) {
        return false;
    }

    return (backendOps->mount != NULL) &&
           (backendOps->unmount != NULL) &&
           (backendOps->format != NULL) &&
           (backendOps->getSpaceInfo != NULL) &&
           (backendOps->stat != NULL) &&
           (backendOps->listDir != NULL) &&
           (backendOps->mkdir != NULL) &&
           (backendOps->remove != NULL) &&
           (backendOps->rename != NULL) &&
           (backendOps->fileOpen != NULL) &&
           (backendOps->fileGetSize != NULL) &&
           (backendOps->fileRead != NULL) &&
           (backendOps->fileWrite != NULL) &&
           (backendOps->fileClose != NULL);
}

static bool vfsCopyText(char *buffer, const char *text, uint32_t capacity)
{
    uint32_t lLength;

    if ((buffer == NULL) || (text == NULL) || (capacity == 0U)) {
        return false;
    }

    lLength = (uint32_t)strlen(text);
    if ((lLength + 1U) > capacity) {
        return false;
    }

    (void)memcpy(buffer, text, lLength + 1U);
    return true;
}

static bool vfsFlagsRequireWriteAccess(uint32_t flags)
{
    return ((flags & VFS_FILE_FLAG_WRITE) != 0U) ||
           ((flags & VFS_FILE_FLAG_CREATE) != 0U) ||
           ((flags & VFS_FILE_FLAG_APPEND) != 0U) ||
           ((flags & VFS_FILE_FLAG_TRUNC) != 0U);
}

static bool vfsMountAllowsWriteInt(uint32_t mountIndex)
{
    if ((mountIndex >= VFS_MAX_MOUNTS) || !gVfsMounts[mountIndex].isUsed) {
        vfsSetStatus(eVFS_STATE_FAULT, false, eVFS_INVALID_PARAM);
        return false;
    }

    if (gVfsMounts[mountIndex].isReadOnly) {
        vfsSetStatus(eVFS_STATE_READY, true, eVFS_READ_ONLY);
        return false;
    }

    return true;
}

static bool vfsPathMatchesMount(const char *path, const char *mountPath)
{
    uint32_t lMountPathLength;

    if ((path == NULL) || (mountPath == NULL)) {
        return false;
    }

    lMountPathLength = (uint32_t)strlen(mountPath);
    if (strncmp(path, mountPath, lMountPathLength) != 0) {
        return false;
    }

    return (path[lMountPathLength] == '\0') || (path[lMountPathLength] == '/');
}

static bool vfsFindMountByMountPathInt(const char *mountPath, uint32_t *mountIndex)
{
    uint32_t lIndex;

    if ((mountPath == NULL) || (mountIndex == NULL)) {
        return false;
    }

    for (lIndex = 0U; lIndex < VFS_MAX_MOUNTS; ++lIndex) {
        if (gVfsMounts[lIndex].isUsed && (strcmp(gVfsMounts[lIndex].mountPath, mountPath) == 0)) {
            *mountIndex = lIndex;
            return true;
        }
    }

    return false;
}

static bool vfsFindMountByPathInt(const char *path, uint32_t *mountIndex, char *backendPath, uint32_t backendPathCapacity)
{
    uint32_t lIndex;
    uint32_t lBestIndex = VFS_MAX_MOUNTS;
    uint32_t lBestLength = 0U;
    uint32_t lMountPathLength;

    if ((path == NULL) || (mountIndex == NULL)) {
        return false;
    }

    for (lIndex = 0U; lIndex < VFS_MAX_MOUNTS; ++lIndex) {
        if (!gVfsMounts[lIndex].isUsed) {
            continue;
        }

        if (!vfsPathMatchesMount(path, gVfsMounts[lIndex].mountPath)) {
            continue;
        }

        lMountPathLength = (uint32_t)strlen(gVfsMounts[lIndex].mountPath);
        if (lMountPathLength >= lBestLength) {
            lBestIndex = lIndex;
            lBestLength = lMountPathLength;
        }
    }

    if (lBestIndex >= VFS_MAX_MOUNTS) {
        return false;
    }

    *mountIndex = lBestIndex;
    if (backendPath != NULL) {
        if ((path[lBestLength] == '\0') && !vfsCopyText(backendPath, "/", backendPathCapacity)) {
            return false;
        }

        if ((path[lBestLength] == '/') && !vfsCopyText(backendPath, &path[lBestLength], backendPathCapacity)) {
            return false;
        }
    }

    return true;
}

static bool vfsMountInt(uint32_t mountIndex)
{
    eVfsResult lError = eVFS_OK;

    if ((mountIndex >= VFS_MAX_MOUNTS) || !gVfsMounts[mountIndex].isUsed) {
        vfsSetStatus(eVFS_STATE_FAULT, false, eVFS_INVALID_PARAM);
        return false;
    }

    if (gVfsMounts[mountIndex].isMounted) {
        vfsSetStatus(eVFS_STATE_READY, true, eVFS_OK);
        return true;
    }

    if ((gVfsMounts[mountIndex].backendOps == NULL) || (gVfsMounts[mountIndex].backendOps->mount == NULL)) {
        vfsSetStatus(eVFS_STATE_FAULT, false, eVFS_INVALID_PARAM);
        return false;
    }

    if (!gVfsMounts[mountIndex].backendOps->mount(gVfsMounts[mountIndex].backendContext,
                                                  gVfsMounts[mountIndex].isReadOnly,
                                                  &lError)) {
        vfsSetStatus(eVFS_STATE_READY, true, lError);
        LOG_E(VFS_LOG_TAG, "mount fail path=%s err=%u", gVfsMounts[mountIndex].mountPath, (unsigned)lError);
        return false;
    }

    gVfsMounts[mountIndex].isMounted = true;
    vfsSetStatus(eVFS_STATE_READY, true, eVFS_OK);
    return true;
}

static bool vfsUnmountInt(uint32_t mountIndex)
{
    eVfsResult lError = eVFS_OK;

    if ((mountIndex >= VFS_MAX_MOUNTS) || !gVfsMounts[mountIndex].isUsed) {
        vfsSetStatus(eVFS_STATE_FAULT, false, eVFS_INVALID_PARAM);
        return false;
    }

    if (!gVfsMounts[mountIndex].isMounted) {
        vfsSetStatus(eVFS_STATE_READY, true, eVFS_OK);
        return true;
    }

    if ((gVfsMounts[mountIndex].backendOps == NULL) || (gVfsMounts[mountIndex].backendOps->unmount == NULL)) {
        vfsSetStatus(eVFS_STATE_FAULT, false, eVFS_INVALID_PARAM);
        return false;
    }

    if (!gVfsMounts[mountIndex].backendOps->unmount(gVfsMounts[mountIndex].backendContext, &lError)) {
        vfsSetStatus(eVFS_STATE_READY, true, lError);
        LOG_E(VFS_LOG_TAG, "unmount fail path=%s err=%u", gVfsMounts[mountIndex].mountPath, (unsigned)lError);
        return false;
    }

    gVfsMounts[mountIndex].isMounted = false;
    vfsSetStatus(eVFS_STATE_READY, true, eVFS_OK);
    return true;
}

static bool vfsEnsureMountedInt(uint32_t mountIndex)
{
    if ((mountIndex >= VFS_MAX_MOUNTS) || !gVfsMounts[mountIndex].isUsed) {
        vfsSetStatus(eVFS_STATE_FAULT, false, eVFS_INVALID_PARAM);
        return false;
    }

    if (gVfsMounts[mountIndex].isMounted) {
        return true;
    }

    return vfsMountInt(mountIndex);
}

static bool vfsOpenFileInt(const char *path, uint32_t flags, struct stVfsBackendFile *file)
{
    char lPath[VFS_PATH_MAX];
    char lBackendPath[VFS_PATH_MAX];
    uint32_t lMountIndex;
    eVfsResult lError = eVFS_OK;

    if ((path == NULL) || (file == NULL)) {
        vfsSetStatus(eVFS_STATE_READY, true, eVFS_INVALID_PARAM);
        return false;
    }

    if (!vfsNormalizePath(path, lPath, sizeof(lPath))) {
        vfsSetStatus(eVFS_STATE_READY, true, eVFS_INVALID_PARAM);
        return false;
    }

    if (!vfsFindMountByPathInt(lPath, &lMountIndex, lBackendPath, sizeof(lBackendPath))) {
        vfsSetStatus(eVFS_STATE_READY, true, eVFS_NOT_FOUND);
        return false;
    }

    if (!vfsEnsureMountedInt(lMountIndex)) {
        return false;
    }

    if (vfsFlagsRequireWriteAccess(flags) && !vfsMountAllowsWriteInt(lMountIndex)) {
        return false;
    }

    (void)memset(file, 0, sizeof(*file));
    if (!gVfsMounts[lMountIndex].backendOps->fileOpen(gVfsMounts[lMountIndex].backendContext,
                                                      lBackendPath,
                                                      flags,
                                                      file->context,
                                                      &lError)) {
        vfsSetStatus(eVFS_STATE_READY, true, lError);
        LOG_E(VFS_LOG_TAG, "file open fail path=%s err=%u", lPath, (unsigned)lError);
        return false;
    }

    file->mountIndex = (uint8_t)lMountIndex;
    file->isOpen = true;
    vfsSetStatus(eVFS_STATE_READY, true, eVFS_OK);
    return true;
}

static void vfsCloseFileInt(struct stVfsBackendFile *file)
{
    eVfsResult lError = eVFS_OK;

    if ((file == NULL) || !file->isOpen || (file->mountIndex >= VFS_MAX_MOUNTS) || !gVfsMounts[file->mountIndex].isUsed) {
        return;
    }

    if (!gVfsMounts[file->mountIndex].backendOps->fileClose(gVfsMounts[file->mountIndex].backendContext,
                                                            file->context,
                                                            &lError)) {
        vfsSetStatus(eVFS_STATE_READY, true, lError);
    }

    file->isOpen = false;
}

static bool vfsCopyFileInt(const char *sourcePath, const char *targetPath)
{
    struct stVfsBackendFile lSourceFile;
    struct stVfsBackendFile lTargetFile;
    uint32_t lActualSize;
    uint32_t lWrittenSize;
    eVfsResult lError = eVFS_OK;

    if (!vfsLock()) {
        return false;
    }

    if (!vfsOpenFileInt(sourcePath, VFS_FILE_FLAG_READ, &lSourceFile)) {
        vfsUnlock();
        return false;
    }

    if (!vfsOpenFileInt(targetPath,
                        VFS_FILE_FLAG_WRITE | VFS_FILE_FLAG_CREATE | VFS_FILE_FLAG_TRUNC,
                        &lTargetFile)) {
        vfsCloseFileInt(&lSourceFile);
        vfsUnlock();
        return false;
    }

    while (true) {
        if (!gVfsMounts[lSourceFile.mountIndex].backendOps->fileRead(gVfsMounts[lSourceFile.mountIndex].backendContext,
                                                                     lSourceFile.context,
                                                                     gVfsCopyBuffer,
                                                                     sizeof(gVfsCopyBuffer),
                                                                     &lActualSize,
                                                                     &lError)) {
            vfsSetStatus(eVFS_STATE_READY, true, lError);
            vfsCloseFileInt(&lTargetFile);
            vfsCloseFileInt(&lSourceFile);
            vfsUnlock();
            return false;
        }

        if (lActualSize == 0U) {
            break;
        }

        if (!gVfsMounts[lTargetFile.mountIndex].backendOps->fileWrite(gVfsMounts[lTargetFile.mountIndex].backendContext,
                                                                      lTargetFile.context,
                                                                      gVfsCopyBuffer,
                                                                      lActualSize,
                                                                      &lWrittenSize,
                                                                      &lError) ||
            (lWrittenSize != lActualSize)) {
            vfsSetStatus(eVFS_STATE_READY, true, lError);
            vfsCloseFileInt(&lTargetFile);
            vfsCloseFileInt(&lSourceFile);
            vfsUnlock();
            return false;
        }
    }

    vfsCloseFileInt(&lTargetFile);
    vfsCloseFileInt(&lSourceFile);
    vfsSetStatus(eVFS_STATE_READY, true, eVFS_OK);
    vfsUnlock();
    return true;
}

bool vfsInit(void)
{
    if (gVfsStatus.state != eVFS_STATE_UNINIT) {
        return gVfsStatus.state == eVFS_STATE_READY;
    }

    if (!gVfsMutex.isCreated && (repRtosMutexCreate(&gVfsMutex) != REP_RTOS_STATUS_OK)) {
        vfsSetStatus(eVFS_STATE_FAULT, false, eVFS_IO);
        LOG_E(VFS_LOG_TAG, "mutex create fail");
        return false;
    }

    (void)memset(gVfsMounts, 0, sizeof(gVfsMounts));
    vfsSetStatus(eVFS_STATE_READY, true, eVFS_OK);
    return true;
}

bool vfsIsReady(void)
{
    return (gVfsStatus.state == eVFS_STATE_READY) && gVfsStatus.isReady;
}

const stVfsStatus *vfsGetStatus(void)
{
    return &gVfsStatus;
}

bool vfsRegisterMount(const stVfsMountCfg *cfg)
{
    uint32_t lIndex;
    char lMountPath[VFS_PATH_MAX];

    if ((cfg == NULL) || (cfg->mountPath == NULL) || !vfsValidateBackendOps(cfg->backendOps) || (cfg->backendContext == NULL)) {
        vfsSetStatus(eVFS_STATE_READY, true, eVFS_INVALID_PARAM);
        return false;
    }

    if (!vfsInit() || !vfsLock()) {
        return false;
    }

    if (!vfsNormalizePath(cfg->mountPath, lMountPath, sizeof(lMountPath)) || (strcmp(lMountPath, "/") == 0)) {
        vfsSetStatus(eVFS_STATE_READY, true, eVFS_INVALID_PARAM);
        vfsUnlock();
        return false;
    }

    if (vfsFindMountByMountPathInt(lMountPath, &lIndex)) {
        vfsSetStatus(eVFS_STATE_READY, true, eVFS_ALREADY_EXISTS);
        vfsUnlock();
        return false;
    }

    for (lIndex = 0U; lIndex < VFS_MAX_MOUNTS; ++lIndex) {
        if (!gVfsMounts[lIndex].isUsed) {
            break;
        }
    }

    if (lIndex >= VFS_MAX_MOUNTS) {
        vfsSetStatus(eVFS_STATE_READY, true, eVFS_NO_SPACE);
        vfsUnlock();
        return false;
    }

    (void)memset(&gVfsMounts[lIndex], 0, sizeof(gVfsMounts[lIndex]));
    (void)memcpy(gVfsMounts[lIndex].mountPath, lMountPath, strlen(lMountPath) + 1U);
    gVfsMounts[lIndex].backendOps = cfg->backendOps;
    gVfsMounts[lIndex].backendContext = cfg->backendContext;
    gVfsMounts[lIndex].isAutoMount = cfg->isAutoMount;
    gVfsMounts[lIndex].isReadOnly = cfg->isReadOnly;
    gVfsMounts[lIndex].isUsed = true;

    if (cfg->isAutoMount && !vfsMountInt(lIndex)) {
        (void)memset(&gVfsMounts[lIndex], 0, sizeof(gVfsMounts[lIndex]));
        vfsUnlock();
        return false;
    }

    vfsSetStatus(eVFS_STATE_READY, true, eVFS_OK);
    vfsUnlock();
    return true;
}

bool vfsIsMounted(const char *mountPath)
{
    char lMountPath[VFS_PATH_MAX];
    uint32_t lIndex;
    bool lIsMounted;

    if ((mountPath == NULL) || !vfsInit() || !vfsLock()) {
        return false;
    }

    if (!vfsNormalizePath(mountPath, lMountPath, sizeof(lMountPath)) || !vfsFindMountByMountPathInt(lMountPath, &lIndex)) {
        vfsUnlock();
        return false;
    }

    lIsMounted = gVfsMounts[lIndex].isMounted;
    vfsUnlock();
    return lIsMounted;
}

bool vfsMount(const char *mountPath)
{
    char lMountPath[VFS_PATH_MAX];
    uint32_t lIndex;
    bool lResult;

    if ((mountPath == NULL) || !vfsInit() || !vfsLock()) {
        return false;
    }

    if (!vfsNormalizePath(mountPath, lMountPath, sizeof(lMountPath)) || !vfsFindMountByMountPathInt(lMountPath, &lIndex)) {
        vfsSetStatus(eVFS_STATE_READY, true, eVFS_NOT_FOUND);
        vfsUnlock();
        return false;
    }

    lResult = vfsMountInt(lIndex);
    vfsUnlock();
    return lResult;
}

bool vfsUnmount(const char *mountPath)
{
    char lMountPath[VFS_PATH_MAX];
    uint32_t lIndex;
    bool lResult;

    if ((mountPath == NULL) || !vfsInit() || !vfsLock()) {
        return false;
    }

    if (!vfsNormalizePath(mountPath, lMountPath, sizeof(lMountPath)) || !vfsFindMountByMountPathInt(lMountPath, &lIndex)) {
        vfsSetStatus(eVFS_STATE_READY, true, eVFS_NOT_FOUND);
        vfsUnlock();
        return false;
    }

    lResult = vfsUnmountInt(lIndex);
    vfsUnlock();
    return lResult;
}

bool vfsFormat(const char *mountPath)
{
    char lMountPath[VFS_PATH_MAX];
    uint32_t lIndex;
    eVfsResult lError = eVFS_OK;
    bool lResult;

    if ((mountPath == NULL) || !vfsInit() || !vfsLock()) {
        return false;
    }

    if (!vfsNormalizePath(mountPath, lMountPath, sizeof(lMountPath)) || !vfsFindMountByMountPathInt(lMountPath, &lIndex)) {
        vfsSetStatus(eVFS_STATE_READY, true, eVFS_NOT_FOUND);
        vfsUnlock();
        return false;
    }

    if ((gVfsMounts[lIndex].backendOps == NULL) || (gVfsMounts[lIndex].backendOps->format == NULL)) {
        vfsSetStatus(eVFS_STATE_READY, true, eVFS_UNSUPPORTED);
        vfsUnlock();
        return false;
    }

    if (!vfsMountAllowsWriteInt(lIndex)) {
        vfsUnlock();
        return false;
    }

    lResult = gVfsMounts[lIndex].backendOps->format(gVfsMounts[lIndex].backendContext, &lError);
    if (!lResult) {
        vfsSetStatus(eVFS_STATE_READY, true, lError);
    } else {
        gVfsMounts[lIndex].isMounted = true;
        vfsSetStatus(eVFS_STATE_READY, true, eVFS_OK);
    }

    vfsUnlock();
    return lResult;
}

bool vfsNormalizePath(const char *source, char *path, uint32_t capacity)
{
    const char *lCursor;
    const char *lSegmentStart;
    uint32_t lPathLength = 1U;
    uint32_t lSegmentLength;

    if ((source == NULL) || (path == NULL) || (capacity < 2U) || (source[0] != '/')) {
        return false;
    }

    path[0] = '/';
    path[1] = '\0';
    lCursor = source;
    while (*lCursor != '\0') {
        while (*lCursor == '/') {
            lCursor++;
        }

        if (*lCursor == '\0') {
            break;
        }

        lSegmentStart = lCursor;
        while ((*lCursor != '\0') && (*lCursor != '/')) {
            lCursor++;
        }

        lSegmentLength = (uint32_t)(lCursor - lSegmentStart);
        if ((lSegmentLength == 1U) && (lSegmentStart[0] == '.')) {
            continue;
        }

        if ((lSegmentLength == 2U) && (lSegmentStart[0] == '.') && (lSegmentStart[1] == '.')) {
            if (lPathLength > 1U) {
                while ((lPathLength > 1U) && (path[lPathLength - 1U] != '/')) {
                    lPathLength--;
                }

                if (lPathLength > 1U) {
                    lPathLength--;
                }

                path[lPathLength] = '\0';
            }
            continue;
        }

        if ((lPathLength + lSegmentLength + 1U) >= capacity) {
            return false;
        }

        if (lPathLength > 1U) {
            path[lPathLength] = '/';
            lPathLength++;
        }

        (void)memcpy(&path[lPathLength], lSegmentStart, lSegmentLength);
        lPathLength += lSegmentLength;
        path[lPathLength] = '\0';
    }

    return true;
}

bool vfsTranslateMountPath(const char *mountPath, const char *localPath, char *absolutePath, uint32_t capacity)
{
    char lMountPath[VFS_PATH_MAX];
    char lLocalPath[VFS_PATH_MAX];
    uint32_t lMountLength;

    if ((mountPath == NULL) || (localPath == NULL) || (absolutePath == NULL)) {
        return false;
    }

    if (!vfsNormalizePath(mountPath, lMountPath, sizeof(lMountPath)) || !vfsNormalizePath(localPath, lLocalPath, sizeof(lLocalPath))) {
        return false;
    }

    lMountLength = (uint32_t)strlen(lMountPath);
    if (strcmp(lLocalPath, "/") == 0) {
        return vfsCopyText(absolutePath, lMountPath, capacity);
    }

    if (strcmp(lMountPath, "/") == 0) {
        return vfsCopyText(absolutePath, lLocalPath, capacity);
    }

    if ((lMountLength + (uint32_t)strlen(lLocalPath) + 1U) > capacity) {
        return false;
    }

    (void)memcpy(absolutePath, lMountPath, lMountLength);
    (void)memcpy(&absolutePath[lMountLength], lLocalPath, strlen(lLocalPath) + 1U);
    return true;
}

bool vfsGetSpaceInfo(const char *path, stVfsSpaceInfo *info)
{
    char lPath[VFS_PATH_MAX];
    char lBackendPath[VFS_PATH_MAX];
    uint32_t lMountIndex;
    eVfsResult lError = eVFS_OK;
    bool lResult;

    if ((path == NULL) || (info == NULL) || !vfsInit() || !vfsLock()) {
        return false;
    }

    if (!vfsNormalizePath(path, lPath, sizeof(lPath)) || !vfsFindMountByPathInt(lPath, &lMountIndex, lBackendPath, sizeof(lBackendPath))) {
        vfsSetStatus(eVFS_STATE_READY, true, eVFS_NOT_FOUND);
        vfsUnlock();
        return false;
    }

    if (!vfsEnsureMountedInt(lMountIndex)) {
        vfsUnlock();
        return false;
    }

    lResult = gVfsMounts[lMountIndex].backendOps->getSpaceInfo(gVfsMounts[lMountIndex].backendContext, info, &lError);
    vfsSetStatus(eVFS_STATE_READY, true, lResult ? eVFS_OK : lError);
    vfsUnlock();
    return lResult;
}

bool vfsGetInfo(const char *path, stVfsNodeInfo *info)
{
    char lPath[VFS_PATH_MAX];
    char lBackendPath[VFS_PATH_MAX];
    uint32_t lMountIndex;
    eVfsResult lError = eVFS_OK;
    bool lResult;

    if ((path == NULL) || (info == NULL) || !vfsInit() || !vfsLock()) {
        return false;
    }

    if (!vfsNormalizePath(path, lPath, sizeof(lPath))) {
        vfsSetStatus(eVFS_STATE_READY, true, eVFS_INVALID_PARAM);
        vfsUnlock();
        return false;
    }

    (void)memset(info, 0, sizeof(*info));
    if (strcmp(lPath, "/") == 0) {
        info->type = eVFS_NODE_DIR;
        info->name[0] = '/';
        info->name[1] = '\0';
        vfsSetStatus(eVFS_STATE_READY, true, eVFS_OK);
        vfsUnlock();
        return true;
    }

    if (!vfsFindMountByPathInt(lPath, &lMountIndex, lBackendPath, sizeof(lBackendPath))) {
        vfsSetStatus(eVFS_STATE_READY, true, eVFS_NOT_FOUND);
        vfsUnlock();
        return false;
    }

    if (!vfsEnsureMountedInt(lMountIndex)) {
        vfsUnlock();
        return false;
    }

    lResult = gVfsMounts[lMountIndex].backendOps->stat(gVfsMounts[lMountIndex].backendContext,
                                                       lBackendPath,
                                                       info,
                                                       &lError);
    vfsSetStatus(eVFS_STATE_READY, true, lResult ? eVFS_OK : lError);
    vfsUnlock();
    return lResult;
}

bool vfsListDir(const char *path, pfVfsDirVisitor visitor, void *context, uint32_t *entryCount)
{
    char lPath[VFS_PATH_MAX];
    char lBackendPath[VFS_PATH_MAX];
    stVfsNodeInfo lEntries[VFS_LIST_BATCH_SIZE];
    stVfsNodeInfo lMountEntries[VFS_MAX_MOUNTS];
    uint32_t lMountIndex;
    uint32_t lIndex;
    uint32_t lBatchIndex;
    uint32_t lStartIndex = 0U;
    uint32_t lBatchCount = 0U;
    uint32_t lCount = 0U;
    eVfsResult lError = eVFS_OK;
    bool lHasMore = false;
    bool lResult;

    if ((path == NULL) || !vfsInit() || !vfsLock()) {
        return false;
    }

    if (!vfsNormalizePath(path, lPath, sizeof(lPath))) {
        vfsSetStatus(eVFS_STATE_READY, true, eVFS_INVALID_PARAM);
        vfsUnlock();
        return false;
    }

    if (strcmp(lPath, "/") == 0) {
        for (lIndex = 0U; lIndex < VFS_MAX_MOUNTS; ++lIndex) {
            if (!gVfsMounts[lIndex].isUsed) {
                continue;
            }

            (void)memset(&lMountEntries[lCount], 0, sizeof(lMountEntries[lCount]));
            lMountEntries[lCount].type = eVFS_NODE_DIR;
            if (!vfsCopyText(lMountEntries[lCount].name, &gVfsMounts[lIndex].mountPath[1], sizeof(lMountEntries[lCount].name))) {
                vfsSetStatus(eVFS_STATE_READY, true, eVFS_NAME_TOO_LONG);
                vfsUnlock();
                return false;
            }

            lCount++;
        }

        LOG_D(VFS_LOG_TAG, "list root path=%s mountCount=%lu visitor=%u", lPath, (unsigned long)lCount, (visitor != NULL) ? 1U : 0U);
        for (lIndex = 0U; lIndex < lCount; ++lIndex) {
            LOG_D(VFS_LOG_TAG,
                  "list root entry[%lu] name=%s type=%u",
                  (unsigned long)lIndex,
                  lMountEntries[lIndex].name,
                  (unsigned)lMountEntries[lIndex].type);
        }

        vfsSetStatus(eVFS_STATE_READY, true, eVFS_OK);
        vfsUnlock();

        if (entryCount != NULL) {
            *entryCount = lCount;
        }

        if (visitor != NULL) {
            for (lIndex = 0U; lIndex < lCount; ++lIndex) {
                if (!visitor(context, &lMountEntries[lIndex])) {
                    LOG_D(VFS_LOG_TAG, "list root visitor stop index=%lu name=%s", (unsigned long)lIndex, lMountEntries[lIndex].name);
                    break;
                }
            }
        }

        return true;
    }

    while (true) {
        if (!vfsFindMountByPathInt(lPath, &lMountIndex, lBackendPath, sizeof(lBackendPath))) {
            vfsSetStatus(eVFS_STATE_READY, true, eVFS_NOT_FOUND);
            vfsUnlock();
            return false;
        }

        if (!vfsEnsureMountedInt(lMountIndex)) {
            vfsUnlock();
            return false;
        }

        lResult = gVfsMounts[lMountIndex].backendOps->listDir(gVfsMounts[lMountIndex].backendContext,
                                                              lBackendPath,
                                                              lStartIndex,
                                                              lEntries,
                                                              VFS_LIST_BATCH_SIZE,
                                                              &lBatchCount,
                                                              &lHasMore,
                                                              &lError);
        vfsSetStatus(eVFS_STATE_READY, true, lResult ? eVFS_OK : lError);
        vfsUnlock();
        if (!lResult) {
            return false;
        }

        lCount += lBatchCount;
        if (visitor != NULL) {
            for (lBatchIndex = 0U; lBatchIndex < lBatchCount; ++lBatchIndex) {
                if (!visitor(context, &lEntries[lBatchIndex])) {
                    if (entryCount != NULL) {
                        *entryCount = lCount;
                    }
                    return true;
                }
            }
        }

        if (!lHasMore || (lBatchCount == 0U)) {
            break;
        }

        lStartIndex += lBatchCount;
        if (!vfsLock()) {
            return false;
        }
    }

    if (entryCount != NULL) {
        *entryCount = lCount;
    }

    return true;
}

bool vfsExists(const char *path)
{
    stVfsNodeInfo lInfo;
    return vfsGetInfo(path, &lInfo);
}

bool vfsMkdir(const char *path)
{
    char lPath[VFS_PATH_MAX];
    char lBackendPath[VFS_PATH_MAX];
    uint32_t lMountIndex;
    eVfsResult lError = eVFS_OK;
    bool lResult;

    if ((path == NULL) || !vfsInit() || !vfsLock()) {
        return false;
    }

    if (!vfsNormalizePath(path, lPath, sizeof(lPath)) || (strcmp(lPath, "/") == 0)) {
        vfsSetStatus(eVFS_STATE_READY, true, eVFS_INVALID_PARAM);
        vfsUnlock();
        return false;
    }

    if (!vfsFindMountByPathInt(lPath, &lMountIndex, lBackendPath, sizeof(lBackendPath))) {
        vfsSetStatus(eVFS_STATE_READY, true, eVFS_NOT_FOUND);
        vfsUnlock();
        return false;
    }

    if (!vfsEnsureMountedInt(lMountIndex)) {
        vfsUnlock();
        return false;
    }

    if (!vfsMountAllowsWriteInt(lMountIndex)) {
        vfsUnlock();
        return false;
    }

    lResult = gVfsMounts[lMountIndex].backendOps->mkdir(gVfsMounts[lMountIndex].backendContext, lBackendPath, &lError);
    vfsSetStatus(eVFS_STATE_READY, true, lResult ? eVFS_OK : lError);
    vfsUnlock();
    return lResult;
}

bool vfsDelete(const char *path)
{
    char lPath[VFS_PATH_MAX];
    char lBackendPath[VFS_PATH_MAX];
    uint32_t lMountIndex;
    eVfsResult lError = eVFS_OK;
    bool lResult;

    if ((path == NULL) || !vfsInit() || !vfsLock()) {
        return false;
    }

    if (!vfsNormalizePath(path, lPath, sizeof(lPath)) || (strcmp(lPath, "/") == 0)) {
        vfsSetStatus(eVFS_STATE_READY, true, eVFS_INVALID_PARAM);
        vfsUnlock();
        return false;
    }

    if (!vfsFindMountByPathInt(lPath, &lMountIndex, lBackendPath, sizeof(lBackendPath))) {
        vfsSetStatus(eVFS_STATE_READY, true, eVFS_NOT_FOUND);
        vfsUnlock();
        return false;
    }

    if (!vfsEnsureMountedInt(lMountIndex)) {
        vfsUnlock();
        return false;
    }

    if (!vfsMountAllowsWriteInt(lMountIndex)) {
        vfsUnlock();
        return false;
    }

    lResult = gVfsMounts[lMountIndex].backendOps->remove(gVfsMounts[lMountIndex].backendContext, lBackendPath, &lError);
    vfsSetStatus(eVFS_STATE_READY, true, lResult ? eVFS_OK : lError);
    vfsUnlock();
    return lResult;
}

bool vfsRename(const char *oldPath, const char *newPath)
{
    char lOldPath[VFS_PATH_MAX];
    char lNewPath[VFS_PATH_MAX];
    char lOldBackendPath[VFS_PATH_MAX];
    char lNewBackendPath[VFS_PATH_MAX];
    uint32_t lOldMountIndex;
    uint32_t lNewMountIndex;
    eVfsResult lError = eVFS_OK;
    bool lResult;

    if ((oldPath == NULL) || (newPath == NULL) || !vfsInit() || !vfsLock()) {
        return false;
    }

    if (!vfsNormalizePath(oldPath, lOldPath, sizeof(lOldPath)) || !vfsNormalizePath(newPath, lNewPath, sizeof(lNewPath))) {
        vfsSetStatus(eVFS_STATE_READY, true, eVFS_INVALID_PARAM);
        vfsUnlock();
        return false;
    }

    if ((strcmp(lOldPath, "/") == 0) || (strcmp(lNewPath, "/") == 0)) {
        vfsSetStatus(eVFS_STATE_READY, true, eVFS_INVALID_PARAM);
        vfsUnlock();
        return false;
    }

    if (!vfsFindMountByPathInt(lOldPath, &lOldMountIndex, lOldBackendPath, sizeof(lOldBackendPath)) ||
        !vfsFindMountByPathInt(lNewPath, &lNewMountIndex, lNewBackendPath, sizeof(lNewBackendPath))) {
        vfsSetStatus(eVFS_STATE_READY, true, eVFS_NOT_FOUND);
        vfsUnlock();
        return false;
    }

    if (lOldMountIndex != lNewMountIndex) {
        vfsSetStatus(eVFS_STATE_READY, true, eVFS_CROSS_DEVICE);
        vfsUnlock();
        return false;
    }

    if (!vfsEnsureMountedInt(lOldMountIndex)) {
        vfsUnlock();
        return false;
    }

    if (!vfsMountAllowsWriteInt(lOldMountIndex)) {
        vfsUnlock();
        return false;
    }

    lResult = gVfsMounts[lOldMountIndex].backendOps->rename(gVfsMounts[lOldMountIndex].backendContext,
                                                            lOldBackendPath,
                                                            lNewBackendPath,
                                                            &lError);
    vfsSetStatus(eVFS_STATE_READY, true, lResult ? eVFS_OK : lError);
    vfsUnlock();
    return lResult;
}

bool vfsCopy(const char *sourcePath, const char *targetPath)
{
    stVfsNodeInfo lInfo;

    if ((sourcePath == NULL) || (targetPath == NULL)) {
        vfsSetStatus(eVFS_STATE_READY, true, eVFS_INVALID_PARAM);
        return false;
    }

    if (!vfsGetInfo(sourcePath, &lInfo)) {
        return false;
    }

    if (lInfo.type != eVFS_NODE_FILE) {
        vfsSetStatus(eVFS_STATE_READY, true, eVFS_UNSUPPORTED);
        return false;
    }

    return vfsCopyFileInt(sourcePath, targetPath);
}

bool vfsMove(const char *sourcePath, const char *targetPath)
{
    stVfsNodeInfo lInfo;
    char lSourceNormPath[VFS_PATH_MAX];
    char lTargetNormPath[VFS_PATH_MAX];
    uint32_t lSourceMountIndex;
    uint32_t lTargetMountIndex;

    if ((sourcePath == NULL) || (targetPath == NULL)) {
        vfsSetStatus(eVFS_STATE_READY, true, eVFS_INVALID_PARAM);
        return false;
    }

    if (vfsRename(sourcePath, targetPath)) {
        return true;
    }

    if (gVfsStatus.lastError != eVFS_CROSS_DEVICE) {
        return false;
    }

    if (!vfsGetInfo(sourcePath, &lInfo)) {
        return false;
    }

    if (lInfo.type != eVFS_NODE_FILE) {
        vfsSetStatus(eVFS_STATE_READY, true, eVFS_UNSUPPORTED);
        return false;
    }

    if (!vfsLock()) {
        return false;
    }

    if (!vfsNormalizePath(sourcePath, lSourceNormPath, sizeof(lSourceNormPath)) ||
        !vfsNormalizePath(targetPath, lTargetNormPath, sizeof(lTargetNormPath)) ||
        !vfsFindMountByPathInt(lSourceNormPath, &lSourceMountIndex, NULL, 0U) ||
        !vfsFindMountByPathInt(lTargetNormPath, &lTargetMountIndex, NULL, 0U)) {
        vfsSetStatus(eVFS_STATE_READY, true, eVFS_NOT_FOUND);
        vfsUnlock();
        return false;
    }

    if (!vfsMountAllowsWriteInt(lSourceMountIndex) || !vfsMountAllowsWriteInt(lTargetMountIndex)) {
        vfsUnlock();
        return false;
    }

    vfsUnlock();

    if (!vfsCopyFileInt(sourcePath, targetPath)) {
        return false;
    }

    return vfsDelete(sourcePath);
}

bool vfsGetFileSize(const char *path, uint32_t *size)
{
    stVfsNodeInfo lInfo;

    if ((path == NULL) || (size == NULL)) {
        vfsSetStatus(eVFS_STATE_READY, true, eVFS_INVALID_PARAM);
        return false;
    }

    if (!vfsGetInfo(path, &lInfo) || (lInfo.type != eVFS_NODE_FILE)) {
        if (gVfsStatus.lastError == eVFS_OK) {
            vfsSetStatus(eVFS_STATE_READY, true, eVFS_IS_DIR);
        }
        return false;
    }

    *size = lInfo.size;
    return true;
}

bool vfsReadFile(const char *path, void *buffer, uint32_t bufferSize, uint32_t *actualSize)
{
    struct stVfsBackendFile lFile;
    uint32_t lFileSize = 0U;
    uint32_t lReadSize = 0U;
    uint32_t lTotalRead = 0U;
    eVfsResult lError = eVFS_OK;

    if ((path == NULL) || ((buffer == NULL) && (bufferSize > 0U))) {
        vfsSetStatus(eVFS_STATE_READY, true, eVFS_INVALID_PARAM);
        return false;
    }

    if (!vfsLock()) {
        return false;
    }

    if (!vfsOpenFileInt(path, VFS_FILE_FLAG_READ, &lFile)) {
        vfsUnlock();
        return false;
    }

    if (!gVfsMounts[lFile.mountIndex].backendOps->fileGetSize(gVfsMounts[lFile.mountIndex].backendContext,
                                                              lFile.context,
                                                              &lFileSize,
                                                              &lError)) {
        vfsSetStatus(eVFS_STATE_READY, true, lError);
        vfsCloseFileInt(&lFile);
        vfsUnlock();
        return false;
    }

    if (lFileSize > bufferSize) {
        vfsSetStatus(eVFS_STATE_READY, true, eVFS_NO_SPACE);
        vfsCloseFileInt(&lFile);
        vfsUnlock();
        return false;
    }

    while (lTotalRead < lFileSize) {
        if (!gVfsMounts[lFile.mountIndex].backendOps->fileRead(gVfsMounts[lFile.mountIndex].backendContext,
                                                               lFile.context,
                                                               &((uint8_t *)buffer)[lTotalRead],
                                                               lFileSize - lTotalRead,
                                                               &lReadSize,
                                                               &lError)) {
            vfsSetStatus(eVFS_STATE_READY, true, lError);
            vfsCloseFileInt(&lFile);
            vfsUnlock();
            return false;
        }

        if (lReadSize == 0U) {
            break;
        }

        lTotalRead += lReadSize;
    }

    if (actualSize != NULL) {
        *actualSize = lTotalRead;
    }

    vfsCloseFileInt(&lFile);
    vfsSetStatus(eVFS_STATE_READY, true, eVFS_OK);
    vfsUnlock();
    return lTotalRead == lFileSize;
}

bool vfsWriteFile(const char *path, const void *data, uint32_t size)
{
    struct stVfsBackendFile lFile;
    uint32_t lWriteSize = 0U;
    eVfsResult lError = eVFS_OK;
    bool lResult;

    if ((path == NULL) || ((data == NULL) && (size > 0U))) {
        vfsSetStatus(eVFS_STATE_READY, true, eVFS_INVALID_PARAM);
        return false;
    }

    if (!vfsLock()) {
        return false;
    }

    if (!vfsOpenFileInt(path,
                        VFS_FILE_FLAG_WRITE | VFS_FILE_FLAG_CREATE | VFS_FILE_FLAG_TRUNC,
                        &lFile)) {
        vfsUnlock();
        return false;
    }

    lResult = gVfsMounts[lFile.mountIndex].backendOps->fileWrite(gVfsMounts[lFile.mountIndex].backendContext,
                                                                 lFile.context,
                                                                 data,
                                                                 size,
                                                                 &lWriteSize,
                                                                 &lError);
    vfsCloseFileInt(&lFile);
    vfsSetStatus(eVFS_STATE_READY, true, (lResult && (lWriteSize == size)) ? eVFS_OK : lError);
    vfsUnlock();
    return lResult && (lWriteSize == size);
}

bool vfsAppendFile(const char *path, const void *data, uint32_t size)
{
    struct stVfsBackendFile lFile;
    uint32_t lWriteSize = 0U;
    eVfsResult lError = eVFS_OK;
    bool lResult;

    if ((path == NULL) || ((data == NULL) && (size > 0U))) {
        vfsSetStatus(eVFS_STATE_READY, true, eVFS_INVALID_PARAM);
        return false;
    }

    if (!vfsLock()) {
        return false;
    }

    if (!vfsOpenFileInt(path,
                        VFS_FILE_FLAG_WRITE | VFS_FILE_FLAG_CREATE | VFS_FILE_FLAG_APPEND,
                        &lFile)) {
        vfsUnlock();
        return false;
    }

    lResult = gVfsMounts[lFile.mountIndex].backendOps->fileWrite(gVfsMounts[lFile.mountIndex].backendContext,
                                                                 lFile.context,
                                                                 data,
                                                                 size,
                                                                 &lWriteSize,
                                                                 &lError);
    vfsCloseFileInt(&lFile);
    vfsSetStatus(eVFS_STATE_READY, true, (lResult && (lWriteSize == size)) ? eVFS_OK : lError);
    vfsUnlock();
    return lResult && (lWriteSize == size);
}

/**************************End of file********************************/
