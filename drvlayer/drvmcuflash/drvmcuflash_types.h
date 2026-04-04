/************************************************************************************
* @file     : drvmcuflash_types.h
* @brief    : Public MCU flash logical area definitions.
* @details  : Keeps reusable flash API dependencies independent from the port layer.
***********************************************************************************/
#ifndef DRVMCUFLASH_TYPES_H
#define DRVMCUFLASH_TYPES_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum eDrvMcuFlashAreaMap {
    DRVMCUFLASH_AREA_USER = 0,
    DRVMCUFLASH_MAX,
} eDrvMcuFlashAreaMap;

#ifdef __cplusplus
}
#endif

#endif  // DRVMCUFLASH_TYPES_H
/**************************End of file********************************/