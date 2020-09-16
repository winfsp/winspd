/**
 * @file sys/ioctl.c
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

static VOID SpdIoctlProvision(SPD_DEVICE_EXTENSION *DeviceExtension,
    ULONG InputBufferLength, ULONG OutputBufferLength, SPD_IOCTL_PROVISION_PARAMS *Params,
    PIRP Irp)
{
    static GUID NullGuid = { 0 };
    UINT32 Btl;

    if (sizeof *Params > InputBufferLength || sizeof *Params > OutputBufferLength)
    {
        Irp->IoStatus.Status = STATUS_INVALID_PARAMETER;
        goto exit;
    }

    if (RtlEqualMemory(&NullGuid, &Params->Dir.Par.StorageUnitParams.Guid, sizeof NullGuid) ||
        0 == Params->Dir.Par.StorageUnitParams.BlockCount ||
        sizeof(SPD_IOCTL_UNMAP_DESCRIPTOR) > Params->Dir.Par.StorageUnitParams.BlockLength ||
        DIRECT_ACCESS_DEVICE != Params->Dir.Par.StorageUnitParams.DeviceType ||
        0 == Params->Dir.Par.StorageUnitParams.MaxTransferLength ||
        0 != Params->Dir.Par.StorageUnitParams.MaxTransferLength %
            Params->Dir.Par.StorageUnitParams.BlockLength)
    {
        Irp->IoStatus.Status = STATUS_INVALID_PARAMETER;
        goto exit;
    }

    Irp->IoStatus.Status = SpdStorageUnitProvision(
        DeviceExtension,
        &Params->Dir.Par.StorageUnitParams,
        IoGetRequestorProcessId(Irp),
        &Btl);
    if (!NT_SUCCESS(Irp->IoStatus.Status))
        goto exit;

    RtlZeroMemory(Params, sizeof *Params);
    Params->Base.Size = sizeof *Params;
    Params->Base.Code = SPD_IOCTL_PROVISION;
    Params->Dir.Ret.Btl = Btl;

    Irp->IoStatus.Status = STATUS_SUCCESS;
    Irp->IoStatus.Information = sizeof *Params;

exit:;
}

static VOID SpdIoctlUnprovision(SPD_DEVICE_EXTENSION *DeviceExtension,
    ULONG InputBufferLength, ULONG OutputBufferLength, SPD_IOCTL_UNPROVISION_PARAMS *Params,
    PIRP Irp)
{
    if (sizeof *Params > InputBufferLength)
    {
        Irp->IoStatus.Status = STATUS_INVALID_PARAMETER;
        goto exit;
    }

    Irp->IoStatus.Status = SpdStorageUnitUnprovision(
        DeviceExtension,
        &Params->Dir.Par.Guid, 0,
        IoGetRequestorProcessId(Irp));
    if (!NT_SUCCESS(Irp->IoStatus.Status))
        goto exit;

    Irp->IoStatus.Status = STATUS_SUCCESS;
    Irp->IoStatus.Information = 0;

exit:;
}

static VOID SpdIoctlGetList(SPD_DEVICE_EXTENSION *DeviceExtension,
    ULONG InputBufferLength, ULONG OutputBufferLength, SPD_IOCTL_LIST_PARAMS *Params,
    PIRP Irp)
{
    PUINT32 BtlBgnP = Irp->AssociatedIrp.SystemBuffer;
    PUINT32 BtlEndP = (PVOID)((PUINT8)BtlBgnP + OutputBufferLength);
    PUINT32 BtlP = BtlBgnP;
    UINT8 Bitmap[32];
    ULONG Count;

    if (sizeof *Params > InputBufferLength)
    {
        Irp->IoStatus.Status = STATUS_INVALID_PARAMETER;
        goto exit;
    }

    Count = SpdStorageUnitGetUseBitmap(DeviceExtension, 0, Bitmap);

    for (ULONG I = 0; 0 < Count && sizeof Bitmap * 8 > I; I++)
        if (FlagOn(Bitmap[I >> 3], 1 << (I & 7)))
        {
            if (BtlP + 1 > BtlEndP)
            {
                Irp->IoStatus.Status = STATUS_BUFFER_TOO_SMALL;
                goto exit;
            }

            *BtlP++ = SPD_BTL_FROM_INDEX(I);
            Count--;
        }

    Irp->IoStatus.Status = STATUS_SUCCESS;
    Irp->IoStatus.Information = (PUINT8)BtlP - (PUINT8)BtlBgnP;

exit:;
}

static VOID SpdIoctlTransact(SPD_DEVICE_EXTENSION *DeviceExtension,
    ULONG InputBufferLength, ULONG OutputBufferLength, SPD_IOCTL_TRANSACT_PARAMS *Params,
    PIRP Irp)
{
    SPD_STORAGE_UNIT *StorageUnit = 0;
    PMDL Mdl = 0;
    PVOID DataBuffer;

    if (sizeof *Params > InputBufferLength || sizeof *Params > OutputBufferLength)
    {
        Irp->IoStatus.Status = STATUS_INVALID_PARAMETER;
        goto exit;
    }

    DataBuffer = (PVOID)(UINT_PTR)Params->DataBuffer;

    if ((!Params->ReqValid && !Params->RspValid) ||
        (Params->ReqValid && 0 == DataBuffer))
    {
        Irp->IoStatus.Status = STATUS_INVALID_PARAMETER;
        goto exit;
    }

    StorageUnit = SpdStorageUnitReferenceByBtl(DeviceExtension, Params->Btl);
    if (0 == StorageUnit)
    {
        Irp->IoStatus.Status = STATUS_CANCELLED;
        goto exit;
    }

    if (IoGetRequestorProcessId(Irp) != StorageUnit->TransactProcessId)
    {
        Irp->IoStatus.Status = STATUS_ACCESS_DENIED;
        goto exit;
    }

    if (0 != DataBuffer && UserMode == Irp->RequestorMode)
    {
        try
        {
            ProbeForWrite(DataBuffer, StorageUnit->StorageUnitParams.MaxTransferLength, 1);

            Mdl = IoAllocateMdl(
                DataBuffer,
                StorageUnit->StorageUnitParams.MaxTransferLength,
                0 != Irp->MdlAddress,
                FALSE,
                Irp);
            if (0 == Mdl)
            {
                Irp->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;
                leave;
            }

            MmProbeAndLockPages(Mdl, UserMode, IoWriteAccess);

            DataBuffer = MmGetSystemAddressForMdlSafe(Mdl, NormalPagePriority);
            if (0 == DataBuffer)
            {
                Irp->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;
                leave;
            }
        }
        except (EXCEPTION_EXECUTE_HANDLER)
        {
            Irp->IoStatus.Status = GetExceptionCode();
            goto exit;
        }

        if (!NT_SUCCESS(Irp->IoStatus.Status))
            goto exit;
    }

    if (Params->RspValid)
        SpdIoqEndProcessingSrb(StorageUnit->Ioq,
            Params->Dir.Rsp.Hint, SpdSrbExecuteScsiComplete, &Params->Dir.Rsp, DataBuffer);

    if (Params->ReqValid)
    {
        Params->ReqValid = 0;
        Params->RspValid = 0;
        RtlZeroMemory(&Params->Dir.Req, sizeof Params->Dir.Req);

        /* wait for an SRB to arrive */
        while (STATUS_UNSUCCESSFUL == (Irp->IoStatus.Status =
            SpdIoqStartProcessingSrb(StorageUnit->Ioq,
                0, Irp, SpdSrbExecuteScsiPrepare, &Params->Dir.Req, DataBuffer)))
        {
            if (SpdIoqStopped(StorageUnit->Ioq))
            {
                Irp->IoStatus.Status = STATUS_CANCELLED;
                goto exit;
            }
        }

        if (!NT_SUCCESS(Irp->IoStatus.Status))
            goto exit;
        else if (STATUS_TIMEOUT == Irp->IoStatus.Status)
        {
            Irp->IoStatus.Status = STATUS_SUCCESS;
            Irp->IoStatus.Information = 0;
            goto exit;
        }

        Params->ReqValid = 1;
    }

    Irp->IoStatus.Status = STATUS_SUCCESS;
    Irp->IoStatus.Information = Params->ReqValid ? sizeof *Params : 0;

