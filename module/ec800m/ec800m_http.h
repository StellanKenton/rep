/************************************************************************************
* @file     : ec800m_http.h
* @brief    : EC800M HTTP AT helper declarations.
* @details  : Builds Quectel HTTP commands used by project managers.
* @author   : GitHub Copilot
* @date     : 2026-04-27
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef EC800M_HTTP_H
#define EC800M_HTTP_H

#include "ec800m.h"

#ifdef __cplusplus
extern "C" {
#endif

eEc800mStatus ec800mHttpBuildUrlCommand(uint16_t urlLen, uint16_t inputTimeoutSec, char *buffer, uint16_t bufferSize);
eEc800mStatus ec800mHttpBuildPostCommand(uint16_t inputTimeoutSec, uint16_t responseTimeoutSec, uint16_t outputTimeoutSec, char *buffer, uint16_t bufferSize);
eEc800mStatus ec800mHttpBuildReadCommand(uint16_t responseTimeoutSec, char *buffer, uint16_t bufferSize);

#ifdef __cplusplus
}
#endif

#endif  // EC800M_HTTP_H
/**************************End of file********************************/
