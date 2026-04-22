/***********************************************************************************
* @file     : vfs_debug.c
* @brief    : Console command implementation for a single vfs mount shell.
* @details  : Provides shell-like commands such as mkdir, ls, cd, pwd, cat,
*             tee, mv, rm, and df for one configured vfs mount root.
* @author   : 
* @date     : 
* @version  : 
* @copyright: Copyright (c) 2050
**********************************************************************************/
#include "vfs_debug.h"

#include <stdint.h>
#include <string.h>

#include "vfs.h"
#include "../log/console.h"

#define VFS_DEBUG_CWD_MAX  96U
#define VFS_DEBUG_RW_BUFFER_MAX  256U
#define VFS_DEBUG_REPLY_LINE_MAX  64U

struct stVfsDebugSession {
    uint32_t transport;
    char cwd[VFS_DEBUG_CWD_MAX];
    bool isUsed;
};

struct stVfsDebugLsContext {
    uint32_t transport;
    uint32_t entryCount;
    uint32_t seenEntryHashes[16];
    uint32_t seenEntryHashCount;
    bool hasReplyError;
};

struct stVfsDebugDeleteFirstChildContext {
    char parentPath[VFS_PATH_MAX];
    char childPath[VFS_PATH_MAX];
    bool hasChild;
    bool isPathOverflow;
};

static struct stVfsDebugSession *vfsDebugGetSession(uint32_t transport);
static bool vfsDebugCopyText(char *buffer, const char *text, uint32_t capacity);
static bool vfsDebugResolveLocalPath(const struct stVfsDebugSession *session, const char *input, char *path, uint32_t capacity);
static bool vfsDebugComposeAbsolutePath(const char *localPath, char *absolutePath, uint32_t capacity);
static bool vfsDebugIsPrintableByte(uint8_t value);
static bool vfsDebugBuildWritePayload(int argc, char *argv[], uint32_t startIndex, char *buffer, uint32_t capacity, uint32_t *size);
static bool vfsDebugPathContains(const char *basePath, const char *targetPath);
static bool vfsDebugBuildChildPath(const char *parentPath, const char *name, char *path, uint32_t capacity);
static bool vfsDebugUpdateSessionCwdAfterMove(struct stVfsDebugSession *session, const char *oldPath, const char *newPath);
static uint32_t vfsDebugHashEntry(const stVfsNodeInfo *entry);
static bool vfsDebugLsEntrySeen(struct stVfsDebugLsContext *context, const stVfsNodeInfo *entry);
static bool vfsDebugDeletePathRecursive(const char *absolutePath);
static eConsoleCommandResult vfsDebugReplyFileContent(uint32_t transport, const uint8_t *data, uint32_t size);
static eConsoleCommandResult vfsDebugReplyPathError(uint32_t transport);
static bool vfsDebugLsVisitor(void *context, const stVfsNodeInfo *entry);
static bool vfsDebugDeleteFindFirstChildVisitor(void *context, const stVfsNodeInfo *entry);
static eConsoleCommandResult vfsDebugConsoleMkdirHandler(uint32_t transport, int argc, char *argv[]);
static eConsoleCommandResult vfsDebugConsoleLsHandler(uint32_t transport, int argc, char *argv[]);
static eConsoleCommandResult vfsDebugConsoleCdHandler(uint32_t transport, int argc, char *argv[]);
static eConsoleCommandResult vfsDebugConsolePwdHandler(uint32_t transport, int argc, char *argv[]);
static eConsoleCommandResult vfsDebugConsoleCatHandler(uint32_t transport, int argc, char *argv[]);
static eConsoleCommandResult vfsDebugConsoleTeeHandler(uint32_t transport, int argc, char *argv[]);
static eConsoleCommandResult vfsDebugConsoleMvHandler(uint32_t transport, int argc, char *argv[]);
static eConsoleCommandResult vfsDebugConsoleRmHandler(uint32_t transport, int argc, char *argv[]);
static eConsoleCommandResult vfsDebugConsoleDfHandler(uint32_t transport, int argc, char *argv[]);

static struct stVfsDebugSession gVfsDebugSessions[CONSOLE_MAX_SESSIONS];
static stVfsNodeInfo gVfsDebugNodeInfo;
static char gVfsDebugRootPath[VFS_PATH_MAX];
static char gVfsDebugPathBuffer[VFS_PATH_MAX];
static uint8_t gVfsDebugRwBuffer[VFS_DEBUG_RW_BUFFER_MAX];
static char gVfsDebugReplyLine[VFS_DEBUG_REPLY_LINE_MAX + 1U];
static bool gVfsDebugIsRegistered = false;

static const stConsoleCommand gVfsDebugMkdirConsoleCommand = {
    .commandName = "mkdir",
    .helpText = "mkdir <path> - create a directory in the current vfs mount",
    .ownerTag = "vfs",
    .handler = vfsDebugConsoleMkdirHandler,
};

static const stConsoleCommand gVfsDebugLsConsoleCommand = {
    .commandName = "ls",
    .helpText = "ls [path] - list files and directories in the current vfs mount",
    .ownerTag = "vfs",
    .handler = vfsDebugConsoleLsHandler,
};

