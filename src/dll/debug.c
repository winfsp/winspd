/**
 * @file dll/debug.c
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

#include <winspd/winspd.h>
#include <shared/minimal.h>
#include <stdarg.h>

static INIT_ONCE SpdDiagIdentInitOnce = INIT_ONCE_STATIC_INIT;
static WCHAR SpdDiagIdentBuf[20];

static BOOL WINAPI SpdDiagIdentInitialize(
    PINIT_ONCE InitOnce, PVOID Parameter, PVOID *Context)
{
    WCHAR ModuleFileName[MAX_PATH];
    PWSTR ModuleBaseName;

    if (0 == GetModuleFileNameW(0, ModuleFileName, sizeof ModuleFileName / sizeof(WCHAR)))
        lstrcpyW(ModuleFileName, L"UNKNOWN");

    ModuleBaseName = ModuleFileName;
    for (PWSTR P = ModuleBaseName, Dot = 0;; P++)
    {
        if (L'\0' == *P)
        {
            if (0 != Dot)
                *Dot = L'\0';
            break;
        }
        else if (L'\\' == *P)
            ModuleBaseName = P;
        else if (L'.' == *P)
            Dot = P;
    }

    lstrcpynW(SpdDiagIdentBuf, ModuleBaseName, sizeof SpdDiagIdentBuf / sizeof(WCHAR));
    SpdDiagIdentBuf[(sizeof SpdDiagIdentBuf / sizeof(WCHAR)) - 1] = L'\0';

    return TRUE;
}

static PWSTR SpdDiagIdent(VOID)
{
    InitOnceExecuteOnce(&SpdDiagIdentInitOnce, SpdDiagIdentInitialize, 0, 0);
    return SpdDiagIdentBuf;
}

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
        SpdDebugLog("%S[TID=%04lx]: %p: >>Read "
            "BlockAddress=%lx:%lx, Address=%p, Length=%u, ForceUnitAccess=%u\n",
            SpdDiagIdent(), GetCurrentThreadId(), (PVOID)Request->Hint,
            MAKE_UINT32_PAIR(Request->Op.Read.BlockAddress),
            (PVOID)Request->Op.Read.Address,
            (unsigned)Request->Op.Read.Length,
            (unsigned)Request->Op.Read.ForceUnitAccess);
        break;
    case SpdIoctlTransactWriteKind:
        SpdDebugLog("%S[TID=%04lx]: %p: >>Write "
            "BlockAddress=%lx:%lx, Address=%p, Length=%u, ForceUnitAccess=%u\n",
            SpdDiagIdent(), GetCurrentThreadId(), (PVOID)Request->Hint,
            MAKE_UINT32_PAIR(Request->Op.Write.BlockAddress),
            (PVOID)Request->Op.Write.Address,
            (unsigned)Request->Op.Write.Length,
            (unsigned)Request->Op.Write.ForceUnitAccess);
        break;
    case SpdIoctlTransactFlushKind:
        SpdDebugLog("%S[TID=%04lx]: %p: >>Flush "
            "BlockAddress=%lx:%lx, Length=%u, ForceUnitAccess=%u\n",
            SpdDiagIdent(), GetCurrentThreadId(), (PVOID)Request->Hint,
            MAKE_UINT32_PAIR(Request->Op.Flush.BlockAddress),
            (unsigned)Request->Op.Flush.Length);
        break;
    case SpdIoctlTransactUnmapKind:
        SpdDebugLog("%S[TID=%04lx]: %p: >>Unmap "
            "Count=%u\n",
            SpdDiagIdent(), GetCurrentThreadId(), (PVOID)Request->Hint,
            (unsigned)Request->Op.Unmap.Count);
        break;
    default:
        SpdDebugLog("%S[TID=%04lx]: %p: >>INVALID\n",
            SpdDiagIdent(), GetCurrentThreadId(), (PVOID)Request->Hint);
        break;
    }
}

VOID SpdDebugLogResponse(SPD_IOCTL_TRANSACT_RSP *Response)
{
    if (-1 == Response->Status.ScsiStatus)
        return;

    switch (Response->Kind)
    {
    case SpdIoctlTransactReadKind:
        SpdDebugLog("%S[TID=%04lx]: %p: <<Read Status=%u\n",
            SpdDiagIdent(), GetCurrentThreadId(), (PVOID)Response->Hint,
            (unsigned)Response->Status.ScsiStatus);
        break;
    case SpdIoctlTransactWriteKind:
        SpdDebugLog("%S[TID=%04lx]: %p: <<Write Status=%u\n",
            SpdDiagIdent(), GetCurrentThreadId(), (PVOID)Response->Hint,
            (unsigned)Response->Status.ScsiStatus);
        break;
    case SpdIoctlTransactFlushKind:
        SpdDebugLog("%S[TID=%04lx]: %p: <<Flush Status=%u\n",
            SpdDiagIdent(), GetCurrentThreadId(), (PVOID)Response->Hint,
            (unsigned)Response->Status.ScsiStatus);
        break;
    case SpdIoctlTransactUnmapKind:
        SpdDebugLog("%S[TID=%04lx]: %p: <<Unmap Status=%u\n",
            SpdDiagIdent(), GetCurrentThreadId(), (PVOID)Response->Hint,
            (unsigned)Response->Status.ScsiStatus);
        break;
    default:
        SpdDebugLog("%S[TID=%04lx]: %p: <<INVALID Status=%u\n",
            SpdDiagIdent(), GetCurrentThreadId(), (PVOID)Response->Hint,
            (unsigned)Response->Status.ScsiStatus);
        break;
    }
}
