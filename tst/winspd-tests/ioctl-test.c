/**
 * @file ioctl-test.c
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

#include <winspd/winspd.h>
#include <shared/minimal.h>
#include <tlib/testsuite.h>
#include <process.h>
#include <strsafe.h>

static const GUID TestGuid = 
    { 0x4112a9a1, 0xf079, 0x4f3d, { 0xba, 0x53, 0x2d, 0x5d, 0xf2, 0x7d, 0x28, 0xb5 } };
static const GUID TestGuid2 = 
    { 0xd7f5a95d, 0xb9f0, 0x4e47, { 0x87, 0x3b, 0xa, 0xb0, 0xa, 0x89, 0xf9, 0x5a } };

static void ioctl_provision_test(void)
{
    SPD_IOCTL_STORAGE_UNIT_PARAMS StorageUnitParams;
    HANDLE DeviceHandle;
    UINT32 Btl;
    DWORD Error;
    BOOL Success;

    Error = SpdIoctlOpenDevice(L"" SPD_IOCTL_HARDWARE_ID, &DeviceHandle);
    ASSERT(ERROR_SUCCESS == Error);

    memset(&StorageUnitParams, 0, sizeof StorageUnitParams);
    memcpy(&StorageUnitParams.Guid, &TestGuid, sizeof TestGuid);
    StorageUnitParams.BlockCount = 16;
    StorageUnitParams.BlockLength = 512;
    StorageUnitParams.MaxTransferLength = 512;
    Error = SpdIoctlProvision(DeviceHandle, &StorageUnitParams, &Btl);
    ASSERT(ERROR_SUCCESS == Error);
    ASSERT(0 == Btl);

    Error = SpdIoctlUnprovision(DeviceHandle, &StorageUnitParams.Guid);
    ASSERT(ERROR_SUCCESS == Error);

    Success = CloseHandle(DeviceHandle);
    ASSERT(Success);
}

static void ioctl_provision_invalid_test(void)
{
    SPD_IOCTL_STORAGE_UNIT_PARAMS StorageUnitParams;
    HANDLE DeviceHandle;
    UINT32 Btl;
    DWORD Error;
    BOOL Success;

    Error = SpdIoctlOpenDevice(L"" SPD_IOCTL_HARDWARE_ID, &DeviceHandle);
    ASSERT(ERROR_SUCCESS == Error);

    memset(&StorageUnitParams, 0, sizeof StorageUnitParams);
    Error = SpdIoctlProvision(DeviceHandle, &StorageUnitParams, &Btl);
    ASSERT(ERROR_INVALID_PARAMETER == Error);
    ASSERT((UINT32)-1 == Btl);

    memcpy(&StorageUnitParams.Guid, &TestGuid, sizeof TestGuid);
    Error = SpdIoctlProvision(DeviceHandle, &StorageUnitParams, &Btl);
    ASSERT(ERROR_INVALID_PARAMETER == Error);
    ASSERT((UINT32)-1 == Btl);

    StorageUnitParams.BlockCount = 16;
    StorageUnitParams.BlockLength = 512;
    StorageUnitParams.MaxTransferLength = 512 + 1;
    Error = SpdIoctlProvision(DeviceHandle, &StorageUnitParams, &Btl);
    ASSERT(ERROR_INVALID_PARAMETER == Error);
    ASSERT((UINT32)-1 == Btl);

    StorageUnitParams.MaxTransferLength = 512;
    Error = SpdIoctlProvision(DeviceHandle, &StorageUnitParams, &Btl);
    ASSERT(ERROR_SUCCESS == Error);
    ASSERT(0 == Btl);

    Error = SpdIoctlUnprovision(DeviceHandle, &StorageUnitParams.Guid);
    ASSERT(ERROR_SUCCESS == Error);

    Success = CloseHandle(DeviceHandle);
    ASSERT(Success);
}

static void ioctl_provision_multi_test(void)
{
    SPD_IOCTL_STORAGE_UNIT_PARAMS StorageUnitParams;
    HANDLE DeviceHandle;
    UINT32 Btl;
    DWORD Error;
    BOOL Success;

    Error = SpdIoctlOpenDevice(L"" SPD_IOCTL_HARDWARE_ID, &DeviceHandle);
    ASSERT(ERROR_SUCCESS == Error);

    memset(&StorageUnitParams, 0, sizeof StorageUnitParams);
    memcpy(&StorageUnitParams.Guid, &TestGuid, sizeof TestGuid);
    StorageUnitParams.BlockCount = 16;
    StorageUnitParams.BlockLength = 512;
    StorageUnitParams.MaxTransferLength = 512;
    Error = SpdIoctlProvision(DeviceHandle, &StorageUnitParams, &Btl);
    ASSERT(ERROR_SUCCESS == Error);
    ASSERT(0 == Btl);

    Error = SpdIoctlProvision(DeviceHandle, &StorageUnitParams, &Btl);
    ASSERT(ERROR_ALREADY_EXISTS == Error);
    ASSERT((UINT32)-1 == Btl);

    memset(&StorageUnitParams, 0, sizeof StorageUnitParams);
    memcpy(&StorageUnitParams.Guid, &TestGuid2, sizeof TestGuid2);
    StorageUnitParams.BlockCount = 16;
    StorageUnitParams.BlockLength = 512;
    StorageUnitParams.MaxTransferLength = 512;
    Error = SpdIoctlProvision(DeviceHandle, &StorageUnitParams, &Btl);
    ASSERT(ERROR_SUCCESS == Error);
    ASSERT(SPD_IOCTL_BTL(0, 1, 0) == Btl);

    Error = SpdIoctlUnprovision(DeviceHandle, &TestGuid);
    ASSERT(ERROR_SUCCESS == Error);

    Error = SpdIoctlUnprovision(DeviceHandle, &TestGuid2);
    ASSERT(ERROR_SUCCESS == Error);

    Error = SpdIoctlUnprovision(DeviceHandle, &TestGuid);
    ASSERT(ERROR_FILE_NOT_FOUND == Error);

    Success = CloseHandle(DeviceHandle);
    ASSERT(Success);
}

static void ioctl_provision_toomany_test(void)
{
    ULONG StorageUnitCapacity = SPD_IOCTL_STORAGE_UNIT_CAPACITY; // !!!: read from registry
    SPD_IOCTL_STORAGE_UNIT_PARAMS StorageUnitParams;
    HANDLE DeviceHandle;
    UINT32 Btl;
    DWORD Error;
    BOOL Success;

    Error = SpdIoctlOpenDevice(L"" SPD_IOCTL_HARDWARE_ID, &DeviceHandle);
    ASSERT(ERROR_SUCCESS == Error);

    for (ULONG I = 0; StorageUnitCapacity > I; I++)
    {
        memset(&StorageUnitParams, 0, sizeof StorageUnitParams);
        StorageUnitParams.Guid.Data1 = I;
        StorageUnitParams.Guid.Data2 = 42;
        StorageUnitParams.BlockCount = 16;
        StorageUnitParams.BlockLength = 512;
        StorageUnitParams.MaxTransferLength = 512;
        Error = SpdIoctlProvision(DeviceHandle, &StorageUnitParams, &Btl);
        ASSERT(ERROR_SUCCESS == Error);
        ASSERT(SPD_IOCTL_BTL(0, I, 0) == Btl);
    }

    memset(&StorageUnitParams, 0, sizeof StorageUnitParams);
    memcpy(&StorageUnitParams.Guid, &TestGuid, sizeof TestGuid);
    StorageUnitParams.BlockCount = 16;
    StorageUnitParams.BlockLength = 512;
    StorageUnitParams.MaxTransferLength = 512;
    Error = SpdIoctlProvision(DeviceHandle, &StorageUnitParams, &Btl);
    ASSERT(ERROR_CANNOT_MAKE == Error);
    ASSERT(-1 == Btl);

    for (ULONG I = 0; StorageUnitCapacity > I; I++)
    {
        memset(&StorageUnitParams, 0, sizeof StorageUnitParams);
        StorageUnitParams.Guid.Data1 = I;
        StorageUnitParams.Guid.Data2 = 42;
        Error = SpdIoctlUnprovision(DeviceHandle, &StorageUnitParams.Guid);
        ASSERT(ERROR_SUCCESS == Error);
    }

    Success = CloseHandle(DeviceHandle);
    ASSERT(Success);
}

static void ioctl_list_test(void)
{
    SPD_IOCTL_STORAGE_UNIT_PARAMS StorageUnitParams;
    HANDLE DeviceHandle;
    UINT32 Btl;
    UINT32 BtlBuf[256];
    UINT32 BtlBufSize;
    DWORD Error;
    BOOL Success;

    Error = SpdIoctlOpenDevice(L"" SPD_IOCTL_HARDWARE_ID, &DeviceHandle);
    ASSERT(ERROR_SUCCESS == Error);

    memset(&StorageUnitParams, 0, sizeof StorageUnitParams);
    memcpy(&StorageUnitParams.Guid, &TestGuid, sizeof TestGuid);
    StorageUnitParams.BlockCount = 16;
    StorageUnitParams.BlockLength = 512;
    StorageUnitParams.MaxTransferLength = 512;
    Error = SpdIoctlProvision(DeviceHandle, &StorageUnitParams, &Btl);
    ASSERT(ERROR_SUCCESS == Error);
    ASSERT(0 == Btl);

    memset(&StorageUnitParams, 0, sizeof StorageUnitParams);
    memcpy(&StorageUnitParams.Guid, &TestGuid2, sizeof TestGuid2);
    StorageUnitParams.BlockCount = 16;
    StorageUnitParams.BlockLength = 512;
    StorageUnitParams.MaxTransferLength = 512;
    Error = SpdIoctlProvision(DeviceHandle, &StorageUnitParams, &Btl);
    ASSERT(ERROR_SUCCESS == Error);
    ASSERT(SPD_IOCTL_BTL(0, 1, 0) == Btl);

    BtlBufSize = sizeof(UINT32);
    Error = SpdIoctlGetList(DeviceHandle, BtlBuf, &BtlBufSize);
    ASSERT(ERROR_INSUFFICIENT_BUFFER == Error);

    BtlBufSize = sizeof(BtlBuf);
    Error = SpdIoctlGetList(DeviceHandle, BtlBuf, &BtlBufSize);
    ASSERT(ERROR_SUCCESS == Error);
    ASSERT(2 * sizeof(UINT32) == BtlBufSize);
    ASSERT(0 == BtlBuf[0]);
    ASSERT(SPD_IOCTL_BTL(0, 1, 0) == BtlBuf[1]);

    Error = SpdIoctlUnprovision(DeviceHandle, &TestGuid);
    ASSERT(ERROR_SUCCESS == Error);

    Error = SpdIoctlUnprovision(DeviceHandle, &TestGuid2);
    ASSERT(ERROR_SUCCESS == Error);

    Success = CloseHandle(DeviceHandle);
    ASSERT(Success);
}

/* begin: from stgtest.c */
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
/* end: from stgtest.c */

