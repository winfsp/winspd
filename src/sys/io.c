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

    switch (SrbGetSrbFunction(Srb))
    {
    case SRB_FUNCTION_EXECUTE_SCSI:
        SpdSrbExecuteScsi(DeviceExtension, Srb);
        break;
    case SRB_FUNCTION_ABORT_COMMAND:
        SpdSrbAbortCommand(DeviceExtension, Srb);
        break;
    case SRB_FUNCTION_RESET_BUS:
        SpdSrbResetBus(DeviceExtension, Srb);
        break;
    case SRB_FUNCTION_RESET_DEVICE:
        SpdSrbResetDevice(DeviceExtension, Srb);
        break;
    case SRB_FUNCTION_FLUSH:
        SpdSrbFlush(DeviceExtension, Srb);
        break;
    case SRB_FUNCTION_SHUTDOWN:
        SpdSrbShutdown(DeviceExtension, Srb);
        break;
    case SRB_FUNCTION_IO_CONTROL:
        SpdSrbIoControl(DeviceExtension, Srb);
        break;
    case SRB_FUNCTION_PNP:
        SpdSrbPnp(DeviceExtension, Srb);
        break;
    case SRB_FUNCTION_WMI:
        SpdSrbWmi(DeviceExtension, Srb);
        break;
    case SRB_FUNCTION_DUMP_POINTERS:
        SpdSrbDumpPointers(DeviceExtension, Srb);
        break;
    default:
        SpdSrbUnsupported(DeviceExtension, Srb);
        break;
    }

    SPD_LEAVE(io,
        "DeviceExtension=%p, %s", " = %s%s",
        DeviceExtension, SrbFunctionSym(SrbGetSrbFunction(Srb)),
        SrbStatusSym(SrbGetSrbStatus(Srb)), SrbStatusMaskSym(SrbGetSrbStatus(Srb)));

    return TRUE;
}

VOID SpdSrbAbortCommand(PVOID DeviceExtension, PVOID Srb)
{
    SpdSrbUnsupported(DeviceExtension, Srb);
}

VOID SpdSrbResetBus(PVOID DeviceExtension, PVOID Srb)
{
    SpdSrbUnsupported(DeviceExtension, Srb);
}

VOID SpdSrbResetDevice(PVOID DeviceExtension, PVOID Srb)
{
    SpdSrbUnsupported(DeviceExtension, Srb);
}

VOID SpdSrbFlush(PVOID DeviceExtension, PVOID Srb)
{
    SpdSrbUnsupported(DeviceExtension, Srb);
}

VOID SpdSrbShutdown(PVOID DeviceExtension, PVOID Srb)
{
    SpdSrbUnsupported(DeviceExtension, Srb);
}

VOID SpdSrbIoControl(PVOID DeviceExtension, PVOID Srb)
{
    SpdSrbUnsupported(DeviceExtension, Srb);
}

VOID SpdSrbPnp(PVOID DeviceExtension, PVOID Srb)
{
    SpdSrbUnsupported(DeviceExtension, Srb);
}

VOID SpdSrbWmi(PVOID DeviceExtension, PVOID Srb)
{
    SpdSrbUnsupported(DeviceExtension, Srb);
}

VOID SpdSrbDumpPointers(PVOID DeviceExtension, PVOID Srb)
{
    SpdSrbUnsupported(DeviceExtension, Srb);
}

VOID SpdSrbUnsupported(PVOID DeviceExtension, PVOID Srb)
{
    SrbSetSrbStatus(Srb, SRB_STATUS_INVALID_REQUEST);
    SpdSrbComplete(DeviceExtension, Srb);
}
