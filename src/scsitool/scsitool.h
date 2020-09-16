/**
 * @file scsitool/scsitool.h
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

#ifndef WINSPD_SCSITOOL_SCSITOOL_H_INCLUDED
#define WINSPD_SCSITOOL_SCSITOOL_H_INCLUDED

#include <shared/shared.h>

#define PROGNAME                        "scsitool"

#define info(format, ...)               \
    SpdPrintLog(GetStdHandle(STD_OUTPUT_HANDLE), format, __VA_ARGS__)
#define warn(format, ...)               \
    SpdPrintLog(GetStdHandle(STD_ERROR_HANDLE), format, __VA_ARGS__)
#define fail(ExitCode, format, ...)     \
    (SpdPrintLog(GetStdHandle(STD_ERROR_HANDLE), format, __VA_ARGS__), ExitProcess(ExitCode))

void ScsiLineText(HANDLE h, const char *format, void *buf, size_t len);
void ScsiTableText(HANDLE h, const char *format, void *buf, size_t len);

#endif
