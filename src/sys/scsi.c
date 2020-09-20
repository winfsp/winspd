/**
 * @file sys/scsi.c
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

#include <sys/driver.h>

static UCHAR SpdScsiReportLuns(PVOID DeviceExtension, SPD_STORAGE_UNIT *StorageUnit,
    PVOID Srb, PCDB Cdb);
static UCHAR SpdScsiInquiry(PVOID DeviceExtension, SPD_STORAGE_UNIT *StorageUnit,
    PVOID Srb, PCDB Cdb);
static UCHAR SpdScsiModeSense(PVOID DeviceExtension, SPD_STORAGE_UNIT *StorageUnit,
    PVOID Srb, PCDB Cdb);
static UCHAR SpdScsiReadCapacity(PVOID DeviceExtension, SPD_STORAGE_UNIT *StorageUnit,
    PVOID Srb, PCDB Cdb);
static UCHAR SpdScsiPostRangeSrb(PVOID DeviceExtension, SPD_STORAGE_UNIT *StorageUnit,
    PVOID Srb, PCDB Cdb);
static UCHAR SpdScsiPostUnmapSrb(PVOID DeviceExtension, SPD_STORAGE_UNIT *StorageUnit,
    PVOID Srb, PCDB Cdb);
static UCHAR SpdScsiPostSrb(PVOID DeviceExtension, SPD_STORAGE_UNIT *StorageUnit,
    PVOID Srb, ULONG DataLength);
static UCHAR SpdScsiErrorEx(PVOID Srb,
    UCHAR SenseKey,
    UCHAR AdditionalSenseCode,
    UCHAR AdditionalSenseCodeQualifier,
    PUINT64 PInformation);
static VOID SpdCdbGetRange(PCDB Cdb,
    PUINT64 POffset, PUINT32 PLength, PUINT32 PForceUnitAccess);

#define SpdScsiError(S,K,A)             SpdScsiErrorEx(S,K,A,0,0)

UCHAR SpdSrbExecuteScsi(PVOID DeviceExtension, PVOID Srb)
{
    ASSERT(DISPATCH_LEVEL >= KeGetCurrentIrql());

    PCDB Cdb;
    SPD_STORAGE_UNIT *StorageUnit = 0;
    UCHAR SrbStatus;

    Cdb = SrbGetCdb(Srb);

    StorageUnit = SpdStorageUnitReference(DeviceExtension, Srb);
    if (0 == StorageUnit)
    {
        if (SCSIOP_REPORT_LUNS == Cdb->AsByte[0])
            SrbStatus = SpdScsiReportLuns(DeviceExtension, 0, Srb, Cdb);
        else
            SrbStatus = SRB_STATUS_NO_DEVICE;
        goto exit;
    }

    switch (Cdb->AsByte[0])
    {
    case SCSIOP_REPORT_LUNS:
        SrbStatus = SpdScsiReportLuns(DeviceExtension, StorageUnit, Srb, Cdb);
        break;

    case SCSIOP_TEST_UNIT_READY:
        SrbStatus = SRB_STATUS_SUCCESS;
        break;

    case SCSIOP_INQUIRY:
        SrbStatus = SpdScsiInquiry(DeviceExtension, StorageUnit, Srb, Cdb);
        break;

    case SCSIOP_MODE_SENSE:
    case SCSIOP_MODE_SENSE10:
        SrbStatus = SpdScsiModeSense(DeviceExtension, StorageUnit, Srb, Cdb);
        break;

    case SCSIOP_READ_CAPACITY:
        SrbStatus = SpdScsiReadCapacity(DeviceExtension, StorageUnit, Srb, Cdb);
        break;

    case SCSIOP_READ6:
    case SCSIOP_READ:
    case SCSIOP_READ12:
    case SCSIOP_READ16:
    case SCSIOP_WRITE6:
    case SCSIOP_WRITE:
    case SCSIOP_WRITE12:
    case SCSIOP_WRITE16:
    case SCSIOP_SYNCHRONIZE_CACHE:
    case SCSIOP_SYNCHRONIZE_CACHE16:
        SrbStatus = SpdScsiPostRangeSrb(DeviceExtension, StorageUnit, Srb, Cdb);
        break;

    case SCSIOP_UNMAP:
        SrbStatus = SpdScsiPostUnmapSrb(DeviceExtension, StorageUnit, Srb, Cdb);
        break;

    case SCSIOP_SERVICE_ACTION_IN16:
        if (SERVICE_ACTION_READ_CAPACITY16 == Cdb->READ_CAPACITY16.ServiceAction)
        {
            SrbStatus = SpdScsiReadCapacity(DeviceExtension, StorageUnit, Srb, Cdb);
            break;
        }
        /* fall through */

    default:
        SrbStatus = SRB_STATUS_INVALID_REQUEST;
        break;
    }

exit:
    if (0 != StorageUnit)
        SpdStorageUnitDereference(DeviceExtension, StorageUnit);

    return SrbStatus;
}

static UCHAR SpdScsiReportLuns(PVOID DeviceExtension, SPD_STORAGE_UNIT *StorageUnit,
    PVOID Srb, PCDB Cdb)
{
    PVOID DataBuffer = SrbGetDataBuffer(Srb);
    ULONG DataTransferLength = SrbGetDataTransferLength(Srb);
    PLUN_LIST LunList;
    ULONG Length;

    if (0 == DataBuffer)
        return SRB_STATUS_INTERNAL_ERROR;

    RtlZeroMemory(DataBuffer, DataTransferLength);

    LunList = DataBuffer;
    if (0 != StorageUnit)
    {
        Length = RTL_FIELD_SIZE(LUN_LIST, Lun[0]);

        if (sizeof(LUN_LIST) + Length > DataTransferLength)
            return SRB_STATUS_DATA_OVERRUN;

        // Our Lun is always 0. See RtlZeroMemory above.
        // LunList->Lun[0][0] = 0;
        // LunList->Lun[0][1] = 0;
        // LunList->Lun[0][2] = 0;
        // LunList->Lun[0][3] = 0;
        // LunList->Lun[0][4] = 0;
        // LunList->Lun[0][5] = 0;
        // LunList->Lun[0][6] = 0;
        // LunList->Lun[0][7] = 0;
    }
    else
    {
        Length = 0;

        if (sizeof(LUN_LIST) + Length > DataTransferLength)
            return SRB_STATUS_DATA_OVERRUN;
    }

    LunList->LunListLength[0] = (Length >> 24) & 0xff;
    LunList->LunListLength[1] = (Length >> 16) & 0xff;
    LunList->LunListLength[2] = (Length >> 8) & 0xff;
    LunList->LunListLength[3] = Length & 0xff;

    SrbSetDataTransferLength(Srb, sizeof(LUN_LIST) + Length);

    return SRB_STATUS_SUCCESS;
}

