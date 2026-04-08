#include "fc41d.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "fc41d_priv.h"

#define FC41D_AT_TEXT_BUF_MAX               256U

static const char *const gFc41dAtRespDone[] = {
	"OK",
};

static const char *const gFc41dAtErrPat[] = {
	"ERROR",
	"FAIL",
};

static const char *const gFc41dAtUrcPatterns[] = {
	"+BLE*",
	"+WIFI*",
};

static const char gFc41dAtCmdCheckAlive[] = "AT";
static const char gFc41dAtCmdReset[] = "AT+QRST";
static const char gFc41dAtCmdStaStop[] = "AT+QSTASTOP";
static const char gFc41dAtCmdBleInitGatts[] = "AT+QBLEINIT=2";
static const char gFc41dAtCmdBleAddrQuery[] = "AT+QBLEADDR?";
static const char gFc41dAtCmdVersionQuery[] = "AT+QVERSION";
static const char gFc41dAtCmdBleAdvStart[] = "AT+QBLEADVSTART";

static const stFc41dAtCmdInfo gFc41dAtCmdInfoTable[] = {
	{FC41D_AT_CATALOG_CMD_AT, FC41D_AT_GROUP_GENERAL, "AT", "AT test command"},
	{FC41D_AT_CATALOG_CMD_QRST, FC41D_AT_GROUP_WIFI, "AT+QRST", "Restart module"},
	{FC41D_AT_CATALOG_CMD_QVERSION, FC41D_AT_GROUP_WIFI, "AT+QVERSION", "Get firmware version"},
	{FC41D_AT_CATALOG_CMD_QECHO, FC41D_AT_GROUP_WIFI, "AT+QECHO", "Enable or disable echo function"},
	{FC41D_AT_CATALOG_CMD_QURCCFG, FC41D_AT_GROUP_WIFI, "AT+QURCCFG", "Enable or disable URC report"},
	{FC41D_AT_CATALOG_CMD_QPING, FC41D_AT_GROUP_WIFI, "AT+QPING", "Ping external IP"},
	{FC41D_AT_CATALOG_CMD_QGETIP, FC41D_AT_GROUP_WIFI, "AT+QGETIP", "Get IP information"},
	{FC41D_AT_CATALOG_CMD_QSETBAND, FC41D_AT_GROUP_WIFI, "AT+QSETBAND", "Configure serial port baud rate"},
	{FC41D_AT_CATALOG_CMD_QWLANOTA, FC41D_AT_GROUP_WIFI, "AT+QWLANOTA", "Start OTA"},
	{FC41D_AT_CATALOG_CMD_QLOWPOWER, FC41D_AT_GROUP_WIFI, "AT+QLOWPOWER", "Enter low power mode"},
	{FC41D_AT_CATALOG_CMD_QDEEPSLEEP, FC41D_AT_GROUP_WIFI, "AT+QDEEPSLEEP", "Enter deep sleep mode"},
	{FC41D_AT_CATALOG_CMD_QWLMAC, FC41D_AT_GROUP_WIFI, "AT+QWLMAC", "Get MAC address"},
	{FC41D_AT_CATALOG_CMD_QAIRKISS, FC41D_AT_GROUP_WIFI, "AT+QAIRKISS", "Enable or disable AirKiss"},
	{FC41D_AT_CATALOG_CMD_QSTAST, FC41D_AT_GROUP_WIFI, "AT+QSTAST", "Query STA mode state"},
	{FC41D_AT_CATALOG_CMD_QSTADHCP, FC41D_AT_GROUP_WIFI, "AT+QSTADHCP", "Enable or disable DHCP in STA mode"},
	{FC41D_AT_CATALOG_CMD_QSTADHCPDEF, FC41D_AT_GROUP_WIFI, "AT+QSTADHCPDEF", "Enable or disable DHCP in STA mode and save configuration"},
	{FC41D_AT_CATALOG_CMD_QSTASTATIC, FC41D_AT_GROUP_WIFI, "AT+QSTASTATIC", "Configure static IP for STA mode"},
	{FC41D_AT_CATALOG_CMD_QSTASTOP, FC41D_AT_GROUP_WIFI, "AT+QSTASTOP", "Disable STA mode"},
	{FC41D_AT_CATALOG_CMD_QSOFTAP, FC41D_AT_GROUP_WIFI, "AT+QSOFTAP", "Enable AP mode"},
	{FC41D_AT_CATALOG_CMD_QAPSTATE, FC41D_AT_GROUP_WIFI, "AT+QAPSTATE", "Query AP mode state"},
	{FC41D_AT_CATALOG_CMD_QAPSTATIC, FC41D_AT_GROUP_WIFI, "AT+QAPSTATIC", "Configure static IP for AP mode"},
	{FC41D_AT_CATALOG_CMD_QSOFTAPSTOP, FC41D_AT_GROUP_WIFI, "AT+QSOFTAPSTOP", "Disable AP mode"},
	{FC41D_AT_CATALOG_CMD_QSTAAPINFO, FC41D_AT_GROUP_WIFI, "AT+QSTAAPINFO", "Connect to an AP hotspot"},
	{FC41D_AT_CATALOG_CMD_QSTAAPINFODEF, FC41D_AT_GROUP_WIFI, "AT+QSTAAPINFODEF", "Connect to a hotspot and save hotspot information"},
	{FC41D_AT_CATALOG_CMD_QGETWIFISTATE, FC41D_AT_GROUP_WIFI, "AT+QGETWIFISTATE", "Query connected hotspot"},
	{FC41D_AT_CATALOG_CMD_QWSCAN, FC41D_AT_GROUP_WIFI, "AT+QWSCAN", "Query scanned hotspot information"},
	{FC41D_AT_CATALOG_CMD_QWEBCFG, FC41D_AT_GROUP_WIFI, "AT+QWEBCFG", "Enable or disable configuring Wi-Fi via Web"},
	{FC41D_AT_CATALOG_CMD_QBLEINIT, FC41D_AT_GROUP_BLE, "AT+QBLEINIT", "Initialize BLE service"},
	{FC41D_AT_CATALOG_CMD_QBLEADDR, FC41D_AT_GROUP_BLE, "AT+QBLEADDR", "Query BLE device address"},
	{FC41D_AT_CATALOG_CMD_QBLENAME, FC41D_AT_GROUP_BLE, "AT+QBLENAME", "Set BLE name"},
	{FC41D_AT_CATALOG_CMD_QBLEADVPARAM, FC41D_AT_GROUP_BLE, "AT+QBLEADVPARAM", "Configure BLE advertising parameters"},
	{FC41D_AT_CATALOG_CMD_QBLEADVDATA, FC41D_AT_GROUP_BLE, "AT+QBLEADVDATA", "Set BLE advertising data"},
	{FC41D_AT_CATALOG_CMD_QBLEGATTSSRV, FC41D_AT_GROUP_BLE, "AT+QBLEGATTSSRV", "Establish a BLE service"},
	{FC41D_AT_CATALOG_CMD_QBLEGATTSCHAR, FC41D_AT_GROUP_BLE, "AT+QBLEGATTSCHAR", "Set BLE characteristic UUID"},
	{FC41D_AT_CATALOG_CMD_QBLEADVSTART, FC41D_AT_GROUP_BLE, "AT+QBLEADVSTART", "Start BLE advertising"},
	{FC41D_AT_CATALOG_CMD_QBLEADVSTOP, FC41D_AT_GROUP_BLE, "AT+QBLEADVSTOP", "Stop BLE advertising"},
	{FC41D_AT_CATALOG_CMD_QBLEGATTSNTFY, FC41D_AT_GROUP_BLE, "AT+QBLEGATTSNTFY", "Send GATT data"},
	{FC41D_AT_CATALOG_CMD_QBLESCAN, FC41D_AT_GROUP_BLE, "AT+QBLESCAN", "Start or stop BLE scan"},
	{FC41D_AT_CATALOG_CMD_QBLESCANPARAM, FC41D_AT_GROUP_BLE, "AT+QBLESCANPARAM", "Set BLE scan parameters"},
	{FC41D_AT_CATALOG_CMD_QBLECONN, FC41D_AT_GROUP_BLE, "AT+QBLECONN", "Connect a peripheral device"},
	{FC41D_AT_CATALOG_CMD_QBLECONNPARAM, FC41D_AT_GROUP_BLE, "AT+QBLECONNPARAM", "Configure BLE connection parameters"},
	{FC41D_AT_CATALOG_CMD_QBLECFGMTU, FC41D_AT_GROUP_BLE, "AT+QBLECFGMTU", "Configure BLE MTU"},
	{FC41D_AT_CATALOG_CMD_QBLEGATTCNTFCFG, FC41D_AT_GROUP_BLE, "AT+QBLEGATTCNTFCFG", "Turn notifications on or off"},
	{FC41D_AT_CATALOG_CMD_QBLEGATTCWR, FC41D_AT_GROUP_BLE, "AT+QBLEGATTCWR", "Send BLE client data"},
	{FC41D_AT_CATALOG_CMD_QBLEGATTCRD, FC41D_AT_GROUP_BLE, "AT+QBLEGATTCRD", "Read BLE client data"},
	{FC41D_AT_CATALOG_CMD_QBLEDISCONN, FC41D_AT_GROUP_BLE, "AT+QBLEDISCONN", "Disconnect BLE connection"},
	{FC41D_AT_CATALOG_CMD_QBLESTAT, FC41D_AT_GROUP_BLE, "AT+QBLESTAT", "Query BLE device state"},
	{FC41D_AT_CATALOG_CMD_QICFG, FC41D_AT_GROUP_TCPUDP, "AT+QICFG", "Configure optional parameters for TCP or UDP socket service"},
	{FC41D_AT_CATALOG_CMD_QIOPEN, FC41D_AT_GROUP_TCPUDP, "AT+QIOPEN", "Open TCP or UDP socket service"},
	{FC41D_AT_CATALOG_CMD_QISTATE, FC41D_AT_GROUP_TCPUDP, "AT+QISTATE", "Query TCP or UDP socket state"},
	{FC41D_AT_CATALOG_CMD_QISEND, FC41D_AT_GROUP_TCPUDP, "AT+QISEND", "Send data through TCP or UDP socket service"},
	{FC41D_AT_CATALOG_CMD_QIRD, FC41D_AT_GROUP_TCPUDP, "AT+QIRD", "Read received TCP or UDP socket data"},
	{FC41D_AT_CATALOG_CMD_QIACCEPT, FC41D_AT_GROUP_TCPUDP, "AT+QIACCEPT", "Accept or reject incoming TCP or UDP connection"},
	{FC41D_AT_CATALOG_CMD_QISWTMD, FC41D_AT_GROUP_TCPUDP, "AT+QISWTMD", "Switch data access mode"},
	{FC41D_AT_CATALOG_CMD_QICLOSE, FC41D_AT_GROUP_TCPUDP, "AT+QICLOSE", "Close TCP or UDP socket service"},
	{FC41D_AT_CATALOG_CMD_QIGETERROR, FC41D_AT_GROUP_TCPUDP, "AT+QIGETERROR", "Query TCP or UDP result code"},
	{FC41D_AT_CATALOG_CMD_ATO, FC41D_AT_GROUP_TCPUDP, "ATO", "Enter transparent transmission mode"},
	{FC41D_AT_CATALOG_CMD_ESCAPE, FC41D_AT_GROUP_TCPUDP, "+++", "Exit transparent transmission mode"},
	{FC41D_AT_CATALOG_CMD_QSSLCFG, FC41D_AT_GROUP_SSL, "AT+QSSLCFG", "Configure SSL context parameters"},
	{FC41D_AT_CATALOG_CMD_QSSLCERT, FC41D_AT_GROUP_SSL, "AT+QSSLCERT", "Upload, download, or delete SSL certificate"},
	{FC41D_AT_CATALOG_CMD_QSSLOPEN, FC41D_AT_GROUP_SSL, "AT+QSSLOPEN", "Open SSL client"},
	{FC41D_AT_CATALOG_CMD_QSSLSEND, FC41D_AT_GROUP_SSL, "AT+QSSLSEND", "Send data via SSL client"},
	{FC41D_AT_CATALOG_CMD_QSSLRECV, FC41D_AT_GROUP_SSL, "AT+QSSLRECV", "Read data received by SSL client"},
	{FC41D_AT_CATALOG_CMD_QSSLSTATE, FC41D_AT_GROUP_SSL, "AT+QSSLSTATE", "Query SSL client state"},
	{FC41D_AT_CATALOG_CMD_QSSLCLOSE, FC41D_AT_GROUP_SSL, "AT+QSSLCLOSE", "Close SSL client"},
	{FC41D_AT_CATALOG_CMD_QMTCFG, FC41D_AT_GROUP_MQTT, "AT+QMTCFG", "Configure optional parameters of MQTT client"},
	{FC41D_AT_CATALOG_CMD_QMTOPEN, FC41D_AT_GROUP_MQTT, "AT+QMTOPEN", "Open a session between MQTT client and server"},
	{FC41D_AT_CATALOG_CMD_QMTCLOSE, FC41D_AT_GROUP_MQTT, "AT+QMTCLOSE", "Close a session between MQTT client and server"},
	{FC41D_AT_CATALOG_CMD_QMTCONN, FC41D_AT_GROUP_MQTT, "AT+QMTCONN", "Connect a client to MQTT server"},
	{FC41D_AT_CATALOG_CMD_QMTDISC, FC41D_AT_GROUP_MQTT, "AT+QMTDISC", "Disconnect a client from MQTT server"},
	{FC41D_AT_CATALOG_CMD_QMTSUB, FC41D_AT_GROUP_MQTT, "AT+QMTSUB", "Subscribe to topics"},
	{FC41D_AT_CATALOG_CMD_QMTUNS, FC41D_AT_GROUP_MQTT, "AT+QMTUNS", "Unsubscribe from topics"},
	{FC41D_AT_CATALOG_CMD_QMTPUB, FC41D_AT_GROUP_MQTT, "AT+QMTPUB", "Publish message via MQTT server"},
	{FC41D_AT_CATALOG_CMD_QMTRECV, FC41D_AT_GROUP_MQTT, "AT+QMTRECV", "Read messages published by MQTT server"},
	{FC41D_AT_CATALOG_CMD_QHTTPCFG, FC41D_AT_GROUP_HTTP, "AT+QHTTPCFG", "Configure parameters for HTTP(S) client"},
	{FC41D_AT_CATALOG_CMD_QHTTPGET, FC41D_AT_GROUP_HTTP, "AT+QHTTPGET", "Send GET request to HTTP(S) server"},
	{FC41D_AT_CATALOG_CMD_QHTTPPOST, FC41D_AT_GROUP_HTTP, "AT+QHTTPPOST", "Send POST request to HTTP(S) server"},
	{FC41D_AT_CATALOG_CMD_QHTTPPUT, FC41D_AT_GROUP_HTTP, "AT+QHTTPPUT", "Send PUT request to HTTP(S) server"},
	{FC41D_AT_CATALOG_CMD_QHTTPREAD, FC41D_AT_GROUP_HTTP, "AT+QHTTPREAD", "Read response data of HTTP(S) request"},
};

