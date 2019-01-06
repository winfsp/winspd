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

#define info(format, ...)               printlog(GetStdHandle(STD_OUTPUT_HANDLE), format, __VA_ARGS__)
#define warn(format, ...)               printlog(GetStdHandle(STD_ERROR_HANDLE), format, __VA_ARGS__)
#define fail(ExitCode, format, ...)     (warn(format, __VA_ARGS__), ExitProcess(ExitCode))

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
    static char usage[] = ""
        "usage: %s OPTIONS\n"
        "\n"
        "options:\n"
        "    -p \\\\.\\pipe\\PipeName\n"
        "    -c BlockCount\n"
        "    -l BlockLength\n"
        "    -i ProductId        [1-16 chars]\n"
        "    -r ProductRevision  [1-4 chars]\n"
        "    -f RawDiskFile\n";

    fail(ERROR_INVALID_PARAMETER, usage, PROGNAME);
}

static ULONG argtol(wchar_t **argp, ULONG deflt)
{
    if (0 == argp[0])
        usage();

    long long wcstoint(const wchar_t *p, int base, int is_signed, const wchar_t **endp);
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

static HANDLE MainEvent;

static BOOL WINAPI ConsoleCtrlHandler(DWORD CtrlType)
{
    SetEvent(MainEvent);
    return TRUE;
}

int wmain(int argc, wchar_t **argv)
{
    wchar_t **argp;
    PWSTR PipeName = 0;
    ULONG BlockCount = 1024 * 1024;
    ULONG BlockLength = 4096;
    PWSTR ProductId = L"RawDisk";
    PWSTR ProductRevision = L"1.0";
    PWSTR RawDiskFile = 0;
    RAWDISK *RawDisk = 0;
    ULONG DebugFlags = 0;
    PWSTR DebugLogFile = 0;
    HANDLE DebugLogHandle = INVALID_HANDLE_VALUE;
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
        case L'p':
            PipeName = argtos(++argp);
            break;
        case L'c':
            BlockCount = argtol(++argp, BlockCount);
            break;
        case L'l':
            BlockLength = argtol(++argp, BlockLength);
            break;
        case L'i':
            ProductId = argtos(++argp);
            break;
        case L'r':
            ProductRevision = argtos(++argp);
            break;
        case L'f':
            RawDiskFile = argtos(++argp);
            break;
        case L'd':
            DebugFlags = argtol(++argp, DebugFlags);
            break;
        case L'D':
            DebugLogFile = argtos(++argp);
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

    MainEvent = CreateEvent(0, TRUE, FALSE, 0);
    if (0 == MainEvent)
        fail(GetLastError(), "error: cannot create MainEvent: error %lu", GetLastError());

    Error = RawDiskCreate(RawDiskFile, BlockCount, BlockLength, ProductId, ProductRevision, PipeName,
        &RawDisk);
    if (0 != Error)
        fail(Error, "error: cannot create RawDisk: error %lu", Error);
    Error = SpdStorageUnitStartDispatcher(RawDiskStorageUnit(RawDisk), 2);
    if (0 != Error)
        fail(Error, "error: cannot start RawDisk: error %lu", Error);

    SpdStorageUnitSetDebugLog(RawDiskStorageUnit(RawDisk), DebugFlags);

    warn("%s%s%S -c %lu -l %lu -i %S -r %S -f %S",
        PROGNAME,
        0 != PipeName ? " -p " : "",
        0 != PipeName ? PipeName : L"",
        BlockCount, BlockLength, ProductId, ProductRevision, RawDiskFile);

    SetConsoleCtrlHandler(ConsoleCtrlHandler, TRUE);
    if (WAIT_OBJECT_0 != WaitForSingleObject(MainEvent, INFINITE))
        fail(GetLastError(), "error: cannot wait on MainEvent: error %lu", GetLastError());

    SpdStorageUnitStopDispatcher(RawDiskStorageUnit(RawDisk));
    RawDiskDelete(RawDisk);

    /* the OS will handle this! */
    // CloseHandle(MainEvent);
    // MainEvent = 0;

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
