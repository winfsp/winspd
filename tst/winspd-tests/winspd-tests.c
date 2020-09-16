/**
 * @file winspd-tests.c
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
#include <signal.h>
#include <tlib/testsuite.h>

static void exiting(void);

static void abort_handler(int sig)
{
    DWORD Error = GetLastError();
    exiting();
    SetLastError(Error);
}

LONG WINAPI UnhandledExceptionHandler(struct _EXCEPTION_POINTERS *ExceptionInfo)
{
    exiting();
    return EXCEPTION_EXECUTE_HANDLER;
}

int main(int argc, char *argv[])
{
    TESTSUITE(ioctl_tests);
    TESTSUITE(scsi_tests);

    atexit(exiting);
    signal(SIGABRT, abort_handler);
    SetUnhandledExceptionFilter(UnhandledExceptionHandler);

    tlib_run_tests(argc, argv);

    return 0;
}

static void exiting(void)
{
    OutputDebugStringA("winspd-tests: exiting\n");
}