static UCHAR SpdScsiInquiry(PVOID DeviceExtension, SPD_STORAGE_UNIT *StorageUnit,
    PVOID Srb, PCDB Cdb)
{
    PVOID DataBuffer = SrbGetDataBuffer(Srb);
    ULONG DataTransferLength = SrbGetDataTransferLength(Srb);

    if (0 == DataBuffer)
        return SRB_STATUS_INTERNAL_ERROR;

    RtlZeroMemory(DataBuffer, DataTransferLength);

    if (0 == Cdb->CDB6INQUIRY3.EnableVitalProductData)
    {
        if (0 != Cdb->CDB6INQUIRY3.PageCode)
            return SpdScsiError(Srb, SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ADSENSE_INVALID_CDB);

        if (INQUIRYDATABUFFERSIZE > DataTransferLength)
            return SRB_STATUS_DATA_OVERRUN;

        PINQUIRYDATA InquiryData = DataBuffer;
        InquiryData->DeviceType = StorageUnit->StorageUnitParams.DeviceType;
        InquiryData->DeviceTypeQualifier = DEVICE_QUALIFIER_ACTIVE;
        InquiryData->RemovableMedia = 0;
        InquiryData->Versions = 5; /* "The device complies to the standard." */
        InquiryData->ResponseDataFormat = 2;
        InquiryData->CommandQueue = 1;
        InquiryData->AdditionalLength = INQUIRYDATABUFFERSIZE -
            RTL_SIZEOF_THROUGH_FIELD(INQUIRYDATA, AdditionalLength);
        RtlCopyMemory(InquiryData->VendorId, SPD_IOCTL_VENDOR_ID,
            sizeof SPD_IOCTL_VENDOR_ID - 1);
        RtlCopyMemory(InquiryData->ProductId, StorageUnit->StorageUnitParams.ProductId,
            sizeof StorageUnit->StorageUnitParams.ProductId);
        RtlCopyMemory(InquiryData->ProductRevisionLevel, StorageUnit->StorageUnitParams.ProductRevisionLevel,
            sizeof StorageUnit->StorageUnitParams.ProductRevisionLevel);

        SrbSetDataTransferLength(Srb, INQUIRYDATABUFFERSIZE);

        return SRB_STATUS_SUCCESS;
    }
    else
    {
        PVPD_SUPPORTED_PAGES_PAGE SupportedPages;
        PVPD_SERIAL_NUMBER_PAGE SerialNumber;
        PVPD_IDENTIFICATION_PAGE Identification;
        PVPD_IDENTIFICATION_DESCRIPTOR IdentificationDescriptor;
        PVPD_BLOCK_LIMITS_PAGE BlockLimits;
        PVPD_LOGICAL_BLOCK_PROVISIONING_PAGE LogicalBlockProvisioning;
        UINT32 U32;
        enum
        {
            PageCount = 5,
            Identifier0Length =
                sizeof SPD_IOCTL_VENDOR_ID - 1 +
                sizeof StorageUnit->StorageUnitParams.ProductId +
                sizeof StorageUnit->StorageUnitParams.ProductRevisionLevel +
                sizeof StorageUnit->SerialNumber,
            Identifier1Length = 8,
        };

        switch (Cdb->CDB6INQUIRY3.PageCode)
        {
        case VPD_SUPPORTED_PAGES:
            if (sizeof(VPD_SUPPORTED_PAGES_PAGE) + PageCount > DataTransferLength)
                return SRB_STATUS_DATA_OVERRUN;

            SupportedPages = DataBuffer;
            SupportedPages->DeviceType = StorageUnit->StorageUnitParams.DeviceType;
            SupportedPages->DeviceTypeQualifier = DEVICE_QUALIFIER_ACTIVE;
            SupportedPages->PageCode = VPD_SUPPORTED_PAGES;
            SupportedPages->PageLength = PageCount;
            SupportedPages->SupportedPageList[0] = VPD_SUPPORTED_PAGES;
            SupportedPages->SupportedPageList[1] = VPD_SERIAL_NUMBER;
            SupportedPages->SupportedPageList[2] = VPD_DEVICE_IDENTIFIERS;
            SupportedPages->SupportedPageList[3] = VPD_BLOCK_LIMITS;
            SupportedPages->SupportedPageList[4] = VPD_LOGICAL_BLOCK_PROVISIONING;

            SrbSetDataTransferLength(Srb, sizeof(VPD_SUPPORTED_PAGES_PAGE) + PageCount);

            return SRB_STATUS_SUCCESS;

        case VPD_SERIAL_NUMBER:
            if (sizeof(VPD_SERIAL_NUMBER_PAGE) +
                sizeof StorageUnit->SerialNumber > DataTransferLength)
                return SRB_STATUS_DATA_OVERRUN;

            SerialNumber = DataBuffer;
            SerialNumber->DeviceType = StorageUnit->StorageUnitParams.DeviceType;
            SerialNumber->DeviceTypeQualifier = DEVICE_QUALIFIER_ACTIVE;
            SerialNumber->PageCode = VPD_SERIAL_NUMBER;
            SerialNumber->PageLength = sizeof StorageUnit->SerialNumber;
            RtlCopyMemory(SerialNumber->SerialNumber, &StorageUnit->SerialNumber,
                sizeof StorageUnit->SerialNumber);

            SrbSetDataTransferLength(Srb, sizeof(VPD_SERIAL_NUMBER_PAGE) +
                sizeof StorageUnit->SerialNumber);

            return SRB_STATUS_SUCCESS;

        case VPD_DEVICE_IDENTIFIERS:
            if (sizeof(VPD_IDENTIFICATION_PAGE) +
                sizeof(VPD_IDENTIFICATION_DESCRIPTOR) + Identifier0Length +
                sizeof(VPD_IDENTIFICATION_DESCRIPTOR) + Identifier1Length > DataTransferLength)
                return SRB_STATUS_DATA_OVERRUN;

            Identification = DataBuffer;
            Identification->DeviceType = StorageUnit->StorageUnitParams.DeviceType;
            Identification->DeviceTypeQualifier = DEVICE_QUALIFIER_ACTIVE;
            Identification->PageCode = VPD_DEVICE_IDENTIFIERS;
            Identification->PageLength =
                sizeof(VPD_IDENTIFICATION_DESCRIPTOR) + Identifier0Length +
                sizeof(VPD_IDENTIFICATION_DESCRIPTOR) + Identifier1Length;

            IdentificationDescriptor = (PVPD_IDENTIFICATION_DESCRIPTOR)Identification->Descriptors;
            IdentificationDescriptor->CodeSet = VpdCodeSetAscii;
            IdentificationDescriptor->IdentifierType = VpdIdentifierTypeVendorId;
            IdentificationDescriptor->Association = VpdAssocDevice;
            IdentificationDescriptor->IdentifierLength = Identifier0Length;
            RtlCopyMemory(IdentificationDescriptor->Identifier,
                SPD_IOCTL_VENDOR_ID,
                sizeof SPD_IOCTL_VENDOR_ID - 1);
            RtlCopyMemory(IdentificationDescriptor->Identifier +
                    sizeof SPD_IOCTL_VENDOR_ID - 1,
                StorageUnit->StorageUnitParams.ProductId,
                sizeof StorageUnit->StorageUnitParams.ProductId);
            RtlCopyMemory(IdentificationDescriptor->Identifier +
                    sizeof SPD_IOCTL_VENDOR_ID - 1 +
                    sizeof StorageUnit->StorageUnitParams.ProductId,
                StorageUnit->StorageUnitParams.ProductRevisionLevel,
                sizeof StorageUnit->StorageUnitParams.ProductRevisionLevel);
            RtlCopyMemory(IdentificationDescriptor->Identifier +
                    sizeof SPD_IOCTL_VENDOR_ID - 1 +
                    sizeof StorageUnit->StorageUnitParams.ProductId +
                    sizeof StorageUnit->StorageUnitParams.ProductRevisionLevel,
                &StorageUnit->SerialNumber,
                sizeof StorageUnit->SerialNumber);

            IdentificationDescriptor = (PVOID)((PUINT8)IdentificationDescriptor +
                sizeof(VPD_IDENTIFICATION_DESCRIPTOR) + Identifier0Length);
            IdentificationDescriptor->CodeSet = VpdCodeSetBinary;
            IdentificationDescriptor->IdentifierType = VpdIdentifierTypeVendorSpecific;
            IdentificationDescriptor->Association = VpdAssocDevice;
            IdentificationDescriptor->IdentifierLength = Identifier1Length;
            U32 = StorageUnit->OwnerProcessId;
            IdentificationDescriptor->Identifier[0] = 'P';
            IdentificationDescriptor->Identifier[1] = 'I';
            IdentificationDescriptor->Identifier[2] = 'D';
            IdentificationDescriptor->Identifier[3] =
                StorageUnit->StorageUnitParams.EjectDisabled ? 'X' : ' ';
            IdentificationDescriptor->Identifier[4] = (U32 >> 24) & 0xff;
            IdentificationDescriptor->Identifier[5] = (U32 >> 16) & 0xff;
            IdentificationDescriptor->Identifier[6] = (U32 >> 8) & 0xff;
            IdentificationDescriptor->Identifier[7] = U32 & 0xff;

            SrbSetDataTransferLength(Srb, sizeof(VPD_IDENTIFICATION_PAGE) +
                sizeof(VPD_IDENTIFICATION_DESCRIPTOR) + Identifier0Length +
                sizeof(VPD_IDENTIFICATION_DESCRIPTOR) + Identifier1Length);

            return SRB_STATUS_SUCCESS;

        case VPD_BLOCK_LIMITS:
            if (sizeof(VPD_BLOCK_LIMITS_PAGE) > DataTransferLength)
                return SRB_STATUS_DATA_OVERRUN;

            BlockLimits = DataBuffer;
            BlockLimits->DeviceType = StorageUnit->StorageUnitParams.DeviceType;
            BlockLimits->DeviceTypeQualifier = DEVICE_QUALIFIER_ACTIVE;
            BlockLimits->PageCode = VPD_BLOCK_LIMITS;
            BlockLimits->PageLength[1] = sizeof(VPD_BLOCK_LIMITS_PAGE) -
                RTL_SIZEOF_THROUGH_FIELD(VPD_BLOCK_LIMITS_PAGE, PageLength);
            U32 = StorageUnit->StorageUnitParams.MaxTransferLength /
                StorageUnit->StorageUnitParams.BlockLength;
            BlockLimits->MaximumTransferLength[0] = (U32 >> 24) & 0xff;
            BlockLimits->MaximumTransferLength[1] = (U32 >> 16) & 0xff;
            BlockLimits->MaximumTransferLength[2] = (U32 >> 8) & 0xff;
            BlockLimits->MaximumTransferLength[3] = U32 & 0xff;
            if (StorageUnit->StorageUnitParams.UnmapSupported)
            {
                BlockLimits->MaximumUnmapLBACount[0] = 0xff;
                BlockLimits->MaximumUnmapLBACount[1] = 0xff;
                BlockLimits->MaximumUnmapLBACount[2] = 0xff;
                BlockLimits->MaximumUnmapLBACount[3] = 0xff;
                U32 = StorageUnit->StorageUnitParams.MaxTransferLength /
                    sizeof(UNMAP_BLOCK_DESCRIPTOR);
                BlockLimits->MaximumUnmapBlockDescriptorCount[0] = (U32 >> 24) & 0xff;
                BlockLimits->MaximumUnmapBlockDescriptorCount[1] = (U32 >> 16) & 0xff;
                BlockLimits->MaximumUnmapBlockDescriptorCount[2] = (U32 >> 8) & 0xff;
                BlockLimits->MaximumUnmapBlockDescriptorCount[3] = U32 & 0xff;
            }

            SrbSetDataTransferLength(Srb, sizeof(VPD_BLOCK_LIMITS_PAGE));

            return SRB_STATUS_SUCCESS;

        case VPD_LOGICAL_BLOCK_PROVISIONING:
            if (sizeof(VPD_LOGICAL_BLOCK_PROVISIONING_PAGE) > DataTransferLength)
                return SRB_STATUS_DATA_OVERRUN;

            LogicalBlockProvisioning = DataBuffer;
            LogicalBlockProvisioning->DeviceType = StorageUnit->StorageUnitParams.DeviceType;
            LogicalBlockProvisioning->DeviceTypeQualifier = DEVICE_QUALIFIER_ACTIVE;
            LogicalBlockProvisioning->PageCode = VPD_LOGICAL_BLOCK_PROVISIONING;
            LogicalBlockProvisioning->PageLength[1] = sizeof(VPD_LOGICAL_BLOCK_PROVISIONING_PAGE) -
                RTL_SIZEOF_THROUGH_FIELD(VPD_LOGICAL_BLOCK_PROVISIONING_PAGE, PageLength);
            if (StorageUnit->StorageUnitParams.UnmapSupported)
            {
                LogicalBlockProvisioning->LBPU = 1;
                LogicalBlockProvisioning->ProvisioningType = PROVISIONING_TYPE_THIN;
            }

            SrbSetDataTransferLength(Srb, sizeof(VPD_LOGICAL_BLOCK_PROVISIONING_PAGE));

            return SRB_STATUS_SUCCESS;

        default:
            return SpdScsiError(Srb, SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ADSENSE_INVALID_CDB);
        }
    }
}

