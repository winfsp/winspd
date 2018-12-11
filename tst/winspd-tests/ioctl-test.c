/**
 * @file ioctl-test.c
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

#include <winspd/winspd.h>
#include <tlib/testsuite.h>
#include "rawdisk.h"

static const GUID TestGuid = 
    { 0x4112a9a1, 0xf079, 0x4f3d, { 0xba, 0x53, 0x2d, 0x5d, 0xf2, 0x7d, 0x28, 0xb5 } };
static const GUID TestGuid2 = 
    { 0xd7f5a95d, 0xb9f0, 0x4e47, { 0x87, 0x3b, 0xa, 0xb0, 0xa, 0x89, 0xf9, 0x5a } };

static void ioctl_provision_test(void)
{
    SPD_IOCTL_STORAGE_UNIT_PARAMS StorageUnitParams;
    HANDLE DeviceHandle;
    UINT32 Btl;
    DWORD Error;
    BOOL Success;

    Error = SpdIoctlOpenDevice(L"" SPD_IOCTL_HARDWARE_ID, &DeviceHandle);
    ASSERT(ERROR_SUCCESS == Error);

    memset(&StorageUnitParams, 0, sizeof StorageUnitParams);
    memcpy(&StorageUnitParams.Guid, &TestGuid, sizeof TestGuid);
    StorageUnitParams.BlockCount = 16;
    StorageUnitParams.BlockLength = 512;
    StorageUnitParams.MaxTransferLength = 512;
    Error = SpdIoctlProvision(DeviceHandle, &StorageUnitParams, &Btl);
    ASSERT(ERROR_SUCCESS == Error);
    ASSERT(0 == Btl);

    Error = SpdIoctlUnprovision(DeviceHandle, &StorageUnitParams.Guid);
    ASSERT(ERROR_SUCCESS == Error);

    Success = CloseHandle(DeviceHandle);
    ASSERT(Success);
}

static void ioctl_provision_invalid_test(void)
{
    SPD_IOCTL_STORAGE_UNIT_PARAMS StorageUnitParams;
    HANDLE DeviceHandle;
    UINT32 Btl;
    DWORD Error;
    BOOL Success;

    Error = SpdIoctlOpenDevice(L"" SPD_IOCTL_HARDWARE_ID, &DeviceHandle);
    ASSERT(ERROR_SUCCESS == Error);

    memset(&StorageUnitParams, 0, sizeof StorageUnitParams);
    Error = SpdIoctlProvision(DeviceHandle, &StorageUnitParams, &Btl);
    ASSERT(ERROR_INVALID_PARAMETER == Error);
    ASSERT((UINT32)-1 == Btl);

    memcpy(&StorageUnitParams.Guid, &TestGuid, sizeof TestGuid);
    Error = SpdIoctlProvision(DeviceHandle, &StorageUnitParams, &Btl);
    ASSERT(ERROR_INVALID_PARAMETER == Error);
    ASSERT((UINT32)-1 == Btl);

    StorageUnitParams.BlockCount = 16;
    StorageUnitParams.BlockLength = 512;
    StorageUnitParams.MaxTransferLength = 512 + 1;
    Error = SpdIoctlProvision(DeviceHandle, &StorageUnitParams, &Btl);
    ASSERT(ERROR_INVALID_PARAMETER == Error);
    ASSERT((UINT32)-1 == Btl);

    StorageUnitParams.MaxTransferLength = 512;
    Error = SpdIoctlProvision(DeviceHandle, &StorageUnitParams, &Btl);
    ASSERT(ERROR_SUCCESS == Error);
    ASSERT(0 == Btl);

    Error = SpdIoctlUnprovision(DeviceHandle, &StorageUnitParams.Guid);
    ASSERT(ERROR_SUCCESS == Error);

    Success = CloseHandle(DeviceHandle);
    ASSERT(Success);
}

static void ioctl_provision_multi_test(void)
{
    SPD_IOCTL_STORAGE_UNIT_PARAMS StorageUnitParams;
    HANDLE DeviceHandle;
    UINT32 Btl;
    DWORD Error;
    BOOL Success;

    Error = SpdIoctlOpenDevice(L"" SPD_IOCTL_HARDWARE_ID, &DeviceHandle);
    ASSERT(ERROR_SUCCESS == Error);

    memset(&StorageUnitParams, 0, sizeof StorageUnitParams);
    memcpy(&StorageUnitParams.Guid, &TestGuid, sizeof TestGuid);
    StorageUnitParams.BlockCount = 16;
    StorageUnitParams.BlockLength = 512;
    StorageUnitParams.MaxTransferLength = 512;
    Error = SpdIoctlProvision(DeviceHandle, &StorageUnitParams, &Btl);
    ASSERT(ERROR_SUCCESS == Error);
    ASSERT(0 == Btl);

    Error = SpdIoctlProvision(DeviceHandle, &StorageUnitParams, &Btl);
    ASSERT(ERROR_ALREADY_EXISTS == Error);
    ASSERT((UINT32)-1 == Btl);

    memset(&StorageUnitParams, 0, sizeof StorageUnitParams);
    memcpy(&StorageUnitParams.Guid, &TestGuid2, sizeof TestGuid2);
    StorageUnitParams.BlockCount = 16;
    StorageUnitParams.BlockLength = 512;
    StorageUnitParams.MaxTransferLength = 512;
    Error = SpdIoctlProvision(DeviceHandle, &StorageUnitParams, &Btl);
    ASSERT(ERROR_SUCCESS == Error);
    ASSERT(SPD_IOCTL_BTL(0, 1, 0) == Btl);

    Error = SpdIoctlUnprovision(DeviceHandle, &TestGuid);
    ASSERT(ERROR_SUCCESS == Error);

    Error = SpdIoctlUnprovision(DeviceHandle, &TestGuid2);
    ASSERT(ERROR_SUCCESS == Error);

    Success = CloseHandle(DeviceHandle);
    ASSERT(Success);
}

void ioctl_tests(void)
{
    TEST(ioctl_provision_test);
    TEST(ioctl_provision_invalid_test);
    TEST(ioctl_provision_multi_test);
}
