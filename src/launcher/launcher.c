/**
 * @file launcher/launcher.c
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
#include <aclapi.h>
#include <sddl.h>

#define PROGNAME                        "WinSpd.Launcher"

#define LAUNCHER_PIPE_DEFAULT_TIMEOUT   15000
#define LAUNCHER_START_TIMEOUT          30000
#define LAUNCHER_STOP_TIMEOUT           30000
#define LAUNCHER_KILL_TIMEOUT           5000

typedef struct
{
    LONG RefCount;
    PWSTR ClassName;
    PWSTR InstanceName;
    PWSTR CommandLine;
    PSECURITY_DESCRIPTOR SecurityDescriptor;
    BOOLEAN Persistent;
    DWORD ProcessId;
    HANDLE Process;
    HANDLE ProcessWait;
    HANDLE VolumeHandles;
    ULONG VolumeHandleCount;
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

static DWORD GetVolumeOwnerProcessId(PWSTR Volume, PDWORD PProcessId)
{
    HANDLE Handle = INVALID_HANDLE_VALUE;
    STORAGE_PROPERTY_QUERY Query;
    DWORD BytesTransferred;
    union
    {
        STORAGE_DEVICE_DESCRIPTOR Device;
        STORAGE_DEVICE_ID_DESCRIPTOR DeviceId;
        UINT8 B[1024];
    } DescBuf;
    PSTORAGE_IDENTIFIER Identifier;
    DWORD ProcessId;
    DWORD Error;

    *PProcessId = 0;

    Handle = CreateFileW(Volume, 0, 0, 0, OPEN_EXISTING, 0, 0);
    if (INVALID_HANDLE_VALUE == Handle)
    {
        Error = GetLastError();
        goto exit;
    }

    Query.PropertyId = StorageDeviceProperty;
    Query.QueryType = PropertyStandardQuery;
    Query.AdditionalParameters[0] = 0;
    memset(&DescBuf, 0, sizeof DescBuf);

    if (!DeviceIoControl(Handle, IOCTL_STORAGE_QUERY_PROPERTY,
        &Query, sizeof Query, &DescBuf, sizeof DescBuf,
        &BytesTransferred, 0))
    {
        Error = GetLastError();
        goto exit;
    }

    Error = ERROR_NO_ASSOCIATION;
    if (sizeof DescBuf >= DescBuf.Device.Size && 0 != DescBuf.Device.VendorIdOffset)
        if (0 == invariant_strcmp((const char *)((PUINT8)&DescBuf + DescBuf.Device.VendorIdOffset),
            SPD_IOCTL_VENDOR_ID))
            Error = ERROR_SUCCESS;
    if (ERROR_SUCCESS != Error)
        goto exit;

    Query.PropertyId = StorageDeviceIdProperty;
    Query.QueryType = PropertyStandardQuery;
    Query.AdditionalParameters[0] = 0;
    memset(&DescBuf, 0, sizeof DescBuf);

    if (!DeviceIoControl(Handle, IOCTL_STORAGE_QUERY_PROPERTY,
        &Query, sizeof Query, &DescBuf, sizeof DescBuf,
        &BytesTransferred, 0))
    {
        Error = GetLastError();
        goto exit;
    }

    Error = ERROR_NO_ASSOCIATION;
    if (sizeof DescBuf >= DescBuf.DeviceId.Size)
    {
        Identifier = (PSTORAGE_IDENTIFIER)DescBuf.DeviceId.Identifiers;
        for (ULONG I = 0; DescBuf.DeviceId.NumberOfIdentifiers > I; I++)
        {
            if (StorageIdCodeSetBinary == Identifier->CodeSet &&
                StorageIdTypeVendorSpecific == Identifier->Type &&
                StorageIdAssocDevice == Identifier->Association &&
                8 == Identifier->IdentifierSize &&
                'P' == Identifier->Identifier[0] &&
                'I' == Identifier->Identifier[1] &&
                'D' == Identifier->Identifier[2] &&
                /* allow volumes that have EjectDisabled in the launcher */
                (' ' == Identifier->Identifier[3] || 'X' == Identifier->Identifier[3]))
            {
                ProcessId =
                    (Identifier->Identifier[4] << 24) |
                    (Identifier->Identifier[5] << 16) |
                    (Identifier->Identifier[6] << 8) |
                    (Identifier->Identifier[7]);
                Error = ERROR_SUCCESS;
                break;
            }
            Identifier = (PSTORAGE_IDENTIFIER)((PUINT8)Identifier + Identifier->NextOffset);
        }
    }
    if (ERROR_SUCCESS != Error)
        goto exit;

    *PProcessId = ProcessId;

