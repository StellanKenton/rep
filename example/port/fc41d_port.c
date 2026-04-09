#include "fc41d_port.h"

#include <stddef.h>
#include <string.h>

#include "fc41d_assembly.h"
#include "fc41d_priv.h"
#include "main.h"
#include "flowparser_stream_port.h"
#include "Rep/drvlayer/drvuart/drvuart.h"
#include "drvuart_port.h"

#include "stm32f1xx_hal.h"

#if (REP_RTOS_SYSTEM == REP_RTOS_FREERTOS)
#include "FreeRTOS.h"
#include "task.h"
#endif

static uint8_t gFc41dPortLineBuf[FC41D_DEV_MAX][FC41D_PORT_LINE_BUF_SIZE];
static uint8_t gFc41dPortCmdBuf[FC41D_DEV_MAX][FC41D_PORT_CMD_BUF_SIZE];
static uint8_t gFc41dPortPayloadBuf[FC41D_DEV_MAX][FC41D_PORT_PAYLOAD_BUF_SIZE];
static uint8_t gFc41dPortBleRxStorage[FC41D_DEV_MAX][FC41D_PORT_BLE_RX_BUF_SIZE];
static uint8_t gFc41dPortWifiRxStorage[FC41D_DEV_MAX][FC41D_PORT_WIFI_RX_BUF_SIZE];
static bool gFc41dPortCfgDone[FC41D_DEV_MAX] = {false};

#define FC41D_PORT_RESET_ASSERT_MS         10U
#define FC41D_PORT_BOOT_WAIT_MS            300U
#define FC41D_PORT_BLE_FRAME_MIN_LEN       8U

static const stFc41dPortAssembleCfg gFc41dPortDefCfg[FC41D_DEV_MAX] = {
	[FC41D_DEV0] = {
		.uart = DRVUART_WIRELESS,
		.lineBuf = gFc41dPortLineBuf[FC41D_DEV0],
		.lineBufSize = FC41D_PORT_LINE_BUF_SIZE,
		.cmdBuf = gFc41dPortCmdBuf[FC41D_DEV0],
		.cmdBufSize = FC41D_PORT_CMD_BUF_SIZE,
		.payloadBuf = gFc41dPortPayloadBuf[FC41D_DEV0],
		.payloadBufSize = FC41D_PORT_PAYLOAD_BUF_SIZE,
		.bleRxStorage = gFc41dPortBleRxStorage[FC41D_DEV0],
		.bleRxCapacity = FC41D_PORT_BLE_RX_BUF_SIZE,
		.wifiRxStorage = gFc41dPortWifiRxStorage[FC41D_DEV0],
		.wifiRxCapacity = FC41D_PORT_WIFI_RX_BUF_SIZE,
		.txTimeoutMs = FC41D_PORT_TX_TIMEOUT_MS,
		.procBudget = FC41D_PORT_PROC_BUDGET,
		.blePrefix = FC41D_PORT_BLE_PREFIX,
		.wifiPrefix = FC41D_PORT_WIFI_PREFIX,
		.routeFunc = fc41dPortRouteDefault,
		.routeUserCtx = NULL,
	},
};

static stFc41dPortAssembleCfg gFc41dPortCfg[FC41D_DEV_MAX];

static bool fc41dPortIsValidDevMap(eFc41dMapType device);
static stFc41dPortAssembleCfg *fc41dPortGetCfgCtx(eFc41dMapType device);
static bool fc41dPortHasPrefix(const uint8_t *lineBuf, uint16_t lineLen, const char *prefix, uint16_t *prefixLen);
static bool fc41dPortParsePayloadAfterPrefix(const uint8_t *lineBuf, uint16_t lineLen, uint16_t prefixLen,
											 const uint8_t **payloadBuf, uint16_t *payloadLen);
static eDrvStatus fc41dPortSendAdpt(const uint8_t *buf, uint16_t len, void *userCtx);
static bool fc41dPortIsUrc(const uint8_t *lineBuf, uint16_t lineLen, void *userCtx);
static void fc41dPortReleaseHwReset(void);
static void fc41dPortDrainBleFrames(eFc41dMapType device);

