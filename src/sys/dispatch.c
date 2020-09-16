/**
 * @file sys/dispatch.c
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

NTSTATUS SpdDispatchPnp(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    /*
     * Unfortunately Storport does not cleanly send us all PnP requests.
     * So we have to hook the IRP handler and do our own handling.
     */

    NTSTATUS Result = STATUS_INVALID_DEVICE_REQUEST;
    PIO_STACK_LOCATION IrpSp = IoGetCurrentIrpStackLocation(Irp);
    UCHAR MinorFunction = IrpSp->MinorFunction;
    GUID ProvisionGuid;
    ULONG ProvisionPid = (ULONG)-1;

    SPD_ENTER(irp,
        ASSERT(PASSIVE_LEVEL == KeGetCurrentIrql()));

    switch (MinorFunction)
    {
    case IRP_MN_QUERY_CAPABILITIES:
        /* if this is an adapter device set our own capabilities */
        {
            SPD_DEVICE_EXTENSION *DeviceExtension = SpdDeviceExtensionAcquire();
            if (0 != DeviceExtension)
            {
                if (DeviceObject == DeviceExtension->DeviceObject)
                {
                    PDEVICE_CAPABILITIES DeviceCapabilities =
                        IrpSp->Parameters.DeviceCapabilities.Capabilities;
                    SpdPnpSetAdapterCapabilities(DeviceCapabilities);
                }
                SpdDeviceExtensionRelease(DeviceExtension);
            }
        }
        break;

    case IRP_MN_START_DEVICE:
        /* if this is a disk device associate the device object with our storage unit */
        SpdStorageUnitGlobalSetDevice(DeviceObject);
        break;

    case IRP_MN_REMOVE_DEVICE:
        /* if this is a disk device unprovision the associated storage unit after StorPortDispatchPnp */
        {
            SPD_STORAGE_UNIT *StorageUnit = SpdStorageUnitGlobalReferenceByDevice(DeviceObject);
            if (0 != StorageUnit)
            {
                RtlCopyMemory(&ProvisionGuid, &StorageUnit->StorageUnitParams.Guid, sizeof ProvisionGuid);
                ProvisionPid = StorageUnit->OwnerProcessId;
                SpdStorageUnitGlobalDereference(StorageUnit);
            }
        }
        break;
    }

    Result = StorPortDispatchPnp(DeviceObject, Irp);

    /* DO NOT ACCESS THE IRP! IT HAS BEEN COMPLETED BY STORPORT! */

    switch (MinorFunction)
    {
    case IRP_MN_REMOVE_DEVICE:
        if (-1 != ProvisionPid)
        {
            SPD_DEVICE_EXTENSION *DeviceExtension = SpdDeviceExtensionAcquire();
            if (0 != DeviceExtension)
            {
                SpdStorageUnitUnprovision(DeviceExtension, &ProvisionGuid, 0, ProvisionPid);
                SpdDeviceExtensionRelease(DeviceExtension);
            }
        }
        break;
    }

    SPD_LEAVE(irp,
        "DeviceObject=%p, MinorFunction=%u", " = %lx",
        DeviceObject, (unsigned)MinorFunction, Result);

    return Result;
}

PDRIVER_DISPATCH StorPortDispatchPnp;
