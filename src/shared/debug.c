/**
 * @file dll/debug.c
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

#include <shared/shared.h>
#include <stdarg.h>

PWSTR SpdDiagIdent(VOID);

static HANDLE SpdDebugLogHandle = INVALID_HANDLE_VALUE;

VOID SpdDebugLogSetHandle(HANDLE Handle)
{
    SpdDebugLogHandle = Handle;
}

VOID SpdDebugLog(const char *format, ...)
{
    char buf[1024];
        /* DbgPrint has a 512 byte limit, but wvsprintf is only safe with a 1024 byte buffer */
    va_list ap;
    va_start(ap, format);
    wvsprintfA(buf, format, ap);
    va_end(ap);
    buf[sizeof buf - 1] = '\0';
    if (INVALID_HANDLE_VALUE != SpdDebugLogHandle)
    {
        DWORD bytes;
        WriteFile(SpdDebugLogHandle, buf, lstrlenA(buf), &bytes, 0);
    }
    else
        OutputDebugStringA(buf);
}

#define MAKE_UINT32_PAIR(v)             \
    ((PLARGE_INTEGER)&(v))->HighPart, ((PLARGE_INTEGER)&(v))->LowPart

VOID SpdDebugLogRequest(SPD_IOCTL_TRANSACT_REQ *Request)
{
    switch (Request->Kind)
    {
    case SpdIoctlTransactReadKind:
        SpdDebugLog("%S[TID=%04lx]: %p: >>Read  "
            "BlockAddress=%lx:%lx, BlockCount=%u, FUA=%u\n",
            SpdDiagIdent(), GetCurrentThreadId(), (PVOID)Request->Hint,
            MAKE_UINT32_PAIR(Request->Op.Read.BlockAddress),
            (unsigned)Request->Op.Read.BlockCount,
            (unsigned)Request->Op.Read.ForceUnitAccess);
        break;
    case SpdIoctlTransactWriteKind:
        SpdDebugLog("%S[TID=%04lx]: %p: >>Write "
            "BlockAddress=%lx:%lx, BlockCount=%u, FUA=%u\n",
            SpdDiagIdent(), GetCurrentThreadId(), (PVOID)Request->Hint,
            MAKE_UINT32_PAIR(Request->Op.Write.BlockAddress),
            (unsigned)Request->Op.Write.BlockCount,
            (unsigned)Request->Op.Write.ForceUnitAccess);
        break;
    case SpdIoctlTransactFlushKind:
        SpdDebugLog("%S[TID=%04lx]: %p: >>Flush "
            "BlockAddress=%lx:%lx, BlockCount=%u\n",
            SpdDiagIdent(), GetCurrentThreadId(), (PVOID)Request->Hint,
            MAKE_UINT32_PAIR(Request->Op.Flush.BlockAddress),
            (unsigned)Request->Op.Flush.BlockCount);
        break;
    case SpdIoctlTransactUnmapKind:
        SpdDebugLog("%S[TID=%04lx]: %p: >>Unmap "
            "Count=%u\n",
            SpdDiagIdent(), GetCurrentThreadId(), (PVOID)Request->Hint,
            (unsigned)Request->Op.Unmap.Count);
        break;
    default:
        SpdDebugLog("%S[TID=%04lx]: %p: >>INVLD\n",
            SpdDiagIdent(), GetCurrentThreadId(), (PVOID)Request->Hint);
        break;
    }
}

static const char *SpdDebugLogScsiStatusSym(UINT8 ScsiStatus)
{
    switch (ScsiStatus)
    {
    case SCSISTAT_GOOD:
        return "GOOD";
    case SCSISTAT_CHECK_CONDITION:
        return "CHECK_CONDITION";
    case SCSISTAT_CONDITION_MET:
        return "CONDITION_MET";
    case SCSISTAT_BUSY:
        return "BUSY";
    case SCSISTAT_RESERVATION_CONFLICT:
        return "RESERVATION_CONFLICT";
    case SCSISTAT_QUEUE_FULL:
        return "TASK_SET_FULL";
    case 0x30:
        return "ACA_ACTIVE";
    case 0x40:
        return "TASK_ABORTED";
    default:
        return "UNKNOWN";
    }
}

