/**
 * @file sys/io.c
 *
 * @copyright 2018-2020 Bill Zissimopoulos
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
    SPD_ENTER(io,
        ASSERT(DISPATCH_LEVEL >= KeGetCurrentIrql()));

    PVOID Srb = Srb0;
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
    case SRB_FUNCTION_PNP:
        SrbStatus = SpdSrbPnp(DeviceExtension, Srb);
        break;
    case SRB_FUNCTION_WMI:
        SrbStatus = SpdSrbWmi(DeviceExtension, Srb);
        break;
    default:
        SrbStatus = SRB_STATUS_INVALID_REQUEST;
        break;
    }

    switch (SRB_STATUS(SrbStatus))
    {
    case SRB_STATUS_PENDING:
        /* no completion */
#if DBG
        {
            char buf[1024];
            DEBUGLOG_EX(srb, "%p, Srb=%p {%s}", DeviceExtension, Srb, SrbStringize(Srb, buf, sizeof buf));
        }
#endif
        break;
    case SRB_STATUS_INTERNAL_ERROR:
        if (STATUS_SUCCESS == SrbGetSystemStatus(Srb))
            SrbSetSystemStatus(Srb, (ULONG)STATUS_INVALID_PARAMETER);
        /* fall through */
    default:
        SpdSrbComplete(DeviceExtension, Srb, SrbStatus);
        break;
    }

    SPD_LEAVE_NOLOG(io);

    return TRUE;
}

UCHAR SpdSrbAbortCommand(PVOID DeviceExtension, PVOID Srb)
{
    SPD_STORAGE_UNIT *StorageUnit;
    NTSTATUS Result;

    StorageUnit = SpdStorageUnitReference(DeviceExtension, Srb);
    if (0 == StorageUnit)
        return SRB_STATUS_NO_DEVICE;

    Result = SpdIoqCancelSrb(StorageUnit->Ioq, Srb);

    SpdStorageUnitDereference(DeviceExtension, StorageUnit);

    return NT_SUCCESS(Result) ? SRB_STATUS_SUCCESS : SRB_STATUS_ABORT_FAILED;
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
    StorageUnit = SpdStorageUnitReferenceByBtl(DeviceExtension,
        SPD_IOCTL_BTL(PathId, TargetId, 0/* Lun is invalid! */));
    if (0 == StorageUnit)
        return SRB_STATUS_NO_DEVICE;

    SpdIoqReset(StorageUnit->Ioq, FALSE);

    SpdStorageUnitDereference(DeviceExtension, StorageUnit);

    return SRB_STATUS_SUCCESS;
}

UCHAR SpdSrbResetLogicalUnit(PVOID DeviceExtension, PVOID Srb)
{
    SPD_STORAGE_UNIT *StorageUnit;

    StorageUnit = SpdStorageUnitReference(DeviceExtension, Srb);
    if (0 == StorageUnit)
        return SRB_STATUS_NO_DEVICE;

    SpdIoqReset(StorageUnit->Ioq, FALSE);

    SpdStorageUnitDereference(DeviceExtension, StorageUnit);

    return SRB_STATUS_SUCCESS;
}

UCHAR SpdSrbFlush(PVOID DeviceExtension, PVOID Srb)
{
    /* must have already received SYNCHRONIZE CACHE; just return SUCCESS */
    return SRB_STATUS_SUCCESS;
}

UCHAR SpdSrbShutdown(PVOID DeviceExtension, PVOID Srb)
{
    /* our user mode process must be gone; just return SUCCESS */
    return SRB_STATUS_SUCCESS;
}

UCHAR SpdSrbPnp(PVOID DeviceExtension, PVOID Srb)
{
    /*
     * This function assumes use of SCSI_REQUEST_BLOCK and SCSI_PNP_REQUEST_BLOCK.
     * Revisit if/when we switch to STORAGE_REQUEST_BLOCK.
     */

    PSCSI_PNP_REQUEST_BLOCK Pnp = Srb;
    UCHAR SrbStatus = SRB_STATUS_INVALID_REQUEST;

    switch (Pnp->PnPAction)
    {
    case StorQueryCapabilities:
        if (!FlagOn(Pnp->SrbPnPFlags, SRB_PNP_FLAGS_ADAPTER_REQUEST))
        {
            PVOID DataBuffer = SrbGetDataBuffer(Srb);
            ULONG DataTransferLength = SrbGetDataTransferLength(Srb);

            if (0 == DataBuffer ||
                sizeof(STOR_DEVICE_CAPABILITIES) > DataTransferLength)
                break;

            PSTOR_DEVICE_CAPABILITIES DeviceCapabilities = DataBuffer;
            SpdPnpSetDeviceCapabilities(DeviceCapabilities);

            SrbStatus = SRB_STATUS_SUCCESS;
        }
        break;
    }

    return SrbStatus;
}

UCHAR SpdSrbWmi(PVOID DeviceExtension, PVOID Srb)
{
    return SRB_STATUS_INVALID_REQUEST;
}

NTSTATUS SpdNtStatusFromStorStatus(ULONG StorStatus)
{
    switch (StorStatus)
    {
    case STOR_STATUS_SUCCESS:
        return STATUS_SUCCESS;
    case STOR_STATUS_UNSUCCESSFUL:
        return STATUS_UNSUCCESSFUL;
    case STOR_STATUS_NOT_IMPLEMENTED:
        return STATUS_NOT_IMPLEMENTED;
    case STOR_STATUS_INSUFFICIENT_RESOURCES:
        return STATUS_INSUFFICIENT_RESOURCES;
    case STOR_STATUS_BUFFER_TOO_SMALL:
        return STATUS_BUFFER_TOO_SMALL;
    case STOR_STATUS_ACCESS_DENIED:
        return STATUS_ACCESS_DENIED;
    case STOR_STATUS_INVALID_PARAMETER:
        return STATUS_INVALID_PARAMETER;
    case STOR_STATUS_INVALID_DEVICE_REQUEST:
        return STATUS_INVALID_DEVICE_REQUEST;
    case STOR_STATUS_INVALID_IRQL:
        return STATUS_INVALID_PARAMETER;
    case STOR_STATUS_INVALID_DEVICE_STATE:
        return STATUS_INVALID_DEVICE_STATE;
    case STOR_STATUS_INVALID_BUFFER_SIZE:
        return STATUS_INVALID_BUFFER_SIZE;
    case STOR_STATUS_UNSUPPORTED_VERSION:
        return STATUS_INVALID_PARAMETER;
    case STOR_STATUS_BUSY:
        return STATUS_INVALID_PARAMETER;
    default:
        return STATUS_INVALID_PARAMETER;
    }
}
