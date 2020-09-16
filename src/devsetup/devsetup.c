/**
 * @file devsetup/devsetup.c
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

#include <windows.h>
#include <cfgmgr32.h>
#include <newdev.h>
#include <setupapi.h>
#include <shared/minimal.h>

#define PROGNAME                        L"devsetup"

/*
 * Define MSITEST for MSI testing and no actual device setup.
 */
//#define MSITEST

static int MessageBoxFormat(DWORD Type, PWSTR Caption, PWSTR Format, ...)
{
    WCHAR Buf[1024];
    va_list ap;

    va_start(ap, Format);
    wvsprintfW(Buf, Format, ap);
    va_end(ap);

    return MessageBoxW(0, Buf, Caption, Type);
}

static void usage(void)
{
    MessageBoxFormat(MB_ICONEXCLAMATION | MB_OK, PROGNAME, L""
        "usage: %s COMMAND ARGS\n"
        "\n"
        "commands:\n"
        "    add hwid file.inf\n"
        "    remove hwid\n"
        "    removedev hwid\n",
        PROGNAME);

    ExitProcess(ERROR_INVALID_PARAMETER);
}

#if !defined(MSITEST)

static DWORD EnumerateDevices(PWSTR HardwareId, int (*Fn)(HDEVINFO DiHandle, PSP_DEVINFO_DATA Info))
{
    HDEVINFO DiHandle = INVALID_HANDLE_VALUE;
    SP_DEVINFO_DATA Info;
    WCHAR PropBuf[1024];
    BOOL Found = FALSE, RebootRequired = FALSE;
    DWORD Error;

    DiHandle = SetupDiGetClassDevsW(0, 0, 0, DIGCF_ALLCLASSES | DIGCF_PRESENT);
    if (INVALID_HANDLE_VALUE == DiHandle)
        goto lasterr;

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
                if (0 != Fn)
                {
                    Error = Fn(DiHandle, &Info);
                    if (0 != Error)
                    {
                        if (ERROR_SUCCESS_REBOOT_REQUIRED != Error)
                            goto exit;

                        RebootRequired = TRUE;
                    }
                }

                Found = TRUE;
            }
    }
    if (!Found)
        goto lasterr;

    Error = RebootRequired ? ERROR_SUCCESS_REBOOT_REQUIRED : ERROR_SUCCESS;

exit:
    if (ERROR_NO_MORE_ITEMS == Error)
        Error = ERROR_FILE_NOT_FOUND;

    if (INVALID_HANDLE_VALUE != DiHandle)
        SetupDiDestroyDeviceInfoList(DiHandle);

    return Error;

lasterr:
    Error = GetLastError();
    goto exit;
}

static DWORD GetInstalledDriver(HDEVINFO DiHandle, PSP_DEVINFO_DATA Info,
    PSP_DRVINFO_DATA_W DrvInfo, PSP_DRVINFO_DETAIL_DATA_W DrvDetail)
{
    SP_DEVINSTALL_PARAMS_W InstallParams;
    BOOL InfoList = FALSE, Found = FALSE;
    DWORD Error;

    InstallParams.cbSize = sizeof InstallParams;
    if (!SetupDiGetDeviceInstallParamsW(DiHandle, Info, &InstallParams))
        goto lasterr;

    InstallParams.FlagsEx |= DI_FLAGSEX_INSTALLEDDRIVER | DI_FLAGSEX_ALLOWEXCLUDEDDRVS;
    if (!SetupDiSetDeviceInstallParamsW(DiHandle, Info, &InstallParams))
        goto lasterr;

    if (!SetupDiBuildDriverInfoList(DiHandle, Info, SPDIT_CLASSDRIVER))
        goto lasterr;
    InfoList = TRUE;

    for (DWORD I = 0; SetupDiEnumDriverInfoW(DiHandle, Info, SPDIT_CLASSDRIVER, I, DrvInfo); I++)
    {
        if (!SetupDiGetDriverInfoDetailW(DiHandle, Info, DrvInfo, DrvDetail, DrvDetail->cbSize, 0)
            && ERROR_INSUFFICIENT_BUFFER != GetLastError())
            continue;

        Found = TRUE;
        break;
    }
    if (!Found)
        goto lasterr;

    Error = ERROR_SUCCESS;

exit:
    if (ERROR_NO_MORE_ITEMS == Error)
        Error = ERROR_FILE_NOT_FOUND;

    if (InfoList)
        SetupDiDestroyDriverInfoList(DiHandle, Info, SPDIT_COMPATDRIVER);

    return Error;

lasterr:
    Error = GetLastError();
    goto exit;
}

