/**
 * @file shared/ioctl.c
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
#pragma warning(push)
#pragma warning(disable:4091)           /* typedef ignored */
#include <ntddscsi.h>
#pragma warning(pop)
#include <setupapi.h>

#define GLOBAL                          L"\\\\?\\"
#define GLOBALROOT                      L"\\\\?\\GLOBALROOT"

static DWORD GetDevicePathByHardwareId(GUID *ClassGuid, PWSTR HardwareId,
    PWCHAR PathBuf, UINT32 PathBufSize)
{
    HDEVINFO DiHandle;
    SP_DEVINFO_DATA Info;
    WCHAR PropBuf[1024];
    BOOLEAN Found = FALSE;
    DWORD Error;

    DiHandle = SetupDiGetClassDevsW(ClassGuid, 0, 0,
        (0 == ClassGuid ? DIGCF_ALLCLASSES : 0) | DIGCF_PRESENT);
    if (INVALID_HANDLE_VALUE == DiHandle)
    {
        Error = GetLastError();
        goto exit;
    }

    Info.cbSize = sizeof Info;
    for (DWORD I = 0; !Found && SetupDiEnumDeviceInfo(DiHandle, I, &Info); I++)
    {
        if (!SetupDiGetDeviceRegistryPropertyW(DiHandle, &Info, SPDRP_HARDWAREID, 0,
            (PVOID)PropBuf, sizeof PropBuf - 2 * sizeof(WCHAR), 0))
            continue;
        PropBuf[sizeof PropBuf / 2 - 2] = L'\0';
        PropBuf[sizeof PropBuf / 2 - 1] = L'\0';

        for (PWSTR P = PropBuf; L'\0' != *P; P = P + lstrlenW(P) + 1)
            if (0 == invariant_wcsicmp(P, HardwareId))
            {
                Found = TRUE;
                break;
            }
    }
    if (!Found)
    {
        Error = GetLastError();
        goto exit;
    }

    if (!SetupDiGetDeviceRegistryPropertyW(DiHandle, &Info, SPDRP_PHYSICAL_DEVICE_OBJECT_NAME, 0,
        (PVOID)PathBuf, PathBufSize, 0))
    {
        Error = GetLastError();
        goto exit;
    }

    Error = ERROR_SUCCESS;

exit:
    if (ERROR_NO_MORE_ITEMS == Error)
        Error = ERROR_FILE_NOT_FOUND;

    if (INVALID_HANDLE_VALUE != DiHandle)
        SetupDiDestroyDeviceInfoList(DiHandle);

    return Error;
}

DWORD SpdIoctlGetDevicePath(GUID *ClassGuid, PWSTR DeviceName,
    PWCHAR PathBuf, UINT32 PathBufSize)
{
    BOOLEAN IsHwid = FALSE;
    PWSTR Prefix;
    DWORD PrefixSize, NameSize;
    DWORD Error;

    if (L'\\' == DeviceName[0])
    {
        if (L'\\' == DeviceName[1] &&
            (L'?' == DeviceName[2] || L'.' == DeviceName[2]) &&
            L'\\' == DeviceName[3])
        {
            Prefix = 0;
            PrefixSize = 0;
            NameSize = lstrlenW(DeviceName) * sizeof(WCHAR);
        }
        else
        {
            Prefix = GLOBALROOT;
            PrefixSize = sizeof GLOBALROOT - sizeof(WCHAR);
            NameSize = lstrlenW(DeviceName) * sizeof(WCHAR);
        }
    }
    else
    {
        IsHwid = FALSE;
        for (PWSTR P = DeviceName; L'\0' != *P; P++)
            if (L'\\' == *P || L'*' == *P)
            {
                IsHwid = TRUE;
                break;
            }

        if (IsHwid)
        {
            Prefix = GLOBALROOT;
            PrefixSize = sizeof GLOBALROOT - sizeof(WCHAR);
            NameSize = 0;
        }
        else
        {
            Prefix = GLOBAL;
            PrefixSize = sizeof GLOBAL - sizeof(WCHAR);
            NameSize = lstrlenW(DeviceName) * sizeof(WCHAR);
        }
    }

    if (PrefixSize + NameSize + sizeof(WCHAR) > PathBufSize)
    {
        Error = ERROR_MORE_DATA;
        goto exit;
    }

    memcpy(PathBuf, Prefix, PrefixSize);
    memcpy(PathBuf + PrefixSize / sizeof(WCHAR), DeviceName, NameSize);
    PathBuf[(PrefixSize + NameSize) / sizeof(WCHAR)] = L'\0';

    if (IsHwid)
    {
        Error = GetDevicePathByHardwareId(ClassGuid, DeviceName,
            PathBuf + PrefixSize / sizeof(WCHAR), PathBufSize - PrefixSize);
        if (ERROR_SUCCESS != Error)
        {
            if (ERROR_INSUFFICIENT_BUFFER == Error)
                Error = ERROR_MORE_DATA;
            goto exit;
        }
    }

    Error = ERROR_SUCCESS;

exit:
    return Error;
}

