/**
 * @file sys/driver.c
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

DRIVER_INITIALIZE DriverEntry;

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT, DriverEntry)
#endif

NTSTATUS DriverEntry(
    PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath)
{
    NTSTATUS Result = STATUS_UNSUCCESSFUL;
    SPD_ENTER(drvrld,
        ASSERT(PASSIVE_LEVEL == KeGetCurrentIrql()));

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
    Data.DeviceExtensionSize = 0;
    Data.SpecificLuExtensionSize = 0;
    Data.SrbExtensionSize = 0;
    Data.MapBuffers = STOR_MAP_NO_BUFFERS;
    Data.TaggedQueuing = FALSE;         /* docs say MUST be TRUE */
    Data.AutoRequestSense = FALSE;      /* docs say MUST be TRUE */
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

#pragma prefast(suppress:28175, "We are in DriverEntry: ok to access DriverName")
    SPD_LEAVE(drvrld,
        "DriverName=\"%wZ\", RegistryPath=\"%wZ\"", " = %lx",
        &DriverObject->DriverName, RegistryPath, Result);
    return Result;
}
