/**
 * @file stgtest.c
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

#define PROGNAME                        "stgtest"

#define info(format, ...)               \
    SpdPrintLog(GetStdHandle(STD_OUTPUT_HANDLE), format, __VA_ARGS__)
#define warn(format, ...)               \
    SpdPrintLog(GetStdHandle(STD_ERROR_HANDLE), format, __VA_ARGS__)
#define fail(ExitCode, format, ...)     \
    (SpdPrintLog(GetStdHandle(STD_ERROR_HANDLE), format, __VA_ARGS__), ExitProcess(ExitCode))

#define IsPipeHandle(Handle)            (((UINT_PTR)(Handle)) & 1)
#define GetPipeHandle(Handle)           ((HANDLE)((UINT_PTR)(Handle) & ~1))
#define SetPipeHandle(Handle)           ((HANDLE)((UINT_PTR)(Handle) | 1))
#define GetRawHandle(Handle)            (Handle)

typedef union
{
    SPD_IOCTL_TRANSACT_REQ Req;
    SPD_IOCTL_TRANSACT_RSP Rsp;
} TRANSACT_MSG;

static DWORD StgOpenPipe(PWSTR PipeName, ULONG Timeout,
    PHANDLE PHandle, SPD_IOCTL_STORAGE_UNIT_PARAMS *StorageUnitParams)
{
    HANDLE Handle = INVALID_HANDLE_VALUE;
    DWORD PipeMode;
    DWORD BytesTransferred;
    DWORD Error;

    *PHandle = INVALID_HANDLE_VALUE;

    Handle = CreateFileW(PipeName,
        GENERIC_READ | FILE_WRITE_DATA | FILE_WRITE_ATTRIBUTES,
        FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING,
        FILE_FLAG_OVERLAPPED | SECURITY_SQOS_PRESENT | SECURITY_IDENTIFICATION,
        0);
    if (INVALID_HANDLE_VALUE == Handle)
    {
        Error = GetLastError();
        if (ERROR_PIPE_BUSY != Error)
            goto exit;

        WaitNamedPipeW(PipeName, Timeout);

        Handle = CreateFileW(PipeName,
            GENERIC_READ | FILE_WRITE_DATA | FILE_WRITE_ATTRIBUTES,
            FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING,
            FILE_FLAG_OVERLAPPED | SECURITY_SQOS_PRESENT | SECURITY_IDENTIFICATION,
            0);
        if (INVALID_HANDLE_VALUE == Handle)
        {
            Error = GetLastError();
            goto exit;
        }
    }

    PipeMode = PIPE_READMODE_MESSAGE | PIPE_WAIT;
    if (!SetNamedPipeHandleState(Handle, &PipeMode, 0, 0))
    {
        Error = GetLastError();
        goto exit;
    }

    /* ok to use non-overlapped ReadFile with overlapped handle, because no other I/O is posted on it */
    if (!ReadFile(Handle, StorageUnitParams, sizeof *StorageUnitParams, &BytesTransferred, 0))
    {
        Error = GetLastError();
        goto exit;
    }
    if (sizeof *StorageUnitParams > BytesTransferred ||
        0 == StorageUnitParams->BlockCount ||
        sizeof(SPD_IOCTL_UNMAP_DESCRIPTOR) > StorageUnitParams->BlockLength ||
        0 == StorageUnitParams->MaxTransferLength ||
        0 != StorageUnitParams->MaxTransferLength % StorageUnitParams->BlockLength)
    {
        Error = ERROR_IO_DEVICE;
        goto exit;
    }

    *PHandle = SetPipeHandle(Handle);

    Error = ERROR_SUCCESS;

exit:
    if (ERROR_SUCCESS != Error)
    {
        if (INVALID_HANDLE_VALUE != Handle)
            CloseHandle(Handle);
    }

    return Error;
}