static UCHAR SpdScsiModeSense(PVOID DeviceExtension, SPD_STORAGE_UNIT *StorageUnit,
    PVOID Srb, PCDB Cdb)
{
    PVOID DataBuffer = SrbGetDataBuffer(Srb);
    ULONG DataTransferLength = SrbGetDataTransferLength(Srb);

    if (0 == DataBuffer)
        return SRB_STATUS_INTERNAL_ERROR;

    RtlZeroMemory(DataBuffer, DataTransferLength);

    PMODE_CACHING_PAGE ModeCachingPage;
    ULONG DataLength;
    if (SCSIOP_MODE_SENSE == Cdb->AsByte[0])
    {
        /* MODE SENSE (6) */
        if (MODE_SENSE_CHANGEABLE_VALUES == Cdb->MODE_SENSE.Pc ||
            (MODE_PAGE_CACHING != Cdb->MODE_SENSE.PageCode &&
            MODE_SENSE_RETURN_ALL != Cdb->MODE_SENSE.PageCode))
            return SpdScsiError(Srb, SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ADSENSE_INVALID_CDB);

        DataLength = sizeof(MODE_PARAMETER_HEADER) + sizeof(MODE_CACHING_PAGE);
        if (DataLength > DataTransferLength)
            return SRB_STATUS_DATA_OVERRUN;

        PMODE_PARAMETER_HEADER ModeParameterHeader = DataBuffer;
        ModeCachingPage = (PVOID)(ModeParameterHeader + 1);

        ModeParameterHeader->ModeDataLength = (UCHAR)(DataLength -
            RTL_SIZEOF_THROUGH_FIELD(MODE_PARAMETER_HEADER, ModeDataLength));
        ModeParameterHeader->MediumType = 0;
        ModeParameterHeader->DeviceSpecificParameter =
            (StorageUnit->StorageUnitParams.WriteProtected ? MODE_DSP_WRITE_PROTECT : 0) |
            (StorageUnit->StorageUnitParams.CacheSupported ? MODE_DSP_FUA_SUPPORTED : 0);
        ModeParameterHeader->BlockDescriptorLength = 0;
    }
    else
    {
        /* MODE SENSE (10) */
        if (MODE_SENSE_CHANGEABLE_VALUES == Cdb->MODE_SENSE10.Pc ||
            (MODE_PAGE_CACHING != Cdb->MODE_SENSE10.PageCode &&
            MODE_SENSE_RETURN_ALL != Cdb->MODE_SENSE10.PageCode))
            return SpdScsiError(Srb, SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ADSENSE_INVALID_CDB);

        DataLength = sizeof(MODE_PARAMETER_HEADER10) + sizeof(MODE_CACHING_PAGE);
        if (DataLength > DataTransferLength)
            return SRB_STATUS_DATA_OVERRUN;

        PMODE_PARAMETER_HEADER10 ModeParameterHeader = DataBuffer;
        ModeCachingPage = (PVOID)(ModeParameterHeader + 1);

        ModeParameterHeader->ModeDataLength[0] = 0;
        ModeParameterHeader->ModeDataLength[1] = (UCHAR)(DataLength -
            RTL_SIZEOF_THROUGH_FIELD(MODE_PARAMETER_HEADER10, ModeDataLength));
        ModeParameterHeader->MediumType = 0;
        ModeParameterHeader->DeviceSpecificParameter =
            (StorageUnit->StorageUnitParams.WriteProtected ? MODE_DSP_WRITE_PROTECT : 0) |
            (StorageUnit->StorageUnitParams.CacheSupported ? MODE_DSP_FUA_SUPPORTED : 0);
        ModeParameterHeader->BlockDescriptorLength[0] = 0;
        ModeParameterHeader->BlockDescriptorLength[1] = 0;
    }

    ModeCachingPage->PageCode = MODE_PAGE_CACHING;
    ModeCachingPage->PageSavable = 0;
    ModeCachingPage->PageLength = sizeof(MODE_CACHING_PAGE) -
        RTL_SIZEOF_THROUGH_FIELD(MODE_CACHING_PAGE, PageLength);
    ModeCachingPage->ReadDisableCache = !StorageUnit->StorageUnitParams.CacheSupported;
    ModeCachingPage->WriteCacheEnable = !!StorageUnit->StorageUnitParams.CacheSupported;

    SrbSetDataTransferLength(Srb, DataLength);

    return SRB_STATUS_SUCCESS;
}