static const stConsoleCommand gVfsDebugCdConsoleCommand = {
    .commandName = "cd",
    .helpText = "cd [path] - change current directory in the current vfs mount",
    .ownerTag = "vfs",
    .handler = vfsDebugConsoleCdHandler,
};

static const stConsoleCommand gVfsDebugPwdConsoleCommand = {
    .commandName = "pwd",
    .helpText = "pwd - show current directory in the current vfs mount",
    .ownerTag = "vfs",
    .handler = vfsDebugConsolePwdHandler,
};

static const stConsoleCommand gVfsDebugCatConsoleCommand = {
    .commandName = "cat",
    .helpText = "cat <path> - read file content from the current vfs mount",
    .ownerTag = "vfs",
    .handler = vfsDebugConsoleCatHandler,
};

static const stConsoleCommand gVfsDebugTeeConsoleCommand = {
    .commandName = "tee",
    .helpText = "tee [-a] <path> [text...] - write or append file content in the current vfs mount",
    .ownerTag = "vfs",
    .handler = vfsDebugConsoleTeeHandler,
};

static const stConsoleCommand gVfsDebugMvConsoleCommand = {
    .commandName = "mv",
    .helpText = "mv <source> <target> - rename or move a file or directory in the current vfs mount",
    .ownerTag = "vfs",
    .handler = vfsDebugConsoleMvHandler,
};

static const stConsoleCommand gVfsDebugRmConsoleCommand = {
    .commandName = "rm",
    .helpText = "rm <path> - delete a file or directory in the current vfs mount",
    .ownerTag = "vfs",
    .handler = vfsDebugConsoleRmHandler,
};

static const stConsoleCommand gVfsDebugDfConsoleCommand = {
    .commandName = "df",
    .helpText = "df - show used and free space for the current vfs mount",
    .ownerTag = "vfs",
    .handler = vfsDebugConsoleDfHandler,
};

static struct stVfsDebugSession *vfsDebugGetSession(uint32_t transport)
{
    uint32_t lIndex;
    struct stVfsDebugSession *lFreeSession = NULL;

    for (lIndex = 0U; lIndex < CONSOLE_MAX_SESSIONS; ++lIndex) {
        if (gVfsDebugSessions[lIndex].isUsed && (gVfsDebugSessions[lIndex].transport == transport)) {
            return &gVfsDebugSessions[lIndex];
        }

        if (!gVfsDebugSessions[lIndex].isUsed && (lFreeSession == NULL)) {
            lFreeSession = &gVfsDebugSessions[lIndex];
        }
    }

    if (lFreeSession == NULL) {
        return NULL;
    }

    (void)memset(lFreeSession, 0, sizeof(*lFreeSession));
    lFreeSession->transport = transport;
    lFreeSession->isUsed = true;
    lFreeSession->cwd[0] = '/';
    lFreeSession->cwd[1] = '\0';
    return lFreeSession;
}

static bool vfsDebugCopyText(char *buffer, const char *text, uint32_t capacity)
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

static bool vfsDebugResolveLocalPath(const struct stVfsDebugSession *session, const char *input, char *path, uint32_t capacity)
{
    char lTemp[VFS_DEBUG_CWD_MAX * 2U];
    uint32_t lBaseLength;
    uint32_t lInputLength;

    if ((session == NULL) || (path == NULL) || (capacity < 2U)) {
        return false;
    }

    if ((input == NULL) || (input[0] == '\0')) {
        return vfsNormalizePath(session->cwd, path, capacity);
    }

    if (input[0] == '/') {
        return vfsNormalizePath(input, path, capacity);
    }

    lBaseLength = (uint32_t)strlen(session->cwd);
    lInputLength = (uint32_t)strlen(input);
    if ((lBaseLength + lInputLength + 2U) > (uint32_t)sizeof(lTemp)) {
        return false;
    }

    (void)memcpy(lTemp, session->cwd, lBaseLength);
    if ((lBaseLength > 1U) && (lTemp[lBaseLength - 1U] != '/')) {
        lTemp[lBaseLength] = '/';
        lBaseLength++;
    }

    (void)memcpy(&lTemp[lBaseLength], input, lInputLength);
    lTemp[lBaseLength + lInputLength] = '\0';
    return vfsNormalizePath(lTemp, path, capacity);
}

static bool vfsDebugComposeAbsolutePath(const char *localPath, char *absolutePath, uint32_t capacity)
{
    return vfsTranslateMountPath(gVfsDebugRootPath, localPath, absolutePath, capacity);
}

static bool vfsDebugIsPrintableByte(uint8_t value)
{
    return (value >= 32U) && (value <= 126U);
}

