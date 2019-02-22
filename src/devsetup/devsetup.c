/**
 * @file devsetup/devsetup.c
 *
 * @copyright 2018-2019 Bill Zissimopoulos
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
        "    remove hwid\n",
        PROGNAME);

    ExitProcess(ERROR_INVALID_PARAMETER);
}

#if !defined(MSITEST)

static DWORD EnumerateDevices(PWSTR HardwareId, int (*Fn)(HDEVINFO DiHandle, SP_DEVINFO_DATA *Info))
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
                break;
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

static DWORD AddDriverAndDevice(PWSTR HardwareId, PWSTR FileName)
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

static DWORD RemoveDevice(HDEVINFO DiHandle, SP_DEVINFO_DATA *Info)
{
    SP_REMOVEDEVICE_PARAMS RemoveParams;
    SP_DEVINSTALL_PARAMS_W InstallParams;
    BOOL RebootRequired = FALSE;
    DWORD Error;

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

    Error = RebootRequired ? ERROR_SUCCESS_REBOOT_REQUIRED : ERROR_SUCCESS;

exit:
    return Error;

lasterr:
    Error = GetLastError();
    goto exit;
}

static DWORD RemoveDeviceAndDriver(HDEVINFO DiHandle, SP_DEVINFO_DATA *Info)
{
    SP_DRVINFO_DATA_W DrvInfo;
    SP_DRVINFO_DETAIL_DATA_W DrvDetail;
    PWSTR BaseName = 0;
    BOOL RebootRequired = FALSE;
    DWORD Error;

    if (SetupDiBuildDriverInfoList(DiHandle, Info, SPDIT_COMPATDRIVER))
    {
        DrvInfo.cbSize = sizeof DrvInfo;
        for (DWORD I = 0; SetupDiEnumDriverInfoW(DiHandle, Info, SPDIT_COMPATDRIVER, I, &DrvInfo); I++)
        {
            DrvDetail.cbSize = sizeof DrvDetail;
            if (!SetupDiGetDriverInfoDetailW(DiHandle, Info, &DrvInfo, &DrvDetail, sizeof DrvDetail, 0)
                && ERROR_INSUFFICIENT_BUFFER != GetLastError())
                continue;

            BaseName = DrvDetail.InfFileName;
            for (PWSTR P = DrvDetail.InfFileName; L'\0' != *P; P++)
                if (L'\\' == *P)
                    BaseName = P + 1;

            break;
        }

        SetupDiDestroyDriverInfoList(DiHandle, Info, SPDIT_COMPATDRIVER);
    }

    Error = RemoveDevice(DiHandle, Info);
    if (ERROR_SUCCESS_REBOOT_REQUIRED == Error)
        RebootRequired = TRUE;
    else if (ERROR_SUCCESS != Error)
        goto exit;

    if (0 != BaseName && !SetupUninstallOEMInfW(BaseName, SUOI_FORCEDELETE, 0))
        /* ignore errors */;

    Error = RebootRequired ? ERROR_SUCCESS_REBOOT_REQUIRED : ERROR_SUCCESS;

exit:
    return Error;
}

static DWORD add(PWSTR HardwareId, PWSTR FileName)
{
    return AddDriverAndDevice(HardwareId, FileName);
}

static DWORD remove(PWSTR HardwareId)
{
    return EnumerateDevices(HardwareId, RemoveDeviceAndDriver);
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
