/**
 * @file launcher/launcher.c
 *
 * @copyright 2018-2019 Bill Zissimopoulos
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

#include <windows.h>
#include <shared/minimal.h>
#include <shared/launch.h>
#include <aclapi.h>
#include <sddl.h>

#define PROGNAME                        "WinSpd.Launcher"

#define LAUNCHER_PIPE_DEFAULT_TIMEOUT   (1 * 15000 + 1000)
#define LAUNCHER_STOP_TIMEOUT           5500
#define LAUNCHER_KILL_TIMEOUT           5000

typedef struct
{
    LONG RefCount;
    PWSTR ClassName;
    PWSTR InstanceName;
    PWSTR CommandLine;
    PSECURITY_DESCRIPTOR SecurityDescriptor;
    DWORD ProcessId;
    HANDLE Process;
    HANDLE ProcessWait;
    LIST_ENTRY ListEntry;
    WCHAR Buffer[];
} SVC_INSTANCE;

typedef struct
{
    HANDLE Process;
    HANDLE ProcessWait;
} KILL_PROCESS_DATA;

static CRITICAL_SECTION SvcInstanceLock;
static HANDLE SvcInstanceEvent;
static LIST_ENTRY SvcInstanceList = { &SvcInstanceList, &SvcInstanceList };

/*
 * The OS will cleanup for us. So there is no need to explicitly release these resources.
 *
 * This also protects us from various races that we do not have to solve.
 */
static SERVICE_STATUS_HANDLE SvcHandle;
static HANDLE SvcJob, SvcThread, SvcEvent;
static DWORD SvcThreadId;
static HANDLE SvcPipe = INVALID_HANDLE_VALUE;
static OVERLAPPED SvcOverlapped;

static VOID CALLBACK KillProcessWait(PVOID Context, BOOLEAN Timeout);
static VOID CALLBACK SvcInstanceTerminated(PVOID Context, BOOLEAN Timeout);

static VOID SvcLog(ULONG Type, PWSTR Format, ...)
{
    static HANDLE SvcLogHandle;

    if (0 == SvcLogHandle)
        SvcLogHandle = RegisterEventSourceW(0, L"" PROGNAME);

    if (0 != Format)
    {
        WCHAR Buf[1024], *Strings[2];
            /* wvsprintfW is only safe with a 1024 WCHAR buffer */
        va_list ap;

        Strings[0] = L"" PROGNAME;

        va_start(ap, Format);
        wvsprintfW(Buf, Format, ap);
        va_end(ap);
        Buf[(sizeof Buf / sizeof Buf[0]) - 1] = L'\0';
        Strings[1] = Buf;

        ReportEventW(SvcLogHandle, (WORD)Type, 0, 1, 0, 2, 0, Strings, 0);
    }
}

static VOID KillProcess(ULONG ProcessId, HANDLE Process, ULONG Timeout)
{
    if (GenerateConsoleCtrlEvent(CTRL_BREAK_EVENT, ProcessId))
    {
        /*
         * If GenerateConsoleCtrlEvent succeeds, but the child process does not exit
         * timely we will terminate it with extreme prejudice. This is done by calling
         * RegisterWaitForSingleObject with timeout on a duplicated process handle.
         *
         * If GenerateConsoleCtrlEvent succeeds, but we are not able to successfully call
         * RegisterWaitForSingleObject, we do NOT terminate the child process forcibly.
         * This is by design as it is not the child process's fault and the child process
         * should (we hope in this case) respond to the console control event timely.
         */

        KILL_PROCESS_DATA *KillProcessData;

        KillProcessData = MemAlloc(sizeof *KillProcessData);
        if (0 != KillProcessData)
        {
            if (DuplicateHandle(GetCurrentProcess(), Process, GetCurrentProcess(), &KillProcessData->Process,
                0, FALSE, DUPLICATE_SAME_ACCESS))
            {
                if (RegisterWaitForSingleObject(&KillProcessData->ProcessWait, KillProcessData->Process,
                    KillProcessWait, KillProcessData, Timeout, WT_EXECUTEONLYONCE))
                    KillProcessData = 0;
                else
                    CloseHandle(KillProcessData->Process);
            }

            MemFree(KillProcessData);
        }
    }
    else
        TerminateProcess(Process, 0);
}

static VOID CALLBACK KillProcessWait(PVOID Context, BOOLEAN Timeout)
{
    KILL_PROCESS_DATA *KillProcessData = Context;

    if (Timeout)
        TerminateProcess(KillProcessData->Process, 0);

    UnregisterWaitEx(KillProcessData->ProcessWait, 0);
    CloseHandle(KillProcessData->Process);
    MemFree(KillProcessData);
}

static SVC_INSTANCE *SvcInstanceLookup(PWSTR ClassName, PWSTR InstanceName)
{
    SVC_INSTANCE *SvcInstance;
    PLIST_ENTRY ListEntry;

    for (ListEntry = SvcInstanceList.Flink;
        &SvcInstanceList != ListEntry;
        ListEntry = ListEntry->Flink)
    {
        SvcInstance = CONTAINING_RECORD(ListEntry, SVC_INSTANCE, ListEntry);

        if (0 == invariant_wcsicmp(ClassName, SvcInstance->ClassName) &&
            0 == invariant_wcsicmp(InstanceName, SvcInstance->InstanceName))
            return SvcInstance;
    }

    return 0;
}

