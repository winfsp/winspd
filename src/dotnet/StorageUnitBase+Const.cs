/*
 * dotnet/StorageUnitBase+Const.cs
 *
 * Copyright 2018-2020 Bill Zissimopoulos
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

using System;

namespace Spd
{

    public partial class StorageUnitBase
    {
        /* SCSI Status Codes */
        public const Byte SCSISTAT_GOOD = 0x00;
        public const Byte SCSISTAT_CHECK_CONDITION = 0x02;
        public const Byte SCSISTAT_CONDITION_MET = 0x04;
        public const Byte SCSISTAT_BUSY = 0x08;
        public const Byte SCSISTAT_INTERMEDIATE = 0x10;
        public const Byte SCSISTAT_INTERMEDIATE_COND_MET = 0x14;
        public const Byte SCSISTAT_RESERVATION_CONFLICT = 0x18;
        public const Byte SCSISTAT_COMMAND_TERMINATED = 0x22;
        public const Byte SCSISTAT_QUEUE_FULL = 0x28;

        /* SCSI Sense Keys */
        public const Byte SCSI_SENSE_NO_SENSE = 0x00;
        public const Byte SCSI_SENSE_RECOVERED_ERROR = 0x01;
        public const Byte SCSI_SENSE_NOT_READY = 0x02;
        public const Byte SCSI_SENSE_MEDIUM_ERROR = 0x03;
        public const Byte SCSI_SENSE_HARDWARE_ERROR = 0x04;
        public const Byte SCSI_SENSE_ILLEGAL_REQUEST = 0x05;
        public const Byte SCSI_SENSE_UNIT_ATTENTION = 0x06;
        public const Byte SCSI_SENSE_DATA_PROTECT = 0x07;
        public const Byte SCSI_SENSE_BLANK_CHECK = 0x08;
        public const Byte SCSI_SENSE_UNIQUE = 0x09;
        public const Byte SCSI_SENSE_COPY_ABORTED = 0x0A;
        public const Byte SCSI_SENSE_ABORTED_COMMAND = 0x0B;
        public const Byte SCSI_SENSE_EQUAL = 0x0C;
        public const Byte SCSI_SENSE_VOL_OVERFLOW = 0x0D;
        public const Byte SCSI_SENSE_MISCOMPARE = 0x0E;
        public const Byte SCSI_SENSE_RESERVED = 0x0F;

        /* SCSI Additional Sense Codes */
        public const Byte SCSI_ADSENSE_NO_SENSE = 0x00;
        public const Byte SCSI_ADSENSE_NO_SEEK_COMPLETE = 0x02;
        public const Byte SCSI_ADSENSE_WRITE = 0x03;
        public const Byte SCSI_ADSENSE_LUN_NOT_READY = 0x04;
        public const Byte SCSI_ADSENSE_LUN_COMMUNICATION = 0x08;
        public const Byte SCSI_ADSENSE_SERVO_ERROR = 0x09;
        public const Byte SCSI_ADSENSE_WARNING = 0x0B;
        public const Byte SCSI_ADSENSE_WRITE_ERROR = 0x0C;
        public const Byte SCSI_ADSENSE_COPY_TARGET_DEVICE_ERROR = 0x0D;
        public const Byte SCSI_ADSENSE_UNRECOVERED_ERROR = 0x11;
        public const Byte SCSI_ADSENSE_TRACK_ERROR = 0x14;
        public const Byte SCSI_ADSENSE_SEEK_ERROR = 0x15;
        public const Byte SCSI_ADSENSE_REC_DATA_NOECC = 0x17;
        public const Byte SCSI_ADSENSE_REC_DATA_ECC = 0x18;
        public const Byte SCSI_ADSENSE_DEFECT_LIST_ERROR = 0x19;
        public const Byte SCSI_ADSENSE_PARAMETER_LIST_LENGTH = 0x1A;
        public const Byte SCSI_ADSENSE_ILLEGAL_COMMAND = 0x20;
        public const Byte SCSI_ADSENSE_ACCESS_DENIED = 0x20;
        public const Byte SCSI_ADSENSE_ILLEGAL_BLOCK = 0x21;
        public const Byte SCSI_ADSENSE_INVALID_TOKEN = 0x23;
        public const Byte SCSI_ADSENSE_INVALID_CDB = 0x24;
        public const Byte SCSI_ADSENSE_INVALID_LUN = 0x25;
        public const Byte SCSI_ADSENSE_INVALID_FIELD_PARAMETER_LIST = 0x26;
        public const Byte SCSI_ADSENSE_WRITE_PROTECT = 0x27;
        public const Byte SCSI_ADSENSE_MEDIUM_CHANGED = 0x28;
        public const Byte SCSI_ADSENSE_BUS_RESET = 0x29;
        public const Byte SCSI_ADSENSE_PARAMETERS_CHANGED = 0x2A;
        public const Byte SCSI_ADSENSE_INSUFFICIENT_TIME_FOR_OPERATION = 0x2E;
        public const Byte SCSI_ADSENSE_INVALID_MEDIA = 0x30;
        public const Byte SCSI_ADSENSE_DEFECT_LIST = 0x32;
        public const Byte SCSI_ADSENSE_LB_PROVISIONING = 0x38;
        public const Byte SCSI_ADSENSE_NO_MEDIA_IN_DEVICE = 0x3a;
        public const Byte SCSI_ADSENSE_POSITION_ERROR = 0x3b;
        public const Byte SCSI_ADSENSE_LOGICAL_UNIT_ERROR = 0x3e;
        public const Byte SCSI_ADSENSE_OPERATING_CONDITIONS_CHANGED = 0x3f;
        public const Byte SCSI_ADSENSE_DATA_PATH_FAILURE = 0x41;
        public const Byte SCSI_ADSENSE_POWER_ON_SELF_TEST_FAILURE = 0x42;
        public const Byte SCSI_ADSENSE_INTERNAL_TARGET_FAILURE = 0x44;
        public const Byte SCSI_ADSENSE_DATA_TRANSFER_ERROR = 0x4b;
        public const Byte SCSI_ADSENSE_LUN_FAILED_SELF_CONFIGURATION = 0x4c;
        public const Byte SCSI_ADSENSE_RESOURCE_FAILURE = 0x55;
        public const Byte SCSI_ADSENSE_OPERATOR_REQUEST = 0x5a;
        public const Byte SCSI_ADSENSE_FAILURE_PREDICTION_THRESHOLD_EXCEEDED = 0x5d;
        public const Byte SCSI_ADSENSE_ILLEGAL_MODE_FOR_THIS_TRACK = 0x64;
        public const Byte SCSI_ADSENSE_COPY_PROTECTION_FAILURE = 0x6f;
        public const Byte SCSI_ADSENSE_POWER_CALIBRATION_ERROR = 0x73;
        public const Byte SCSI_ADSENSE_VENDOR_UNIQUE = 0x80;
        public const Byte SCSI_ADSENSE_MUSIC_AREA = 0xA0;
        public const Byte SCSI_ADSENSE_DATA_AREA = 0xA1;
        public const Byte SCSI_ADSENSE_VOLUME_OVERFLOW = 0xA7;
    }

}
