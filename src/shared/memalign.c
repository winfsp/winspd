/**
 * @file shared/memalign.c
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

DWORD SpdIoctlMemAlignAlloc(UINT32 Size, UINT32 AlignmentMask, PVOID *PP)
{
    if (AlignmentMask + 1 < sizeof(PVOID))
        AlignmentMask = sizeof(PVOID) - 1;

    PVOID P = MemAlloc(Size + AlignmentMask);
    if (0 == P)
    {
        *PP = 0;
        return ERROR_NOT_ENOUGH_MEMORY;
    }

    *PP = (PVOID)(((UINT_PTR)(PUINT8)P + (UINT_PTR)AlignmentMask) & ~(UINT_PTR)AlignmentMask);
    ((PVOID *)*PP)[-1] = P;
    return ERROR_SUCCESS;
}

VOID SpdIoctlMemAlignFree(PVOID P)
{
    if (0 == P)
        return;

    MemFree(((PVOID *)P)[-1]);
}
