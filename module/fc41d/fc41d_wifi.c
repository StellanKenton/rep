/***********************************************************************************
* @file     : fc41d_wifi.c
* @brief    : 
* @details  : 
* @author   : 
* @date     : 
* @version  : 
* @copyright: Copyright (c) 2050
**********************************************************************************/
#include "fc41d_wifi.h"

#include <stddef.h>
#include <string.h>

#include "fc41d_priv.h"

static const char *fc41dWifiGetDefStopCmdText(const stFc41dWifiCfg *cfg);

stRingBuffer *fc41dWifiGetRxRingBuffer(eFc41dMapType device)
{
	return fc41dGetRxRbByChannel(fc41dGetCtx(device), FC41D_RX_CHANNEL_WIFI);
}

uint32_t fc41dWifiRead(eFc41dMapType device, uint8_t *buffer, uint32_t length)
{
	stRingBuffer *rb = fc41dWifiGetRxRingBuffer(device);
	return (rb != NULL) ? ringBufferRead(rb, buffer, length) : 0U;
}

uint32_t fc41dWifiPeek(eFc41dMapType device, uint8_t *buffer, uint32_t length)
{
	stRingBuffer *rb = fc41dWifiGetRxRingBuffer(device);
	return (rb != NULL) ? ringBufferPeek(rb, buffer, length) : 0U;
}

uint32_t fc41dWifiDiscard(eFc41dMapType device, uint32_t length)
{
	stRingBuffer *rb = fc41dWifiGetRxRingBuffer(device);
	return (rb != NULL) ? ringBufferDiscard(rb, length) : 0U;
}

eFc41dStatus fc41dWifiClearRx(eFc41dMapType device)
{
	stRingBuffer *rb = fc41dWifiGetRxRingBuffer(device);

	if (rb == NULL) {
		return FC41D_STATUS_INVALID_PARAM;
	}

	return (ringBufferReset(rb) == RINGBUFFER_OK) ? FC41D_STATUS_OK : FC41D_STATUS_ERROR;
}

void fc41dWifiSyncInfoFromCfg(stFc41dCtx *ctx)
{
	if (ctx == NULL) {
		return;
	}

	ctx->info.wifi.enableRx = ctx->cfg.wifi.enableRx;
	ctx->info.wifi.workMode = ctx->cfg.wifi.workMode;
	fc41dWifiResetState(ctx);
}


void fc41dWifiResetState(stFc41dCtx *ctx)
{
	if (ctx == NULL) {
		return;
	}

	if (ctx->cfg.wifi.workMode == FC41D_WIFI_WORK_MODE_STA) {
		ctx->info.wifi.state = FC41D_WIFI_STATE_STA_INIT;
	} else if (ctx->cfg.wifi.workMode == FC41D_WIFI_WORK_MODE_AP) {
		ctx->info.wifi.state = FC41D_WIFI_STATE_AP_INIT;
	} else {
		ctx->info.wifi.state = FC41D_WIFI_STATE_IDLE;
	}
}

void fc41dWifiAccumulateRxStats(stFc41dCtx *ctx, uint32_t written, uint32_t dropped)
{
	if (ctx == NULL) {
		return;
	}

	ctx->info.wifi.rxRoutedBytes += written;
	ctx->info.wifi.rxDroppedBytes += dropped;
}