static DWORD StgTransactPipe(HANDLE Handle,
    SPD_IOCTL_TRANSACT_REQ *Req,
    SPD_IOCTL_TRANSACT_RSP *Rsp,
    PVOID DataBuffer,
    const SPD_IOCTL_STORAGE_UNIT_PARAMS *StorageUnitParams)
{
    ULONG DataLength;
    TRANSACT_MSG *Msg = 0;
    OVERLAPPED Overlapped;
    DWORD BytesTransferred;
    DWORD Error;

    if (0 == Req || 0 == Rsp)
        return ERROR_INVALID_PARAMETER;

    Error = SpdOverlappedInit(&Overlapped);
    if (ERROR_SUCCESS != Error)
        goto exit;

    Msg = MemAlloc(
        sizeof(TRANSACT_MSG) + StorageUnitParams->MaxTransferLength);
    if (0 == Msg)
    {
        Error = ERROR_NO_SYSTEM_RESOURCES;
        goto exit;
    }

    DataLength = 0;
    if (0 != DataBuffer)
        switch (Req->Kind)
        {
        case SpdIoctlTransactWriteKind:
            DataLength = Req->Op.Write.BlockCount * StorageUnitParams->BlockLength;
            break;
        case SpdIoctlTransactUnmapKind:
            DataLength = Req->Op.Unmap.Count * sizeof(SPD_IOCTL_UNMAP_DESCRIPTOR);
            break;
        default:
            break;
        }
    memcpy(Msg, Req, sizeof *Req);
    if (0 != DataLength)
        memcpy(Msg + 1, DataBuffer, DataLength);
    Error = SpdOverlappedWaitResult(
        WriteFile(Handle, Msg, sizeof(TRANSACT_MSG) + DataLength, 0, &Overlapped),
        Handle, &Overlapped, &BytesTransferred);
    if (ERROR_SUCCESS != Error)
        goto exit;

    Error = SpdOverlappedWaitResult(
        ReadFile(Handle,
            Msg, sizeof(TRANSACT_MSG) + StorageUnitParams->MaxTransferLength, 0, &Overlapped),
        Handle, &Overlapped, &BytesTransferred);
    if (ERROR_SUCCESS != Error)
        goto exit;
    if (sizeof(TRANSACT_MSG) > BytesTransferred || Req->Hint != Msg->Rsp.Hint)
    {
        Error = ERROR_IO_DEVICE;
        goto exit;
    }
    if (SpdIoctlTransactReadKind == Msg->Rsp.Kind && SCSISTAT_GOOD == Msg->Rsp.Status.ScsiStatus)
    {
        DataLength = Req->Op.Read.BlockCount * StorageUnitParams->BlockLength;
        if (DataLength > StorageUnitParams->MaxTransferLength)
        {
            Error = ERROR_IO_DEVICE;
            goto exit;
        }
        BytesTransferred -= sizeof(TRANSACT_MSG);
        if (BytesTransferred > DataLength)
            BytesTransferred = DataLength;
        memcpy(DataBuffer, Msg + 1, BytesTransferred);
        memset((PUINT8)(DataBuffer) + BytesTransferred, 0, DataLength - BytesTransferred);
    }
    memcpy(Rsp, &Msg->Rsp, sizeof *Rsp);

    Error = ERROR_SUCCESS;

exit:
    MemFree(Msg);

    SpdOverlappedFini(&Overlapped);

    return Error;
}

