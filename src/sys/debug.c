/**
 * @file sys/debug.c
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

#if DBG
#define SYM(x)                          case x: return #x;

const char *AdapterControlSym(ULONG Control)
{
    switch (Control)
    {
    SYM(ScsiQuerySupportedControlTypes)
    SYM(ScsiStopAdapter)
    SYM(ScsiRestartAdapter)
    SYM(ScsiSetBootConfig)
    SYM(ScsiSetRunningConfig)
    SYM(ScsiPowerSettingNotification)
    SYM(ScsiAdapterPower)
    SYM(ScsiAdapterPoFxPowerRequired)
    SYM(ScsiAdapterPoFxPowerActive)
    SYM(ScsiAdapterPoFxPowerSetFState)
    SYM(ScsiAdapterPoFxPowerControl)
    SYM(ScsiAdapterPrepareForBusReScan)
    SYM(ScsiAdapterSystemPowerHints)
    SYM(ScsiAdapterFilterResourceRequirements)
    SYM(ScsiAdapterPoFxMaxOperationalPower)
    SYM(ScsiAdapterPoFxSetPerfState)
    SYM(ScsiAdapterSurpriseRemoval)
    default:
        return "Control:Unknown";
    }
}

const char *SrbFunctionSym(ULONG Function)
{
    switch (Function)
    {
    SYM(SRB_FUNCTION_EXECUTE_SCSI)
    SYM(SRB_FUNCTION_CLAIM_DEVICE)
    SYM(SRB_FUNCTION_IO_CONTROL)
    SYM(SRB_FUNCTION_RECEIVE_EVENT)
    SYM(SRB_FUNCTION_RELEASE_QUEUE)
    SYM(SRB_FUNCTION_ATTACH_DEVICE)
    SYM(SRB_FUNCTION_RELEASE_DEVICE)
    SYM(SRB_FUNCTION_SHUTDOWN)
    SYM(SRB_FUNCTION_FLUSH)
    SYM(SRB_FUNCTION_PROTOCOL_COMMAND)
    SYM(SRB_FUNCTION_ABORT_COMMAND)
    SYM(SRB_FUNCTION_RELEASE_RECOVERY)
    SYM(SRB_FUNCTION_RESET_BUS)
    SYM(SRB_FUNCTION_RESET_DEVICE)
    SYM(SRB_FUNCTION_TERMINATE_IO)
    SYM(SRB_FUNCTION_FLUSH_QUEUE)
    SYM(SRB_FUNCTION_REMOVE_DEVICE)
    SYM(SRB_FUNCTION_WMI)
    SYM(SRB_FUNCTION_LOCK_QUEUE)
    SYM(SRB_FUNCTION_UNLOCK_QUEUE)
    SYM(SRB_FUNCTION_QUIESCE_DEVICE)
    SYM(SRB_FUNCTION_RESET_LOGICAL_UNIT)
    SYM(SRB_FUNCTION_SET_LINK_TIMEOUT)
    SYM(SRB_FUNCTION_LINK_TIMEOUT_OCCURRED)
    SYM(SRB_FUNCTION_LINK_TIMEOUT_COMPLETE)
    SYM(SRB_FUNCTION_POWER)
    SYM(SRB_FUNCTION_PNP)
    SYM(SRB_FUNCTION_DUMP_POINTERS)
    SYM(SRB_FUNCTION_FREE_DUMP_POINTERS)
    SYM(SRB_FUNCTION_STORAGE_REQUEST_BLOCK)
    default:
        return "SrbFunction:Unknown";
    }
}

const char *SrbStatusSym(ULONG Status)
{
    switch (SRB_STATUS(Status))
    {
    SYM(SRB_STATUS_PENDING)
    SYM(SRB_STATUS_SUCCESS)
    SYM(SRB_STATUS_ABORTED)
    SYM(SRB_STATUS_ABORT_FAILED)
    SYM(SRB_STATUS_ERROR)
    SYM(SRB_STATUS_BUSY)
    SYM(SRB_STATUS_INVALID_REQUEST)
    SYM(SRB_STATUS_INVALID_PATH_ID)
    SYM(SRB_STATUS_NO_DEVICE)
    SYM(SRB_STATUS_TIMEOUT)
    SYM(SRB_STATUS_SELECTION_TIMEOUT)
    SYM(SRB_STATUS_COMMAND_TIMEOUT)
    SYM(SRB_STATUS_MESSAGE_REJECTED)
    SYM(SRB_STATUS_BUS_RESET)
    SYM(SRB_STATUS_PARITY_ERROR)
    SYM(SRB_STATUS_REQUEST_SENSE_FAILED)
    SYM(SRB_STATUS_NO_HBA)
    SYM(SRB_STATUS_DATA_OVERRUN)
    SYM(SRB_STATUS_UNEXPECTED_BUS_FREE)
    SYM(SRB_STATUS_PHASE_SEQUENCE_FAILURE)
    SYM(SRB_STATUS_BAD_SRB_BLOCK_LENGTH)
    SYM(SRB_STATUS_REQUEST_FLUSHED)
    SYM(SRB_STATUS_INVALID_LUN)
    SYM(SRB_STATUS_INVALID_TARGET_ID)
    SYM(SRB_STATUS_BAD_FUNCTION)
    SYM(SRB_STATUS_ERROR_RECOVERY)
    SYM(SRB_STATUS_NOT_POWERED)
    SYM(SRB_STATUS_LINK_DOWN)
    SYM(SRB_STATUS_INTERNAL_ERROR)
    default:
        return "SrbStatus:Unknown";
    }
}

const char *SrbStatusMaskSym(ULONG Status)
{
    switch (Status & (SRB_STATUS_QUEUE_FROZEN | SRB_STATUS_AUTOSENSE_VALID))
    {
    default:
    case 0:
        return "";
    case SRB_STATUS_QUEUE_FROZEN:
        return "[Qf]";
    case SRB_STATUS_AUTOSENSE_VALID:
        return "[Av]";
    case SRB_STATUS_QUEUE_FROZEN | SRB_STATUS_AUTOSENSE_VALID:
        return "[QfAv]";
    }
}

const char *CdbOperationCodeSym(ULONG OperationCode)
{
    switch (OperationCode)
    {
    SYM(SCSIOP_TEST_UNIT_READY)
    SYM(SCSIOP_REZERO_UNIT)
    //SYM(SCSIOP_REWIND)
    SYM(SCSIOP_REQUEST_BLOCK_ADDR)
    SYM(SCSIOP_REQUEST_SENSE)
    SYM(SCSIOP_FORMAT_UNIT)
    SYM(SCSIOP_READ_BLOCK_LIMITS)
    SYM(SCSIOP_REASSIGN_BLOCKS)
    //SYM(SCSIOP_INIT_ELEMENT_STATUS)
    SYM(SCSIOP_READ6)
    //SYM(SCSIOP_RECEIVE)
    SYM(SCSIOP_WRITE6)
    //SYM(SCSIOP_PRINT)
    //SYM(SCSIOP_SEND)
    SYM(SCSIOP_SEEK6)
    //SYM(SCSIOP_TRACK_SELECT)
    //SYM(SCSIOP_SLEW_PRINT)
    //SYM(SCSIOP_SET_CAPACITY)
    SYM(SCSIOP_SEEK_BLOCK)
    SYM(SCSIOP_PARTITION)
    SYM(SCSIOP_READ_REVERSE)
    SYM(SCSIOP_WRITE_FILEMARKS)
    //SYM(SCSIOP_FLUSH_BUFFER)
    SYM(SCSIOP_SPACE)
    SYM(SCSIOP_INQUIRY)
    SYM(SCSIOP_VERIFY6)
    SYM(SCSIOP_RECOVER_BUF_DATA)
    SYM(SCSIOP_MODE_SELECT)
    SYM(SCSIOP_RESERVE_UNIT)
    SYM(SCSIOP_RELEASE_UNIT)
    SYM(SCSIOP_COPY)
    SYM(SCSIOP_ERASE)
    SYM(SCSIOP_MODE_SENSE)
    SYM(SCSIOP_START_STOP_UNIT)
    //SYM(SCSIOP_STOP_PRINT)
    //SYM(SCSIOP_LOAD_UNLOAD)
    SYM(SCSIOP_RECEIVE_DIAGNOSTIC)
    SYM(SCSIOP_SEND_DIAGNOSTIC)
    SYM(SCSIOP_MEDIUM_REMOVAL)
    SYM(SCSIOP_READ_FORMATTED_CAPACITY)
    SYM(SCSIOP_READ_CAPACITY)
    SYM(SCSIOP_READ)
    SYM(SCSIOP_WRITE)
    SYM(SCSIOP_SEEK)
    //SYM(SCSIOP_LOCATE)
    //SYM(SCSIOP_POSITION_TO_ELEMENT)
    SYM(SCSIOP_WRITE_VERIFY)
    SYM(SCSIOP_VERIFY)
    SYM(SCSIOP_SEARCH_DATA_HIGH)
    SYM(SCSIOP_SEARCH_DATA_EQUAL)
    SYM(SCSIOP_SEARCH_DATA_LOW)
    SYM(SCSIOP_SET_LIMITS)
    SYM(SCSIOP_READ_POSITION)
    SYM(SCSIOP_SYNCHRONIZE_CACHE)
    SYM(SCSIOP_COMPARE)
    SYM(SCSIOP_COPY_COMPARE)
    SYM(SCSIOP_WRITE_DATA_BUFF)
    SYM(SCSIOP_READ_DATA_BUFF)
    SYM(SCSIOP_WRITE_LONG)
    SYM(SCSIOP_CHANGE_DEFINITION)
    SYM(SCSIOP_WRITE_SAME)
    SYM(SCSIOP_READ_SUB_CHANNEL)
    //SYM(SCSIOP_UNMAP)
    SYM(SCSIOP_READ_TOC)
    SYM(SCSIOP_READ_HEADER)
    //SYM(SCSIOP_REPORT_DENSITY_SUPPORT)
    SYM(SCSIOP_PLAY_AUDIO)
    SYM(SCSIOP_GET_CONFIGURATION)
    SYM(SCSIOP_PLAY_AUDIO_MSF)
    SYM(SCSIOP_PLAY_TRACK_INDEX)
    //SYM(SCSIOP_SANITIZE)
    SYM(SCSIOP_PLAY_TRACK_RELATIVE)
    SYM(SCSIOP_GET_EVENT_STATUS)
    SYM(SCSIOP_PAUSE_RESUME)
    SYM(SCSIOP_LOG_SELECT)
    SYM(SCSIOP_LOG_SENSE)
    SYM(SCSIOP_STOP_PLAY_SCAN)
    SYM(SCSIOP_XDWRITE)
    SYM(SCSIOP_XPWRITE)
    //SYM(SCSIOP_READ_DISK_INFORMATION)
    //SYM(SCSIOP_READ_DISC_INFORMATION)
    SYM(SCSIOP_READ_TRACK_INFORMATION)
    SYM(SCSIOP_XDWRITE_READ)
    //SYM(SCSIOP_RESERVE_TRACK_RZONE)
    SYM(SCSIOP_SEND_OPC_INFORMATION)
    SYM(SCSIOP_MODE_SELECT10)
    SYM(SCSIOP_RESERVE_UNIT10)
    //SYM(SCSIOP_RESERVE_ELEMENT)
    SYM(SCSIOP_RELEASE_UNIT10)
    //SYM(SCSIOP_RELEASE_ELEMENT)
    SYM(SCSIOP_REPAIR_TRACK)
    SYM(SCSIOP_MODE_SENSE10)
    SYM(SCSIOP_CLOSE_TRACK_SESSION)
    SYM(SCSIOP_READ_BUFFER_CAPACITY)
    SYM(SCSIOP_SEND_CUE_SHEET)
    SYM(SCSIOP_PERSISTENT_RESERVE_IN)
    SYM(SCSIOP_PERSISTENT_RESERVE_OUT)
    SYM(SCSIOP_REPORT_LUNS)
    SYM(SCSIOP_BLANK)
    //SYM(SCSIOP_ATA_PASSTHROUGH12)
    SYM(SCSIOP_SEND_EVENT)
    //SYM(SCSIOP_SECURITY_PROTOCOL_IN)
    SYM(SCSIOP_SEND_KEY)
    //SYM(SCSIOP_MAINTENANCE_IN)
    SYM(SCSIOP_REPORT_KEY)
    //SYM(SCSIOP_MAINTENANCE_OUT)
    SYM(SCSIOP_MOVE_MEDIUM)
    SYM(SCSIOP_LOAD_UNLOAD_SLOT)
    //SYM(SCSIOP_EXCHANGE_MEDIUM)
    SYM(SCSIOP_SET_READ_AHEAD)
    //SYM(SCSIOP_MOVE_MEDIUM_ATTACHED)
    SYM(SCSIOP_READ12)
    //SYM(SCSIOP_GET_MESSAGE)
    SYM(SCSIOP_SERVICE_ACTION_OUT12)
    SYM(SCSIOP_WRITE12)
    SYM(SCSIOP_SEND_MESSAGE)
    //SYM(SCSIOP_SERVICE_ACTION_IN12)
    SYM(SCSIOP_GET_PERFORMANCE)
    SYM(SCSIOP_READ_DVD_STRUCTURE)
    SYM(SCSIOP_WRITE_VERIFY12)
    SYM(SCSIOP_VERIFY12)
    SYM(SCSIOP_SEARCH_DATA_HIGH12)
    SYM(SCSIOP_SEARCH_DATA_EQUAL12)
    SYM(SCSIOP_SEARCH_DATA_LOW12)
    SYM(SCSIOP_SET_LIMITS12)
    SYM(SCSIOP_READ_ELEMENT_STATUS_ATTACHED)
    SYM(SCSIOP_REQUEST_VOL_ELEMENT)
    //SYM(SCSIOP_SECURITY_PROTOCOL_OUT)
    SYM(SCSIOP_SEND_VOLUME_TAG)
    //SYM(SCSIOP_SET_STREAMING)
    SYM(SCSIOP_READ_DEFECT_DATA)
    SYM(SCSIOP_READ_ELEMENT_STATUS)
    SYM(SCSIOP_READ_CD_MSF)
    SYM(SCSIOP_SCAN_CD)
    //SYM(SCSIOP_REDUNDANCY_GROUP_IN)
    SYM(SCSIOP_SET_CD_SPEED)
    //SYM(SCSIOP_REDUNDANCY_GROUP_OUT)
    SYM(SCSIOP_PLAY_CD)
    //SYM(SCSIOP_SPARE_IN)
    SYM(SCSIOP_MECHANISM_STATUS)
    //SYM(SCSIOP_SPARE_OUT)
    SYM(SCSIOP_READ_CD)
    //SYM(SCSIOP_VOLUME_SET_IN)
    SYM(SCSIOP_SEND_DVD_STRUCTURE)
    //SYM(SCSIOP_VOLUME_SET_OUT)
    SYM(SCSIOP_INIT_ELEMENT_RANGE)
    SYM(SCSIOP_XDWRITE_EXTENDED16)
    //SYM(SCSIOP_WRITE_FILEMARKS16)
    SYM(SCSIOP_REBUILD16)
    //SYM(SCSIOP_READ_REVERSE16)
    SYM(SCSIOP_REGENERATE16)
    SYM(SCSIOP_EXTENDED_COPY)
    //SYM(SCSIOP_POPULATE_TOKEN)
    //SYM(SCSIOP_WRITE_USING_TOKEN)
    SYM(SCSIOP_RECEIVE_COPY_RESULTS)
    //SYM(SCSIOP_RECEIVE_ROD_TOKEN_INFORMATION)
    SYM(SCSIOP_ATA_PASSTHROUGH16)
    SYM(SCSIOP_ACCESS_CONTROL_IN)
    SYM(SCSIOP_ACCESS_CONTROL_OUT)
    SYM(SCSIOP_READ16)
    SYM(SCSIOP_COMPARE_AND_WRITE)
    SYM(SCSIOP_WRITE16)
    SYM(SCSIOP_READ_ATTRIBUTES)
    SYM(SCSIOP_WRITE_ATTRIBUTES)
    SYM(SCSIOP_WRITE_VERIFY16)
    SYM(SCSIOP_VERIFY16)
    SYM(SCSIOP_PREFETCH16)
    SYM(SCSIOP_SYNCHRONIZE_CACHE16)
    //SYM(SCSIOP_SPACE16)
    SYM(SCSIOP_LOCK_UNLOCK_CACHE16)
    //SYM(SCSIOP_LOCATE16)
    SYM(SCSIOP_WRITE_SAME16)
    //SYM(SCSIOP_ERASE16)
    SYM(SCSIOP_READ_CAPACITY16)
    //SYM(SCSIOP_GET_LBA_STATUS)
    //SYM(SCSIOP_SERVICE_ACTION_IN16)
    SYM(SCSIOP_SERVICE_ACTION_OUT16)
    SYM(SCSIOP_OPERATION32)
    default:
        return "CdbOperationCodeSym:Unknown";
    }
}

const char *SrbStringize(PVOID Srb, char Buffer[], size_t Size)
{
    ULONG Function = SrbGetSrbFunction(Srb);
    UCHAR Status = SrbGetSrbStatus(Srb);

    switch (Function)
    {
    case SRB_FUNCTION_EXECUTE_SCSI:
        if (SRB_STATUS_ERROR != SRB_STATUS(Status))
        {
            RtlStringCbPrintfA(Buffer, Size,
                "%s[%s], Lun=%u:%u:%u, DataTransferLength=%lu, SrbStatus=%s%s",
                SrbFunctionSym(Function), CdbOperationCodeSym(SrbGetCdb(Srb)->AsByte[0]),
                SrbGetPathId(Srb), SrbGetTargetId(Srb), SrbGetLun(Srb),
                SrbGetDataTransferLength(Srb),
                SrbStatusSym(Status), SrbStatusMaskSym(Status));
        }
        else
        {
            UCHAR SenseInfoBufferLength = SrbGetSenseInfoBufferLength(Srb);
            PSENSE_DATA SenseInfoBuffer = SrbGetSenseInfoBuffer(Srb);

            if (0 != SenseInfoBuffer &&
                sizeof(SENSE_DATA) <= SenseInfoBufferLength &&
                !FlagOn(SrbGetSrbFlags(Srb), SRB_FLAGS_DISABLE_AUTOSENSE) &&
                SenseInfoBuffer->Valid &&
                FlagOn(Status, SRB_STATUS_AUTOSENSE_VALID))
            {
                RtlStringCbPrintfA(Buffer, Size,
                    "%s[%s], Lun=%u:%u:%u, ScsiStatus=%u, SenseData={KEY=%u,ASC=%u}, SrbStatus=%s%s",
                    SrbFunctionSym(Function), CdbOperationCodeSym(SrbGetCdb(Srb)->AsByte[0]),
                    SrbGetPathId(Srb), SrbGetTargetId(Srb), SrbGetLun(Srb),
                    SrbGetScsiStatus(Srb),
                    SenseInfoBuffer->SenseKey, SenseInfoBuffer->AdditionalSenseCode,
                    SrbStatusSym(Status), SrbStatusMaskSym(Status));
            }
            else
            {
                RtlStringCbPrintfA(Buffer, Size,
                    "%s[%s], Lun=%u:%u:%u, ScsiStatus=%u, SrbStatus=%s%s",
                    SrbFunctionSym(Function), CdbOperationCodeSym(SrbGetCdb(Srb)->AsByte[0]),
                    SrbGetPathId(Srb), SrbGetTargetId(Srb), SrbGetLun(Srb),
                    SrbGetScsiStatus(Srb),
                    SrbStatusSym(Status), SrbStatusMaskSym(Status));
            }
        }
        break;

    default:
        RtlStringCbPrintfA(Buffer, Size,
            "%s, SrbStatus=%s%s",
            SrbFunctionSym(Function),
            SrbStatusSym(Status), SrbStatusMaskSym(Status));
        break;
    }

    return Buffer;
}

#endif