static unsigned __stdcall ioctl_transact_read_test_thread(void *Data)
{
    UINT32 Btl = (UINT32)(UINT_PTR)Data;
    HANDLE DeviceHandle;
    DWORD Error;
    CDB Cdb;
    UINT8 DataBuffer[5 * 512];
    UINT32 DataLength;
    UCHAR ScsiStatus;
    union
    {
        SENSE_DATA Data;
        UCHAR Buffer[32];
    } Sense;

    Error = SpdIoctlOpenDevice(L"" SPD_IOCTL_HARDWARE_ID, &DeviceHandle);
    if (ERROR_SUCCESS != Error)
        goto exit;

    SpdIoctlScsiInquiry(DeviceHandle, Btl, 0, 3000);

    memset(&Cdb, 0, sizeof Cdb);
    Cdb.READ16.OperationCode = SCSIOP_READ16;
    Cdb.READ16.LogicalBlock[7] = 7;
    Cdb.READ16.TransferLength[3] = 5;

    memset(DataBuffer, 0, sizeof DataBuffer);
    DataLength = sizeof DataBuffer;
    Error = SpdIoctlScsiExecute(DeviceHandle, Btl, &Cdb, +1, DataBuffer, &DataLength,
        &ScsiStatus, Sense.Buffer);

    CloseHandle(DeviceHandle);

    if (ERROR_SUCCESS != Error)
        goto exit;

    if (ScsiStatus != SCSISTAT_GOOD ||
        5 * 512 != DataLength)
    {
        Error = -'ASR1';
        goto exit;
    }

    if (!FillOrTest(DataBuffer, 512, 7, 5, SpdIoctlTransactWriteKind))
    {
        Error = -'ASR2';
        goto exit;
    }

    Error = ERROR_SUCCESS;

exit:
    tlib_printf("thread=%lu ", Error);

    return Error;
}

static void ioctl_transact_read_dotest(ULONG MaxBlockCount)
{
    SPD_IOCTL_STORAGE_UNIT_PARAMS StorageUnitParams;
    SPD_IOCTL_TRANSACT_REQ Req;
    SPD_IOCTL_TRANSACT_RSP Rsp;
    PVOID DataBuffer = 0;
    OVERLAPPED Overlapped;
    HANDLE DeviceHandle;
    UINT32 Btl;
    DWORD Error;
    BOOL Success;
    HANDLE Thread;
    DWORD ExitCode;

    DataBuffer = malloc(MaxBlockCount * 512);
    ASSERT(0 != DataBuffer);

    Error = SpdOverlappedInit(&Overlapped);
    ASSERT(ERROR_SUCCESS == Error);

    Error = SpdIoctlOpenDevice(L"" SPD_IOCTL_HARDWARE_ID, &DeviceHandle);
    ASSERT(ERROR_SUCCESS == Error);

    memset(&StorageUnitParams, 0, sizeof StorageUnitParams);
    memcpy(&StorageUnitParams.Guid, &TestGuid, sizeof TestGuid);
    StorageUnitParams.BlockCount = 16;
    StorageUnitParams.BlockLength = 512;
    StorageUnitParams.MaxTransferLength = MaxBlockCount * 512;
    Error = SpdIoctlProvision(DeviceHandle, &StorageUnitParams, &Btl);
    ASSERT(ERROR_SUCCESS == Error);
    ASSERT(0 == Btl);

    Error = SpdIoctlScsiInquiry(DeviceHandle, Btl, 0, 3000);
    ASSERT(ERROR_SUCCESS == Error);

    Thread = (HANDLE)_beginthreadex(0, 0, ioctl_transact_read_test_thread, (PVOID)(UINT_PTR)Btl, 0, 0);
    ASSERT(0 != Thread);

    memset(DataBuffer, 0, sizeof DataBuffer);
    Error = SpdIoctlTransact(DeviceHandle, Btl, 0, &Req, DataBuffer, &Overlapped);
    ASSERT(ERROR_SUCCESS == Error);
    Error = ResetEvent(Overlapped.hEvent);
    ASSERT(ERROR_SUCCESS == Error);

    ASSERT(0 != Req.Hint);
    ASSERT(SpdIoctlTransactReadKind == Req.Kind);
    ASSERT(7 == Req.Op.Read.BlockAddress);
    ASSERT(MaxBlockCount == Req.Op.Read.BlockCount);
    ASSERT(1 == Req.Op.Read.ForceUnitAccess);
    ASSERT(0 == Req.Op.Read.Reserved);

    FillOrTest(DataBuffer, 512, 7, MaxBlockCount, SpdIoctlTransactReservedKind);

    memset(&Rsp, 0, sizeof Rsp);
    Rsp.Hint = Req.Hint;
    Rsp.Kind = Req.Kind;

    Error = SpdIoctlTransact(DeviceHandle, Btl, &Rsp, 0, DataBuffer, &Overlapped);
    ASSERT(ERROR_SUCCESS == Error);
    Error = ResetEvent(Overlapped.hEvent);
    ASSERT(ERROR_SUCCESS == Error);

    if (5 > MaxBlockCount)
    {
        memset(DataBuffer, 0, sizeof DataBuffer);
        Error = SpdIoctlTransact(DeviceHandle, Btl, 0, &Req, DataBuffer, &Overlapped);
        ASSERT(ERROR_SUCCESS == Error);
        Error = ResetEvent(Overlapped.hEvent);
        ASSERT(ERROR_SUCCESS == Error);

        ASSERT(0 != Req.Hint);
        ASSERT(SpdIoctlTransactReadKind == Req.Kind);
        ASSERT(7 + MaxBlockCount == Req.Op.Read.BlockAddress);
        ASSERT(5 - MaxBlockCount == Req.Op.Read.BlockCount);
        ASSERT(1 == Req.Op.Read.ForceUnitAccess);
        ASSERT(0 == Req.Op.Read.Reserved);

        FillOrTest(DataBuffer, 512, 7 + MaxBlockCount, 5 - MaxBlockCount, SpdIoctlTransactReservedKind);

        memset(&Rsp, 0, sizeof Rsp);
        Rsp.Hint = Req.Hint;
        Rsp.Kind = Req.Kind;

        Error = SpdIoctlTransact(DeviceHandle, Btl, &Rsp, 0, DataBuffer, &Overlapped);
        ASSERT(ERROR_SUCCESS == Error);
        Error = ResetEvent(Overlapped.hEvent);
        ASSERT(ERROR_SUCCESS == Error);
    }

    Error = SpdIoctlUnprovision(DeviceHandle, &StorageUnitParams.Guid);
    ASSERT(ERROR_SUCCESS == Error);

    Success = CloseHandle(DeviceHandle);
    ASSERT(Success);

    SpdOverlappedFini(&Overlapped);

    free(DataBuffer);

    WaitForSingleObject(Thread, INFINITE);
    GetExitCodeThread(Thread, &ExitCode);
    CloseHandle(Thread);

    ASSERT(ERROR_SUCCESS == ExitCode);
}