static DWORD StgOpenRaw(PWSTR Name, ULONG Timeout,
    PHANDLE PHandle, SPD_IOCTL_STORAGE_UNIT_PARAMS *StorageUnitParams)
{
    typedef struct _FILE_FS_SIZE_INFORMATION
    {
        LARGE_INTEGER TotalAllocationUnits;
        LARGE_INTEGER AvailableAllocationUnits;
        ULONG SectorsPerAllocationUnit;
        ULONG BytesPerSector;
    } FILE_FS_SIZE_INFORMATION, *PFILE_FS_SIZE_INFORMATION;
    typedef struct _IO_STATUS_BLOCK
    {
        union
        {
            NTSTATUS Status;
            PVOID Pointer;
        } DUMMYUNIONNAME;
        ULONG_PTR Information;
    } IO_STATUS_BLOCK, *PIO_STATUS_BLOCK;
    NTSTATUS NTAPI NtQueryVolumeInformationFile(
        HANDLE FileHandle,
        PIO_STATUS_BLOCK IoStatusBlock,
        PVOID FsInformation,
        ULONG Length,
        int FsInformationClass);
    ULONG NTAPI RtlNtStatusToDosError(
        NTSTATUS Status);

    HANDLE Handle = INVALID_HANDLE_VALUE;
    FILE_FS_SIZE_INFORMATION FsSizeInfo;
    IO_STATUS_BLOCK IoStatus;
    UINT32 Btl = 0;
    CDB Cdb;
    UINT8 DataBuffer[255];
    UINT32 DataLength;
    PVPD_BLOCK_LIMITS_PAGE BlockLimits;
    UCHAR ScsiStatus;
    union
    {
        SENSE_DATA Data;
        UCHAR Buffer[32];
    } Sense;
    DWORD Error;

    *PHandle = INVALID_HANDLE_VALUE;
    memset(StorageUnitParams, 0, sizeof *StorageUnitParams);

    Handle = CreateFileW(Name,
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING,
        FILE_FLAG_OVERLAPPED,
        0);
    if (INVALID_HANDLE_VALUE == Handle)
    {
        Error = GetLastError();
        goto exit;
    }

    IoStatus.Status = NtQueryVolumeInformationFile(Handle,
        &IoStatus, &FsSizeInfo, sizeof FsSizeInfo, 3/*FileFsSizeInformation*/);
    if (0 > IoStatus.Status)
    {
        Error = RtlNtStatusToDosError(IoStatus.Status);
        goto exit;
    }
    StorageUnitParams->BlockCount =
        FsSizeInfo.TotalAllocationUnits.QuadPart * FsSizeInfo.SectorsPerAllocationUnit;
    StorageUnitParams->BlockLength =
        FsSizeInfo.BytesPerSector;

    memset(&Cdb, 0, sizeof Cdb);
    Cdb.CDB6INQUIRY3.OperationCode = SCSIOP_INQUIRY;
    Cdb.CDB6INQUIRY3.EnableVitalProductData = 1;
    Cdb.CDB6INQUIRY3.PageCode = VPD_BLOCK_LIMITS;
    Cdb.CDB6INQUIRY3.AllocationLength = VPD_MAX_BUFFER_SIZE;
    DataLength = sizeof DataBuffer;
    Error = SpdIoctlScsiExecute(Handle, Btl, &Cdb, +1, DataBuffer, &DataLength,
        &ScsiStatus, Sense.Buffer);
    if (ERROR_SUCCESS != Error)
        goto exit;
    if (SCSISTAT_GOOD != ScsiStatus)
    {
        Error = ERROR_IO_DEVICE;
        goto exit;
    }
    BlockLimits = (PVOID)DataBuffer;
    StorageUnitParams->MaxTransferLength = StorageUnitParams->BlockLength * (
        (BlockLimits->MaximumTransferLength[0] << 24) |
        (BlockLimits->MaximumTransferLength[1] << 16) |
        (BlockLimits->MaximumTransferLength[2] << 8) |
        (BlockLimits->MaximumTransferLength[3]));
    if (0 == StorageUnitParams->MaxTransferLength)
        StorageUnitParams->MaxTransferLength = 64 * 1024;

    /*
     * To test our driver's chunking method we allow up to 3 * MaxTransferLength,
     * when using the RAW file system.
     */
    StorageUnitParams->MaxTransferLength *= 3;

    if (0 == StorageUnitParams->BlockCount ||
        0 == StorageUnitParams->BlockLength)
    {
        Error = ERROR_IO_DEVICE;
        goto exit;
    }

    *PHandle = Handle;

    Error = ERROR_SUCCESS;

exit:
    if (ERROR_SUCCESS != Error)
    {
        if (INVALID_HANDLE_VALUE != Handle)
            CloseHandle(Handle);
    }

    return Error;
}

