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

BOOLEAN SpdHwStartIo(PVOID DeviceExtension, PSCSI_REQUEST_BLOCK Srb0)
{
    PVOID Srb = Srb0;
    SPD_ENTER(io,
        ASSERT(DISPATCH_LEVEL >= KeGetCurrentIrql()));

    UCHAR SrbStatus;
    switch (SrbGetSrbFunction(Srb))
    {
    case SRB_FUNCTION_EXECUTE_SCSI:
        SrbStatus = SpdSrbExecuteScsi(DeviceExtension, Srb);
        break;
    case SRB_FUNCTION_ABORT_COMMAND:
        SrbStatus = SpdSrbAbortCommand(DeviceExtension, Srb);
        break;
    case SRB_FUNCTION_RESET_BUS:
        SrbStatus = SpdSrbResetBus(DeviceExtension, Srb);
        break;
    case SRB_FUNCTION_RESET_DEVICE:
        SrbStatus = SpdSrbResetDevice(DeviceExtension, Srb);
        break;
    case SRB_FUNCTION_FLUSH:
        SrbStatus = SpdSrbFlush(DeviceExtension, Srb);
        break;
    case SRB_FUNCTION_SHUTDOWN:
        SrbStatus = SpdSrbShutdown(DeviceExtension, Srb);
        break;
    case SRB_FUNCTION_IO_CONTROL:
        SrbStatus = SpdSrbIoControl(DeviceExtension, Srb);
        break;
    case SRB_FUNCTION_PNP:
        SrbStatus = SpdSrbPnp(DeviceExtension, Srb);
        break;
    case SRB_FUNCTION_WMI:
        SrbStatus = SpdSrbWmi(DeviceExtension, Srb);
        break;
    case SRB_FUNCTION_DUMP_POINTERS:
        SrbStatus = SpdSrbDumpPointers(DeviceExtension, Srb);
        break;
    case SRB_FUNCTION_FREE_DUMP_POINTERS:
        SrbStatus = SpdSrbFreeDumpPointers(DeviceExtension, Srb);
        break;
    default:
        SrbStatus = SRB_STATUS_INVALID_REQUEST;
        break;
    }

    if (SRB_STATUS_PENDING != SRB_STATUS(SrbStatus))
        SpdSrbComplete(DeviceExtension, Srb, SrbStatus);

    SPD_LEAVE(io,
        "DeviceExtension=%p, %s", " = %s%s",
        DeviceExtension, SrbFunctionSym(SrbGetSrbFunction(Srb)),
        SrbStatusSym(SrbGetSrbStatus(Srb)), SrbStatusMaskSym(SrbGetSrbStatus(Srb)));

    return TRUE;
}

UCHAR SpdSrbAbortCommand(PVOID DeviceExtension, PVOID Srb)
{
    return SRB_STATUS_INVALID_REQUEST;
}

UCHAR SpdSrbResetBus(PVOID DeviceExtension, PVOID Srb)
{
    return SRB_STATUS_INVALID_REQUEST;
}

UCHAR SpdSrbResetDevice(PVOID DeviceExtension, PVOID Srb)
{
    return SRB_STATUS_INVALID_REQUEST;
}

UCHAR SpdSrbFlush(PVOID DeviceExtension, PVOID Srb)
{
    return SRB_STATUS_INVALID_REQUEST;
}

UCHAR SpdSrbShutdown(PVOID DeviceExtension, PVOID Srb)
{
    return SRB_STATUS_INVALID_REQUEST;
}

UCHAR SpdSrbIoControl(PVOID DeviceExtension, PVOID Srb)
{
    return SRB_STATUS_INVALID_REQUEST;
}

UCHAR SpdSrbPnp(PVOID DeviceExtension, PVOID Srb)
{
    return SRB_STATUS_INVALID_REQUEST;
}

UCHAR SpdSrbWmi(PVOID DeviceExtension, PVOID Srb)
{
    return SRB_STATUS_INVALID_REQUEST;
}

UCHAR SpdSrbDumpPointers(PVOID DeviceExtension, PVOID Srb)
{
    return SRB_STATUS_INVALID_REQUEST;
}

UCHAR SpdSrbFreeDumpPointers(PVOID DeviceExtension, PVOID Srb)
{
    return SRB_STATUS_INVALID_REQUEST;
}