static void ioctl_transact_read_test(void)
{
    ioctl_transact_read_dotest(5);
}

static void ioctl_transact_read_chunked_test(void)
{
    ioctl_transact_read_dotest(3);
}

static unsigned __stdcall ioctl_transact_write_test_thread(void *Data)
{
    UINT32 Btl = (UINT32)(UINT_PTR)Data;
    HANDLE DeviceHandle;
    DWORD Error;
    CDB Cdb;
    UINT8 DataBuffer[5 * 512];
    UINT32 DataLength;
    UCHAR ScsiStatus;
    union
    {
        SENSE_DATA Data;
        UCHAR Buffer[32];
    } Sense;

    Error = SpdIoctlOpenDevice(L"" SPD_IOCTL_HARDWARE_ID, &DeviceHandle);
    if (ERROR_SUCCESS != Error)
        goto exit;

    SpdIoctlScsiInquiry(DeviceHandle, Btl, 0, 3000);

    memset(&Cdb, 0, sizeof Cdb);
    Cdb.WRITE16.OperationCode = SCSIOP_WRITE16;
    Cdb.WRITE16.LogicalBlock[7] = 7;
    Cdb.WRITE16.TransferLength[3] = 5;

    FillOrTest(DataBuffer, 512, 7, 5, SpdIoctlTransactReservedKind);

    DataLength = sizeof DataBuffer;
    Error = SpdIoctlScsiExecute(DeviceHandle, Btl, &Cdb, -1, DataBuffer, &DataLength,
        &ScsiStatus, Sense.Buffer);

    CloseHandle(DeviceHandle);

    if (ERROR_SUCCESS != Error)
        goto exit;

    if (ScsiStatus != SCSISTAT_GOOD ||
        sizeof DataBuffer != DataLength)
    {
        Error = -'ASRT';
        goto exit;
    }

    Error = ERROR_SUCCESS;

exit:
    tlib_printf("thread=%lu ", Error);

    return Error;
}

static void ioctl_transact_write_dotest(ULONG MaxBlockCount)
{
    SPD_IOCTL_STORAGE_UNIT_PARAMS StorageUnitParams;
    SPD_IOCTL_TRANSACT_REQ Req;
    SPD_IOCTL_TRANSACT_RSP Rsp;
    PVOID DataBuffer = 0;
    OVERLAPPED Overlapped;
    HANDLE DeviceHandle;
    UINT32 Btl;
    DWORD Error;
    BOOL Success;
    HANDLE Thread;
    DWORD ExitCode;

    DataBuffer = malloc(MaxBlockCount * 512);
    ASSERT(0 != DataBuffer);

    Error = SpdOverlappedInit(&Overlapped);
    ASSERT(ERROR_SUCCESS == Error);

    Error = SpdIoctlOpenDevice(L"" SPD_IOCTL_HARDWARE_ID, &DeviceHandle);
    ASSERT(ERROR_SUCCESS == Error);

    memset(&StorageUnitParams, 0, sizeof StorageUnitParams);
    memcpy(&StorageUnitParams.Guid, &TestGuid, sizeof TestGuid);
    StorageUnitParams.BlockCount = 16;
    StorageUnitParams.BlockLength = 512;
    StorageUnitParams.MaxTransferLength = MaxBlockCount * 512;
    Error = SpdIoctlProvision(DeviceHandle, &StorageUnitParams, &Btl);
    ASSERT(ERROR_SUCCESS == Error);
    ASSERT(0 == Btl);

    Error = SpdIoctlScsiInquiry(DeviceHandle, Btl, 0, 3000);
    ASSERT(ERROR_SUCCESS == Error);

    Thread = (HANDLE)_beginthreadex(0, 0, ioctl_transact_write_test_thread, (PVOID)(UINT_PTR)Btl, 0, 0);
    ASSERT(0 != Thread);

    memset(DataBuffer, 0, sizeof DataBuffer);
    Error = SpdIoctlTransact(DeviceHandle, Btl, 0, &Req, DataBuffer, &Overlapped);
    ASSERT(ERROR_SUCCESS == Error);
    Error = ResetEvent(Overlapped.hEvent);
    ASSERT(ERROR_SUCCESS == Error);

    ASSERT(0 != Req.Hint);
    ASSERT(SpdIoctlTransactWriteKind == Req.Kind);
    ASSERT(7 == Req.Op.Write.BlockAddress);
    ASSERT(MaxBlockCount == Req.Op.Write.BlockCount);
    ASSERT(1 == Req.Op.Write.ForceUnitAccess);
    ASSERT(0 == Req.Op.Write.Reserved);

    ASSERT(FillOrTest(DataBuffer, 512, 7, MaxBlockCount, SpdIoctlTransactWriteKind));

    memset(&Rsp, 0, sizeof Rsp);
    Rsp.Hint = Req.Hint;
    Rsp.Kind = Req.Kind;

    Error = SpdIoctlTransact(DeviceHandle, Btl, &Rsp, 0, DataBuffer, &Overlapped);
    ASSERT(ERROR_SUCCESS == Error);
    Error = ResetEvent(Overlapped.hEvent);
    ASSERT(ERROR_SUCCESS == Error);

    if (5 > MaxBlockCount)
    {
        memset(DataBuffer, 0, sizeof DataBuffer);
        Error = SpdIoctlTransact(DeviceHandle, Btl, 0, &Req, DataBuffer, &Overlapped);
        ASSERT(ERROR_SUCCESS == Error);
        Error = ResetEvent(Overlapped.hEvent);
        ASSERT(ERROR_SUCCESS == Error);

        ASSERT(0 != Req.Hint);
        ASSERT(SpdIoctlTransactWriteKind == Req.Kind);
        ASSERT(7 + MaxBlockCount == Req.Op.Write.BlockAddress);
        ASSERT(5 - MaxBlockCount == Req.Op.Write.BlockCount);
        ASSERT(1 == Req.Op.Write.ForceUnitAccess);
        ASSERT(0 == Req.Op.Write.Reserved);

        ASSERT(FillOrTest(DataBuffer, 512, 7 + MaxBlockCount, 5 - MaxBlockCount, SpdIoctlTransactWriteKind));

        memset(&Rsp, 0, sizeof Rsp);
        Rsp.Hint = Req.Hint;
        Rsp.Kind = Req.Kind;

        Error = SpdIoctlTransact(DeviceHandle, Btl, &Rsp, 0, DataBuffer, &Overlapped);
        ASSERT(ERROR_SUCCESS == Error);
        Error = ResetEvent(Overlapped.hEvent);
        ASSERT(ERROR_SUCCESS == Error);
    }

    Error = SpdIoctlUnprovision(DeviceHandle, &StorageUnitParams.Guid);
    ASSERT(ERROR_SUCCESS == Error);

    Success = CloseHandle(DeviceHandle);
    ASSERT(Success);

    SpdOverlappedFini(&Overlapped);

    free(DataBuffer);

    WaitForSingleObject(Thread, INFINITE);
    GetExitCodeThread(Thread, &ExitCode);
    CloseHandle(Thread);

    ASSERT(ERROR_SUCCESS == ExitCode);
}

