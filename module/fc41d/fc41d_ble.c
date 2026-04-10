/***********************************************************************************
* @file     : fc41d_ble.c
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

stRingBuffer *fc41dBleGetRxRingBuffer(eFc41dMapType device)
{
	return fc41dGetRxRbByChannel(fc41dGetCtx(device), FC41D_RX_CHANNEL_BLE);
}

uint32_t fc41dBleRead(eFc41dMapType device, uint8_t *buffer, uint32_t length)
{
	stRingBuffer *rb = fc41dBleGetRxRingBuffer(device);
	return (rb != NULL) ? ringBufferRead(rb, buffer, length) : 0U;
}

uint32_t fc41dBlePeek(eFc41dMapType device, uint8_t *buffer, uint32_t length)
{
	stRingBuffer *rb = fc41dBleGetRxRingBuffer(device);
	return (rb != NULL) ? ringBufferPeek(rb, buffer, length) : 0U;
}

uint32_t fc41dBleDiscard(eFc41dMapType device, uint32_t length)
{
	stRingBuffer *rb = fc41dBleGetRxRingBuffer(device);
	return (rb != NULL) ? ringBufferDiscard(rb, length) : 0U;
}

eFc41dStatus fc41dBleClearRx(eFc41dMapType device)
{
	stRingBuffer *rb = fc41dBleGetRxRingBuffer(device);

	if (rb == NULL) {
		return FC41D_STATUS_INVALID_PARAM;
	}

	return (ringBufferReset(rb) == RINGBUFFER_OK) ? FC41D_STATUS_OK : FC41D_STATUS_ERROR;
}

/**************************End of file********************************/