exit:
    if (INVALID_HANDLE_VALUE != Handle)
        CloseHandle(Handle);

    return Error;
}

static DWORD OpenAndDismountVolume(PWSTR Volume, PHANDLE PHandle)
{
    HANDLE Handle = INVALID_HANDLE_VALUE;
    DWORD BytesTransferred;
    DWORD Error;

    *PHandle = INVALID_HANDLE_VALUE;

    Handle = CreateFileW(
        Volume,
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        0,
        OPEN_EXISTING,
        0,
        0);
    if (INVALID_HANDLE_VALUE == Handle)
    {
        Error = GetLastError();
        goto exit;
    }

    /* lock and dismount; unlock happens at CloseHandle */
    if (!DeviceIoControl(Handle, FSCTL_LOCK_VOLUME, 0, 0, 0, 0, &BytesTransferred, 0) ||
        !DeviceIoControl(Handle, FSCTL_DISMOUNT_VOLUME, 0, 0, 0, 0, &BytesTransferred, 0))
    {
        Error = GetLastError();
        if (ERROR_ACCESS_DENIED == Error)
            Error = ERROR_DEVICE_IN_USE;

        CloseHandle(Handle);
        goto exit;
    }

    *PHandle = Handle;

    Error = ERROR_SUCCESS;

exit:
    return Error;
}

static VOID CloseHandles(PHANDLE Handles, ULONG Count)
{
    for (ULONG Index = 0; Count > Index; Index++)
        CloseHandle(Handles[Index]);
}

static DWORD OpenAndDismountVolumesForProcessId(DWORD ProcessId, PHANDLE Handles, PULONG PCount)
{
    HANDLE FindHandle = INVALID_HANDLE_VALUE;
    WCHAR Volume[MAX_PATH];
    DWORD OwnerProcessId;
    ULONG Index = 0, Count = *PCount;
    DWORD Error, DevicesInUse = 0;

    *PCount = 0;

    FindHandle = FindFirstVolumeW(Volume, MAX_PATH);
    if (INVALID_HANDLE_VALUE == FindHandle)
    {
        Error = GetLastError();
        goto exit;
    }

    do
    {
        if (Count <= Index)
            break;

        PWSTR EndP = Volume + lstrlenW(Volume);
        if (Volume < EndP && L'\\' == EndP[-1])
            EndP[-1] = L'\0';

        Error = GetVolumeOwnerProcessId(Volume, &OwnerProcessId);
        if (ERROR_SUCCESS != Error || ProcessId != OwnerProcessId)
            continue;

        Error = OpenAndDismountVolume(Volume, &Handles[Index]);
        if (ERROR_SUCCESS == Error)
            Index++;
        else
        if (ERROR_DEVICE_IN_USE == Error)
            DevicesInUse++;

    } while (FindNextVolumeW(FindHandle, Volume, MAX_PATH));

    /* treat only non-zero DevicesInUse as error */
    Error = 0 == DevicesInUse ? ERROR_SUCCESS : ERROR_DEVICE_IN_USE;

    *PCount = Index;

exit:
    if (ERROR_SUCCESS != Error)
        CloseHandles(Handles, Index);

    if (INVALID_HANDLE_VALUE != FindHandle)
        FindVolumeClose(FindHandle);

    return Error;
}