static DWORD SvcInstanceAddUserRights(HANDLE Token,
    PSECURITY_DESCRIPTOR SecurityDescriptor, PSECURITY_DESCRIPTOR *PNewSecurityDescriptor)
{
    PSECURITY_DESCRIPTOR NewSecurityDescriptor;
    TOKEN_USER *User = 0;
    EXPLICIT_ACCESSW AccessEntry;
    DWORD Size, Error;

    *PNewSecurityDescriptor = 0;

    if (GetTokenInformation(Token, TokenUser, 0, 0, &Size))
    {
        Error = ERROR_INVALID_PARAMETER;
        goto exit;
    }
    if (ERROR_INSUFFICIENT_BUFFER != GetLastError())
    {
        Error = GetLastError();
        goto exit;
    }

    User = MemAlloc(Size);
    if (0 == User)
    {
        Error = ERROR_NO_SYSTEM_RESOURCES;
        goto exit;
    }

    if (!GetTokenInformation(Token, TokenUser, User, Size, &Size))
    {
        Error = GetLastError();
        goto exit;
    }

    AccessEntry.grfAccessPermissions = SERVICE_QUERY_STATUS | SERVICE_STOP;
    AccessEntry.grfAccessMode = GRANT_ACCESS;
    AccessEntry.grfInheritance = NO_INHERITANCE;
    AccessEntry.Trustee.pMultipleTrustee = 0;
    AccessEntry.Trustee.MultipleTrusteeOperation = NO_MULTIPLE_TRUSTEE;
    AccessEntry.Trustee.TrusteeForm = TRUSTEE_IS_SID;
    AccessEntry.Trustee.TrusteeType = TRUSTEE_IS_UNKNOWN;
    AccessEntry.Trustee.ptstrName = User->User.Sid;

    Error = BuildSecurityDescriptorW(0, 0, 1, &AccessEntry, 0, 0, SecurityDescriptor,
        &Size, &NewSecurityDescriptor);
    if (ERROR_SUCCESS != Error)
        goto exit;

    *PNewSecurityDescriptor = NewSecurityDescriptor;
    Error = ERROR_SUCCESS;

exit:
    MemFree(User);

    return Error;
}

static DWORD SvcInstanceAccessCheck(HANDLE ClientToken, ULONG DesiredAccess,
    PSECURITY_DESCRIPTOR SecurityDescriptor)
{
    static GENERIC_MAPPING GenericMapping =
    {
        .GenericRead = STANDARD_RIGHTS_READ | SERVICE_QUERY_CONFIG | SERVICE_QUERY_STATUS |
            SERVICE_INTERROGATE | SERVICE_ENUMERATE_DEPENDENTS,
        .GenericWrite = STANDARD_RIGHTS_WRITE | SERVICE_CHANGE_CONFIG,
        .GenericExecute = STANDARD_RIGHTS_EXECUTE | SERVICE_START | SERVICE_STOP |
            SERVICE_PAUSE_CONTINUE | SERVICE_USER_DEFINED_CONTROL,
        .GenericAll = SERVICE_ALL_ACCESS,
    };
    UINT8 PrivilegeSetBuf[sizeof(PRIVILEGE_SET) + 15 * sizeof(LUID_AND_ATTRIBUTES)];
    PPRIVILEGE_SET PrivilegeSet = (PVOID)PrivilegeSetBuf;
    DWORD PrivilegeSetLength = sizeof PrivilegeSetBuf;
    ULONG GrantedAccess;
    BOOL AccessStatus;
    DWORD Error;

    if (AccessCheck(SecurityDescriptor, ClientToken, DesiredAccess,
        &GenericMapping, PrivilegeSet, &PrivilegeSetLength, &GrantedAccess, &AccessStatus))
        Error = AccessStatus ? ERROR_SUCCESS : ERROR_ACCESS_DENIED;
    else
        Error = GetLastError();

    return Error;
}

static ULONG SvcInstanceArgumentLength(PWSTR Arg)
{
    ULONG Length;

    Length = 2; /* for beginning and ending quotes */
    for (PWSTR P = Arg; *P; P++)
        if (L'"' != *P)
            Length++;

    return Length;
}

static PWSTR SvcInstanceArgumentCopy(PWSTR Dest, PWSTR Arg)
{
    *Dest++ = L'"';
    for (PWSTR P = Arg; *P; P++)
        if (L'"' != *P)
            *Dest++ = *P;
    *Dest++ = L'"';

    return Dest;
}

static DWORD SvcInstanceReplaceArguments(PWSTR String, ULONG Argc, PWSTR *Argv,
    PWSTR *PNewString)
{
    PWSTR NewString = 0, P, Q;
    PWSTR EmptyArg = L"";
    ULONG Length;

    *PNewString = 0;

    Length = 0;
    for (P = String; *P; P++)
    {
        switch (*P)
        {
        case L'%':
            P++;
            if (L'0' <= *P && *P <= '9')
            {
                if (Argc > (ULONG)(*P - L'0'))
                    Length += SvcInstanceArgumentLength(Argv[*P - L'0']);
                else
                    Length += SvcInstanceArgumentLength(EmptyArg);
            }
            else
                Length++;
            break;
        default:
            Length++;
            break;
        }
    }

    NewString = MemAlloc((Length + 1) * sizeof(WCHAR));
    if (0 == NewString)
        return ERROR_NO_SYSTEM_RESOURCES;

    for (P = String, Q = NewString; *P; P++)
    {
        switch (*P)
        {
        case L'%':
            P++;
            if (L'0' <= *P && *P <= '9')
            {
                if (Argc > (ULONG)(*P - L'0'))
                    Q = SvcInstanceArgumentCopy(Q, Argv[*P - L'0']);
                else
                    Q = SvcInstanceArgumentCopy(Q, EmptyArg);
            }
            else
                *Q++ = *P;
            break;
        default:
            *Q++ = *P;
            break;
        }
    }
    *Q = L'\0';

    *PNewString = NewString;

    return ERROR_SUCCESS;
}

