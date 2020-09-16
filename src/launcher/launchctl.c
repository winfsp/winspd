/**
 * @file launcher/launchctl.c
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

#define PROGNAME                        "launchctl"

#define info(format, ...)               \
    SpdPrintLog(GetStdHandle(STD_OUTPUT_HANDLE), format, __VA_ARGS__)
#define warn(format, ...)               \
    SpdPrintLog(GetStdHandle(STD_ERROR_HANDLE), format, __VA_ARGS__)
#define fail(ExitCode, format, ...)     \
    (SpdPrintLog(GetStdHandle(STD_ERROR_HANDLE), format, __VA_ARGS__), ExitProcess(ExitCode))

static void usage(void)
{
    fail(ERROR_INVALID_PARAMETER, L""
        "usage: %s COMMAND ARGS\n"
        "\n"
        "commands:\n"
        "    start               ClassName InstanceName Args...\n"
        "    stop                ClassName InstanceName\n"
        "    stopForced          ClassName InstanceName\n"
        "    info                ClassName InstanceName\n"
        "    list\n",
        L"" PROGNAME);
}

static int call_pipe_and_report(PWSTR PipeBuf, ULONG SendSize, ULONG RecvSize)
{
    DWORD LastError, BytesTransferred;

    LastError = SpdCallNamedPipeSecurelyEx(L"" SPD_LAUNCH_PIPE_NAME, PipeBuf, SendSize, PipeBuf, RecvSize,
        &BytesTransferred, NMPWAIT_USE_DEFAULT_WAIT, FALSE, SPD_LAUNCH_PIPE_OWNER);

    if (0 != LastError)
        warn(L"KO CallNamedPipe = %ld", LastError);
    else if (sizeof(WCHAR) > BytesTransferred)
        warn(L"KO launcher: empty buffer");
    else if (SpdLaunchCmdSuccess == PipeBuf[0])
    {
        if (sizeof(WCHAR) == BytesTransferred)
            info(L"OK");
        else
        {
            ULONG Count = 0;

            for (PWSTR P = PipeBuf, PipeBufEnd = P + BytesTransferred / sizeof(WCHAR);
                PipeBufEnd > P; P++)
                if (L'\0' == *P)
                {
                    /* print a newline every 2 nulls; this works for both list and info */
                    *P = 1 == Count % 2 ? L'\n' : L' ';
                    Count++;
                }

            if (BytesTransferred < RecvSize)
                PipeBuf[BytesTransferred / sizeof(WCHAR)] = L'\0';
            else
                PipeBuf[RecvSize / sizeof(WCHAR) - 1] = L'\0';

            info(L"OK\n%s", PipeBuf + 1);
        }
    }
    else if (SpdLaunchCmdFailure == PipeBuf[0])
    {
        if (BytesTransferred < RecvSize)
            PipeBuf[BytesTransferred / sizeof(WCHAR)] = L'\0';
        else
            PipeBuf[RecvSize / sizeof(WCHAR) - 1] = L'\0';

        info(L"KO launcher: error %s", PipeBuf + 1);
    }
    else 
        warn(L"KO launcher: corrupted buffer", 0);

    return LastError;
}

static int start(PWSTR PipeBuf, ULONG PipeBufSize,
    PWSTR ClassName, PWSTR InstanceName, DWORD Argc, PWSTR *Argv)
{
    PWSTR P;
    DWORD ClassNameSize, InstanceNameSize, ArgvSize;

    ClassNameSize = lstrlenW(ClassName) + 1;
    InstanceNameSize = lstrlenW(InstanceName) + 1;
    ArgvSize = 0;
    for (DWORD Argi = 0; Argc > Argi; Argi++)
        ArgvSize += lstrlenW(Argv[Argi]) + 1;

    if (PipeBufSize < (1 + ClassNameSize + InstanceNameSize + ArgvSize) * sizeof(WCHAR))
        return ERROR_INVALID_PARAMETER;

    P = PipeBuf;
    *P++ = SpdLaunchCmdStart;
    memcpy(P, ClassName, ClassNameSize * sizeof(WCHAR)); P += ClassNameSize;
    memcpy(P, InstanceName, InstanceNameSize * sizeof(WCHAR)); P += InstanceNameSize;
    for (DWORD Argi = 0; Argc > Argi; Argi++)
    {
        ArgvSize = lstrlenW(Argv[Argi]) + 1;
        memcpy(P, Argv[Argi], ArgvSize * sizeof(WCHAR)); P += ArgvSize;
    }

    return call_pipe_and_report(PipeBuf, (ULONG)((P - PipeBuf) * sizeof(WCHAR)), PipeBufSize);
}

