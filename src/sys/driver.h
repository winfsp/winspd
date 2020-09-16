/**
 * @file sys/driver.h
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

#ifndef WINSPD_SYS_DRIVER_H_INCLUDED
#define WINSPD_SYS_DRIVER_H_INCLUDED

#define WINSPD_SYS_INTERNAL

#define POOL_NX_OPTIN                   1
#include <ntifs.h>
#include <ntstrsafe.h>
#include <storport.h>
#include <ntddscsi.h>
#include <winspd/ioctl.h>
#include "srbcompat.h"

/* disable warnings */
#pragma warning(disable:4100)           /* unreferenced formal parameter */
#pragma warning(disable:4200)           /* zero-sized array in struct/union */

#define DRIVER_NAME                     SPD_IOCTL_DRIVER_NAME

/* debug */
#if DBG
enum
{
    spd_debug_bp_generic                = 0x00000001,   /* generic breakpoint switch */
    spd_debug_bp_drvrld                 = 0x00000002,   /* DriverEntry breakpoint switch */
    spd_debug_bp_tracing                = 0x00000004,   /* tracing functions breakpoint switch */
    spd_debug_bp_adapter                = 0x00000008,   /* adapter functions breakpoint switch */
    spd_debug_bp_ioctl                  = 0x00000010,   /* ioctl functions breakpoint switch */
    spd_debug_bp_io                     = 0x00000020,   /* io functions breakpoint switch */
    spd_debug_bp_irp                    = 0x00000040,   /* irp breakpoint switch */
    spd_debug_dp_generic                = 0x00010000,   /* generic DbgPrint switch */
    spd_debug_dp_drvrld                 = 0x00020000,   /* DriverEntry DbgPrint switch */
    spd_debug_dp_tracing                = 0x00040000,   /* tracing functions DbgPrint switch */
    spd_debug_dp_adapter                = 0x00080000,   /* adapter functions DbgPrint switch */
    spd_debug_dp_ioctl                  = 0x00100000,   /* ioctl functions DbgPrint switch */
    spd_debug_dp_io                     = 0x00200000,   /* io functions DbgPrint switch */
    spd_debug_dp_irp                    = 0x00400000,   /* irp DbgPrint switch */
    spd_debug_dp_srb                    = 0x80000000,   /* srb completion DbgPrint switch */
    spd_debug_dp_srberr                 = 0x40000000,   /* srb error completion DbgPrint switch */
    spd_debug_dp                        = 0xffff0000,
};
extern __declspec(selectany) int spd_debug =
    spd_debug_bp_drvrld | spd_debug_dp_srberr;
const char *AdapterControlSym(ULONG Control);
const char *SrbFunctionSym(ULONG Function);
const char *SrbStatusSym(ULONG Status);
const char *SrbStatusMaskSym(ULONG Status);
const char *CdbOperationCodeSym(ULONG OperationCode);
const char *SrbStringize(PVOID Srb, char Buffer[], size_t Size);
#endif

