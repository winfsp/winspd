/**
 * @file dll/stghandle.c
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

#include <shared/shared.h>

#define SPD_INDEX_FROM_BTL(Btl)         SPD_IOCTL_BTL_T(Btl)
#define SPD_BTL_FROM_INDEX(Idx)         SPD_IOCTL_BTL(0, Idx, 0)

#define IsPipeHandle(Handle)            (((UINT_PTR)(Handle)) & 1)
#define GetPipeHandle(Handle)           ((HANDLE)((UINT_PTR)(Handle) & ~1))
#define SetPipeHandle(Handle)           ((HANDLE)((UINT_PTR)(Handle) | 1))
#define GetDeviceHandle(Handle)         (Handle)

typedef union
{
    SPD_IOCTL_TRANSACT_REQ Req;
    SPD_IOCTL_TRANSACT_RSP Rsp;
} TRANSACT_MSG;
typedef struct _HINT_VALUE_ITEM
{
    struct _HINT_VALUE_ITEM *HashNext;
    UINT64 Hint;
    ULONG Value;
} HINT_VALUE_ITEM;
typedef struct
{
    SPD_IOCTL_STORAGE_UNIT_PARAMS StorageUnitParams;
    HANDLE Event;
    HANDLE Pipe;
    SRWLOCK Lock;
    LONG Connected;
    HINT_VALUE_ITEM *HashBuckets[61];
} STORAGE_UNIT;

static SRWLOCK StorageUnitLock = SRWLOCK_INIT;
static STORAGE_UNIT **StorageUnits;

static inline UINT64 HashMix64(UINT64 k)
{
    k ^= k >> 33;
    k *= 0xff51afd7ed558ccdULL;
    k ^= k >> 33;
    k *= 0xc4ceb9fe1a85ec53ULL;
    k ^= k >> 33;
    return k;
}

static inline ULONG TakeHintValue(STORAGE_UNIT *StorageUnit, UINT64 Hint)
{
    ULONG Result = 0;

    AcquireSRWLockExclusive(&StorageUnit->Lock);

    ULONG HashIndex = HashMix64(Hint) %
        (sizeof StorageUnit->HashBuckets / sizeof StorageUnit->HashBuckets[0]);
    for (HINT_VALUE_ITEM **P = &StorageUnit->HashBuckets[HashIndex]; *P; P = &(*P)->HashNext)
        if ((*P)->Hint == Hint)
        {
            HINT_VALUE_ITEM *Item = *P;
            *P = (*P)->HashNext;
            Result = Item->Value;
            MemFree(Item);
            break;
        }

    ReleaseSRWLockExclusive(&StorageUnit->Lock);

    return Result;
}

static inline BOOLEAN PutHintValue(STORAGE_UNIT *StorageUnit, UINT64 Hint, ULONG Value)
{
    BOOLEAN Result = FALSE;

    AcquireSRWLockExclusive(&StorageUnit->Lock);

    ULONG HashIndex = HashMix64(Hint) %
        (sizeof StorageUnit->HashBuckets / sizeof StorageUnit->HashBuckets[0]);
    for (HINT_VALUE_ITEM *ItemX = StorageUnit->HashBuckets[HashIndex]; ItemX; ItemX = ItemX->HashNext)
        if (ItemX->Hint == Hint)
            goto exit;
    HINT_VALUE_ITEM *Item = MemAlloc(sizeof(HINT_VALUE_ITEM));
    if (0 == Item)
        goto exit;
    Item->Hint = Hint;
    Item->Value = Value;
    Item->HashNext = StorageUnit->HashBuckets[HashIndex];
    StorageUnit->HashBuckets[HashIndex] = Item;

    Result = TRUE;

exit:
    ReleaseSRWLockExclusive(&StorageUnit->Lock);

    return Result;
}

static inline GuidEqual(const GUID *a, const GUID *b)
{
    return
        a->Data1 == b->Data1 &&
        a->Data2 == b->Data2 &&
        a->Data3 == b->Data3 &&
        a->Data4[0] == b->Data4[0] &&
        a->Data4[1] == b->Data4[1] &&
        a->Data4[2] == b->Data4[2] &&
        a->Data4[3] == b->Data4[3] &&
        a->Data4[4] == b->Data4[4] &&
        a->Data4[5] == b->Data4[5] &&
        a->Data4[6] == b->Data4[6] &&
        a->Data4[7] == b->Data4[7];
}

static DWORD SpdStorageUnitHandleOpenPipe(PWSTR Name,
    const SPD_IOCTL_STORAGE_UNIT_PARAMS *StorageUnitParams,
    PHANDLE PHandle, PUINT32 PBtl)
{
    UINT32 Btl = (UINT32)-1;
    STORAGE_UNIT *StorageUnit = 0;
    STORAGE_UNIT *DuplicateUnit = 0;
    WCHAR PipeNameBuf[1024];
    DWORD Error;

    *PHandle = INVALID_HANDLE_VALUE;
    *PBtl = (UINT32)-1;

    AcquireSRWLockExclusive(&StorageUnitLock);

    if (0 == StorageUnits)
    {
        StorageUnits = MemAlloc(sizeof *StorageUnits * SPD_IOCTL_STORAGE_UNIT_MAX_CAPACITY);
        if (0 == StorageUnits)
        {
            Error = ERROR_NO_SYSTEM_RESOURCES;
            goto exit;
        }
        memset(StorageUnits, 0, sizeof *StorageUnits * SPD_IOCTL_STORAGE_UNIT_MAX_CAPACITY);
    }

    StorageUnit = MemAlloc(sizeof *StorageUnit);
    if (0 == StorageUnit)
    {
        Error = ERROR_NO_SYSTEM_RESOURCES;
        goto exit;
    }
    memset(StorageUnit, 0, sizeof *StorageUnit);
    memcpy(&StorageUnit->StorageUnitParams, StorageUnitParams, sizeof *StorageUnitParams);
    StorageUnit->Pipe = INVALID_HANDLE_VALUE;
    InitializeSRWLock(&StorageUnit->Lock);

    for (ULONG I = 0; SPD_IOCTL_STORAGE_UNIT_MAX_CAPACITY > I; I++)
    {
        STORAGE_UNIT *Unit = StorageUnits[I];
        if (0 == Unit)
        {
            if ((UINT32)-1 == Btl)
                Btl = SPD_BTL_FROM_INDEX(I);
            continue;
        }

        if (GuidEqual(&StorageUnit->StorageUnitParams.Guid, &Unit->StorageUnitParams.Guid))
        {
            DuplicateUnit = Unit;
            break;
        }
    }
    if (0 == DuplicateUnit && -1 != Btl)
        StorageUnits[SPD_INDEX_FROM_BTL(Btl)] = StorageUnit;

    if (0 != DuplicateUnit)
    {
        Error = ERROR_ALREADY_EXISTS;
        goto exit;
    }
    if (-1 == Btl)
    {
        Error = ERROR_CANNOT_MAKE;
        goto exit;
    }

    StorageUnit->Event = CreateEventW(0, TRUE, FALSE, 0);
    if (0 == StorageUnit->Event)
    {
        Error = GetLastError();
        goto exit;
    }

    wsprintfW(PipeNameBuf, L"%s\\%u", Name, (unsigned)SPD_INDEX_FROM_BTL(Btl));
    StorageUnit->Pipe = CreateNamedPipeW(PipeNameBuf,
        PIPE_ACCESS_DUPLEX |
            FILE_FLAG_FIRST_PIPE_INSTANCE | FILE_FLAG_WRITE_THROUGH | FILE_FLAG_OVERLAPPED,
        PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT | PIPE_REJECT_REMOTE_CLIENTS,
        1,
        sizeof(TRANSACT_MSG) + StorageUnit->StorageUnitParams.MaxTransferLength,
        sizeof(TRANSACT_MSG) + StorageUnit->StorageUnitParams.MaxTransferLength,
        60 * 60 * 1000,
        0);
    if (INVALID_HANDLE_VALUE == StorageUnit->Pipe)
    {
        Error = GetLastError();
        goto exit;
    }

    *PHandle = SetPipeHandle(StorageUnit);
    *PBtl = Btl;

    Error = ERROR_SUCCESS;

exit:
    if (ERROR_SUCCESS != Error)
    {
        if (0 == DuplicateUnit && -1 != Btl)
            StorageUnits[SPD_INDEX_FROM_BTL(Btl)] = 0;

        if (0 != StorageUnit)
        {
            if (INVALID_HANDLE_VALUE != StorageUnit->Pipe)
                CloseHandle(StorageUnit->Pipe);
            if (0 != StorageUnit->Event)
                CloseHandle(StorageUnit->Event);
            MemFree(StorageUnit);
        }
    }

    ReleaseSRWLockExclusive(&StorageUnitLock);

    return Error;
}

static inline DWORD WaitOverlappedResult(HANDLE StopEvent, BOOL Success,
    HANDLE Handle, OVERLAPPED *Overlapped, PDWORD PBytesTransferred)
{
    HANDLE WaitObjects[2];
    DWORD WaitResult;

    if (!Success && ERROR_IO_PENDING != GetLastError())
        return GetLastError();

    WaitObjects[0] = StopEvent;
    WaitObjects[1] = Overlapped->hEvent;
    WaitResult = WaitForMultipleObjects(2, WaitObjects, FALSE, INFINITE);
    if (WAIT_OBJECT_0 == WaitResult)
    {
        CancelIoEx(Handle, Overlapped);
        GetOverlappedResult(Handle, Overlapped, PBytesTransferred, TRUE);
        return ERROR_OPERATION_ABORTED;
    }
    else if (WAIT_OBJECT_0 + 1 == WaitResult)
    {
        if (!GetOverlappedResult(Handle, Overlapped, PBytesTransferred, TRUE))
            return GetLastError();
    }
    else
        return GetLastError();

    return ERROR_SUCCESS;
}

static DWORD SpdStorageUnitHandleTransactPipe(HANDLE Handle,
    UINT32 Btl,
    SPD_IOCTL_TRANSACT_RSP *Rsp,
    SPD_IOCTL_TRANSACT_REQ *Req,
    PVOID DataBuffer)
{
    STORAGE_UNIT *StorageUnit = Handle;
    LONG Connected;
    ULONG DataLength;
    TRANSACT_MSG *Msg = 0;
    OVERLAPPED Overlapped;
    DWORD BytesTransferred;
    DWORD Error;

    if ((0 == Req && 0 == Rsp) ||
        (0 != Req && 0 == DataBuffer))
        return ERROR_INVALID_PARAMETER;

    Error = SpdOverlappedInit(&Overlapped);
    if (ERROR_SUCCESS != Error)
        goto exit;

    AcquireSRWLockShared(&StorageUnitLock);
    Error = StorageUnit == StorageUnits[SPD_INDEX_FROM_BTL(Btl)] ?
        ERROR_SUCCESS : ERROR_FILE_NOT_FOUND;
    ReleaseSRWLockShared(&StorageUnitLock);
    if (ERROR_SUCCESS != Error)
        goto exit;

    Msg = MemAlloc(sizeof(TRANSACT_MSG) + StorageUnit->StorageUnitParams.MaxTransferLength);
    if (0 == Msg)
    {
        Error = ERROR_NO_SYSTEM_RESOURCES;
        goto exit;
    }

    Error = ERROR_SUCCESS;
    AcquireSRWLockExclusive(&StorageUnit->Lock);
    if (0 >= StorageUnit->Connected)
    {
        Error = WaitOverlappedResult(
            StorageUnit->Event,
            ConnectNamedPipe(StorageUnit->Pipe, &Overlapped),
            StorageUnit->Pipe, &Overlapped, &BytesTransferred);
        if (ERROR_SUCCESS == Error || ERROR_PIPE_CONNECTED == Error)
        {
            Error = WaitOverlappedResult(
                StorageUnit->Event,
                WriteFile(StorageUnit->Pipe,
                    &StorageUnit->StorageUnitParams, sizeof StorageUnit->StorageUnitParams,
                    0, &Overlapped),
                StorageUnit->Pipe, &Overlapped, &BytesTransferred);
            if (ERROR_SUCCESS == Error)
            {
                StorageUnit->Connected = -StorageUnit->Connected;
                StorageUnit->Connected++;
            }
            else
            {
                DisconnectNamedPipe(StorageUnit->Pipe);
                Error = ERROR_NO_DATA;
            }
        }
    }
    Connected = StorageUnit->Connected;
    ReleaseSRWLockExclusive(&StorageUnit->Lock);
    if (ERROR_NO_DATA == Error)
        goto zeroout;
    if (ERROR_SUCCESS != Error)
        goto exit;

    if (0 != Rsp)
    {
        DataLength = SpdIoctlTransactReadKind == Rsp->Kind && 0 != DataBuffer ?
            TakeHintValue(StorageUnit, Rsp->Hint) : 0;
        memcpy(Msg, Rsp, sizeof *Rsp);
        if (0 != DataLength)
            memcpy(Msg + 1, DataBuffer, DataLength);
        Error = WaitOverlappedResult(
            StorageUnit->Event,
            WriteFile(StorageUnit->Pipe, Msg, sizeof(TRANSACT_MSG) + DataLength, 0, &Overlapped),
            StorageUnit->Pipe, &Overlapped, &BytesTransferred);
        if (ERROR_SUCCESS != Error)
            goto disconnect;
    }

    if (0 != Req)
    {
        Error = WaitOverlappedResult(
            StorageUnit->Event,
            ReadFile(StorageUnit->Pipe,
                Msg, sizeof(TRANSACT_MSG) + StorageUnit->StorageUnitParams.MaxTransferLength, 0, &Overlapped),
            StorageUnit->Pipe, &Overlapped, &BytesTransferred);
        if (ERROR_SUCCESS != Error)
            goto disconnect;

        if (sizeof(TRANSACT_MSG) > BytesTransferred)
            goto zeroout;

        if (SpdIoctlTransactReadKind == Msg->Req.Kind)
        {
            DataLength = Msg->Req.Op.Read.BlockCount *
                StorageUnit->StorageUnitParams.BlockLength;
            if (DataLength > StorageUnit->StorageUnitParams.MaxTransferLength)
                goto zeroout;

            if (!PutHintValue(StorageUnit, Msg->Req.Hint, DataLength))
                goto zeroout;
        }
        else if (SpdIoctlTransactWriteKind == Msg->Req.Kind)
        {
            DataLength = Msg->Req.Op.Write.BlockCount *
                StorageUnit->StorageUnitParams.BlockLength;
            if (DataLength > StorageUnit->StorageUnitParams.MaxTransferLength)
                goto zeroout;

            BytesTransferred -= sizeof(TRANSACT_MSG);
            if (BytesTransferred > DataLength)
                BytesTransferred = DataLength;
            memcpy(DataBuffer, Msg + 1, BytesTransferred);
            memset((PUINT8)(DataBuffer) + BytesTransferred, 0, DataLength - BytesTransferred);
        }
        else if (SpdIoctlTransactUnmapKind == Msg->Req.Kind)
        {
            DataLength = Msg->Req.Op.Unmap.Count *
                sizeof(SPD_IOCTL_UNMAP_DESCRIPTOR);
            if (DataLength > StorageUnit->StorageUnitParams.MaxTransferLength)
                goto zeroout;

            BytesTransferred -= sizeof(TRANSACT_MSG);
            if (BytesTransferred > DataLength)
                BytesTransferred = DataLength;
            memcpy(DataBuffer, Msg + 1, BytesTransferred);
            memset((PUINT8)(DataBuffer) + BytesTransferred, 0, DataLength - BytesTransferred);
        }

        memcpy(Req, &Msg->Req, sizeof *Req);
    }

    Error = ERROR_SUCCESS;

exit:
    MemFree(Msg);

    SpdOverlappedFini(&Overlapped);

    return Error;

disconnect:
    AcquireSRWLockExclusive(&StorageUnit->Lock);
    if (Connected == StorageUnit->Connected)
    {
        DisconnectNamedPipe(StorageUnit->Pipe);
        StorageUnit->Connected = -StorageUnit->Connected;
    }
    ReleaseSRWLockExclusive(&StorageUnit->Lock);

zeroout:
    if (0 != Req)
        memset(Req, 0, sizeof *Req);

    Error = ERROR_SUCCESS;
    goto exit;
}

static DWORD SpdStorageUnitHandleShutdownPipe(HANDLE Handle,
    const GUID *Guid)
{
    STORAGE_UNIT *StorageUnit = Handle;
    ULONG Index = -1;
    DWORD Error;

    AcquireSRWLockExclusive(&StorageUnitLock);

    for (ULONG I = 0; SPD_IOCTL_STORAGE_UNIT_MAX_CAPACITY > I; I++)
    {
        STORAGE_UNIT *Unit = StorageUnits[I];
        if (0 == Unit)
            continue;

        if (GuidEqual(Guid, &Unit->StorageUnitParams.Guid))
        {
            Index = I;
            break;
        }
    }
    if (-1 != Index && StorageUnit == StorageUnits[Index])
        StorageUnits[Index] = 0;
    else
    {
        Error = ERROR_FILE_NOT_FOUND;
        goto exit;
    }

    SetEvent(StorageUnit->Event);

    Error = ERROR_SUCCESS;

exit:
    ReleaseSRWLockExclusive(&StorageUnitLock);

    return Error;
}

static DWORD SpdStorageUnitHandleClosePipe(HANDLE Handle)
{
    STORAGE_UNIT *StorageUnit = Handle;

    CloseHandle(StorageUnit->Pipe);
    CloseHandle(StorageUnit->Event);
    MemFree(StorageUnit);

    return ERROR_SUCCESS;
}

static DWORD SpdStorageUnitHandleOpenDevice(PWSTR Name,
    const SPD_IOCTL_STORAGE_UNIT_PARAMS *StorageUnitParams,
    PHANDLE PHandle, PUINT32 PBtl)
{
    HANDLE Handle = INVALID_HANDLE_VALUE;
    UINT32 Btl;
    DWORD Error;

    *PHandle = INVALID_HANDLE_VALUE;
    *PBtl = (UINT32)-1;

    Error = SpdIoctlOpenDevice(Name, &Handle);
    if (ERROR_SUCCESS != Error)
        goto exit;

    Error = SpdIoctlProvision(Handle, StorageUnitParams, &Btl);
    if (ERROR_SUCCESS != Error)
        goto exit;

    *PHandle = Handle;
    *PBtl = Btl;
    Error = ERROR_SUCCESS;

exit:
    if (ERROR_SUCCESS != Error)
    {
        if (INVALID_HANDLE_VALUE != Handle)
            CloseHandle(Handle);
    }

    return Error;
}

DWORD SpdStorageUnitHandleOpen(PWSTR Name,
    const SPD_IOCTL_STORAGE_UNIT_PARAMS *StorageUnitParams,
    PHANDLE PHandle, PUINT32 PBtl)
{
    if (L'\\' == Name[0] &&
        L'\\' == Name[1] &&
        L'.'  == Name[2] &&
        L'\\' == Name[3] &&
        L'p'  == Name[4] &&
        L'i'  == Name[5] &&
        L'p'  == Name[6] &&
        L'e'  == Name[7] &&
        L'\\' == Name[8])
        return SpdStorageUnitHandleOpenPipe(Name, StorageUnitParams, PHandle, PBtl);
    else
        return SpdStorageUnitHandleOpenDevice(Name, StorageUnitParams, PHandle, PBtl);
}

DWORD SpdStorageUnitHandleTransact(HANDLE Handle,
    UINT32 Btl,
    SPD_IOCTL_TRANSACT_RSP *Rsp,
    SPD_IOCTL_TRANSACT_REQ *Req,
    PVOID DataBuffer)
{
    if (IsPipeHandle(Handle))
        return SpdStorageUnitHandleTransactPipe(GetPipeHandle(Handle), Btl, Rsp, Req, DataBuffer);
    else
        return SpdIoctlTransact(GetDeviceHandle(Handle), Btl, Rsp, Req, DataBuffer);
}

DWORD SpdStorageUnitHandleShutdown(HANDLE Handle,
    const GUID *Guid)
{
    if (IsPipeHandle(Handle))
        return SpdStorageUnitHandleShutdownPipe(GetPipeHandle(Handle), Guid);
    else
        return SpdIoctlUnprovision(GetDeviceHandle(Handle), Guid);
}

DWORD SpdStorageUnitHandleClose(HANDLE Handle)
{
    if (IsPipeHandle(Handle))
        return SpdStorageUnitHandleClosePipe(GetPipeHandle(Handle));
    else
        return CloseHandle(GetDeviceHandle(Handle)) ? 0 : GetLastError();
}