exit:;
    if (0 != StorageUnit)
        SpdStorageUnitDereference(DeviceExtension, StorageUnit);
}

static VOID SpdIoctlSetTransactProcessId(SPD_DEVICE_EXTENSION *DeviceExtension,
    ULONG InputBufferLength, ULONG OutputBufferLength, SPD_IOCTL_SET_TRANSACT_PID_PARAMS *Params,
    PIRP Irp)
{
    SPD_STORAGE_UNIT *StorageUnit = 0;

    if (sizeof *Params > InputBufferLength)
    {
        Irp->IoStatus.Status = STATUS_INVALID_PARAMETER;
        goto exit;
    }

    StorageUnit = SpdStorageUnitReferenceByBtl(DeviceExtension, Params->Btl);
    if (0 == StorageUnit)
    {
        Irp->IoStatus.Status = STATUS_CANCELLED;
        goto exit;
    }

    /* 32-bit store is atomic */
    StorageUnit->TransactProcessId = Params->ProcessId;

    Irp->IoStatus.Status = STATUS_SUCCESS;
    Irp->IoStatus.Information = 0;

exit:;
    if (0 != StorageUnit)
        SpdStorageUnitDereference(DeviceExtension, StorageUnit);
}

VOID SpdHwProcessServiceRequest(PVOID DeviceExtension, PVOID Irp0)
{
    SPD_ENTER(ioctl,
        ASSERT(PASSIVE_LEVEL == KeGetCurrentIrql()));

    PIRP Irp = Irp0;
    PIO_STACK_LOCATION IrpSp = IoGetCurrentIrpStackLocation(Irp);
    ULONG InputBufferLength = IrpSp->Parameters.DeviceIoControl.InputBufferLength;
    ULONG OutputBufferLength = IrpSp->Parameters.DeviceIoControl.OutputBufferLength;
    PVOID Params = Irp->AssociatedIrp.SystemBuffer;

    if (0 == Params ||
        sizeof(SPD_IOCTL_BASE_PARAMS) > InputBufferLength ||
        ((SPD_IOCTL_BASE_PARAMS *)Params)->Size > InputBufferLength)
    {
        Irp->IoStatus.Status = STATUS_INVALID_PARAMETER;
        goto exit;
    }

    switch (((SPD_IOCTL_BASE_PARAMS *)Params)->Code)
    {
    case SPD_IOCTL_PROVISION:
        SpdIoctlProvision(DeviceExtension, InputBufferLength, OutputBufferLength, Params, Irp);
        break;
    case SPD_IOCTL_UNPROVISION:
        SpdIoctlUnprovision(DeviceExtension, InputBufferLength, OutputBufferLength, Params, Irp);
        break;
    case SPD_IOCTL_LIST:
        SpdIoctlGetList(DeviceExtension, InputBufferLength, OutputBufferLength, Params, Irp);
        break;
    case SPD_IOCTL_TRANSACT:
        SpdIoctlTransact(DeviceExtension, InputBufferLength, OutputBufferLength, Params, Irp);
        break;
    case SPD_IOCTL_SET_TRANSACT_PID:
        SpdIoctlSetTransactProcessId(DeviceExtension, InputBufferLength, OutputBufferLength, Params, Irp);
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

VOID SpdHwCompleteServiceIrp(PVOID DeviceExtension0)
{
    SPD_ENTER(ioctl,
        ASSERT(PASSIVE_LEVEL == KeGetCurrentIrql()));

    SPD_DEVICE_EXTENSION *DeviceExtension = DeviceExtension0;
    SPD_STORAGE_UNIT *StorageUnit;

    for (ULONG I = 0; DeviceExtension->StorageUnitCapacity > I; I++)
    {
        StorageUnit = SpdStorageUnitReferenceByBtl(DeviceExtension, SPD_IOCTL_BTL(0, I, 0));
        if (0 == StorageUnit)
            continue;

        /* stop the unit's Ioq; this will cause all pending service IRP's to be cancelled */
        SpdIoqReset(StorageUnit->Ioq, TRUE);

        SpdStorageUnitDereference(DeviceExtension, StorageUnit);
    }

    SPD_LEAVE(ioctl,
        "%p", "",
        DeviceExtension0);
}
