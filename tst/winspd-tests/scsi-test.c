/**
 * @file scsi-test.c
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

#include <winspd/winspd.h>
#include <tlib/testsuite.h>
#include <process.h>

static const GUID TestGuid = 
    { 0x4112a9a1, 0xf079, 0x4f3d, { 0xba, 0x53, 0x2d, 0x5d, 0xf2, 0x7d, 0x28, 0xb5 } };
static const char *TestGuidString =
    "4112a9a1-f079-4f3d-ba53-2d5df27d28b5";

static void scsi_inquiry_dotest(BOOLEAN UnmapSupported)
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
    /* NOTE:
     * Comment the following 2 lines out. If we do not do this, then the partmgr(?)
     * sends us a READ_CAPACITY and a READ command for block 0 (MBR). This causes 2 problems:
     *
     * - Because we do not execute SpdIoctlTransact we never complete the READ SRB.
     * - For some reason our SCSI ioctls fail with ERROR_INVALID_FUNCTION.
     */
    //memcpy(StorageUnitParams.ProductId, "TestDisk        ", 16);
    //memcpy(StorageUnitParams.ProductRevisionLevel, "0.1 ", 4);
    StorageUnitParams.BlockCount = 16;
    StorageUnitParams.BlockLength = 512;
    StorageUnitParams.MaxTransferLength = 512;
    StorageUnitParams.UnmapSupported = !!UnmapSupported;
    Error = SpdIoctlProvision(DeviceHandle, &StorageUnitParams, &Btl);
    ASSERT(ERROR_SUCCESS == Error);
    ASSERT(0 == Btl);

    Error = SpdIoctlScsiInquiry(DeviceHandle, Btl, 0, 3000);
    ASSERT(ERROR_SUCCESS == Error);

    CDB Cdb;
    UINT8 DataBuffer[VPD_MAX_BUFFER_SIZE];
    UINT32 DataLength;
    UCHAR ScsiStatus;
    union
    {
        SENSE_DATA Data;
        UCHAR Buffer[32];
    } Sense;

    {
        memset(&Cdb, 0, sizeof Cdb);
        Cdb.CDB6INQUIRY3.OperationCode = SCSIOP_INQUIRY;
        Cdb.CDB6INQUIRY3.AllocationLength = VPD_MAX_BUFFER_SIZE;

        DataLength = sizeof DataBuffer;
        Error = SpdIoctlScsiExecute(DeviceHandle, Btl, &Cdb, +1, DataBuffer, &DataLength,
            &ScsiStatus, Sense.Buffer);
        ASSERT(ERROR_SUCCESS == Error);

        PINQUIRYDATA InquiryData = (PVOID)DataBuffer;
        ASSERT(0 == memcmp(InquiryData->VendorId, SPD_IOCTL_VENDOR_ID, 8));
        ASSERT(0 == memcmp(InquiryData->ProductId, StorageUnitParams.ProductId, 16));
        ASSERT(0 == memcmp(InquiryData->ProductRevisionLevel, StorageUnitParams.ProductRevisionLevel, 4));
    }

    {
        memset(&Cdb, 0, sizeof Cdb);
        Cdb.CDB6INQUIRY3.OperationCode = SCSIOP_INQUIRY;
        Cdb.CDB6INQUIRY3.EnableVitalProductData = 1;
        Cdb.CDB6INQUIRY3.PageCode = VPD_SUPPORTED_PAGES;
        Cdb.CDB6INQUIRY3.AllocationLength = VPD_MAX_BUFFER_SIZE;

        DataLength = sizeof DataBuffer;
        Error = SpdIoctlScsiExecute(DeviceHandle, Btl, &Cdb, +1, DataBuffer, &DataLength,
            &ScsiStatus, Sense.Buffer);
        ASSERT(ERROR_SUCCESS == Error);

        PVPD_SUPPORTED_PAGES_PAGE SupportedPages = (PVOID)DataBuffer;
        ASSERT(5 == SupportedPages->PageLength);
        ASSERT(VPD_SUPPORTED_PAGES == SupportedPages->SupportedPageList[0]);
        ASSERT(VPD_SERIAL_NUMBER == SupportedPages->SupportedPageList[1]);
        ASSERT(VPD_DEVICE_IDENTIFIERS == SupportedPages->SupportedPageList[2]);
        ASSERT(VPD_BLOCK_LIMITS == SupportedPages->SupportedPageList[3]);
        ASSERT(VPD_LOGICAL_BLOCK_PROVISIONING == SupportedPages->SupportedPageList[4]);
    }

    {
        memset(&Cdb, 0, sizeof Cdb);
        Cdb.CDB6INQUIRY3.OperationCode = SCSIOP_INQUIRY;
        Cdb.CDB6INQUIRY3.EnableVitalProductData = 1;
        Cdb.CDB6INQUIRY3.PageCode = VPD_SERIAL_NUMBER;
        Cdb.CDB6INQUIRY3.AllocationLength = VPD_MAX_BUFFER_SIZE;

        DataLength = sizeof DataBuffer;
        Error = SpdIoctlScsiExecute(DeviceHandle, Btl, &Cdb, +1, DataBuffer, &DataLength,
            &ScsiStatus, Sense.Buffer);
        ASSERT(ERROR_SUCCESS == Error);

        PVPD_SERIAL_NUMBER_PAGE SerialNumber = (PVOID)DataBuffer;
        ASSERT(36 == SerialNumber->PageLength);
        ASSERT(0 == memcmp(SerialNumber->SerialNumber, TestGuidString, 36));
    }

    {
        memset(&Cdb, 0, sizeof Cdb);
        Cdb.CDB6INQUIRY3.OperationCode = SCSIOP_INQUIRY;
        Cdb.CDB6INQUIRY3.EnableVitalProductData = 1;
        Cdb.CDB6INQUIRY3.PageCode = VPD_DEVICE_IDENTIFIERS;
        Cdb.CDB6INQUIRY3.AllocationLength = VPD_MAX_BUFFER_SIZE;

        DataLength = sizeof DataBuffer;
        Error = SpdIoctlScsiExecute(DeviceHandle, Btl, &Cdb, +1, DataBuffer, &DataLength,
            &ScsiStatus, Sense.Buffer);
        ASSERT(ERROR_SUCCESS == Error);

        PVPD_IDENTIFICATION_PAGE Identification = (PVOID)DataBuffer;
        ASSERT(Identification->PageLength ==
            sizeof(VPD_IDENTIFICATION_PAGE) + 8 + 16 + 4 + 36 +
            sizeof(VPD_IDENTIFICATION_PAGE) + 8);

        PVPD_IDENTIFICATION_DESCRIPTOR IdentificationDescriptor =
            (PVPD_IDENTIFICATION_DESCRIPTOR)Identification->Descriptors;
        ASSERT(VpdCodeSetAscii == IdentificationDescriptor->CodeSet);
        ASSERT(VpdIdentifierTypeVendorId == IdentificationDescriptor->IdentifierType);
        ASSERT(8 + 16 + 4 + 36 == IdentificationDescriptor->IdentifierLength);
        ASSERT(0 == memcmp(IdentificationDescriptor->Identifier + 0,
            SPD_IOCTL_VENDOR_ID, 8));
        ASSERT(0 == memcmp(IdentificationDescriptor->Identifier + 8,
            StorageUnitParams.ProductId, 16));
        ASSERT(0 == memcmp(IdentificationDescriptor->Identifier + 8 + 16,
            StorageUnitParams.ProductRevisionLevel, 4));
        ASSERT(0 == memcmp(IdentificationDescriptor->Identifier + 8 + 16 + 4,
            TestGuidString, 36));

        IdentificationDescriptor = (PVOID)((PUINT8)IdentificationDescriptor +
            sizeof(VPD_IDENTIFICATION_PAGE) + 8 + 16 + 4 + 36);
        ASSERT(VpdCodeSetBinary == IdentificationDescriptor->CodeSet);
        ASSERT(VpdIdentifierTypeVendorSpecific == IdentificationDescriptor->IdentifierType);
        ASSERT(8 == IdentificationDescriptor->IdentifierLength);
        ASSERT(
            'P' == IdentificationDescriptor->Identifier[0] &&
            'I' == IdentificationDescriptor->Identifier[1] &&
            'D' == IdentificationDescriptor->Identifier[2] &&
            ' ' == IdentificationDescriptor->Identifier[3]);
    }

    {
        memset(&Cdb, 0, sizeof Cdb);
        Cdb.CDB6INQUIRY3.OperationCode = SCSIOP_INQUIRY;
        Cdb.CDB6INQUIRY3.EnableVitalProductData = 1;
        Cdb.CDB6INQUIRY3.PageCode = VPD_BLOCK_LIMITS;
        Cdb.CDB6INQUIRY3.AllocationLength = VPD_MAX_BUFFER_SIZE;

        DataLength = sizeof DataBuffer;
        Error = SpdIoctlScsiExecute(DeviceHandle, Btl, &Cdb, +1, DataBuffer, &DataLength,
            &ScsiStatus, Sense.Buffer);
        ASSERT(ERROR_SUCCESS == Error);

        PVPD_BLOCK_LIMITS_PAGE BlockLimits = (PVOID)DataBuffer;
        ASSERT(StorageUnitParams.MaxTransferLength / StorageUnitParams.BlockLength == (
            (BlockLimits->MaximumTransferLength[0] << 24) |
            (BlockLimits->MaximumTransferLength[1] << 16) |
            (BlockLimits->MaximumTransferLength[2] << 8) |
            (BlockLimits->MaximumTransferLength[3])));
        ASSERT((UnmapSupported ? 0xffffffff : 0) == (
            (BlockLimits->MaximumUnmapLBACount[0] << 24) |
            (BlockLimits->MaximumUnmapLBACount[1] << 16) |
            (BlockLimits->MaximumUnmapLBACount[2] << 8) |
            (BlockLimits->MaximumUnmapLBACount[3])));
        ASSERT((UnmapSupported ? StorageUnitParams.MaxTransferLength / 16 : 0) == (
            (BlockLimits->MaximumUnmapBlockDescriptorCount[0] << 24) |
            (BlockLimits->MaximumUnmapBlockDescriptorCount[1] << 16) |
            (BlockLimits->MaximumUnmapBlockDescriptorCount[2] << 8) |
            (BlockLimits->MaximumUnmapBlockDescriptorCount[3])));
    }

    {
        memset(&Cdb, 0, sizeof Cdb);
        Cdb.CDB6INQUIRY3.OperationCode = SCSIOP_INQUIRY;
        Cdb.CDB6INQUIRY3.EnableVitalProductData = 1;
        Cdb.CDB6INQUIRY3.PageCode = VPD_LOGICAL_BLOCK_PROVISIONING;
        Cdb.CDB6INQUIRY3.AllocationLength = VPD_MAX_BUFFER_SIZE;

        DataLength = sizeof DataBuffer;
        Error = SpdIoctlScsiExecute(DeviceHandle, Btl, &Cdb, +1, DataBuffer, &DataLength,
            &ScsiStatus, Sense.Buffer);
        ASSERT(ERROR_SUCCESS == Error);

        PVPD_LOGICAL_BLOCK_PROVISIONING_PAGE LogicalBlockProvisioning = (PVOID)DataBuffer;
        ASSERT((UnmapSupported ? PROVISIONING_TYPE_THIN : 0) == LogicalBlockProvisioning->ProvisioningType);
        ASSERT((UnmapSupported ? 1 : 0) == LogicalBlockProvisioning->LBPU);
        ASSERT(!LogicalBlockProvisioning->ANC_SUP);
    }

    Error = SpdIoctlUnprovision(DeviceHandle, &StorageUnitParams.Guid);
    ASSERT(ERROR_SUCCESS == Error);

    Success = CloseHandle(DeviceHandle);
    ASSERT(Success);
}