static bool vfsDebugBuildWritePayload(int argc, char *argv[], uint32_t startIndex, char *buffer, uint32_t capacity, uint32_t *size)
{
    uint32_t lOffset = 0U;
    uint32_t lIndex;
    uint32_t lPartLength;

    if ((argv == NULL) || (buffer == NULL) || (capacity == 0U) || (size == NULL) || (startIndex > (uint32_t)argc)) {
        return false;
    }

    for (lIndex = startIndex; lIndex < (uint32_t)argc; ++lIndex) {
        if (lOffset >= capacity) {
            return false;
        }

        if (lIndex > startIndex) {
            buffer[lOffset] = ' ';
            lOffset++;
            if (lOffset > capacity) {
                return false;
            }
        }

        lPartLength = (uint32_t)strlen(argv[lIndex]);
        if ((lOffset + lPartLength) > capacity) {
            return false;
        }

        (void)memcpy(&buffer[lOffset], argv[lIndex], lPartLength);
        lOffset += lPartLength;
    }

    *size = lOffset;
    return true;
}

static bool vfsDebugPathContains(const char *basePath, const char *targetPath)
{
    uint32_t lBaseLength;

    if ((basePath == NULL) || (targetPath == NULL)) {
        return false;
    }

    if ((basePath[0] == '/') && (basePath[1] == '\0')) {
        return true;
    }

    lBaseLength = (uint32_t)strlen(basePath);
    if (strncmp(basePath, targetPath, lBaseLength) != 0) {
        return false;
    }

    return (targetPath[lBaseLength] == '\0') || (targetPath[lBaseLength] == '/');
}

static bool vfsDebugBuildChildPath(const char *parentPath, const char *name, char *path, uint32_t capacity)
{
    uint32_t lParentLength;
    uint32_t lNameLength;

    if ((parentPath == NULL) || (name == NULL) || (path == NULL) || (capacity == 0U)) {
        return false;
    }

    lParentLength = (uint32_t)strlen(parentPath);
    lNameLength = (uint32_t)strlen(name);
    if ((lParentLength + lNameLength + 2U) > capacity) {
        return false;
    }

    (void)memcpy(path, parentPath, lParentLength);
    if ((lParentLength > 1U) && (path[lParentLength - 1U] != '/')) {
        path[lParentLength] = '/';
        lParentLength++;
    }

    (void)memcpy(&path[lParentLength], name, lNameLength);
    path[lParentLength + lNameLength] = '\0';
    return true;
}

static bool vfsDebugUpdateSessionCwdAfterMove(struct stVfsDebugSession *session, const char *oldPath, const char *newPath)
{
    uint32_t lOldPathLength;
    uint32_t lNewPathLength;
    uint32_t lSuffixLength;

    if ((session == NULL) || (oldPath == NULL) || (newPath == NULL)) {
        return false;
    }

    if (!vfsDebugPathContains(oldPath, session->cwd)) {
        return true;
    }

    lOldPathLength = (uint32_t)strlen(oldPath);
    lNewPathLength = (uint32_t)strlen(newPath);
    lSuffixLength = (uint32_t)strlen(&session->cwd[lOldPathLength]);
    if ((lNewPathLength + lSuffixLength + 1U) > sizeof(session->cwd)) {
        session->cwd[0] = '/';
        session->cwd[1] = '\0';
        return false;
    }

    (void)memmove(&session->cwd[lNewPathLength], &session->cwd[lOldPathLength], lSuffixLength + 1U);
    (void)memcpy(session->cwd, newPath, lNewPathLength);
    return true;
}

static uint32_t vfsDebugHashEntry(const stVfsNodeInfo *entry)
{
    const uint8_t *lNameCursor;
    uint32_t lHash = 2166136261UL;

    if (entry == NULL) {
        return 0U;
    }

    lHash ^= (uint32_t)entry->type;
    lHash *= 16777619UL;
    lNameCursor = (const uint8_t *)entry->name;
    while (*lNameCursor != 0U) {
        lHash ^= (uint32_t)(*lNameCursor);
        lHash *= 16777619UL;
        lNameCursor++;
    }

    return lHash;
}

static bool vfsDebugLsEntrySeen(struct stVfsDebugLsContext *context, const stVfsNodeInfo *entry)
{
    uint32_t lEntryHash;
    uint32_t lIndex;

    if ((context == NULL) || (entry == NULL)) {
        return true;
    }

    lEntryHash = vfsDebugHashEntry(entry);
    for (lIndex = 0U; lIndex < context->seenEntryHashCount; ++lIndex) {
        if (context->seenEntryHashes[lIndex] == lEntryHash) {
            return true;
        }
    }

    if (context->seenEntryHashCount < (uint32_t)(sizeof(context->seenEntryHashes) / sizeof(context->seenEntryHashes[0]))) {
        context->seenEntryHashes[context->seenEntryHashCount] = lEntryHash;
        context->seenEntryHashCount++;
    }

    return false;
}

static bool vfsDebugDeleteFindFirstChildVisitor(void *context, const stVfsNodeInfo *entry)
{
    struct stVfsDebugDeleteFirstChildContext *lDeleteContext = (struct stVfsDebugDeleteFirstChildContext *)context;

    if ((lDeleteContext == NULL) || (entry == NULL)) {
        return false;
    }

    if (!vfsDebugBuildChildPath(lDeleteContext->parentPath,
                                entry->name,
                                lDeleteContext->childPath,
                                sizeof(lDeleteContext->childPath))) {
        lDeleteContext->isPathOverflow = true;
        return false;
    }

    lDeleteContext->hasChild = true;
    return false;
}