static bool fc41dPortIsValidDevMap(eFc41dMapType device)
{
	return ((uint32_t)device < (uint32_t)FC41D_DEV_MAX);
}

static stFc41dPortAssembleCfg *fc41dPortGetCfgCtx(eFc41dMapType device)
{
	if (!fc41dPortIsValidDevMap(device)) {
		return NULL;
	}

	if (!gFc41dPortCfgDone[device]) {
		gFc41dPortCfg[device] = gFc41dPortDefCfg[device];
		gFc41dPortCfgDone[device] = true;
	}

	return &gFc41dPortCfg[device];
}

static void fc41dPortReleaseHwReset(void)
{
	HAL_GPIO_WritePin(RESET_WIFI_GPIO_Port, RESET_WIFI_Pin, GPIO_PIN_RESET);
	HAL_Delay(FC41D_PORT_RESET_ASSERT_MS);
	HAL_GPIO_WritePin(RESET_WIFI_GPIO_Port, RESET_WIFI_Pin, GPIO_PIN_SET);
	HAL_Delay(FC41D_PORT_BOOT_WAIT_MS);
}

static void fc41dPortDrainBleFrames(eFc41dMapType device)
{
	stFc41dPortAssembleCfg *cfg = fc41dPortGetCfgCtx(device);
	stFc41dCtx *ctx = fc41dGetCtx(device);
	stRingBuffer *uartRb;
	uint8_t header[6];
	uint32_t used;
	uint16_t payloadLen;
	uint16_t frameLen;

	if (!fc41dPortIsValidAssembleCfg(cfg) || (ctx == NULL) || !ctx->info.isReady || flowparserStreamIsBusy(&ctx->atStream)) {
		return;
	}

	uartRb = drvUartGetRingBuffer(cfg->uart);
	if (uartRb == NULL) {
		return;
	}

	while (true) {
		used = ringBufferGetUsed(uartRb);
		if (used < FC41D_PORT_BLE_FRAME_MIN_LEN) {
			return;
		}

		if (ringBufferPeek(uartRb, header, sizeof(header)) != sizeof(header)) {
			return;
		}

		if ((header[0] != 0xFAU) || (header[1] != 0xFCU) || (header[2] != 0x01U)) {
			(void)ringBufferDiscard(uartRb, 1U);
			continue;
		}

		payloadLen = (uint16_t)(((uint16_t)header[4] << 8U) | header[5]);
		frameLen = (uint16_t)(3U + 1U + 2U + payloadLen + 2U);
		if ((frameLen < FC41D_PORT_BLE_FRAME_MIN_LEN) || (frameLen > cfg->payloadBufSize)) {
			(void)ringBufferDiscard(uartRb, 1U);
			continue;
		}

		if (used < frameLen) {
			return;
		}

		if (ringBufferRead(uartRb, cfg->payloadBuf, frameLen) != frameLen) {
			return;
		}

		(void)ringBufferWriteOverwrite(&ctx->bleRxRb, cfg->payloadBuf, frameLen);
		ctx->info.lastRxChannel = FC41D_RX_CHANNEL_BLE;
		ctx->info.bleRxRoutedBytes += frameLen;
	}
}

bool fc41dPortIsValidAssembleCfg(const stFc41dPortAssembleCfg *cfg)
{
	return (cfg != NULL) &&
		   (cfg->lineBuf != NULL) && (cfg->lineBufSize > 1U) &&
		   (cfg->cmdBuf != NULL) && (cfg->cmdBufSize > 0U) &&
		   (cfg->bleRxStorage != NULL) && (cfg->bleRxCapacity > 0U) &&
		   (cfg->wifiRxStorage != NULL) && (cfg->wifiRxCapacity > 0U) &&
		   (cfg->uart < DRVUART_MAX);
}

eFc41dStatus fc41dPortGetDefAssembleCfg(eFc41dMapType device, stFc41dPortAssembleCfg *cfg)
{
	if ((cfg == NULL) || !fc41dPortIsValidDevMap(device)) {
		return FC41D_STATUS_INVALID_PARAM;
	}

	*cfg = gFc41dPortDefCfg[device];
	return FC41D_STATUS_OK;
}

