/************************************************************************************
* @file     : gd25qxxx_port.h
* @brief    : GD25Qxxx project port-layer declarations.
* @details  : This file keeps project-level SPI device mapping and timing hooks
*             separate from the reusable GD25Qxxx core implementation.
***********************************************************************************/
#ifndef GD25QXXX_PORT_H
#define GD25QXXX_PORT_H

#include <stdbool.h>
#include <stdint.h>

#include "gd25qxxx.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef GD25QXXX_CONSOLE_SUPPORT
#define GD25QXXX_CONSOLE_SUPPORT              1
#endif

#ifndef GD25QXXX_PORT_READ_FILL_DATA
#define GD25QXXX_PORT_READ_FILL_DATA          0xFFU
#endif

void gd25qxxxPortGetDefBind(stGd25qxxxPortSpiBinding *bind);
void gd25qxxxPortGetDefCfg(eGd25qxxxMapType device, stGd25qxxxCfg *cfg);
eDrvStatus gd25qxxxPortSetHardSpi(stGd25qxxxPortSpiBinding *bind, eDrvSpiPortMap spi);
bool gd25qxxxPortIsValidBind(const stGd25qxxxPortSpiBinding *bind);
bool gd25qxxxPortHasValidSpiIf(const stGd25qxxxPortSpiBinding *bind);
const stGd25qxxxPortSpiInterface *gd25qxxxPortGetSpiIf(const stGd25qxxxPortSpiBinding *bind);
void gd25qxxxPortDelayMs(uint32_t delayMs);

#ifdef __cplusplus
}
#endif

#endif  // GD25QXXX_PORT_H
/**************************End of file********************************/
