/**
 * @file rawdisk.h
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

#ifndef RAWDISK_H_INCLUDED
#define RAWDISK_H_INCLUDED

#include <winspd/winspd.h>
#include <shared/minimal.h>
#include <shared/printlog.h>
#include <shared/strtoint.h>

typedef struct _RAWDISK RAWDISK;

DWORD RawDiskCreate(PWSTR RawDiskFile,
    UINT64 BlockCount, UINT32 BlockLength,
    PWSTR ProductId, PWSTR ProductRevision,
    BOOLEAN WriteProtected,
    BOOLEAN CacheSupported,
    BOOLEAN UnmapSupported,
    PWSTR PipeName,
    RAWDISK **PRawDisk);
VOID RawDiskDelete(RAWDISK *RawDisk);
SPD_STORAGE_UNIT *RawDiskStorageUnit(RAWDISK *RawDisk);

#endif