static void ioctl_transact_write_test(void)
{
    ioctl_transact_write_dotest(5);
}

static void ioctl_transact_write_chunked_test(void)
{
    ioctl_transact_write_dotest(3);
}

static unsigned __stdcall ioctl_transact_flush_test_thread(void *Data)
{
    UINT32 Btl = (UINT32)(UINT_PTR)Data;
    HANDLE DeviceHandle;
    DWORD Error;
    CDB Cdb;
    UINT32 DataLength;
    UCHAR ScsiStatus;
    union
    {
        SENSE_DATA Data;
        UCHAR Buffer[32];
    } Sense;

    Error = SpdIoctlOpenDevice(L"" SPD_IOCTL_HARDWARE_ID, &DeviceHandle);
    if (ERROR_SUCCESS != Error)
        goto exit;

    SpdIoctlScsiInquiry(DeviceHandle, Btl, 0, 3000);

    memset(&Cdb, 0, sizeof Cdb);
    Cdb.SYNCHRONIZE_CACHE16.OperationCode = SCSIOP_SYNCHRONIZE_CACHE16;
    Cdb.SYNCHRONIZE_CACHE16.LogicalBlock[7] = 7;
    Cdb.SYNCHRONIZE_CACHE16.BlockCount[3] = 5;

    DataLength = 0;
    Error = SpdIoctlScsiExecute(DeviceHandle, Btl, &Cdb, 0, 0, &DataLength,
        &ScsiStatus, Sense.Buffer);

    CloseHandle(DeviceHandle);

    if (ERROR_SUCCESS != Error)
        goto exit;

    if (ScsiStatus != SCSISTAT_GOOD ||
        0 != DataLength)
    {
        Error = -'ASRT';
        goto exit;
    }

    Error = ERROR_SUCCESS;

exit:
    tlib_printf("thread=%lu ", Error);

    return Error;
}

static void ioctl_transact_flush_test(void)
{
    SPD_IOCTL_STORAGE_UNIT_PARAMS StorageUnitParams;
    SPD_IOCTL_TRANSACT_REQ Req;
    SPD_IOCTL_TRANSACT_RSP Rsp;
    PVOID DataBuffer = 0;
    OVERLAPPED Overlapped;
    HANDLE DeviceHandle;
    UINT32 Btl;
    DWORD Error;
    BOOL Success;
    HANDLE Thread;
    DWORD ExitCode;

    DataBuffer = malloc(5 * 512);
    ASSERT(0 != DataBuffer);

    Error = SpdOverlappedInit(&Overlapped);
    ASSERT(ERROR_SUCCESS == Error);

    Error = SpdIoctlOpenDevice(L"" SPD_IOCTL_HARDWARE_ID, &DeviceHandle);
    ASSERT(ERROR_SUCCESS == Error);

    memset(&StorageUnitParams, 0, sizeof StorageUnitParams);
    memcpy(&StorageUnitParams.Guid, &TestGuid, sizeof TestGuid);
    StorageUnitParams.BlockCount = 16;
    StorageUnitParams.BlockLength = 512;
    StorageUnitParams.CacheSupported = 1;
    StorageUnitParams.MaxTransferLength = 5 * 512;
    Error = SpdIoctlProvision(DeviceHandle, &StorageUnitParams, &Btl);
    ASSERT(ERROR_SUCCESS == Error);
    ASSERT(0 == Btl);

    Error = SpdIoctlScsiInquiry(DeviceHandle, Btl, 0, 3000);
    ASSERT(ERROR_SUCCESS == Error);

    Thread = (HANDLE)_beginthreadex(0, 0, ioctl_transact_flush_test_thread, (PVOID)(UINT_PTR)Btl, 0, 0);
    ASSERT(0 != Thread);

    memset(DataBuffer, 0, sizeof DataBuffer);
    Error = SpdIoctlTransact(DeviceHandle, Btl, 0, &Req, DataBuffer, &Overlapped);
    ASSERT(ERROR_SUCCESS == Error);
    Error = ResetEvent(Overlapped.hEvent);
    ASSERT(ERROR_SUCCESS == Error);

    ASSERT(0 != Req.Hint);
    ASSERT(SpdIoctlTransactFlushKind == Req.Kind);
    ASSERT(7 == Req.Op.Flush.BlockAddress);
    ASSERT(5 == Req.Op.Flush.BlockCount);

    memset(&Rsp, 0, sizeof Rsp);
    Rsp.Hint = Req.Hint;
    Rsp.Kind = Req.Kind;

    Error = SpdIoctlTransact(DeviceHandle, Btl, &Rsp, 0, DataBuffer, &Overlapped);
    ASSERT(ERROR_SUCCESS == Error);
    Error = ResetEvent(Overlapped.hEvent);
    ASSERT(ERROR_SUCCESS == Error);

    Error = SpdIoctlUnprovision(DeviceHandle, &StorageUnitParams.Guid);
    ASSERT(ERROR_SUCCESS == Error);

    Success = CloseHandle(DeviceHandle);
    ASSERT(Success);

    SpdOverlappedFini(&Overlapped);

    free(DataBuffer);

    WaitForSingleObject(Thread, INFINITE);
    GetExitCodeThread(Thread, &ExitCode);
    CloseHandle(Thread);

    ASSERT(ERROR_SUCCESS == ExitCode);
}

static unsigned __stdcall ioctl_transact_unmap_test_thread(void *Data)
{
    UINT32 Btl = (UINT32)(UINT_PTR)Data;
    HANDLE DeviceHandle;
    DWORD Error;
    CDB Cdb;
    union
    {
        UINT8 Buffer[FIELD_OFFSET(UNMAP_LIST_HEADER, Descriptors) + 3 * sizeof(UNMAP_BLOCK_DESCRIPTOR)];
        UNMAP_LIST_HEADER List;
    } UnmapBuffer;
    UINT32 DataLength;
    UCHAR ScsiStatus;
    union
    {
        SENSE_DATA Data;
        UCHAR Buffer[32];
    } Sense;

    Error = SpdIoctlOpenDevice(L"" SPD_IOCTL_HARDWARE_ID, &DeviceHandle);
    if (ERROR_SUCCESS != Error)
        goto exit;

    SpdIoctlScsiInquiry(DeviceHandle, Btl, 0, 3000);

    memset(&Cdb, 0, sizeof Cdb);
    Cdb.UNMAP.OperationCode = SCSIOP_UNMAP;
    Cdb.UNMAP.AllocationLength[1] = sizeof UnmapBuffer;

    memset(&UnmapBuffer, 0, sizeof UnmapBuffer);
    UnmapBuffer.List.DataLength[1] = sizeof(UNMAP_LIST_HEADER) - 2 + 3 * sizeof(UNMAP_BLOCK_DESCRIPTOR);
    UnmapBuffer.List.BlockDescrDataLength[1] = 3 * sizeof(UNMAP_BLOCK_DESCRIPTOR);
    UnmapBuffer.List.Descriptors[0].StartingLba[7] = 7;
    UnmapBuffer.List.Descriptors[0].LbaCount[3] = 1;
    UnmapBuffer.List.Descriptors[1].StartingLba[7] = 6;
    UnmapBuffer.List.Descriptors[1].LbaCount[3] = 2;
    UnmapBuffer.List.Descriptors[2].StartingLba[7] = 5;
    UnmapBuffer.List.Descriptors[2].LbaCount[3] = 3;

    DataLength = sizeof UnmapBuffer;
    Error = SpdIoctlScsiExecute(DeviceHandle, Btl, &Cdb, -1, &UnmapBuffer, &DataLength,
        &ScsiStatus, Sense.Buffer);

    CloseHandle(DeviceHandle);

    if (ERROR_SUCCESS != Error)
        goto exit;

    if (ScsiStatus != SCSISTAT_GOOD ||
        sizeof UnmapBuffer != DataLength)
    {
        Error = -'ASRT';
        goto exit;
    }

    Error = ERROR_SUCCESS;

exit:
    tlib_printf("thread=%lu ", Error);

    return Error;
}

