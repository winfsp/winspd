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

static UCHAR SpdScsiTestUnitReady(PVOID DeviceExtension, SPD_LOGICAL_UNIT *LogicalUnit, PVOID Srb);
static UCHAR SpdScsiInquiry(PVOID DeviceExtension, SPD_LOGICAL_UNIT *LogicalUnit, PVOID Srb);
static UCHAR SpdScsiReadCapacity(PVOID DeviceExtension, SPD_LOGICAL_UNIT *LogicalUnit, PVOID Srb);
static UCHAR SpdScsiRead(PVOID DeviceExtension, SPD_LOGICAL_UNIT *LogicalUnit, PVOID Srb);
static UCHAR SpdScsiWrite(PVOID DeviceExtension, SPD_LOGICAL_UNIT *LogicalUnit, PVOID Srb);
static UCHAR SpdScsiVerify(PVOID DeviceExtension, SPD_LOGICAL_UNIT *LogicalUnit, PVOID Srb);
static UCHAR SpdScsiSynchronizeCache(PVOID DeviceExtension, SPD_LOGICAL_UNIT *LogicalUnit, PVOID Srb);
static UCHAR SpdScsiStartStopUnit(PVOID DeviceExtension, SPD_LOGICAL_UNIT *LogicalUnit, PVOID Srb);
static UCHAR SpdScsiReportLuns(PVOID DeviceExtension, SPD_LOGICAL_UNIT *LogicalUnit, PVOID Srb);

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
        SrbStatus = SpdScsiTestUnitReady(DeviceExtension, LogicalUnit, Srb);
        break;

    case SCSIOP_INQUIRY:
        SrbStatus = SpdScsiInquiry(DeviceExtension, LogicalUnit, Srb);
        break;

    case SCSIOP_READ_CAPACITY:
        SrbStatus = SpdScsiReadCapacity(DeviceExtension, LogicalUnit, Srb);
        break;

    case SCSIOP_READ:
        SrbStatus = SpdScsiRead(DeviceExtension, LogicalUnit, Srb);
        break;

    case SCSIOP_WRITE:
        SrbStatus = SpdScsiWrite(DeviceExtension, LogicalUnit, Srb);
        break;

    case SCSIOP_VERIFY:
        SrbStatus = SpdScsiVerify(DeviceExtension, LogicalUnit, Srb);
        break;

    case SCSIOP_SYNCHRONIZE_CACHE:
        SrbStatus = SpdScsiSynchronizeCache(DeviceExtension, LogicalUnit, Srb);
        break;

    case SCSIOP_START_STOP_UNIT:
        SrbStatus = SpdScsiStartStopUnit(DeviceExtension, LogicalUnit, Srb);
        break;

    case SCSIOP_REPORT_LUNS:
        SrbStatus = SpdScsiReportLuns(DeviceExtension, LogicalUnit, Srb);
        break;

    default:
        SrbStatus = SRB_STATUS_INVALID_REQUEST;
        break;
    }

exit:;
    return SrbStatus;
}

UCHAR SpdScsiTestUnitReady(PVOID DeviceExtension, SPD_LOGICAL_UNIT *LogicalUnit, PVOID Srb)
{
    return SRB_STATUS_SUCCESS;
}

UCHAR SpdScsiInquiry(PVOID DeviceExtension, SPD_LOGICAL_UNIT *LogicalUnit, PVOID Srb)
{
    return SRB_STATUS_INVALID_REQUEST;
}

UCHAR SpdScsiReadCapacity(PVOID DeviceExtension, SPD_LOGICAL_UNIT *LogicalUnit, PVOID Srb)
{
    return SRB_STATUS_INVALID_REQUEST;
}

UCHAR SpdScsiRead(PVOID DeviceExtension, SPD_LOGICAL_UNIT *LogicalUnit, PVOID Srb)
{
    return SRB_STATUS_INVALID_REQUEST;
}

UCHAR SpdScsiWrite(PVOID DeviceExtension, SPD_LOGICAL_UNIT *LogicalUnit, PVOID Srb)
{
    return SRB_STATUS_INVALID_REQUEST;
}

UCHAR SpdScsiVerify(PVOID DeviceExtension, SPD_LOGICAL_UNIT *LogicalUnit, PVOID Srb)
{
    return SRB_STATUS_INVALID_REQUEST;
}

UCHAR SpdScsiSynchronizeCache(PVOID DeviceExtension, SPD_LOGICAL_UNIT *LogicalUnit, PVOID Srb)
{
    return SRB_STATUS_INVALID_REQUEST;
}

UCHAR SpdScsiStartStopUnit(PVOID DeviceExtension, SPD_LOGICAL_UNIT *LogicalUnit, PVOID Srb)
{
    return SRB_STATUS_INVALID_REQUEST;
}

UCHAR SpdScsiReportLuns(PVOID DeviceExtension, SPD_LOGICAL_UNIT *LogicalUnit, PVOID Srb)
{
    return SRB_STATUS_INVALID_REQUEST;
}