static SVC_INSTANCE *SvcInstanceLookupByPid(DWORD ProcessId)
{
    SVC_INSTANCE *SvcInstance;
    PLIST_ENTRY ListEntry;

    for (ListEntry = SvcInstanceList.Flink;
        &SvcInstanceList != ListEntry;
        ListEntry = ListEntry->Flink)
    {
        SvcInstance = CONTAINING_RECORD(ListEntry, SVC_INSTANCE, ListEntry);

        if (ProcessId == SvcInstance->ProcessId)
            return SvcInstance;
    }

    return 0;
}

static SVC_INSTANCE *SvcInstanceLookup(PWSTR ClassName, PWSTR InstanceName,
    BOOLEAN AllowPidLookup)
{
    if (AllowPidLookup && 0 == invariant_wcsicmp(ClassName, L".pid."))
    {
        PWSTR Endp;
        DWORD ProcessId;

        ProcessId = (DWORD)wcstoint(InstanceName, 0, 0, &Endp);

        return SvcInstanceLookupByPid(ProcessId);
    }

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

static BOOLEAN SvcInstanceSetPersistent(
    PWSTR ClassName, PWSTR InstanceName, ULONG Argc, PWSTR *Argv,
    PSECURITY_DESCRIPTOR SecurityDescriptor)
{
    HKEY RegKey = 0, ClassRegKey = 0;
    DWORD RegSize, Persistent, ArgSize;
    PWSTR Security = 0;
    PWSTR ArgBuf = 0, ArgP;
    DWORD Error;
    BOOLEAN Result = FALSE;

    Error = RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"" SPD_LAUNCH_REGKEY,
        0, SPD_LAUNCH_REGKEY_WOW64 | KEY_READ, &RegKey);
    if (ERROR_SUCCESS != Error)
        goto exit;

    RegSize = sizeof Persistent;
    Error = RegGetValueW(RegKey, ClassName, L"Persistent", RRF_RT_REG_DWORD, 0,
        &Persistent, &RegSize);
    if (ERROR_SUCCESS != Error)
        goto exit;

    if (!Persistent)
        goto exit;

    if (!ConvertSecurityDescriptorToStringSecurityDescriptorW(SecurityDescriptor,
        SDDL_REVISION_1,
        OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION,
        &Security, 0))
    {
        Error = GetLastError();
        goto exit;
    }

    ArgSize = 1;
    for (ULONG I = 0; Argc > I; I++)
        ArgSize += lstrlenW(Argv[I]) + 1;
    ArgSize += lstrlenW(Security) + 1;
    ArgSize *= sizeof(WCHAR);

    ArgBuf = MemAlloc(ArgSize);
    if (0 == ArgBuf)
    {
        Error = ERROR_NO_SYSTEM_RESOURCES;
        goto exit;
    }

    ArgP = ArgBuf;
    for (ULONG I = 0; Argc > I; I++)
    {
        ArgSize = lstrlen(Argv[I]) + 1;
        memcpy(ArgP, Argv[I], ArgSize * sizeof(WCHAR));
        ArgP += ArgSize;
    }
    ArgSize = lstrlen(Security) + 1;
    memcpy(ArgP, Security, ArgSize * sizeof(WCHAR));
    ArgP += ArgSize;
    *ArgP++ = L'\0';

    Error = RegOpenKeyExW(RegKey, ClassName,
        0, SPD_LAUNCH_REGKEY_WOW64 | KEY_ALL_ACCESS, &ClassRegKey);
    if (ERROR_SUCCESS != Error)
        goto exit;

    Error = RegSetKeyValueW(ClassRegKey, L"Instances", InstanceName,
        REG_MULTI_SZ, ArgBuf, (DWORD)((PUINT8)ArgP - (PUINT8)ArgBuf));
    if (ERROR_SUCCESS != Error)
        goto exit;

    Result = TRUE;

exit:
    MemFree(ArgBuf);

    LocalFree(Security);

    if (0 != ClassRegKey)
        RegCloseKey(ClassRegKey);

    if (0 != RegKey)
        RegCloseKey(RegKey);

    return Result;
}

