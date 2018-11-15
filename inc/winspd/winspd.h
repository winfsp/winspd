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
    UCHAR (*Read)(SPD_STORAGE_UNIT *StorageUnit,
        UINT64 BlockAddress, PVOID Buffer, UINT32 Length, BOOLEAN Flush,
        PSENSE_DATA SenseData);
    UCHAR (*Write)(SPD_STORAGE_UNIT *StorageUnit,
        UINT64 BlockAddress, PVOID Buffer, UINT32 Length, BOOLEAN Flush,
        PSENSE_DATA SenseData);
    UCHAR (*Flush)(SPD_STORAGE_UNIT *StorageUnit,
        UINT64 BlockAddress, UINT32 Length,
        PSENSE_DATA SenseData);
    UCHAR (*Unmap)(SPD_STORAGE_UNIT *StorageUnit,
        UINT64 BlockAddresses[], UINT32 Lengths[], UINT32 Count,
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
    GUID Guid;
    HANDLE DeviceHandle;
    UINT32 Btl;
    const SPD_STORAGE_UNIT_INTERFACE *Interface;
    HANDLE DispatcherThread;
    ULONG DispatcherThreadCount;
    DWORD DispatcherError;
    UINT32 DebugLog;
} SPD_STORAGE_UNIT;
typedef struct _SPD_STORAGE_UNIT_OPERATION_CONTEXT
{
    SPD_IOCTL_TRANSACT_REQ *Request;
    SPD_IOCTL_TRANSACT_RSP *Response;
} SPD_STORAGE_UNIT_OPERATION_CONTEXT;
/**
 * Create a storage unit object.
 *
 * @param StorageUnitParams
 *     Parameters for the newly created storage unit.
 * @param Interface
 *     A pointer to the operations that implement this storage unit.
 * @param PStorageUnit [out]
 *     Pointer that will receive the storage unit object created on successful return from this
 *     call.
 * @return
 *     ERROR_SUCCESS or error code.
 */
DWORD SpdStorageUnitCreate(
    const SPD_IOCTL_STORAGE_UNIT_PARAMS *StorageUnitParams,
    const SPD_STORAGE_UNIT_INTERFACE *Interface,
    SPD_STORAGE_UNIT **PStorageUnit);
/**
 * Delete a storage unit object.
 *
 * @param StorageUnit
 *     The storage unit object.
 */
VOID SpdStorageUnitDelete(SPD_STORAGE_UNIT *StorageUnit);
/**
 * Start the storage unit dispatcher.
 *
 * @param StorageUnit
 *     The storage unit object.
 * @param ThreadCount
 *     The number of threads for the dispatcher. A value of 0 will create a default
 *     number of threads and should be chosen in most cases.
 * @return
 *     ERROR_SUCCESS or error code.
 */
DWORD SpdStorageUnitStartDispatcher(SPD_STORAGE_UNIT *StorageUnit, ULONG ThreadCount);
/**
 * Stop the storage unit dispatcher.
 *
 * @param StorageUnit
 *     The storage unit object.
 */
VOID SpdStorageUnitStopDispatcher(SPD_STORAGE_UNIT *StorageUnit);
/**
 * Send a response to the kernel.
 *
 * @param StorageUnit
 *     The storage unit object.
 * @param Response
 *     The response buffer.
 */
VOID SpdStorageUnitSendResponse(SPD_STORAGE_UNIT *StorageUnit,
    SPD_IOCTL_TRANSACT_RSP *Response);
/**
 * Get the current operation context.
 *
 * This function may be used only when servicing one of the SPD_STORAGE_UNIT_INTERFACE operations.
 * The current operation context is stored in thread local storage. It allows access to the
 * Request and Response associated with this operation.
 *
 * @return
 *     The current operation context.
 */
SPD_STORAGE_UNIT_OPERATION_CONTEXT *SpdStorageUnitGetOperationContext(VOID);
static inline
VOID SpdStorageUnitGetDispatcherError(SPD_STORAGE_UNIT *StorageUnit,
    DWORD *PDispatcherError)
{
    /* 32-bit reads are atomic */
    *PDispatcherError = StorageUnit->DispatcherError;
    MemoryBarrier();
}
static inline
VOID SpdStorageUnitSetDispatcherError(SPD_STORAGE_UNIT *StorageUnit,
    DWORD DispatcherError)
{
    if (ERROR_SUCCESS == DispatcherError)
        return;
    InterlockedCompareExchange(&StorageUnit->DispatcherError, DispatcherError, 0);
}

/*
 * Utility
 */
VOID SpdDebugLogSetHandle(HANDLE Handle);
VOID SpdDebugLog(const char *Format, ...);
VOID SpdDebugLogRequest(SPD_IOCTL_TRANSACT_REQ *Request);
VOID SpdDebugLogResponse(SPD_IOCTL_TRANSACT_RSP *Response);

#ifdef __cplusplus
}
#endif

#endif
