/**
 * @file ioctl-test.c
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

    for (ULONG I = 0, N = sizeof DataBuffer; N > I; I++)
        if (((PUINT8)DataBuffer)[I] != (UINT8)I)
        {
            Error = -'ASR2';
            goto exit;
        }

    Error = ERROR_SUCCESS;

exit:
    tlib_printf("thread=%lu ", Error);

    return Error;
}

static void ioctl_transact_read_test(void)
{
    SPD_IOCTL_STORAGE_UNIT_PARAMS StorageUnitParams;
    SPD_IOCTL_TRANSACT_REQ Req;
    SPD_IOCTL_TRANSACT_RSP Rsp;
    PVOID DataBuffer = 0;
    HANDLE DeviceHandle;
    UINT32 Btl;
    DWORD Error;
    BOOL Success;
    HANDLE Thread;
    DWORD ExitCode;

    DataBuffer = malloc(5 * 512);
    ASSERT(0 != DataBuffer);

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

    Thread = (HANDLE)_beginthreadex(0, 0, ioctl_transact_read_test_thread, (PVOID)(UINT_PTR)Btl, 0, 0);
    ASSERT(0 != Thread);

    memset(DataBuffer, 0, sizeof DataBuffer);
    Error = SpdIoctlTransact(DeviceHandle, Btl, 0, &Req, DataBuffer);
    ASSERT(ERROR_SUCCESS == Error);

    ASSERT(0 != Req.Hint);
    ASSERT(SpdIoctlTransactReadKind == Req.Kind);
    ASSERT(7 == Req.Op.Read.BlockAddress);
    ASSERT(5 == Req.Op.Read.BlockCount);
    ASSERT(0 == Req.Op.Read.ForceUnitAccess);
    ASSERT(0 == Req.Op.Read.Reserved);

    for (ULONG I = 0, N = Req.Op.Read.BlockCount * StorageUnitParams.BlockLength; N > I; I++)
        ((PUINT8)DataBuffer)[I] = (UINT8)I;

    memset(&Rsp, 0, sizeof Rsp);
    Rsp.Hint = Req.Hint;
    Rsp.Kind = Req.Kind;

    Error = SpdIoctlTransact(DeviceHandle, Btl, &Rsp, 0, DataBuffer);
    ASSERT(ERROR_SUCCESS == Error);

    Error = SpdIoctlUnprovision(DeviceHandle, &StorageUnitParams.Guid);
    ASSERT(ERROR_SUCCESS == Error);

    Success = CloseHandle(DeviceHandle);
    ASSERT(Success);

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
    HANDLE DeviceHandle;
    UINT32 Btl;
    DWORD Error;
    BOOL Success;
    HANDLE Thread;
    DWORD ExitCode;

    DataBuffer = malloc(5 * 512);
    ASSERT(0 != DataBuffer);

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

    Error = SpdIoctlTransact(DeviceHandle, Btl, 0, &Req, DataBuffer);
    ASSERT(ERROR_SUCCESS == Error);

    ASSERT(0 != Req.Hint);
    ASSERT(SpdIoctlTransactReadKind == Req.Kind);
    ASSERT(7 == Req.Op.Read.BlockAddress);
    ASSERT(5 == Req.Op.Read.BlockCount);
    ASSERT(0 == Req.Op.Read.ForceUnitAccess);
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

    Error = SpdIoctlTransact(DeviceHandle, Btl, &Rsp, 0, DataBuffer);
    ASSERT(ERROR_SUCCESS == Error);

    Error = SpdIoctlUnprovision(DeviceHandle, &StorageUnitParams.Guid);
    ASSERT(ERROR_SUCCESS == Error);

    Success = CloseHandle(DeviceHandle);
    ASSERT(Success);

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
    HANDLE DeviceHandle;
    UINT32 Btl;
    DWORD Error;
    BOOL Success;
    HANDLE Thread;
    DWORD ExitCode;

    DataBuffer = malloc(5 * 512);
    ASSERT(0 != DataBuffer);

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

    Error = SpdIoctlTransact(DeviceHandle, Btl, 0, &Req, DataBuffer);
    ASSERT(ERROR_SUCCESS == Error);

    ASSERT(0 != Req.Hint);
    ASSERT(SpdIoctlTransactReadKind == Req.Kind);
    ASSERT(7 == Req.Op.Read.BlockAddress);
    ASSERT(5 == Req.Op.Read.BlockCount);
    ASSERT(0 == Req.Op.Read.ForceUnitAccess);
    ASSERT(0 == Req.Op.Read.Reserved);

    Error = SpdIoctlUnprovision(DeviceHandle, &StorageUnitParams.Guid);
    ASSERT(ERROR_SUCCESS == Error);

    Success = CloseHandle(DeviceHandle);
    ASSERT(Success);

    free(DataBuffer);

    WaitForSingleObject(Thread, INFINITE);
    GetExitCodeThread(Thread, &ExitCode);
    CloseHandle(Thread);

    ASSERT(0 != ExitCode);
}

static void ioctl_provision_die_test_DO_NOT_RUN_FROM_COMMAND_LINE(void)
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
        L"\"%s\" +ioctl_provision_die_test_DO_NOT_RUN_FROM_COMMAND_LINE",
        FileName);

    memset(&StartupInfo, 0, sizeof StartupInfo);
    StartupInfo.cb = sizeof StartupInfo;
    StartupInfo.dwFlags = STARTF_USESTDHANDLES;
    StartupInfo.hStdOutput = Stdout;
    StartupInfo.hStdError = Stderr;
    Success = CreateProcessW(FileName, CommandLine, 0, 0, TRUE, 0, 0, 0, &StartupInfo, &ProcessInfo);
    ASSERT(Success);

    CloseHandle(Stdout);
    CloseHandle(Stderr);

    WaitResult = WaitForSingleObject(ProcessInfo.hProcess, 3000);
    ASSERT(WAIT_OBJECT_0 == WaitResult);

    ASSERT(GetExitCodeProcess(ProcessInfo.hProcess, &ExitCode));
    ASSERT(0 == ExitCode);

    CloseHandle(ProcessInfo.hThread);
    CloseHandle(ProcessInfo.hProcess);

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

void ioctl_tests(void)
{
    TEST(ioctl_provision_test);
    TEST(ioctl_provision_invalid_test);
    TEST(ioctl_provision_multi_test);
    TEST(ioctl_provision_toomany_test);
    TEST(ioctl_list_test);
    TEST(ioctl_transact_read_test);
    TEST(ioctl_transact_error_test);
    TEST(ioctl_transact_cancel_test);
    TEST_OPT(ioctl_provision_die_test_DO_NOT_RUN_FROM_COMMAND_LINE);
    TEST(ioctl_process_death_test);
}
