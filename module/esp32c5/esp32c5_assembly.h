/************************************************************************************
* @file     : esp32c5_assembly.h
* @brief    : ESP32-C5 assembly-time contract shared by core and port.
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef ESP32C5_ASSEMBLY_H
#define ESP32C5_ASSEMBLY_H

#include <stdint.h>

#include "esp32c5.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef eDrvStatus (*esp32c5TransportInitFunc)(uint8_t linkId);
typedef eDrvStatus (*esp32c5TransportWriteFunc)(uint8_t linkId, const uint8_t *buffer, uint16_t length, uint32_t timeoutMs);
typedef uint16_t (*esp32c5TransportGetRxLenFunc)(uint8_t linkId);
typedef eDrvStatus (*esp32c5TransportReadFunc)(uint8_t linkId, uint8_t *buffer, uint16_t length);
typedef uint32_t (*esp32c5TransportGetTickMsFunc)(void);
typedef void (*esp32c5ControlInitFunc)(uint8_t resetPin);
typedef void (*esp32c5ControlSetResetLevelFunc)(uint8_t resetPin, bool isActive);

typedef struct stEsp32c5TransportInterface {
    esp32c5TransportInitFunc init;
    esp32c5TransportWriteFunc write;
    esp32c5TransportGetRxLenFunc getRxLen;
    esp32c5TransportReadFunc read;
    esp32c5TransportGetTickMsFunc getTickMs;
} stEsp32c5TransportInterface;

typedef struct stEsp32c5ControlInterface {
    esp32c5ControlInitFunc init;
    esp32c5ControlSetResetLevelFunc setResetLevel;
} stEsp32c5ControlInterface;

void esp32c5LoadPlatformDefaultCfg(eEsp32c5MapType device, stEsp32c5Cfg *cfg);
const stEsp32c5TransportInterface *esp32c5GetPlatformTransportInterface(const stEsp32c5Cfg *cfg);
const stEsp32c5ControlInterface *esp32c5GetPlatformControlInterface(eEsp32c5MapType device);
bool esp32c5PlatformIsValidCfg(const stEsp32c5Cfg *cfg);

#ifdef __cplusplus
}
#endif

#endif  // ESP32C5_ASSEMBLY_H
/**************************End of file********************************/
