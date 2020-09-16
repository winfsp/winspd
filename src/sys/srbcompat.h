/**
 * @file sys/srbcompat.h
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

#ifndef WINSPD_SYS_SRBCOMPAT_H_INCLUDED
#define WINSPD_SYS_SRBCOMPAT_H_INCLUDED

#define SRBHELPER_ASSERT ASSERT
#include <srbhelper.h>

/* SRB compatiblity functions (missing when NTDDI_VERSION < NTDDI_WIN8) */
#if NTDDI_VERSION < NTDDI_WIN8

FORCEINLINE BOOLEAN
SrbCopySrb(
    _In_ PVOID DestinationSrb,
    _In_ ULONG DestinationSrbLength,
    _In_ PVOID SourceSrb)
{
    BOOLEAN Result = FALSE;

    if (DestinationSrbLength >= SCSI_REQUEST_BLOCK_SIZE)
    {
        RtlCopyMemory(DestinationSrb, SourceSrb, SCSI_REQUEST_BLOCK_SIZE);
        Result = TRUE;
    }

    return Result;
}

FORCEINLINE VOID
SrbZeroSrb(
    _In_ PVOID Srb)
{
    UCHAR Function = ((PSCSI_REQUEST_BLOCK)Srb)->Function;
    USHORT Length = ((PSCSI_REQUEST_BLOCK)Srb)->Length;

    RtlZeroMemory(Srb, sizeof(SCSI_REQUEST_BLOCK));

    ((PSCSI_REQUEST_BLOCK)Srb)->Function = Function;
    ((PSCSI_REQUEST_BLOCK)Srb)->Length = Length;
}

FORCEINLINE ULONG
SrbGetSrbLength(
    _In_ PVOID Srb)
{
    UNREFERENCED_PARAMETER(Srb);

    return sizeof(SCSI_REQUEST_BLOCK);
}

FORCEINLINE PCDB
SrbGetCdb(
    _In_ PVOID Srb)
{
    return (PCDB)((PSCSI_REQUEST_BLOCK)Srb)->Cdb;
}

FORCEINLINE ULONG
SrbGetSrbFunction(
    _In_ PVOID Srb)
{
    return (ULONG)((PSCSI_REQUEST_BLOCK)Srb)->Function;
}

FORCEINLINE PVOID
SrbGetSenseInfoBuffer(
    _In_ PVOID Srb)
{
    return ((PSCSI_REQUEST_BLOCK)Srb)->SenseInfoBuffer;
}

FORCEINLINE UCHAR
SrbGetSenseInfoBufferLength(
    _In_ PVOID Srb)
{
    return ((PSCSI_REQUEST_BLOCK)Srb)->SenseInfoBufferLength;
}

FORCEINLINE VOID
SrbSetSenseInfoBuffer(
    _In_ PVOID Srb,
    _In_opt_ PVOID SenseInfoBuffer)
{
    ((PSCSI_REQUEST_BLOCK)Srb)->SenseInfoBuffer = SenseInfoBuffer;
}

FORCEINLINE VOID
SrbSetSenseInfoBufferLength(
    _In_ PVOID Srb,
    _In_ UCHAR SenseInfoBufferLength)
{
    ((PSCSI_REQUEST_BLOCK)Srb)->SenseInfoBufferLength = SenseInfoBufferLength;
}

FORCEINLINE PVOID
SrbGetOriginalRequest(
    _In_ PVOID Srb)
{
    return ((PSCSI_REQUEST_BLOCK)Srb)->OriginalRequest;
}

FORCEINLINE VOID
SrbSetOriginalRequest(
    _In_ PVOID Srb,
    _In_opt_ PVOID OriginalRequest)
{
    ((PSCSI_REQUEST_BLOCK)Srb)->OriginalRequest = OriginalRequest;
}

FORCEINLINE PVOID
SrbGetDataBuffer(
    _In_ PVOID Srb)
{
    return ((PSCSI_REQUEST_BLOCK)Srb)->DataBuffer;
}

FORCEINLINE VOID
SrbSetDataBuffer(
    _In_ PVOID Srb,
    _In_opt_ __drv_aliasesMem PVOID DataBuffer)
{
    ((PSCSI_REQUEST_BLOCK)Srb)->DataBuffer = DataBuffer;
}

FORCEINLINE ULONG
SrbGetDataTransferLength(
    _In_ PVOID Srb)
{
    return ((PSCSI_REQUEST_BLOCK)Srb)->DataTransferLength;
}

FORCEINLINE VOID
SrbSetDataTransferLength(
    _In_ PVOID Srb,
    _In_ ULONG DataTransferLength)
{
    ((PSCSI_REQUEST_BLOCK)Srb)->DataTransferLength = DataTransferLength;
}

FORCEINLINE ULONG
SrbGetTimeOutValue(
    _In_ PVOID Srb)
{
    return ((PSCSI_REQUEST_BLOCK)Srb)->TimeOutValue;
}

FORCEINLINE VOID
SrbSetTimeOutValue(
    _In_ PVOID Srb,
    _In_ ULONG TimeOutValue)
{
    ((PSCSI_REQUEST_BLOCK)Srb)->TimeOutValue = TimeOutValue;
}

FORCEINLINE VOID
SrbSetQueueSortKey(
    _In_ PVOID Srb,
    _In_ ULONG QueueSortKey)
{
    ((PSCSI_REQUEST_BLOCK)Srb)->QueueSortKey = QueueSortKey;
}

FORCEINLINE VOID
SrbSetQueueTag(
    _In_ PVOID Srb,
    _In_ ULONG QueueTag)
{
    ((PSCSI_REQUEST_BLOCK)Srb)->QueueTag = (UCHAR)QueueTag;
}

#define SrbSetRequestTag SrbSetQueueTag