static VOID SvcInstanceDeletePersistent(
    PWSTR ClassName, PWSTR InstanceName)
{
    HKEY RegKey = 0, ClassRegKey = 0;
    DWORD Error;

    Error = RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"" SPD_LAUNCH_REGKEY,
        0, SPD_LAUNCH_REGKEY_WOW64 | KEY_READ, &RegKey);
    if (ERROR_SUCCESS != Error)
        goto exit;

    Error = RegOpenKeyExW(RegKey, ClassName,
        0, SPD_LAUNCH_REGKEY_WOW64 | KEY_ALL_ACCESS, &ClassRegKey);
    if (ERROR_SUCCESS != Error)
        goto exit;

    Error = RegDeleteKeyValueW(ClassRegKey, L"Instances", InstanceName);
    if (ERROR_SUCCESS != Error)
        goto exit;

    Error = ERROR_SUCCESS;

exit:
    if (0 != ClassRegKey)
        RegCloseKey(ClassRegKey);

    if (0 != RegKey)
        RegCloseKey(RegKey);
}

__declspec(noinline) /* noinline resolves "unresolved external symbol __chkstk" on Release builds */
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

static DWORD SvcInstanceCreate(HANDLE ClientToken, PWSTR ExternalSecurity,
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

    if (0 == invariant_wcsicmp(ClassName, L".pid."))
    {
        Error = ERROR_INVALID_PARAMETER;
        goto exit;
    }

    if (0 != SvcInstanceLookup(ClassName, InstanceName, FALSE))
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

    if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(
        0 != ExternalSecurity ? ExternalSecurity : Security,
        SDDL_REVISION_1, &SecurityDescriptor, 0))
    {
        Error = GetLastError();
        goto exit;
    }

    if (0 != ClientToken)
    {
        Error = SvcInstanceAccessCheck(ClientToken, SERVICE_START, SecurityDescriptor);
        if (ERROR_SUCCESS != Error)
            goto exit;

        Error = SvcInstanceAddUserRights(ClientToken, SecurityDescriptor, &NewSecurityDescriptor);
        if (ERROR_SUCCESS != Error)
            goto exit;
        LocalFree(SecurityDescriptor);
        SecurityDescriptor = NewSecurityDescriptor;
    }

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
            SpdServiceLog(EVENTLOG_WARNING_TYPE,
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

    SpdServiceLog(EVENTLOG_INFORMATION_TYPE,
        L"create %s %s = %ld", ClassName, InstanceName, Error);

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

    if (0 != SvcInstance->VolumeHandles)
    {
        CloseHandles(SvcInstance->VolumeHandles, SvcInstance->VolumeHandleCount);
        MemFree(SvcInstance->VolumeHandles);
    }

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

    SpdServiceLog(EVENTLOG_INFORMATION_TYPE,
        L"terminated %s %s", SvcInstance->ClassName, SvcInstance->InstanceName);

    SvcInstanceRelease(SvcInstance);
}

static DWORD SvcInstanceKill(SVC_INSTANCE *SvcInstance, BOOLEAN Forced)
{
    HANDLE Handles[256];
    ULONG Count;
    DWORD Error;

    if (0 == SvcInstance->VolumeHandles)
    {
        Count = sizeof Handles / sizeof Handles[0];
        Error = OpenAndDismountVolumesForProcessId(SvcInstance->ProcessId, Handles, &Count);
        if (ERROR_SUCCESS == Error && 0 < Count)
        {
            SvcInstance->VolumeHandles = MemAlloc(Count * sizeof(HANDLE));
            if (0 != SvcInstance->VolumeHandles)
            {
                memcpy(SvcInstance->VolumeHandles, Handles, Count * sizeof(HANDLE));
                SvcInstance->VolumeHandleCount = Count;
                Count = 0;
            }
        }
    }
    else
    {
        /* if we already have volume handles we are not going to try dismounting again! */
        Count = 0;
        Error = ERROR_SUCCESS;
    }

    if (ERROR_SUCCESS == Error || Forced)
        KillProcess(SvcInstance->ProcessId, SvcInstance->Process, LAUNCHER_KILL_TIMEOUT);

    CloseHandles(Handles, Count);

    return Error;
}

