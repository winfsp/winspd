/**
 * @file scsitool/scsitool.c
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

#define _NTSCSI_USER_MODE_
#include <windows.h>
#include <devguid.h>
#include <ntddscsi.h>
#include <setupapi.h>
#include <scsi.h>
#include <shared/minimal.h>

#define PROGNAME                        "scsitool"

#define GLOBAL                          L"\\\\?\\"
#define GLOBALROOT                      L"\\\\?\\GLOBALROOT"

#define info(format, ...)               printlog(GetStdHandle(STD_OUTPUT_HANDLE), format, __VA_ARGS__)
#define warn(format, ...)               printlog(GetStdHandle(STD_ERROR_HANDLE), format, __VA_ARGS__)
#define fatal(ExitCode, format, ...)    (warn(format, __VA_ARGS__), ExitProcess(ExitCode))

static void vprintlog(HANDLE h, const char *format, va_list ap)
{
    char buf[1024];
        /* wvsprintf is only safe with a 1024 byte buffer */
    size_t len;
    DWORD BytesTransferred;

    wvsprintfA(buf, format, ap);
    buf[sizeof buf - 1] = '\0';

    len = lstrlenA(buf);
    buf[len++] = '\n';

    WriteFile(h, buf, (DWORD)len, &BytesTransferred, 0);
}

static void printlog(HANDLE h, const char *format, ...)
{
    va_list ap;

    va_start(ap, format);
    vprintlog(h, format, ap);
    va_end(ap);
}

static void usage(void)
{
    fatal(ERROR_INVALID_PARAMETER,
        "usage: %s COMMAND ARGS\n"
        "\n"
        "commands:\n"
        "    devpath device-name\n"
        "    inquiry device-name\n"
        "    report-luns device-name\n",
        PROGNAME);
}

static DWORD GetDevicePathByHardwareId(const GUID *ClassGuid, PWSTR HardwareId,
    PWCHAR PathBuf, DWORD PathBufSize)
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

static DWORD GetDevicePath(PWSTR DeviceName,
    PWCHAR PathBuf, DWORD PathBufSize)
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
        Error = GetDevicePathByHardwareId(&GUID_DEVCLASS_SCSIADAPTER, DeviceName,
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

static DWORD OpenDevice(PWSTR DeviceName, PHANDLE PDeviceHandle)
{
    WCHAR PathBuf[1024];
    HANDLE DeviceHandle = INVALID_HANDLE_VALUE;
    DWORD Error;

    *PDeviceHandle = INVALID_HANDLE_VALUE;

    Error = GetDevicePath(DeviceName, PathBuf, sizeof PathBuf);
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

static DWORD ScsiControl(HANDLE DeviceHandle,
    DWORD Ptl, PCDB Cdb, UCHAR DataDirection, PVOID DataBuffer, PDWORD PDataLength,
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
    Scsi.Base.PathId = (Ptl >> 16) & 0xff;
    Scsi.Base.TargetId = (Ptl >> 8) & 0xff;
    Scsi.Base.Lun = Ptl & 0xff;
    switch (Cdb->AsByte[0] & 0xE0)
    {
    case 0:
        Scsi.Base.CdbLength = 6;
        break;
    case 1:
    case 2:
        Scsi.Base.CdbLength = 10;
        break;
    case 4:
        Scsi.Base.CdbLength = 16;
        break;
    case 5:
        Scsi.Base.CdbLength = 12;
        break;
    default:
        Error = ERROR_INVALID_PARAMETER;
        goto exit;
    }
    Scsi.Base.SenseInfoLength = sizeof(Scsi.SenseInfoBuffer);
    Scsi.Base.DataIn = DataDirection;
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

static DWORD ScsiControlByName(PWSTR DeviceName,
    DWORD Ptl, PCDB Cdb, UCHAR DataDirection, PVOID DataBuffer, PDWORD PDataLength,
    PUCHAR PScsiStatus, UCHAR SenseInfoBuffer[32])
{
    HANDLE DeviceHandle = INVALID_HANDLE_VALUE;
    DWORD Error;

    Error = OpenDevice(DeviceName, &DeviceHandle);
    if (ERROR_SUCCESS != Error)
        goto exit;

    Error = ScsiControl(DeviceHandle, Ptl, Cdb,
        DataDirection, DataBuffer, PDataLength, PScsiStatus, SenseInfoBuffer);

exit:
    if (INVALID_HANDLE_VALUE != DeviceHandle)
        CloseHandle(DeviceHandle);

    return Error;
}

static int devpath(int argc, wchar_t **argv)
{
    if (2 != argc)
        usage();

    WCHAR PathBuf[1024];
    DWORD Error;

    Error = GetDevicePath(argv[1], PathBuf, sizeof PathBuf);
    if (ERROR_SUCCESS != Error)
        goto exit;

    info("%S", PathBuf);

exit:
    return Error;
}

static int inquiry(int argc, wchar_t **argv)
{
    if (2 != argc)
        usage();

    CDB Cdb;
    __declspec(align(16)) UCHAR DataBuffer[VPD_MAX_BUFFER_SIZE];
    DWORD DataLength = sizeof DataBuffer;
    UCHAR ScsiStatus;
    UCHAR SenseInfoBuffer[32];
    DWORD Error;

    memset(&Cdb, 0, sizeof Cdb);
    Cdb.CDB6INQUIRY3.OperationCode = SCSIOP_INQUIRY;
    Cdb.CDB6INQUIRY3.AllocationLength = VPD_MAX_BUFFER_SIZE;

    Error = ScsiControlByName(argv[1], 0, &Cdb, SCSI_IOCTL_DATA_IN,
        DataBuffer, &DataLength, &ScsiStatus, SenseInfoBuffer);

    return Error;
}

static int report_luns(int argc, wchar_t **argv)
{
    if (2 != argc)
        usage();

    return 0;
}

int wmain(int argc, wchar_t **argv)
{
    argc--;
    argv++;

    if (0 == argc)
        usage();

    if (0 == invariant_wcscmp(L"devpath", argv[0]))
        return devpath(argc, argv);
    else
    if (0 == invariant_wcscmp(L"inquiry", argv[0]))
        return inquiry(argc, argv);
    else
    if (0 == invariant_wcscmp(L"report-luns", argv[0]))
        return report_luns(argc, argv);
    else
        usage();

    return 0;
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
