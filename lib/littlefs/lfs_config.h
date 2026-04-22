/************************************************************************************
* @file     : lfs_config.h
* @brief    : Project-local littlefs build configuration.
* @details  : Disables the default littlefs malloc and stdio helpers so the
*             library can run in the current MCU project with static buffers.
* @author   : 
* @date     : 
* @version  : 
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef USER_LIB_LITTLEFS_LFS_CONFIG_H
#define USER_LIB_LITTLEFS_LFS_CONFIG_H

#define LFS_NO_MALLOC
#define LFS_NO_ASSERT
#define LFS_NO_DEBUG
#define LFS_NO_WARN
#define LFS_NO_ERROR

#endif  // USER_LIB_LITTLEFS_LFS_CONFIG_H
/**************************End of file********************************/
