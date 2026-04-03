/************************************************************************************
* @file     : w25qxxx_port.h
* @brief    : W25Qxxx project port-layer declarations.
* @details  : This file keeps project-level SPI device mapping and timing hooks
*             separate from the reusable W25Qxxx core implementation.
***********************************************************************************/
#ifndef W25QXXX_PORT_H
#define W25QXXX_PORT_H

#include <stdbool.h>
#include <stdint.h>

#include "w25qxxx.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef W25QXXX_CONSOLE_SUPPORT
#define W25QXXX_CONSOLE_SUPPORT              1
#endif

#ifndef W25QXXX_PORT_READ_FILL_DATA
#define W25QXXX_PORT_READ_FILL_DATA           0xFFU
#endif

void w25qxxxPortGetDefBind(stW25qxxxPortSpiBinding *bind);
void w25qxxxPortGetDefCfg(eW25qxxxMapType device, stW25qxxxCfg *cfg);
eDrvStatus w25qxxxPortSetHardSpi(stW25qxxxPortSpiBinding *bind, eDrvSpiPortMap spi);
bool w25qxxxPortIsValidBind(const stW25qxxxPortSpiBinding *bind);
bool w25qxxxPortHasValidSpiIf(const stW25qxxxPortSpiBinding *bind);
const stW25qxxxPortSpiInterface *w25qxxxPortGetSpiIf(const stW25qxxxPortSpiBinding *bind);
void w25qxxxPortDelayMs(uint32_t delayMs);

#ifdef __cplusplus
}
#endif

#endif  // W25QXXX_PORT_H
/**************************End of file********************************/
