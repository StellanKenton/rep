/************************************************************************************
* @file     : tm1651.h
* @brief    : TM1651 segment display module public interface.
* @details  : This module keeps TM1651 command sequencing in the core layer and
*             relies on the port layer to bind the device to the project
*             software IIC driver.
***********************************************************************************/
#ifndef TM1651_H
#define TM1651_H

#include <stdbool.h>
#include <stdint.h>

#include "drvanlogiic.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum eTm1651DevMap {
    TM1651_DEV0 = 0,
    TM1651_DEV_MAX,
} eTm1651MapType;

#define TM1651_DIGIT_MAX                     4U
#define TM1651_DEFAULT_DIGIT_COUNT           3U

#define TM1651_DATA_CMD_AUTO_ADDR            0x40U
#define TM1651_DATA_CMD_FIXED_ADDR           0x44U
#define TM1651_ADDR_CMD_BASE                 0xC0U
#define TM1651_DISPLAY_OFF_CMD               0x80U
#define TM1651_DISPLAY_ON_CMD                0x88U

#define TM1651_SYMBOL_0                      0U
#define TM1651_SYMBOL_1                      1U
#define TM1651_SYMBOL_2                      2U
#define TM1651_SYMBOL_3                      3U
#define TM1651_SYMBOL_4                      4U
#define TM1651_SYMBOL_5                      5U
#define TM1651_SYMBOL_6                      6U
#define TM1651_SYMBOL_7                      7U
#define TM1651_SYMBOL_8                      8U
#define TM1651_SYMBOL_9                      9U
#define TM1651_SYMBOL_BLANK                  10U
#define TM1651_SYMBOL_DASH                   11U
#define TM1651_SYMBOL_E                      12U

typedef eDrvStatus eTm1651Status;

#define TM1651_STATUS_OK                     DRV_STATUS_OK
#define TM1651_STATUS_INVALID_PARAM          DRV_STATUS_INVALID_PARAM
#define TM1651_STATUS_NOT_READY              DRV_STATUS_NOT_READY
#define TM1651_STATUS_BUSY                   DRV_STATUS_BUSY
#define TM1651_STATUS_TIMEOUT                DRV_STATUS_TIMEOUT
#define TM1651_STATUS_NACK                   DRV_STATUS_NACK
#define TM1651_STATUS_UNSUPPORTED            DRV_STATUS_UNSUPPORTED
#define TM1651_STATUS_ERROR                  DRV_STATUS_ERROR

typedef struct stTm1651Cfg {
    uint8_t brightness;
    uint8_t digitCount;
    bool isDisplayOn;
} stTm1651Cfg;

typedef struct stTm1651Device {
    stTm1651Cfg cfg;
    uint8_t segData[TM1651_DIGIT_MAX];
    bool isReady;
} stTm1651Device;

eTm1651Status tm1651GetDefCfg(eTm1651MapType device, stTm1651Cfg *cfg);
eTm1651Status tm1651GetCfg(eTm1651MapType device, stTm1651Cfg *cfg);
eTm1651Status tm1651SetCfg(eTm1651MapType device, const stTm1651Cfg *cfg);
eTm1651Status tm1651Init(eTm1651MapType device);
bool tm1651IsReady(eTm1651MapType device);
eTm1651Status tm1651SetBrightness(eTm1651MapType device, uint8_t brightness);
eTm1651Status tm1651SetDisplayOn(eTm1651MapType device, bool isDisplayOn);
eTm1651Status tm1651DisplayRaw(eTm1651MapType device, const uint8_t *segData, uint8_t length);
eTm1651Status tm1651DisplayDigits(eTm1651MapType device, uint8_t dig1, uint8_t dig2, uint8_t dig3, uint8_t dig4);
eTm1651Status tm1651ClearDisplay(eTm1651MapType device);
eTm1651Status tm1651ShowNone(eTm1651MapType device);
eTm1651Status tm1651ShowNumber3(eTm1651MapType device, uint16_t value);
eTm1651Status tm1651ShowError(eTm1651MapType device, uint16_t value);

#ifdef __cplusplus
}
#endif

#endif  // TM1651_H
/**************************End of file********************************/