_Static_assert((sizeof(gFc41dAtCmdInfoTable) / sizeof(gFc41dAtCmdInfoTable[0])) == FC41D_AT_CATALOG_CMD_MAX,
			   "FC41D AT command catalog is out of sync");

static bool fc41dAtMatchPattern(const uint8_t *lineBuf, uint16_t lineLen, const char *pattern);
static bool fc41dAtMatchPatterns(const uint8_t *lineBuf, uint16_t lineLen, const char *const *patterns, uint8_t patternCnt);
static bool fc41dAtIsStandardAtName(const char *name);
static eFc41dStatus fc41dAtBuildSimpleCmd(char *cmdBuf, uint16_t cmdBufSize, const char *cmdText);
static eFc41dStatus fc41dAtBuildFmtCmd(char *cmdBuf, uint16_t cmdBufSize, const char *fmt, ...);

void fc41dAtGetBaseOpt(stFc41dAtOpt *opt)
{
	if (opt == NULL) {
		return;
	}

	(void)memset(opt, 0, sizeof(*opt));
	opt->responseDonePatterns = gFc41dAtRespDone;
	opt->responseDonePatternCnt = (uint8_t)(sizeof(gFc41dAtRespDone) / sizeof(gFc41dAtRespDone[0]));
	opt->errorPatterns = gFc41dAtErrPat;
	opt->errorPatternCnt = (uint8_t)(sizeof(gFc41dAtErrPat) / sizeof(gFc41dAtErrPat[0]));
	opt->totalToutMs = 5000U;
	opt->responseToutMs = 1000U;
	opt->promptToutMs = 1000U;
	opt->finalToutMs = 5000U;
	opt->needPrompt = false;
}