static DWORD SvcInstanceCreateProcess(
    PWSTR Executable, PWSTR CommandLine, PWSTR WorkDirectory,
    PPROCESS_INFORMATION ProcessInfo)
{
    WCHAR WorkDirectoryBuf[MAX_PATH];
    STARTUPINFOW StartupInfo;

    if (0 != WorkDirectory && L'.' == WorkDirectory[0] && L'\0' == WorkDirectory[1])
    {
        PWSTR Backslash = 0, P;

        if (0 == GetModuleFileNameW(0, WorkDirectoryBuf, MAX_PATH))
            return GetLastError();

        for (P = WorkDirectoryBuf; *P; P++)
            if (L'\\' == *P)
                Backslash = P;
        if (0 != Backslash && WorkDirectoryBuf < Backslash && L':' != Backslash[-1])
            *Backslash = L'\0';

        WorkDirectory = WorkDirectoryBuf;
    }

    memset(&StartupInfo, 0, sizeof StartupInfo);
    StartupInfo.cb = sizeof StartupInfo;

    if (!CreateProcessW(
        Executable, CommandLine, 0, 0, FALSE,
        CREATE_SUSPENDED | CREATE_NEW_PROCESS_GROUP,
        0, WorkDirectory,
        &StartupInfo, ProcessInfo))
        return GetLastError();

    return ERROR_SUCCESS;
}