static void ioctl_transact_unmap_test(void)
{
    SPD_IOCTL_STORAGE_UNIT_PARAMS StorageUnitParams;
    SPD_IOCTL_TRANSACT_REQ Req;
    SPD_IOCTL_TRANSACT_RSP Rsp;
    PVOID DataBuffer = 0;
    OVERLAPPED Overlapped;
    HANDLE DeviceHandle;
    UINT32 Btl;
    DWORD Error;
    BOOL Success;
    HANDLE Thread;
    DWORD ExitCode;

    DataBuffer = malloc(5 * 512);
    ASSERT(0 != DataBuffer);

    Error = SpdOverlappedInit(&Overlapped);
    ASSERT(ERROR_SUCCESS == Error);

    Error = SpdIoctlOpenDevice(L"" SPD_IOCTL_HARDWARE_ID, &DeviceHandle);
    ASSERT(ERROR_SUCCESS == Error);

    memset(&StorageUnitParams, 0, sizeof StorageUnitParams);
    memcpy(&StorageUnitParams.Guid, &TestGuid, sizeof TestGuid);
    StorageUnitParams.BlockCount = 16;
    StorageUnitParams.BlockLength = 512;
    StorageUnitParams.UnmapSupported = 1;
    StorageUnitParams.MaxTransferLength = 5 * 512;
    Error = SpdIoctlProvision(DeviceHandle, &StorageUnitParams, &Btl);
    ASSERT(ERROR_SUCCESS == Error);
    ASSERT(0 == Btl);

    Error = SpdIoctlScsiInquiry(DeviceHandle, Btl, 0, 3000);
    ASSERT(ERROR_SUCCESS == Error);

    Thread = (HANDLE)_beginthreadex(0, 0, ioctl_transact_unmap_test_thread, (PVOID)(UINT_PTR)Btl, 0, 0);
    ASSERT(0 != Thread);

    memset(DataBuffer, 0, sizeof DataBuffer);
    Error = SpdIoctlTransact(DeviceHandle, Btl, 0, &Req, DataBuffer, &Overlapped);
    ASSERT(ERROR_SUCCESS == Error);
    Error = ResetEvent(Overlapped.hEvent);
    ASSERT(ERROR_SUCCESS == Error);

    ASSERT(0 != Req.Hint);
    ASSERT(SpdIoctlTransactUnmapKind == Req.Kind);
    ASSERT(3 == Req.Op.Unmap.Count);

    ASSERT(7 == ((SPD_IOCTL_UNMAP_DESCRIPTOR *)DataBuffer)[0].BlockAddress);
    ASSERT(1 == ((SPD_IOCTL_UNMAP_DESCRIPTOR *)DataBuffer)[0].BlockCount);
    ASSERT(0 == ((SPD_IOCTL_UNMAP_DESCRIPTOR *)DataBuffer)[0].Reserved);
    ASSERT(6 == ((SPD_IOCTL_UNMAP_DESCRIPTOR *)DataBuffer)[1].BlockAddress);
    ASSERT(2 == ((SPD_IOCTL_UNMAP_DESCRIPTOR *)DataBuffer)[1].BlockCount);
    ASSERT(0 == ((SPD_IOCTL_UNMAP_DESCRIPTOR *)DataBuffer)[1].Reserved);
    ASSERT(5 == ((SPD_IOCTL_UNMAP_DESCRIPTOR *)DataBuffer)[2].BlockAddress);
    ASSERT(3 == ((SPD_IOCTL_UNMAP_DESCRIPTOR *)DataBuffer)[2].BlockCount);
    ASSERT(0 == ((SPD_IOCTL_UNMAP_DESCRIPTOR *)DataBuffer)[2].Reserved);

    memset(&Rsp, 0, sizeof Rsp);
    Rsp.Hint = Req.Hint;
    Rsp.Kind = Req.Kind;

    Error = SpdIoctlTransact(DeviceHandle, Btl, &Rsp, 0, DataBuffer, &Overlapped);
    ASSERT(ERROR_SUCCESS == Error);
    Error = ResetEvent(Overlapped.hEvent);
    ASSERT(ERROR_SUCCESS == Error);

    Error = SpdIoctlUnprovision(DeviceHandle, &StorageUnitParams.Guid);
    ASSERT(ERROR_SUCCESS == Error);

    Success = CloseHandle(DeviceHandle);
    ASSERT(Success);

    SpdOverlappedFini(&Overlapped);

    free(DataBuffer);

    WaitForSingleObject(Thread, INFINITE);
    GetExitCodeThread(Thread, &ExitCode);
    CloseHandle(Thread);

    ASSERT(ERROR_SUCCESS == ExitCode);
}

static unsigned __stdcall ioctl_transact_error_test_thread(void *Data)
{
    UINT32 Btl = (UINT32)(UINT_PTR)Data;
    HANDLE DeviceHandle;
    DWORD Error;
    CDB Cdb;
    UINT8 DataBuffer[5 * 512];
    UINT32 DataLength;
    UCHAR ScsiStatus;
    union
    {
        SENSE_DATA Data;
        UCHAR Buffer[32];
    } Sense;

    Error = SpdIoctlOpenDevice(L"" SPD_IOCTL_HARDWARE_ID, &DeviceHandle);
    if (ERROR_SUCCESS != Error)
        goto exit;

    SpdIoctlScsiInquiry(DeviceHandle, Btl, 0, 3000);

    memset(&Cdb, 0, sizeof Cdb);
    Cdb.READ16.OperationCode = SCSIOP_READ16;
    Cdb.READ16.LogicalBlock[7] = 7;
    Cdb.READ16.TransferLength[3] = 5;

    DataLength = sizeof DataBuffer;
    Error = SpdIoctlScsiExecute(DeviceHandle, Btl, &Cdb, +1, DataBuffer, &DataLength,
        &ScsiStatus, Sense.Buffer);

    CloseHandle(DeviceHandle);

    if (ERROR_SUCCESS != Error)
        goto exit;

    if (ScsiStatus != SCSISTAT_CHECK_CONDITION ||
        Sense.Data.SenseKey != SCSI_SENSE_MEDIUM_ERROR ||
        Sense.Data.AdditionalSenseCode != SCSI_ADSENSE_SEEK_ERROR ||
        Sense.Data.AdditionalSenseCodeQualifier != SCSI_SENSEQ_POSITIONING_ERROR_DETECTED_BY_READ_OF_MEDIUM ||
        Sense.Data.Information[3] != 11 ||
        Sense.Data.Valid != 1)
    {
        Error = -'ASRT';
        goto exit;
    }

    Error = ERROR_SUCCESS;

exit:
    tlib_printf("thread=%lu ", Error);

    return Error;
}

