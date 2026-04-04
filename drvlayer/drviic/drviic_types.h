/************************************************************************************
* @file     : drviic_types.h
* @brief    : Public hardware IIC logical bus definitions.
* @details  : Keeps reusable hardware IIC API dependencies independent from the
*             port layer.
***********************************************************************************/
#ifndef DRVIIC_TYPES_H
#define DRVIIC_TYPES_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum eDrvIicPortMap {
    DRVIIC_BUS0 = 0,
    DRVIIC_MAX,
} eDrvIicPortMap;

#ifdef __cplusplus
}
#endif

#endif  // DRVIIC_TYPES_H
/**************************End of file********************************/