DWORD SpdIoctlOpenDevice(PWSTR DeviceName, PHANDLE PDeviceHandle)
{
    WCHAR PathBuf[1024];
    HANDLE DeviceHandle = INVALID_HANDLE_VALUE;
    DWORD Error;

    *PDeviceHandle = INVALID_HANDLE_VALUE;

    Error = SpdIoctlGetDevicePath(0, DeviceName, PathBuf, sizeof PathBuf);
    if (ERROR_SUCCESS != Error)
        goto exit;

    DeviceHandle = CreateFileW(PathBuf,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
        0, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, 0);
    if (INVALID_HANDLE_VALUE == DeviceHandle)
    {
        Error = GetLastError();
        goto exit;
    }

    *PDeviceHandle = DeviceHandle;
    Error = ERROR_SUCCESS;

exit:
    return Error;
}

DWORD SpdIoctlScsiExecute(HANDLE DeviceHandle,
    UINT32 Btl, PCDB Cdb, INT DataDirection, PVOID DataBuffer, PUINT32 PDataLength,
    PUCHAR PScsiStatus, UCHAR SenseInfoBuffer[32])
{
    typedef struct
    {
        SCSI_PASS_THROUGH_DIRECT Base;
        __declspec(align(16)) UCHAR SenseInfoBuffer[32];
    } SCSI_PASS_THROUGH_DIRECT_DATA;
    SCSI_PASS_THROUGH_DIRECT_DATA Scsi;
    OVERLAPPED Overlapped;
    DWORD BytesTransferred;
    DWORD Error;

    memset(&Scsi, 0, sizeof Scsi);
    Scsi.Base.Length = sizeof Scsi.Base;
    Scsi.Base.PathId = (Btl >> 16) & 0xff;
    Scsi.Base.TargetId = (Btl >> 8) & 0xff;
    Scsi.Base.Lun = Btl & 0xff;
    switch (Cdb->AsByte[0] & 0xE0)
    {
    case 0 << 5:
        Scsi.Base.CdbLength = 6;
        break;
    case 1 << 5:
    case 2 << 5:
        Scsi.Base.CdbLength = 10;
        break;
    case 4 << 5:
        Scsi.Base.CdbLength = 16;
        break;
    case 5 << 5:
        Scsi.Base.CdbLength = 12;
        break;
    default:
        return ERROR_INVALID_PARAMETER;
    }
    Scsi.Base.SenseInfoLength = sizeof(Scsi.SenseInfoBuffer);
    Scsi.Base.DataIn = 0 < DataDirection ?
        SCSI_IOCTL_DATA_IN :
        (0 > DataDirection ?
            SCSI_IOCTL_DATA_OUT :
            SCSI_IOCTL_DATA_UNSPECIFIED);
    Scsi.Base.DataTransferLength = 0 != PDataLength ? *PDataLength : 0;
    Scsi.Base.TimeOutValue = 10;
    Scsi.Base.DataBuffer = DataBuffer;
    Scsi.Base.SenseInfoOffset = FIELD_OFFSET(SCSI_PASS_THROUGH_DIRECT_DATA, SenseInfoBuffer);
    memcpy(Scsi.Base.Cdb, Cdb, sizeof Scsi.Base.Cdb);

    Error = SpdOverlappedInit(&Overlapped);
    if (ERROR_SUCCESS != Error)
        goto exit;

    Error = SpdOverlappedWaitResult(
        DeviceIoControl(DeviceHandle, IOCTL_SCSI_PASS_THROUGH_DIRECT,
            &Scsi, sizeof Scsi,
            &Scsi, sizeof Scsi,
            0, &Overlapped),
        DeviceHandle, &Overlapped, &BytesTransferred);
    if (ERROR_SUCCESS != Error)
        goto exit;

    if (0 != PDataLength)
        *PDataLength = Scsi.Base.DataTransferLength;

    if (0 != PScsiStatus)
        *PScsiStatus = Scsi.Base.ScsiStatus;

    if (SCSISTAT_GOOD != Scsi.Base.ScsiStatus && 0 != SenseInfoBuffer)
        memcpy(SenseInfoBuffer, Scsi.SenseInfoBuffer, Scsi.Base.SenseInfoLength);

    Error = ERROR_SUCCESS;

exit:
    SpdOverlappedFini(&Overlapped);

    return Error;
}

