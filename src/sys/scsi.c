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

static UCHAR SpdScsiReportLuns(PVOID DeviceExtension, SPD_STORAGE_UNIT *StorageUnit,
    PVOID Srb, PCDB Cdb);
static UCHAR SpdScsiInquiry(PVOID DeviceExtension, SPD_STORAGE_UNIT *StorageUnit,
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
        PVPD_BLOCK_LIMITS_PAGE BlockLimits;
        PVPD_LOGICAL_BLOCK_PROVISIONING_PAGE LogicalBlockProvisioning;
        UINT32 U32;
        enum
        {
            PageCount = 5,
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
            LogicalBlockProvisioning->ANC_SUP = 0;
            LogicalBlockProvisioning->LBPU = 1;
            LogicalBlockProvisioning->ProvisioningType = PROVISIONING_TYPE_THIN;

            SrbSetDataTransferLength(Srb, sizeof(VPD_LOGICAL_BLOCK_PROVISIONING_PAGE));

            return SRB_STATUS_SUCCESS;

        default:
            return SpdScsiError(Srb, SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ADSENSE_INVALID_CDB);
        }
    }
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
        U32 = 0xffffffffULL >= StorageUnit->StorageUnitParams.BlockCount ?
            (UINT32)StorageUnit->StorageUnitParams.BlockCount : 0xffffffff;
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
        if (sizeof(READ_CAPACITY16_DATA) > DataTransferLength)
            return SRB_STATUS_DATA_OVERRUN;

        PREAD_CAPACITY16_DATA ReadCapacityData = DataBuffer;
        UINT64 U64;
        UINT32 U32;
        U64 = StorageUnit->StorageUnitParams.BlockCount;
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
        ReadCapacityData->LBPME = 1;

        SrbSetDataTransferLength(Srb, sizeof(READ_CAPACITY16_DATA));

        return SRB_STATUS_SUCCESS;
    }
}

static UCHAR SpdScsiPostRangeSrb(PVOID DeviceExtension, SPD_STORAGE_UNIT *StorageUnit,
    PVOID Srb, PCDB Cdb)
{
    UINT64 BlockAddress, EndBlockAddress;
    UINT32 BlockCount;
    BOOLEAN HasDataBuffer;

    SpdCdbGetRange(Cdb, &BlockAddress, &BlockCount, 0);

    if (0 == BlockCount)
        return SRB_STATUS_SUCCESS;

    EndBlockAddress = BlockAddress + BlockCount;
    if (EndBlockAddress < BlockAddress ||
        EndBlockAddress > StorageUnit->StorageUnitParams.BlockCount)
        return SpdScsiError(Srb, SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ADSENSE_ILLEGAL_BLOCK);

    HasDataBuffer = !(
        SCSIOP_SYNCHRONIZE_CACHE == Cdb->AsByte[0] ||
        SCSIOP_SYNCHRONIZE_CACHE16 == Cdb->AsByte[0]);

    return SpdScsiPostSrb(DeviceExtension, StorageUnit, Srb,
        HasDataBuffer ? BlockCount * StorageUnit->StorageUnitParams.BlockLength : 0);
}

static UCHAR SpdScsiPostUnmapSrb(PVOID DeviceExtension, SPD_STORAGE_UNIT *StorageUnit,
    PVOID Srb, PCDB Cdb)
{
    PUNMAP_LIST_HEADER DataBuffer = SrbGetDataBuffer(Srb);
    ULONG DataTransferLength = SrbGetDataTransferLength(Srb);
    ULONG Length;

    if (0 == DataBuffer)
        return SRB_STATUS_INTERNAL_ERROR;

    if (Cdb->UNMAP.Anchor)
        return SpdScsiError(Srb, SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ADSENSE_INVALID_CDB);

    if (DataTransferLength < sizeof(UNMAP_LIST_HEADER) ||
        DataTransferLength < sizeof(UNMAP_LIST_HEADER) +
            (Length = ((ULONG)DataBuffer->DataLength[0] << 8) | (ULONG)DataBuffer->DataLength[1]))
        return SRB_STATUS_INVALID_REQUEST;

    if (0 == Length)
        return SRB_STATUS_SUCCESS;

    for (ULONG I = 0, N = Length / sizeof(UNMAP_BLOCK_DESCRIPTOR); N > I; I++)
    {
        PUNMAP_BLOCK_DESCRIPTOR Descr = &DataBuffer->Descriptors[I];
        UINT64 BlockAddress =
            ((UINT64)Descr->StartingLba[0] << 56) |
            ((UINT64)Descr->StartingLba[1] << 48) |
            ((UINT64)Descr->StartingLba[2] << 40) |
            ((UINT64)Descr->StartingLba[3] << 32) |
            ((UINT64)Descr->StartingLba[4] << 24) |
            ((UINT64)Descr->StartingLba[5] << 16) |
            ((UINT64)Descr->StartingLba[6] << 8) |
            ((UINT64)Descr->StartingLba[7]);
        UINT32 BlockCount =
            ((UINT32)Descr->LbaCount[0] << 24) |
            ((UINT32)Descr->LbaCount[1] << 16) |
            ((UINT32)Descr->LbaCount[2] << 8) |
            ((UINT32)Descr->LbaCount[3]);
        UINT64 EndBlockAddress = BlockAddress + BlockCount;

        if (EndBlockAddress < BlockAddress ||
            EndBlockAddress > StorageUnit->StorageUnitParams.BlockCount)
            return SpdScsiError(Srb, SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ADSENSE_ILLEGAL_BLOCK);
    }

    return SpdScsiPostSrb(DeviceExtension, StorageUnit, Srb, Length);
}