bool fc41dAtIsUrc(const uint8_t *lineBuf, uint16_t lineLen)
{
	if ((lineBuf == NULL) || (lineLen == 0U)) {
		return false;
	}

	return fc41dAtMatchPatterns(lineBuf, lineLen, gFc41dAtUrcPatterns,
								(uint8_t)(sizeof(gFc41dAtUrcPatterns) / sizeof(gFc41dAtUrcPatterns[0])));
}

const char *fc41dAtGetCmdText(eFc41dAtCmd cmd)
{
	switch (cmd) {
		case FC41D_AT_CMD_CHECK_ALIVE:
			return gFc41dAtCmdCheckAlive;
		case FC41D_AT_CMD_RESET:
			return gFc41dAtCmdReset;
		case FC41D_AT_CMD_STA_STOP:
			return gFc41dAtCmdStaStop;
		case FC41D_AT_CMD_BLE_INIT_GATTS:
			return gFc41dAtCmdBleInitGatts;
		case FC41D_AT_CMD_BLE_ADDR_QUERY:
			return gFc41dAtCmdBleAddrQuery;
		case FC41D_AT_CMD_VERSION_QUERY:
			return gFc41dAtCmdVersionQuery;
		case FC41D_AT_CMD_BLE_ADV_START:
			return gFc41dAtCmdBleAdvStart;
		default:
			return NULL;
	}
}

