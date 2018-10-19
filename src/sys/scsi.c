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

static UCHAR SpdScsiInquiry(SPD_LOGICAL_UNIT *LogicalUnit, PVOID Srb, PCDB Cdb);
static UCHAR SpdScsiModeSense(SPD_LOGICAL_UNIT *LogicalUnit, PVOID Srb, PCDB Cdb);
static UCHAR SpdScsiReadCapacity(SPD_LOGICAL_UNIT *LogicalUnit, PVOID Srb, PCDB Cdb);
static UCHAR SpdScsiRead(SPD_LOGICAL_UNIT *LogicalUnit, PVOID Srb, PCDB Cdb);
static UCHAR SpdScsiWrite(SPD_LOGICAL_UNIT *LogicalUnit, PVOID Srb, PCDB Cdb);
static UCHAR SpdScsiVerify(SPD_LOGICAL_UNIT *LogicalUnit, PVOID Srb, PCDB Cdb);
static UCHAR SpdScsiSynchronizeCache(SPD_LOGICAL_UNIT *LogicalUnit, PVOID Srb, PCDB Cdb);
static UCHAR SpdScsiReportLuns(SPD_LOGICAL_UNIT *LogicalUnit, PVOID Srb, PCDB Cdb);

static BOOLEAN SpdCdbGetRange(PCDB Cdb, PUINT64 POffset, PUINT32 PLength);

UCHAR SpdSrbExecuteScsi(PVOID DeviceExtension, PVOID Srb)
{
    UCHAR PathId, TargetId, Lun;
    SPD_LOGICAL_UNIT *LogicalUnit;
    PCDB Cdb;
    UCHAR SrbStatus = SRB_STATUS_PENDING;

    SrbGetPathTargetLun(Srb, &PathId, &TargetId, &Lun);
    LogicalUnit = StorPortGetLogicalUnit(DeviceExtension, PathId, TargetId, Lun);
    if (0 == LogicalUnit)
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

    case SCSIOP_START_STOP_UNIT:
        SrbStatus = SRB_STATUS_SUCCESS;
        break;

    case SCSIOP_INQUIRY:
        SrbStatus = SpdScsiInquiry(LogicalUnit, Srb, Cdb);
        break;

    case SCSIOP_MODE_SENSE:
        SrbStatus = SpdScsiModeSense(LogicalUnit, Srb, Cdb);
        break;

    case SCSIOP_READ_CAPACITY:
    case SCSIOP_READ_CAPACITY16:
        SrbStatus = SpdScsiReadCapacity(LogicalUnit, Srb, Cdb);
        break;

    case SCSIOP_READ6:
    case SCSIOP_READ:
    case SCSIOP_READ12:
    case SCSIOP_READ16:
        SrbStatus = SpdScsiRead(LogicalUnit, Srb, Cdb);
        break;

    case SCSIOP_WRITE6:
    case SCSIOP_WRITE:
    case SCSIOP_WRITE12:
    case SCSIOP_WRITE16:
        SrbStatus = SpdScsiWrite(LogicalUnit, Srb, Cdb);
        break;

    //case SCSIOP_VERIFY6: /* no support! */
    case SCSIOP_VERIFY:
    case SCSIOP_VERIFY12:
    case SCSIOP_VERIFY16:
        SrbStatus = SpdScsiVerify(LogicalUnit, Srb, Cdb);
        break;

    case SCSIOP_SYNCHRONIZE_CACHE:
        SrbStatus = SpdScsiSynchronizeCache(LogicalUnit, Srb, Cdb);
        break;

    case SCSIOP_REPORT_LUNS:
        SrbStatus = SpdScsiReportLuns(LogicalUnit, Srb, Cdb);
        break;

    default:
        SrbStatus = SRB_STATUS_INVALID_REQUEST;
        break;
    }

exit:
    return SrbStatus;
}

UCHAR SpdScsiInquiry(SPD_LOGICAL_UNIT *LogicalUnit, PVOID Srb, PCDB Cdb)
{
    return SRB_STATUS_INVALID_REQUEST;
}

UCHAR SpdScsiModeSense(SPD_LOGICAL_UNIT *LogicalUnit, PVOID Srb, PCDB Cdb)
{
    return SRB_STATUS_INVALID_REQUEST;
}

UCHAR SpdScsiReadCapacity(SPD_LOGICAL_UNIT *LogicalUnit, PVOID Srb, PCDB Cdb)
{
    return SRB_STATUS_INVALID_REQUEST;
}

UCHAR SpdScsiRead(SPD_LOGICAL_UNIT *LogicalUnit, PVOID Srb, PCDB Cdb)
{
    return SRB_STATUS_INVALID_REQUEST;
}

UCHAR SpdScsiWrite(SPD_LOGICAL_UNIT *LogicalUnit, PVOID Srb, PCDB Cdb)
{
    return SRB_STATUS_INVALID_REQUEST;
}

UCHAR SpdScsiVerify(SPD_LOGICAL_UNIT *LogicalUnit, PVOID Srb, PCDB Cdb)
{
    return SRB_STATUS_INVALID_REQUEST;
}

UCHAR SpdScsiSynchronizeCache(SPD_LOGICAL_UNIT *LogicalUnit, PVOID Srb, PCDB Cdb)
{
    return SRB_STATUS_INVALID_REQUEST;
}

UCHAR SpdScsiReportLuns(SPD_LOGICAL_UNIT *LogicalUnit, PVOID Srb, PCDB Cdb)
{
    return SRB_STATUS_INVALID_REQUEST;
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
    case 0:
        /* CDB6 */
        *POffset =
            ((UINT64)Cdb->CDB6READWRITE.LogicalBlockMsb1 << 16) |
            ((UINT64)Cdb->CDB6READWRITE.LogicalBlockMsb0 << 8) |
            ((UINT64)Cdb->CDB6READWRITE.LogicalBlockLsb);
        *PLength =
            ((UINT32)Cdb->CDB6READWRITE.TransferBlocks);
        return TRUE;

    case 1:
    case 2:
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

    case 4:
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

    case 5:
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
