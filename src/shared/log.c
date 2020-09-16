/**
 * @file shared/log.c
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

VOID SpdPrintLog(HANDLE Handle, PWSTR Format, ...)
{
    va_list ap;

    va_start(ap, Format);
    SpdPrintLogV(Handle, Format, ap);
    va_end(ap);
}

VOID SpdPrintLogV(HANDLE Handle, PWSTR Format, va_list ap)
{
    WCHAR BufW[1024];
        /* wvsprintfW is only safe with a 1024 WCHAR buffer */
    PSTR BufA;
    DWORD Length;

    wvsprintfW(BufW, Format, ap);
    BufW[(sizeof BufW / sizeof BufW[0]) - 1] = L'\0';

    Length = lstrlenW(BufW);
    BufA = MemAlloc(Length * 3 + 1/* '\n' */);
    if (0 != BufA)
    {
        Length = WideCharToMultiByte(CP_UTF8, 0, BufW, Length, BufA, Length * 3 + 1/* '\n' */,
            0, 0);
        if (0 < Length)
        {
            BufA[Length++] = '\n';
            WriteFile(Handle, BufA, Length, &Length, 0);
        }
        MemFree(BufA);
    }
}

#define SPD_EVENTLOG_NAME               "WinSpd"
#define SPD_EVENTLOG_INFORMATION         0x60000001L
#define SPD_EVENTLOG_WARNING             0xA0000001L
#define SPD_EVENTLOG_ERROR               0xE0000001L

static INIT_ONCE SpdEventLogInitOnce = INIT_ONCE_STATIC_INIT;
static HANDLE SpdEventLogHandle;

static BOOL WINAPI SpdEventLogInitialize(
    PINIT_ONCE InitOnce, PVOID Parameter, PVOID *Context)
{
    SpdEventLogHandle = RegisterEventSourceW(0, L"" SPD_EVENTLOG_NAME);
    return TRUE;
}

VOID SpdEventLog(ULONG Type, PWSTR Format, ...)
{
    va_list ap;

    va_start(ap, Format);
    SpdEventLogV(Type, Format, ap);
    va_end(ap);
}

VOID SpdEventLogV(ULONG Type, PWSTR Format, va_list ap)
{
    InitOnceExecuteOnce(&SpdEventLogInitOnce, SpdEventLogInitialize, 0, 0);
    if (0 == SpdEventLogHandle)
        return;

    WCHAR Buf[1024], *Strings[2];
        /* wvsprintfW is only safe with a 1024 WCHAR buffer */
    DWORD EventId;

    Strings[0] = SpdDiagIdent();

    wvsprintfW(Buf, Format, ap);
    Buf[(sizeof Buf / sizeof Buf[0]) - 1] = L'\0';
    Strings[1] = Buf;

    switch (Type)
    {
    default:
    case EVENTLOG_INFORMATION_TYPE:
    case EVENTLOG_SUCCESS:
        EventId = SPD_EVENTLOG_INFORMATION;
        break;
    case EVENTLOG_WARNING_TYPE:
        EventId = SPD_EVENTLOG_WARNING;
        break;
    case EVENTLOG_ERROR_TYPE:
        EventId = SPD_EVENTLOG_ERROR;
        break;
    }

    ReportEventW(SpdEventLogHandle, (WORD)Type, 0, EventId, 0, 2, 0, Strings, 0);
}

static BOOLEAN SpdIsInteractive(VOID)
{
    /*
     * Modeled after System.Environment.UserInteractive.
     * See http://referencesource.microsoft.com/#mscorlib/system/environment.cs,947ad026e7cb830c
     */
    static HWINSTA ProcessWindowStation;
    static BOOLEAN IsInteractive;
    HWINSTA CurrentWindowStation;
    USEROBJECTFLAGS Flags;

    CurrentWindowStation = GetProcessWindowStation();
    if (0 != CurrentWindowStation && ProcessWindowStation != CurrentWindowStation)
    {
        if (GetUserObjectInformationW(CurrentWindowStation, UOI_FLAGS, &Flags, sizeof Flags, 0))
            IsInteractive = 0 != (Flags.dwFlags & WSF_VISIBLE);
        ProcessWindowStation = CurrentWindowStation;
    }
    return IsInteractive;
}

VOID SpdServiceLog(ULONG Type, PWSTR Format, ...)
{
    va_list ap;

    va_start(ap, Format);
    SpdServiceLogV(Type, Format, ap);
    va_end(ap);
}

VOID SpdServiceLogV(ULONG Type, PWSTR Format, va_list ap)
{
    if (SpdIsInteractive())
        SpdPrintLogV(GetStdHandle(STD_ERROR_HANDLE), Format, ap);
    else
        SpdEventLogV(Type, Format, ap);
}

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
            ModuleBaseName = P + 1;
        else if (L'.' == *P)
            Dot = P;
    }

    lstrcpynW(SpdDiagIdentBuf, ModuleBaseName, sizeof SpdDiagIdentBuf / sizeof(WCHAR));
    SpdDiagIdentBuf[(sizeof SpdDiagIdentBuf / sizeof(WCHAR)) - 1] = L'\0';

    return TRUE;
}

PWSTR SpdDiagIdent(VOID)
{
    InitOnceExecuteOnce(&SpdDiagIdentInitOnce, SpdDiagIdentInitialize, 0, 0);
    return SpdDiagIdentBuf;
}
