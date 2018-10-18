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
    default:
        SrbStatus = SRB_STATUS_INVALID_REQUEST;
        break;
    }

exit:;
    return SrbStatus;
}