/* DEBUGBREAK */
#if DBG
#define DEBUGBREAK_EX(category)         \
    do                                  \
    {                                   \
        static int bp = 1;              \
        if (bp && (spd_debug & spd_debug_bp_ ## category) && !KD_DEBUGGER_NOT_PRESENT)\
            DbgBreakPoint();            \
    } while (0,0)
#else
#define DEBUGBREAK_EX(category)         do {} while (0,0)
#endif
#define DEBUGBREAK()                    DEBUGBREAK_EX(generic)

/* DEBUGLOG */
#if DBG
#define DEBUGLOG_COND(cond, fmt, ...)   \
    ((void)((cond) ?\
        DbgPrint("[%d] " DRIVER_NAME "!" __FUNCTION__ ": " fmt "\n", KeGetCurrentIrql(), __VA_ARGS__) :\
        0))
#else
#define DEBUGLOG_COND(cond, fmt, ...)   ((void)0)
#endif
#define DEBUGLOG_EX(category, fmt, ...) DEBUGLOG_COND(spd_debug & spd_debug_dp_ ## category, fmt, __VA_ARGS__)
#define DEBUGLOG(fmt, ...)              DEBUGLOG_EX(generic, fmt, __VA_ARGS__)

/* SPD_ENTER/SPD_LEAVE */
#if DBG
#define SPD_DEBUGLOG_(category, fmt, rfmt, ...)\
    ((void)((spd_debug & spd_debug_dp_ ## category) ?\
        DbgPrint(AbnormalTermination() ?\
            "[%d] " DRIVER_NAME "!" __FUNCTION__ "(" fmt ") = *AbnormalTermination*\n" :\
            "[%d] " DRIVER_NAME "!" __FUNCTION__ "(" fmt ")" rfmt "\n"\
            , KeGetCurrentIrql(), __VA_ARGS__) :\
        0))
#else
#define SPD_DEBUGLOG_(category, fmt, rfmt, ...)\
    ((void)0)
#endif
#define SPD_ENTER_(category, ...)       \
    DEBUGBREAK_EX(category);            \
    try                                 \
    {                                   \
        __VA_ARGS__
#define SPD_LEAVE_(...)                 \
    goto spd_leave_label;               \
    spd_leave_label:;                   \
    }                                   \
    finally                             \
    {                                   \
        __VA_ARGS__;                    \
    }
#define SPD_ENTER(category, ...)        \
    SPD_ENTER_(category, __VA_ARGS__)
#define SPD_LEAVE_NOLOG(category)\
    SPD_LEAVE_()
#define SPD_LEAVE(category, fmt, rfmt, ...)\
    SPD_LEAVE_(SPD_DEBUGLOG_(category, fmt, rfmt, __VA_ARGS__))
#define SPD_RETURN(...)                 \
    do                                  \
    {                                   \
        __VA_ARGS__;                    \
        goto spd_leave_label;           \
    } while (0,0)

/* memory allocation */
#define SpdAllocNonPaged(Size, Tag)     ExAllocatePoolWithTag(NonPagedPool, Size, Tag)
#define SpdFree(Pointer, Tag)           ExFreePoolWithTag(Pointer, Tag)
#define SpdTagStorageUnit               'SdpS'
#define SpdTagIoq                       'QdpS'

/* hash mix */
/* Based on the MurmurHash3 fmix32/fmix64 function:
 * See: https://code.google.com/p/smhasher/source/browse/trunk/MurmurHash3.cpp?r=152#68
 */
static inline
UINT32 SpdHashMix32(UINT32 h)
{
    h ^= h >> 16;
    h *= 0x85ebca6b;
    h ^= h >> 13;
    h *= 0xc2b2ae35;
    h ^= h >> 16;
    return h;
}
static inline
UINT64 SpdHashMix64(UINT64 k)
{
    k ^= k >> 33;
    k *= 0xff51afd7ed558ccdULL;
    k ^= k >> 33;
    k *= 0xc4ceb9fe1a85ec53ULL;
    k ^= k >> 33;
    return k;
}
static inline
ULONG SpdHashMixPointer(PVOID Pointer)
{
#if _WIN64
    return (ULONG)SpdHashMix64((UINT64)Pointer);
#else
    return (ULONG)SpdHashMix32((UINT32)Pointer);
#endif
}

/* IRP dispatch */
DRIVER_DISPATCH SpdDispatchPnp;
extern PDRIVER_DISPATCH StorPortDispatchPnp;

/* virtual miniport functions */
HW_INITIALIZE_TRACING SpdHwInitializeTracing;
HW_CLEANUP_TRACING SpdHwCleanupTracing;
VIRTUAL_HW_FIND_ADAPTER SpdHwFindAdapter;
HW_INITIALIZE SpdHwInitialize;
HW_FREE_ADAPTER_RESOURCES SpdHwFreeAdapterResources;
HW_RESET_BUS SpdHwResetBus;
HW_ADAPTER_CONTROL SpdHwAdapterControl;
HW_PROCESS_SERVICE_REQUEST SpdHwProcessServiceRequest;
HW_COMPLETE_SERVICE_IRP SpdHwCompleteServiceIrp;
HW_STARTIO SpdHwStartIo;

/* I/O */
FORCEINLINE VOID SpdSrbComplete(PVOID DeviceExtension, PVOID Srb, UCHAR SrbStatus)
{
    ASSERT(SRB_STATUS_PENDING != SRB_STATUS(SrbStatus));
    SrbSetSrbStatus(Srb, SrbStatus);
#if DBG
    {
        char buf[1024];
        DEBUGLOG_COND(
            (spd_debug & spd_debug_dp_srb) ||
                ((SRB_STATUS_SUCCESS != SRB_STATUS(SrbStatus)) && (spd_debug & spd_debug_dp_srberr)),
            "%p, Srb=%p {%s}", DeviceExtension, Srb, SrbStringize(Srb, buf, sizeof buf));
    }
#endif
    StorPortNotification(RequestComplete, DeviceExtension, Srb);
}
UCHAR SpdSrbExecuteScsi(PVOID DeviceExtension, PVOID Srb);
VOID SpdSrbExecuteScsiPrepare(PVOID SrbExtension, PVOID Context, PVOID DataBuffer);
UCHAR SpdSrbExecuteScsiComplete(PVOID SrbExtension, PVOID Context, PVOID DataBuffer);
UCHAR SpdSrbAbortCommand(PVOID DeviceExtension, PVOID Srb);
UCHAR SpdSrbResetBus(PVOID DeviceExtension, PVOID Srb);
UCHAR SpdSrbResetDevice(PVOID DeviceExtension, PVOID Srb);
UCHAR SpdSrbResetLogicalUnit(PVOID DeviceExtension, PVOID Srb);
UCHAR SpdSrbFlush(PVOID DeviceExtension, PVOID Srb);
UCHAR SpdSrbShutdown(PVOID DeviceExtension, PVOID Srb);
UCHAR SpdSrbPnp(PVOID DeviceExtension, PVOID Srb);
UCHAR SpdSrbWmi(PVOID DeviceExtension, PVOID Srb);
NTSTATUS SpdNtStatusFromStorStatus(ULONG StorStatus);

/* PnP capabilities */
#define SpdPnpSetAdapterCapabilities(D) \
    do                                  \
    {                                   \
        (D)->Removable = 1;             \
        (D)->SilentInstall = 1;         \
        (D)->SurpriseRemovalOK = 1;     \
    } while (0,0)
#define SpdPnpSetDeviceCapabilities(D)  \
    do                                  \
    {                                   \
        (D)->Removable = 1;             \
        (D)->SilentInstall = 1;         \
        (D)->SurpriseRemovalOK = 1;     \
    } while (0,0)

/*
 * Queued Events
 *
 * Queued Events are an implementation of SynchronizationEvent's using
 * a KQUEUE, originally from WinFsp. For a discussion see:
 * https://github.com/billziss-gh/winfsp/wiki/Queued-Events
 */
typedef struct _SPD_QEVENT
{
    KQUEUE Queue;
    LIST_ENTRY DummyEntry;
    KSPIN_LOCK SpinLock;
} SPD_QEVENT;
static inline
VOID SpdQeventInitialize(SPD_QEVENT *Qevent, ULONG ThreadCount)
{
    KeInitializeQueue(&Qevent->Queue, ThreadCount);
    RtlZeroMemory(&Qevent->DummyEntry, sizeof Qevent->DummyEntry);
    KeInitializeSpinLock(&Qevent->SpinLock);
}
static inline
VOID SpdQeventFinalize(SPD_QEVENT *Qevent)
{
    KeRundownQueue(&Qevent->Queue);
}
static inline
VOID SpdQeventSetNoLock(SPD_QEVENT *Qevent)
{
    ASSERT(KeGetCurrentIrql() == DISPATCH_LEVEL);
    if (0 == KeReadStateQueue(&Qevent->Queue))
        KeInsertQueue(&Qevent->Queue, &Qevent->DummyEntry);
}
static inline
VOID SpdQeventSet(SPD_QEVENT *Qevent)
{
    KIRQL Irql;
    KeAcquireSpinLock(&Qevent->SpinLock, &Irql);
    SpdQeventSetNoLock(Qevent);
    KeReleaseSpinLock(&Qevent->SpinLock, Irql);
}
static inline
NTSTATUS SpdQeventWait(SPD_QEVENT *Qevent,
    KPROCESSOR_MODE WaitMode, BOOLEAN Alertable, PLARGE_INTEGER PTimeout)
{
    PLIST_ENTRY ListEntry;
    KeRemoveQueueEx(&Qevent->Queue, WaitMode, Alertable, PTimeout, &ListEntry, 1);
    if (ListEntry == &Qevent->DummyEntry)
        return STATUS_SUCCESS;
    return (NTSTATUS)(UINT_PTR)ListEntry;
}
static inline
NTSTATUS SpdQeventCancellableWait(SPD_QEVENT *Qevent,
    PLARGE_INTEGER PTimeout, PIRP Irp)
{
    NTSTATUS Result;
    UINT64 ExpirationTime = 0, InterruptTime;
    if (0 != PTimeout && 0 > PTimeout->QuadPart)
        ExpirationTime = KeQueryInterruptTime() - PTimeout->QuadPart;
retry:
    Result = SpdQeventWait(Qevent, KernelMode, TRUE, PTimeout);
    if (STATUS_ALERTED == Result)
    {
        if (PsIsThreadTerminating(PsGetCurrentThread()))
            return STATUS_THREAD_IS_TERMINATING;
        if (0 != Irp && Irp->Cancel)
            return STATUS_CANCELLED;
        if (0 != ExpirationTime)
        {
            InterruptTime = KeQueryInterruptTime();
            if (ExpirationTime <= InterruptTime)
                return STATUS_TIMEOUT;
            PTimeout->QuadPart = (INT64)InterruptTime - (INT64)ExpirationTime;
        }
        goto retry;
    }
    return Result;
}

/* I/O queue */
typedef struct
{
    PVOID DeviceExtension;
    KSPIN_LOCK SpinLock;
    BOOLEAN Stopped;
    SPD_QEVENT PendingEvent;
    LIST_ENTRY PendingList, ProcessList;
    ULONG ProcessBucketCount;
    PVOID ProcessBuckets[];
} SPD_IOQ;
NTSTATUS SpdIoqCreate(PVOID DeviceExtension, SPD_IOQ **PIoq);
VOID SpdIoqDelete(SPD_IOQ *Ioq);
VOID SpdIoqReset(SPD_IOQ *Ioq, BOOLEAN Stop);
BOOLEAN SpdIoqStopped(SPD_IOQ *Ioq);
NTSTATUS SpdIoqCancelSrb(SPD_IOQ *Ioq, PVOID Srb);
NTSTATUS SpdIoqPostSrb(SPD_IOQ *Ioq, PVOID Srb);
NTSTATUS SpdIoqStartProcessingSrb(SPD_IOQ *Ioq, PLARGE_INTEGER Timeout, PIRP CancellableIrp,
    VOID (*Prepare)(PVOID SrbExtension, PVOID Context, PVOID DataBuffer),
    PVOID Context, PVOID DataBuffer);
VOID SpdIoqEndProcessingSrb(SPD_IOQ *Ioq, UINT64 Hint,
    UCHAR (*Complete)(PVOID SrbExtension, PVOID Context, PVOID DataBuffer),
    PVOID Context, PVOID DataBuffer);
typedef struct _SPD_SRB_EXTENSION
{
    struct _SPD_STORAGE_UNIT *StorageUnit;
    LIST_ENTRY ListEntry;
    PVOID HashNext;
    PVOID Srb;
    PVOID SystemDataBuffer;
    ULONG SystemDataLength;
    ULONG ChunkOffset;
} SPD_SRB_EXTENSION;
#define SpdSrbExtension(Srb)            ((SPD_SRB_EXTENSION *)SrbGetMiniportContext(Srb))

/* storage units */
typedef struct _SPD_STORAGE_UNIT SPD_STORAGE_UNIT;
typedef struct _SPD_DEVICE_EXTENSION
{
    KSPIN_LOCK SpinLock;
    PDEVICE_OBJECT DeviceObject;        /* adapter device */
    ULONG StorageUnitCount, StorageUnitCapacity;
    SPD_STORAGE_UNIT *StorageUnits[];
} SPD_DEVICE_EXTENSION;
typedef struct _SPD_STORAGE_UNIT
{
    /* fields protected by SPD_DEVICE_EXTENSION::SpinLock */
    ULONG RefCount;
    /* fields below are read-only after construction */
    SPD_IOCTL_STORAGE_UNIT_PARAMS StorageUnitParams;
    CHAR SerialNumber[36];
    ULONG OwnerProcessId;
    SPD_IOQ *Ioq;
    /* fields not protected */
    PDEVICE_OBJECT DeviceObject;        /* disk device */
    ULONG TransactProcessId;
} SPD_STORAGE_UNIT;
NTSTATUS SpdDeviceExtensionInit(SPD_DEVICE_EXTENSION *DeviceExtension, PVOID BusInformation);
VOID SpdDeviceExtensionFini(SPD_DEVICE_EXTENSION *DeviceExtension);
SPD_DEVICE_EXTENSION *SpdDeviceExtensionAcquire(VOID);
VOID SpdDeviceExtensionRelease(SPD_DEVICE_EXTENSION *DeviceExtension);
NTSTATUS SpdStorageUnitProvision(
    SPD_DEVICE_EXTENSION *DeviceExtension,
    SPD_IOCTL_STORAGE_UNIT_PARAMS *StorageUnitParams,
    ULONG ProcessId,
    PUINT32 PBtl);
NTSTATUS SpdStorageUnitUnprovision(
    SPD_DEVICE_EXTENSION *DeviceExtension,
    PGUID Guid, ULONG Index,
    ULONG ProcessId);
SPD_STORAGE_UNIT *SpdStorageUnitReferenceByBtl(
    SPD_DEVICE_EXTENSION *DeviceExtension,
    UINT32 Btl);
VOID SpdStorageUnitDereference(
    SPD_DEVICE_EXTENSION *DeviceExtension,
    SPD_STORAGE_UNIT *StorageUnit);
ULONG SpdStorageUnitGetUseBitmap(
    SPD_DEVICE_EXTENSION *DeviceExtension,
    PULONG PProcessId,
    UINT8 Bitmap[32]);
NTSTATUS SpdStorageUnitGlobalSetDevice(
    PDEVICE_OBJECT DeviceObject);
SPD_STORAGE_UNIT *SpdStorageUnitGlobalReferenceByDevice(
    PDEVICE_OBJECT DeviceObject);
VOID SpdStorageUnitGlobalDereference(
    SPD_STORAGE_UNIT *StorageUnit);
static inline
SPD_STORAGE_UNIT *SpdStorageUnitReference(PVOID DeviceExtension, PVOID Srb)
{
    UCHAR PathId, TargetId, Lun;

    SrbGetPathTargetLun(Srb, &PathId, &TargetId, &Lun);
    return SpdStorageUnitReferenceByBtl(DeviceExtension, SPD_IOCTL_BTL(PathId, TargetId, Lun));
}
extern ERESOURCE SpdGlobalDeviceResource;
extern SPD_DEVICE_EXTENSION *SpdGlobalDeviceExtension;  /* protected by SpdGlobalDeviceResource */
extern ULONG SpdStorageUnitCapacity;                    /* read-only after DriverLoad */
#define SPD_INDEX_FROM_BTL(Btl)         SPD_IOCTL_BTL_T(Btl)
#define SPD_BTL_FROM_INDEX(Idx)         SPD_IOCTL_BTL(0, Idx, 0)

/* utility */
NTSTATUS SpdRegistryGetValue(PUNICODE_STRING Path, PUNICODE_STRING ValueName,
    PKEY_VALUE_PARTIAL_INFORMATION ValueInformation, PULONG PValueInformationLength);
VOID SpdMakeDeviceIoControlRequest(
    ULONG ControlCode,
    PDEVICE_OBJECT DeviceObject,
    PVOID InputBuffer,
    ULONG InputBufferLength,
    PVOID OutputBuffer,
    ULONG OutputBufferLength,
    PIO_STATUS_BLOCK IoStatus);
NTSTATUS SpdGetScsiAddress(
    PDEVICE_OBJECT DeviceObject,
    PSCSI_ADDRESS ScsiAddress);

/*
 * Fixes
 */

/* RtlEqualMemory: this is defined as memcmp, which does not exist on Win7 x86! */
#undef RtlEqualMemory
static inline
LOGICAL RtlEqualMemory(const VOID *Source1, const VOID *Source2, SIZE_T Length)
{
    return Length == RtlCompareMemory(Source1, Source2, Length);
}

#endif