static const char *SpdDebugLogSenseKeySym(UINT8 SenseKey)
{
    switch (SenseKey)
    {
    case SCSI_SENSE_NO_SENSE:
        return "NO_SENSE";
    case SCSI_SENSE_RECOVERED_ERROR:
        return "RECOVERED_ERROR";
    case SCSI_SENSE_NOT_READY:
        return "NOT_READY";
    case SCSI_SENSE_MEDIUM_ERROR:
        return "MEDIUM_ERROR";
    case SCSI_SENSE_HARDWARE_ERROR:
        return "HARDWARE_ERROR";
    case SCSI_SENSE_ILLEGAL_REQUEST:
        return "ILLEGAL_REQUEST";
    case SCSI_SENSE_UNIT_ATTENTION:
        return "UNIT_ATTENTION";
    case SCSI_SENSE_DATA_PROTECT:
        return "DATA_PROTECT";
    case SCSI_SENSE_BLANK_CHECK:
        return "BLANK_CHECK";
    case SCSI_SENSE_UNIQUE:
        return "VENDOR_SPECIFIC";
    case SCSI_SENSE_COPY_ABORTED:
        return "COPY_ABORTED";
    case SCSI_SENSE_ABORTED_COMMAND:
        return "ABORTED_COMMAND";
    case SCSI_SENSE_VOL_OVERFLOW:
        return "VOL_OVERFLOW";
    case SCSI_SENSE_MISCOMPARE:
        return "MISCOMPARE";
    default:
        return "UNKNOWN";
    }
}

static VOID SpdDebugLogResponseStatus(SPD_IOCTL_TRANSACT_RSP *Response, const char *Name)
{
    if (SCSISTAT_GOOD == Response->Status.ScsiStatus)
        SpdDebugLog("%S[TID=%04lx]: %p: <<%s Status=%s\n",
            SpdDiagIdent(), GetCurrentThreadId(), (PVOID)Response->Hint, Name,
            SpdDebugLogScsiStatusSym(Response->Status.ScsiStatus));
    else if (!Response->Status.InformationValid)
        SpdDebugLog("%S[TID=%04lx]: %p: <<%s Status=%u SenseKey=%s ASC/ASCQ=%lu/%lu\n",
            SpdDiagIdent(), GetCurrentThreadId(), (PVOID)Response->Hint, Name,
            SpdDebugLogScsiStatusSym(Response->Status.ScsiStatus),
            SpdDebugLogSenseKeySym(Response->Status.SenseKey),
            (unsigned)Response->Status.ASC,
            (unsigned)Response->Status.ASCQ);
    else
        SpdDebugLog("%S[TID=%04lx]: %p: <<%s Status=%u SenseKey=%s ASC/ASCQ=%lu/%lu Information=%lx:%lx\n",
            SpdDiagIdent(), GetCurrentThreadId(), (PVOID)Response->Hint, Name,
            SpdDebugLogScsiStatusSym(Response->Status.ScsiStatus),
            SpdDebugLogSenseKeySym(Response->Status.SenseKey),
            (unsigned)Response->Status.ASC,
            (unsigned)Response->Status.ASCQ,
            MAKE_UINT32_PAIR(Response->Status.Information));
}

VOID SpdDebugLogResponse(SPD_IOCTL_TRANSACT_RSP *Response)
{
    switch (Response->Kind)
    {
    case SpdIoctlTransactReadKind:
        SpdDebugLogResponseStatus(Response, "Read ");
        break;
    case SpdIoctlTransactWriteKind:
        SpdDebugLogResponseStatus(Response, "Write");
        break;
    case SpdIoctlTransactFlushKind:
        SpdDebugLogResponseStatus(Response, "Flush");
        break;
    case SpdIoctlTransactUnmapKind:
        SpdDebugLogResponseStatus(Response, "Unmap");
        break;
    default:
        SpdDebugLogResponseStatus(Response, "INVLD");
        break;
    }
}