static DWORD StgTransactRaw(HANDLE Handle,
    SPD_IOCTL_TRANSACT_REQ *Req,
    SPD_IOCTL_TRANSACT_RSP *Rsp,
    PVOID DataBuffer,
    const SPD_IOCTL_STORAGE_UNIT_PARAMS *StorageUnitParams)
{
    LARGE_INTEGER Offset;
    ULONG DataLength;
    OVERLAPPED Overlapped;
    DWORD BytesTransferred;
    DWORD Error;

    if (0 == Req || 0 == Rsp ||
        (SpdIoctlTransactReadKind != Req->Kind && SpdIoctlTransactWriteKind != Req->Kind) ||
        0 == DataBuffer)
        return ERROR_INVALID_PARAMETER;

    Error = SpdOverlappedInit(&Overlapped);
    if (ERROR_SUCCESS != Error)
        goto exit;

    DataLength = 0;
    switch (Req->Kind)
    {
    case SpdIoctlTransactWriteKind:
        Offset.QuadPart = Req->Op.Write.BlockAddress * StorageUnitParams->BlockLength;
        DataLength = Req->Op.Write.BlockCount * StorageUnitParams->BlockLength;
        Overlapped.Offset = Offset.LowPart;
        Overlapped.OffsetHigh = Offset.HighPart;
        Error = SpdOverlappedWaitResult(
            WriteFile(Handle, DataBuffer, DataLength, 0, &Overlapped),
            Handle, &Overlapped, &BytesTransferred);
        break;
    case SpdIoctlTransactReadKind:
        Offset.QuadPart = Req->Op.Read.BlockAddress * StorageUnitParams->BlockLength;
        DataLength = Req->Op.Read.BlockCount * StorageUnitParams->BlockLength;
        Overlapped.Offset = Offset.LowPart;
        Overlapped.OffsetHigh = Offset.HighPart;
        Error = SpdOverlappedWaitResult(
            ReadFile(Handle, DataBuffer, DataLength, 0, &Overlapped),
            Handle, &Overlapped, &BytesTransferred);
        break;
    }
    if (ERROR_SUCCESS != Error)
        goto exit;
    if (DataLength != BytesTransferred)
    {
        Error = ERROR_IO_DEVICE;
        goto exit;
    }

    memset(Rsp, 0, sizeof *Rsp);
    Rsp->Hint = Req->Hint;
    Rsp->Kind = Req->Kind;
    Rsp->Status.ScsiStatus = SCSISTAT_GOOD;

    Error = ERROR_SUCCESS;

exit:
    SpdOverlappedFini(&Overlapped);

    return Error;
}

static DWORD StgOpen(PWSTR Name, ULONG Timeout,
    PHANDLE PHandle, SPD_IOCTL_STORAGE_UNIT_PARAMS *StorageUnitParams)
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
        return StgOpenPipe(Name, Timeout, PHandle, StorageUnitParams);
    else
        return StgOpenRaw(Name, Timeout, PHandle, StorageUnitParams);
}

static DWORD StgTransact(HANDLE Handle,
    SPD_IOCTL_TRANSACT_REQ *Req,
    SPD_IOCTL_TRANSACT_RSP *Rsp,
    PVOID DataBuffer,
    const SPD_IOCTL_STORAGE_UNIT_PARAMS *StorageUnitParams)
{
    if (IsPipeHandle(Handle))
        return StgTransactPipe(GetPipeHandle(Handle), Req, Rsp, DataBuffer, StorageUnitParams);
    else
        return StgTransactRaw(GetRawHandle(Handle), Req, Rsp, DataBuffer, StorageUnitParams);
}

DWORD StgClose(HANDLE Handle)
{
    if (IsPipeHandle(Handle))
        return CloseHandle(GetPipeHandle(Handle)) ? 0 : GetLastError();
    else
        return CloseHandle(GetRawHandle(Handle)) ? 0 : GetLastError();
}

static inline UINT64 HashMix64(UINT64 k)
{
    k ^= k >> 33;
    k *= 0xff51afd7ed558ccdULL;
    k ^= k >> 33;
    k *= 0xc4ceb9fe1a85ec53ULL;
    k ^= k >> 33;
    return k;
}

static int FillOrTest(PVOID DataBuffer, UINT32 BlockLength, UINT64 BlockAddress, UINT32 BlockCount,
    UINT8 FillOrTestOpKind)
{
    for (ULONG I = 0, N = BlockCount; N > I; I++)
    {
        PUINT64 Buffer = (PVOID)((PUINT8)DataBuffer + I * BlockLength);
        UINT64 HashAddress = HashMix64(BlockAddress + I + 1);
        for (ULONG J = 0, M = BlockLength / 8; M > J; J++)
            if (SpdIoctlTransactReservedKind == FillOrTestOpKind)
                /* fill buffer */
                Buffer[J] = HashAddress;
            else if (SpdIoctlTransactWriteKind == FillOrTestOpKind)
            {
                /* test buffer for Write */
                if (Buffer[J] != HashAddress)
                    return 0;
            }
            else if (SpdIoctlTransactUnmapKind == FillOrTestOpKind)
            {
                /* test buffer for Unmap */
                if (Buffer[J] != 0)
                    return 0;
            }
    }
    return 1;
}