static bool vfsDebugDeletePathRecursive(const char *absolutePath)
{
    struct stVfsDebugDeleteFirstChildContext lDeleteContext;
    stVfsNodeInfo lInfo;

    if ((absolutePath == NULL) || !vfsGetInfo(absolutePath, &lInfo)) {
        return false;
    }

    if (lInfo.type == eVFS_NODE_DIR) {
        while (true) {
            (void)memset(&lDeleteContext, 0, sizeof(lDeleteContext));
            if (!vfsDebugCopyText(lDeleteContext.parentPath, absolutePath, sizeof(lDeleteContext.parentPath))) {
                return false;
            }

            if (!vfsListDir(absolutePath, vfsDebugDeleteFindFirstChildVisitor, &lDeleteContext, NULL)) {
                return false;
            }

            if (lDeleteContext.isPathOverflow) {
                return false;
            }

            if (!lDeleteContext.hasChild) {
                break;
            }

            if (!vfsDebugDeletePathRecursive(lDeleteContext.childPath)) {
                return false;
            }
        }
    }

    return vfsDelete(absolutePath);
}

static eConsoleCommandResult vfsDebugReplyFileContent(uint32_t transport, const uint8_t *data, uint32_t size)
{
    uint32_t lDataIndex;
    uint32_t lLineLength = 0U;

    if ((data == NULL) && (size > 0U)) {
        return CONSOLE_COMMAND_RESULT_ERROR;
    }

    if (size == 0U) {
        return (logConsoleReply(transport, "empty") > 0) ? CONSOLE_COMMAND_RESULT_OK : CONSOLE_COMMAND_RESULT_ERROR;
    }

    for (lDataIndex = 0U; lDataIndex < size; ++lDataIndex) {
        if (vfsDebugIsPrintableByte(data[lDataIndex])) {
            if (lLineLength >= VFS_DEBUG_REPLY_LINE_MAX) {
                gVfsDebugReplyLine[lLineLength] = '\0';
                if (logConsoleReply(transport, "%s", gVfsDebugReplyLine) <= 0) {
                    return CONSOLE_COMMAND_RESULT_ERROR;
                }
                lLineLength = 0U;
            }

            gVfsDebugReplyLine[lLineLength] = (char)data[lDataIndex];
            lLineLength++;
            continue;
        }

        if (data[lDataIndex] == '\n') {
            if (lLineLength >= VFS_DEBUG_REPLY_LINE_MAX) {
                gVfsDebugReplyLine[lLineLength] = '\0';
                if (logConsoleReply(transport, "%s", gVfsDebugReplyLine) <= 0) {
                    return CONSOLE_COMMAND_RESULT_ERROR;
                }
                lLineLength = 0U;
            }

            gVfsDebugReplyLine[lLineLength++] = '\\';
            gVfsDebugReplyLine[lLineLength++] = 'n';
            continue;
        }

        if (data[lDataIndex] == '\r') {
            if (lLineLength >= (VFS_DEBUG_REPLY_LINE_MAX - 1U)) {
                gVfsDebugReplyLine[lLineLength] = '\0';
                if (logConsoleReply(transport, "%s", gVfsDebugReplyLine) <= 0) {
                    return CONSOLE_COMMAND_RESULT_ERROR;
                }
                lLineLength = 0U;
            }

            gVfsDebugReplyLine[lLineLength++] = '\\';
            gVfsDebugReplyLine[lLineLength++] = 'r';
            continue;
        }

        if (data[lDataIndex] == '\t') {
            if (lLineLength >= (VFS_DEBUG_REPLY_LINE_MAX - 1U)) {
                gVfsDebugReplyLine[lLineLength] = '\0';
                if (logConsoleReply(transport, "%s", gVfsDebugReplyLine) <= 0) {
                    return CONSOLE_COMMAND_RESULT_ERROR;
                }
                lLineLength = 0U;
            }

            gVfsDebugReplyLine[lLineLength++] = '\\';
            gVfsDebugReplyLine[lLineLength++] = 't';
            continue;
        }

        if (lLineLength >= (VFS_DEBUG_REPLY_LINE_MAX - 4U)) {
            gVfsDebugReplyLine[lLineLength] = '\0';
            if (logConsoleReply(transport, "%s", gVfsDebugReplyLine) <= 0) {
                return CONSOLE_COMMAND_RESULT_ERROR;
            }
            lLineLength = 0U;
        }

        gVfsDebugReplyLine[lLineLength + 0U] = '\\';
        gVfsDebugReplyLine[lLineLength + 1U] = 'x';
        gVfsDebugReplyLine[lLineLength + 2U] = "0123456789ABCDEF"[(data[lDataIndex] >> 4) & 0x0FU];
        gVfsDebugReplyLine[lLineLength + 3U] = "0123456789ABCDEF"[data[lDataIndex] & 0x0FU];
        lLineLength += 4U;
    }

    if (lLineLength > 0U) {
        gVfsDebugReplyLine[lLineLength] = '\0';
        if (logConsoleReply(transport, "%s", gVfsDebugReplyLine) <= 0) {
            return CONSOLE_COMMAND_RESULT_ERROR;
        }
    }

    return CONSOLE_COMMAND_RESULT_OK;
}