static void ioctl_transact_error_test(void)
{
    SPD_IOCTL_STORAGE_UNIT_PARAMS StorageUnitParams;
    SPD_IOCTL_TRANSACT_REQ Req;
    SPD_IOCTL_TRANSACT_RSP Rsp;
    PVOID DataBuffer = 0;
    OVERLAPPED Overlapped;
    HANDLE DeviceHandle;
    UINT32 Btl;
    DWORD Error;
    BOOL Success;
    HANDLE Thread;
    DWORD ExitCode;

    DataBuffer = malloc(5 * 512);
    ASSERT(0 != DataBuffer);

    Error = SpdOverlappedInit(&Overlapped);
    ASSERT(ERROR_SUCCESS == Error);

    Error = SpdIoctlOpenDevice(L"" SPD_IOCTL_HARDWARE_ID, &DeviceHandle);
    ASSERT(ERROR_SUCCESS == Error);

    memset(&StorageUnitParams, 0, sizeof StorageUnitParams);
    memcpy(&StorageUnitParams.Guid, &TestGuid, sizeof TestGuid);
    StorageUnitParams.BlockCount = 16;
    StorageUnitParams.BlockLength = 512;
    StorageUnitParams.MaxTransferLength = 5 * 512;
    Error = SpdIoctlProvision(DeviceHandle, &StorageUnitParams, &Btl);
    ASSERT(ERROR_SUCCESS == Error);
    ASSERT(0 == Btl);

    Error = SpdIoctlScsiInquiry(DeviceHandle, Btl, 0, 3000);
    ASSERT(ERROR_SUCCESS == Error);

    Thread = (HANDLE)_beginthreadex(0, 0, ioctl_transact_error_test_thread, (PVOID)(UINT_PTR)Btl, 0, 0);
    ASSERT(0 != Thread);

    Error = SpdIoctlTransact(DeviceHandle, Btl, 0, &Req, DataBuffer, &Overlapped);
    ASSERT(ERROR_SUCCESS == Error);
    Error = ResetEvent(Overlapped.hEvent);
    ASSERT(ERROR_SUCCESS == Error);

    ASSERT(0 != Req.Hint);
    ASSERT(SpdIoctlTransactReadKind == Req.Kind);
    ASSERT(7 == Req.Op.Read.BlockAddress);
    ASSERT(5 == Req.Op.Read.BlockCount);
    ASSERT(1 == Req.Op.Read.ForceUnitAccess);
    ASSERT(0 == Req.Op.Read.Reserved);

    memset(&Rsp, 0, sizeof Rsp);
    Rsp.Hint = Req.Hint;
    Rsp.Kind = Req.Kind;
    Rsp.Status.ScsiStatus = SCSISTAT_CHECK_CONDITION;
    Rsp.Status.SenseKey = SCSI_SENSE_MEDIUM_ERROR;
    Rsp.Status.ASC = SCSI_ADSENSE_SEEK_ERROR;
    Rsp.Status.ASCQ = SCSI_SENSEQ_POSITIONING_ERROR_DETECTED_BY_READ_OF_MEDIUM;
    Rsp.Status.Information = 11;
    Rsp.Status.InformationValid = 1;

    Error = SpdIoctlTransact(DeviceHandle, Btl, &Rsp, 0, DataBuffer, &Overlapped);
    ASSERT(ERROR_SUCCESS == Error);
    Error = ResetEvent(Overlapped.hEvent);
    ASSERT(ERROR_SUCCESS == Error);

    Error = SpdIoctlUnprovision(DeviceHandle, &StorageUnitParams.Guid);
    ASSERT(ERROR_SUCCESS == Error);

    Success = CloseHandle(DeviceHandle);
    ASSERT(Success);

    SpdOverlappedFini(&Overlapped);

    free(DataBuffer);

    WaitForSingleObject(Thread, INFINITE);
    GetExitCodeThread(Thread, &ExitCode);
    CloseHandle(Thread);

    ASSERT(ERROR_SUCCESS == ExitCode);
}

static unsigned __stdcall ioctl_transact_cancel_test_thread(void *Data)
{
    UINT32 Btl = (UINT32)(UINT_PTR)Data;
    HANDLE DeviceHandle;
    DWORD Error;
    CDB Cdb;
    UINT8 DataBuffer[5 * 512];
    UINT32 DataLength;
    UCHAR ScsiStatus;
    union
    {
        SENSE_DATA Data;
        UCHAR Buffer[32];
    } Sense;

    Error = SpdIoctlOpenDevice(L"" SPD_IOCTL_HARDWARE_ID, &DeviceHandle);
    if (ERROR_SUCCESS != Error)
        goto exit;

    SpdIoctlScsiInquiry(DeviceHandle, Btl, 0, 3000);

    memset(&Cdb, 0, sizeof Cdb);
    Cdb.READ16.OperationCode = SCSIOP_READ16;
    Cdb.READ16.LogicalBlock[7] = 7;
    Cdb.READ16.TransferLength[3] = 5;

    DataLength = sizeof DataBuffer;
    Error = SpdIoctlScsiExecute(DeviceHandle, Btl, &Cdb, +1, DataBuffer, &DataLength,
        &ScsiStatus, Sense.Buffer);

    CloseHandle(DeviceHandle);

exit:
    tlib_printf("thread=%lu ", Error);

    return Error;
}

static void ioctl_transact_cancel_test(void)
{
    SPD_IOCTL_STORAGE_UNIT_PARAMS StorageUnitParams;
    SPD_IOCTL_TRANSACT_REQ Req;
    PVOID DataBuffer = 0;
    OVERLAPPED Overlapped;
    HANDLE DeviceHandle;
    UINT32 Btl;
    DWORD Error;
    BOOL Success;
    HANDLE Thread;
    DWORD ExitCode;

    DataBuffer = malloc(5 * 512);
    ASSERT(0 != DataBuffer);

    Error = SpdOverlappedInit(&Overlapped);
    ASSERT(ERROR_SUCCESS == Error);

    Error = SpdIoctlOpenDevice(L"" SPD_IOCTL_HARDWARE_ID, &DeviceHandle);
    ASSERT(ERROR_SUCCESS == Error);

    memset(&StorageUnitParams, 0, sizeof StorageUnitParams);
    memcpy(&StorageUnitParams.Guid, &TestGuid, sizeof TestGuid);
    StorageUnitParams.BlockCount = 16;
    StorageUnitParams.BlockLength = 512;
    StorageUnitParams.MaxTransferLength = 5 * 512;
    Error = SpdIoctlProvision(DeviceHandle, &StorageUnitParams, &Btl);
    ASSERT(ERROR_SUCCESS == Error);
    ASSERT(0 == Btl);

    Error = SpdIoctlScsiInquiry(DeviceHandle, Btl, 0, 3000);
    ASSERT(ERROR_SUCCESS == Error);

    Thread = (HANDLE)_beginthreadex(0, 0, ioctl_transact_cancel_test_thread, (PVOID)(UINT_PTR)Btl, 0, 0);
    ASSERT(0 != Thread);

    Error = SpdIoctlTransact(DeviceHandle, Btl, 0, &Req, DataBuffer, &Overlapped);
    ASSERT(ERROR_SUCCESS == Error);
    Error = ResetEvent(Overlapped.hEvent);
    ASSERT(ERROR_SUCCESS == Error);

    ASSERT(0 != Req.Hint);
    ASSERT(SpdIoctlTransactReadKind == Req.Kind);
    ASSERT(7 == Req.Op.Read.BlockAddress);
    ASSERT(5 == Req.Op.Read.BlockCount);
    ASSERT(1 == Req.Op.Read.ForceUnitAccess);
    ASSERT(0 == Req.Op.Read.Reserved);

    Error = SpdIoctlUnprovision(DeviceHandle, &StorageUnitParams.Guid);
    ASSERT(ERROR_SUCCESS == Error);

    Success = CloseHandle(DeviceHandle);
    ASSERT(Success);

    SpdOverlappedFini(&Overlapped);

    free(DataBuffer);

    WaitForSingleObject(Thread, INFINITE);
    GetExitCodeThread(Thread, &ExitCode);
    CloseHandle(Thread);

    ASSERT(0 != ExitCode);
}

