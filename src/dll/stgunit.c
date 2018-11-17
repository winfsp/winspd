/**
 * @file dll/stgunit.c
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

#include <winspd/winspd.h>
#include <shared/minimal.h>

static SPD_STORAGE_UNIT_INTERFACE SpdStorageUnitNullInterface;

static INIT_ONCE SpdStorageUnitInitOnce = INIT_ONCE_STATIC_INIT;
static DWORD SpdStorageUnitTlsKey = TLS_OUT_OF_INDEXES;

static BOOL WINAPI SpdStorageUnitInitialize(
    PINIT_ONCE InitOnce, PVOID Parameter, PVOID *Context)
{
    SpdStorageUnitTlsKey = TlsAlloc();
    return TRUE;
}

VOID SpdStorageUnitFinalize(BOOLEAN Dynamic)
{
    if (Dynamic && TLS_OUT_OF_INDEXES != SpdStorageUnitTlsKey)
        TlsFree(SpdStorageUnitTlsKey);
}

DWORD SpdStorageUnitCreate(
    const SPD_STORAGE_UNIT_PARAMS *StorageUnitParams,
    const SPD_STORAGE_UNIT_INTERFACE *Interface,
    SPD_STORAGE_UNIT **PStorageUnit)
{
    DWORD Error;
    SPD_STORAGE_UNIT *StorageUnit = 0;
    HANDLE DeviceHandle = INVALID_HANDLE_VALUE;
    UINT32 Btl;

    *PStorageUnit = 0;

    if (0 == Interface)
        Interface = &SpdStorageUnitNullInterface;

    InitOnceExecuteOnce(&SpdStorageUnitInitOnce, SpdStorageUnitInitialize, 0, 0);
    if (TLS_OUT_OF_INDEXES == SpdStorageUnitTlsKey)
    {
        Error = ERROR_NO_SYSTEM_RESOURCES;
        goto exit;
    }

    StorageUnit = MemAlloc(sizeof *StorageUnit);
    if (0 == StorageUnit)
    {
        Error = ERROR_NO_SYSTEM_RESOURCES;
        goto exit;
    }
    memset(StorageUnit, 0, sizeof *StorageUnit);

    Error = SpdIoctlOpenDevice(L"" SPD_IOCTL_HARDWARE_ID, &DeviceHandle);
    if (ERROR_SUCCESS != Error)
        goto exit;

    Error = SpdIoctlProvision(DeviceHandle, StorageUnitParams, &Btl);
    if (ERROR_SUCCESS != Error)
        goto exit;

    memcpy(&StorageUnit->Guid, &StorageUnitParams->Guid, sizeof StorageUnitParams->Guid);
    StorageUnit->DeviceHandle = DeviceHandle;
    StorageUnit->Btl = Btl;
    StorageUnit->Interface = Interface;

    *PStorageUnit = StorageUnit;

    Error = ERROR_SUCCESS;

exit:
    if (ERROR_SUCCESS != Error)
    {
        if (INVALID_HANDLE_VALUE != DeviceHandle)
            CloseHandle(DeviceHandle);

        MemFree(StorageUnit);
    }

    return Error;
}

VOID SpdStorageUnitDelete(SPD_STORAGE_UNIT *StorageUnit)
{
    SpdIoctlUnprovision(StorageUnit->DeviceHandle, &StorageUnit->Guid);
    CloseHandle(StorageUnit->DeviceHandle);
    MemFree(StorageUnit);
}

static DWORD WINAPI SpdStorageUnitDispatcherThread(PVOID StorageUnit0)
{
    SPD_STORAGE_UNIT *StorageUnit = StorageUnit0;
    SPD_IOCTL_TRANSACT_REQ RequestBuf, *Request = &RequestBuf;
    SPD_IOCTL_TRANSACT_RSP ResponseBuf, *Response;
    SPD_STORAGE_UNIT_OPERATION_CONTEXT OperationContext;
    HANDLE DispatcherThread = 0;
    BOOLEAN Complete;
    DWORD Error;

    if (1 < StorageUnit->DispatcherThreadCount)
    {
        StorageUnit->DispatcherThreadCount--;
        DispatcherThread = CreateThread(0, 0, SpdStorageUnitDispatcherThread, StorageUnit, 0, 0);
        if (0 == DispatcherThread)
        {
            Error = GetLastError();
            goto exit;
        }
    }

    OperationContext.Request = &RequestBuf;
    OperationContext.Response = &ResponseBuf;
    TlsSetValue(SpdStorageUnitTlsKey, &OperationContext);

    Response = 0;
    for (;;)
    {
        memset(Request, 0, sizeof *Request);
        Error = SpdIoctlTransact(StorageUnit->DeviceHandle, StorageUnit->Btl, Response, Request);
        if (ERROR_SUCCESS != Error)
            goto exit;

        if (0 == Request->Hint)
            continue;

        if (StorageUnit->DebugLog)
        {
            if (SpdIoctlTransactKindCount <= Request->Kind ||
                (StorageUnit->DebugLog & (1 << Request->Kind)))
                SpdDebugLogRequest(Request);
        }

        Response = &ResponseBuf;
        memset(Response, 0, sizeof *Response);
        Response->Hint = Request->Hint;
        Response->Kind = Request->Kind;
        switch (Request->Kind)
        {
        case SpdIoctlTransactReadKind:
            if (0 == StorageUnit->Interface->Read)
                goto invalid;
            Complete = StorageUnit->Interface->Read(
                StorageUnit,
                (PVOID)(UINT_PTR)Request->Op.Read.Address,
                Request->Op.Read.BlockAddress,
                Request->Op.Read.BlockCount,
                Request->Op.Read.ForceUnitAccess,
                &Response->Status);
            break;
        case SpdIoctlTransactWriteKind:
            if (0 == StorageUnit->Interface->Write)
                goto invalid;
            Complete = StorageUnit->Interface->Write(
                StorageUnit,
                (PVOID)(UINT_PTR)Request->Op.Write.Address,
                Request->Op.Write.BlockAddress,
                Request->Op.Write.BlockCount,
                Request->Op.Write.ForceUnitAccess,
                &Response->Status);
            break;
        case SpdIoctlTransactFlushKind:
            if (0 == StorageUnit->Interface->Flush)
                goto invalid;
            Complete = StorageUnit->Interface->Flush(
                StorageUnit,
                Request->Op.Flush.BlockAddress,
                Request->Op.Flush.BlockCount,
                &Response->Status);
            break;
        case SpdIoctlTransactUnmapKind:
            if (0 == StorageUnit->Interface->Unmap)
                goto invalid;
            Complete = StorageUnit->Interface->Unmap(
                StorageUnit,
                Request->Op.Unmap.BlockAddresses,
                Request->Op.Unmap.BlockCounts,
                Request->Op.Unmap.Count,
                &Response->Status);
            break;
        default:
        invalid:
            Response->Status.ScsiStatus = SCSISTAT_CHECK_CONDITION;
            Response->Status.SenseKey = SCSI_SENSE_ILLEGAL_REQUEST;
            Response->Status.ASC = SCSI_ADSENSE_ILLEGAL_COMMAND;
            Complete = TRUE;
            break;
        }

        if (Complete && StorageUnit->DebugLog)
        {
            if (SpdIoctlTransactKindCount <= Response->Kind ||
                (StorageUnit->DebugLog & (1 << Response->Kind)))
                SpdDebugLogResponse(Response);
        }

        if (!Complete)
            Response = 0;
    }

exit:
    TlsSetValue(SpdStorageUnitTlsKey, 0);

    SpdStorageUnitSetDispatcherError(StorageUnit, Error);

    SpdIoctlUnprovision(StorageUnit->DeviceHandle, &StorageUnit->Guid);

    if (0 != DispatcherThread)
    {
        WaitForSingleObject(DispatcherThread, INFINITE);
        CloseHandle(DispatcherThread);
    }

    return Error;
}

DWORD SpdStorageUnitStartDispatcher(SPD_STORAGE_UNIT *StorageUnit, ULONG ThreadCount)
{
    if (0 != StorageUnit->DispatcherThread)
        return ERROR_INVALID_PARAMETER;

    if (0 == ThreadCount)
    {
        DWORD_PTR ProcessMask, SystemMask;

        if (!GetProcessAffinityMask(GetCurrentProcess(), &ProcessMask, &SystemMask))
            return GetLastError();

        for (ThreadCount = 0; 0 != ProcessMask; ProcessMask >>= 1)
            ThreadCount += ProcessMask & 1;
    }

    StorageUnit->DispatcherThreadCount = ThreadCount;
    StorageUnit->DispatcherThread = CreateThread(0, 0,
        SpdStorageUnitDispatcherThread, StorageUnit, 0, 0);
    if (0 == StorageUnit->DispatcherThread)
        return GetLastError();

    return ERROR_SUCCESS;
}

VOID SpdStorageUnitStopDispatcher(SPD_STORAGE_UNIT *StorageUnit)
{
    if (0 == StorageUnit->DispatcherThread)
        return;

    SpdIoctlUnprovision(StorageUnit->DeviceHandle, &StorageUnit->Guid);

    WaitForSingleObject(StorageUnit->DispatcherThread, INFINITE);
    CloseHandle(StorageUnit->DispatcherThread);
    StorageUnit->DispatcherThread = 0;
}

VOID SpdStorageUnitSendResponse(SPD_STORAGE_UNIT *StorageUnit,
    SPD_IOCTL_TRANSACT_RSP *Response)
{
    DWORD Error;

    if (StorageUnit->DebugLog)
    {
        if (SpdIoctlTransactKindCount <= Response->Kind ||
            (StorageUnit->DebugLog & (1 << Response->Kind)))
            SpdDebugLogResponse(Response);
    }

    Error = SpdIoctlTransact(StorageUnit->DeviceHandle, StorageUnit->Btl, Response, 0);
    if (ERROR_SUCCESS != Error)
    {
        SpdStorageUnitSetDispatcherError(StorageUnit, Error);

        SpdIoctlUnprovision(StorageUnit->DeviceHandle, &StorageUnit->Guid);
    }
}

SPD_STORAGE_UNIT_OPERATION_CONTEXT *SpdStorageUnitGetOperationContext(VOID)
{
    return (SPD_STORAGE_UNIT_OPERATION_CONTEXT *)TlsGetValue(SpdStorageUnitTlsKey);
}