eFc41dStatus fc41dExecAtCmd(eFc41dMapType device, eFc41dAtCmd cmd, const stFc41dAtOpt *opt, stFc41dAtResp *resp)
{
	const char *cmdText = fc41dAtGetCmdText(cmd);

	if (cmdText == NULL) {
		return FC41D_STATUS_INVALID_PARAM;
	}

	return fc41dExecAtText(device, cmdText, opt, resp);
}

eFc41dStatus fc41dExecAtText(eFc41dMapType device, const char *cmdText, const stFc41dAtOpt *opt, stFc41dAtResp *resp)
{
	uint8_t cmdBuf[FC41D_AT_TEXT_BUF_MAX];
	uint32_t cmdLen;

	if (cmdText == NULL) {
		return FC41D_STATUS_INVALID_PARAM;
	}

	cmdLen = (uint32_t)strlen(cmdText);
	if ((cmdLen == 0U) || (cmdLen >= (uint32_t)(sizeof(cmdBuf) - 2U))) {
		return FC41D_STATUS_INVALID_PARAM;
	}

	(void)memcpy(cmdBuf, cmdText, cmdLen);
	if ((cmdLen < 2U) || (cmdBuf[cmdLen - 2U] != '\r') || (cmdBuf[cmdLen - 1U] != '\n')) {
		cmdBuf[cmdLen++] = '\r';
		cmdBuf[cmdLen++] = '\n';
	}

	return fc41dExecAt(device, cmdBuf, (uint16_t)cmdLen, NULL, 0U, opt, resp);
}

