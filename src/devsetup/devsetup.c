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
#include <shared/minimal.h>

#define PROGNAME                        L"devsetup"

static void usage(void)
{
    WCHAR Buf[1024];

    wsprintfW(Buf, L""
        "usage: %s COMMAND ARGS\n"
        "\n"
        "commands:\n"
        "    add file.inf hwid\n"
        "    remove hwid\n",
        PROGNAME);

    MessageBoxW(0, Buf, PROGNAME, MB_ICONEXCLAMATION | MB_OK);

    ExitProcess(ERROR_INVALID_PARAMETER);
}

static int add(PWSTR FileName, PWSTR HardwareId)
{
    WCHAR Buf[1024];

    wsprintfW(Buf, L"add %s %s", FileName, HardwareId);

    MessageBoxW(0, Buf, PROGNAME, MB_OK);

    return 0;
}

static int remove(PWSTR HardwareId)
{
    WCHAR Buf[1024];

    wsprintfW(Buf, L"remove %s", HardwareId);

    MessageBoxW(0, Buf, PROGNAME, MB_OK);

    return 0;
}

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