FORCEINLINE ULONG
SrbGetQueueTag(
    _In_ PVOID Srb)
{
    return ((PSCSI_REQUEST_BLOCK)Srb)->QueueTag;
}

#define SrbGetRequestTag SrbGetQueueTag

FORCEINLINE PVOID
SrbGetNextSrb(
    _In_ PVOID Srb)
{
    return (PVOID)((PSCSI_REQUEST_BLOCK)Srb)->NextSrb;
}

FORCEINLINE VOID
SrbSetNextSrb(
    _In_ PVOID Srb,
    _In_opt_ PVOID NextSrb)
{
    ((PSCSI_REQUEST_BLOCK)Srb)->NextSrb = (PSCSI_REQUEST_BLOCK)NextSrb;
}

FORCEINLINE ULONG
SrbGetSrbFlags(
    _In_ PVOID Srb)
{
    return ((PSCSI_REQUEST_BLOCK)Srb)->SrbFlags;
}

FORCEINLINE VOID
SrbAssignSrbFlags(
    _In_ PVOID Srb,
    _In_ ULONG Flags)
{
    ((PSCSI_REQUEST_BLOCK)Srb)->SrbFlags = Flags;
}

FORCEINLINE VOID
SrbSetSrbFlags(
    _In_ PVOID Srb,
    _In_ ULONG Flags)
{
    ((PSCSI_REQUEST_BLOCK)Srb)->SrbFlags |= Flags;
}

FORCEINLINE VOID
SrbClearSrbFlags(
    _In_ PVOID Srb,
    _In_ ULONG Flags)
{
    ((PSCSI_REQUEST_BLOCK)Srb)->SrbFlags &= ~Flags;
}

FORCEINLINE ULONG
SrbGetSystemStatus(
    _In_ PVOID Srb)
{
    return ((PSCSI_REQUEST_BLOCK)Srb)->InternalStatus;
}

FORCEINLINE VOID
SrbSetSystemStatus(
    _In_ PVOID Srb,
    _In_ ULONG Status)
{
    ((PSCSI_REQUEST_BLOCK)Srb)->InternalStatus = Status;
}

FORCEINLINE UCHAR
SrbGetScsiStatus(
    _In_ PVOID Srb)
{
    return ((PSCSI_REQUEST_BLOCK)Srb)->ScsiStatus;
}

FORCEINLINE VOID
SrbSetScsiStatus(
    _In_ PVOID Srb,
    _In_ UCHAR ScsiStatus)
{
    ((PSCSI_REQUEST_BLOCK)Srb)->ScsiStatus = ScsiStatus;
}

FORCEINLINE UCHAR
SrbGetCdbLength(
    _In_ PVOID Srb)
{
    return ((PSCSI_REQUEST_BLOCK)Srb)->CdbLength;
}

FORCEINLINE VOID
SrbSetCdbLength(
    _In_ PVOID Srb,
    _In_ UCHAR CdbLength)
{
    ((PSCSI_REQUEST_BLOCK)Srb)->CdbLength = CdbLength;
}

FORCEINLINE ULONG
SrbGetRequestAttribute(
    _In_ PVOID Srb)
{
    return ((PSCSI_REQUEST_BLOCK)Srb)->QueueAction;
}

#define SrbGetQueueAction SrbGetRequestAttribute

FORCEINLINE VOID
SrbSetRequestAttribute(
    _In_ PVOID Srb,
    _In_ UCHAR RequestAttribute)
{
    ((PSCSI_REQUEST_BLOCK)Srb)->QueueAction = RequestAttribute;
}

#define SrbSetQueueAction SrbSetRequestAttribute

FORCEINLINE UCHAR
SrbGetPathId(
    _In_ PVOID Srb)
{
    return ((PSCSI_REQUEST_BLOCK)Srb)->PathId;
}

FORCEINLINE UCHAR
SrbGetTargetId(
    _In_ PVOID Srb)
{
    return ((PSCSI_REQUEST_BLOCK)Srb)->TargetId;
}

FORCEINLINE UCHAR
SrbGetLun(
    _In_ PVOID Srb)
{
    return ((PSCSI_REQUEST_BLOCK)Srb)->Lun;
}

FORCEINLINE VOID
SrbGetPathTargetLun(
    _In_ PVOID Srb,
    _In_opt_ PUCHAR PathId,
    _In_opt_ PUCHAR TargetId,
    _In_opt_ PUCHAR Lun)
{
    if (PathId != NULL)
        *PathId = ((PSCSI_REQUEST_BLOCK)Srb)->PathId;
    if (TargetId != NULL)
        *TargetId = ((PSCSI_REQUEST_BLOCK)Srb)->TargetId;
    if (Lun != NULL)
        *Lun = ((PSCSI_REQUEST_BLOCK)Srb)->Lun;
}

FORCEINLINE PVOID
SrbGetMiniportContext(
    _In_ PVOID Srb)
{
    return ((PSCSI_REQUEST_BLOCK)Srb)->SrbExtension;
}

FORCEINLINE UCHAR
SrbGetSrbStatus(
    _In_ PVOID Srb)
{
    return ((PSCSI_REQUEST_BLOCK)Srb)->SrbStatus;
}

FORCEINLINE VOID
SrbSetSrbStatus(
    _In_ PVOID Srb,
    _In_ UCHAR Status)
{
    if (((PSCSI_REQUEST_BLOCK)Srb)->SrbStatus & SRB_STATUS_AUTOSENSE_VALID)
        ((PSCSI_REQUEST_BLOCK)Srb)->SrbStatus = Status | SRB_STATUS_AUTOSENSE_VALID;
    else
        ((PSCSI_REQUEST_BLOCK)Srb)->SrbStatus = Status;
}

#endif

#endif