static UCHAR SpdScsiReadCapacity(PVOID DeviceExtension, SPD_STORAGE_UNIT *StorageUnit,
    PVOID Srb, PCDB Cdb)
{
    PVOID DataBuffer = SrbGetDataBuffer(Srb);
    ULONG DataTransferLength = SrbGetDataTransferLength(Srb);

    if (0 == DataBuffer)
        return SRB_STATUS_INTERNAL_ERROR;

    RtlZeroMemory(DataBuffer, DataTransferLength);

    if (SCSIOP_READ_CAPACITY == Cdb->AsByte[0])
    {
        /* READ CAPACITY (10) */
        if (sizeof(READ_CAPACITY_DATA) > DataTransferLength)
            return SRB_STATUS_DATA_OVERRUN;

        PREAD_CAPACITY_DATA ReadCapacityData = DataBuffer;
        UINT32 U32;
        U32 = 0x100000000ULL > StorageUnit->StorageUnitParams.BlockCount ?
            (UINT32)StorageUnit->StorageUnitParams.BlockCount - 1 : 0xffffffff;
        ((PUINT8)&ReadCapacityData->LogicalBlockAddress)[0] = (U32 >> 24) & 0xff;
        ((PUINT8)&ReadCapacityData->LogicalBlockAddress)[1] = (U32 >> 16) & 0xff;
        ((PUINT8)&ReadCapacityData->LogicalBlockAddress)[2] = (U32 >> 8) & 0xff;
        ((PUINT8)&ReadCapacityData->LogicalBlockAddress)[3] = U32 & 0xff;
        U32 = StorageUnit->StorageUnitParams.BlockLength;
        ((PUINT8)&ReadCapacityData->BytesPerBlock)[0] = (U32 >> 24) & 0xff;
        ((PUINT8)&ReadCapacityData->BytesPerBlock)[1] = (U32 >> 16) & 0xff;
        ((PUINT8)&ReadCapacityData->BytesPerBlock)[2] = (U32 >> 8) & 0xff;
        ((PUINT8)&ReadCapacityData->BytesPerBlock)[3] = U32 & 0xff;

        SrbSetDataTransferLength(Srb, sizeof(READ_CAPACITY_DATA));

        return SRB_STATUS_SUCCESS;
    }
    else
    {
        /* READ CAPACITY (16) */
        if (sizeof(READ_CAPACITY_DATA_EX) > DataTransferLength)
            return SRB_STATUS_DATA_OVERRUN;

        PREAD_CAPACITY_DATA_EX ReadCapacityData = DataBuffer;
        UINT64 U64;
        UINT32 U32;
        U64 = StorageUnit->StorageUnitParams.BlockCount - 1;
        ((PUINT8)&ReadCapacityData->LogicalBlockAddress)[0] = (U64 >> 56) & 0xff;
        ((PUINT8)&ReadCapacityData->LogicalBlockAddress)[1] = (U64 >> 48) & 0xff;
        ((PUINT8)&ReadCapacityData->LogicalBlockAddress)[2] = (U64 >> 40) & 0xff;
        ((PUINT8)&ReadCapacityData->LogicalBlockAddress)[3] = (U64 >> 32) & 0xff;
        ((PUINT8)&ReadCapacityData->LogicalBlockAddress)[4] = (U64 >> 24) & 0xff;
        ((PUINT8)&ReadCapacityData->LogicalBlockAddress)[5] = (U64 >> 16) & 0xff;
        ((PUINT8)&ReadCapacityData->LogicalBlockAddress)[6] = (U64 >> 8) & 0xff;
        ((PUINT8)&ReadCapacityData->LogicalBlockAddress)[7] = U64 & 0xff;
        U32 = StorageUnit->StorageUnitParams.BlockLength;
        ((PUINT8)&ReadCapacityData->BytesPerBlock)[0] = (U32 >> 24) & 0xff;
        ((PUINT8)&ReadCapacityData->BytesPerBlock)[1] = (U32 >> 16) & 0xff;
        ((PUINT8)&ReadCapacityData->BytesPerBlock)[2] = (U32 >> 8) & 0xff;
        ((PUINT8)&ReadCapacityData->BytesPerBlock)[3] = U32 & 0xff;

        ULONG DataLength;
        if (sizeof(READ_CAPACITY16_DATA) <= DataTransferLength)
        {
            if (StorageUnit->StorageUnitParams.UnmapSupported)
                ((PREAD_CAPACITY16_DATA)ReadCapacityData)->LBPME = 1;
            DataLength = sizeof(READ_CAPACITY16_DATA);
        }
        else
            DataLength = sizeof(READ_CAPACITY_DATA_EX);

        SrbSetDataTransferLength(Srb, DataLength);

        return SRB_STATUS_SUCCESS;
    }
}