static VOID GenRandomBytes(PULONG PSeed, PVOID Buffer, ULONG Size)
{
    ULONG Seed = 0 != *PSeed ? *PSeed : 1;
    for (PUINT8 P = Buffer, EndP = P + Size; EndP > P; P++)
    {
        /* see ucrt sources */
        Seed = Seed * 214013 + 2531011;
        *P = (UINT8)(Seed >> 16);
    }
    *PSeed = Seed;
}

static void OpWarn(UINT8 OpKind, UINT64 BlockAddress, UINT32 BlockCount,
    const char *Description, const char *Detail, DWORD Error)
{
    PWSTR OpName;
    switch (OpKind)
    {
    case SpdIoctlTransactReadKind:
        OpName = L"Read";
        break;
    case SpdIoctlTransactWriteKind:
        OpName = L"Write";
        break;
    case SpdIoctlTransactFlushKind:
        OpName = L"Flush";
        break;
    case SpdIoctlTransactUnmapKind:
        OpName = L"Unmap";
        break;
    default:
        OpName = L"INVLD";
        break;
    }

    if (0 != Detail)
        warn(L"%s(Address=%x:%x, Count=%u): %S: %S",
            OpName, (UINT32)(BlockAddress >> 32), (UINT32)BlockAddress, BlockCount,
            Description, Detail);
    else
        warn(L"%s(Address=%x:%x, Count=%u): %S: %lu",
            OpName, (UINT32)(BlockAddress >> 32), (UINT32)BlockAddress, BlockCount,
            Description, Error);
}

