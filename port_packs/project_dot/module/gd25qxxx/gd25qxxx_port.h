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

typedef stGd25qxxxSpiInterface stGd25qxxxPortSpiInterface;

void gd25qxxxPortGetDefCfg(eGd25qxxxMapType device, stGd25qxxxCfg *cfg);
eDrvStatus gd25qxxxPortAssembleHardSpi(stGd25qxxxCfg *cfg, uint8_t spi);
bool gd25qxxxPortIsValidCfg(const stGd25qxxxCfg *cfg);
bool gd25qxxxPortHasValidSpiIf(const stGd25qxxxCfg *cfg);
const stGd25qxxxPortSpiInterface *gd25qxxxPortGetSpiIf(const stGd25qxxxCfg *cfg);
void gd25qxxxPortDelayMs(uint32_t delayMs);

#ifdef __cplusplus
}
#endif

#endif  // GD25QXXX_PORT_H
/**************************End of file********************************/