eFc41dStatus fc41dAtCheckAlive(eFc41dMapType device)
{
	return fc41dExecAtCmd(device, FC41D_AT_CMD_CHECK_ALIVE, NULL, NULL);
}

uint16_t fc41dAtGetCmdInfoCount(void)
{
	return (uint16_t)(sizeof(gFc41dAtCmdInfoTable) / sizeof(gFc41dAtCmdInfoTable[0]));
}

const stFc41dAtCmdInfo *fc41dAtGetCmdInfo(eFc41dAtCatalogCmd cmd)
{
	if ((uint32_t)cmd >= (uint32_t)fc41dAtGetCmdInfoCount()) {
		return NULL;
	}

	return &gFc41dAtCmdInfoTable[cmd];
}

const stFc41dAtCmdInfo *fc41dAtGetCmdInfoByIndex(uint16_t index)
{
	if ((uint32_t)index >= (uint32_t)fc41dAtGetCmdInfoCount()) {
		return NULL;
	}

	return &gFc41dAtCmdInfoTable[index];
}

const stFc41dAtCmdInfo *fc41dAtFindCmdInfo(const char *name)
{
	uint16_t idx;

	if (name == NULL) {
		return NULL;
	}

	for (idx = 0U; idx < fc41dAtGetCmdInfoCount(); idx++) {
		if (strcmp(gFc41dAtCmdInfoTable[idx].name, name) == 0) {
			return &gFc41dAtCmdInfoTable[idx];
		}
	}

	return NULL;
}

