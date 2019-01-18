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

#define WARNONCE(expr)                  \
    do                                  \
    {                                   \
        static LONG Once;               \
        if (!(expr) &&                  \
            0 == InterlockedCompareExchange(&Once, 1, 0))\
            warn("WARNONCE(%s) failed at %s:%d", #expr, __func__, __LINE__);\
    } while (0,0)

typedef struct _RAWDISK
{
    SPD_STORAGE_UNIT *StorageUnit;
    UINT64 BlockCount;
    UINT32 BlockLength;
    HANDLE Handle;
    HANDLE Mapping;
    PVOID Pointer;
    BOOLEAN Sparse;
} RAWDISK;

static inline BOOLEAN ExceptionFilter(ULONG Code, PEXCEPTION_POINTERS Pointers,
    PUINT_PTR PDataAddress)
{
    if (EXCEPTION_IN_PAGE_ERROR != Code)
        return EXCEPTION_CONTINUE_SEARCH;

    *PDataAddress = 2 <= Pointers->ExceptionRecord->NumberParameters ?
        Pointers->ExceptionRecord->ExceptionInformation[1] : 0;
    return EXCEPTION_EXECUTE_HANDLER;
}

static BOOLEAN FlushInternal(SPD_STORAGE_UNIT *StorageUnit,
    UINT64 BlockAddress, UINT32 BlockCount,
    SPD_STORAGE_UNIT_STATUS *Status)
{
    RAWDISK *RawDisk = StorageUnit->UserContext;
    PVOID FileBuffer = (PUINT8)RawDisk->Pointer + BlockAddress * RawDisk->BlockLength;

    if (!FlushViewOfFile(FileBuffer, BlockCount * RawDisk->BlockLength))
        goto error;
    if (!FlushFileBuffers(RawDisk->Handle))
        goto error;

    return TRUE;

error:
    SpdStorageUnitStatusSetSense(Status,
        SCSI_SENSE_MEDIUM_ERROR, SCSI_ADSENSE_WRITE_ERROR, 0);

    return TRUE;
}

static BOOLEAN Read(SPD_STORAGE_UNIT *StorageUnit,
    PVOID Buffer, UINT64 BlockAddress, UINT32 BlockCount, BOOLEAN FlushFlag,
    SPD_STORAGE_UNIT_STATUS *Status)
{
    WARNONCE(StorageUnit->CacheSupported || FlushFlag);

    if (FlushFlag)
    {
        FlushInternal(StorageUnit, BlockAddress, BlockCount, Status);
        if (SCSISTAT_GOOD != Status->ScsiStatus)
            return TRUE;
    }

    RAWDISK *RawDisk = StorageUnit->UserContext;
    PVOID FileBuffer = (PUINT8)RawDisk->Pointer + BlockAddress * RawDisk->BlockLength;
    UINT_PTR ExceptionDataAddress;

    __try
    {
        memcpy(Buffer, FileBuffer, BlockCount * RawDisk->BlockLength);
    }
    __except (ExceptionFilter(GetExceptionCode(), GetExceptionInformation(), &ExceptionDataAddress))
    {
        UINT64 Information = (UINT64)(ExceptionDataAddress - (UINT_PTR)RawDisk->Pointer) /
            RawDisk->BlockLength;
        SpdStorageUnitStatusSetSense(Status,
            SCSI_SENSE_MEDIUM_ERROR, SCSI_ADSENSE_UNRECOVERED_ERROR, 0 != Information ? &Information : 0);
    }

    return TRUE;
}

static BOOLEAN Write(SPD_STORAGE_UNIT *StorageUnit,
    PVOID Buffer, UINT64 BlockAddress, UINT32 BlockCount, BOOLEAN FlushFlag,
    SPD_STORAGE_UNIT_STATUS *Status)
{
    WARNONCE(StorageUnit->CacheSupported || FlushFlag);

    RAWDISK *RawDisk = StorageUnit->UserContext;
    PVOID FileBuffer = (PUINT8)RawDisk->Pointer + BlockAddress * RawDisk->BlockLength;
    UINT_PTR ExceptionDataAddress;

    __try
    {
        memcpy(FileBuffer, Buffer, BlockCount * RawDisk->BlockLength);
    }
    __except (ExceptionFilter(GetExceptionCode(), GetExceptionInformation(), &ExceptionDataAddress))
    {
        UINT64 Information = (UINT64)(ExceptionDataAddress - (UINT_PTR)RawDisk->Pointer) /
            RawDisk->BlockLength;
        SpdStorageUnitStatusSetSense(Status,
            SCSI_SENSE_MEDIUM_ERROR, SCSI_ADSENSE_WRITE_ERROR, 0 != Information ? &Information : 0);
    }

    if (SCSISTAT_GOOD == Status->ScsiStatus && FlushFlag)
        FlushInternal(StorageUnit, BlockAddress, BlockCount, Status);

    return TRUE;
}

static BOOLEAN Flush(SPD_STORAGE_UNIT *StorageUnit,
    UINT64 BlockAddress, UINT32 BlockCount,
    SPD_STORAGE_UNIT_STATUS *Status)
{
    WARNONCE(StorageUnit->CacheSupported);

    return FlushInternal(StorageUnit, BlockAddress, BlockCount, Status);
}

static BOOLEAN Unmap(SPD_STORAGE_UNIT *StorageUnit,
    SPD_UNMAP_DESCRIPTOR Descriptors[], UINT32 Count,
    SPD_STORAGE_UNIT_STATUS *Status)
{
    RAWDISK *RawDisk = StorageUnit->UserContext;
    FILE_ZERO_DATA_INFORMATION Zero;
    DWORD BytesTransferred;
    PVOID FileBuffer;
    UINT_PTR ExceptionDataAddress;

    for (UINT32 I = 0; Count > I; I++)
    {
        BOOLEAN SetZero = FALSE;

        if (RawDisk->Sparse)
        {
            Zero.FileOffset.QuadPart = Descriptors[I].BlockAddress * RawDisk->BlockLength;
            Zero.BeyondFinalZero.QuadPart = (Descriptors[I].BlockAddress + Descriptors[I].BlockCount) *
                RawDisk->BlockLength;
            SetZero = DeviceIoControl(RawDisk->Handle,
                FSCTL_SET_ZERO_DATA, &Zero, sizeof Zero, 0, 0, &BytesTransferred, 0);
        }

        if (!SetZero)
        {
            FileBuffer = (PUINT8)RawDisk->Pointer + Descriptors[I].BlockAddress * RawDisk->BlockLength;

            __try
            {
                memset(FileBuffer, 0, Descriptors[I].BlockCount * RawDisk->BlockLength);
            }
            __except (ExceptionFilter(GetExceptionCode(), GetExceptionInformation(), &ExceptionDataAddress))
            {
            }
        }
    }

    return TRUE;
}

static SPD_STORAGE_UNIT_INTERFACE RawDiskInterface =
{
    Read,
    Write,
    Flush,
    Unmap,
};

DWORD RawDiskCreate(PWSTR RawDiskFile,
    UINT64 BlockCount, UINT32 BlockLength,
    PWSTR ProductId, PWSTR ProductRevision,
    BOOLEAN CacheSupported,
    PWSTR PipeName,
    RAWDISK **PRawDisk)
{
    RAWDISK *RawDisk = 0;
    HANDLE Handle = INVALID_HANDLE_VALUE;
    HANDLE Mapping = 0;
    PVOID Pointer = 0;
    FILE_SET_SPARSE_BUFFER Sparse;
    DWORD BytesTransferred;
    LARGE_INTEGER FileSize;
    SPD_STORAGE_UNIT_PARAMS StorageUnitParams;
    SPD_STORAGE_UNIT *StorageUnit = 0;
    DWORD Error;

    *PRawDisk = 0;

    memset(&StorageUnitParams, 0, sizeof StorageUnitParams);
    UuidCreate(&StorageUnitParams.Guid);
    StorageUnitParams.BlockCount = BlockCount;
    StorageUnitParams.BlockLength = BlockLength;
    StorageUnitParams.MaxTransferLength = 64 * 1024;
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
    StorageUnitParams.CacheSupported = CacheSupported;

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

    Sparse.SetSparse = TRUE;
    Sparse.SetSparse = DeviceIoControl(Handle,
        FSCTL_SET_SPARSE, &Sparse, sizeof Sparse, 0, 0, &BytesTransferred, 0);

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

    Error = SpdStorageUnitCreate(PipeName, &StorageUnitParams, &RawDiskInterface, &StorageUnit);
    if (ERROR_SUCCESS != Error)
        goto exit;

    memset(RawDisk, 0, sizeof *RawDisk);
    RawDisk->StorageUnit = StorageUnit;
    RawDisk->BlockCount = BlockCount;
    RawDisk->BlockLength = BlockLength;
    RawDisk->Handle = Handle;
    RawDisk->Mapping = Mapping;
    RawDisk->Pointer = Pointer;
    RawDisk->Sparse = Sparse.SetSparse;
    StorageUnit->UserContext = RawDisk;

    *PRawDisk = RawDisk;

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

    FlushViewOfFile(RawDisk->Pointer, 0);
    FlushFileBuffers(RawDisk->Handle);
    UnmapViewOfFile(RawDisk->Pointer);
    CloseHandle(RawDisk->Mapping);
    CloseHandle(RawDisk->Handle);

    MemFree(RawDisk);
}

SPD_STORAGE_UNIT *RawDiskStorageUnit(RAWDISK *RawDisk)
{
    return RawDisk->StorageUnit;
}
