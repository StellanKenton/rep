/************************************************************************************
* @file     : drvadc_types.h
* @brief    : Public ADC logical channel definitions.
* @details  : Keeps reusable ADC API dependencies independent from the port layer.
***********************************************************************************/
#ifndef DRVADC_TYPES_H
#define DRVADC_TYPES_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum eDrvAdcPortMap {
    DRVADC_CH0 = 0,
    DRVADC_CH1 = 1,
    DRVADC_CH2 = 2,
    DRVADC_MAX,
} eDrvAdcPortMap;

#ifdef __cplusplus
}
#endif

#endif  // DRVADC_TYPES_H
/**************************End of file********************************/