static int stop(PWSTR PipeBuf, ULONG PipeBufSize,
    PWSTR ClassName, PWSTR InstanceName, BOOLEAN Forced)
{
    PWSTR P;
    DWORD ClassNameSize, InstanceNameSize;

    ClassNameSize = lstrlenW(ClassName) + 1;
    InstanceNameSize = lstrlenW(InstanceName) + 1;

    if (PipeBufSize < (1 + ClassNameSize + InstanceNameSize) * sizeof(WCHAR))
        return ERROR_INVALID_PARAMETER;

    P = PipeBuf;
    *P++ = Forced ? SpdLaunchCmdStopForced : SpdLaunchCmdStop;
    memcpy(P, ClassName, ClassNameSize * sizeof(WCHAR)); P += ClassNameSize;
    memcpy(P, InstanceName, InstanceNameSize * sizeof(WCHAR)); P += InstanceNameSize;

    return call_pipe_and_report(PipeBuf, (ULONG)((P - PipeBuf) * sizeof(WCHAR)), PipeBufSize);
}

static int getinfo(PWSTR PipeBuf, ULONG PipeBufSize,
    PWSTR ClassName, PWSTR InstanceName)
{
    PWSTR P;
    DWORD ClassNameSize, InstanceNameSize;

    ClassNameSize = lstrlenW(ClassName) + 1;
    InstanceNameSize = lstrlenW(InstanceName) + 1;

    if (PipeBufSize < (1 + ClassNameSize + InstanceNameSize) * sizeof(WCHAR))
        return ERROR_INVALID_PARAMETER;

    P = PipeBuf;
    *P++ = SpdLaunchCmdGetInfo;
    memcpy(P, ClassName, ClassNameSize * sizeof(WCHAR)); P += ClassNameSize;
    memcpy(P, InstanceName, InstanceNameSize * sizeof(WCHAR)); P += InstanceNameSize;

    return call_pipe_and_report(PipeBuf, (ULONG)((P - PipeBuf) * sizeof(WCHAR)), PipeBufSize);
}

static int list(PWSTR PipeBuf, ULONG PipeBufSize)
{
    PWSTR P;

    if (PipeBufSize < 1 * sizeof(WCHAR))
        return ERROR_INVALID_PARAMETER;

    P = PipeBuf;
    *P++ = SpdLaunchCmdGetNameList;

    return call_pipe_and_report(PipeBuf, (ULONG)((P - PipeBuf) * sizeof(WCHAR)), PipeBufSize);
}

static int quit(PWSTR PipeBuf, ULONG PipeBufSize)
{
    /* works only against DEBUG version of launcher */

    PWSTR P;

    if (PipeBufSize < 1 * sizeof(WCHAR))
        return ERROR_INVALID_PARAMETER;

    P = PipeBuf;
    *P++ = SpdLaunchCmdQuit;

    return call_pipe_and_report(PipeBuf, (ULONG)((P - PipeBuf) * sizeof(WCHAR)), PipeBufSize);
}

int wmain(int argc, wchar_t **argv)
{
    PWSTR PipeBuf = 0;

    /* allocate our PipeBuf early on; freed on process exit by the system */
    PipeBuf = MemAlloc(SPD_LAUNCH_PIPE_BUFFER_SIZE);
    if (0 == PipeBuf)
        return ERROR_NO_SYSTEM_RESOURCES;

    argc--;
    argv++;

    if (0 == argc)
        usage();

    if (0 == invariant_wcscmp(L"start", argv[0]))
    {
        if (3 > argc || argc > 12)
            usage();

        return start(PipeBuf, SPD_LAUNCH_PIPE_BUFFER_SIZE, argv[1], argv[2], argc - 3, argv + 3);
    }
    else
    if (0 == invariant_wcscmp(L"stop", argv[0]))
    {
        if (3 != argc)
            usage();

        return stop(PipeBuf, SPD_LAUNCH_PIPE_BUFFER_SIZE, argv[1], argv[2], FALSE);
    }
    else
    if (0 == invariant_wcscmp(L"stopForced", argv[0]))
    {
        if (3 != argc)
            usage();

        return stop(PipeBuf, SPD_LAUNCH_PIPE_BUFFER_SIZE, argv[1], argv[2], TRUE);
    }
    else
    if (0 == invariant_wcscmp(L"info", argv[0]))
    {
        if (3 != argc)
            usage();

        return getinfo(PipeBuf, SPD_LAUNCH_PIPE_BUFFER_SIZE, argv[1], argv[2]);
    }
    else
    if (0 == invariant_wcscmp(L"list", argv[0]))
    {
        if (1 != argc)
            usage();

        return list(PipeBuf, SPD_LAUNCH_PIPE_BUFFER_SIZE);
    }
    else
    if (0 == invariant_wcscmp(L"quit", argv[0]))
    {
        if (1 != argc)
            usage();

        /* works only against DEBUG version of launcher */
        return quit(PipeBuf, SPD_LAUNCH_PIPE_BUFFER_SIZE);
    }
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