eFc41dStatus fc41dWifiProcessStateMachine(stFc41dCtx *ctx, eFc41dMapType device)
{
	eFc41dStatus status;
	stFc41dWifiCfg prevCfg;

	if (ctx == NULL) {
		return FC41D_STATUS_INVALID_PARAM;
	}

	if (ctx->info.wifi.workMode != ctx->cfg.wifi.workMode) {
		prevCfg = ctx->cfg.wifi;
		prevCfg.workMode = ctx->info.wifi.workMode;
		status = fc41dExecOptionalCmdText(device, FC41D_EXEC_OWNER_WIFI, fc41dWifiGetDefStopCmdText(&prevCfg));
		if (status == FC41D_STATUS_BUSY) {
			return FC41D_STATUS_OK;
		}
		if (status != FC41D_STATUS_OK) {
			ctx->info.wifi.state = FC41D_WIFI_STATE_ERROR;
			return status;
		}
		ctx->info.wifi.workMode = ctx->cfg.wifi.workMode;
		fc41dWifiResetState(ctx);
	}

	switch (ctx->info.wifi.state) {
		case FC41D_WIFI_STATE_IDLE:
		case FC41D_WIFI_STATE_STA_CONNECTED:
		case FC41D_WIFI_STATE_AP_ACTIVE:
		case FC41D_WIFI_STATE_STA_DISCONNECTED:
		case FC41D_WIFI_STATE_AP_STOPPED:
			return FC41D_STATUS_OK;
		case FC41D_WIFI_STATE_STA_INIT:
			status = fc41dExecOptionalCmdText(device, FC41D_EXEC_OWNER_WIFI, ctx->cfg.wifi.initCmdText);
			if (status == FC41D_STATUS_BUSY) {
				return FC41D_STATUS_OK;
			}
			ctx->info.wifi.state = (status == FC41D_STATUS_OK) ? FC41D_WIFI_STATE_STA_CONNECTING : FC41D_WIFI_STATE_ERROR;
			return status;
		case FC41D_WIFI_STATE_STA_CONNECTING:
			status = fc41dExecOptionalCmdText(device, FC41D_EXEC_OWNER_WIFI, ctx->cfg.wifi.startCmdText);
			if (status == FC41D_STATUS_BUSY) {
				return FC41D_STATUS_OK;
			}
			ctx->info.wifi.state = (status == FC41D_STATUS_OK) ? FC41D_WIFI_STATE_STA_DISCONNECTED : FC41D_WIFI_STATE_ERROR;
			return status;
		case FC41D_WIFI_STATE_AP_INIT:
			status = fc41dExecOptionalCmdText(device, FC41D_EXEC_OWNER_WIFI, ctx->cfg.wifi.initCmdText);
			if (status == FC41D_STATUS_BUSY) {
				return FC41D_STATUS_OK;
			}
			ctx->info.wifi.state = (status == FC41D_STATUS_OK) ? FC41D_WIFI_STATE_AP_STARTING : FC41D_WIFI_STATE_ERROR;
			return status;
		case FC41D_WIFI_STATE_AP_STARTING:
			if ((ctx->cfg.wifi.startCmdText == NULL) || (ctx->cfg.wifi.startCmdText[0] == '\0')) {
				ctx->info.wifi.state = FC41D_WIFI_STATE_AP_STOPPED;
				return FC41D_STATUS_OK;
			}
			status = fc41dExecOptionalCmdText(device, FC41D_EXEC_OWNER_WIFI, ctx->cfg.wifi.startCmdText);
			if (status == FC41D_STATUS_BUSY) {
				return FC41D_STATUS_OK;
			}
			ctx->info.wifi.state = (status == FC41D_STATUS_OK) ? FC41D_WIFI_STATE_AP_ACTIVE : FC41D_WIFI_STATE_ERROR;
			return status;
		case FC41D_WIFI_STATE_ERROR:
		default:
			return FC41D_STATUS_ERROR;
	}
}

void fc41dWifiUpdateLinkStateByUrc(stFc41dCtx *ctx, const uint8_t *lineBuf, uint16_t lineLen)
{
	if (ctx == NULL) {
		return;
	}

	if (!fc41dLineHasToken(lineBuf, lineLen, "WIFI") &&
		!fc41dLineHasToken(lineBuf, lineLen, "STA") &&
		!fc41dLineHasToken(lineBuf, lineLen, "AP")) {
		return;
	}

	if (fc41dLineHasToken(lineBuf, lineLen, "DISCONN")) {
		ctx->info.wifi.state = (ctx->info.wifi.workMode == FC41D_WIFI_WORK_MODE_AP) ?
			FC41D_WIFI_STATE_AP_STOPPED : FC41D_WIFI_STATE_STA_DISCONNECTED;
	} else if (fc41dLineHasToken(lineBuf, lineLen, "GOT IP") || fc41dLineHasToken(lineBuf, lineLen, "CONNECTED")) {
		ctx->info.wifi.state = FC41D_WIFI_STATE_STA_CONNECTED;
	} else if (fc41dLineHasToken(lineBuf, lineLen, "SOFTAP") || fc41dLineHasToken(lineBuf, lineLen, "AP START")) {
		ctx->info.wifi.state = FC41D_WIFI_STATE_AP_ACTIVE;
	}
}

static const char *fc41dWifiGetDefStopCmdText(const stFc41dWifiCfg *cfg)
{
	if (cfg == NULL) {
		return NULL;
	}

	if (cfg->stopCmdText != NULL) {
		return cfg->stopCmdText;
	}

	if (cfg->workMode == FC41D_WIFI_WORK_MODE_STA) {
		return "AT+QSTASTOP";
	}

	if (cfg->workMode == FC41D_WIFI_WORK_MODE_AP) {
		return "AT+QSOFTAPSTOP";
	}

	return NULL;
}

/**************************End of file********************************/