static UCHAR SpdScsiPostRangeSrb(PVOID DeviceExtension, SPD_STORAGE_UNIT *StorageUnit,
    PVOID Srb, PCDB Cdb)
{
    UINT64 BlockAddress, EndBlockAddress;
    UINT32 BlockCount;
    ULONG DataLength;

    switch (Cdb->AsByte[0])
    {
    case SCSIOP_READ6:
    case SCSIOP_READ:
    case SCSIOP_READ12:
    case SCSIOP_READ16:
        SpdCdbGetRange(Cdb, &BlockAddress, &BlockCount, 0);
        DataLength = BlockCount * StorageUnit->StorageUnitParams.BlockLength;
        if (SrbGetDataTransferLength(Srb) < DataLength)
            return SRB_STATUS_INTERNAL_ERROR;
        break;

    case SCSIOP_WRITE6:
    case SCSIOP_WRITE:
    case SCSIOP_WRITE12:
    case SCSIOP_WRITE16:
        if (StorageUnit->StorageUnitParams.WriteProtected)
            return SpdScsiError(Srb, SCSI_SENSE_DATA_PROTECT, SCSI_ADSENSE_WRITE_PROTECT);
        SpdCdbGetRange(Cdb, &BlockAddress, &BlockCount, 0);
        DataLength = BlockCount * StorageUnit->StorageUnitParams.BlockLength;
        if (SrbGetDataTransferLength(Srb) < DataLength)
            return SRB_STATUS_INTERNAL_ERROR;
        break;

    case SCSIOP_SYNCHRONIZE_CACHE:
    case SCSIOP_SYNCHRONIZE_CACHE16:
        if (!StorageUnit->StorageUnitParams.CacheSupported)
            return SRB_STATUS_INVALID_REQUEST;
        if (StorageUnit->StorageUnitParams.WriteProtected)
            return SpdScsiError(Srb, SCSI_SENSE_DATA_PROTECT, SCSI_ADSENSE_WRITE_PROTECT);
        SpdCdbGetRange(Cdb, &BlockAddress, &BlockCount, 0);
        DataLength = 0;
        break;

    default:
        ASSERT(FALSE);
        return SRB_STATUS_INVALID_REQUEST;
    }

    if (0 == BlockCount)
        return SRB_STATUS_SUCCESS;

    EndBlockAddress = BlockAddress + BlockCount;
    if (EndBlockAddress < BlockAddress ||
        EndBlockAddress > StorageUnit->StorageUnitParams.BlockCount)
        return SpdScsiError(Srb, SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ADSENSE_ILLEGAL_BLOCK);

    return SpdScsiPostSrb(DeviceExtension, StorageUnit, Srb, DataLength);
}