static DWORD EnumerateDriverFiles(HDEVINFO DiHandle, PSP_DEVINFO_DATA Info,
    PSP_DRVINFO_DATA_W DrvInfo, PSP_FILE_CALLBACK_W Fn, PVOID Data)
{
    HSPFILEQ QueueHandle = INVALID_HANDLE_VALUE;
    SP_DEVINSTALL_PARAMS_W InstallParams;
    DWORD Error, ScanResult;

    if (!SetupDiSetSelectedDriver(DiHandle, Info, DrvInfo))
        goto lasterr;

    QueueHandle = SetupOpenFileQueue();
    if (INVALID_HANDLE_VALUE == QueueHandle)
        goto lasterr;

    InstallParams.cbSize = sizeof InstallParams;
    if (!SetupDiGetDeviceInstallParamsW(DiHandle, Info, &InstallParams))
        goto lasterr;

    InstallParams.FileQueue = QueueHandle;
    InstallParams.Flags |= DI_NOVCP;
    if (!SetupDiSetDeviceInstallParamsW(DiHandle, Info, &InstallParams))
        goto lasterr;

    /* this will not actually install the files; it will queue them in QueueHandle */
    if (!SetupDiCallClassInstaller(DIF_INSTALLDEVICEFILES, DiHandle, Info))
        goto lasterr;

    /* play back the file queue */
    if (0 == SetupScanFileQueueW(QueueHandle, SPQ_SCAN_USE_CALLBACK, 0, Fn, Data, &ScanResult))
    {
        Error = 0 != ScanResult ? ScanResult : GetLastError();
        goto exit;
    }

    Error = ERROR_SUCCESS;

exit:
    if (INVALID_HANDLE_VALUE != QueueHandle)
        SetupCloseFileQueue(QueueHandle);

    return Error;

lasterr:
    Error = GetLastError();
    goto exit;
}

static DWORD AddDevice(PWSTR HardwareId, PWSTR FileName)
{
    WCHAR FileNameBuf[MAX_PATH];
    WCHAR HardwareIdBuf[LINE_LEN + 2];
    WCHAR ClassName[MAX_CLASS_NAME_LEN];
    GUID ClassGuid;
    HDEVINFO DiHandle = INVALID_HANDLE_VALUE;
    SP_DEVINFO_DATA Info;
    SP_DEVINSTALL_PARAMS_W InstallParams;
    BOOL RebootRequired0 = FALSE, RebootRequired1 = FALSE;
    int Error;

    if (0 == GetFullPathNameW(FileName, MAX_PATH, FileNameBuf, 0))
        goto lasterr;

    if (ERROR_SUCCESS != EnumerateDevices(HardwareId, 0))
    {
        if (!SetupDiGetINFClassW(FileNameBuf, &ClassGuid, ClassName, MAX_CLASS_NAME_LEN, 0))
            goto lasterr;

        if (INVALID_HANDLE_VALUE == (DiHandle = SetupDiCreateDeviceInfoList(&ClassGuid, 0)))
            goto lasterr;

        Info.cbSize = sizeof Info;
        if (!SetupDiCreateDeviceInfoW(DiHandle, ClassName, &ClassGuid, 0, 0, DICD_GENERATE_ID, &Info))
            goto lasterr;

        memset(HardwareIdBuf, 0, sizeof HardwareIdBuf);
        lstrcpynW(HardwareIdBuf, HardwareId, LINE_LEN + 1);
        if (!SetupDiSetDeviceRegistryPropertyW(DiHandle, &Info,
            SPDRP_HARDWAREID, (PVOID)HardwareIdBuf, (lstrlenW(HardwareIdBuf) + 2) * sizeof(WCHAR)))
            goto lasterr;

        if (!SetupDiCallClassInstaller(DIF_REGISTERDEVICE, DiHandle, &Info))
            goto lasterr;

        InstallParams.cbSize = sizeof InstallParams;
        if (!SetupDiGetDeviceInstallParamsW(DiHandle, &Info, &InstallParams))
            goto lasterr;
        RebootRequired0 = 0 != (InstallParams.Flags & (DI_NEEDREBOOT | DI_NEEDRESTART));
    }

    if (!UpdateDriverForPlugAndPlayDevicesW(0, HardwareId, FileNameBuf, 0, &RebootRequired1))
        goto lasterr;

    Error = RebootRequired0 || RebootRequired1 ? ERROR_SUCCESS_REBOOT_REQUIRED : ERROR_SUCCESS;

exit:
    if (INVALID_HANDLE_VALUE != DiHandle)
        SetupDiDestroyDeviceInfoList(DiHandle);

    return Error;

lasterr:
    Error = GetLastError();
    goto exit;
}

