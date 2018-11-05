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

static VOID SpdIoctlProvision(
    ULONG InputBufferLength, ULONG OutputBufferLength, ULONG ProcessId,
    SPD_IOCTL_PROVISION_PARAMS *Params,
    PIO_STATUS_BLOCK PIoStatus)
{
    ASSERT(PASSIVE_LEVEL == KeGetCurrentIrql());

    if (sizeof *Params > InputBufferLength)
    {
        PIoStatus->Status = STATUS_INVALID_PARAMETER;
        goto exit;
    }

    PIoStatus->Status = STATUS_INVALID_PARAMETER;

exit:;
}

static VOID SpdIoctlUnprovision(
    ULONG InputBufferLength, ULONG ProcessId,
    SPD_IOCTL_UNPROVISION_PARAMS *Params,
    PIO_STATUS_BLOCK PIoStatus)
{
    ASSERT(PASSIVE_LEVEL == KeGetCurrentIrql());

    if (sizeof *Params > InputBufferLength)
    {
        PIoStatus->Status = STATUS_INVALID_PARAMETER;
        goto exit;
    }

    PIoStatus->Status = STATUS_INVALID_PARAMETER;

exit:;
}

static VOID SpdIoctlList(
    ULONG InputBufferLength, ULONG OutputBufferLength,
    SPD_IOCTL_LIST_PARAMS *Params,
    PIO_STATUS_BLOCK PIoStatus)
{
    ASSERT(PASSIVE_LEVEL == KeGetCurrentIrql());

    if (sizeof *Params > InputBufferLength)
    {
        PIoStatus->Status = STATUS_INVALID_PARAMETER;
        goto exit;
    }

    PIoStatus->Status = STATUS_INVALID_PARAMETER;

exit:;
}

static VOID SpdIoctlTransact(
    ULONG InputBufferLength, ULONG OutputBufferLength, ULONG ProcessId,
    SPD_IOCTL_TRANSACT_PARAMS *Params,
    PIO_STATUS_BLOCK PIoStatus)
{
    ASSERT(PASSIVE_LEVEL == KeGetCurrentIrql());

    if (sizeof *Params > InputBufferLength)
    {
        PIoStatus->Status = STATUS_INVALID_PARAMETER;
        goto exit;
    }

    PIoStatus->Status = STATUS_INVALID_PARAMETER;

exit:;
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

    if (sizeof(SPD_IOCTL_BASE_PARAMS) > InputBufferLength ||
        ((SPD_IOCTL_BASE_PARAMS *)Params)->Size > InputBufferLength)
    {
        Irp->IoStatus.Status = STATUS_INVALID_PARAMETER;
        goto exit;
    }

    switch (((SPD_IOCTL_BASE_PARAMS *)Params)->Code)
    {
    case SPD_IOCTL_PROVISION:
        SpdIoctlProvision(InputBufferLength, OutputBufferLength, IoGetRequestorProcessId(Irp),
            Params, &Irp->IoStatus);
        break;
    case SPD_IOCTL_UNPROVISION:
        SpdIoctlUnprovision(InputBufferLength, IoGetRequestorProcessId(Irp),
            Params, &Irp->IoStatus);
        break;
    case SPD_IOCTL_LIST:
        SpdIoctlList(InputBufferLength, OutputBufferLength,
            Params, &Irp->IoStatus);
        break;
    case SPD_IOCTL_TRANSACT:
        SpdIoctlTransact(InputBufferLength, OutputBufferLength, IoGetRequestorProcessId(Irp),
            Params, &Irp->IoStatus);
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
