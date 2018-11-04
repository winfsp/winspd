/**
 * @file winspd/winspd.h
 * WinSpd User Mode API.
 *
 * In order to use the WinSpd API the user mode Storage Port Driver must include
 * &lt;winspd/winspd.h&gt; and link with the winspd_x64.dll (or winspd_x86.dll) library.
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

#ifndef WINSPD_WINSPD_H_INCLUDED
#define WINSPD_WINSPD_H_INCLUDED

#define _NTSCSI_USER_MODE_
#include <windows.h>
#include <scsi.h>

#include <winspd/ioctl.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @class SPD_STORAGE_UNIT_INTERFACE
 * Storage unit interface.
 */
typedef struct _SPD_STORAGE_UNIT SPD_STORAGE_UNIT;
typedef struct _SPD_STORAGE_UNIT_INTERFACE
{
    UCHAR (*Read)(SPD_STORAGE_UNIT *StorageUnit, UINT64 BlockAddress, PVOID Buffer, UINT32 Length,
        PSENSE_DATA SenseData);
    UCHAR (*Write)(SPD_STORAGE_UNIT *StorageUnit, UINT64 BlockAddress, PVOID Buffer, UINT32 Length,
        PSENSE_DATA SenseData);
    UCHAR (*Flush)(SPD_STORAGE_UNIT *StorageUnit, UINT64 BlockAddress, UINT32 Count,
        PSENSE_DATA SenseData);
    UCHAR (*Unmap)(SPD_STORAGE_UNIT *StorageUnit, PUNMAP_BLOCK_DESCRIPTOR Descriptors, UINT32 Count,
        PSENSE_DATA SenseData);

    /*
     * This ensures that this interface will always contain 16 function pointers.
     * Please update when changing the interface as it is important for future compatibility.
     */
    UCHAR (*Reserved[12])();
} SPD_STORAGE_UNIT_INTERFACE;
typedef struct _SPD_STORAGE_UNIT
{
    UINT16 Version;
    PVOID UserContext;
} SPD_STORAGE_UNIT;

#ifdef __cplusplus
}
#endif

#endif