static DWORD RemoveDevice(HDEVINFO DiHandle, PSP_DEVINFO_DATA Info)
{
    BOOL RebootRequired = FALSE;
    DWORD Error;

#if 1
    /* this uninstalls the device and all its children devices */
    if (!DiUninstallDevice(0, DiHandle, Info, 0, &RebootRequired))
        goto lasterr;
#else
    SP_REMOVEDEVICE_PARAMS RemoveParams;
    SP_DEVINSTALL_PARAMS_W InstallParams;
    RemoveParams.ClassInstallHeader.cbSize = sizeof RemoveParams.ClassInstallHeader;
    RemoveParams.ClassInstallHeader.InstallFunction = DIF_REMOVE;
    RemoveParams.Scope = DI_REMOVEDEVICE_GLOBAL;
    RemoveParams.HwProfile = 0;
    if (!SetupDiSetClassInstallParamsW(DiHandle, Info,
        &RemoveParams.ClassInstallHeader, sizeof RemoveParams))
        goto lasterr;
    if (!SetupDiCallClassInstaller(DIF_REMOVE, DiHandle, Info))
        goto lasterr;
    InstallParams.cbSize = sizeof InstallParams;
    if (!SetupDiGetDeviceInstallParamsW(DiHandle, Info, &InstallParams))
        goto lasterr;
    RebootRequired = 0 != (InstallParams.Flags & (DI_NEEDREBOOT | DI_NEEDRESTART));
#endif

    Error = RebootRequired ? ERROR_SUCCESS_REBOOT_REQUIRED : ERROR_SUCCESS;

exit:
    return Error;

lasterr:
    Error = GetLastError();
    goto exit;
}

static DWORD RemoveFile(PWSTR FileName, PBOOL RebootRequired)
{
    WCHAR NewFileName[MAX_PATH + 40];
    GUID Guid;
    HANDLE Handle;
    DWORD Error;

    UuidCreate(&Guid);
    wsprintfW(NewFileName,
        L"%s.%08lx%04x%04x%02x%02x%02x%02x%02x%02x%02x%02x",
        FileName,
        Guid.Data1, Guid.Data2, Guid.Data3,
        Guid.Data4[0], Guid.Data4[1], Guid.Data4[2], Guid.Data4[3],
        Guid.Data4[4], Guid.Data4[5], Guid.Data4[6], Guid.Data4[7]);

    /* rename file to unique file name in same directory */
    if (!MoveFileExW(FileName, NewFileName, 0))
        goto lasterr;

    /* open new file name with DELETE_ON_CLOSE */
    Handle = CreateFileW(
        NewFileName,
        DELETE,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        0,
        OPEN_EXISTING,
        FILE_FLAG_DELETE_ON_CLOSE,
        0);
    if (INVALID_HANDLE_VALUE != Handle)
        CloseHandle(Handle);

    if (INVALID_HANDLE_VALUE == Handle)
        switch (GetLastError())
        {
        case ERROR_ACCESS_DENIED:
            /* if DELETE_ON_CLOSE failed with ERROR_ACCESS_DENIED it may be STATUS_CANNOT_DELETE */
            SetFileAttributesW(NewFileName, FILE_ATTRIBUTE_NORMAL);
            Handle = CreateFileW(
                NewFileName,
                DELETE,
                FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                0,
                OPEN_EXISTING,
                FILE_FLAG_DELETE_ON_CLOSE,
                0);
            if (INVALID_HANDLE_VALUE != Handle)
            {
                CloseHandle(Handle);
                break;
            }
            /* fall through */

        case ERROR_SHARING_VIOLATION:
            /* try one more time at reboot time */
            if (!MoveFileExW(NewFileName, 0, MOVEFILE_DELAY_UNTIL_REBOOT))
                goto lasterr;
            *RebootRequired = TRUE;
            break;

        default:
            goto lasterr;
        }

    Error = ERROR_SUCCESS;

exit:
    if (ERROR_FILE_NOT_FOUND == Error)
        Error = ERROR_SUCCESS;

    return Error;

lasterr:
    Error = GetLastError();
    goto exit;
}