static void scsi_inquiry_test(void)
{
    scsi_inquiry_dotest(FALSE);
    scsi_inquiry_dotest(TRUE);
}

static void scsi_mode_sense_dotest(BOOLEAN WriteProtected, BOOLEAN CacheSupported)
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
    /* NOTE:
     * Comment the following 2 lines out. If we do not do this, then the partmgr(?)
     * sends us a READ_CAPACITY and a READ command for block 0 (MBR). This causes 2 problems:
     *
     * - Because we do not execute SpdIoctlTransact we never complete the READ SRB.
     * - For some reason our SCSI ioctls fail with ERROR_INVALID_FUNCTION.
     */
    //memcpy(StorageUnitParams.ProductId, "TestDisk        ", 16);
    //memcpy(StorageUnitParams.ProductRevisionLevel, "0.1 ", 4);
    StorageUnitParams.BlockCount = 16;
    StorageUnitParams.BlockLength = 512;
    StorageUnitParams.MaxTransferLength = 512;
    StorageUnitParams.WriteProtected = !!WriteProtected;
    StorageUnitParams.CacheSupported = !!CacheSupported;
    Error = SpdIoctlProvision(DeviceHandle, &StorageUnitParams, &Btl);
    ASSERT(ERROR_SUCCESS == Error);
    ASSERT(0 == Btl);

    Error = SpdIoctlScsiInquiry(DeviceHandle, Btl, 0, 3000);
    ASSERT(ERROR_SUCCESS == Error);

    CDB Cdb;
    UINT8 DataBuffer[255];
    UINT32 DataLength;
    UCHAR ScsiStatus;
    union
    {
        SENSE_DATA Data;
        UCHAR Buffer[32];
    } Sense;

    {
        memset(&Cdb, 0, sizeof Cdb);
        Cdb.MODE_SENSE.OperationCode = SCSIOP_MODE_SENSE;
        Cdb.MODE_SENSE.Pc = MODE_SENSE_CURRENT_VALUES;
        Cdb.MODE_SENSE.PageCode = MODE_SENSE_RETURN_ALL;
        Cdb.MODE_SENSE.AllocationLength = 255;

        DataLength = sizeof DataBuffer;
        Error = SpdIoctlScsiExecute(DeviceHandle, Btl, &Cdb, +1, DataBuffer, &DataLength,
            &ScsiStatus, Sense.Buffer);
        ASSERT(ERROR_SUCCESS == Error);

        PMODE_PARAMETER_HEADER ModeParameterHeader = (PVOID)DataBuffer;
        ASSERT(0 == ModeParameterHeader->MediumType);
        ASSERT(
            ((WriteProtected ? MODE_DSP_WRITE_PROTECT : 0) | (CacheSupported ? MODE_DSP_FUA_SUPPORTED : 0)) ==
            ModeParameterHeader->DeviceSpecificParameter);
        ASSERT(0 == ModeParameterHeader->BlockDescriptorLength);

        PMODE_CACHING_PAGE ModeCachingPage = (PVOID)(ModeParameterHeader + 1);
        ASSERT(MODE_PAGE_CACHING == ModeCachingPage->PageCode);
        ASSERT(0 == ModeCachingPage->PageSavable);
        ASSERT(!CacheSupported == ModeCachingPage->ReadDisableCache);
        ASSERT(!!CacheSupported == ModeCachingPage->WriteCacheEnable);
    }

    {
        memset(&Cdb, 0, sizeof Cdb);
        Cdb.MODE_SENSE10.OperationCode = SCSIOP_MODE_SENSE10;
        Cdb.MODE_SENSE10.Pc = MODE_SENSE_CURRENT_VALUES;
        Cdb.MODE_SENSE10.PageCode = MODE_SENSE_RETURN_ALL;
        Cdb.MODE_SENSE10.AllocationLength[1] = 255;

        DataLength = sizeof DataBuffer;
        Error = SpdIoctlScsiExecute(DeviceHandle, Btl, &Cdb, +1, DataBuffer, &DataLength,
            &ScsiStatus, Sense.Buffer);
        ASSERT(ERROR_SUCCESS == Error);

        PMODE_PARAMETER_HEADER10 ModeParameterHeader = (PVOID)DataBuffer;
        ASSERT(0 == ModeParameterHeader->MediumType);
        ASSERT(
            ((WriteProtected ? MODE_DSP_WRITE_PROTECT : 0) | (CacheSupported ? MODE_DSP_FUA_SUPPORTED : 0)) ==
            ModeParameterHeader->DeviceSpecificParameter);
        ASSERT(
            0 == ModeParameterHeader->BlockDescriptorLength[0] &&
            0 == ModeParameterHeader->BlockDescriptorLength[1]);

        PMODE_CACHING_PAGE ModeCachingPage = (PVOID)(ModeParameterHeader + 1);
        ASSERT(MODE_PAGE_CACHING == ModeCachingPage->PageCode);
        ASSERT(0 == ModeCachingPage->PageSavable);
        ASSERT(!CacheSupported == ModeCachingPage->ReadDisableCache);
        ASSERT(!!CacheSupported == ModeCachingPage->WriteCacheEnable);
    }

    Error = SpdIoctlUnprovision(DeviceHandle, &StorageUnitParams.Guid);
    ASSERT(ERROR_SUCCESS == Error);

    Success = CloseHandle(DeviceHandle);
    ASSERT(Success);
}