static eConsoleCommandResult vfsDebugReplyPathError(uint32_t transport)
{
    return (logConsoleReply(transport, "ERROR: invalid path") > 0) ? CONSOLE_COMMAND_RESULT_OK : CONSOLE_COMMAND_RESULT_ERROR;
}

static bool vfsDebugLsVisitor(void *context, const stVfsNodeInfo *entry)
{
    struct stVfsDebugLsContext *lContext = (struct stVfsDebugLsContext *)context;
    int32_t lReplyResult;

    if ((lContext == NULL) || (entry == NULL)) {
        return false;
    }

    if (vfsDebugLsEntrySeen(lContext, entry)) {
        return true;
    }

    if (entry->type == eVFS_NODE_DIR) {
        lReplyResult = logConsoleReply(lContext->transport, "dir  %s/", entry->name);
    } else {
        lReplyResult = logConsoleReply(lContext->transport, "file %lu %s", (unsigned long)entry->size, entry->name);
    }

    if (lReplyResult <= 0) {
        lContext->hasReplyError = true;
        return false;
    }

    lContext->entryCount++;
    return true;
}

static eConsoleCommandResult vfsDebugConsoleMkdirHandler(uint32_t transport, int argc, char *argv[])
{
    struct stVfsDebugSession *lSession;
    char lLocalPath[VFS_DEBUG_CWD_MAX];

    if (argc != 2) {
        return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
    }

    lSession = vfsDebugGetSession(transport);
    if ((lSession == NULL) || !vfsDebugResolveLocalPath(lSession, argv[1], lLocalPath, sizeof(lLocalPath)) ||
        !vfsDebugComposeAbsolutePath(lLocalPath, gVfsDebugPathBuffer, sizeof(gVfsDebugPathBuffer))) {
        return vfsDebugReplyPathError(transport);
    }

    if (!vfsMkdir(gVfsDebugPathBuffer)) {
        if (logConsoleReply(transport, "ERROR: mkdir fail path=%s err=%u", lLocalPath, (unsigned)vfsGetStatus()->lastError) <= 0) {
            return CONSOLE_COMMAND_RESULT_ERROR;
        }
        return CONSOLE_COMMAND_RESULT_OK;
    }

    return (logConsoleReply(transport, "mkdir ok %s", lLocalPath) > 0) ? CONSOLE_COMMAND_RESULT_OK : CONSOLE_COMMAND_RESULT_ERROR;
}

static eConsoleCommandResult vfsDebugConsoleLsHandler(uint32_t transport, int argc, char *argv[])
{
    struct stVfsDebugSession *lSession;
    struct stVfsDebugLsContext lContext;
    char lLocalPath[VFS_DEBUG_CWD_MAX];

    if ((argc != 1) && (argc != 2)) {
        return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
    }

    lSession = vfsDebugGetSession(transport);
    if ((lSession == NULL) || !vfsDebugResolveLocalPath(lSession, (argc == 2) ? argv[1] : NULL, lLocalPath, sizeof(lLocalPath)) ||
        !vfsDebugComposeAbsolutePath(lLocalPath, gVfsDebugPathBuffer, sizeof(gVfsDebugPathBuffer))) {
        return vfsDebugReplyPathError(transport);
    }

    if (!vfsGetInfo(gVfsDebugPathBuffer, &gVfsDebugNodeInfo)) {
        if (logConsoleReply(transport, "ERROR: path not found %s err=%u", lLocalPath, (unsigned)vfsGetStatus()->lastError) <= 0) {
            return CONSOLE_COMMAND_RESULT_ERROR;
        }
        return CONSOLE_COMMAND_RESULT_OK;
    }

    if (gVfsDebugNodeInfo.type == eVFS_NODE_FILE) {
        return (logConsoleReply(transport, "file %lu %s", (unsigned long)gVfsDebugNodeInfo.size, lLocalPath) > 0) ? CONSOLE_COMMAND_RESULT_OK : CONSOLE_COMMAND_RESULT_ERROR;
    }

    (void)memset(&lContext, 0, sizeof(lContext));
    lContext.transport = transport;
    if (!vfsListDir(gVfsDebugPathBuffer, vfsDebugLsVisitor, &lContext, NULL)) {
        if (logConsoleReply(transport, "ERROR: ls fail path=%s err=%u", lLocalPath, (unsigned)vfsGetStatus()->lastError) <= 0) {
            return CONSOLE_COMMAND_RESULT_ERROR;
        }
        return CONSOLE_COMMAND_RESULT_OK;
    }

    if (lContext.hasReplyError) {
        return CONSOLE_COMMAND_RESULT_ERROR;
    }

    if (lContext.entryCount == 0U) {
        return (logConsoleReply(transport, "empty") > 0) ? CONSOLE_COMMAND_RESULT_OK : CONSOLE_COMMAND_RESULT_ERROR;
    }

    return CONSOLE_COMMAND_RESULT_OK;
}

