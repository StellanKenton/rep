/************************************************************************************
* @file     : fc41d_wifi.c
* @brief    : FC41D WiFi AT helper implementation.
***********************************************************************************/
#include "fc41d_wifi.h"

#include <string.h>

static eFc41dStatus fc41dWifiAppendText(char *buffer, uint16_t bufferSize, uint16_t *length, const char *text);
static bool fc41dWifiMatchPrefix(const uint8_t *lineBuf, uint16_t lineLen, const char *prefix);

bool fc41dWifiIsUrc(const uint8_t *lineBuf, uint16_t lineLen)
{
    return fc41dWifiMatchPrefix(lineBuf, lineLen, "WIFI CONNECTED") ||
           fc41dWifiMatchPrefix(lineBuf, lineLen, "WIFI GOT IP") ||
           fc41dWifiMatchPrefix(lineBuf, lineLen, "WIFI DISCONNECT") ||
           fc41dWifiMatchPrefix(lineBuf, lineLen, "WIFI DISCONNECTED") ||
           fc41dWifiMatchPrefix(lineBuf, lineLen, "+CWJAP:") ||
           fc41dWifiMatchPrefix(lineBuf, lineLen, "+CWSTATE:");
}

eFc41dStatus fc41dWifiBuildStationModeCommand(char *buffer, uint16_t bufferSize)
{
    if ((buffer == NULL) || (bufferSize < 14U)) {
        return FC41D_STATUS_INVALID_PARAM;
    }

    (void)memcpy(buffer, "AT\r\n", 5U);
    return FC41D_STATUS_OK;
}

eFc41dStatus fc41dWifiBuildJoinCommand(const char *ssid, const char *password, char *buffer, uint16_t bufferSize)
{
    uint16_t length;
    eFc41dStatus status;

    if ((ssid == NULL) || (password == NULL) || (buffer == NULL) || (bufferSize == 0U)) {
        return FC41D_STATUS_INVALID_PARAM;
    }

    length = 0U;
    buffer[0] = '\0';
    status = fc41dWifiAppendText(buffer, bufferSize, &length, "AT+QSTAAPINFO=");
    if (status == FC41D_STATUS_OK) {
        status = fc41dWifiAppendText(buffer, bufferSize, &length, ssid);
    }
    if (status == FC41D_STATUS_OK) {
        status = fc41dWifiAppendText(buffer, bufferSize, &length, ",");
    }
    if (status == FC41D_STATUS_OK) {
        status = fc41dWifiAppendText(buffer, bufferSize, &length, password);
    }
    if (status == FC41D_STATUS_OK) {
        status = fc41dWifiAppendText(buffer, bufferSize, &length, "\r\n");
    }
    return status;
}

static eFc41dStatus fc41dWifiAppendText(char *buffer, uint16_t bufferSize, uint16_t *length, const char *text)
{
    if ((buffer == NULL) || (length == NULL) || (text == NULL)) {
        return FC41D_STATUS_INVALID_PARAM;
    }

    while (*text != '\0') {
        if (*length >= (uint16_t)(bufferSize - 1U)) {
            return FC41D_STATUS_OVERFLOW;
        }
        buffer[*length] = *text++;
        *length = (uint16_t)(*length + 1U);
    }
    buffer[*length] = '\0';
    return FC41D_STATUS_OK;
}

static bool fc41dWifiMatchPrefix(const uint8_t *lineBuf, uint16_t lineLen, const char *prefix)
{
    uint16_t prefixLen;

    if ((lineBuf == NULL) || (prefix == NULL)) {
        return false;
    }

    prefixLen = (uint16_t)strlen(prefix);
    return (lineLen >= prefixLen) && (memcmp(lineBuf, prefix, prefixLen) == 0);
}
