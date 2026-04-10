/***********************************************************************************
* @file     : fc41d_wifi.c
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

/**************************End of file********************************/
