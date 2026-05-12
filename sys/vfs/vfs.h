/************************************************************************************
* @file     : vfs.h
* @brief    : Lightweight virtual filesystem service public interface.
* @details  : Provides a mount-based filesystem facade for project-side storage
*             backends such as littlefs while keeping project bindings in User/.
* @author   : 
* @date     : 
* @version  : 
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef REP_SERVICE_VFS_H
#define REP_SERVICE_VFS_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef VFS_MAX_MOUNTS
#define VFS_MAX_MOUNTS                         4U
#endif

#ifndef VFS_PATH_MAX
#define VFS_PATH_MAX                           128U
#endif

#ifndef VFS_ENTRY_NAME_MAX
#define VFS_ENTRY_NAME_MAX                     255U
#endif

#ifndef VFS_COPY_BUFFER_SIZE
#define VFS_COPY_BUFFER_SIZE                   256U
#endif

#ifndef VFS_FILE_CONTEXT_SIZE
#define VFS_FILE_CONTEXT_SIZE                  640U
#endif

#ifndef VFS_FILE_CONTEXT_WORD_COUNT
#define VFS_FILE_CONTEXT_WORD_COUNT            ((VFS_FILE_CONTEXT_SIZE + sizeof(uint32_t) - 1U) / sizeof(uint32_t))
#endif

#ifndef VFS_LIST_BATCH_SIZE
#define VFS_LIST_BATCH_SIZE                    8U
#endif

#define VFS_FILE_FLAG_READ                     0x01U
#define VFS_FILE_FLAG_WRITE                    0x02U
#define VFS_FILE_FLAG_CREATE                   0x04U
#define VFS_FILE_FLAG_APPEND                   0x08U
#define VFS_FILE_FLAG_TRUNC                    0x10U

typedef enum eVfsState {
    eVFS_STATE_UNINIT = 0,
    eVFS_STATE_READY,
    eVFS_STATE_FAULT,
} eVfsState;

typedef enum eVfsResult {
    eVFS_OK = 0,
    eVFS_INVALID_PARAM,
    eVFS_NOT_READY,
    eVFS_NOT_FOUND,
    eVFS_ALREADY_EXISTS,
    eVFS_NOT_DIR,
    eVFS_IS_DIR,
    eVFS_NO_SPACE,
    eVFS_IO,
    eVFS_CORRUPT,
    eVFS_UNSUPPORTED,
    eVFS_BUSY,
    eVFS_NAME_TOO_LONG,
    eVFS_READ_ONLY,
    eVFS_CROSS_DEVICE,
    eVFS_OVERFLOW,
} eVfsResult;

typedef enum eVfsNodeType {
    eVFS_NODE_FILE = 0,
    eVFS_NODE_DIR,
} eVfsNodeType;

typedef struct stVfsStatus {
    eVfsState state;
    bool isReady;
    eVfsResult lastError;
} stVfsStatus;

typedef struct stVfsSpaceInfo {
    uint32_t totalSize;
    uint32_t usedSize;
    uint32_t freeSize;
} stVfsSpaceInfo;

typedef struct stVfsNodeInfo {
    eVfsNodeType type;
    uint32_t size;
    char name[VFS_ENTRY_NAME_MAX + 1U];
} stVfsNodeInfo;

typedef bool (*pfVfsDirVisitor)(void *context, const stVfsNodeInfo *entry);

typedef struct stVfsBackendOps {
    bool (*mount)(void *backendContext, bool isReadOnly, eVfsResult *error);
    bool (*unmount)(void *backendContext, eVfsResult *error);
    bool (*format)(void *backendContext, eVfsResult *error);
    bool (*getSpaceInfo)(void *backendContext, stVfsSpaceInfo *info, eVfsResult *error);
    bool (*stat)(void *backendContext, const char *path, stVfsNodeInfo *info, eVfsResult *error);
    bool (*listDir)(void *backendContext,
                    const char *path,
                    uint32_t startIndex,
                    stVfsNodeInfo *entries,
                    uint32_t entryCapacity,
                    uint32_t *entryCount,
                    bool *hasMore,
                    eVfsResult *error);
    bool (*mkdir)(void *backendContext, const char *path, eVfsResult *error);
    bool (*remove)(void *backendContext, const char *path, eVfsResult *error);
    bool (*rename)(void *backendContext, const char *oldPath, const char *newPath, eVfsResult *error);
    bool (*fileOpen)(void *backendContext, const char *path, uint32_t flags, void *fileContext, eVfsResult *error);
    bool (*fileGetSize)(void *backendContext, void *fileContext, uint32_t *size, eVfsResult *error);
    bool (*fileRead)(void *backendContext, void *fileContext, void *buffer, uint32_t bufferSize, uint32_t *actualSize, eVfsResult *error);
    bool (*fileWrite)(void *backendContext, void *fileContext, const void *data, uint32_t size, uint32_t *actualSize, eVfsResult *error);
    bool (*fileClose)(void *backendContext, void *fileContext, eVfsResult *error);
} stVfsBackendOps;

typedef struct stVfsMountCfg {
    const char *mountPath;
    const stVfsBackendOps *backendOps;
    void *backendContext;
    bool isAutoMount;
    bool isReadOnly;
} stVfsMountCfg;

struct stVfsMountEntry {
    char mountPath[VFS_PATH_MAX];
    const stVfsBackendOps *backendOps;
    void *backendContext;
    bool isAutoMount;
    bool isReadOnly;
    bool isMounted;
    bool isUsed;
};

struct stVfsBackendFile {
    uint8_t mountIndex;
    bool isOpen;
    uint32_t context[VFS_FILE_CONTEXT_WORD_COUNT];
};

bool vfsInit(void);
bool vfsIsReady(void);
const stVfsStatus *vfsGetStatus(void);
bool vfsRegisterMount(const stVfsMountCfg *cfg);
bool vfsIsMounted(const char *mountPath);
bool vfsMount(const char *mountPath);
bool vfsUnmount(const char *mountPath);
bool vfsFormat(const char *mountPath);
bool vfsNormalizePath(const char *source, char *path, uint32_t capacity);
bool vfsTranslateMountPath(const char *mountPath, const char *localPath, char *absolutePath, uint32_t capacity);
bool vfsGetSpaceInfo(const char *path, stVfsSpaceInfo *info);
bool vfsGetInfo(const char *path, stVfsNodeInfo *info);
bool vfsListDir(const char *path, pfVfsDirVisitor visitor, void *context, uint32_t *entryCount);
bool vfsExists(const char *path);
bool vfsMkdir(const char *path);
bool vfsDelete(const char *path);
bool vfsRename(const char *oldPath, const char *newPath);
bool vfsCopy(const char *sourcePath, const char *targetPath);
bool vfsMove(const char *sourcePath, const char *targetPath);
bool vfsGetFileSize(const char *path, uint32_t *size);
bool vfsReadFile(const char *path, void *buffer, uint32_t bufferSize, uint32_t *actualSize);
bool vfsWriteFile(const char *path, const void *data, uint32_t size);
bool vfsAppendFile(const char *path, const void *data, uint32_t size);

#ifdef __cplusplus
}
#endif

#endif  // REP_SERVICE_VFS_H
/**************************End of file********************************/