static DWORD SvcInstanceCreate(HANDLE ClientToken,
    PWSTR ClassName, PWSTR InstanceName, ULONG Argc, PWSTR *Argv0, HANDLE Job,
    SVC_INSTANCE **PSvcInstance)
{
    SVC_INSTANCE *SvcInstance = 0;
    HKEY RegKey = 0;
    DWORD RegSize;
    DWORD ClassNameSize, InstanceNameSize;
    WCHAR Executable[MAX_PATH], CommandLineBuf[512], WorkDirectory[MAX_PATH],
        SecurityBuf[512];
    PWSTR CommandLine, Security;
    DWORD JobControl;
    PSECURITY_DESCRIPTOR SecurityDescriptor = 0, NewSecurityDescriptor;
    PWSTR Argv[10];
    PROCESS_INFORMATION ProcessInfo;
    DWORD Error;

    *PSvcInstance = 0;

    lstrcpyW(CommandLineBuf, L"%0 ");
    lstrcpyW(SecurityBuf, L"O:SYG:SY");

    if (Argc > sizeof Argv / sizeof Argv[0] - 1)
        Argc = sizeof Argv / sizeof Argv[0] - 1;
    memcpy(Argv + 1, Argv0, Argc * sizeof(PWSTR));
    Argv[0] = 0;
    Argc++;

    memset(&ProcessInfo, 0, sizeof ProcessInfo);

    EnterCriticalSection(&SvcInstanceLock);

    if (0 != SvcInstanceLookup(ClassName, InstanceName))
    {
        Error = ERROR_ALREADY_EXISTS;
        goto exit;
    }

    Error = RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"" SPD_LAUNCH_REGKEY,
        0, SPD_LAUNCH_REGKEY_WOW64 | KEY_READ, &RegKey);
    if (ERROR_SUCCESS != Error)
        goto exit;

    RegSize = sizeof Executable;
    Executable[0] = L'\0';
    Error = RegGetValueW(RegKey, ClassName, L"Executable", RRF_RT_REG_SZ, 0,
        Executable, &RegSize);
    if (ERROR_SUCCESS != Error)
        goto exit;
    Argv[0] = Executable;

    CommandLine = CommandLineBuf + lstrlenW(CommandLineBuf);
    RegSize = (DWORD)(sizeof CommandLineBuf - (CommandLine - CommandLineBuf) * sizeof(WCHAR));
    Error = RegGetValueW(RegKey, ClassName, L"CommandLine", RRF_RT_REG_SZ, 0,
        CommandLine, &RegSize);
    if (ERROR_SUCCESS != Error && ERROR_FILE_NOT_FOUND != Error)
        goto exit;
    if (ERROR_FILE_NOT_FOUND == Error)
        CommandLine[-1] = L'\0';
    CommandLine = CommandLineBuf;

    RegSize = sizeof WorkDirectory;
    WorkDirectory[0] = L'\0';
    Error = RegGetValueW(RegKey, ClassName, L"WorkDirectory", RRF_RT_REG_SZ, 0,
        WorkDirectory, &RegSize);
    if (ERROR_SUCCESS != Error && ERROR_FILE_NOT_FOUND != Error)
        goto exit;

    Security = SecurityBuf + lstrlenW(SecurityBuf);
    RegSize = (DWORD)(sizeof SecurityBuf - (Security - SecurityBuf) * sizeof(WCHAR));
    Error = RegGetValueW(RegKey, ClassName, L"Security", RRF_RT_REG_SZ, 0,
        Security, &RegSize);
    if (ERROR_SUCCESS != Error && ERROR_FILE_NOT_FOUND != Error)
        goto exit;

    RegSize = sizeof JobControl;
    JobControl = 1; /* default is YES! */
    Error = RegGetValueW(RegKey, ClassName, L"JobControl", RRF_RT_REG_DWORD, 0,
        &JobControl, &RegSize);
    if (ERROR_SUCCESS != Error && ERROR_FILE_NOT_FOUND != Error)
        goto exit;

    RegCloseKey(RegKey);
    RegKey = 0;

    if (L'\0' == Security[0])
        lstrcpyW(Security, L"" SPD_LAUNCH_SERVICE_DEFAULT_SDDL);
    if (L'D' == Security[0] && L':' == Security[1])
        Security = SecurityBuf;

    if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(Security, SDDL_REVISION_1,
        &SecurityDescriptor, 0))
    {
        Error = GetLastError();
        goto exit;
    }

    Error = SvcInstanceAccessCheck(ClientToken, SERVICE_START, SecurityDescriptor);
    if (ERROR_SUCCESS != Error)
        goto exit;

    Error = SvcInstanceAddUserRights(ClientToken, SecurityDescriptor, &NewSecurityDescriptor);
    if (ERROR_SUCCESS != Error)
        goto exit;
    LocalFree(SecurityDescriptor);
    SecurityDescriptor = NewSecurityDescriptor;

    ClassNameSize = (lstrlenW(ClassName) + 1) * sizeof(WCHAR);
    InstanceNameSize = (lstrlenW(InstanceName) + 1) * sizeof(WCHAR);

    SvcInstance = MemAlloc(sizeof *SvcInstance + ClassNameSize + InstanceNameSize);
    if (0 == SvcInstance)
    {
        Error = ERROR_NO_SYSTEM_RESOURCES;
        goto exit;
    }

    memset(SvcInstance, 0, sizeof *SvcInstance);
    SvcInstance->RefCount = 2;
    memcpy(SvcInstance->Buffer, ClassName, ClassNameSize);
    memcpy(SvcInstance->Buffer + ClassNameSize / sizeof(WCHAR), InstanceName, InstanceNameSize);
    SvcInstance->ClassName = SvcInstance->Buffer;
    SvcInstance->InstanceName = SvcInstance->Buffer + ClassNameSize / sizeof(WCHAR);
    SvcInstance->SecurityDescriptor = SecurityDescriptor;

    Error = SvcInstanceReplaceArguments(CommandLine, Argc, Argv, &SvcInstance->CommandLine);
    if (ERROR_SUCCESS != Error)
        goto exit;

    Error = SvcInstanceCreateProcess(
        Executable, SvcInstance->CommandLine, L'\0' != WorkDirectory[0] ? WorkDirectory : 0,
        &ProcessInfo);
    if (ERROR_SUCCESS != Error)
        goto exit;

    SvcInstance->ProcessId = ProcessInfo.dwProcessId;
    SvcInstance->Process = ProcessInfo.hProcess;

    if (!RegisterWaitForSingleObject(&SvcInstance->ProcessWait, SvcInstance->Process,
        SvcInstanceTerminated, SvcInstance, INFINITE, WT_EXECUTEONLYONCE))
    {
        Error = GetLastError();
        goto exit;
    }

    if (0 != Job && JobControl)
    {
        if (!AssignProcessToJobObject(Job, SvcInstance->Process))
            SvcLog(EVENTLOG_WARNING_TYPE,
                L"Ignorning error: AssignProcessToJobObject = %ld", GetLastError());
    }

    /*
     * ONCE THE PROCESS IS RESUMED NO MORE FAILURES ALLOWED!
     */

    ResumeThread(ProcessInfo.hThread);
    CloseHandle(ProcessInfo.hThread);
    ProcessInfo.hThread = 0;

    InsertTailList(&SvcInstanceList, &SvcInstance->ListEntry);
    ResetEvent(SvcInstanceEvent);

    *PSvcInstance = SvcInstance;

    Error = ERROR_SUCCESS;

exit:
    if (ERROR_SUCCESS != Error)
    {
        LocalFree(SecurityDescriptor);

        if (0 != ProcessInfo.hThread)
            CloseHandle(ProcessInfo.hThread);

        if (0 != SvcInstance)
        {
            if (0 != SvcInstance->ProcessWait)
                UnregisterWaitEx(SvcInstance->ProcessWait, 0);

            if (0 != SvcInstance->Process)
            {
                TerminateProcess(SvcInstance->Process, 0);
                CloseHandle(SvcInstance->Process);
            }

            MemFree(SvcInstance->CommandLine);
            MemFree(SvcInstance);
        }
    }

    if (0 != RegKey)
        RegCloseKey(RegKey);

    LeaveCriticalSection(&SvcInstanceLock);

    SvcLog(EVENTLOG_INFORMATION_TYPE,
        L"create %s\\%s = %ld", ClassName, InstanceName, Error);

    return Error;
}

