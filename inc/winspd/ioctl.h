/**
 * @file winspd/ioctl.h
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

#ifndef WINSPD_IOCTL_H_INCLUDED
#define WINSPD_IOCTL_H_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif

#define SPD_IOCTL_DRIVER_NAME           "WinSpd"
#define SPD_IOCTL_VENDOR_ID             "WinSpd  "

/* alignment macros */
#define SPD_IOCTL_ALIGN_UP(x, s)        (((x) + ((s) - 1L)) & ~((s) - 1L))
#define SPD_IOCTL_DEFAULT_ALIGNMENT     8
#define SPD_IOCTL_DEFAULT_ALIGN_UP(x)   SPD_IOCTL_ALIGN_UP(x, SPD_IOCTL_DEFAULT_ALIGNMENT)
#define SPD_IOCTL_DECLSPEC_ALIGN        __declspec(align(SPD_IOCTL_DEFAULT_ALIGNMENT))

/* IOCTL_MINIPORT_PROCESS_SERVICE_IRP codes */
#define SPD_IOCTL_PROVISION             ('p')
#define SPD_IOCTL_UNPROVISION           ('u')
#define SPD_IOCTL_LIST                  ('l')
#define SPD_IOCTL_TRANSACT              ('t')

#define SPD_IOCTL_MAX_UNMAP_DESCR       16

/* IOCTL_MINIPORT_PROCESS_SERVICE_IRP marshalling */
#pragma warning(push)
#pragma warning(disable:4200)           /* zero-sized array in struct/union */
typedef struct
{
    UINT64 BlockCount;                  /* geometry */
    UINT32 BlockLength;                 /* geometry */
    UCHAR ProductId[16];
    UCHAR ProductRevisionLevel[4];
    GUID Guid;
    UINT8 DeviceType;                   /* must be 0: direct access block device */
    UINT32 RemovableMedia:1;            /* must be 0: no removable media */
} SPD_IOCTL_STORAGE_UNIT_PARAMS;
typedef struct
{
    UINT64 Hint;
    UINT8 Kind;
    union
    {
        struct
        {
            UINT64 Address;
            UINT64 BlockAddress;
            UINT32 Length;
            UINT32 ForceUnitAccess:1;
            UINT32 Reserved:31;
        } Read;
        struct
        {
            UINT64 Address;
            UINT64 BlockAddress;
            UINT32 Length;
            UINT32 ForceUnitAccess:1;
            UINT32 Reserved:31;
        } Write;
        struct
        {
            UINT64 BlockAddress;
            UINT32 Length;
        } Flush;
        struct
        {
            UINT16 Count;
            UINT64 BlockAddresses[SPD_IOCTL_MAX_UNMAP_DESCR];
            UINT32 Lengths[SPD_IOCTL_MAX_UNMAP_DESCR];
        } Unmap;
    } Op;
} SPD_IOCTL_TRANSACT_REQ;
typedef struct
{
    UINT64 Hint;
    UINT8 Kind;
    struct
    {
        UINT8 ScsiStatus;
        SENSE_DATA SenseData;
    } Status;
} SPD_IOCTL_TRANSACT_RSP;
typedef struct
{
    SPD_IOCTL_DECLSPEC_ALIGN UINT16 Size;
    UINT16 Code;
} SPD_IOCTL_BASE_PARAMS;
typedef struct
{
    SPD_IOCTL_BASE_PARAMS Base;
    union
    {
        struct
        {
            SPD_IOCTL_STORAGE_UNIT_PARAMS StorageUnitParams;
        } Par;
        struct
        {
            UINT32 Btl;
        } Ret;
    } Dir;
} SPD_IOCTL_PROVISION_PARAMS;
typedef struct
{
    SPD_IOCTL_BASE_PARAMS Base;
    union
    {
        struct
        {
            UINT32 Btl;
        } Par;
    } Dir;
} SPD_IOCTL_UNPROVISION_PARAMS;
typedef struct
{
    SPD_IOCTL_BASE_PARAMS Base;
} SPD_IOCTL_LIST_PARAMS;
typedef struct
{
    SPD_IOCTL_BASE_PARAMS Base;
    UINT32 Btl;
    UINT32 ReqValid:1;
    UINT32 RspValid:1;
    union
    {
        SPD_IOCTL_TRANSACT_REQ Req;
        SPD_IOCTL_TRANSACT_RSP Rsp;
    } Dir;
} SPD_IOCTL_TRANSACT_PARAMS;
#pragma warning(pop)

#if !defined(WINSPD_SYS_INTERNAL)
DWORD SpdIoctlGetDevicePath(GUID *ClassGuid, PWSTR DeviceName,
    PWCHAR PathBuf, UINT32 PathBufSize);
DWORD SpdIoctlOpenDevice(PWSTR DeviceName, PHANDLE PDeviceHandle);
DWORD SpdIoctlScsiExecute(HANDLE DeviceHandle,
    UINT32 Btl, PCDB Cdb, INT DataDirection, PVOID DataBuffer, PUINT32 PDataLength,
    PUCHAR PScsiStatus, UCHAR SenseInfoBuffer[32]);
DWORD SpdIoctlProvision(HANDLE DeviceHandle,
    SPD_IOCTL_STORAGE_UNIT_PARAMS *Params, PUINT32 PBtl);
DWORD SpdIoctlUnprovision(HANDLE DeviceHandle,
    UINT32 Btl);
DWORD SpdIoctlGetList(HANDLE DeviceHandle,
    PUINT32 ListBuf, PUINT32 PListSize);
DWORD SpdIoctlTransact(HANDLE DeviceHandle,
    UINT32 Btl, SPD_IOCTL_TRANSACT_RSP *Rsp, SPD_IOCTL_TRANSACT_REQ *Req);
DWORD SpdIoctlMemAlignAlloc(UINT32 Size, UINT32 AlignmentMask, PVOID *PP);
VOID SpdIoctlMemAlignFree(PVOID P);
#endif

#ifdef __cplusplus
}
#endif

#endif
