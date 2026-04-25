/************************************************************************************
* @file     : esp32c5_http.h
* @brief    : ESP32-C5 HTTP AT helper declarations.
* @details  : Builds ESP-AT HTTP commands used by project managers.
* @author   : GitHub Copilot
* @date     : 2026-04-25
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef ESP32C5_HTTP_H
#define ESP32C5_HTTP_H

#include "esp32c5.h"

#ifdef __cplusplus
extern "C" {
#endif

eEsp32c5Status esp32c5HttpBuildPostJsonCommand(const char *url, uint16_t payloadLen, char *buffer, uint16_t bufferSize);

#ifdef __cplusplus
}
#endif

#endif  // ESP32C5_HTTP_H
/**************************End of file********************************/
