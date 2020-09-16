/**
 * @file sys/tracing.c
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

#include <sys/driver.h>

VOID SpdHwInitializeTracing(PVOID Arg1, PVOID Arg2)
{
    SPD_ENTER(tracing,
        ASSERT(PASSIVE_LEVEL == KeGetCurrentIrql()));

    SPD_LEAVE(tracing,
        "Arg1=%p, Arg2=%p", "",
        Arg1, Arg2);
}

VOID SpdHwCleanupTracing(PVOID Arg1)
{
    SPD_ENTER(tracing,
        ASSERT(PASSIVE_LEVEL == KeGetCurrentIrql()));

    SPD_LEAVE(tracing,
        "Arg1=%p", "",
        Arg1);
}