eFc41dStatus fc41dPortGetAssembleCfg(eFc41dMapType device, stFc41dPortAssembleCfg *cfg)
{
	stFc41dPortAssembleCfg *ctx;

	if (cfg == NULL) {
		return FC41D_STATUS_INVALID_PARAM;
	}

	ctx = fc41dPortGetCfgCtx(device);
	if (ctx == NULL) {
		return FC41D_STATUS_INVALID_PARAM;
	}

	*cfg = *ctx;
	return FC41D_STATUS_OK;
}

eFc41dStatus fc41dPortSetAssembleCfg(eFc41dMapType device, const stFc41dPortAssembleCfg *cfg)
{
	stFc41dPortAssembleCfg *ctx;

	if (!fc41dPortIsValidAssembleCfg(cfg)) {
		return FC41D_STATUS_INVALID_PARAM;
	}

	ctx = fc41dPortGetCfgCtx(device);
	if (ctx == NULL) {
		return FC41D_STATUS_INVALID_PARAM;
	}

	*ctx = *cfg;
	gFc41dPortCfgDone[device] = true;
	return FC41D_STATUS_OK;
}

void fc41dLoadPlatformDefaultCfg(eFc41dMapType device, stFc41dCfg *cfg)
{
	(void)device;

	if (cfg == NULL) {
		return;
	}

	cfg->enableBleRx = true;
	cfg->enableWifiRx = true;
	cfg->bleRxOverwriteOnFull = true;
	cfg->wifiRxOverwriteOnFull = true;
	cfg->execGuardMs = 5000U;
	cfg->bootMode = FC41D_MODE_COMMAND;
}

bool fc41dPlatformIsValidAssemble(eFc41dMapType device)
{
	return fc41dPortIsValidAssembleCfg(fc41dPortGetCfgCtx(device));
}

