/**
 * @file rawdisk.c
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

typedef struct _RAWDISK
{
    SPD_STORAGE_UNIT *StorageUnit;
    UINT64 BlockCount;
    UINT32 BlockLength;
    HANDLE Handle;
    HANDLE Mapping;
    PVOID Pointer;
} RAWDISK;

UCHAR Read(SPD_STORAGE_UNIT *StorageUnit,
    UINT64 BlockAddress, PVOID Buffer, UINT32 Length, BOOLEAN Flush,
    PSENSE_DATA SenseData)
{
    return 0;
}

UCHAR Write(SPD_STORAGE_UNIT *StorageUnit,
    UINT64 BlockAddress, PVOID Buffer, UINT32 Length, BOOLEAN Flush,
    PSENSE_DATA SenseData)
{
    return 0;
}

UCHAR Flush(SPD_STORAGE_UNIT *StorageUnit,
    UINT64 BlockAddress, UINT32 Count,
    PSENSE_DATA SenseData)
{
    return 0;
}

UCHAR Unmap(SPD_STORAGE_UNIT *StorageUnit,
    UINT64 BlockAddresses[], UINT32 Lengths[], UINT32 Count,
    PSENSE_DATA SenseData)
{
    return 0;
}

static SPD_STORAGE_UNIT_INTERFACE RawDiskInterface =
{
    Read,
    Write,
    Flush,
    Unmap,
};

DWORD RawDiskCreate(PWSTR RawDiskFile,
    UINT64 BlockCount, UINT32 BlockLength, PWSTR ProductId, PWSTR ProductRevision,
    RAWDISK **PRawDisk)
{
    RAWDISK *RawDisk = 0;
    HANDLE Handle = INVALID_HANDLE_VALUE;
    HANDLE Mapping = 0;
    PVOID Pointer = 0;
    LARGE_INTEGER FileSize;
    SPD_IOCTL_STORAGE_UNIT_PARAMS StorageUnitParams;
    SPD_STORAGE_UNIT *StorageUnit = 0;
    DWORD Error;

    *PRawDisk = 0;

    memset(&StorageUnitParams, 0, sizeof StorageUnitParams);
    UuidCreate(&StorageUnitParams.Guid);
    StorageUnitParams.BlockCount = BlockCount;
    StorageUnitParams.BlockLength = BlockLength;
    if (0 == WideCharToMultiByte(CP_UTF8, 0,
        ProductId, lstrlenW(ProductId),
        StorageUnitParams.ProductId, sizeof StorageUnitParams.ProductId,
        0, 0))
    {
        Error = ERROR_INVALID_PARAMETER;
        goto exit;
    }
    if (0 == WideCharToMultiByte(CP_UTF8, 0,
        ProductRevision, lstrlenW(ProductRevision),
        StorageUnitParams.ProductRevisionLevel, sizeof StorageUnitParams.ProductRevisionLevel,
        0, 0))
    {
        Error = ERROR_INVALID_PARAMETER;
        goto exit;
    }


    RawDisk = MemAlloc(sizeof *RawDisk);
    if (0 == RawDisk)
    {
        Error = ERROR_NOT_ENOUGH_MEMORY;
        goto exit;
    }

    Handle = CreateFileW(RawDiskFile,
        GENERIC_READ | GENERIC_WRITE, 0, 0, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
    if (INVALID_HANDLE_VALUE == Handle)
    {
        Error = GetLastError();
        goto exit;
    }

    if (!GetFileSizeEx(Handle, &FileSize))
    {
        Error = GetLastError();
        goto exit;
    }

    if (0 == FileSize.QuadPart)
        FileSize.QuadPart = BlockCount * BlockLength;
    if (0 == FileSize.QuadPart || BlockCount * BlockLength != FileSize.QuadPart)
    {
        Error = ERROR_INVALID_PARAMETER;
        goto exit;
    }

    if (!SetFilePointerEx(Handle, FileSize, 0, FILE_BEGIN) ||
        !SetEndOfFile(Handle))
    {
        Error = GetLastError();
        goto exit;
    }

    Mapping = CreateFileMappingW(Handle, 0, PAGE_READWRITE, 0, 0, 0);
    if (0 == Mapping)
    {
        Error = GetLastError();
        goto exit;
    }

    Pointer = MapViewOfFile(Mapping, FILE_MAP_ALL_ACCESS, 0, 0, 0);
    if (0 == Pointer)
    {
        Error = GetLastError();
        goto exit;
    }

    Error = SpdStorageUnitCreate(&StorageUnitParams, &RawDiskInterface, &StorageUnit);
    if (ERROR_SUCCESS != Error)
        goto exit;

    memset(RawDisk, 0, sizeof *RawDisk);
    RawDisk->StorageUnit = StorageUnit;
    RawDisk->BlockCount = BlockCount;
    RawDisk->BlockLength = BlockLength;
    RawDisk->Handle = Handle;
    RawDisk->Mapping = Mapping;
    RawDisk->Pointer = Pointer;
    StorageUnit->UserContext = RawDisk;

    Error = ERROR_SUCCESS;

exit:
    if (ERROR_SUCCESS != Error)
    {
        if (0 != StorageUnit)
            SpdStorageUnitDelete(StorageUnit);

        if (0 != Pointer)
            UnmapViewOfFile(Pointer);

        if (0 != Mapping)
            CloseHandle(Mapping);

        if (INVALID_HANDLE_VALUE != Handle)
            CloseHandle(Handle);

        MemFree(RawDisk);
    }

    return Error;
}

VOID RawDiskDelete(RAWDISK *RawDisk)
{
    SpdStorageUnitDelete(RawDisk->StorageUnit);

    UnmapViewOfFile(RawDisk->Pointer);

    CloseHandle(RawDisk->Mapping);

    CloseHandle(RawDisk->Handle);

    MemFree(RawDisk);
}

SPD_STORAGE_UNIT *RawDiskStorageUnit(RAWDISK *RawDisk)
{
    return RawDisk->StorageUnit;
}
