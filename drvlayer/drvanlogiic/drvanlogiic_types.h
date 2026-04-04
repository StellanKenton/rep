/************************************************************************************
* @file     : drvanlogiic_types.h
* @brief    : Public software IIC logical bus definitions.
* @details  : Keeps reusable software IIC API dependencies independent from the
*             port layer.
***********************************************************************************/
#ifndef DRVANLOGIIC_TYPES_H
#define DRVANLOGIIC_TYPES_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum eDrvAnlogIicPortMap {
    DRVANLOGIIC_PCA = 0,
    DRVANLOGIIC_TM,
    DRVANLOGIIC_MAX,
} eDrvAnlogIicPortMap;

#ifdef __cplusplus
}
#endif

#endif  // DRVANLOGIIC_TYPES_H
/**************************End of file********************************/