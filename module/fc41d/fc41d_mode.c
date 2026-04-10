/***********************************************************************************
* @file     : fc41d_mode.c
* @brief    : 
* @details  : 
* @author   : 
* @date     : 
* @version  : 
* @copyright: Copyright (c) 2050
**********************************************************************************/
#include "fc41d.h"

#include <stddef.h>

#include "fc41d_priv.h"

eFc41dStatus fc41dSetModeState(eFc41dMapType device, eFc41dMode mode)
{
	stFc41dCtx *ctx;

	if (mode > FC41D_MODE_WIFI_DATA) {
		return FC41D_STATUS_INVALID_PARAM;
	}

	ctx = fc41dGetCtx(device);
	if (ctx == NULL) {
		return FC41D_STATUS_INVALID_PARAM;
	}

	ctx->info.mode = mode;
	return FC41D_STATUS_OK;
}

eFc41dMode fc41dGetModeState(eFc41dMapType device)
{
	stFc41dCtx *ctx = fc41dGetCtx(device);
	return (ctx != NULL) ? ctx->info.mode : FC41D_MODE_COMMAND;
}

/**************************End of file********************************/
