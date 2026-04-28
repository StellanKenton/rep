/************************************************************************************
* @file     : fc41d_http.c
* @brief    : FC41D HTTP AT helper implementation.
***********************************************************************************/
#include "fc41d_http.h"

static eFc41dStatus appendText(char *buffer, uint16_t bufferSize, uint16_t *length, const char *text);
static eFc41dStatus appendU16(char *buffer, uint16_t bufferSize, uint16_t *length, uint16_t value);
static eFc41dStatus appendQuotedText(char *buffer, uint16_t bufferSize, uint16_t *length, const char *text);

eFc41dStatus fc41dHttpBuildHeaderCommand(char *buffer, uint16_t bufferSize)
{
    if ((buffer == NULL) || (bufferSize < 55U)) {
        return FC41D_STATUS_INVALID_PARAM;
    }
    return appendText(buffer, bufferSize, &(uint16_t){0U}, "AT+QHTTPCFG=\"header\",\"Content-Type\",\"application/json\"\r\n");
}

eFc41dStatus fc41dHttpBuildUrlCommand(const char *url, char *buffer, uint16_t bufferSize)
{
    uint16_t length = 0U;
    eFc41dStatus status;

    if ((url == NULL) || (url[0] == '\0') || (buffer == NULL) || (bufferSize == 0U)) {
        return FC41D_STATUS_INVALID_PARAM;
    }
    buffer[0] = '\0';
    status = appendText(buffer, bufferSize, &length, "AT+QHTTPCFG=\"url\",");
    if (status == FC41D_STATUS_OK) status = appendQuotedText(buffer, bufferSize, &length, url);
    if (status == FC41D_STATUS_OK) status = appendText(buffer, bufferSize, &length, "\r\n");
    return status;
}

eFc41dStatus fc41dHttpBuildPostJsonCommand(const char *url, uint16_t payloadLen, char *buffer, uint16_t bufferSize)
{
    uint16_t length = 0U;
    eFc41dStatus status;

    (void)url;
    if ((payloadLen == 0U) || (buffer == NULL) || (bufferSize == 0U)) {
        return FC41D_STATUS_INVALID_PARAM;
    }

    buffer[0] = '\0';
    status = appendText(buffer, bufferSize, &length, "AT+QHTTPPOST=");
    if (status == FC41D_STATUS_OK) status = appendU16(buffer, bufferSize, &length, payloadLen);
    if (status == FC41D_STATUS_OK) status = appendText(buffer, bufferSize, &length, ",60,60\r\n");
    return status;
}

eFc41dStatus fc41dHttpBuildReadCommand(char *buffer, uint16_t bufferSize)
{
    if ((buffer == NULL) || (bufferSize < 18U)) {
        return FC41D_STATUS_INVALID_PARAM;
    }
    return appendText(buffer, bufferSize, &(uint16_t){0U}, "AT+QHTTPREAD=60\r\n");
}

static eFc41dStatus appendText(char *buffer, uint16_t bufferSize, uint16_t *length, const char *text)
{
    if ((buffer == NULL) || (length == NULL) || (text == NULL)) return FC41D_STATUS_INVALID_PARAM;
    while (*text != '\0') {
        if (*length >= (uint16_t)(bufferSize - 1U)) return FC41D_STATUS_OVERFLOW;
        buffer[*length] = *text++;
        *length = (uint16_t)(*length + 1U);
    }
    buffer[*length] = '\0';
    return FC41D_STATUS_OK;
}

static eFc41dStatus appendU16(char *buffer, uint16_t bufferSize, uint16_t *length, uint16_t value)
{
    char digits[5];
    uint16_t index = 0U;
    if ((buffer == NULL) || (length == NULL)) return FC41D_STATUS_INVALID_PARAM;
    do {
        digits[index++] = (char)('0' + (value % 10U));
        value = (uint16_t)(value / 10U);
    } while ((value > 0U) && (index < (uint16_t)sizeof(digits)));
    while (index > 0U) {
        index--;
        if (*length >= (uint16_t)(bufferSize - 1U)) return FC41D_STATUS_OVERFLOW;
        buffer[*length] = digits[index];
        *length = (uint16_t)(*length + 1U);
    }
    buffer[*length] = '\0';
    return FC41D_STATUS_OK;
}

static eFc41dStatus appendQuotedText(char *buffer, uint16_t bufferSize, uint16_t *length, const char *text)
{
    eFc41dStatus status = appendText(buffer, bufferSize, length, "\"");
    while ((status == FC41D_STATUS_OK) && (*text != '\0')) {
        if ((*text == '"') || (*text == '\r') || (*text == '\n')) return FC41D_STATUS_INVALID_PARAM;
        if (*length >= (uint16_t)(bufferSize - 1U)) return FC41D_STATUS_OVERFLOW;
        buffer[*length] = *text++;
        *length = (uint16_t)(*length + 1U);
    }
    if (status == FC41D_STATUS_OK) status = appendText(buffer, bufferSize, length, "\"");
    return status;
}
