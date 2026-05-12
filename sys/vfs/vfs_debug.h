/************************************************************************************
* @file     : vfs_debug.h
* @brief    : Console helpers for vfs-backed mount shells.
* @details  : Registers Linux-like debug commands against a single vfs mount
*             root while keeping shell paths relative to that mount.
* @author   : 
* @date     : 
* @version  : 
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef REP_SERVICE_VFS_DEBUG_H
#define REP_SERVICE_VFS_DEBUG_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

bool vfsDebugConsoleRegister(const char *rootPath);

#ifdef __cplusplus
}
#endif

#endif  // REP_SERVICE_VFS_DEBUG_H
/**************************End of file********************************/
