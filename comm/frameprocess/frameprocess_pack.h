/************************************************************************************
* @file     : frameprocess_pack.h
* @brief    : Frame process business pack handlers.
* @details  : Converts parsed RX commands into TX store updates and runtime changes.
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef FRAMEPROCESS_PACK_H
#define FRAMEPROCESS_PACK_H

#include <stdbool.h>
#include <stdint.h>

typedef struct stFrmProcCtx stFrmProcCtx;

#ifdef __cplusplus
extern "C" {
#endif

bool frmProcPackOnRx(stFrmProcCtx *ctx, uint8_t cmd);

#ifdef __cplusplus
}
#endif

#endif  // FRAMEPROCESS_PACK_H
/**************************End of file********************************/