static int run(PWSTR PipeName, ULONG OpCount, PWSTR OpSet, UINT64 BlockAddress, UINT32 BlockCount,
    PULONG RandomSeed)
{
#define CheckCondition(x)               \
    if (!(x))                           \
    {                                   \
        OpWarn(Req.Kind, BlockAddress, OpBlockCount, "condition fail", #x, 0);\
        Error = ERROR_IO_DEVICE;        \
        goto exit;                      \
    }                                   \
    else
    HANDLE Handle = INVALID_HANDLE_VALUE;
    SPD_IOCTL_STORAGE_UNIT_PARAMS StorageUnitParams;
    SPD_IOCTL_TRANSACT_REQ Req;
    SPD_IOCTL_TRANSACT_RSP Rsp;
    PVOID DataBuffer = 0;
    UINT8 OpKinds[32];
    ULONG OpKindCount;
    UINT8 TestOpKind;
    BOOLEAN RandomAddress, RandomCount;
    UINT32 MaxBlockCount, OpBlockCount;
    DWORD ThreadId;
    DWORD Error;

    Error = StgOpen(PipeName, 3000, &Handle, &StorageUnitParams);
    if (ERROR_SUCCESS != Error)
    {
        warn(L"cannot open %s: %lu", PipeName, Error);
        goto exit;
    }

    DataBuffer = MemAlloc(StorageUnitParams.MaxTransferLength);
    if (0 == DataBuffer)
    {
        Error = ERROR_NO_SYSTEM_RESOURCES;
        warn(L"cannot allocate memory");
        goto exit;
    }

    if (0 == OpCount)
        OpCount = 1;

    OpKindCount = 0;
    for (ULONG I = 0, N = sizeof OpKinds / sizeof OpKinds[0]; N > I && L'\0' != OpSet[I]; I++)
        switch (OpSet[I])
        {
        case 'R': case 'r':
            OpKinds[OpKindCount++] = SpdIoctlTransactReadKind;
            break;
        case 'W': case 'w':
            OpKinds[OpKindCount++] = SpdIoctlTransactWriteKind;
            break;
        case 'F': case 'f':
            OpKinds[OpKindCount++] = SpdIoctlTransactFlushKind;
            break;
        case 'U': case 'u':
            OpKinds[OpKindCount++] = SpdIoctlTransactUnmapKind;
            break;
        }
    if (0 == OpKindCount)
    {
        OpKinds[OpKindCount++] = SpdIoctlTransactWriteKind;
        OpKinds[OpKindCount++] = SpdIoctlTransactReadKind;
    }

    RandomAddress = -1 == BlockAddress;
    RandomCount = -1 == BlockCount;
    MaxBlockCount = StorageUnitParams.MaxTransferLength / StorageUnitParams.BlockLength;
    ThreadId = GetCurrentThreadId();

    for (ULONG I = 0, J = 0; OpCount > I; I++)
    {
        if (0 == J)
        {
            if (RandomAddress)
                GenRandomBytes(RandomSeed, &BlockAddress, sizeof BlockAddress);
            else if (0 != I)
                BlockAddress += BlockCount;
            BlockAddress %= StorageUnitParams.BlockCount;

            if (RandomCount)
            {
                GenRandomBytes(RandomSeed, &BlockCount, sizeof BlockCount);
                BlockCount %= MaxBlockCount;
            }
            else if (BlockCount > MaxBlockCount)
                BlockCount = MaxBlockCount;
            if (BlockCount == 0)
                BlockCount = 1;

            OpBlockCount = BlockAddress + BlockCount <= StorageUnitParams.BlockCount ?
                BlockCount : (UINT32)(StorageUnitParams.BlockCount - BlockAddress);

            TestOpKind = SpdIoctlTransactReservedKind;
        }

        memset(&Req, 0, sizeof Req);
        memset(&Rsp, 0, sizeof Rsp);

        Req.Hint = ((UINT64)ThreadId << 32) | I;
        Req.Kind = OpKinds[I % OpKindCount];
        switch (Req.Kind)
        {
        case SpdIoctlTransactReadKind:
            Req.Op.Read.BlockAddress = BlockAddress;
            Req.Op.Read.BlockCount = OpBlockCount;
            Req.Op.Read.ForceUnitAccess = !StorageUnitParams.CacheSupported;
            break;
        case SpdIoctlTransactWriteKind:
            Req.Op.Write.BlockAddress = BlockAddress;
            Req.Op.Write.BlockCount = OpBlockCount;
            Req.Op.Write.ForceUnitAccess = !StorageUnitParams.CacheSupported;
            FillOrTest(DataBuffer, StorageUnitParams.BlockLength, BlockAddress, OpBlockCount, 0);
            TestOpKind = SpdIoctlTransactWriteKind;
            break;
        case SpdIoctlTransactFlushKind:
            Req.Op.Flush.BlockAddress = BlockAddress;
            Req.Op.Flush.BlockCount = OpBlockCount;
            break;
        case SpdIoctlTransactUnmapKind:
            Req.Op.Unmap.Count = 1;
            ((SPD_IOCTL_UNMAP_DESCRIPTOR *)DataBuffer)->BlockAddress = BlockAddress;
            ((SPD_IOCTL_UNMAP_DESCRIPTOR *)DataBuffer)->BlockCount = OpBlockCount;
            ((SPD_IOCTL_UNMAP_DESCRIPTOR *)DataBuffer)->Reserved = 0;
            TestOpKind = SpdIoctlTransactUnmapKind;
            break;
        }

        Error = StgTransact(Handle, &Req, &Rsp, DataBuffer, &StorageUnitParams);
        if (ERROR_SUCCESS != Error)
        {
            OpWarn(Req.Kind, BlockAddress, OpBlockCount, "transact error", 0, Error);
            goto exit;
        }

        CheckCondition(Req.Hint == Rsp.Hint);
        CheckCondition(Req.Kind == Rsp.Kind);
        CheckCondition(SCSISTAT_GOOD == Rsp.Status.ScsiStatus);
        switch (Rsp.Kind)
        {
        case SpdIoctlTransactReadKind:
            if (SpdIoctlTransactWriteKind == TestOpKind)
            {
                /* test buffer after Write */
                if (!FillOrTest(DataBuffer, StorageUnitParams.BlockLength, BlockAddress, OpBlockCount,
                    SpdIoctlTransactWriteKind))
                {
                    OpWarn(Req.Kind, BlockAddress, OpBlockCount, "bad buffer", "after Write", 0);
                    Error = ERROR_IO_DEVICE;
                    goto exit;
                }
            }
            else if (SpdIoctlTransactUnmapKind == TestOpKind)
            {
                /* test buffer after Unmap */
                if (!FillOrTest(DataBuffer, StorageUnitParams.BlockLength, BlockAddress, OpBlockCount,
                    SpdIoctlTransactUnmapKind))
                {
                    OpWarn(Req.Kind, BlockAddress, OpBlockCount, "bad buffer", "after Unmap", 0);
                    Error = ERROR_IO_DEVICE;
                    goto exit;
                }
            }
            break;
        }

        J++;
        J %= OpKindCount;
    }

    Error = ERROR_SUCCESS;

exit:
    MemFree(DataBuffer);

    if (INVALID_HANDLE_VALUE != Handle)
        StgClose(Handle);

    return Error;
#undef CheckCondition
}

static void usage(void)
{
    warn(L""
        "usage: %s [-s Seed] \\\\.\\pipe\\PipeName\\Target OpCount [RWFU] [Address|*] [Count|*]\n"
        "usage: %s [-s Seed] \\\\.\\X: OpCount [RWFU] [Address|*] [Count|*]\n"
        "    -s Seed     Seed to use for randomness (default: time)\n"
        "    PipeName    Name of storage unit pipe\n"
        "    Target      SCSI target id (usually 0)\n"
        "    X:          Volume drive (must use RAW file system; requires admin)\n"
        "    OpCount     Operation count\n"
        "    RWFU        One or more: R: Read, W: Write, F: Flush, U: Unmap\n"
        "    Address     Starting block address, *: random\n"
        "    Count       Block count per operation, *: random\n"
        "",
        L"" PROGNAME, L"" PROGNAME);

    ExitProcess(ERROR_INVALID_PARAMETER);
}

int wmain(int argc, wchar_t **argv)
{
    PWSTR PipeName = 0;
    ULONG OpCount = 0;
    PWSTR OpSet = L"";
    UINT64 BlockAddress = 0;
    UINT32 BlockCount = 0;
    ULONG RandomSeed = 1;
    wchar_t *endp;

    argc--;
    argv++;
    if (0 != argv[0] && L'-' == argv[0][0] && L's' == argv[0][1] && L'\0' == argv[0][2] &&
        0 != argv[1])
    {
        RandomSeed = (ULONG)wcstoint(argv[1], 0, 0, &endp);
        argc -= 2;
        argv += 2;
    }
    else
        RandomSeed = GetTickCount();

    if (2 > argc || 5 < argc)
        usage();

    PipeName = argv[0];
    OpCount = (ULONG)wcstoint(argv[1], 0, 0, &endp);
    if (3 <= argc)
        OpSet = argv[2];
    if (4 <= argc)
        BlockAddress = L'*' == argv[3][0] && L'\0' == argv[3][1] ?
            -1 : wcstoint(argv[3], 0, 0, &endp);
    if (5 <= argc)
        BlockCount = L'*' == argv[4][0] && L'\0' == argv[4][1] ?
            -1 : (UINT32)wcstoint(argv[4], 0, 0, &endp);

    WCHAR BlockAddressStr[64], BlockCountStr[64];
    BlockAddressStr[0] = BlockCountStr[0] = L'*';
    BlockAddressStr[1] = BlockCountStr[1] = L'\0';
    if (-1 != BlockAddress)
        wsprintfW(BlockAddressStr, L"%x:%x", (UINT32)(BlockAddress >> 32), BlockAddress);
    if (-1 != BlockCount)
        wsprintfW(BlockCountStr, L"%lu", BlockCount);
    info(L"%s -s %lu %s %lu \"%s\" %s %s",
        L"" PROGNAME, RandomSeed, PipeName, OpCount, OpSet, BlockAddressStr, BlockCountStr);

    int ExitCode = run(PipeName, OpCount, OpSet, BlockAddress, BlockCount, &RandomSeed);
    if (0 == ExitCode)
        info(L"OK");
    return ExitCode;
}

void wmainCRTStartup(void)
{
    DWORD Argc;
    PWSTR *Argv;

    Argv = CommandLineToArgvW(GetCommandLineW(), &Argc);
    if (0 == Argv)
        ExitProcess(GetLastError());

    ExitProcess(wmain(Argc, Argv));
}