static UCHAR SpdScsiPostSrb(PVOID DeviceExtension, SPD_STORAGE_UNIT *StorageUnit,
    PVOID Srb, ULONG DataLength)
{
    SPD_SRB_EXTENSION *SrbExtension;
    ULONG StorResult;
    NTSTATUS Result;

    if (0 != DataLength)
    {
        if (0 == SrbGetDataBuffer(Srb))
            return SRB_STATUS_INTERNAL_ERROR;

        SrbExtension = SpdSrbExtension(Srb);
        StorResult = StorPortGetSystemAddress(DeviceExtension, Srb, &SrbExtension->SystemDataBuffer);
        if (STOR_STATUS_SUCCESS != StorResult)
        {
            ASSERT(STOR_STATUS_INSUFFICIENT_RESOURCES == StorResult);
            SrbSetSystemStatus(Srb, (ULONG)STATUS_INSUFFICIENT_RESOURCES);
            return SRB_STATUS_INTERNAL_ERROR;
        }
        SrbExtension->SystemDataLength = DataLength;
        ASSERT(SrbExtension->SystemDataLength <= StorageUnit->StorageUnitParams.MaxTransferLength);
    }

    Result = SpdIoqPostSrb(StorageUnit->Ioq, Srb);
    return NT_SUCCESS(Result) ? SRB_STATUS_PENDING : SRB_STATUS_ABORTED;
}

VOID SpdSrbExecuteScsiPrepare(PVOID Srb, PVOID Context, PVOID DataBuffer)
{
    ASSERT(DISPATCH_LEVEL == KeGetCurrentIrql());

    SPD_IOCTL_TRANSACT_REQ *Req = Context;
    PCDB Cdb;
    SPD_SRB_EXTENSION *SrbExtension;
    UINT32 ForceUnitAccess;

    Cdb = SrbGetCdb(Srb);
    switch (Cdb->AsByte[0])
    {
    case SCSIOP_READ6:
    case SCSIOP_READ:
    case SCSIOP_READ12:
    case SCSIOP_READ16:
        Req->Hint = (UINT64)(UINT_PTR)Srb;
        Req->Kind = SpdIoctlTransactReadKind;
        SpdCdbGetRange(Cdb,
            &Req->Op.Read.BlockAddress,
            &Req->Op.Read.BlockCount,
            &ForceUnitAccess);
        Req->Op.Read.ForceUnitAccess = ForceUnitAccess;
        break;

    case SCSIOP_WRITE6:
    case SCSIOP_WRITE:
    case SCSIOP_WRITE12:
    case SCSIOP_WRITE16:
        Req->Hint = (UINT64)(UINT_PTR)Srb;
        Req->Kind = SpdIoctlTransactWriteKind;
        SpdCdbGetRange(Cdb,
            &Req->Op.Write.BlockAddress,
            &Req->Op.Write.BlockCount,
            &ForceUnitAccess);
        Req->Op.Write.ForceUnitAccess = ForceUnitAccess;
        SrbExtension = SpdSrbExtension(Srb);
        RtlCopyMemory(DataBuffer, SrbExtension->SystemDataBuffer, SrbExtension->SystemDataLength);
        break;

    case SCSIOP_SYNCHRONIZE_CACHE:
    case SCSIOP_SYNCHRONIZE_CACHE16:
        Req->Hint = (UINT64)(UINT_PTR)Srb;
        Req->Kind = SpdIoctlTransactFlushKind;
        SpdCdbGetRange(Cdb,
            &Req->Op.Flush.BlockAddress,
            &Req->Op.Flush.BlockCount,
            0);
        break;

    case SCSIOP_UNMAP:
        Req->Hint = (UINT64)(UINT_PTR)Srb;
        Req->Kind = SpdIoctlTransactUnmapKind;
        SrbExtension = SpdSrbExtension(Srb);
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
        break;

    default:
        ASSERT(FALSE);
        break;
    }
}

VOID SpdSrbExecuteScsiComplete(PVOID Srb, PVOID Context, PVOID DataBuffer)
{
    ASSERT(DISPATCH_LEVEL == KeGetCurrentIrql());

    SPD_IOCTL_TRANSACT_RSP *Rsp = Context;
    PCDB Cdb;
    SPD_SRB_EXTENSION *SrbExtension;

    if (SCSISTAT_GOOD != Rsp->Status.ScsiStatus)
    {
        SpdScsiErrorEx(Srb,
            Rsp->Status.SenseKey,
            Rsp->Status.ASC,
            Rsp->Status.ASCQ,
            Rsp->Status.InformationValid ? &Rsp->Status.Information : 0);
        return;
    }

    Cdb = SrbGetCdb(Srb);
    switch (Cdb->AsByte[0])
    {
    case SCSIOP_READ6:
    case SCSIOP_READ:
    case SCSIOP_READ12:
    case SCSIOP_READ16:
        SrbExtension = SpdSrbExtension(Srb);
        if (0 != DataBuffer)
            RtlCopyMemory(SrbExtension->SystemDataBuffer, DataBuffer, SrbExtension->SystemDataLength);
        else
            RtlZeroMemory(SrbExtension->SystemDataBuffer, SrbExtension->SystemDataLength);
        break;

    default:
        ASSERT(FALSE);
        break;
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
        SCSIOP_WRITE16 == Cdb->AsByte[0]);

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
