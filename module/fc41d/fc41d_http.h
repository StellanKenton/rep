/************************************************************************************
* @file     : fc41d_http.h
* @brief    : FC41D HTTP AT helper declarations.
***********************************************************************************/
#ifndef FC41D_HTTP_H
#define FC41D_HTTP_H

#include "fc41d.h"

#ifdef __cplusplus
extern "C" {
#endif

eFc41dStatus fc41dHttpBuildHeaderCommand(char *buffer, uint16_t bufferSize);
eFc41dStatus fc41dHttpBuildUrlCommand(const char *url, char *buffer, uint16_t bufferSize);
eFc41dStatus fc41dHttpBuildPostJsonCommand(const char *url, uint16_t payloadLen, char *buffer, uint16_t bufferSize);
eFc41dStatus fc41dHttpBuildReadCommand(char *buffer, uint16_t bufferSize);

#ifdef __cplusplus
}
#endif

#endif
