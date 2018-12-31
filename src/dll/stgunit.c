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

DWORD SpdStorageUnitHandleOpen(PWSTR Name,
    const SPD_IOCTL_STORAGE_UNIT_PARAMS *StorageUnitParams,
    PHANDLE PHandle, PUINT32 PBtl);
DWORD SpdStorageUnitHandleTransact(HANDLE Handle,
    UINT32 Btl,
    SPD_IOCTL_TRANSACT_RSP *Rsp,
    SPD_IOCTL_TRANSACT_REQ *Req,
    PVOID DataBuffer);
DWORD SpdStorageUnitHandleShutdown(HANDLE Handle,
    const GUID *Guid);
DWORD SpdStorageUnitHandleClose(HANDLE Handle);

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
    PWSTR DeviceName,
    const SPD_STORAGE_UNIT_PARAMS *StorageUnitParams,
    const SPD_STORAGE_UNIT_INTERFACE *Interface,
    SPD_STORAGE_UNIT **PStorageUnit)
{
    DWORD Error;
    SPD_STORAGE_UNIT *StorageUnit = 0;
    HANDLE Handle;
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

    if (0 == DeviceName)
        DeviceName = L"" SPD_IOCTL_HARDWARE_ID;

    Error = SpdStorageUnitHandleOpen(DeviceName, StorageUnitParams, &Handle, &Btl);
    if (ERROR_SUCCESS != Error)
        goto exit;

    memcpy(&StorageUnit->Guid, &StorageUnitParams->Guid, sizeof StorageUnitParams->Guid);
    StorageUnit->MaxTransferLength = StorageUnitParams->MaxTransferLength;
    StorageUnit->Handle = Handle;
    StorageUnit->Btl = Btl;
    StorageUnit->Interface = Interface;

    *PStorageUnit = StorageUnit;

    Error = ERROR_SUCCESS;

exit:
    if (ERROR_SUCCESS != Error)
        MemFree(StorageUnit);

    return Error;
}

VOID SpdStorageUnitDelete(SPD_STORAGE_UNIT *StorageUnit)
{
    SpdStorageUnitHandleShutdown(StorageUnit->Handle, &StorageUnit->Guid);
    SpdStorageUnitHandleClose(StorageUnit->Handle);
    MemFree(StorageUnit);
}

static DWORD WINAPI SpdStorageUnitDispatcherThread(PVOID StorageUnit0)
{
    SPD_STORAGE_UNIT *StorageUnit = StorageUnit0;
    SPD_IOCTL_TRANSACT_REQ RequestBuf, *Request = &RequestBuf;
    SPD_IOCTL_TRANSACT_RSP ResponseBuf, *Response;
    SPD_STORAGE_UNIT_OPERATION_CONTEXT OperationContext;
    PVOID DataBuffer = 0;
    HANDLE DispatcherThread = 0;
    BOOLEAN Complete;
    DWORD Error;

    DataBuffer = MemAlloc(StorageUnit->MaxTransferLength);
    if (0 == DataBuffer)
    {
        Error = ERROR_NO_SYSTEM_RESOURCES;
        goto exit;
    }

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
    OperationContext.DataBuffer = DataBuffer;
    TlsSetValue(SpdStorageUnitTlsKey, &OperationContext);

    Response = 0;
    for (;;)
    {
        memset(Request, 0, sizeof *Request);
        Error = SpdStorageUnitHandleTransact(StorageUnit->Handle,
            StorageUnit->Btl, Response, Request, DataBuffer);
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
                DataBuffer,
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
                DataBuffer,
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
                DataBuffer,
                Request->Op.Unmap.Count,
                &Response->Status);
            break;
        default:
        invalid:
            SpdStorageUnitStatusSetSense(&Response->Status,
                SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ADSENSE_ILLEGAL_COMMAND, 0);
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

    MemFree(DataBuffer);

    SpdStorageUnitSetDispatcherError(StorageUnit, Error);

    SpdStorageUnitHandleShutdown(StorageUnit->Handle, &StorageUnit->Guid);

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

    SpdStorageUnitHandleShutdown(StorageUnit->Handle, &StorageUnit->Guid);

    WaitForSingleObject(StorageUnit->DispatcherThread, INFINITE);
    CloseHandle(StorageUnit->DispatcherThread);
    StorageUnit->DispatcherThread = 0;
}

VOID SpdStorageUnitSendResponse(SPD_STORAGE_UNIT *StorageUnit,
    SPD_IOCTL_TRANSACT_RSP *Response, PVOID DataBuffer)
{
    DWORD Error;

    if (StorageUnit->DebugLog)
    {
        if (SpdIoctlTransactKindCount <= Response->Kind ||
            (StorageUnit->DebugLog & (1 << Response->Kind)))
            SpdDebugLogResponse(Response);
    }

    Error = SpdStorageUnitHandleTransact(StorageUnit->Handle,
        StorageUnit->Btl, Response, 0, DataBuffer);
    if (ERROR_SUCCESS != Error)
    {
        SpdStorageUnitSetDispatcherError(StorageUnit, Error);

        SpdStorageUnitHandleShutdown(StorageUnit->Handle, &StorageUnit->Guid);
    }
}

SPD_STORAGE_UNIT_OPERATION_CONTEXT *SpdStorageUnitGetOperationContext(VOID)
{
    return (SPD_STORAGE_UNIT_OPERATION_CONTEXT *)TlsGetValue(SpdStorageUnitTlsKey);
}