static VOID SvcInstanceRelease(SVC_INSTANCE *SvcInstance)
{
    if (0 != InterlockedDecrement(&SvcInstance->RefCount))
        return;

    EnterCriticalSection(&SvcInstanceLock);
    if (RemoveEntryList(&SvcInstance->ListEntry))
        SetEvent(SvcInstanceEvent);
    LeaveCriticalSection(&SvcInstanceLock);

    if (0 != SvcInstance->ProcessWait)
        UnregisterWaitEx(SvcInstance->ProcessWait, 0);
    if (0 != SvcInstance->Process)
        CloseHandle(SvcInstance->Process);

    LocalFree(SvcInstance->SecurityDescriptor);

    MemFree(SvcInstance->CommandLine);
    MemFree(SvcInstance);
}

static VOID CALLBACK SvcInstanceTerminated(PVOID Context, BOOLEAN Timeout)
{
    SVC_INSTANCE *SvcInstance = Context;

    SvcLog(EVENTLOG_INFORMATION_TYPE,
        L"terminated %s\\%s", SvcInstance->ClassName, SvcInstance->InstanceName);

    SvcInstanceRelease(SvcInstance);
}

static DWORD SvcInstanceStart(HANDLE ClientToken,
    PWSTR ClassName, PWSTR InstanceName, ULONG Argc, PWSTR *Argv, HANDLE Job)
{
    SVC_INSTANCE *SvcInstance;

    return SvcInstanceCreate(ClientToken, ClassName, InstanceName, Argc, Argv, Job,
        &SvcInstance);
}

static DWORD SvcInstanceStop(HANDLE ClientToken,
    PWSTR ClassName, PWSTR InstanceName)
{
    SVC_INSTANCE *SvcInstance;
    DWORD Error;

    EnterCriticalSection(&SvcInstanceLock);

    SvcInstance = SvcInstanceLookup(ClassName, InstanceName);
    if (0 == SvcInstance)
    {
        Error = ERROR_FILE_NOT_FOUND;
        goto exit;
    }

    Error = SvcInstanceAccessCheck(ClientToken, SERVICE_STOP, SvcInstance->SecurityDescriptor);
    if (ERROR_SUCCESS != Error)
        goto exit;

    KillProcess(SvcInstance->ProcessId, SvcInstance->Process, LAUNCHER_KILL_TIMEOUT);

    Error = ERROR_SUCCESS;

exit:
    LeaveCriticalSection(&SvcInstanceLock);

    return Error;
}

static DWORD SvcInstanceGetInfo(HANDLE ClientToken,
    PWSTR ClassName, PWSTR InstanceName, PWSTR Buffer, PULONG PSize)
{
    SVC_INSTANCE *SvcInstance;
    PWSTR P = Buffer;
    ULONG ClassNameSize, InstanceNameSize, CommandLineSize;
    DWORD Error;

    EnterCriticalSection(&SvcInstanceLock);

    SvcInstance = SvcInstanceLookup(ClassName, InstanceName);
    if (0 == SvcInstance)
    {
        Error = ERROR_FILE_NOT_FOUND;
        goto exit;
    }

    Error = SvcInstanceAccessCheck(ClientToken, SERVICE_QUERY_STATUS, SvcInstance->SecurityDescriptor);
    if (ERROR_SUCCESS != Error)
        goto exit;

    ClassNameSize = lstrlenW(SvcInstance->ClassName) + 1;
    InstanceNameSize = lstrlenW(SvcInstance->InstanceName) + 1;
    CommandLineSize = lstrlenW(SvcInstance->CommandLine) + 1;

    if (*PSize < (ClassNameSize + InstanceNameSize + CommandLineSize) * sizeof(WCHAR))
    {
        Error = ERROR_INSUFFICIENT_BUFFER;
        goto exit;
    }

    memcpy(P, SvcInstance->ClassName, ClassNameSize * sizeof(WCHAR)); P += ClassNameSize;
    memcpy(P, SvcInstance->InstanceName, InstanceNameSize * sizeof(WCHAR)); P += InstanceNameSize;
    memcpy(P, SvcInstance->CommandLine, CommandLineSize * sizeof(WCHAR)); P += CommandLineSize;

    *PSize = (ULONG)(P - Buffer) * sizeof(WCHAR);

    Error = ERROR_SUCCESS;

exit:
    LeaveCriticalSection(&SvcInstanceLock);

    return Error;
}

static DWORD SvcInstanceGetNameList(HANDLE ClientToken,
    PWSTR Buffer, PULONG PSize)
{
    SVC_INSTANCE *SvcInstance;
    PLIST_ENTRY ListEntry;
    PWSTR P = Buffer, BufferEnd = P + *PSize / sizeof(WCHAR);
    ULONG ClassNameSize, InstanceNameSize;

    EnterCriticalSection(&SvcInstanceLock);

    for (ListEntry = SvcInstanceList.Flink;
        &SvcInstanceList != ListEntry;
        ListEntry = ListEntry->Flink)
    {
        SvcInstance = CONTAINING_RECORD(ListEntry, SVC_INSTANCE, ListEntry);

        ClassNameSize = lstrlenW(SvcInstance->ClassName) + 1;
        InstanceNameSize = lstrlenW(SvcInstance->InstanceName) + 1;

        if (BufferEnd < P + ClassNameSize + InstanceNameSize)
            break;

        memcpy(P, SvcInstance->ClassName, ClassNameSize * sizeof(WCHAR)); P += ClassNameSize;
        memcpy(P, SvcInstance->InstanceName, InstanceNameSize * sizeof(WCHAR)); P += InstanceNameSize;
    }

    LeaveCriticalSection(&SvcInstanceLock);

    *PSize = (ULONG)(P - Buffer) * sizeof(WCHAR);

    return ERROR_SUCCESS;
}