static void ioctl_process_death_test_DO_NOT_RUN_FROM_COMMAND_LINE(void)
{
    SPD_IOCTL_STORAGE_UNIT_PARAMS StorageUnitParams;
    HANDLE DeviceHandle;
    UINT32 Btl;
    DWORD Error;
    BOOL Success;

    Error = SpdIoctlOpenDevice(L"" SPD_IOCTL_HARDWARE_ID, &DeviceHandle);
    ASSERT(ERROR_SUCCESS == Error);

    memset(&StorageUnitParams, 0, sizeof StorageUnitParams);
    memcpy(&StorageUnitParams.Guid, &TestGuid, sizeof TestGuid);
    StorageUnitParams.BlockCount = 16;
    StorageUnitParams.BlockLength = 512;
    StorageUnitParams.MaxTransferLength = 512;
    Error = SpdIoctlProvision(DeviceHandle, &StorageUnitParams, &Btl);
    ASSERT(ERROR_SUCCESS == Error);

    /* do not unprovision! */

    Success = CloseHandle(DeviceHandle);
    ASSERT(Success);
}

static void ioctl_process_death_test(void)
{
    HANDLE Stdout, Stderr;
    WCHAR FileName[MAX_PATH];
    WCHAR CommandLine[MAX_PATH + 64];
    STARTUPINFOW StartupInfo;
    PROCESS_INFORMATION ProcessInfo;
    DWORD WaitResult;
    DWORD ExitCode;
    BOOL Success;

    Stdout = CreateFileW(L"NUL",
        GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
        0, OPEN_EXISTING, 0, 0);
    ASSERT(INVALID_HANDLE_VALUE != Stdout);
    Stderr = CreateFileW(L"NUL",
        GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
        0, OPEN_EXISTING, 0, 0);
    ASSERT(INVALID_HANDLE_VALUE != Stderr);

    GetModuleFileNameW(0, FileName, MAX_PATH);
    ASSERT(ERROR_SUCCESS == GetLastError());

    StringCbPrintfW(CommandLine, sizeof CommandLine,
        L"\"%s\" +ioctl_process_death_test_DO_NOT_RUN_FROM_COMMAND_LINE",
        FileName);

    memset(&StartupInfo, 0, sizeof StartupInfo);
    StartupInfo.cb = sizeof StartupInfo;
    StartupInfo.dwFlags = STARTF_USESTDHANDLES;
    StartupInfo.hStdOutput = Stdout;
    StartupInfo.hStdError = Stderr;
    Success = CreateProcessW(FileName, CommandLine, 0, 0, TRUE, 0, 0, 0, &StartupInfo, &ProcessInfo);
    ASSERT(Success);

    WaitResult = WaitForSingleObject(ProcessInfo.hProcess, 3000);
    ASSERT(WAIT_OBJECT_0 == WaitResult);

    ASSERT(GetExitCodeProcess(ProcessInfo.hProcess, &ExitCode));
    ASSERT(0 == ExitCode);

    CloseHandle(ProcessInfo.hThread);
    CloseHandle(ProcessInfo.hProcess);

    CloseHandle(Stdout);
    CloseHandle(Stderr);

    HANDLE DeviceHandle;
    UINT32 BtlBuf[256];
    UINT32 BtlBufSize;
    DWORD Error;

    Error = SpdIoctlOpenDevice(L"" SPD_IOCTL_HARDWARE_ID, &DeviceHandle);
    ASSERT(ERROR_SUCCESS == Error);

    BtlBufSize = sizeof(BtlBuf);
    Error = SpdIoctlGetList(DeviceHandle, BtlBuf, &BtlBufSize);
    ASSERT(ERROR_SUCCESS == Error);
    ASSERT(0 == BtlBufSize);

    Success = CloseHandle(DeviceHandle);
    ASSERT(Success);
}

static void ioctl_process_access_test_DO_NOT_RUN_FROM_COMMAND_LINE(void)
{
    HANDLE DeviceHandle;
    SPD_IOCTL_TRANSACT_REQ Req;
    PVOID DataBuffer = 0;
    OVERLAPPED Overlapped;
    DWORD Error;
    BOOL Success;

    DataBuffer = malloc(5 * 512);
    ASSERT(0 != DataBuffer);

    Error = SpdOverlappedInit(&Overlapped);
    ASSERT(ERROR_SUCCESS == Error);

    Error = SpdIoctlOpenDevice(L"" SPD_IOCTL_HARDWARE_ID, &DeviceHandle);
    ASSERT(ERROR_SUCCESS == Error);

    memset(DataBuffer, 0, sizeof DataBuffer);
    Error = SpdIoctlTransact(DeviceHandle, 0, 0, &Req, DataBuffer, &Overlapped);
    ASSERT(ERROR_ACCESS_DENIED == Error);
    Error = ResetEvent(Overlapped.hEvent);
    ASSERT(ERROR_SUCCESS == Error);

    Error = SpdIoctlUnprovision(DeviceHandle, &TestGuid);
    ASSERT(ERROR_ACCESS_DENIED == Error);

    Success = CloseHandle(DeviceHandle);
    ASSERT(Success);

    SpdOverlappedFini(&Overlapped);

    free(DataBuffer);
}

static void ioctl_process_access_test(void)
{
    SPD_IOCTL_STORAGE_UNIT_PARAMS StorageUnitParams;
    HANDLE DeviceHandle;
    UINT32 Btl;
    DWORD Error;
    HANDLE Stdout, Stderr;
    WCHAR FileName[MAX_PATH];
    WCHAR CommandLine[MAX_PATH + 64];
    STARTUPINFOW StartupInfo;
    PROCESS_INFORMATION ProcessInfo;
    DWORD WaitResult;
    DWORD ExitCode;
    BOOL Success;

    Error = SpdIoctlOpenDevice(L"" SPD_IOCTL_HARDWARE_ID, &DeviceHandle);
    ASSERT(ERROR_SUCCESS == Error);

    memset(&StorageUnitParams, 0, sizeof StorageUnitParams);
    memcpy(&StorageUnitParams.Guid, &TestGuid, sizeof TestGuid);
    StorageUnitParams.BlockCount = 16;
    StorageUnitParams.BlockLength = 512;
    StorageUnitParams.MaxTransferLength = 5 * 512;
    Error = SpdIoctlProvision(DeviceHandle, &StorageUnitParams, &Btl);
    ASSERT(ERROR_SUCCESS == Error);
    ASSERT(0 == Btl);

    Error = SpdIoctlScsiInquiry(DeviceHandle, Btl, 0, 3000);
    ASSERT(ERROR_SUCCESS == Error);

    Stdout = CreateFileW(L"NUL",
        GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
        0, OPEN_EXISTING, 0, 0);
    ASSERT(INVALID_HANDLE_VALUE != Stdout);
    Stderr = CreateFileW(L"NUL",
        GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
        0, OPEN_EXISTING, 0, 0);
    ASSERT(INVALID_HANDLE_VALUE != Stderr);

    GetModuleFileNameW(0, FileName, MAX_PATH);
    ASSERT(ERROR_SUCCESS == GetLastError());

    StringCbPrintfW(CommandLine, sizeof CommandLine,
        L"\"%s\" +ioctl_process_access_test_DO_NOT_RUN_FROM_COMMAND_LINE",
        FileName);

    memset(&StartupInfo, 0, sizeof StartupInfo);
    StartupInfo.cb = sizeof StartupInfo;
    StartupInfo.dwFlags = STARTF_USESTDHANDLES;
    StartupInfo.hStdOutput = Stdout;
    StartupInfo.hStdError = Stderr;
    Success = CreateProcessW(FileName, CommandLine, 0, 0, TRUE, 0, 0, 0, &StartupInfo, &ProcessInfo);
    ASSERT(Success);

    WaitResult = WaitForSingleObject(ProcessInfo.hProcess, 3000);
    ASSERT(WAIT_OBJECT_0 == WaitResult);

    ASSERT(GetExitCodeProcess(ProcessInfo.hProcess, &ExitCode));
    ASSERT(0 == ExitCode);

    CloseHandle(ProcessInfo.hThread);
    CloseHandle(ProcessInfo.hProcess);

    CloseHandle(Stdout);
    CloseHandle(Stderr);

    Error = SpdIoctlUnprovision(DeviceHandle, &StorageUnitParams.Guid);
    ASSERT(ERROR_SUCCESS == Error);

    Success = CloseHandle(DeviceHandle);
    ASSERT(Success);
}

