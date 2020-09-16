/**
 * @file sys/driver.c
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

DRIVER_INITIALIZE DriverEntry;

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT, DriverEntry)
#endif

static DRIVER_UNLOAD DriverUnload;
static PDRIVER_UNLOAD StorPortDriverUnload;

NTSTATUS DriverEntry(
    PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath)
{
    NTSTATUS Result = STATUS_UNSUCCESSFUL;
    SPD_ENTER(drvrld,
        ASSERT(PASSIVE_LEVEL == KeGetCurrentIrql()));

    ExInitializeResourceLite(&SpdGlobalDeviceResource);

    UNICODE_STRING RegistryValueName;
    union
    {
        KEY_VALUE_PARTIAL_INFORMATION Information;
        UINT8 Buffer[FIELD_OFFSET(KEY_VALUE_PARTIAL_INFORMATION, Data) + sizeof(DWORD)];
    } RegistryValue;
    ULONG RegistryValueLength;
    RtlInitUnicodeString(&RegistryValueName, L"StorageUnitCapacity");
    RegistryValueLength = sizeof RegistryValue;
    Result = SpdRegistryGetValue(RegistryPath, &RegistryValueName,
        &RegistryValue.Information, &RegistryValueLength);
    if (NT_SUCCESS(Result) && REG_DWORD == RegistryValue.Information.Type)
    {
        ULONG Value = *(PULONG)&RegistryValue.Information.Data;
        if (SPD_IOCTL_STORAGE_UNIT_CAPACITY <= Value && Value <= SPD_IOCTL_STORAGE_UNIT_MAX_CAPACITY)
            SpdStorageUnitCapacity = Value;
    }

    VIRTUAL_HW_INITIALIZATION_DATA Data;
    RtlZeroMemory(&Data, sizeof(Data));
    Data.HwInitializationDataSize = sizeof(VIRTUAL_HW_INITIALIZATION_DATA);
    Data.AdapterInterfaceType = Internal;
    Data.HwInitialize = SpdHwInitialize;
    Data.HwStartIo = SpdHwStartIo;
    Data.HwInterrupt = 0;
    Data.HwFindAdapter = SpdHwFindAdapter;
    Data.HwResetBus = SpdHwResetBus;
    Data.HwDmaStarted = 0;
    Data.HwAdapterState = 0;
    Data.DeviceExtensionSize = sizeof(SPD_DEVICE_EXTENSION) +
        sizeof(SPD_STORAGE_UNIT *) * SpdStorageUnitCapacity;
    Data.SpecificLuExtensionSize = 0;
    Data.SrbExtensionSize = sizeof(SPD_SRB_EXTENSION);
    Data.MapBuffers = STOR_MAP_NON_READ_WRITE_BUFFERS;
    Data.TaggedQueuing = TRUE;
    Data.AutoRequestSense = TRUE;
    Data.MultipleRequestPerLu = TRUE;
    Data.HwAdapterControl = SpdHwAdapterControl;
    Data.HwBuildIo = 0;
    Data.HwFreeAdapterResources = SpdHwFreeAdapterResources;
    Data.HwProcessServiceRequest = SpdHwProcessServiceRequest;
    Data.HwCompleteServiceIrp = SpdHwCompleteServiceIrp;
    Data.HwInitializeTracing = SpdHwInitializeTracing;
    Data.HwCleanupTracing = SpdHwCleanupTracing;

    Result = StorPortInitialize(
        DriverObject, RegistryPath, (PHW_INITIALIZATION_DATA)&Data, 0);
    if (!NT_SUCCESS(Result))
        SPD_RETURN(ExDeleteResourceLite(&SpdGlobalDeviceResource));

    StorPortDriverUnload = DriverObject->DriverUnload;
    StorPortDispatchPnp = DriverObject->MajorFunction[IRP_MJ_PNP];
    DriverObject->DriverUnload = DriverUnload;
    DriverObject->MajorFunction[IRP_MJ_PNP] = 0 != StorPortDispatchPnp ? SpdDispatchPnp : 0;

#pragma prefast(suppress:28175, "We are in DriverEntry: ok to access DriverName")
    SPD_LEAVE(drvrld,
        "DriverName=\"%wZ\", RegistryPath=\"%wZ\"", " = %lx",
        &DriverObject->DriverName, RegistryPath, Result);
    return Result;
}

VOID DriverUnload(
    PDRIVER_OBJECT DriverObject)
{
    SPD_ENTER(drvrld,
        ASSERT(PASSIVE_LEVEL == KeGetCurrentIrql()));

    if (0 != StorPortDriverUnload)
        StorPortDriverUnload(DriverObject);

    ExDeleteResourceLite(&SpdGlobalDeviceResource);

#pragma prefast(suppress:28175, "We are in DriverUnload: ok to access DriverName")
    SPD_LEAVE(drvrld,
        "DriverName=\"%wZ\"", "",
        &DriverObject->DriverName);
}