static VOID SvcInstanceStopAndWaitAll(VOID)
{
    SVC_INSTANCE *SvcInstance;
    PLIST_ENTRY ListEntry;

    EnterCriticalSection(&SvcInstanceLock);

    for (ListEntry = SvcInstanceList.Flink;
        &SvcInstanceList != ListEntry;
        ListEntry = ListEntry->Flink)
    {
        SvcInstance = CONTAINING_RECORD(ListEntry, SVC_INSTANCE, ListEntry);

        KillProcess(SvcInstance->ProcessId, SvcInstance->Process, LAUNCHER_KILL_TIMEOUT);
    }

    LeaveCriticalSection(&SvcInstanceLock);

    WaitForSingleObject(SvcInstanceEvent, LAUNCHER_STOP_TIMEOUT);
}

static inline PWSTR SvcPipeTransactGetPart(PWSTR *PP, PWSTR PipeBufEnd)
{
    PWSTR PipeBufBeg = *PP, P;

    for (P = PipeBufBeg; PipeBufEnd > P && *P; P++)
        ;

    if (PipeBufEnd > P)
    {
        *PP = P + 1;
        return PipeBufBeg;
    }
    else
    {
        *PP = P;
        return 0;
    }
}

static inline VOID SvcPipeTransactResult(DWORD Error, PWSTR PipeBuf, PULONG PSize)
{
    if (ERROR_SUCCESS == Error)
    {
        *PipeBuf = SpdLaunchCmdSuccess;
        *PSize += sizeof(WCHAR);
    }
    else
        *PSize = (wsprintfW(PipeBuf, L"%c%ld", SpdLaunchCmdFailure, Error) + 1) * sizeof(WCHAR);
}

static VOID SvcPipeTransact(HANDLE ClientToken, PWSTR PipeBuf, PULONG PSize)
{
    if (sizeof(WCHAR) > *PSize)
        return;

    PWSTR P = PipeBuf, PipeBufEnd = PipeBuf + *PSize / sizeof(WCHAR);
    PWSTR ClassName, InstanceName;
    ULONG Argc; PWSTR Argv[9];
    DWORD Error;

    *PSize = 0;

    switch (*P++)
    {
    case SpdLaunchCmdStart:
        ClassName = SvcPipeTransactGetPart(&P, PipeBufEnd);
        InstanceName = SvcPipeTransactGetPart(&P, PipeBufEnd);
        for (Argc = 0; sizeof Argv / sizeof Argv[0] > Argc; Argc++)
            if (0 == (Argv[Argc] = SvcPipeTransactGetPart(&P, PipeBufEnd)))
                break;

        Error = ERROR_INVALID_PARAMETER;
        if (0 != ClassName && 0 != InstanceName)
            Error = SvcInstanceStart(ClientToken, ClassName, InstanceName, Argc, Argv, SvcJob);

        SvcPipeTransactResult(Error, PipeBuf, PSize);
        break;

    case SpdLaunchCmdStop:
        ClassName = SvcPipeTransactGetPart(&P, PipeBufEnd);
        InstanceName = SvcPipeTransactGetPart(&P, PipeBufEnd);

        Error = ERROR_INVALID_PARAMETER;
        if (0 != ClassName && 0 != InstanceName)
            Error = SvcInstanceStop(ClientToken, ClassName, InstanceName);

        SvcPipeTransactResult(Error, PipeBuf, PSize);
        break;

    case SpdLaunchCmdGetInfo:
        ClassName = SvcPipeTransactGetPart(&P, PipeBufEnd);
        InstanceName = SvcPipeTransactGetPart(&P, PipeBufEnd);

        Error = ERROR_INVALID_PARAMETER;
        if (0 != ClassName && 0 != InstanceName)
        {
            *PSize = SPD_LAUNCH_PIPE_BUFFER_SIZE - 1;
            Error = SvcInstanceGetInfo(ClientToken, ClassName, InstanceName, PipeBuf + 1, PSize);
        }

        SvcPipeTransactResult(Error, PipeBuf, PSize);
        break;

    case SpdLaunchCmdGetNameList:
        *PSize = SPD_LAUNCH_PIPE_BUFFER_SIZE - 1;
        Error = SvcInstanceGetNameList(ClientToken, PipeBuf + 1, PSize);

        SvcPipeTransactResult(Error, PipeBuf, PSize);
        break;

#if !defined(NDEBUG)
    case SpdLaunchCmdQuit:
        SetEvent(SvcEvent);

        SvcPipeTransactResult(ERROR_SUCCESS, PipeBuf, PSize);
        break;
#endif
    default:
        SvcPipeTransactResult(ERROR_INVALID_PARAMETER, PipeBuf, PSize);
        break;
    }
}

static inline DWORD SvcPipeWaitResult(HANDLE StopEvent, BOOL Success,
    HANDLE Handle, OVERLAPPED *Overlapped, PDWORD PBytesTransferred)
{
    HANDLE WaitObjects[2];
    DWORD WaitResult;

    if (!Success && ERROR_IO_PENDING != GetLastError())
        return GetLastError();

    WaitObjects[0] = StopEvent;
    WaitObjects[1] = Overlapped->hEvent;
    WaitResult = WaitForMultipleObjects(2, WaitObjects, FALSE, INFINITE);
    if (WAIT_OBJECT_0 == WaitResult)
    {
        CancelIoEx(Handle, Overlapped);
        GetOverlappedResult(Handle, Overlapped, PBytesTransferred, TRUE);
        return -1; /* special: stop thread */
    }
    else if (WAIT_OBJECT_0 + 1 == WaitResult)
    {
        if (!GetOverlappedResult(Handle, Overlapped, PBytesTransferred, TRUE))
            return GetLastError();
        return ERROR_SUCCESS;
    }
    else
        return GetLastError();
}