eFc41dStatus fc41dAtBuildExecCmd(char *cmdBuf, uint16_t cmdBufSize, eFc41dAtCatalogCmd cmd)
{
	const stFc41dAtCmdInfo *cmdInfo = fc41dAtGetCmdInfo(cmd);

	if (cmdInfo == NULL) {
		return FC41D_STATUS_INVALID_PARAM;
	}

	return fc41dAtBuildSimpleCmd(cmdBuf, cmdBufSize, cmdInfo->name);
}

eFc41dStatus fc41dAtBuildQueryCmd(char *cmdBuf, uint16_t cmdBufSize, eFc41dAtCatalogCmd cmd)
{
	const stFc41dAtCmdInfo *cmdInfo = fc41dAtGetCmdInfo(cmd);

	if ((cmdInfo == NULL) || !fc41dAtIsStandardAtName(cmdInfo->name)) {
		return FC41D_STATUS_UNSUPPORTED;
	}

	return fc41dAtBuildFmtCmd(cmdBuf, cmdBufSize, "%s?", cmdInfo->name);
}

eFc41dStatus fc41dAtBuildTestCmd(char *cmdBuf, uint16_t cmdBufSize, eFc41dAtCatalogCmd cmd)
{
	const stFc41dAtCmdInfo *cmdInfo = fc41dAtGetCmdInfo(cmd);

	if ((cmdInfo == NULL) || !fc41dAtIsStandardAtName(cmdInfo->name)) {
		return FC41D_STATUS_UNSUPPORTED;
	}

	return fc41dAtBuildFmtCmd(cmdBuf, cmdBufSize, "%s=?", cmdInfo->name);
}

eFc41dStatus fc41dAtBuildSetCmd(char *cmdBuf, uint16_t cmdBufSize, eFc41dAtCatalogCmd cmd, const char *args)
{
	const stFc41dAtCmdInfo *cmdInfo = fc41dAtGetCmdInfo(cmd);

	if ((cmdInfo == NULL) || (args == NULL) || !fc41dAtIsStandardAtName(cmdInfo->name)) {
		return FC41D_STATUS_INVALID_PARAM;
	}

	return fc41dAtBuildFmtCmd(cmdBuf, cmdBufSize, "%s=%s", cmdInfo->name, args);
}

eFc41dStatus fc41dAtBuildBleNameCmd(char *cmdBuf, uint16_t cmdBufSize, const char *name)
{
	if (name == NULL) {
		return FC41D_STATUS_INVALID_PARAM;
	}

	return fc41dAtBuildSetCmd(cmdBuf, cmdBufSize, FC41D_AT_CATALOG_CMD_QBLENAME, name);
}

eFc41dStatus fc41dAtBuildBleGattServiceCmd(char *cmdBuf, uint16_t cmdBufSize, uint16_t serviceUuid)
{
	return fc41dAtBuildFmtCmd(cmdBuf, cmdBufSize, "%s=%04X",
							  fc41dAtGetCmdInfo(FC41D_AT_CATALOG_CMD_QBLEGATTSSRV)->name, serviceUuid);
}

eFc41dStatus fc41dAtBuildBleGattCharCmd(char *cmdBuf, uint16_t cmdBufSize, uint16_t charUuid)
{
	return fc41dAtBuildFmtCmd(cmdBuf, cmdBufSize, "%s=%04X",
							  fc41dAtGetCmdInfo(FC41D_AT_CATALOG_CMD_QBLEGATTSCHAR)->name, charUuid);
}

