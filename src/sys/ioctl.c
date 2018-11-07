/**
 * @file sys/ioctl.c
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

static VOID SpdIoctlProvision(SPD_DEVICE_EXTENSION *DeviceExtension,
    ULONG Length, SPD_IOCTL_PROVISION_PARAMS *Params,
    PIRP Irp)
{
    ASSERT(PASSIVE_LEVEL == KeGetCurrentIrql());

    static GUID NullGuid = { 0 };
    SPD_LOGICAL_UNIT *LogicalUnit;
    KIRQL Irql;
    UINT32 Btl;

    if (sizeof *Params > Length)
    {
        Irp->IoStatus.Status = STATUS_INVALID_PARAMETER;
        goto exit;
    }

    if (RtlEqualMemory(&NullGuid, &Params->Dir.Par.StorageUnitParams.Guid, sizeof NullGuid) ||
        0 == Params->Dir.Par.StorageUnitParams.BlockCount ||
        0 == Params->Dir.Par.StorageUnitParams.BlockLength ||
        DIRECT_ACCESS_DEVICE != Params->Dir.Par.StorageUnitParams.DeviceType ||
        0 != Params->Dir.Par.StorageUnitParams.RemovableMedia)
    {
        Irp->IoStatus.Status = STATUS_INVALID_PARAMETER;
        goto exit;
    }

    KeAcquireSpinLock(&DeviceExtension->SpinLock, &Irql);

    LogicalUnit = 0;
    for (
        PLIST_ENTRY Head = &DeviceExtension->LogicalUnitList, Entry = Head->Flink;
        Head != Entry;
        Entry = Entry->Flink)
    {
        SPD_LOGICAL_UNIT *Unit = CONTAINING_RECORD(Entry, SPD_LOGICAL_UNIT, ListEntry);
        if (RtlEqualMemory(&Params->Dir.Par.StorageUnitParams.Guid, &Unit->StorageUnitParams.Guid,
            sizeof Unit->StorageUnitParams.Guid))
        {
            LogicalUnit = Unit;
            break;
        }
    }

    Btl = (ULONG)-1;
    for (ULONG I = 0; ARRAYSIZE(DeviceExtension->LogicalUnitBitmap) > I; I++)
    {
        ULONG J;
        if (BitScanForward(&J, DeviceExtension->LogicalUnitBitmap[I]))
        {
            Btl = SPD_IOCTL_BTL(0, I * 8 + J, 0);
            break;
        }
    }

    KeReleaseSpinLock(&DeviceExtension->SpinLock, Irql);

    if (0 != LogicalUnit)
    {
        Irp->IoStatus.Status = STATUS_OBJECT_NAME_COLLISION;
        goto exit;
    }

    if (-1 == Btl)
    {
        Irp->IoStatus.Status = STATUS_CANNOT_MAKE;
        goto exit;
    }

    LogicalUnit = StorPortGetLogicalUnit(DeviceExtension,
        SPD_IOCTL_BTL_B(Btl), SPD_IOCTL_BTL_T(Btl), SPD_IOCTL_BTL_L(Btl));
    if (0 == LogicalUnit)
    {
        Irp->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;
        goto exit;
    }

    ASSERT(
        (0 == LogicalUnit->ListEntry.Flink && 0 == LogicalUnit->ListEntry.Blink) ||
        IsListEmpty(&LogicalUnit->ListEntry));
    RtlZeroMemory(LogicalUnit, sizeof *LogicalUnit);
    RtlCopyMemory(&LogicalUnit->StorageUnitParams, &Params->Dir.Par.StorageUnitParams,
        sizeof Params->Dir.Par.StorageUnitParams);

    KeAcquireSpinLock(&DeviceExtension->SpinLock, &Irql);
    InsertTailList(&DeviceExtension->LogicalUnitList, &LogicalUnit->ListEntry);
    KeReleaseSpinLock(&DeviceExtension->SpinLock, Irql);

    RtlZeroMemory(Params, sizeof *Params);
    Params->Base.Size = sizeof *Params;
    Params->Base.Code = SPD_IOCTL_TRANSACT;
    Params->Dir.Ret.Btl = Btl;

    Irp->IoStatus.Status = STATUS_SUCCESS;
    Irp->IoStatus.Information = sizeof *Params;

    StorPortNotification(BusChangeDetected, DeviceExtension, (UCHAR)0);

exit:;
}

static VOID SpdIoctlUnprovision(SPD_DEVICE_EXTENSION *DeviceExtension,
    ULONG Length, SPD_IOCTL_UNPROVISION_PARAMS *Params,
    PIRP Irp)
{
    ASSERT(PASSIVE_LEVEL == KeGetCurrentIrql());

    if (sizeof *Params > Length)
    {
        Irp->IoStatus.Status = STATUS_INVALID_PARAMETER;
        goto exit;
    }

    Irp->IoStatus.Status = STATUS_INVALID_PARAMETER;

exit:;
}

static VOID SpdIoctlList(SPD_DEVICE_EXTENSION *DeviceExtension,
    ULONG Length, SPD_IOCTL_LIST_PARAMS *Params,
    PIRP Irp)
{
    ASSERT(PASSIVE_LEVEL == KeGetCurrentIrql());

    if (sizeof *Params > Length)
    {
        Irp->IoStatus.Status = STATUS_INVALID_PARAMETER;
        goto exit;
    }

    Irp->IoStatus.Status = STATUS_INVALID_PARAMETER;

exit:;
}

static VOID SpdIoctlTransact(SPD_DEVICE_EXTENSION *DeviceExtension,
    ULONG Length, SPD_IOCTL_TRANSACT_PARAMS *Params,
    PIRP Irp)
{
    ASSERT(PASSIVE_LEVEL == KeGetCurrentIrql());

    if (sizeof *Params > Length)
    {
        Irp->IoStatus.Status = STATUS_INVALID_PARAMETER;
        goto exit;
    }

    Irp->IoStatus.Status = STATUS_INVALID_PARAMETER;

exit:;
}

VOID SpdHwProcessServiceRequest(PVOID DeviceExtension, PVOID Irp0)
{
    SPD_ENTER(ioctl,
        ASSERT(PASSIVE_LEVEL == KeGetCurrentIrql()));

    PIRP Irp = Irp0;
    PIO_STACK_LOCATION IrpSp = IoGetCurrentIrpStackLocation(Irp);
    ULONG Length = IrpSp->Parameters.DeviceIoControl.InputBufferLength;
    PVOID Params = Irp->AssociatedIrp.SystemBuffer;

    if (0 == Params ||
        sizeof(SPD_IOCTL_BASE_PARAMS) > Length ||
        ((SPD_IOCTL_BASE_PARAMS *)Params)->Size > Length)
    {
        Irp->IoStatus.Status = STATUS_INVALID_PARAMETER;
        goto exit;
    }

    switch (((SPD_IOCTL_BASE_PARAMS *)Params)->Code)
    {
    case SPD_IOCTL_PROVISION:
        SpdIoctlProvision(DeviceExtension, Length, Params, Irp);
        break;
    case SPD_IOCTL_UNPROVISION:
        SpdIoctlUnprovision(DeviceExtension, Length, Params, Irp);
        break;
    case SPD_IOCTL_LIST:
        SpdIoctlList(DeviceExtension, Length, Params, Irp);
        break;
    case SPD_IOCTL_TRANSACT:
        SpdIoctlTransact(DeviceExtension, Length, Params, Irp);
        break;
    default:
        Irp->IoStatus.Status = STATUS_INVALID_PARAMETER;
        goto exit;
    }

exit:
    if (STATUS_SUCCESS != Irp->IoStatus.Status &&
        STATUS_BUFFER_OVERFLOW != Irp->IoStatus.Status)
        Irp->IoStatus.Information = 0;
    StorPortCompleteServiceIrp(DeviceExtension, Irp);

    SPD_LEAVE(ioctl,
        "%p, Irp=%p", "",
        DeviceExtension, Irp0);
}

VOID SpdHwCompleteServiceIrp(PVOID DeviceExtension)
{
    SPD_ENTER(ioctl,
        ASSERT(PASSIVE_LEVEL == KeGetCurrentIrql()));

    SPD_LEAVE(ioctl,
        "%p", "",
        DeviceExtension);
}
