/**
 * @file shared/ioctl.c
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
        Error = ERROR_INVALID_PARAMETER;
        goto exit;
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

    if (!DeviceIoControl(DeviceHandle, IOCTL_SCSI_PASS_THROUGH_DIRECT,
        &Scsi, sizeof Scsi,
        &Scsi, sizeof Scsi,
        &BytesTransferred, 0))
    {
        Error = GetLastError();
        goto exit;
    }

    if (0 != PDataLength)
        *PDataLength = Scsi.Base.DataTransferLength;

    if (0 != PScsiStatus)
        *PScsiStatus = Scsi.Base.ScsiStatus;

    if (SCSISTAT_GOOD != Scsi.Base.ScsiStatus && 0 != SenseInfoBuffer)
        memcpy(SenseInfoBuffer, Scsi.SenseInfoBuffer, Scsi.Base.SenseInfoLength);

    Error = ERROR_SUCCESS;

exit:
    return Error;
}

DWORD SpdIoctlProvision(HANDLE DeviceHandle,
    SPD_IOCTL_STORAGE_UNIT_PARAMS *StorageUnitParams, BOOLEAN Public)
{
    SPD_IOCTL_PROVISION_PARAMS Params;
    DWORD BytesTransferred;
    DWORD Error;

    memset(&Params, 0, sizeof Params);
    Params.Base.Size = sizeof Params;
    Params.Base.Code = Public ? SPD_IOCTL_PROVISION_PUBLIC : SPD_IOCTL_PROVISION;
    memcpy(&Params.StorageUnitParams, StorageUnitParams, sizeof Params.StorageUnitParams);

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

DWORD SpdIoctlUnprovision(HANDLE DeviceHandle,
    GUID *Guid, BOOLEAN Public)
{
    SPD_IOCTL_UNPROVISION_PARAMS Params;
    DWORD BytesTransferred;
    DWORD Error;

    memset(&Params, 0, sizeof Params);
    Params.Base.Size = sizeof Params;
    Params.Base.Code = Public ? SPD_IOCTL_UNPROVISION_PUBLIC : SPD_IOCTL_UNPROVISION;
    memcpy(&Params.Guid, Guid, sizeof Params.Guid);

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
    BOOLEAN ListAll, GUID *ListBuf, PUINT32 PListSize)
{
    SPD_IOCTL_LIST_PARAMS Params;
    DWORD BytesTransferred;
    DWORD Error;

    memset(&Params, 0, sizeof Params);
    Params.Base.Size = sizeof Params;
    Params.Base.Code = SPD_IOCTL_LIST;
    Params.ListAll = !!ListAll;

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
    PVOID ResponseBuf, UINT32 ResponseBufSize,
    PVOID RequestBuf, UINT32 *PRequestBufSize)
{
    return ERROR_INVALID_FUNCTION;
}

DWORD SpdIoctlMemAlignAlloc(UINT32 Size, UINT32 AlignmentMask, PVOID *PP)
{
    if (AlignmentMask + 1 < sizeof(PVOID))
        AlignmentMask = sizeof(PVOID) - 1;

    PVOID P = MemAlloc(Size + AlignmentMask);
    if (0 == P)
    {
        *PP = 0;
        return ERROR_NOT_ENOUGH_MEMORY;
    }

    *PP = (PVOID)(((UINT_PTR)(PUINT8)P + (UINT_PTR)AlignmentMask) & ~(UINT_PTR)AlignmentMask);
    ((PVOID *)*PP)[-1] = P;
    return ERROR_SUCCESS;
}

VOID SpdIoctlMemAlignFree(PVOID P)
{
    if (0 == P)
        return;

    MemFree(((PVOID *)P)[-1]);
}
