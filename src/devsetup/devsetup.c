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

#define PROGNAME                        "devsetup"

static void usage(void)
{
    WCHAR Buf[1024];

    wsprintfW(Buf, L""
        "usage: %s COMMAND ARGS\n"
        "\n"
        "commands:\n"
        "    install             file.inf hwid\n"
        "    uninstall           hwid\n",
        PROGNAME);

    MessageBoxW(0, Buf, L"" PROGNAME, MB_OK);

    ExitProcess(ERROR_INVALID_PARAMETER);
}

int wmain(int argc, wchar_t **argv)
{
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
