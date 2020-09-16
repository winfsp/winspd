/**
 * @file shared/launch.c
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

DWORD SpdLaunchCallLauncherPipe(
    WCHAR Command, ULONG Argc, PWSTR *Argv, ULONG *Argl,
    PWSTR Buffer, PULONG PSize,
    PDWORD PLauncherError)
{
    PWSTR PipeBuf = 0, P;
    ULONG Length, BytesTransferred;
    DWORD Error, LauncherError;

    *PLauncherError = 0;

    PipeBuf = MemAlloc(SPD_LAUNCH_PIPE_BUFFER_SIZE);
    if (0 == PipeBuf)
    {
        Error = ERROR_NO_SYSTEM_RESOURCES;
        goto exit;
    }

    P = PipeBuf;
    *P++ = Command;
    for (ULONG I = 0; Argc > I; I++)
        if (0 != Argv[I])
        {
            Length = 0 == Argl || -1 == Argl[I] ? lstrlenW(Argv[I]) : Argl[I];
            if (SPD_LAUNCH_PIPE_BUFFER_SIZE < ((ULONG)(P - PipeBuf) + Length + 1) * sizeof(WCHAR))
            {
                Error = ERROR_INVALID_PARAMETER;
                goto exit;
            }
            memcpy(P, Argv[I], Length * sizeof(WCHAR)); P += Length; *P++ = L'\0';
        }

    Error = SpdCallNamedPipeSecurely(L"" SPD_LAUNCH_PIPE_NAME,
        PipeBuf, (ULONG)(P - PipeBuf) * sizeof(WCHAR), PipeBuf, SPD_LAUNCH_PIPE_BUFFER_SIZE,
        &BytesTransferred, NMPWAIT_USE_DEFAULT_WAIT, SPD_LAUNCH_PIPE_OWNER);
    if (ERROR_SUCCESS != Error)
        goto exit;

    Error = ERROR_SUCCESS;
    LauncherError = ERROR_BROKEN_PIPE; /* protocol error! */
    if (sizeof(WCHAR) <= BytesTransferred)
    {
        if (SpdLaunchCmdSuccess == PipeBuf[0])
        {
            LauncherError = 0;

            if (0 != PSize)
            {
                BytesTransferred -= sizeof(WCHAR);
                memcpy(Buffer, PipeBuf + 1, *PSize < BytesTransferred ? *PSize : BytesTransferred);
                *PSize = BytesTransferred;
            }
        }
        else if (SpdLaunchCmdFailure == PipeBuf[0])
        {
            LauncherError = 0;

            for (PWSTR P = PipeBuf + 1, EndP = PipeBuf + BytesTransferred / sizeof(WCHAR); EndP > P; P++)
            {
                if (L'0' > *P || *P > L'9')
                    break;
                LauncherError = 10 * LauncherError + (*P - L'0');
            }

            if (0 == LauncherError)
                LauncherError = ERROR_BROKEN_PIPE; /* protocol error! */
        }
    }

    *PLauncherError = LauncherError;

exit:
    if (ERROR_SUCCESS != Error && 0 != PSize)
        *PSize = 0;

    MemFree(PipeBuf);

    return Error;
}

DWORD SpdLaunchStart(
    PWSTR ClassName, PWSTR InstanceName, ULONG Argc, PWSTR *Argv0,
    PDWORD PLauncherError)
{
    PWSTR Argv[9 + 2];

    if (9 < Argc)
        return ERROR_INVALID_PARAMETER;

    Argv[0] = ClassName;
    Argv[1] = InstanceName;
    memcpy(Argv + 2, Argv0, Argc * sizeof(PWSTR));

    return SpdLaunchCallLauncherPipe(
        SpdLaunchCmdStart,
        Argc + 2, Argv, 0, 0, 0, PLauncherError);
}

DWORD SpdLaunchStop(
    PWSTR ClassName, PWSTR InstanceName,
    PDWORD PLauncherError)
{
    PWSTR Argv[2];

    Argv[0] = ClassName;
    Argv[1] = InstanceName;

    return SpdLaunchCallLauncherPipe(SpdLaunchCmdStop,
        2, Argv, 0, 0, 0, PLauncherError);
}

DWORD SpdLaunchGetInfo(
    PWSTR ClassName, PWSTR InstanceName,
    PWSTR Buffer, PULONG PSize,
    PDWORD PLauncherError)
{
    PWSTR Argv[2];

    Argv[0] = ClassName;
    Argv[1] = InstanceName;

    return SpdLaunchCallLauncherPipe(SpdLaunchCmdGetInfo,
        2, Argv, 0, Buffer, PSize, PLauncherError);
}

DWORD SpdLaunchGetNameList(
    PWSTR Buffer, PULONG PSize,
    PDWORD PLauncherError)
{
    return SpdLaunchCallLauncherPipe(SpdLaunchCmdGetNameList,
        0, 0, 0, Buffer, PSize, PLauncherError);
}