static UINT CALLBACK RemoveDeviceAndDriverCallback(
    PVOID Context,
    UINT Notification,
    UINT_PTR Param1,
    UINT_PTR Param2)
{
    if (SPFILENOTIFY_QUEUESCAN != Notification)
        return ERROR_SUCCESS;

    PUINT8 *PBuffer = Context, Buffer = *PBuffer;
    PWSTR FileName = (PWSTR)Param1;
    ULONG FileNameSize = (lstrlenW(FileName) + 1) * sizeof(WCHAR);
    ULONG OldSize = 0 == Buffer ? sizeof(ULONG) : *(PULONG)Buffer;
    ULONG NewSize = OldSize + FileNameSize;

    Buffer = MemRealloc(Buffer, NewSize);
    if (0 == Buffer)
        return ERROR_NOT_ENOUGH_MEMORY;
    *PBuffer = Buffer;

    memcpy(Buffer + OldSize, FileName, FileNameSize);
    *(PULONG)Buffer = NewSize;

    return ERROR_SUCCESS;
}

static DWORD RemoveDeviceAndDriver(HDEVINFO DiHandle, PSP_DEVINFO_DATA Info)
{
    /* make a best effort to delete device and driver files; ignore errors */

    SP_DRVINFO_DATA_W DrvInfo;
    SP_DRVINFO_DETAIL_DATA_W DrvDetail;
    PUINT8 DriverFiles = 0;
    PWSTR BaseName = 0;
    BOOL RebootRequired = FALSE;
    DWORD Error;

    DrvInfo.cbSize = sizeof DrvInfo;
    DrvDetail.cbSize = sizeof DrvDetail;
    if (ERROR_SUCCESS == GetInstalledDriver(DiHandle, Info, &DrvInfo, &DrvDetail))
    {
        EnumerateDriverFiles(DiHandle, Info, &DrvInfo,
            RemoveDeviceAndDriverCallback, &DriverFiles);

        BaseName = DrvDetail.InfFileName;
        for (PWSTR P = DrvDetail.InfFileName; L'\0' != *P; P++)
            if (L'\\' == *P)
                BaseName = P + 1;
    }

    Error = RemoveDevice(DiHandle, Info);
    if (ERROR_SUCCESS_REBOOT_REQUIRED == Error)
        RebootRequired = TRUE;

    if (0 != DriverFiles)
        for (PWSTR P = (PWSTR)(DriverFiles + sizeof(ULONG));
            DriverFiles + *(PULONG)DriverFiles > (PUINT8)P; P += lstrlenW(P) + 1)
            RemoveFile(P, &RebootRequired);

    if (0 != BaseName)
        SetupUninstallOEMInfW(BaseName, SUOI_FORCEDELETE, 0);

    Error = RebootRequired ? ERROR_SUCCESS_REBOOT_REQUIRED : ERROR_SUCCESS;

    if (0 != DriverFiles)
        MemFree(DriverFiles);

    return Error;
}

static DWORD add(PWSTR HardwareId, PWSTR FileName)
{
    return AddDevice(HardwareId, FileName);
}

static DWORD remove(PWSTR HardwareId)
{
    return EnumerateDevices(HardwareId, RemoveDeviceAndDriver);
}

static DWORD removedev(PWSTR HardwareId)
{
    return EnumerateDevices(HardwareId, RemoveDevice);
}

#else

static DWORD add(PWSTR HardwareId, PWSTR FileName)
{
    MessageBoxFormat(MB_ICONINFORMATION | MB_OK, PROGNAME,
        L"add(HardwareId=%s, FileName=%s)", HardwareId, FileName);

    return ERROR_SUCCESS;
}

static DWORD remove(PWSTR HardwareId)
{
    MessageBoxFormat(MB_ICONINFORMATION | MB_OK, PROGNAME,
        L"remove(HardwareId=%s)", HardwareId);

    return ERROR_SUCCESS;
}

static DWORD removedev(PWSTR HardwareId)
{
    MessageBoxFormat(MB_ICONINFORMATION | MB_OK, PROGNAME,
        L"removedev(HardwareId=%s)", HardwareId);

    return ERROR_SUCCESS;
}

#endif

int wmain(int argc, wchar_t **argv)
{
    argc--;
    argv++;

    if (0 == argc)
        usage();

    if (0 == invariant_wcscmp(L"add", argv[0]))
    {
        if (3 != argc)
            usage();

        return add(argv[1], argv[2]);
    }
    else
    if (0 == invariant_wcscmp(L"remove", argv[0]))
    {
        if (2 != argc)
            usage();

        return remove(argv[1]);
    }
    else
    if (0 == invariant_wcscmp(L"removedev", argv[0]))
    {
        if (2 != argc)
            usage();

        return removedev(argv[1]);
    }
    else
        usage();

    return 0;
}

void WinMainCRTStartup(void)
{
    DWORD Argc;
    PWSTR *Argv;

    Argv = CommandLineToArgvW(GetCommandLineW(), &Argc);
    if (0 == Argv)
        ExitProcess(GetLastError());

    ExitProcess(wmain(Argc, Argv));
}
