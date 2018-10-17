/**
 * @file sys/io.c
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

BOOLEAN SpdHwStartIo(PVOID DeviceExtension, PSCSI_REQUEST_BLOCK Srb)
{
    SPD_ENTER(io,
        ASSERT(DISPATCH_LEVEL >= KeGetCurrentIrql()));

    switch (Srb->Function)
    {
    case SRB_FUNCTION_EXECUTE_SCSI:
        //SrbExecuteScsi(DeviceExtension, Srb);
        break;
    case SRB_FUNCTION_CLAIM_DEVICE:
    case SRB_FUNCTION_IO_CONTROL:
    case SRB_FUNCTION_RECEIVE_EVENT:
    case SRB_FUNCTION_RELEASE_QUEUE:
    case SRB_FUNCTION_ATTACH_DEVICE:
    case SRB_FUNCTION_RELEASE_DEVICE:
    case SRB_FUNCTION_SHUTDOWN:
    case SRB_FUNCTION_FLUSH:
    case SRB_FUNCTION_PROTOCOL_COMMAND:
    case SRB_FUNCTION_ABORT_COMMAND:
    case SRB_FUNCTION_RELEASE_RECOVERY:
    case SRB_FUNCTION_RESET_BUS:
    case SRB_FUNCTION_RESET_DEVICE:
    case SRB_FUNCTION_TERMINATE_IO:
    case SRB_FUNCTION_FLUSH_QUEUE:
    case SRB_FUNCTION_REMOVE_DEVICE:
    case SRB_FUNCTION_WMI:
    case SRB_FUNCTION_LOCK_QUEUE:
    case SRB_FUNCTION_UNLOCK_QUEUE:
    case SRB_FUNCTION_QUIESCE_DEVICE:
    case SRB_FUNCTION_RESET_LOGICAL_UNIT:
    case SRB_FUNCTION_SET_LINK_TIMEOUT:
    case SRB_FUNCTION_LINK_TIMEOUT_OCCURRED:
    case SRB_FUNCTION_LINK_TIMEOUT_COMPLETE:
    case SRB_FUNCTION_POWER:
    case SRB_FUNCTION_PNP:
    case SRB_FUNCTION_DUMP_POINTERS:
    case SRB_FUNCTION_FREE_DUMP_POINTERS:
    case SRB_FUNCTION_STORAGE_REQUEST_BLOCK:
    default:
        Srb->SrbStatus = SRB_STATUS_INVALID_REQUEST;
        break;
    }

    SPD_LEAVE(io,
        "DeviceExtension=%p, %s", " = %s%s",
        DeviceExtension, SrbFunctionSym(Srb->Function),
        SrbStatusSym(Srb->SrbStatus), SrbStatusMaskSym(Srb->SrbStatus));

    return TRUE;
}