DWORD SpdIoctlScsiInquiry(HANDLE DeviceHandle,
    UINT32 Btl, PINQUIRYDATA InquiryData, ULONG Timeout)
{
    static const ULONG Delays[] =
    {
        100,
        300,
        300,
        300,
        1000,
    };
    CDB Cdb;
    __declspec(align(16)) UINT8 DataBuffer[VPD_MAX_BUFFER_SIZE];
    UINT32 DataLength;
    UCHAR ScsiStatus, SenseInfoBuffer[32];
    ULONG Delay;
    DWORD Error;

    for (ULONG I = 0, N = sizeof(Delays) / sizeof(Delays[0]);; I++)
    {
        memset(&Cdb, 0, sizeof Cdb);
        Cdb.CDB6INQUIRY3.OperationCode = SCSIOP_INQUIRY;
        Cdb.CDB6INQUIRY3.AllocationLength = sizeof DataBuffer;

        DataLength = sizeof DataBuffer;
        Error = SpdIoctlScsiExecute(DeviceHandle, Btl, &Cdb, +1, DataBuffer, &DataLength,
            &ScsiStatus, SenseInfoBuffer);
        if (ERROR_SUCCESS == Error && SCSISTAT_GOOD == ScsiStatus)
            break;
        if (0 == Timeout)
        {
            Error = ERROR_TIMEOUT;
            break;
        }

        Delay = N > I ? Delays[I] : Delays[N - 1];
        Delay = Delay < Timeout ? Delay : Timeout;
        Sleep(Delay);
        Timeout -= Delay;
    }

    if (0 != InquiryData)
    {
        memset(InquiryData, 0, sizeof *InquiryData);
        if (ERROR_SUCCESS == Error)
            memcpy(InquiryData, DataBuffer,
                sizeof *InquiryData < DataLength ? sizeof *InquiryData : DataLength);
    }

    return Error;
}

DWORD SpdIoctlProvision(HANDLE DeviceHandle,
    const SPD_IOCTL_STORAGE_UNIT_PARAMS *StorageUnitParams, PUINT32 PBtl)
{
    SPD_IOCTL_PROVISION_PARAMS Params;
    DWORD BytesTransferred;
    DWORD Error;

    *PBtl = (UINT32)-1;

    memset(&Params, 0, sizeof Params);
    Params.Base.Size = sizeof Params;
    Params.Base.Code = SPD_IOCTL_PROVISION;
    memcpy(&Params.Dir.Par.StorageUnitParams, StorageUnitParams,
        sizeof *StorageUnitParams);

    if (!DeviceIoControl(DeviceHandle, IOCTL_MINIPORT_PROCESS_SERVICE_IRP,
        &Params, sizeof Params,
        &Params, sizeof Params,
        &BytesTransferred, 0))
    {
        Error = GetLastError();
        goto exit;
    }

    *PBtl = Params.Dir.Ret.Btl;
    Error = ERROR_SUCCESS;

exit:
    return Error;
}

DWORD SpdIoctlUnprovision(HANDLE DeviceHandle,
    const GUID *Guid)
{
    SPD_IOCTL_UNPROVISION_PARAMS Params;
    DWORD BytesTransferred;
    DWORD Error;

    memset(&Params, 0, sizeof Params);
    Params.Base.Size = sizeof Params;
    Params.Base.Code = SPD_IOCTL_UNPROVISION;
    memcpy(&Params.Dir.Par.Guid, Guid, sizeof *Guid);

    if (!DeviceIoControl(DeviceHandle, IOCTL_MINIPORT_PROCESS_SERVICE_IRP,
        &Params, sizeof Params,
        0, 0,
        &BytesTransferred, 0))
    {
        Error = GetLastError();
        goto exit;
    }

    Error = ERROR_SUCCESS;

exit:
    return Error;
}