static UCHAR SpdScsiPostUnmapSrb(PVOID DeviceExtension, SPD_STORAGE_UNIT *StorageUnit,
    PVOID Srb, PCDB Cdb)
{
    if (!StorageUnit->StorageUnitParams.UnmapSupported)
        return SRB_STATUS_INVALID_REQUEST;
    if (StorageUnit->StorageUnitParams.WriteProtected)
        return SpdScsiError(Srb, SCSI_SENSE_DATA_PROTECT, SCSI_ADSENSE_WRITE_PROTECT);

    PUNMAP_LIST_HEADER DataBuffer = SrbGetDataBuffer(Srb);
    ULONG DataTransferLength = SrbGetDataTransferLength(Srb);
    ULONG DataLength;

    if (0 == DataBuffer ||
        DataTransferLength < sizeof(UNMAP_LIST_HEADER) ||
        DataTransferLength < sizeof(UNMAP_LIST_HEADER) + (DataLength =
            ((ULONG)DataBuffer->BlockDescrDataLength[0] << 8) | (ULONG)DataBuffer->BlockDescrDataLength[1]))
        return SRB_STATUS_INTERNAL_ERROR;

    if (DataLength > StorageUnit->StorageUnitParams.MaxTransferLength)
        return SpdScsiError(Srb, SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ADSENSE_INVALID_FIELD_PARAMETER_LIST);

    if (Cdb->UNMAP.Anchor)
        return SpdScsiError(Srb, SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ADSENSE_INVALID_CDB);

    if (0 == DataLength)
        return SRB_STATUS_SUCCESS;

    for (ULONG I = 0, N = DataLength / sizeof(UNMAP_BLOCK_DESCRIPTOR); N > I; I++)
    {
        PUNMAP_BLOCK_DESCRIPTOR Src = &DataBuffer->Descriptors[I];
        UINT64 BlockAddress =
            ((UINT64)Src->StartingLba[0] << 56) |
            ((UINT64)Src->StartingLba[1] << 48) |
            ((UINT64)Src->StartingLba[2] << 40) |
            ((UINT64)Src->StartingLba[3] << 32) |
            ((UINT64)Src->StartingLba[4] << 24) |
            ((UINT64)Src->StartingLba[5] << 16) |
            ((UINT64)Src->StartingLba[6] << 8) |
            ((UINT64)Src->StartingLba[7]);
        UINT32 BlockCount =
            ((UINT32)Src->LbaCount[0] << 24) |
            ((UINT32)Src->LbaCount[1] << 16) |
            ((UINT32)Src->LbaCount[2] << 8) |
            ((UINT32)Src->LbaCount[3]);
        UINT64 EndBlockAddress = BlockAddress + BlockCount;

        if (EndBlockAddress < BlockAddress ||
            EndBlockAddress > StorageUnit->StorageUnitParams.BlockCount)
            return SpdScsiError(Srb, SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ADSENSE_ILLEGAL_BLOCK);
    }

    return SpdScsiPostSrb(DeviceExtension, StorageUnit, Srb, DataLength);
}

static UCHAR SpdScsiPostSrb(PVOID DeviceExtension, SPD_STORAGE_UNIT *StorageUnit,
    PVOID Srb, ULONG DataLength)
{
    SPD_SRB_EXTENSION *SrbExtension;
    ULONG StorResult;
    NTSTATUS Result;

    SrbExtension = SpdSrbExtension(Srb);
    RtlZeroMemory(SrbExtension, sizeof(SPD_SRB_EXTENSION));
    SrbExtension->StorageUnit = StorageUnit;

    if (0 != DataLength)
    {
        StorResult = StorPortGetSystemAddress(DeviceExtension, Srb, &SrbExtension->SystemDataBuffer);
        if (STOR_STATUS_SUCCESS != StorResult)
        {
            SrbSetSystemStatus(Srb, SpdNtStatusFromStorStatus(StorResult));
            return SRB_STATUS_INTERNAL_ERROR;
        }
        SrbExtension->SystemDataLength = DataLength;
    }

    Result = SpdIoqPostSrb(StorageUnit->Ioq, Srb);
    return NT_SUCCESS(Result) ? SRB_STATUS_PENDING : SRB_STATUS_ABORTED;
}