uint32_t fc41dPlatformGetTickMs(void)
{
#if (REP_RTOS_SYSTEM == REP_RTOS_FREERTOS)
	return (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
#else
	return HAL_GetTick();
#endif
}

eFc41dStatus fc41dPlatformInitTransport(eFc41dMapType device)
{
	stFc41dPortAssembleCfg *cfg = fc41dPortGetCfgCtx(device);
	eFc41dStatus status;

	if (!fc41dPortIsValidAssembleCfg(cfg)) {
		return FC41D_STATUS_INVALID_PARAM;
	}

	status = drvUartInit(cfg->uart);
	if (status != FC41D_STATUS_OK) {
		return status;
	}

	fc41dPortReleaseHwReset();
	return FC41D_STATUS_OK;
}

void fc41dPlatformPollRx(eFc41dMapType device)
{
	stFc41dPortAssembleCfg *cfg = fc41dPortGetCfgCtx(device);

	if (cfg == NULL) {
		return;
	}

	(void)drvUartGetDataLen(cfg->uart);
	fc41dPortDrainBleFrames(device);
}

eFc41dStatus fc41dPlatformInitRxBuffers(eFc41dMapType device, stRingBuffer *bleRxRb, stRingBuffer *wifiRxRb)
{
	stFc41dPortAssembleCfg *cfg = fc41dPortGetCfgCtx(device);

	if (!fc41dPortIsValidAssembleCfg(cfg) || (bleRxRb == NULL) || (wifiRxRb == NULL)) {
		return FC41D_STATUS_INVALID_PARAM;
	}

	if ((ringBufferInit(bleRxRb, cfg->bleRxStorage, cfg->bleRxCapacity) != RINGBUFFER_OK) ||
		(ringBufferInit(wifiRxRb, cfg->wifiRxStorage, cfg->wifiRxCapacity) != RINGBUFFER_OK)) {
		return FC41D_STATUS_ERROR;
	}

	return FC41D_STATUS_OK;
}

static eDrvStatus fc41dPortSendAdpt(const uint8_t *buf, uint16_t len, void *userCtx)
{
	eFc41dMapType device = (eFc41dMapType)(uintptr_t)userCtx;
	stFc41dPortAssembleCfg *cfg = fc41dPortGetCfgCtx(device);
	eDrvStatus status;

	if (!fc41dPortIsValidAssembleCfg(cfg) || (buf == NULL) || (len == 0U)) {
		return DRV_STATUS_INVALID_PARAM;
	}

	status = drvUartTransmitDma(cfg->uart, buf, len);
	if ((status == DRV_STATUS_OK) || (status != DRV_STATUS_UNSUPPORTED)) {
		if (status != DRV_STATUS_BUSY) {
			return status;
		}
	}

	return drvUartTransmit(cfg->uart, buf, len, cfg->txTimeoutMs);
}

static bool fc41dPortHasPrefix(const uint8_t *lineBuf, uint16_t lineLen, const char *prefix, uint16_t *prefixLen)
{
	uint32_t len;

	if ((lineBuf == NULL) || (prefix == NULL)) {
		return false;
	}

	len = strlen(prefix);
	if ((len == 0U) || (lineLen < len)) {
		return false;
	}

	if (memcmp(lineBuf, prefix, len) != 0) {
		return false;
	}

	if (prefixLen != NULL) {
		*prefixLen = (uint16_t)len;
	}
	return true;
}

static bool fc41dPortParsePayloadAfterPrefix(const uint8_t *lineBuf, uint16_t lineLen, uint16_t prefixLen,
											 const uint8_t **payloadBuf, uint16_t *payloadLen)
{
	const uint8_t *cursor;
	const uint8_t *colon;
	uint32_t lenField = 0U;
	bool hasLen = false;

	if ((lineBuf == NULL) || (payloadBuf == NULL) || (payloadLen == NULL) || (lineLen <= prefixLen)) {
		return false;
	}

	cursor = &lineBuf[prefixLen];
	if ((*cursor == ',') || (*cursor == ':')) {
		cursor++;
	}

	colon = (const uint8_t *)memchr(cursor, ':', (size_t)(lineLen - (uint16_t)(cursor - lineBuf)));
	if (colon != NULL) {
		const uint8_t *scan = colon;
		const uint8_t *digitsStart = NULL;

		while (scan > cursor) {
			scan--;
			if ((*scan >= '0') && (*scan <= '9')) {
				digitsStart = scan;
				while ((digitsStart > cursor) && (*(digitsStart - 1) >= '0') && (*(digitsStart - 1) <= '9')) {
					digitsStart--;
				}
				break;
			}
			if (*scan == ',') {
				continue;
			}
			digitsStart = NULL;
			break;
		}

		if (digitsStart != NULL) {
			const uint8_t *digit = digitsStart;
			while (digit < colon) {
				if ((*digit >= '0') && (*digit <= '9')) {
					hasLen = true;
					lenField = (lenField * 10U) + (uint32_t)(*digit - '0');
				}
				digit++;
			}
		}

		*payloadBuf = colon + 1;
		*payloadLen = (uint16_t)(lineLen - (uint16_t)((colon + 1) - lineBuf));
		if (hasLen && (lenField <= (uint32_t)(lineLen - (uint16_t)((colon + 1) - lineBuf)))) {
			*payloadLen = (uint16_t)lenField;
		}
		return (*payloadLen > 0U);
	}

	*payloadBuf = cursor;
	*payloadLen = (uint16_t)(lineLen - (uint16_t)(cursor - lineBuf));
	return (*payloadLen > 0U);
}

bool fc41dPortRouteDefault(const uint8_t *lineBuf, uint16_t lineLen,
						   eFc41dRxChannel *channel, const uint8_t **payloadBuf,
						   uint16_t *payloadLen, void *userCtx)
{
	stFc41dPortAssembleCfg *cfg = (stFc41dPortAssembleCfg *)userCtx;
	uint16_t prefixLen;

	if (cfg == NULL) {
		return false;
	}

	if ((lineBuf != NULL) && (lineLen >= 3U) && (lineBuf[0] == 0xFAU) && (lineBuf[1] == 0xFCU) && (lineBuf[2] == 0x01U)) {
		*channel = FC41D_RX_CHANNEL_BLE;
		*payloadBuf = lineBuf;
		*payloadLen = lineLen;
		return true;
	}

	if (fc41dPortHasPrefix(lineBuf, lineLen, cfg->blePrefix, &prefixLen)) {
		*channel = FC41D_RX_CHANNEL_BLE;
		return fc41dPortParsePayloadAfterPrefix(lineBuf, lineLen, prefixLen, payloadBuf, payloadLen);
	}

	if (fc41dPortHasPrefix(lineBuf, lineLen, cfg->wifiPrefix, &prefixLen)) {
		*channel = FC41D_RX_CHANNEL_WIFI;
		return fc41dPortParsePayloadAfterPrefix(lineBuf, lineLen, prefixLen, payloadBuf, payloadLen);
	}

	return false;
}

bool fc41dPlatformRouteLine(eFc41dMapType device, const uint8_t *lineBuf, uint16_t lineLen,
							eFc41dRxChannel *channel, const uint8_t **payloadBuf, uint16_t *payloadLen)
{
	stFc41dPortAssembleCfg *cfg = fc41dPortGetCfgCtx(device);

	if (!fc41dPortIsValidAssembleCfg(cfg) || (channel == NULL) || (payloadBuf == NULL) || (payloadLen == NULL)) {
		return false;
	}

	*channel = FC41D_RX_CHANNEL_NONE;
	*payloadBuf = NULL;
	*payloadLen = 0U;

	if (cfg->routeFunc != NULL) {
		return cfg->routeFunc(lineBuf, lineLen, channel, payloadBuf, payloadLen,
							  (cfg->routeUserCtx != NULL) ? cfg->routeUserCtx : cfg);
	}

	return fc41dPortRouteDefault(lineBuf, lineLen, channel, payloadBuf, payloadLen, cfg);
}

static bool fc41dPortIsUrc(const uint8_t *lineBuf, uint16_t lineLen, void *userCtx)
{
	eFc41dMapType device = (eFc41dMapType)(uintptr_t)userCtx;
	eFc41dRxChannel channel;
	const uint8_t *payloadBuf;
	uint16_t payloadLen;

	if (fc41dPlatformRouteLine(device, lineBuf, lineLen, &channel, &payloadBuf, &payloadLen)) {
		return true;
	}

	return fc41dAtIsUrc(lineBuf, lineLen);
}

eFlowParserStrmSta fc41dPlatformInitAtStream(eFc41dMapType device, stFlowParserStream *stream,
											 flowparserStreamLineFunc urcHandler, void *urcUserCtx)
{
	stFc41dPortAssembleCfg *cfg = fc41dPortGetCfgCtx(device);
	stFlowParserStreamCfg fpCfg;
	stRingBuffer *uartRb;

	if (!fc41dPortIsValidAssembleCfg(cfg) || (stream == NULL)) {
		return FLOWPARSER_STREAM_INVALID_ARG;
	}

	uartRb = drvUartGetRingBuffer(cfg->uart);
	if (uartRb == NULL) {
		return FLOWPARSER_STREAM_NOT_INIT;
	}

	(void)memset(&fpCfg, 0, sizeof(fpCfg));
	fpCfg.tokCfg.ringBuf = uartRb;
	fpCfg.tokCfg.lineBuf = cfg->lineBuf;
	fpCfg.tokCfg.lineBufSize = cfg->lineBufSize;
	fpCfg.cmdBuf = cfg->cmdBuf;
	fpCfg.cmdBufSize = cfg->cmdBufSize;
	fpCfg.payloadBuf = cfg->payloadBuf;
	fpCfg.payloadBufSize = cfg->payloadBufSize;
	fpCfg.send = fc41dPortSendAdpt;
	fpCfg.portUserCtx = (void *)(uintptr_t)device;
	fpCfg.getTick = fc41dPlatformGetTickMs;
	fpCfg.isUrc = fc41dPortIsUrc;
	fpCfg.urcMatchUserCtx = (void *)(uintptr_t)device;
	fpCfg.urcHandler = urcHandler;
	fpCfg.urcUserCtx = urcUserCtx;
	fpCfg.procBudget = cfg->procBudget;
	flowparserPortApplyDftCfg(&fpCfg);

	return flowparserStreamInit(stream, &fpCfg);
}
/**************************End of file********************************/