eFc41dStatus fc41dAtBuildBleAdvParamCmd(char *cmdBuf, uint16_t cmdBufSize, uint16_t intervalMin, uint16_t intervalMax)
{
	return fc41dAtBuildFmtCmd(cmdBuf, cmdBufSize, "%s=%u,%u",
							  fc41dAtGetCmdInfo(FC41D_AT_CATALOG_CMD_QBLEADVPARAM)->name,
							  (unsigned int)intervalMin, (unsigned int)intervalMax);
}

eFc41dStatus fc41dAtBuildBleAdvDataCmd(char *cmdBuf, uint16_t cmdBufSize, const uint8_t *advData, uint16_t advLen)
{
	uint16_t idx;
	uint32_t offset;
	int written;

	if ((cmdBuf == NULL) || (cmdBufSize == 0U) || ((advLen > 0U) && (advData == NULL))) {
		return FC41D_STATUS_INVALID_PARAM;
	}

	written = snprintf(cmdBuf, cmdBufSize, "%s=", fc41dAtGetCmdInfo(FC41D_AT_CATALOG_CMD_QBLEADVDATA)->name);
	if ((written < 0) || ((uint32_t)written >= cmdBufSize)) {
		return FC41D_STATUS_ERROR;
	}

	offset = (uint32_t)written;
	for (idx = 0U; idx < advLen; idx++) {
		written = snprintf(&cmdBuf[offset], (size_t)(cmdBufSize - offset), "%02X", advData[idx]);
		if ((written < 0) || ((uint32_t)written >= (uint32_t)(cmdBufSize - offset))) {
			cmdBuf[0] = '\0';
			return FC41D_STATUS_ERROR;
		}
		offset += (uint32_t)written;
	}

	return FC41D_STATUS_OK;
}

static bool fc41dAtIsStandardAtName(const char *name)
{
	return (name != NULL) && (strncmp(name, "AT+", 3U) == 0);
}

static eFc41dStatus fc41dAtBuildSimpleCmd(char *cmdBuf, uint16_t cmdBufSize, const char *cmdText)
{
	if (cmdText == NULL) {
		return FC41D_STATUS_INVALID_PARAM;
	}

	return fc41dAtBuildFmtCmd(cmdBuf, cmdBufSize, "%s", cmdText);
}

static bool fc41dAtMatchPattern(const uint8_t *lineBuf, uint16_t lineLen, const char *pattern)
{
	uint32_t patLen;

	if ((lineBuf == NULL) || (pattern == NULL)) {
		return false;
	}

	patLen = (uint32_t)strlen(pattern);
	if (patLen == 0U) {
		return false;
	}

	if (pattern[patLen - 1U] == '*') {
		patLen--;
		if ((patLen == 0U) || (lineLen < patLen)) {
			return false;
		}
		return memcmp(lineBuf, pattern, patLen) == 0;
	}

	if (lineLen != patLen) {
		return false;
	}

	return memcmp(lineBuf, pattern, patLen) == 0;
}

static bool fc41dAtMatchPatterns(const uint8_t *lineBuf, uint16_t lineLen, const char *const *patterns, uint8_t patternCnt)
{
	uint8_t idx;

	if ((patterns == NULL) || (patternCnt == 0U)) {
		return false;
	}

	for (idx = 0U; idx < patternCnt; idx++) {
		if (fc41dAtMatchPattern(lineBuf, lineLen, patterns[idx])) {
			return true;
		}
	}

	return false;
}

static eFc41dStatus fc41dAtBuildFmtCmd(char *cmdBuf, uint16_t cmdBufSize, const char *fmt, ...)
{
	va_list args;
	int written;

	if ((cmdBuf == NULL) || (cmdBufSize == 0U) || (fmt == NULL)) {
		return FC41D_STATUS_INVALID_PARAM;
	}

	va_start(args, fmt);
	written = vsnprintf(cmdBuf, cmdBufSize, fmt, args);
	va_end(args);
	if ((written < 0) || ((uint32_t)written >= cmdBufSize)) {
		cmdBuf[0] = '\0';
		return FC41D_STATUS_ERROR;
	}

	return FC41D_STATUS_OK;
}
/**************************End of file********************************/
