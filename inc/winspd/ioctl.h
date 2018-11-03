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

#include <devioctl.h>

#ifdef __cplusplus
extern "C" {
#endif

/* static_assert is a C++11 feature, but seems to work with C on MSVC 2015 */
#if defined(WINSPD_SYS_INTERNAL) || defined(WINSPD_DLL_INTERNAL)
#define SPD_IOCTL_STATIC_ASSERT(e,m)    static_assert(e,m)
#else
#define SPD_IOCTL_STATIC_ASSERT(e,m)    static_assert(1,"")
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
#define SPD_IOCTL_PROVISION_PUBLIC      ('P')
#define SPD_IOCTL_UNPROVISION_PUBLIC    ('U')
#define SPD_IOCTL_LIST                  ('l')
#define SPD_IOCTL_TRANSACT              ('t')

/* IOCTL_MINIPORT_PROCESS_SERVICE_IRP marshalling */
#pragma warning(push)
#pragma warning(disable:4200)           /* zero-sized array in struct/union */
typedef struct
{
    GUID Guid;                          /* identity */
    UINT64 BlockCount;                  /* geometry */
    UINT32 BlockLength;                 /* geometry */
    UCHAR ProductId[16];
    UCHAR ProductRevisionLevel[4];
    UINT8 DeviceType;                   /* must be 0: direct access block device */
    UINT32 RemovableMedia:1;            /* must be 0: no removable media */
} SPD_IOCTL_STORAGE_UNIT_PARAMS;
typedef struct
{
    SPD_IOCTL_DECLSPEC_ALIGN UINT16 Size;
    UINT16 Code;
} SPD_IOCTL_BASE_PARAMS;
typedef struct
{
    SPD_IOCTL_BASE_PARAMS Base;
    SPD_IOCTL_STORAGE_UNIT_PARAMS StorageUnit;
} SPD_IOCTL_PROVISION_PARAMS;
typedef struct
{
    SPD_IOCTL_BASE_PARAMS Base;
    GUID Guid;                          /* identity */
} SPD_IOCTL_UNPROVISION_PARAMS;
typedef struct
{
    SPD_IOCTL_BASE_PARAMS Base;
    UINT32 ListAll:1;                   /* include privately provisioned */
    SPD_IOCTL_DECLSPEC_ALIGN UINT8 Buffer[];
} SPD_IOCTL_LIST_PARAMS;
typedef struct
{
    SPD_IOCTL_BASE_PARAMS Base;
    GUID Guid;                          /* identity */
    UINT64 Hint;
    union
    {
        struct
        {
            UINT8 Kind;
            UINT32 Length;
            UINT64 Address;
            UINT64 BlockAddress;
            UINT32 ForceUnitAccess:1;
            UINT32 Reserved:31;
        } Req;
        struct
        {
            UINT8 ScsiStatus;
            SENSE_DATA SenseData;
        } Rsp;
    } Direction;
    SPD_IOCTL_DECLSPEC_ALIGN UINT8 Buffer[];
} SPD_IOCTL_TRANSACT_PARAMS;
#pragma warning(pop)

#if !defined(WINSPD_SYS_INTERNAL)
SPD_API DWORD SpdIoctlGetDevicePath(GUID *ClassGuid, PWSTR DeviceName,
    PWCHAR PathBuf, UINT32 PathBufSize);
SPD_API DWORD SpdIoctlOpenDevice(PWSTR DeviceName, PHANDLE PDeviceHandle);
SPD_API DWORD SpdIoctlProvision(HANDLE DeviceHandle,
    SPD_IOCTL_STORAGE_UNIT_PARAMS *Params, BOOLEAN Public);
SPD_API DWORD SpdIoctlUnprovision(HANDLE DeviceHandle,
    GUID *Guid, BOOLEAN Public);
SPD_API DWORD SpdIoctlGetList(HANDLE DeviceHandle,
    PWCHAR ListBuf, PUINT32 PListSize);
SPD_API DWORD SpdIoctlTransact(HANDLE DeviceHandle,
    PVOID ResponseBuf, UINT32 ResponseBufSize,
    PVOID RequestBuf, UINT32 *PRequestBufSize,
    BOOLEAN Batch);
SPD_API DWORD SpdIoctlScsiExecute(HANDLE DeviceHandle,
    UINT32 Ptl, PCDB Cdb, UCHAR DataDirection, PVOID DataBuffer, PUINT32 PDataLength,
    PUCHAR PScsiStatus, UCHAR SenseInfoBuffer[32]);
SPD_API DWORD SpdIoctlMemAlignAlloc(UINT32 Size, UINT32 AlignmentMask, PVOID *PP);
SPD_API VOID SpdIoctlMemAlignFree(PVOID P);
#endif

#ifdef __cplusplus
}
#endif

#endif
