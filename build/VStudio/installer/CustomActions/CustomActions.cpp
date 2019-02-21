/**
 * @file CustomActions.cpp
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

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <msiquery.h>
#include <wcautil.h>
#include <strutil.h>

UINT __stdcall ExecuteCommand(MSIHANDLE MsiHandle)
{
#if 0
    WCHAR MessageBuf[64];
    wsprintfW(MessageBuf, L"PID=%ld", GetCurrentProcessId());
    MessageBoxW(0, MessageBuf, L"" __FUNCTION__ " Break", MB_OK);
#endif

    HRESULT hr = S_OK;
    UINT err = ERROR_SUCCESS;
    PWSTR CommandLine = 0;
    STARTUPINFOW StartupInfo;
    PROCESS_INFORMATION ProcessInfo;
    DWORD ExitCode = 0;

    hr = WcaInitialize(MsiHandle, __FUNCTION__);
    ExitOnFailure(hr, "Failed to initialize");

    WcaGetProperty(L"" __FUNCTION__, &CommandLine);
    ExitOnFailure(hr, "Failed to get CommandLine");

    WcaLog(LOGMSG_STANDARD, "Initialized: \"%S\"", CommandLine);

    memset(&StartupInfo, 0, sizeof StartupInfo);
    StartupInfo.cb = sizeof StartupInfo;
    StartupInfo.dwFlags = STARTF_USESHOWWINDOW;
    StartupInfo.wShowWindow = SW_HIDE;

    if (!CreateProcessW(0, CommandLine, 0, 0, FALSE, 0, 0, 0, &StartupInfo, &ProcessInfo))
        ExitWithLastError(hr, "Failed to CreateProcessW");

    WaitForSingleObject(ProcessInfo.hProcess, INFINITE);
    GetExitCodeProcess(ProcessInfo.hProcess, &ExitCode);

    CloseHandle(ProcessInfo.hThread);
    CloseHandle(ProcessInfo.hProcess);

    WcaSetIntProperty(L"" __FUNCTION__, ExitCode);

LExit:
    ReleaseStr(CommandLine);

    err = SUCCEEDED(hr) ? ERROR_SUCCESS : ERROR_INSTALL_FAILURE;
    return WcaFinalize(err);
}

extern "C"
BOOL WINAPI DllMain(HINSTANCE Instance, DWORD Reason, PVOID Reserved)
{
    switch(Reason)
    {
    case DLL_PROCESS_ATTACH:
        WcaGlobalInitialize(Instance);
        break;
    case DLL_PROCESS_DETACH:
        WcaGlobalFinalize();
        break;
    }

    return TRUE;
}