VOID SpdSrbExecuteScsiPrepare(PVOID SrbExtension0, PVOID Context, PVOID DataBuffer)
{
    ASSERT(DISPATCH_LEVEL == KeGetCurrentIrql());

    SPD_SRB_EXTENSION *SrbExtension = SrbExtension0;
    SPD_STORAGE_UNIT *StorageUnit = SrbExtension->StorageUnit;
    SPD_IOCTL_TRANSACT_REQ *Req = Context;
    PVOID Srb = SrbExtension->Srb;
    PCDB Cdb;
    UINT32 ForceUnitAccess;
    ULONG ChunkLength;

    Cdb = SrbGetCdb(Srb);
    switch (Cdb->AsByte[0])
    {
    case SCSIOP_READ6:
    case SCSIOP_READ:
    case SCSIOP_READ12:
    case SCSIOP_READ16:
        Req->Hint = (UINT64)(UINT_PTR)SrbExtension;
        Req->Kind = SpdIoctlTransactReadKind;
        SpdCdbGetRange(Cdb,
            &Req->Op.Read.BlockAddress,
            &Req->Op.Read.BlockCount,
            &ForceUnitAccess);
        Req->Op.Read.ForceUnitAccess =
            StorageUnit->StorageUnitParams.CacheSupported ? ForceUnitAccess : 1;
        ChunkLength = SrbExtension->SystemDataLength - SrbExtension->ChunkOffset;
        if (ChunkLength > StorageUnit->StorageUnitParams.MaxTransferLength)
            ChunkLength = StorageUnit->StorageUnitParams.MaxTransferLength;
        Req->Op.Read.BlockAddress +=
            SrbExtension->ChunkOffset / StorageUnit->StorageUnitParams.BlockLength;
        Req->Op.Read.BlockCount =
            ChunkLength / StorageUnit->StorageUnitParams.BlockLength;
        return;

    case SCSIOP_WRITE6:
    case SCSIOP_WRITE:
    case SCSIOP_WRITE12:
    case SCSIOP_WRITE16:
        Req->Hint = (UINT64)(UINT_PTR)SrbExtension;
        Req->Kind = SpdIoctlTransactWriteKind;
        SpdCdbGetRange(Cdb,
            &Req->Op.Write.BlockAddress,
            &Req->Op.Write.BlockCount,
            &ForceUnitAccess);
        Req->Op.Write.ForceUnitAccess =
            StorageUnit->StorageUnitParams.CacheSupported ? ForceUnitAccess : 1;
        ChunkLength = SrbExtension->SystemDataLength - SrbExtension->ChunkOffset;
        if (ChunkLength > StorageUnit->StorageUnitParams.MaxTransferLength)
            ChunkLength = StorageUnit->StorageUnitParams.MaxTransferLength;
        Req->Op.Write.BlockAddress +=
            SrbExtension->ChunkOffset / StorageUnit->StorageUnitParams.BlockLength;
        Req->Op.Write.BlockCount =
            ChunkLength / StorageUnit->StorageUnitParams.BlockLength;
        RtlCopyMemory(DataBuffer,
            (PUINT8)SrbExtension->SystemDataBuffer + SrbExtension->ChunkOffset, ChunkLength);
        return;

    case SCSIOP_SYNCHRONIZE_CACHE:
    case SCSIOP_SYNCHRONIZE_CACHE16:
        Req->Hint = (UINT64)(UINT_PTR)SrbExtension;
        Req->Kind = SpdIoctlTransactFlushKind;
        SpdCdbGetRange(Cdb,
            &Req->Op.Flush.BlockAddress,
            &Req->Op.Flush.BlockCount,
            0);
        return;

    case SCSIOP_UNMAP:
        Req->Hint = (UINT64)(UINT_PTR)SrbExtension;
        Req->Kind = SpdIoctlTransactUnmapKind;
        Req->Op.Unmap.Count = SrbExtension->SystemDataLength / sizeof(UNMAP_BLOCK_DESCRIPTOR);
        for (ULONG I = 0, N = Req->Op.Unmap.Count; N > I; I++)
        {
            PUNMAP_BLOCK_DESCRIPTOR Src = &((PUNMAP_LIST_HEADER)SrbExtension->SystemDataBuffer)->Descriptors[I];
            SPD_IOCTL_UNMAP_DESCRIPTOR *Dst = (SPD_IOCTL_UNMAP_DESCRIPTOR *)DataBuffer + I;
            Dst->BlockAddress =
                ((UINT64)Src->StartingLba[0] << 56) |
                ((UINT64)Src->StartingLba[1] << 48) |
                ((UINT64)Src->StartingLba[2] << 40) |
                ((UINT64)Src->StartingLba[3] << 32) |
                ((UINT64)Src->StartingLba[4] << 24) |
                ((UINT64)Src->StartingLba[5] << 16) |
                ((UINT64)Src->StartingLba[6] << 8) |
                ((UINT64)Src->StartingLba[7]);
            Dst->BlockCount =
                ((UINT32)Src->LbaCount[0] << 24) |
                ((UINT32)Src->LbaCount[1] << 16) |
                ((UINT32)Src->LbaCount[2] << 8) |
                ((UINT32)Src->LbaCount[3]);
            Dst->Reserved = 0;
        }
        return;

    default:
        ASSERT(FALSE);
        return;
    }
}

UCHAR SpdSrbExecuteScsiComplete(PVOID SrbExtension0, PVOID Context, PVOID DataBuffer)
{
    ASSERT(DISPATCH_LEVEL == KeGetCurrentIrql());

    SPD_SRB_EXTENSION *SrbExtension = SrbExtension0;
    SPD_STORAGE_UNIT *StorageUnit = SrbExtension->StorageUnit;
    SPD_IOCTL_TRANSACT_RSP *Rsp = Context;
    PVOID Srb = SrbExtension->Srb;
    PCDB Cdb;
    ULONG ChunkLength;

    if (SCSISTAT_GOOD != Rsp->Status.ScsiStatus)
        return SpdScsiErrorEx(Srb,
            Rsp->Status.SenseKey,
            Rsp->Status.ASC,
            Rsp->Status.ASCQ,
            Rsp->Status.InformationValid ? &Rsp->Status.Information : 0);

    Cdb = SrbGetCdb(Srb);
    switch (Cdb->AsByte[0])
    {
    case SCSIOP_READ6:
    case SCSIOP_READ:
    case SCSIOP_READ12:
    case SCSIOP_READ16:
        ChunkLength = SrbExtension->SystemDataLength - SrbExtension->ChunkOffset;
        if (ChunkLength > StorageUnit->StorageUnitParams.MaxTransferLength)
            ChunkLength = StorageUnit->StorageUnitParams.MaxTransferLength;
        if (0 != DataBuffer)
            RtlCopyMemory((PUINT8)SrbExtension->SystemDataBuffer + SrbExtension->ChunkOffset,
                DataBuffer, ChunkLength);
        else
            RtlZeroMemory((PUINT8)SrbExtension->SystemDataBuffer + SrbExtension->ChunkOffset,
                ChunkLength);
        SrbExtension->ChunkOffset += ChunkLength;
        /* if we are done return SUCCESS; if we have more chunks return PENDING */
        return SrbExtension->ChunkOffset >= SrbExtension->SystemDataLength ?
            SRB_STATUS_SUCCESS : SRB_STATUS_PENDING;

    case SCSIOP_WRITE6:
    case SCSIOP_WRITE:
    case SCSIOP_WRITE12:
    case SCSIOP_WRITE16:
        ChunkLength = SrbExtension->SystemDataLength - SrbExtension->ChunkOffset;
        if (ChunkLength > StorageUnit->StorageUnitParams.MaxTransferLength)
            ChunkLength = StorageUnit->StorageUnitParams.MaxTransferLength;
        SrbExtension->ChunkOffset += ChunkLength;
        /* if we are done return SUCCESS; if we have more chunks return PENDING */
        return SrbExtension->ChunkOffset >= SrbExtension->SystemDataLength ?
            SRB_STATUS_SUCCESS : SRB_STATUS_PENDING;

    case SCSIOP_SYNCHRONIZE_CACHE:
    case SCSIOP_SYNCHRONIZE_CACHE16:
    case SCSIOP_UNMAP:
        return SRB_STATUS_SUCCESS;

    default:
        ASSERT(FALSE);
        return SRB_STATUS_ABORTED;
    }
}