static eConsoleCommandResult vfsDebugConsoleCdHandler(uint32_t transport, int argc, char *argv[])
{
    struct stVfsDebugSession *lSession;
    char lLocalPath[VFS_DEBUG_CWD_MAX];

    if ((argc != 1) && (argc != 2)) {
        return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
    }

    lSession = vfsDebugGetSession(transport);
    if (lSession == NULL) {
        return CONSOLE_COMMAND_RESULT_ERROR;
    }

    if (argc == 1) {
        lLocalPath[0] = '/';
        lLocalPath[1] = '\0';
    } else if (!vfsDebugResolveLocalPath(lSession, argv[1], lLocalPath, sizeof(lLocalPath))) {
        return vfsDebugReplyPathError(transport);
    }

    if (!vfsDebugComposeAbsolutePath(lLocalPath, gVfsDebugPathBuffer, sizeof(gVfsDebugPathBuffer)) || !vfsGetInfo(gVfsDebugPathBuffer, &gVfsDebugNodeInfo)) {
        if (logConsoleReply(transport, "ERROR: path not found %s err=%u", lLocalPath, (unsigned)vfsGetStatus()->lastError) <= 0) {
            return CONSOLE_COMMAND_RESULT_ERROR;
        }
        return CONSOLE_COMMAND_RESULT_OK;
    }

    if (gVfsDebugNodeInfo.type != eVFS_NODE_DIR) {
        return (logConsoleReply(transport, "ERROR: not a directory %s", lLocalPath) > 0) ? CONSOLE_COMMAND_RESULT_OK : CONSOLE_COMMAND_RESULT_ERROR;
    }

    (void)memcpy(lSession->cwd, lLocalPath, sizeof(lSession->cwd));
    lSession->cwd[sizeof(lSession->cwd) - 1U] = '\0';
    return (logConsoleReply(transport, "%s", lSession->cwd) > 0) ? CONSOLE_COMMAND_RESULT_OK : CONSOLE_COMMAND_RESULT_ERROR;
}

static eConsoleCommandResult vfsDebugConsolePwdHandler(uint32_t transport, int argc, char *argv[])
{
    struct stVfsDebugSession *lSession;
    (void)argv;

    if (argc != 1) {
        return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
    }

    lSession = vfsDebugGetSession(transport);
    if (lSession == NULL) {
        return CONSOLE_COMMAND_RESULT_ERROR;
    }

    return (logConsoleReply(transport, "%s", lSession->cwd) > 0) ? CONSOLE_COMMAND_RESULT_OK : CONSOLE_COMMAND_RESULT_ERROR;
}

static eConsoleCommandResult vfsDebugConsoleCatHandler(uint32_t transport, int argc, char *argv[])
{
    struct stVfsDebugSession *lSession;
    uint32_t lActualSize = 0U;
    char lLocalPath[VFS_DEBUG_CWD_MAX];

    if (argc != 2) {
        return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
    }

    lSession = vfsDebugGetSession(transport);
    if ((lSession == NULL) || !vfsDebugResolveLocalPath(lSession, argv[1], lLocalPath, sizeof(lLocalPath)) ||
        !vfsDebugComposeAbsolutePath(lLocalPath, gVfsDebugPathBuffer, sizeof(gVfsDebugPathBuffer))) {
        return vfsDebugReplyPathError(transport);
    }

    if (!vfsGetInfo(gVfsDebugPathBuffer, &gVfsDebugNodeInfo)) {
        if (logConsoleReply(transport, "ERROR: path not found %s err=%u", lLocalPath, (unsigned)vfsGetStatus()->lastError) <= 0) {
            return CONSOLE_COMMAND_RESULT_ERROR;
        }
        return CONSOLE_COMMAND_RESULT_OK;
    }

    if (gVfsDebugNodeInfo.type != eVFS_NODE_FILE) {
        return (logConsoleReply(transport, "ERROR: not a file %s", lLocalPath) > 0) ? CONSOLE_COMMAND_RESULT_OK : CONSOLE_COMMAND_RESULT_ERROR;
    }

    if (gVfsDebugNodeInfo.size > VFS_DEBUG_RW_BUFFER_MAX) {
        return (logConsoleReply(transport,
                                "ERROR: file too large %s size=%lu max=%u",
                                lLocalPath,
                                (unsigned long)gVfsDebugNodeInfo.size,
                                VFS_DEBUG_RW_BUFFER_MAX) > 0) ? CONSOLE_COMMAND_RESULT_OK : CONSOLE_COMMAND_RESULT_ERROR;
    }

    if (!vfsReadFile(gVfsDebugPathBuffer, gVfsDebugRwBuffer, sizeof(gVfsDebugRwBuffer), &lActualSize)) {
        if (logConsoleReply(transport, "ERROR: read fail path=%s err=%u", lLocalPath, (unsigned)vfsGetStatus()->lastError) <= 0) {
            return CONSOLE_COMMAND_RESULT_ERROR;
        }
        return CONSOLE_COMMAND_RESULT_OK;
    }

    if (logConsoleReply(transport, "cat %s size=%lu", lLocalPath, (unsigned long)lActualSize) <= 0) {
        return CONSOLE_COMMAND_RESULT_ERROR;
    }

    return vfsDebugReplyFileContent(transport, gVfsDebugRwBuffer, lActualSize);
}

