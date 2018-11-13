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
    SPD_STORAGE_UNIT *StorageUnit = 0;
    SPD_STORAGE_UNIT *DuplicateUnit;
    UINT32 Btl;
    KIRQL Irql;

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

    StorageUnit = SpdAllocNonPaged(sizeof *StorageUnit, SpdTagStorageUnit);
    if (0 == StorageUnit)
    {
        Irp->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;
        goto exit;
    }

    RtlZeroMemory(StorageUnit, sizeof *StorageUnit);
    RtlCopyMemory(&StorageUnit->StorageUnitParams, &Params->Dir.Par.StorageUnitParams,
        sizeof Params->Dir.Par.StorageUnitParams);
    StorageUnit->ProcessId = IoGetRequestorProcessId(Irp);
#define Guid                            Params->Dir.Par.StorageUnitParams.Guid
    RtlStringCbPrintfA(StorageUnit->SerialNumber, sizeof StorageUnit->SerialNumber,
        "%08lx-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x",
        Guid.Data1, Guid.Data2, Guid.Data3,
        Guid.Data4[0], Guid.Data4[1], Guid.Data4[2], Guid.Data4[3],
        Guid.Data4[4], Guid.Data4[5], Guid.Data4[6], Guid.Data4[7]);
#undef Guid

    KeAcquireSpinLock(&DeviceExtension->SpinLock, &Irql);
    DuplicateUnit = 0;
    Btl = (ULONG)-1;
    for (ULONG I = 0; DeviceExtension->StorageUnitMaxCount > I; I++)
    {
        SPD_STORAGE_UNIT *Unit = DeviceExtension->StorageUnits[I];
        if (0 == Unit)
            Btl = SPD_BTL_FROM_INDEX(I);
        else
        if (RtlEqualMemory(&Params->Dir.Par.StorageUnitParams.Guid, &Unit->StorageUnitParams.Guid,
            sizeof Unit->StorageUnitParams.Guid))
        {
            DuplicateUnit = Unit;
            break;
        }
    }
    if (0 == DuplicateUnit && -1 != Btl)
        DeviceExtension->StorageUnits[SPD_INDEX_FROM_BTL(Btl)] = StorageUnit;
    KeReleaseSpinLock(&DeviceExtension->SpinLock, Irql);

    if (0 != DuplicateUnit)
    {
        Irp->IoStatus.Status = STATUS_OBJECT_NAME_COLLISION;
        goto exit;
    }
    if (-1 == Btl)
    {
        Irp->IoStatus.Status = STATUS_CANNOT_MAKE;
        goto exit;
    }

    RtlZeroMemory(Params, sizeof *Params);
    Params->Base.Size = sizeof *Params;
    Params->Base.Code = SPD_IOCTL_TRANSACT;
    Params->Dir.Ret.Btl = Btl;

    Irp->IoStatus.Status = STATUS_SUCCESS;
    Irp->IoStatus.Information = sizeof *Params;

    StorPortNotification(BusChangeDetected, DeviceExtension, (UCHAR)0);

exit:
    if (!NT_SUCCESS(Irp->IoStatus.Status) && 0 != StorageUnit)
        SpdFree(StorageUnit, SpdTagStorageUnit);
}

static VOID SpdIoctlUnprovision(SPD_DEVICE_EXTENSION *DeviceExtension,
    ULONG Length, SPD_IOCTL_UNPROVISION_PARAMS *Params,
    PIRP Irp)
{
    ASSERT(PASSIVE_LEVEL == KeGetCurrentIrql());

    SPD_STORAGE_UNIT *StorageUnit;
    ULONG ProcessId;
    KIRQL Irql;

    if (sizeof *Params > Length)
    {
        Irp->IoStatus.Status = STATUS_INVALID_PARAMETER;
        goto exit;
    }

    ProcessId = IoGetRequestorProcessId(Irp);

    KeAcquireSpinLock(&DeviceExtension->SpinLock, &Irql);
    StorageUnit = 0;
    for (ULONG I = 0; DeviceExtension->StorageUnitMaxCount > I; I++)
    {
        SPD_STORAGE_UNIT *Unit = DeviceExtension->StorageUnits[I];
        if (0 == Unit)
            ;
        else
        if (RtlEqualMemory(&Params->Dir.Par.Guid, &Unit->StorageUnitParams.Guid,
            sizeof Unit->StorageUnitParams.Guid))
        {
            StorageUnit = Unit;
            break;
        }
    }
    KeReleaseSpinLock(&DeviceExtension->SpinLock, Irql);

    if (0 == StorageUnit)
    {
        Irp->IoStatus.Status = STATUS_OBJECT_NAME_NOT_FOUND;
        goto exit;
    }
    if (ProcessId != StorageUnit->ProcessId)
    {
        Irp->IoStatus.Status = STATUS_ACCESS_DENIED;
        goto exit;
    }

    SpdFree(StorageUnit, SpdTagStorageUnit);

    Irp->IoStatus.Status = STATUS_SUCCESS;
    Irp->IoStatus.Information = sizeof *Params;

    StorPortNotification(BusChangeDetected, DeviceExtension, (UCHAR)0);

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