static DWORD WINAPI SvcPipeServer(PVOID Context)
{
    static PWSTR LoopErrorMessage =
        L"Error in service main loop (%s = %ld). Exiting...";
    static PWSTR LoopWarningMessage =
        L"Error in service main loop (%s = %ld). Continuing...";
    SERVICE_STATUS SvcStatus;
    PWSTR PipeBuf = 0;
    HANDLE ClientToken;
    DWORD LastError, BytesTransferred, ExitCode = 0;

    PipeBuf = MemAlloc(SPD_LAUNCH_PIPE_BUFFER_SIZE);
    if (0 == PipeBuf)
    {
        ExitCode = ERROR_NO_SYSTEM_RESOURCES;
        goto exit;
    }

    for (;;)
    {
        LastError = SvcPipeWaitResult(SvcEvent,
            ConnectNamedPipe(SvcPipe, &SvcOverlapped),
            SvcPipe, &SvcOverlapped, &BytesTransferred);
        if (-1 == LastError)
            break;
        else if (0 != LastError &&
            ERROR_PIPE_CONNECTED != LastError && ERROR_NO_DATA != LastError)
        {
            SvcLog(EVENTLOG_WARNING_TYPE, LoopWarningMessage,
                L"ConnectNamedPipe", LastError);
            continue;
        }

        LastError = SvcPipeWaitResult(SvcEvent,
            ReadFile(SvcPipe, PipeBuf, SPD_LAUNCH_PIPE_BUFFER_SIZE, &BytesTransferred, &SvcOverlapped),
            SvcPipe, &SvcOverlapped, &BytesTransferred);
        if (-1 == LastError)
            break;
        else if (0 != LastError || sizeof(WCHAR) > BytesTransferred)
        {
            DisconnectNamedPipe(SvcPipe);
            if (0 != LastError)
                SvcLog(EVENTLOG_WARNING_TYPE, LoopWarningMessage,
                    L"ReadFile", LastError);
            continue;
        }

        ClientToken = 0;
        if (!ImpersonateNamedPipeClient(SvcPipe) ||
            !OpenThreadToken(GetCurrentThread(), TOKEN_QUERY, FALSE, &ClientToken) ||
            !RevertToSelf())
        {
            LastError = GetLastError();
            if (0 == ClientToken)
            {
                CloseHandle(ClientToken);
                DisconnectNamedPipe(SvcPipe);
                SvcLog(EVENTLOG_WARNING_TYPE, LoopWarningMessage,
                    L"ImpersonateNamedPipeClient||OpenThreadToken", LastError);
                continue;
            }
            else
            {
                CloseHandle(ClientToken);
                DisconnectNamedPipe(SvcPipe);
                SvcLog(EVENTLOG_ERROR_TYPE, LoopErrorMessage,
                    L"RevertToSelf", LastError);
                break;
            }
        }

        SvcPipeTransact(ClientToken, PipeBuf, &BytesTransferred);

        CloseHandle(ClientToken);

        LastError = SvcPipeWaitResult(SvcEvent,
            WriteFile(SvcPipe, PipeBuf, BytesTransferred, &BytesTransferred, &SvcOverlapped),
            SvcPipe, &SvcOverlapped, &BytesTransferred);
        if (-1 == LastError)
            break;
        else if (0 != LastError)
        {
            DisconnectNamedPipe(SvcPipe);
            SvcLog(EVENTLOG_WARNING_TYPE, LoopWarningMessage,
                L"WriteFile", LastError);
            continue;
        }

        DisconnectNamedPipe(SvcPipe);
    }

exit:
    MemFree(PipeBuf);

    SvcInstanceStopAndWaitAll();

    SvcStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    SvcStatus.dwCurrentState = SERVICE_STOPPED;
    SvcStatus.dwControlsAccepted = 0;
    SvcStatus.dwWin32ExitCode = ExitCode;
    SvcStatus.dwServiceSpecificExitCode = 0;
    SvcStatus.dwCheckPoint = 0;
    SvcStatus.dwWaitHint = 0;
    SetServiceStatus(SvcHandle, &SvcStatus);

    return 0;
}

