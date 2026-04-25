/************************************************************************************
* @file     : esp32c5_wifi.h
* @brief    : ESP32-C5 WiFi AT helper declarations.
* @details  : Builds WiFi station commands and parses WiFi URCs.
* @author   : GitHub Copilot
* @date     : 2026-04-25
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef ESP32C5_WIFI_H
#define ESP32C5_WIFI_H

#include "esp32c5.h"

#ifdef __cplusplus
extern "C" {
#endif

bool esp32c5WifiIsUrc(const uint8_t *lineBuf, uint16_t lineLen);
eEsp32c5Status esp32c5WifiBuildStationModeCommand(char *buffer, uint16_t bufferSize);
eEsp32c5Status esp32c5WifiBuildJoinCommand(const char *ssid, const char *password, char *buffer, uint16_t bufferSize);

#ifdef __cplusplus
}
#endif

#endif  // ESP32C5_WIFI_H
/**************************End of file********************************/
