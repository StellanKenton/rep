/***********************************************************************************
* @file     : update_debug.c
* @brief    : Optional update service debug helpers.
* @details  : Keeps state-name mapping and a weak transition callback outside
*             the reusable update core.
* @author   : GitHub Copilot
* @date     : 2026-04-16
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
**********************************************************************************/
#include "update_debug.h"

__attribute__((weak)) void updateDbgOnStateChanged(eUpdateState from, eUpdateState to)
{
    (void)from;
    (void)to;
}

const char *updateDbgGetStateName(eUpdateState state)
{
    switch (state) {
        case E_UPDATE_STATE_UNINIT:
            return "UNINIT";
        case E_UPDATE_STATE_IDLE:
            return "IDLE";
        case E_UPDATE_STATE_CHECK_REQUEST:
            return "CHECK_REQUEST";
        case E_UPDATE_STATE_VALIDATE_STAGING:
            return "VALIDATE_STAGING";
        case E_UPDATE_STATE_PREPARE_BACKUP:
            return "PREPARE_BACKUP";
        case E_UPDATE_STATE_BACKUP_RUN_APP:
            return "BACKUP_RUN_APP";
        case E_UPDATE_STATE_VERIFY_BACKUP:
            return "VERIFY_BACKUP";
        case E_UPDATE_STATE_ERASE_TARGET:
            return "ERASE_TARGET";
        case E_UPDATE_STATE_PROGRAM_TARGET:
            return "PROGRAM_TARGET";
        case E_UPDATE_STATE_VERIFY_TARGET:
            return "VERIFY_TARGET";
        case E_UPDATE_STATE_COMMIT_RESULT:
            return "COMMIT_RESULT";
        case E_UPDATE_STATE_ROLLBACK_ERASE_TARGET:
            return "ROLLBACK_ERASE_TARGET";
        case E_UPDATE_STATE_ROLLBACK_PROGRAM_BACKUP:
            return "ROLLBACK_PROGRAM_BACKUP";
        case E_UPDATE_STATE_VERIFY_ROLLBACK:
            return "VERIFY_ROLLBACK";
        case E_UPDATE_STATE_JUMP_TARGET:
            return "JUMP_TARGET";
        case E_UPDATE_STATE_ERROR:
            return "ERROR";
        default:
            return "UNKNOWN";
    }
}

void updateDbgNotifyStateChanged(eUpdateState from, eUpdateState to)
{
    updateDbgOnStateChanged(from, to);
}

/**************************End of file********************************/
