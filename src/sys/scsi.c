/**
 * @file sys/scsi.c
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

#include <sys/driver.h>

static UCHAR SpdScsiInquiry(SPD_STORAGE_UNIT *StorageUnit, PVOID Srb, PCDB Cdb);
static UCHAR SpdScsiModeSense(SPD_STORAGE_UNIT *StorageUnit, PVOID Srb, PCDB Cdb);
static UCHAR SpdScsiReadCapacity(SPD_STORAGE_UNIT *StorageUnit, PVOID Srb, PCDB Cdb);
static UCHAR SpdScsiRead(SPD_STORAGE_UNIT *StorageUnit, PVOID Srb, PCDB Cdb);
static UCHAR SpdScsiWrite(SPD_STORAGE_UNIT *StorageUnit, PVOID Srb, PCDB Cdb);
static UCHAR SpdScsiSynchronizeCache(SPD_STORAGE_UNIT *StorageUnit, PVOID Srb, PCDB Cdb);
static UCHAR SpdScsiUnmap(SPD_STORAGE_UNIT *StorageUnit, PVOID Srb, PCDB Cdb);
static UCHAR SpdScsiReportLuns(SPD_STORAGE_UNIT *StorageUnit, PVOID Srb, PCDB Cdb);

static UCHAR SpdScsiError(PVOID Srb, UCHAR SenseKey, UCHAR AdditionalSenseCode);
static BOOLEAN SpdCdbGetRange(PCDB Cdb, PUINT64 POffset, PUINT32 PLength);

UCHAR SpdSrbExecuteScsi(PVOID DeviceExtension, PVOID Srb)
{
    ASSERT(DISPATCH_LEVEL >= KeGetCurrentIrql());

    SPD_STORAGE_UNIT *StorageUnit = 0;
    PCDB Cdb;
    UCHAR SrbStatus = SRB_STATUS_PENDING;

    StorageUnit = SpdStorageUnitReference(DeviceExtension, Srb);
    if (0 == StorageUnit)
    {
        SrbStatus = SRB_STATUS_NO_DEVICE;
        goto exit;
    }

    Cdb = SrbGetCdb(Srb);
    switch (Cdb->AsByte[0])
    {
    case SCSIOP_TEST_UNIT_READY:
        SrbStatus = SRB_STATUS_SUCCESS;
        break;

    case SCSIOP_INQUIRY:
        SrbStatus = SpdScsiInquiry(StorageUnit, Srb, Cdb);
        break;

    case SCSIOP_MODE_SENSE:
    case SCSIOP_MODE_SENSE10:
        SrbStatus = SpdScsiModeSense(StorageUnit, Srb, Cdb);
        break;

    case SCSIOP_READ_CAPACITY:
    case SCSIOP_READ_CAPACITY16:
        SrbStatus = SpdScsiReadCapacity(StorageUnit, Srb, Cdb);
        break;

    case SCSIOP_READ6:
    case SCSIOP_READ:
    case SCSIOP_READ12:
    case SCSIOP_READ16:
        SrbStatus = SpdScsiRead(StorageUnit, Srb, Cdb);
        break;

    case SCSIOP_WRITE6:
    case SCSIOP_WRITE:
    case SCSIOP_WRITE12:
    case SCSIOP_WRITE16:
        SrbStatus = SpdScsiWrite(StorageUnit, Srb, Cdb);
        break;

    case SCSIOP_SYNCHRONIZE_CACHE:
        SrbStatus = SpdScsiSynchronizeCache(StorageUnit, Srb, Cdb);
        break;

    case SCSIOP_UNMAP:
        SrbStatus = SpdScsiUnmap(StorageUnit, Srb, Cdb);
        break;

    case SCSIOP_REPORT_LUNS:
        SrbStatus = SpdScsiReportLuns(StorageUnit, Srb, Cdb);
        break;

    default:
        SrbStatus = SRB_STATUS_INVALID_REQUEST;
        break;
    }

exit:
    if (0 != StorageUnit)
        SpdStorageUnitDereference(DeviceExtension, StorageUnit);

    return SrbStatus;
}

UCHAR SpdScsiInquiry(SPD_STORAGE_UNIT *StorageUnit, PVOID Srb, PCDB Cdb)
{
    PVOID DataBuffer = SrbGetDataBuffer(Srb);
    ULONG DataTransferLength = SrbGetDataTransferLength(Srb);

    if (0 == DataBuffer)
        return SRB_STATUS_INTERNAL_ERROR;

    RtlZeroMemory(DataBuffer, DataTransferLength);

    if (0 == Cdb->CDB6INQUIRY3.EnableVitalProductData)
    {
        if (INQUIRYDATABUFFERSIZE > DataTransferLength)
            return SRB_STATUS_DATA_OVERRUN;
        PINQUIRYDATA InquiryData = DataBuffer;
        InquiryData->DeviceType = StorageUnit->StorageUnitParams.DeviceType;
        InquiryData->DeviceTypeQualifier = DEVICE_QUALIFIER_ACTIVE;
        InquiryData->RemovableMedia = !!StorageUnit->StorageUnitParams.RemovableMedia;
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
        enum
        {
            PageCount = 3,
            IdentifierLength =
                sizeof SPD_IOCTL_VENDOR_ID - 1 +
                sizeof StorageUnit->StorageUnitParams.ProductId +
                sizeof StorageUnit->StorageUnitParams.ProductRevisionLevel +
                sizeof StorageUnit->SerialNumber,
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
                sizeof(VPD_IDENTIFICATION_DESCRIPTOR) + IdentifierLength > DataTransferLength)
                return SRB_STATUS_DATA_OVERRUN;
            Identification = DataBuffer;
            Identification->DeviceType = StorageUnit->StorageUnitParams.DeviceType;
            Identification->DeviceTypeQualifier = DEVICE_QUALIFIER_ACTIVE;
            Identification->PageCode = VPD_DEVICE_IDENTIFIERS;
            Identification->PageLength =
                sizeof(VPD_IDENTIFICATION_DESCRIPTOR) + IdentifierLength;
            ((PVPD_IDENTIFICATION_DESCRIPTOR)Identification->Descriptors)->CodeSet =
                VpdCodeSetAscii;
            ((PVPD_IDENTIFICATION_DESCRIPTOR)Identification->Descriptors)->IdentifierType =
                VpdIdentifierTypeVendorId;
            ((PVPD_IDENTIFICATION_DESCRIPTOR)Identification->Descriptors)->Association =
                VpdAssocDevice;
            ((PVPD_IDENTIFICATION_DESCRIPTOR)Identification->Descriptors)->IdentifierLength =
                IdentifierLength;
            RtlCopyMemory(((PVPD_IDENTIFICATION_DESCRIPTOR)Identification->Descriptors)->Identifier,
                SPD_IOCTL_VENDOR_ID,
                sizeof SPD_IOCTL_VENDOR_ID - 1);
            RtlCopyMemory(((PVPD_IDENTIFICATION_DESCRIPTOR)Identification->Descriptors)->Identifier +
                    sizeof SPD_IOCTL_VENDOR_ID - 1,
                StorageUnit->StorageUnitParams.ProductId,
                sizeof StorageUnit->StorageUnitParams.ProductId);
            RtlCopyMemory(((PVPD_IDENTIFICATION_DESCRIPTOR)Identification->Descriptors)->Identifier +
                    sizeof SPD_IOCTL_VENDOR_ID - 1 +
                    sizeof StorageUnit->StorageUnitParams.ProductId,
                StorageUnit->StorageUnitParams.ProductRevisionLevel,
                sizeof StorageUnit->StorageUnitParams.ProductRevisionLevel);
            RtlCopyMemory(((PVPD_IDENTIFICATION_DESCRIPTOR)Identification->Descriptors)->Identifier +
                    sizeof SPD_IOCTL_VENDOR_ID - 1 +
                    sizeof StorageUnit->StorageUnitParams.ProductId +
                    sizeof StorageUnit->StorageUnitParams.ProductRevisionLevel,
                &StorageUnit->SerialNumber,
                sizeof StorageUnit->SerialNumber);
            SrbSetDataTransferLength(Srb, sizeof(VPD_IDENTIFICATION_PAGE) +
                sizeof(VPD_IDENTIFICATION_DESCRIPTOR) + IdentifierLength);
            return SRB_STATUS_SUCCESS;

        default:
            return SpdScsiError(Srb, SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ADSENSE_INVALID_CDB);
        }
    }
}

UCHAR SpdScsiModeSense(SPD_STORAGE_UNIT *StorageUnit, PVOID Srb, PCDB Cdb)
{
    return SRB_STATUS_INVALID_REQUEST;
}

UCHAR SpdScsiReadCapacity(SPD_STORAGE_UNIT *StorageUnit, PVOID Srb, PCDB Cdb)
{
    return SRB_STATUS_INVALID_REQUEST;
}

UCHAR SpdScsiRead(SPD_STORAGE_UNIT *StorageUnit, PVOID Srb, PCDB Cdb)
{
    return SRB_STATUS_INVALID_REQUEST;
}

UCHAR SpdScsiWrite(SPD_STORAGE_UNIT *StorageUnit, PVOID Srb, PCDB Cdb)
{
    return SRB_STATUS_INVALID_REQUEST;
}

UCHAR SpdScsiSynchronizeCache(SPD_STORAGE_UNIT *StorageUnit, PVOID Srb, PCDB Cdb)
{
    return SRB_STATUS_INVALID_REQUEST;
}

UCHAR SpdScsiUnmap(SPD_STORAGE_UNIT *StorageUnit, PVOID Srb, PCDB Cdb)
{
    return SRB_STATUS_INVALID_REQUEST;
}

UCHAR SpdScsiReportLuns(SPD_STORAGE_UNIT *StorageUnit, PVOID Srb, PCDB Cdb)
{
    PVOID DataBuffer = SrbGetDataBuffer(Srb);
    ULONG DataTransferLength = SrbGetDataTransferLength(Srb);

    if (0 == DataBuffer)
        return SRB_STATUS_INTERNAL_ERROR;

    RtlZeroMemory(DataBuffer, DataTransferLength);

    ULONG LunCount = 1;
    UINT32 LunListLength = LunCount * RTL_FIELD_SIZE(LUN_LIST, Lun[0]);
    if (sizeof(LUN_LIST) + LunListLength > DataTransferLength)
        return SRB_STATUS_DATA_OVERRUN;

    PLUN_LIST LunList = DataBuffer;
    LunList->LunListLength[0] = (LunListLength >> 24) & 0xff;
    LunList->LunListLength[1] = (LunListLength >> 16) & 0xff;
    LunList->LunListLength[2] = (LunListLength >> 8) & 0xff;
    LunList->LunListLength[3] = LunListLength & 0xff;
    LunList->Lun[0][1] = 0;
    SrbSetDataTransferLength(Srb, sizeof(LUN_LIST) + LunListLength);

    return SRB_STATUS_SUCCESS;
}

UCHAR SpdScsiError(PVOID Srb, UCHAR SenseKey, UCHAR AdditionalSenseCode)
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
        SenseInfoBuffer->Valid = 1;
        SenseInfoBuffer->SenseKey = SenseKey;
        SenseInfoBuffer->AdditionalSenseCode = AdditionalSenseCode;
        SenseInfoBuffer->AdditionalSenseLength = sizeof(SENSE_DATA) -
            RTL_SIZEOF_THROUGH_FIELD(SENSE_DATA, AdditionalSenseLength);

        SrbSetScsiStatus(Srb, SCSISTAT_CHECK_CONDITION);

        SrbStatus |= SRB_STATUS_AUTOSENSE_VALID;
    }

    return SrbStatus;
}

BOOLEAN SpdCdbGetRange(PCDB Cdb, PUINT64 POffset, PUINT32 PLength)
{
    ASSERT(
        SCSIOP_READ_CAPACITY == Cdb->AsByte[0] ||
        SCSIOP_READ_CAPACITY16 == Cdb->AsByte[0] ||
        SCSIOP_READ6 == Cdb->AsByte[0] ||
        SCSIOP_READ == Cdb->AsByte[0] ||
        SCSIOP_READ12 == Cdb->AsByte[0] ||
        SCSIOP_READ16 == Cdb->AsByte[0] ||
        SCSIOP_WRITE6 == Cdb->AsByte[0] ||
        SCSIOP_WRITE == Cdb->AsByte[0] ||
        SCSIOP_WRITE12 == Cdb->AsByte[0] ||
        SCSIOP_WRITE16 == Cdb->AsByte[0] ||
        SCSIOP_VERIFY == Cdb->AsByte[0] ||
        SCSIOP_VERIFY12 == Cdb->AsByte[0] ||
        SCSIOP_VERIFY16 == Cdb->AsByte[0]);

    switch (Cdb->AsByte[0] & 0xE0)
    {
    case 0 << 5:
        /* CDB6 */
        *POffset =
            ((UINT64)Cdb->CDB6READWRITE.LogicalBlockMsb1 << 16) |
            ((UINT64)Cdb->CDB6READWRITE.LogicalBlockMsb0 << 8) |
            ((UINT64)Cdb->CDB6READWRITE.LogicalBlockLsb);
        *PLength =
            ((UINT32)Cdb->CDB6READWRITE.TransferBlocks);
        return TRUE;

    case 1 << 5:
    case 2 << 5:
        /* CDB10 */
        *POffset =
            ((UINT64)Cdb->CDB10.LogicalBlockByte0 << 24) |
            ((UINT64)Cdb->CDB10.LogicalBlockByte1 << 16) |
            ((UINT64)Cdb->CDB10.LogicalBlockByte2 << 8) |
            ((UINT64)Cdb->CDB10.LogicalBlockByte3);
        *PLength =
            ((UINT32)Cdb->CDB10.TransferBlocksMsb << 8) |
            ((UINT32)Cdb->CDB10.TransferBlocksLsb);
        return TRUE;

    case 4 << 5:
        /* CDB16 */
        *POffset =
            ((UINT64)Cdb->CDB16.LogicalBlock[0] << 56) |
            ((UINT64)Cdb->CDB16.LogicalBlock[1] << 48) |
            ((UINT64)Cdb->CDB16.LogicalBlock[2] << 40) |
            ((UINT64)Cdb->CDB16.LogicalBlock[3] << 32) |
            ((UINT64)Cdb->CDB16.LogicalBlock[4] << 24) |
            ((UINT64)Cdb->CDB16.LogicalBlock[5] << 16) |
            ((UINT64)Cdb->CDB16.LogicalBlock[6] << 8) |
            ((UINT64)Cdb->CDB16.LogicalBlock[7]);
        *PLength =
            ((UINT32)Cdb->CDB16.TransferLength[0] << 24) |
            ((UINT32)Cdb->CDB16.TransferLength[1] << 16) |
            ((UINT32)Cdb->CDB16.TransferLength[2] << 8) |
            ((UINT32)Cdb->CDB16.TransferLength[3]);
        return TRUE;

    case 5 << 5:
        /* CDB12 */
        *POffset =
            ((UINT64)Cdb->CDB12.LogicalBlock[0] << 24) |
            ((UINT64)Cdb->CDB12.LogicalBlock[1] << 16) |
            ((UINT64)Cdb->CDB12.LogicalBlock[2] << 8) |
            ((UINT64)Cdb->CDB12.LogicalBlock[3]);
        *PLength =
            ((UINT32)Cdb->CDB12.TransferLength[0] << 24) |
            ((UINT32)Cdb->CDB12.TransferLength[1] << 16) |
            ((UINT32)Cdb->CDB12.TransferLength[2] << 8) |
            ((UINT32)Cdb->CDB12.TransferLength[3]);
        return TRUE;

    default:
        return FALSE;
    }
}
