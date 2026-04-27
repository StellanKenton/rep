/************************************************************************************
* @file     : ec800m_mqtt.h
* @brief    : EC800M MQTT AT helper declarations.
* @details  : Builds Quectel MQTT commands used by project managers.
* @author   : GitHub Copilot
* @date     : 2026-04-27
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef EC800M_MQTT_H
#define EC800M_MQTT_H

#include "ec800m.h"

#ifdef __cplusplus
extern "C" {
#endif

eEc800mStatus ec800mMqttBuildOpenCommand(uint8_t clientIndex, const char *host, uint16_t port, char *buffer, uint16_t bufferSize);
eEc800mStatus ec800mMqttBuildConnectCommand(uint8_t clientIndex, const char *clientId, const char *username, const char *password, char *buffer, uint16_t bufferSize);
eEc800mStatus ec800mMqttBuildSubscribeCommand(uint8_t clientIndex, uint16_t msgId, const char *topic, uint8_t qos, char *buffer, uint16_t bufferSize);
eEc800mStatus ec800mMqttBuildPublishExCommand(uint8_t clientIndex, uint16_t msgId, uint8_t qos, bool retain, const char *topic, uint16_t payloadLen, char *buffer, uint16_t bufferSize);
eEc800mStatus ec800mMqttBuildCloseCommand(uint8_t clientIndex, char *buffer, uint16_t bufferSize);

#ifdef __cplusplus
}
#endif

#endif  // EC800M_MQTT_H
/**************************End of file********************************/
