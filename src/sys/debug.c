/**
 * @file sys/debug.c
 *
 * @copyright 2018 Bill Zissimopoulos
 */
/*
 * This file is part of WinSpd.
 *
 * You can redistribute it and/or modify it under the terms of the GNU
 * General Public License version 3 as published by the Free Software
 * Foundation.
 *
 * Licensees holding a valid commercial license may use this software
 * in accordance with the commercial license agreement provided in
 * conjunction with the software.  The terms and conditions of any such
 * commercial license agreement shall govern, supersede, and render
 * ineffective any application of the GPLv3 license to this software,
 * notwithstanding of any reference thereto in the software or
 * associated repository.
 */

#include <sys/driver.h>

#if DBG
#define SYM(x)                          case x: return #x;

const char *SrbFunctionSym(ULONG Function)
{
    switch (Function)
    {
    SYM(SRB_FUNCTION_EXECUTE_SCSI)
    SYM(SRB_FUNCTION_CLAIM_DEVICE)
    SYM(SRB_FUNCTION_IO_CONTROL)
    SYM(SRB_FUNCTION_RECEIVE_EVENT)
    SYM(SRB_FUNCTION_RELEASE_QUEUE)
    SYM(SRB_FUNCTION_ATTACH_DEVICE)
    SYM(SRB_FUNCTION_RELEASE_DEVICE)
    SYM(SRB_FUNCTION_SHUTDOWN)
    SYM(SRB_FUNCTION_FLUSH)
    SYM(SRB_FUNCTION_PROTOCOL_COMMAND)
    SYM(SRB_FUNCTION_ABORT_COMMAND)
    SYM(SRB_FUNCTION_RELEASE_RECOVERY)
    SYM(SRB_FUNCTION_RESET_BUS)
    SYM(SRB_FUNCTION_RESET_DEVICE)
    SYM(SRB_FUNCTION_TERMINATE_IO)
    SYM(SRB_FUNCTION_FLUSH_QUEUE)
    SYM(SRB_FUNCTION_REMOVE_DEVICE)
    SYM(SRB_FUNCTION_WMI)
    SYM(SRB_FUNCTION_LOCK_QUEUE)
    SYM(SRB_FUNCTION_UNLOCK_QUEUE)
    SYM(SRB_FUNCTION_QUIESCE_DEVICE)
    SYM(SRB_FUNCTION_RESET_LOGICAL_UNIT)
    SYM(SRB_FUNCTION_SET_LINK_TIMEOUT)
    SYM(SRB_FUNCTION_LINK_TIMEOUT_OCCURRED)
    SYM(SRB_FUNCTION_LINK_TIMEOUT_COMPLETE)
    SYM(SRB_FUNCTION_POWER)
    SYM(SRB_FUNCTION_PNP)
    SYM(SRB_FUNCTION_DUMP_POINTERS)
    SYM(SRB_FUNCTION_FREE_DUMP_POINTERS)
    SYM(SRB_FUNCTION_STORAGE_REQUEST_BLOCK)
    default:
        return "SrbFunction:Unknown";
    }
}

const char *SrbStatusSym(ULONG Status)
{
    switch (SRB_STATUS(Status))
    {
    SYM(SRB_STATUS_PENDING)
    SYM(SRB_STATUS_SUCCESS)
    SYM(SRB_STATUS_ABORTED)
    SYM(SRB_STATUS_ABORT_FAILED)
    SYM(SRB_STATUS_ERROR)
    SYM(SRB_STATUS_BUSY)
    SYM(SRB_STATUS_INVALID_REQUEST)
    SYM(SRB_STATUS_INVALID_PATH_ID)
    SYM(SRB_STATUS_NO_DEVICE)
    SYM(SRB_STATUS_TIMEOUT)
    SYM(SRB_STATUS_SELECTION_TIMEOUT)
    SYM(SRB_STATUS_COMMAND_TIMEOUT)
    SYM(SRB_STATUS_MESSAGE_REJECTED)
    SYM(SRB_STATUS_BUS_RESET)
    SYM(SRB_STATUS_PARITY_ERROR)
    SYM(SRB_STATUS_REQUEST_SENSE_FAILED)
    SYM(SRB_STATUS_NO_HBA)
    SYM(SRB_STATUS_DATA_OVERRUN)
    SYM(SRB_STATUS_UNEXPECTED_BUS_FREE)
    SYM(SRB_STATUS_PHASE_SEQUENCE_FAILURE)
    SYM(SRB_STATUS_BAD_SRB_BLOCK_LENGTH)
    SYM(SRB_STATUS_REQUEST_FLUSHED)
    SYM(SRB_STATUS_INVALID_LUN)
    SYM(SRB_STATUS_INVALID_TARGET_ID)
    SYM(SRB_STATUS_BAD_FUNCTION)
    SYM(SRB_STATUS_ERROR_RECOVERY)
    SYM(SRB_STATUS_NOT_POWERED)
    SYM(SRB_STATUS_LINK_DOWN)
    SYM(SRB_STATUS_INTERNAL_ERROR)
    default:
        return "SrbStatus:Unknown";
    }
}

const char *SrbStatusMaskSym(ULONG Status)
{
    switch (Status & (SRB_STATUS_QUEUE_FROZEN | SRB_STATUS_AUTOSENSE_VALID))
    {
    default:
    case 0:
        return "";
    case SRB_STATUS_QUEUE_FROZEN:
        return "[Qf]";
    case SRB_STATUS_AUTOSENSE_VALID:
        return "[Av]";
    case SRB_STATUS_QUEUE_FROZEN | SRB_STATUS_AUTOSENSE_VALID:
        return "[QfAv]";
    }
}
#endif
