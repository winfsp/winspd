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
    case SRB_FUNCTION_RESET_LOGICAL_UNIT:
        SrbStatus = SpdSrbResetLogicalUnit(DeviceExtension, Srb);
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

    switch (SRB_STATUS(SrbStatus))
    {
    case SRB_STATUS_PENDING:
        /* no completion */
        break;
    case SRB_STATUS_INTERNAL_ERROR:
        if (STATUS_SUCCESS == SrbGetSystemStatus(Srb))
            SrbSetSystemStatus(Srb, (ULONG)STATUS_INVALID_PARAMETER);
        /* fall through */
    default:
        SpdSrbComplete(DeviceExtension, Srb, SrbStatus);
        break;
    }

    SPD_LEAVE(io,
        "%p, %s", "",
        DeviceExtension, SpdStringizeSrb(Srb, SpdDebugLogBuf, sizeof SpdDebugLogBuf));

    return TRUE;
}

UCHAR SpdSrbAbortCommand(PVOID DeviceExtension, PVOID Srb)
{
    SPD_STORAGE_UNIT *StorageUnit;
    NTSTATUS Result;

    StorageUnit = SpdGetStorageUnit(DeviceExtension, Srb);
    if (0 == StorageUnit)
        return SRB_STATUS_NO_DEVICE;

    Result = SpdIoqCancelSrb(StorageUnit->Ioq, Srb);
    if (!NT_SUCCESS(Result))
        return SRB_STATUS_ABORT_FAILED;

    return SRB_STATUS_SUCCESS;
}

UCHAR SpdSrbResetBus(PVOID DeviceExtension, PVOID Srb)
{
    UCHAR PathId, TargetId, Lun;

    SrbGetPathTargetLun(Srb, &PathId, &TargetId, &Lun);
    if (0 != PathId)
        return SRB_STATUS_NO_DEVICE;

    SpdHwResetBus(DeviceExtension, 0);

    return SRB_STATUS_SUCCESS;
}

UCHAR SpdSrbResetDevice(PVOID DeviceExtension, PVOID Srb)
{
    SPD_STORAGE_UNIT *StorageUnit;
    UCHAR PathId, TargetId, Lun;

    SrbGetPathTargetLun(Srb, &PathId, &TargetId, &Lun);
    StorageUnit = SpdGetStorageUnitByBtl(DeviceExtension,
        PathId, TargetId, 0/*Lun: !valid*/);
    if (0 == StorageUnit)
        return SRB_STATUS_NO_DEVICE;

    SpdIoqReset(StorageUnit->Ioq, FALSE);

    return SRB_STATUS_SUCCESS;
}

UCHAR SpdSrbResetLogicalUnit(PVOID DeviceExtension, PVOID Srb)
{
    SPD_STORAGE_UNIT *StorageUnit;

    StorageUnit = SpdGetStorageUnit(DeviceExtension, Srb);
    if (0 == StorageUnit)
        return SRB_STATUS_NO_DEVICE;

    SpdIoqReset(StorageUnit->Ioq, FALSE);

    return SRB_STATUS_SUCCESS;
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