static void ioctl_process_transact_test_DO_NOT_RUN_FROM_COMMAND_LINE(void)
{
    SPD_IOCTL_TRANSACT_REQ Req;
    SPD_IOCTL_TRANSACT_RSP Rsp;
    PVOID DataBuffer = 0;
    OVERLAPPED Overlapped;
    HANDLE DeviceHandle;
    UINT32 Btl;
    DWORD Error;
    BOOL Success;
    HANDLE Thread;
    DWORD ExitCode;

    DataBuffer = malloc(5 * 512);
    ASSERT(0 != DataBuffer);

    Error = SpdOverlappedInit(&Overlapped);
    ASSERT(ERROR_SUCCESS == Error);

    Error = SpdIoctlOpenDevice(L"" SPD_IOCTL_HARDWARE_ID, &DeviceHandle);
    ASSERT(ERROR_SUCCESS == Error);

    Btl = 0;

    Thread = (HANDLE)_beginthreadex(0, 0, ioctl_transact_read_test_thread, (PVOID)(UINT_PTR)Btl, 0, 0);
    ASSERT(0 != Thread);

    memset(DataBuffer, 0, sizeof DataBuffer);
    Error = SpdIoctlTransact(DeviceHandle, Btl, 0, &Req, DataBuffer, &Overlapped);
    ASSERT(ERROR_SUCCESS == Error);
    Error = ResetEvent(Overlapped.hEvent);
    ASSERT(ERROR_SUCCESS == Error);

    ASSERT(0 != Req.Hint);
    ASSERT(SpdIoctlTransactReadKind == Req.Kind);
    ASSERT(7 == Req.Op.Read.BlockAddress);
    ASSERT(5 == Req.Op.Read.BlockCount);
    ASSERT(1 == Req.Op.Read.ForceUnitAccess);
    ASSERT(0 == Req.Op.Read.Reserved);

    FillOrTest(DataBuffer, 512, 7, 5, SpdIoctlTransactReservedKind);

    memset(&Rsp, 0, sizeof Rsp);
    Rsp.Hint = Req.Hint;
    Rsp.Kind = Req.Kind;

    Error = SpdIoctlTransact(DeviceHandle, Btl, &Rsp, 0, DataBuffer, &Overlapped);
    ASSERT(ERROR_SUCCESS == Error);
    Error = ResetEvent(Overlapped.hEvent);
    ASSERT(ERROR_SUCCESS == Error);

    Success = CloseHandle(DeviceHandle);
    ASSERT(Success);

    SpdOverlappedFini(&Overlapped);

    free(DataBuffer);

    WaitForSingleObject(Thread, INFINITE);
    GetExitCodeThread(Thread, &ExitCode);
    CloseHandle(Thread);

    ASSERT(ERROR_SUCCESS == ExitCode);
}

static void ioctl_process_transact_test(void)
{
    SPD_IOCTL_STORAGE_UNIT_PARAMS StorageUnitParams;
    HANDLE DeviceHandle;
    UINT32 Btl;
    DWORD Error;
    HANDLE Stdout, Stderr;
    WCHAR FileName[MAX_PATH];
    WCHAR CommandLine[MAX_PATH + 64];
    STARTUPINFOW StartupInfo;
    PROCESS_INFORMATION ProcessInfo;
    DWORD WaitResult;
    DWORD ExitCode;
    BOOL Success;

    Error = SpdIoctlOpenDevice(L"" SPD_IOCTL_HARDWARE_ID, &DeviceHandle);
    ASSERT(ERROR_SUCCESS == Error);

    memset(&StorageUnitParams, 0, sizeof StorageUnitParams);
    memcpy(&StorageUnitParams.Guid, &TestGuid, sizeof TestGuid);
    StorageUnitParams.BlockCount = 16;
    StorageUnitParams.BlockLength = 512;
    StorageUnitParams.MaxTransferLength = 5 * 512;
    Error = SpdIoctlProvision(DeviceHandle, &StorageUnitParams, &Btl);
    ASSERT(ERROR_SUCCESS == Error);
    ASSERT(0 == Btl);

    Error = SpdIoctlScsiInquiry(DeviceHandle, Btl, 0, 3000);
    ASSERT(ERROR_SUCCESS == Error);

    Stdout = CreateFileW(L"NUL",
        GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
        0, OPEN_EXISTING, 0, 0);
    ASSERT(INVALID_HANDLE_VALUE != Stdout);
    Stderr = CreateFileW(L"NUL",
        GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
        0, OPEN_EXISTING, 0, 0);
    ASSERT(INVALID_HANDLE_VALUE != Stderr);

    GetModuleFileNameW(0, FileName, MAX_PATH);
    ASSERT(ERROR_SUCCESS == GetLastError());

    StringCbPrintfW(CommandLine, sizeof CommandLine,
        L"\"%s\" +ioctl_process_transact_test_DO_NOT_RUN_FROM_COMMAND_LINE",
        FileName);

    memset(&StartupInfo, 0, sizeof StartupInfo);
    StartupInfo.cb = sizeof StartupInfo;
    StartupInfo.dwFlags = STARTF_USESTDHANDLES;
    StartupInfo.hStdOutput = Stdout;
    StartupInfo.hStdError = Stderr;
    Success = CreateProcessW(FileName, CommandLine, 0, 0, TRUE, CREATE_SUSPENDED, 0, 0, &StartupInfo, &ProcessInfo);
    ASSERT(Success);

    Error = SpdIoctlSetTransactProcessId(DeviceHandle, Btl, ProcessInfo.dwProcessId);
    ASSERT(ERROR_SUCCESS == Error);

    Success = ResumeThread(ProcessInfo.hThread);
    ASSERT(Success);

    WaitResult = WaitForSingleObject(ProcessInfo.hProcess, 3000);
    ASSERT(WAIT_OBJECT_0 == WaitResult);

    ASSERT(GetExitCodeProcess(ProcessInfo.hProcess, &ExitCode));
    ASSERT(0 == ExitCode);

    CloseHandle(ProcessInfo.hThread);
    CloseHandle(ProcessInfo.hProcess);

    CloseHandle(Stdout);
    CloseHandle(Stderr);

    Error = SpdIoctlUnprovision(DeviceHandle, &StorageUnitParams.Guid);
    ASSERT(ERROR_SUCCESS == Error);

    Success = CloseHandle(DeviceHandle);
    ASSERT(Success);
}

void ioctl_tests(void)
{
    TEST(ioctl_provision_test);
    TEST(ioctl_provision_invalid_test);
    TEST(ioctl_provision_multi_test);
    TEST(ioctl_provision_toomany_test);
    TEST(ioctl_list_test);
    TEST(ioctl_transact_read_test);
    TEST(ioctl_transact_read_chunked_test);
    TEST(ioctl_transact_write_test);
    TEST(ioctl_transact_write_chunked_test);
    TEST(ioctl_transact_flush_test);
    TEST(ioctl_transact_unmap_test);
    TEST(ioctl_transact_error_test);
    TEST(ioctl_transact_cancel_test);
    TEST_OPT(ioctl_process_death_test_DO_NOT_RUN_FROM_COMMAND_LINE);
    TEST(ioctl_process_death_test);
    TEST_OPT(ioctl_process_access_test_DO_NOT_RUN_FROM_COMMAND_LINE);
    TEST(ioctl_process_access_test);
    TEST_OPT(ioctl_process_transact_test_DO_NOT_RUN_FROM_COMMAND_LINE);
    TEST(ioctl_process_transact_test);
}