DWORD SpdIoctlGetList(HANDLE DeviceHandle,
    PUINT32 ListBuf, PUINT32 PListSize)
{
    SPD_IOCTL_LIST_PARAMS Params;
    DWORD BytesTransferred;
    DWORD Error;

    memset(&Params, 0, sizeof Params);
    Params.Base.Size = sizeof Params;
    Params.Base.Code = SPD_IOCTL_LIST;

    if (!DeviceIoControl(DeviceHandle, IOCTL_MINIPORT_PROCESS_SERVICE_IRP,
        &Params, sizeof Params,
        ListBuf, *PListSize,
        &BytesTransferred, 0))
    {
        Error = GetLastError();
        goto exit;
    }

    *PListSize = BytesTransferred;
    Error = ERROR_SUCCESS;

exit:
    return Error;
}

DWORD SpdIoctlTransact(HANDLE DeviceHandle,
    UINT32 Btl,
    SPD_IOCTL_TRANSACT_RSP *Rsp,
    SPD_IOCTL_TRANSACT_REQ *Req,
    PVOID DataBuffer,
    OVERLAPPED *Overlapped)
{
    SPD_IOCTL_TRANSACT_PARAMS Params;
    DWORD BytesTransferred;
    DWORD Error;

    memset(&Params, 0, sizeof Params);
    Params.Base.Size = sizeof Params;
    Params.Base.Code = SPD_IOCTL_TRANSACT;
    Params.Btl = Btl;
    Params.ReqValid = 0 != Req;
    Params.RspValid = 0 != Rsp;
    Params.DataBuffer = (UINT64)(UINT_PTR)DataBuffer;

    if (Params.RspValid)
        memcpy(&Params.Dir.Rsp, Rsp, sizeof *Rsp);

    /*
     * Our DeviceHandle is opened with FILE_FLAG_OVERLAPPED, but we call
     * DeviceIoControl with a NULL Overlapped parameter, which the MSDN
     * explicitly warns against. Why do we do this and why does it work?
     *
     * The reason that this works despite MSDN warnings is that we know
     * that our kernel driver handles IOCTL_MINIPORT_PROCESS_SERVICE_IRP
     * in a *synchronous* manner and never returns STATUS_PENDING for it.
     * This means that the OVERLAPPED structure special processing never
     * comes into play for our DeviceIoControl calls and it is safe to
     * call DeviceIoControl without an Overlapped structure.
     *
     * The next obvious question: what is the beneft of FILE_FLAG_OVERLAPPED
     * then? To answer this consider that Windows serializes all calls for
     * handles that are not open with FILE_FLAG_OVERLAPPED by acquiring an
     * internal file lock. This effectively means that without the
     * FILE_FLAG_OVERLAPPED flag, there can only be one DeviceIoControl active
     * at a time, which is not something we want. (Our kernel driver I/O queue
     * can very efficiently manage waiting threads and the above mentioned
     * file lock serialization limits our performance.) By specifying the
     * FILE_FLAG_OVERLAPPED flag we ensure that the unproductive serialization
     * does not happen.
     */
     // DeviceIoControl() can hang if lpOverlapped is NULL
    if (!DeviceIoControl(DeviceHandle, IOCTL_MINIPORT_PROCESS_SERVICE_IRP,
        &Params, sizeof Params,
        &Params, sizeof Params,
        &BytesTransferred, Overlapped))
    {
        Error = GetLastError();
        if (ERROR_IO_PENDING == Error)
        {
            if (!GetOverlappedResult(DeviceHandle, Overlapped,
                &BytesTransferred, TRUE))
            {
                Error = GetLastError();
                goto exit;
            }
        }
        else
        {
            goto exit;
        }
    }

    if (0 != Req)
        if (sizeof Params == BytesTransferred && Params.ReqValid)
            memcpy(Req, &Params.Dir.Req, sizeof *Req);
        else
            memset(Req, 0, sizeof *Req);

    Error = ERROR_SUCCESS;

exit:
    return Error;
}

DWORD SpdIoctlSetTransactProcessId(HANDLE DeviceHandle,
    UINT32 Btl,
    ULONG ProcessId)
{
    SPD_IOCTL_SET_TRANSACT_PID_PARAMS Params;
    DWORD BytesTransferred;
    DWORD Error;

    memset(&Params, 0, sizeof Params);
    Params.Base.Size = sizeof Params;
    Params.Base.Code = SPD_IOCTL_SET_TRANSACT_PID;
    Params.Btl = Btl;
    Params.ProcessId = ProcessId;

    if (!DeviceIoControl(DeviceHandle, IOCTL_MINIPORT_PROCESS_SERVICE_IRP,
        &Params, sizeof Params,
        0, 0,
        &BytesTransferred, 0))
    {
        Error = GetLastError();
        goto exit;
    }

    Error = ERROR_SUCCESS;

exit:
    return Error;
}