static eConsoleCommandResult vfsDebugConsoleTeeHandler(uint32_t transport, int argc, char *argv[])
{
    struct stVfsDebugSession *lSession;
    bool lIsAppend = false;
    uint32_t lPathArgIndex = 1U;
    uint32_t lPayloadArgIndex = 2U;
    uint32_t lPayloadSize = 0U;
    char lLocalPath[VFS_DEBUG_CWD_MAX];

    if (argc < 2) {
        return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
    }

    if ((argc >= 2) && (strcmp(argv[1], "-a") == 0)) {
        lIsAppend = true;
        lPathArgIndex = 2U;
        lPayloadArgIndex = 3U;
        if (argc < 3) {
            return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
        }
    }

    lSession = vfsDebugGetSession(transport);
    if ((lSession == NULL) || !vfsDebugResolveLocalPath(lSession, argv[lPathArgIndex], lLocalPath, sizeof(lLocalPath)) ||
        !vfsDebugComposeAbsolutePath(lLocalPath, gVfsDebugPathBuffer, sizeof(gVfsDebugPathBuffer))) {
        return vfsDebugReplyPathError(transport);
    }

    if (!vfsDebugBuildWritePayload(argc,
                                   argv,
                                   lPayloadArgIndex,
                                   (char *)gVfsDebugRwBuffer,
                                   sizeof(gVfsDebugRwBuffer),
                                   &lPayloadSize)) {
        return (logConsoleReply(transport, "ERROR: content too long max=%u", VFS_DEBUG_RW_BUFFER_MAX) > 0) ? CONSOLE_COMMAND_RESULT_OK : CONSOLE_COMMAND_RESULT_ERROR;
    }

    if (!(lIsAppend ? vfsAppendFile(gVfsDebugPathBuffer, gVfsDebugRwBuffer, lPayloadSize)
                    : vfsWriteFile(gVfsDebugPathBuffer, gVfsDebugRwBuffer, lPayloadSize))) {
        if (logConsoleReply(transport,
                            "ERROR: %s fail path=%s err=%u",
                            lIsAppend ? "append" : "write",
                            lLocalPath,
                            (unsigned)vfsGetStatus()->lastError) <= 0) {
            return CONSOLE_COMMAND_RESULT_ERROR;
        }
        return CONSOLE_COMMAND_RESULT_OK;
    }

    return (logConsoleReply(transport,
                            "%s ok %s size=%lu",
                            lIsAppend ? "append" : "write",
                            lLocalPath,
                            (unsigned long)lPayloadSize) > 0) ? CONSOLE_COMMAND_RESULT_OK : CONSOLE_COMMAND_RESULT_ERROR;
}

static eConsoleCommandResult vfsDebugConsoleMvHandler(uint32_t transport, int argc, char *argv[])
{
    struct stVfsDebugSession *lSession;
    stVfsNodeInfo lSourceInfo;
    char lSourcePath[VFS_DEBUG_CWD_MAX];
    char lTargetPath[VFS_DEBUG_CWD_MAX];
    char lSourceAbsolutePath[VFS_PATH_MAX];
    char lTargetAbsolutePath[VFS_PATH_MAX];

    if (argc != 3) {
        return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
    }

    lSession = vfsDebugGetSession(transport);
    if ((lSession == NULL) || !vfsDebugResolveLocalPath(lSession, argv[1], lSourcePath, sizeof(lSourcePath)) || !vfsDebugResolveLocalPath(lSession, argv[2], lTargetPath, sizeof(lTargetPath))) {
        return vfsDebugReplyPathError(transport);
    }

    if ((lSourcePath[0] == '/') && (lSourcePath[1] == '\0')) {
        return (logConsoleReply(transport, "ERROR: mv root is forbidden") > 0) ? CONSOLE_COMMAND_RESULT_OK : CONSOLE_COMMAND_RESULT_ERROR;
    }

    if (!vfsDebugComposeAbsolutePath(lSourcePath, lSourceAbsolutePath, sizeof(lSourceAbsolutePath)) || !vfsGetInfo(lSourceAbsolutePath, &lSourceInfo)) {
        if (logConsoleReply(transport, "ERROR: path not found %s err=%u", lSourcePath, (unsigned)vfsGetStatus()->lastError) <= 0) {
            return CONSOLE_COMMAND_RESULT_ERROR;
        }
        return CONSOLE_COMMAND_RESULT_OK;
    }

    if (strcmp(lSourcePath, lTargetPath) == 0) {
        return (logConsoleReply(transport, "mv ok %s", lSourcePath) > 0) ? CONSOLE_COMMAND_RESULT_OK : CONSOLE_COMMAND_RESULT_ERROR;
    }

    if ((lSourceInfo.type == eVFS_NODE_DIR) && vfsDebugPathContains(lSourcePath, lTargetPath)) {
        return (logConsoleReply(transport, "ERROR: mv target is inside source %s -> %s", lSourcePath, lTargetPath) > 0) ? CONSOLE_COMMAND_RESULT_OK : CONSOLE_COMMAND_RESULT_ERROR;
    }

    if (!vfsDebugComposeAbsolutePath(lSourcePath, lSourceAbsolutePath, sizeof(lSourceAbsolutePath)) ||
        !vfsDebugComposeAbsolutePath(lTargetPath, lTargetAbsolutePath, sizeof(lTargetAbsolutePath)) ||
        !vfsMove(lSourceAbsolutePath, lTargetAbsolutePath)) {
        if (logConsoleReply(transport,
                            "ERROR: mv fail old=%s new=%s err=%u",
                            lSourcePath,
                            lTargetPath,
                            (unsigned)vfsGetStatus()->lastError) <= 0) {
            return CONSOLE_COMMAND_RESULT_ERROR;
        }
        return CONSOLE_COMMAND_RESULT_OK;
    }

    (void)vfsDebugUpdateSessionCwdAfterMove(lSession, lSourcePath, lTargetPath);
    return (logConsoleReply(transport, "mv ok %s -> %s", lSourcePath, lTargetPath) > 0) ? CONSOLE_COMMAND_RESULT_OK : CONSOLE_COMMAND_RESULT_ERROR;
}