static DWORD SvcOnStart(ULONG Argc, PWSTR *Argv)
{
    SECURITY_ATTRIBUTES SecurityAttributes = { 0 };

    /*
     * Allocate a console in case we are running as a service without one.
     * This will ensure that we can send console control events to service instances.
     */
    if (AllocConsole())
        ShowWindow(GetConsoleWindow(), SW_HIDE);

    InitializeCriticalSection(&SvcInstanceLock);

    SecurityAttributes.nLength = sizeof SecurityAttributes;
    SecurityAttributes.bInheritHandle = FALSE;
    if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(L"" SPD_LAUNCH_PIPE_SDDL, SDDL_REVISION_1,
        &SecurityAttributes.lpSecurityDescriptor, 0))
        goto fail;

    SvcInstanceEvent = CreateEventW(0, TRUE, TRUE, 0);
    if (0 == SvcInstanceEvent)
        goto fail;

    SvcJob = CreateJobObjectW(0, 0);
    if (0 != SvcJob)
    {
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION LimitInfo;

        memset(&LimitInfo, 0, sizeof LimitInfo);
        LimitInfo.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        if (!SetInformationJobObject(SvcJob, JobObjectExtendedLimitInformation,
            &LimitInfo, sizeof LimitInfo))
        {
            CloseHandle(SvcJob);
            SvcJob = 0;
        }
    }

    SvcEvent = CreateEventW(0, TRUE, FALSE, 0);
    if (0 == SvcEvent)
        goto fail;

    SvcOverlapped.hEvent = CreateEventW(0, TRUE, FALSE, 0);
    if (0 == SvcOverlapped.hEvent)
        goto fail;

    SvcPipe = CreateNamedPipeW(L"" SPD_LAUNCH_PIPE_NAME,
        PIPE_ACCESS_DUPLEX |
            FILE_FLAG_FIRST_PIPE_INSTANCE | FILE_FLAG_WRITE_THROUGH | FILE_FLAG_OVERLAPPED,
        PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT | PIPE_REJECT_REMOTE_CLIENTS,
        1, SPD_LAUNCH_PIPE_BUFFER_SIZE, SPD_LAUNCH_PIPE_BUFFER_SIZE, LAUNCHER_PIPE_DEFAULT_TIMEOUT,
        &SecurityAttributes);
    if (INVALID_HANDLE_VALUE == SvcPipe)
        goto fail;

    SvcThread = CreateThread(0, 0, SvcPipeServer, 0, 0, &SvcThreadId);
    if (0 == SvcThread)
        goto fail;

    LocalFree(SecurityAttributes.lpSecurityDescriptor);

    return ERROR_SUCCESS;

fail:
    DWORD LastError = GetLastError();

    LocalFree(SecurityAttributes.lpSecurityDescriptor);

    return LastError;
}

static DWORD WINAPI SvcCtrlHandler(
    DWORD Control, DWORD EventType, PVOID EventData, PVOID Context)
{
    SERVICE_STATUS SvcStatus;

    switch (Control)
    {
    case SERVICE_CONTROL_SHUTDOWN:
    case SERVICE_CONTROL_STOP:
        SvcStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
        SvcStatus.dwCurrentState = SERVICE_STOP_PENDING;
        SvcStatus.dwControlsAccepted = 0;
        SvcStatus.dwWin32ExitCode = ERROR_SUCCESS;
        SvcStatus.dwServiceSpecificExitCode = 0;
        SvcStatus.dwCheckPoint = 1;
        SvcStatus.dwWaitHint = LAUNCHER_STOP_TIMEOUT;
        SetServiceStatus(SvcHandle, &SvcStatus);

        SetEvent(SvcEvent);
        WaitForSingleObject(SvcThread, LAUNCHER_STOP_TIMEOUT);

        return ERROR_SUCCESS;

    case SERVICE_CONTROL_PAUSE:
    case SERVICE_CONTROL_CONTINUE:
        return ERROR_CALL_NOT_IMPLEMENTED;

    default:
        return ERROR_SUCCESS;
    }
}

static VOID WINAPI SvcEntry(DWORD Argc, PWSTR *Argv)
{
    SERVICE_STATUS SvcStatus;
    DWORD Error;

    SvcHandle = RegisterServiceCtrlHandlerExW(L"" PROGNAME, SvcCtrlHandler, 0);
    if (0 == SvcHandle)
    {
        SvcLog(EVENTLOG_ERROR_TYPE,
            L"" __FUNCTION__ ": RegisterServiceCtrlHandlerW = %ld", GetLastError());
        return;
    }

    SvcStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    SvcStatus.dwCurrentState = SERVICE_START_PENDING;
    SvcStatus.dwControlsAccepted = 0;
    SvcStatus.dwWin32ExitCode = ERROR_SUCCESS;
    SvcStatus.dwServiceSpecificExitCode = 0;
    SvcStatus.dwCheckPoint = 0;
    SvcStatus.dwWaitHint = 0;
    SetServiceStatus(SvcHandle, &SvcStatus);

    Error = SvcOnStart(Argc, Argv);
    if (ERROR_SUCCESS == Error)
    {
        SvcStatus.dwCurrentState = SERVICE_RUNNING;
        SvcStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP;
        SetServiceStatus(SvcHandle, &SvcStatus);

        SvcLog(EVENTLOG_INFORMATION_TYPE,
            L"The service %s has been started.", L"" PROGNAME);
    }
    else
    {
        SvcStatus.dwCurrentState = SERVICE_STOPPED;
        SvcStatus.dwControlsAccepted = 0;
        SetServiceStatus(SvcHandle, &SvcStatus);

        SvcLog(EVENTLOG_ERROR_TYPE,
            L"The service %s has failed to start (Error=%ld).", L"" PROGNAME, Error);
    }
}

int wmain(int argc, wchar_t **argv)
{
    /* init SvcLog */
    SvcLog(0, 0);

    static SERVICE_TABLE_ENTRYW SvcTable[2] =
    {
        { L"" PROGNAME, SvcEntry},
        { 0 },
    };
    if (!StartServiceCtrlDispatcherW(SvcTable))
        return GetLastError();

    return ERROR_SUCCESS;
}

void wmainCRTStartup(void)
{
    ExitProcess(wmain(0, 0));
}
