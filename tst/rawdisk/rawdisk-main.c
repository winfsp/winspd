/**
 * @file rawdisk-main.c
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

#include "rawdisk.h"

#define PROGNAME                        "rawdisk"

static void usage(void)
{
    static char usage[] = ""
        "usage: %s OPTIONS\n"
        "\n"
        "options:\n"
        "    -f RawDiskFile                      Storage unit data file\n"
        "    -c BlockCount                       Storage unit size in blocks\n"
        "    -l BlockLength                      Storage unit block length\n"
        "    -i ProductId                        1-16 chars\n"
        "    -r ProductRevision                  1-4 chars\n"
        "    -W 0|1                              Disable/enable writes (deflt: enable)\n"
        "    -C 0|1                              Disable/enable cache (deflt: enable)\n"
        "    -U 0|1                              Disable/enable unmap (deflt: enable)\n"
        "    -d -1                               Debug flags\n"
        "    -D DebugLogFile                     Debug log file; - for stderr\n"
        "    -p \\\\.\\pipe\\PipeName                Listen on pipe; omit to use driver\n"
        "";

    fail(ERROR_INVALID_PARAMETER, usage, PROGNAME);
}

static ULONG argtol(wchar_t **argp, ULONG deflt)
{
    if (0 == argp[0])
        usage();

    wchar_t *endp;
    ULONG ul = (ULONG)wcstoint(argp[0], 10, 1, &endp);
    return L'\0' != argp[0][0] && L'\0' == *endp ? ul : deflt;
}

static wchar_t *argtos(wchar_t **argp)
{
    if (0 == argp[0])
        usage();

    return argp[0];
}

static RAWDISK *RawDisk;

static BOOL WINAPI ConsoleCtrlHandler(DWORD CtrlType)
{
    SpdStorageUnitShutdownDispatcher(RawDiskStorageUnit(RawDisk));
    return TRUE;
}

int wmain(int argc, wchar_t **argv)
{
    wchar_t **argp;
    PWSTR RawDiskFile = 0;
    ULONG BlockCount = 1024 * 1024;
    ULONG BlockLength = 512;
    PWSTR ProductId = L"RawDisk";
    PWSTR ProductRevision = L"1.0";
    ULONG WriteAllowed = 1;
    ULONG CacheSupported = 1;
    ULONG UnmapSupported = 1;
    ULONG DebugFlags = 0;
    PWSTR DebugLogFile = 0;
    HANDLE DebugLogHandle = INVALID_HANDLE_VALUE;
    PWSTR PipeName = 0;
    DWORD Error;

    for (argp = argv + 1; 0 != argp[0]; argp++)
    {
        if (L'-' != argp[0][0])
            break;
        switch (argp[0][1])
        {
        case L'?':
            usage();
            break;
        case L'c':
            BlockCount = argtol(++argp, BlockCount);
            break;
        case L'C':
            CacheSupported = argtol(++argp, CacheSupported);
            break;
        case L'd':
            DebugFlags = argtol(++argp, DebugFlags);
            break;
        case L'D':
            DebugLogFile = argtos(++argp);
            break;
        case L'f':
            RawDiskFile = argtos(++argp);
            break;
        case L'i':
            ProductId = argtos(++argp);
            break;
        case L'l':
            BlockLength = argtol(++argp, BlockLength);
            break;
        case L'p':
            PipeName = argtos(++argp);
            break;
        case L'r':
            ProductRevision = argtos(++argp);
            break;
        case L'U':
            UnmapSupported = argtol(++argp, UnmapSupported);
            break;
        case L'W':
            WriteAllowed = argtol(++argp, WriteAllowed);
            break;
        default:
            usage();
            break;
        }
    }

    if (0 != argp[0] || 0 == RawDiskFile)
        usage();

    if (0 != DebugLogFile)
    {
        if (L'-' == DebugLogFile[0] && L'\0' == DebugLogFile[1])
            DebugLogHandle = GetStdHandle(STD_ERROR_HANDLE);
        else
            DebugLogHandle = CreateFileW(
                DebugLogFile,
                FILE_APPEND_DATA,
                FILE_SHARE_READ | FILE_SHARE_WRITE,
                0,
                OPEN_ALWAYS,
                FILE_ATTRIBUTE_NORMAL,
                0);
        if (INVALID_HANDLE_VALUE == DebugLogHandle)
            fail(GetLastError(), "error: cannot open debug log file");

        SpdDebugLogSetHandle(DebugLogHandle);
    }

    Error = RawDiskCreate(RawDiskFile,
        BlockCount, BlockLength,
        ProductId, ProductRevision,
        !WriteAllowed,
        !!CacheSupported,
        !!UnmapSupported,
        PipeName,
        &RawDisk);
    if (0 != Error)
        fail(Error, "error: cannot create RawDisk: error %lu", Error);
    Error = SpdStorageUnitStartDispatcher(RawDiskStorageUnit(RawDisk), 2);
    if (0 != Error)
        fail(Error, "error: cannot start RawDisk: error %lu", Error);

    SpdStorageUnitSetDebugLog(RawDiskStorageUnit(RawDisk), DebugFlags);

    warn("%s -f %S -c %lu -l %lu -i %S -r %S -W %u -C %u -U %u%s%S",
        PROGNAME,
        RawDiskFile,
        BlockCount, BlockLength, ProductId, ProductRevision,
        !!WriteAllowed,
        !!CacheSupported,
        !!UnmapSupported,
        0 != PipeName ? " -p " : "",
        0 != PipeName ? PipeName : L"");

    SetConsoleCtrlHandler(ConsoleCtrlHandler, TRUE);

    SpdStorageUnitWaitDispatcher(RawDiskStorageUnit(RawDisk));
    RawDiskDelete(RawDisk);
    RawDisk = 0;

    return 0;
}

#if 0
void wmainCRTStartup(void)
{
    DWORD Argc;
    PWSTR *Argv;

    Argv = CommandLineToArgvW(GetCommandLineW(), &Argc);
    if (0 == Argv)
        ExitProcess(GetLastError());

    ExitProcess(wmain(Argc, Argv));
}
#endif