static eConsoleCommandResult vfsDebugConsoleRmHandler(uint32_t transport, int argc, char *argv[])
{
    struct stVfsDebugSession *lSession;
    char lLocalPath[VFS_DEBUG_CWD_MAX];

    if (argc != 2) {
        return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
    }

    lSession = vfsDebugGetSession(transport);
    if ((lSession == NULL) || !vfsDebugResolveLocalPath(lSession, argv[1], lLocalPath, sizeof(lLocalPath)) ||
        !vfsDebugComposeAbsolutePath(lLocalPath, gVfsDebugPathBuffer, sizeof(gVfsDebugPathBuffer))) {
        return vfsDebugReplyPathError(transport);
    }

    if ((lLocalPath[0] == '/') && (lLocalPath[1] == '\0')) {
        return (logConsoleReply(transport, "ERROR: rm root is forbidden") > 0) ? CONSOLE_COMMAND_RESULT_OK : CONSOLE_COMMAND_RESULT_ERROR;
    }

    if (!vfsDebugDeletePathRecursive(gVfsDebugPathBuffer)) {
        if (logConsoleReply(transport, "ERROR: rm fail path=%s err=%u", lLocalPath, (unsigned)vfsGetStatus()->lastError) <= 0) {
            return CONSOLE_COMMAND_RESULT_ERROR;
        }
        return CONSOLE_COMMAND_RESULT_OK;
    }

    if (vfsDebugPathContains(lLocalPath, lSession->cwd)) {
        lSession->cwd[0] = '/';
        lSession->cwd[1] = '\0';
    }

    return (logConsoleReply(transport, "rm ok %s", lLocalPath) > 0) ? CONSOLE_COMMAND_RESULT_OK : CONSOLE_COMMAND_RESULT_ERROR;
}

static eConsoleCommandResult vfsDebugConsoleDfHandler(uint32_t transport, int argc, char *argv[])
{
    stVfsSpaceInfo lSpaceInfo;
    (void)argv;

    if (argc != 1) {
        return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
    }

    if (!vfsGetSpaceInfo(gVfsDebugRootPath, &lSpaceInfo)) {
        if (logConsoleReply(transport, "ERROR: df fail err=%u", (unsigned)vfsGetStatus()->lastError) <= 0) {
            return CONSOLE_COMMAND_RESULT_ERROR;
        }
        return CONSOLE_COMMAND_RESULT_OK;
    }

    return (logConsoleReply(transport,
                            "total=%lu used=%lu free=%lu bytes",
                            (unsigned long)lSpaceInfo.totalSize,
                            (unsigned long)lSpaceInfo.usedSize,
                            (unsigned long)lSpaceInfo.freeSize) > 0) ? CONSOLE_COMMAND_RESULT_OK : CONSOLE_COMMAND_RESULT_ERROR;
}

bool vfsDebugConsoleRegister(const char *rootPath)
{
    if ((rootPath == NULL) || !vfsNormalizePath(rootPath, gVfsDebugRootPath, sizeof(gVfsDebugRootPath))) {
        return false;
    }

    if (gVfsDebugIsRegistered) {
        return true;
    }

    if (!logRegisterConsole(&gVfsDebugMkdirConsoleCommand) ||
        !logRegisterConsole(&gVfsDebugLsConsoleCommand) ||
        !logRegisterConsole(&gVfsDebugCdConsoleCommand) ||
        !logRegisterConsole(&gVfsDebugPwdConsoleCommand) ||
        !logRegisterConsole(&gVfsDebugCatConsoleCommand) ||
        !logRegisterConsole(&gVfsDebugTeeConsoleCommand) ||
        !logRegisterConsole(&gVfsDebugMvConsoleCommand) ||
        !logRegisterConsole(&gVfsDebugRmConsoleCommand) ||
        !logRegisterConsole(&gVfsDebugDfConsoleCommand)) {
        return false;
    }

    gVfsDebugIsRegistered = true;
    return true;
}

/**************************End of file********************************/
