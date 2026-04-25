/************************************************************************************
* @file     : esp32c5_ble.h
* @brief    : ESP32-C5 BLE AT helper declarations.
* @details  : Splits BLE command assembly and URC parsing away from the control core.
* @author   : GitHub Copilot
* @date     : 2026-04-25
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef ESP32C5_BLE_H
#define ESP32C5_BLE_H

#include "esp32c5.h"

#ifdef __cplusplus
extern "C" {
#endif

void esp32c5BleLoadDefCfg(stEsp32c5BleCfg *cfg);
bool esp32c5BleIsValidText(const char *text, uint16_t maxLength, bool allowEmpty);
eEsp32c5Status esp32c5BleBuildAdvParamCommand(const stEsp32c5BleCfg *cfg, char *buffer, uint16_t bufferSize);
eEsp32c5Status esp32c5BleBuildAdvDataCommand(const stEsp32c5BleCfg *cfg, char *buffer, uint16_t bufferSize);
uint16_t esp32c5BleGetNotifyPayloadLimit(const stEsp32c5State *state);
bool esp32c5BleIsUrc(const uint8_t *lineBuf, uint16_t lineLen);
bool esp32c5BleTryParseConnIndex(const uint8_t *lineBuf, uint16_t lineLen, uint8_t *connIndex);
bool esp32c5BleTryParseMtu(const uint8_t *lineBuf, uint16_t lineLen, uint8_t *connIndex, uint16_t *mtu);
bool esp32c5BleTryParseMacAddress(const uint8_t *lineBuf, uint16_t lineLen, char *buffer, uint16_t bufferSize);

#ifdef __cplusplus
}
#endif

#endif  // ESP32C5_BLE_H
/**************************End of file********************************/