static UCHAR SpdScsiErrorEx(PVOID Srb,
    UCHAR SenseKey,
    UCHAR AdditionalSenseCode,
    UCHAR AdditionalSenseCodeQualifier,
    PUINT64 PInformation)
{
    UCHAR SenseInfoBufferLength = SrbGetSenseInfoBufferLength(Srb);
    PSENSE_DATA SenseInfoBuffer = SrbGetSenseInfoBuffer(Srb);
    UCHAR SrbStatus = SRB_STATUS_ERROR;

    if (0 != SenseInfoBuffer &&
        sizeof(SENSE_DATA) <= SenseInfoBufferLength &&
        !FlagOn(SrbGetSrbFlags(Srb), SRB_FLAGS_DISABLE_AUTOSENSE))
    {
        RtlZeroMemory(SenseInfoBuffer, SenseInfoBufferLength);
        SenseInfoBuffer->ErrorCode = SCSI_SENSE_ERRORCODE_FIXED_CURRENT;
        SenseInfoBuffer->SenseKey = SenseKey;
        SenseInfoBuffer->AdditionalSenseCode = AdditionalSenseCode;
        SenseInfoBuffer->AdditionalSenseCodeQualifier = AdditionalSenseCodeQualifier;
        SenseInfoBuffer->AdditionalSenseLength = sizeof(SENSE_DATA) -
            RTL_SIZEOF_THROUGH_FIELD(SENSE_DATA, AdditionalSenseLength);
        if (0 != PInformation)
        {
            SenseInfoBuffer->Information[0] = (*PInformation >> 24) & 0xff;
            SenseInfoBuffer->Information[1] = (*PInformation >> 16) & 0xff;
            SenseInfoBuffer->Information[2] = (*PInformation >> 8) & 0xff;
            SenseInfoBuffer->Information[3] = *PInformation & 0xff;
            SenseInfoBuffer->Valid = 1;
        }

        SrbSetScsiStatus(Srb, SCSISTAT_CHECK_CONDITION);

        SrbStatus |= SRB_STATUS_AUTOSENSE_VALID;
    }

    return SrbStatus;
}

static VOID SpdCdbGetRange(PCDB Cdb,
    PUINT64 POffset, PUINT32 PLength, PUINT32 PForceUnitAccess)
{
    ASSERT(
        SCSIOP_READ6 == Cdb->AsByte[0] ||
        SCSIOP_READ == Cdb->AsByte[0] ||
        SCSIOP_READ12 == Cdb->AsByte[0] ||
        SCSIOP_READ16 == Cdb->AsByte[0] ||
        SCSIOP_WRITE6 == Cdb->AsByte[0] ||
        SCSIOP_WRITE == Cdb->AsByte[0] ||
        SCSIOP_WRITE12 == Cdb->AsByte[0] ||
        SCSIOP_WRITE16 == Cdb->AsByte[0] ||
        SCSIOP_SYNCHRONIZE_CACHE == Cdb->AsByte[0] ||
        SCSIOP_SYNCHRONIZE_CACHE16 == Cdb->AsByte[0]);

    switch (Cdb->AsByte[0] & 0xE0)
    {
    case 0 << 5:
        /* CDB6 */
        if (0 != POffset)
            *POffset =
                ((UINT64)Cdb->CDB6READWRITE.LogicalBlockMsb1 << 16) |
                ((UINT64)Cdb->CDB6READWRITE.LogicalBlockMsb0 << 8) |
                ((UINT64)Cdb->CDB6READWRITE.LogicalBlockLsb);
        if (0 != PLength)
            *PLength = 0 != Cdb->CDB6READWRITE.TransferBlocks ?
                ((UINT32)Cdb->CDB6READWRITE.TransferBlocks) :
                256;
        if (0 != PForceUnitAccess)
            *PForceUnitAccess = 0;
        break;

    case 1 << 5:
    case 2 << 5:
        /* CDB10 */
        if (0 != POffset)
            *POffset =
                ((UINT64)Cdb->CDB10.LogicalBlockByte0 << 24) |
                ((UINT64)Cdb->CDB10.LogicalBlockByte1 << 16) |
                ((UINT64)Cdb->CDB10.LogicalBlockByte2 << 8) |
                ((UINT64)Cdb->CDB10.LogicalBlockByte3);
        if (0 != PLength)
            *PLength =
                ((UINT32)Cdb->CDB10.TransferBlocksMsb << 8) |
                ((UINT32)Cdb->CDB10.TransferBlocksLsb);
        if (0 != PForceUnitAccess)
            *PForceUnitAccess = Cdb->CDB10.ForceUnitAccess;
        break;

    case 4 << 5:
        /* CDB16 */
        if (0 != POffset)
            *POffset =
                ((UINT64)Cdb->CDB16.LogicalBlock[0] << 56) |
                ((UINT64)Cdb->CDB16.LogicalBlock[1] << 48) |
                ((UINT64)Cdb->CDB16.LogicalBlock[2] << 40) |
                ((UINT64)Cdb->CDB16.LogicalBlock[3] << 32) |
                ((UINT64)Cdb->CDB16.LogicalBlock[4] << 24) |
                ((UINT64)Cdb->CDB16.LogicalBlock[5] << 16) |
                ((UINT64)Cdb->CDB16.LogicalBlock[6] << 8) |
                ((UINT64)Cdb->CDB16.LogicalBlock[7]);
        if (0 != PLength)
            *PLength =
                ((UINT32)Cdb->CDB16.TransferLength[0] << 24) |
                ((UINT32)Cdb->CDB16.TransferLength[1] << 16) |
                ((UINT32)Cdb->CDB16.TransferLength[2] << 8) |
                ((UINT32)Cdb->CDB16.TransferLength[3]);
        if (0 != PForceUnitAccess)
            *PForceUnitAccess = Cdb->CDB16.ForceUnitAccess;
        break;

    case 5 << 5:
        /* CDB12 */
        if (0 != POffset)
            *POffset =
                ((UINT64)Cdb->CDB12.LogicalBlock[0] << 24) |
                ((UINT64)Cdb->CDB12.LogicalBlock[1] << 16) |
                ((UINT64)Cdb->CDB12.LogicalBlock[2] << 8) |
                ((UINT64)Cdb->CDB12.LogicalBlock[3]);
        if (0 != PLength)
            *PLength =
                ((UINT32)Cdb->CDB12.TransferLength[0] << 24) |
                ((UINT32)Cdb->CDB12.TransferLength[1] << 16) |
                ((UINT32)Cdb->CDB12.TransferLength[2] << 8) |
                ((UINT32)Cdb->CDB12.TransferLength[3]);
        if (0 != PForceUnitAccess)
            *PForceUnitAccess = Cdb->CDB12.ForceUnitAccess;
        break;
    }
}