static void scsi_mode_sense_test(void)
{
    scsi_mode_sense_dotest(FALSE, FALSE);
    scsi_mode_sense_dotest(FALSE, TRUE);
    scsi_mode_sense_dotest(TRUE, FALSE);
}

static void scsi_read_capacity_dotest(BOOLEAN UnmapSupported)
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
    /* NOTE:
     * Comment the following 2 lines out. If we do not do this, then the partmgr(?)
     * sends us a READ_CAPACITY and a READ command for block 0 (MBR). This causes 2 problems:
     *
     * - Because we do not execute SpdIoctlTransact we never complete the READ SRB.
     * - For some reason our SCSI ioctls fail with ERROR_INVALID_FUNCTION.
     */
    //memcpy(StorageUnitParams.ProductId, "TestDisk        ", 16);
    //memcpy(StorageUnitParams.ProductRevisionLevel, "0.1 ", 4);
    StorageUnitParams.BlockCount = 16;
    StorageUnitParams.BlockLength = 512;
    StorageUnitParams.MaxTransferLength = 512;
    StorageUnitParams.UnmapSupported = !!UnmapSupported;
    Error = SpdIoctlProvision(DeviceHandle, &StorageUnitParams, &Btl);
    ASSERT(ERROR_SUCCESS == Error);
    ASSERT(0 == Btl);

    Error = SpdIoctlScsiInquiry(DeviceHandle, Btl, 0, 3000);
    ASSERT(ERROR_SUCCESS == Error);

    CDB Cdb;
    UINT8 DataBuffer[255];
    UINT32 DataLength;
    UCHAR ScsiStatus;
    union
    {
        SENSE_DATA Data;
        UCHAR Buffer[32];
    } Sense;

    {
        memset(&Cdb, 0, sizeof Cdb);
        Cdb.CDB10.OperationCode = SCSIOP_READ_CAPACITY;

        DataLength = sizeof DataBuffer;
        Error = SpdIoctlScsiExecute(DeviceHandle, Btl, &Cdb, +1, DataBuffer, &DataLength,
            &ScsiStatus, Sense.Buffer);
        ASSERT(ERROR_SUCCESS == Error);

        PREAD_CAPACITY_DATA ReadCapacityData = (PVOID)DataBuffer;
        ASSERT(StorageUnitParams.BlockCount - 1 == (
            (((PUINT8)&ReadCapacityData->LogicalBlockAddress)[0] << 24) |
            (((PUINT8)&ReadCapacityData->LogicalBlockAddress)[1] << 16) |
            (((PUINT8)&ReadCapacityData->LogicalBlockAddress)[2] << 8) |
            (((PUINT8)&ReadCapacityData->LogicalBlockAddress)[3])));
        ASSERT(StorageUnitParams.BlockLength == (
            (((PUINT8)&ReadCapacityData->BytesPerBlock)[0] << 24) |
            (((PUINT8)&ReadCapacityData->BytesPerBlock)[1] << 16) |
            (((PUINT8)&ReadCapacityData->BytesPerBlock)[2] << 8) |
            (((PUINT8)&ReadCapacityData->BytesPerBlock)[3])));
    }

    {
        memset(&Cdb, 0, sizeof Cdb);
        Cdb.READ_CAPACITY16.OperationCode = SCSIOP_SERVICE_ACTION_IN16;
        Cdb.READ_CAPACITY16.ServiceAction = SERVICE_ACTION_READ_CAPACITY16;
        Cdb.READ_CAPACITY16.AllocationLength[3] = 255;

        DataLength = sizeof DataBuffer;
        Error = SpdIoctlScsiExecute(DeviceHandle, Btl, &Cdb, +1, DataBuffer, &DataLength,
            &ScsiStatus, Sense.Buffer);
        ASSERT(ERROR_SUCCESS == Error);

        PREAD_CAPACITY16_DATA ReadCapacityData = (PVOID)DataBuffer;
        ASSERT(StorageUnitParams.BlockCount - 1 == (
            ((UINT64)((PUINT8)&ReadCapacityData->LogicalBlockAddress)[0] << 56) |
            ((UINT64)((PUINT8)&ReadCapacityData->LogicalBlockAddress)[1] << 48) |
            ((UINT64)((PUINT8)&ReadCapacityData->LogicalBlockAddress)[2] << 40) |
            ((UINT64)((PUINT8)&ReadCapacityData->LogicalBlockAddress)[3] << 32) |
            ((UINT64)((PUINT8)&ReadCapacityData->LogicalBlockAddress)[4] << 24) |
            ((UINT64)((PUINT8)&ReadCapacityData->LogicalBlockAddress)[5] << 16) |
            ((UINT64)((PUINT8)&ReadCapacityData->LogicalBlockAddress)[6] << 8) |
            ((UINT64)((PUINT8)&ReadCapacityData->LogicalBlockAddress)[7])));
        ASSERT(StorageUnitParams.BlockLength == (
            (((PUINT8)&ReadCapacityData->BytesPerBlock)[0] << 24) |
            (((PUINT8)&ReadCapacityData->BytesPerBlock)[1] << 16) |
            (((PUINT8)&ReadCapacityData->BytesPerBlock)[2] << 8) |
            (((PUINT8)&ReadCapacityData->BytesPerBlock)[3])));
        ASSERT((UnmapSupported ? 1 : 0) == ReadCapacityData->LBPME);
    }

    Error = SpdIoctlUnprovision(DeviceHandle, &StorageUnitParams.Guid);
    ASSERT(ERROR_SUCCESS == Error);

    Success = CloseHandle(DeviceHandle);
    ASSERT(Success);
}

static void scsi_read_capacity_test(void)
{
    scsi_read_capacity_dotest(FALSE);
    scsi_read_capacity_dotest(TRUE);
}

void scsi_tests(void)
{
    TEST(scsi_inquiry_test);
    TEST(scsi_mode_sense_test);
    TEST(scsi_read_capacity_test);
}