static DWORD SvcInstanceStart(HANDLE ClientToken,
    PWSTR ClassName, PWSTR InstanceName, ULONG Argc, PWSTR *Argv, HANDLE Job)
{
    SVC_INSTANCE *SvcInstance;
    DWORD Error;

    Error = SvcInstanceCreate(ClientToken, 0, ClassName, InstanceName, Argc, Argv, Job,
        &SvcInstance);
    if (ERROR_SUCCESS != Error)
        return Error;

    SvcInstance->Persistent = SvcInstanceSetPersistent(
        SvcInstance->ClassName, SvcInstance->InstanceName, Argc, Argv,
        SvcInstance->SecurityDescriptor);

    SvcInstanceRelease(SvcInstance);

    return ERROR_SUCCESS;
}

static DWORD SvcInstanceStop(HANDLE ClientToken,
    PWSTR ClassName, PWSTR InstanceName, BOOLEAN Forced)
{
    SVC_INSTANCE *SvcInstance;
    DWORD Error;

    EnterCriticalSection(&SvcInstanceLock);

    SvcInstance = SvcInstanceLookup(ClassName, InstanceName, TRUE);
    if (0 == SvcInstance)
    {
        Error = ERROR_FILE_NOT_FOUND;
        goto exit;
    }

    Error = SvcInstanceAccessCheck(ClientToken, SERVICE_STOP, SvcInstance->SecurityDescriptor);
    if (ERROR_SUCCESS != Error)
        goto exit;

    Error = SvcInstanceKill(SvcInstance, Forced);
    if (ERROR_SUCCESS == Error)
        SvcInstanceDeletePersistent(
            SvcInstance->ClassName, SvcInstance->InstanceName);

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

    SvcInstance = SvcInstanceLookup(ClassName, InstanceName, TRUE);
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

static DWORD SvcInstanceStartAllPersistent(VOID)
{
    HKEY RegKey = 0, ClassRegKey = 0, InstancesRegKey = 0;
    WCHAR ClassName[64], InstanceName[4/*\\?\*/ + MAX_PATH], RegValue[1024 + 512];
    DWORD RegNameSize, RegValueSize, RegType;
    ULONG Argc;
    PWSTR Argv[10 + 1];
    SVC_INSTANCE *SvcInstance;
    DWORD Error;

    Error = RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"" SPD_LAUNCH_REGKEY,
        0, SPD_LAUNCH_REGKEY_WOW64 | KEY_READ, &RegKey);
    if (ERROR_SUCCESS != Error)
        goto exit;

    for (ULONG I = 0;; I++)
    {
        RegNameSize = sizeof ClassName / sizeof ClassName[0];
        if (ERROR_SUCCESS != RegEnumKeyExW(RegKey, I, ClassName, &RegNameSize, 0, 0, 0, 0))
            break;

        Error = RegOpenKeyExW(RegKey, ClassName,
            0, SPD_LAUNCH_REGKEY_WOW64 | KEY_READ, &ClassRegKey);
        if (ERROR_SUCCESS == Error)
        {
            Error = RegOpenKeyExW(ClassRegKey, L"Instances",
                0, SPD_LAUNCH_REGKEY_WOW64 | KEY_READ, &InstancesRegKey);
            if (ERROR_SUCCESS == Error)
            {
                for (ULONG J = 0;; J++)
                {
                    RegNameSize = sizeof InstanceName / sizeof InstanceName[0];
                    RegValueSize = sizeof RegValue;
                    if (ERROR_SUCCESS != RegEnumValueW(InstancesRegKey, J,
                            InstanceName, &RegNameSize, 0, &RegType, (PVOID)RegValue, &RegValueSize) ||
                        REG_MULTI_SZ != RegType)
                        break;

                    Argc = 0;
                    for (PWSTR P = RegValue;
                        sizeof(Argv) / sizeof(Argv[0]) > Argc &&
                            (DWORD)((PUINT8)P - (PUINT8)RegValue) < RegValueSize;
                        P = P + lstrlenW(P) + 1)
                        Argv[Argc++] = P;

                    if (2 < Argc)
                    {
                        Argc -= 2;
                        Error = SvcInstanceCreate(
                            0, Argv[Argc], ClassName, InstanceName, Argc, Argv, SvcJob,
                            &SvcInstance);
                        if (ERROR_SUCCESS == Error)
                            SvcInstanceRelease(SvcInstance);
                    }
                }

                RegCloseKey(InstancesRegKey);
                InstancesRegKey = 0;
            }

            RegCloseKey(ClassRegKey);
            ClassRegKey = 0;
        }
    }

    Error = ERROR_SUCCESS;

exit:
    if (0 != InstancesRegKey)
        RegCloseKey(InstancesRegKey);

    if (0 != ClassRegKey)
        RegCloseKey(ClassRegKey);

    if (0 != RegKey)
        RegCloseKey(RegKey);

    return Error;
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

        SvcInstanceKill(SvcInstance, TRUE);
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
    BOOLEAN Forced = FALSE;
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

    case SpdLaunchCmdStopForced:
        Forced = TRUE;
        /* fall through! */
    case SpdLaunchCmdStop:
        ClassName = SvcPipeTransactGetPart(&P, PipeBufEnd);
        InstanceName = SvcPipeTransactGetPart(&P, PipeBufEnd);

        Error = ERROR_INVALID_PARAMETER;
        if (0 != ClassName && 0 != InstanceName)
            Error = SvcInstanceStop(ClientToken, ClassName, InstanceName, Forced);

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
            SpdServiceLog(EVENTLOG_WARNING_TYPE, LoopWarningMessage,
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
                SpdServiceLog(EVENTLOG_WARNING_TYPE, LoopWarningMessage,
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
                SpdServiceLog(EVENTLOG_WARNING_TYPE, LoopWarningMessage,
                    L"ImpersonateNamedPipeClient||OpenThreadToken", LastError);
                continue;
            }
            else
            {
                CloseHandle(ClientToken);
                DisconnectNamedPipe(SvcPipe);
                SpdServiceLog(EVENTLOG_ERROR_TYPE, LoopErrorMessage,
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
            SpdServiceLog(EVENTLOG_WARNING_TYPE, LoopWarningMessage,
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

    SpdServiceLog(EVENTLOG_INFORMATION_TYPE,
        L"The service %s has been stopped.", L"" PROGNAME);

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

    SvcInstanceStartAllPersistent();

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
        SpdServiceLog(EVENTLOG_ERROR_TYPE,
            L"" __FUNCTION__ ": RegisterServiceCtrlHandlerW = %ld", GetLastError());
        return;
    }

    SvcStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    SvcStatus.dwCurrentState = SERVICE_START_PENDING;
    SvcStatus.dwControlsAccepted = 0;
    SvcStatus.dwWin32ExitCode = ERROR_SUCCESS;
    SvcStatus.dwServiceSpecificExitCode = 0;
    SvcStatus.dwCheckPoint = 0;
    SvcStatus.dwWaitHint = LAUNCHER_START_TIMEOUT;
    SetServiceStatus(SvcHandle, &SvcStatus);

    Error = SvcOnStart(Argc, Argv);
    if (ERROR_SUCCESS == Error)
    {
        SvcStatus.dwCurrentState = SERVICE_RUNNING;
        SvcStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP;
        SetServiceStatus(SvcHandle, &SvcStatus);

        SpdServiceLog(EVENTLOG_INFORMATION_TYPE,
            L"The service %s has been started.", L"" PROGNAME);
    }
    else
    {
        SvcStatus.dwCurrentState = SERVICE_STOPPED;
        SvcStatus.dwControlsAccepted = 0;
        SetServiceStatus(SvcHandle, &SvcStatus);

        SpdServiceLog(EVENTLOG_ERROR_TYPE,
            L"The service %s has failed to start (Error=%ld).", L"" PROGNAME, Error);
    }
}

int wmain(int argc, wchar_t **argv)
{
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
