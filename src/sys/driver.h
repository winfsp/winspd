/**
 * @file sys/driver.h
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

#ifndef WINSPD_SYS_DRIVER_H_INCLUDED
#define WINSPD_SYS_DRIVER_H_INCLUDED

#define WINSPD_SYS_INTERNAL

#define POOL_NX_OPTIN                   1
#include <ntifs.h>
#include <storport.h>
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
    spd_debug_dp_generic                = 0x00010000,   /* generic DbgPrint switch */
    spd_debug_dp_drvrld                 = 0x00020000,   /* DriverEntry DbgPrint switch */
    spd_debug_dp_tracing                = 0x00040000,   /* tracing functions DbgPrint switch */
    spd_debug_dp_adapter                = 0x00080000,   /* adapter functions DbgPrint switch */
    spd_debug_dp_ioctl                  = 0x00100000,   /* ioctl functions DbgPrint switch */
    spd_debug_dp_io                     = 0x00200000,   /* io functions DbgPrint switch */
};
extern __declspec(selectany) int spd_debug =
    spd_debug_bp_drvrld | spd_debug_dp_drvrld;
const char *SrbFunctionSym(ULONG Function);
const char *SrbStatusSym(ULONG Status);
const char *SrbStatusMaskSym(ULONG Status);
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
#define DEBUGLOG_EX(category, fmt, ...) \
    ((void)((spd_debug & spd_debug_dp_ ## category) ?\
        DbgPrint("[%d] " DRIVER_NAME "!" __FUNCTION__ ": " fmt "\n", KeGetCurrentIrql(), __VA_ARGS__) :\
        0))
#else
#define DEBUGLOG_EX(category, fmt, ...) ((void)0)
#endif
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
#define SPD_LEAVE(category, fmt, rfmt, ...)\
    SPD_LEAVE_(SPD_DEBUGLOG_(category, fmt, rfmt, __VA_ARGS__))
#define SPD_RETURN(...)                 \
    do                                  \
    {                                   \
        __VA_ARGS__;                    \
        goto spd_leave_label;           \
    } while (0,0)

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
    ASSERT(SRB_STATUS_PENDING != SrbStatus);
    SrbSetSrbStatus(Srb, SrbStatus);
    StorPortNotification(RequestComplete, DeviceExtension, Srb);
}
UCHAR SpdSrbExecuteScsi(PVOID DeviceExtension, PVOID Srb);
UCHAR SpdSrbAbortCommand(PVOID DeviceExtension, PVOID Srb);
UCHAR SpdSrbResetBus(PVOID DeviceExtension, PVOID Srb);
UCHAR SpdSrbResetDevice(PVOID DeviceExtension, PVOID Srb);
UCHAR SpdSrbFlush(PVOID DeviceExtension, PVOID Srb);
UCHAR SpdSrbShutdown(PVOID DeviceExtension, PVOID Srb);
UCHAR SpdSrbIoControl(PVOID DeviceExtension, PVOID Srb);
UCHAR SpdSrbPnp(PVOID DeviceExtension, PVOID Srb);
UCHAR SpdSrbWmi(PVOID DeviceExtension, PVOID Srb);
UCHAR SpdSrbDumpPointers(PVOID DeviceExtension, PVOID Srb);
UCHAR SpdSrbFreeDumpPointers(PVOID DeviceExtension, PVOID Srb);

/* extensions */
typedef struct
{
    UINT32 dummy;
} SPD_DEVICE_EXTENSION;
typedef struct
{
    UINT32 dummy;
} SPD_LOGICAL_UNIT;

#endif
