/************************************************************************************
* @file     : drvmcuflash_port.h
* @brief    : Shared MCU flash logical area definitions.
* @details  : This file keeps project-level writable flash area mapping and module
*             options independent from the generic driver interface.
* @author   : GitHub Copilot
* @date     : 2026-04-02
* @version  : V1.0.0
***********************************************************************************/
#ifndef DRVMCUFLASH_PORT_H
#define DRVMCUFLASH_PORT_H

#ifdef __cplusplus
extern "C" {
#endif

#ifndef DRVMCUFLASH_LOG_SUPPORT
#define DRVMCUFLASH_LOG_SUPPORT              1
#endif

#ifndef DRVMCUFLASH_CONSOLE_SUPPORT
#define DRVMCUFLASH_CONSOLE_SUPPORT          0
#endif

#define DRVMCUFLASH_LOCK_WAIT_MS             50U

#ifndef DRVMCUFLASH_AREA_USER_START_ADDR
#define DRVMCUFLASH_AREA_USER_START_ADDR     0x080E0000U
#endif

#ifndef DRVMCUFLASH_AREA_USER_SIZE
#define DRVMCUFLASH_AREA_USER_SIZE           0x00020000U
#endif

typedef enum eDrvMcuFlashAreaMap {
    DRVMCUFLASH_AREA_USER = 0,
    DRVMCUFLASH_MAX,
} eDrvMcuFlashAreaMap;

#ifdef __cplusplus
}
#endif

#endif  // DRVMCUFLASH_PORT_H
/**************************End of file********************************/
