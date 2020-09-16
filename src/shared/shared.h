/**
 * @file shared/shared.h
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

#ifndef WINSPD_SHARED_SHARED_H_INCLUDED
#define WINSPD_SHARED_SHARED_H_INCLUDED

#include <winspd/winspd.h>
#include <shared/minimal.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Registry
 */

typedef struct _REGENTRY
{
    PWSTR Name;
    ULONG Type;
    PVOID Value;
    ULONG Size;
} REGENTRY;

DWORD RegAddEntries(HKEY Key, REGENTRY *Entries, ULONG Count, PBOOLEAN PKeyAdded);
DWORD RegDeleteEntries(HKEY Key, REGENTRY *Entries, ULONG Count, PBOOLEAN PKeyDeleted);

/*
 * Secure Pipes
 */

DWORD SpdCallNamedPipeSecurely(PWSTR PipeName,
    PVOID InBuffer, ULONG InBufferSize, PVOID OutBuffer, ULONG OutBufferSize,
    PULONG PBytesTransferred, ULONG Timeout,
    PSID Sid);
DWORD SpdCallNamedPipeSecurelyEx(PWSTR PipeName,
    PVOID InBuffer, ULONG InBufferSize, PVOID OutBuffer, ULONG OutBufferSize,
    PULONG PBytesTransferred, ULONG Timeout, BOOLEAN AllowImpersonation,
    PSID Sid);

/*
 * Launch
 */

#define SPD_LAUNCH_REGKEY               "Software\\WinSpd\\Services"
#define SPD_LAUNCH_REGKEY_WOW64         KEY_WOW64_32KEY

#define SPD_LAUNCH_PIPE_NAME            "\\\\.\\pipe\\WinSpd.{82021730-C8F8-4C54-BAEB-C04D111FEA57}"
#define SPD_LAUNCH_PIPE_BUFFER_SIZE     4096
#define SPD_LAUNCH_PIPE_OWNER           ((PSID)WinLocalSystemSid)

/*
 * The launcher named pipe SDDL gives full access to LocalSystem and Administrators and
 * GENERIC_READ and FILE_WRITE_DATA access to Everyone. We are careful not to give the
 * FILE_CREATE_PIPE_INSTANCE right to Everyone to disallow the creation of additional
 * pipe instances.
 */
#define SPD_LAUNCH_PIPE_SDDL            "O:SYG:SYD:P(A;;GA;;;SY)(A;;GA;;;BA)(A;;GRDCCR;;;WD)"

/*
 * The default service instance SDDL gives full access to LocalSystem and Administrators.
 * The only possible service instance rights are as follows:
 *     RP   SERVICE_START
 *     WP   SERVICE_STOP
 *     LC   SERVICE_QUERY_STATUS
 *
 * To create a service that can be started, stopped or queried by Everyone, you can set
 * the following SDDL:
 *     D:P(A;;RPWPLC;;;WD)
 */
#define SPD_LAUNCH_SERVICE_DEFAULT_SDDL "D:P(A;;RPWPLC;;;SY)(A;;RPWPLC;;;BA)"
#define SPD_LAUNCH_SERVICE_WORLD_SDDL   "D:P(A;;RPWPLC;;;WD)"

enum
{
    SpdLaunchCmdStart                   = 'S',  /* requires: SERVICE_START */
    SpdLaunchCmdStop                    = 'T',  /* requires: SERVICE_STOP */
    SpdLaunchCmdStopForced              = 'F',  /* requires: SERVICE_STOP */
    SpdLaunchCmdGetInfo                 = 'I',  /* requires: SERVICE_QUERY_STATUS */
    SpdLaunchCmdGetNameList             = 'L',  /* requires: none*/
    SpdLaunchCmdQuit                    = 'Q',  /* DEBUG version only */
};

enum
{
    SpdLaunchCmdSuccess                 = '$',
    SpdLaunchCmdFailure                 = '!',
};

DWORD SpdLaunchCallLauncherPipe(
    WCHAR Command, ULONG Argc, PWSTR *Argv, ULONG *Argl,
    PWSTR Buffer, PULONG PSize,
    PDWORD PLauncherError);
DWORD SpdLaunchStart(
    PWSTR ClassName, PWSTR InstanceName, ULONG Argc, PWSTR *Argv,
    PDWORD PLauncherError);
DWORD SpdLaunchStop(
    PWSTR ClassName, PWSTR InstanceName,
    PDWORD PLauncherError);
DWORD SpdLaunchGetInfo(
    PWSTR ClassName, PWSTR InstanceName,
    PWSTR Buffer, PULONG PSize,
    PDWORD PLauncherError);
DWORD SpdLaunchGetNameList(
    PWSTR Buffer, PULONG PSize,
    PDWORD PLauncherError);


/*
 * Diag
 */
PWSTR SpdDiagIdent(VOID);

/*
 * MemAlign
 */
DWORD SpdIoctlMemAlignAlloc(UINT32 Size, UINT32 AlignmentMask, PVOID *PP);
VOID SpdIoctlMemAlignFree(PVOID P);

/*
 * strtoint
 */
long long strtoint(const char *p, int base, int is_signed, const char **endp);
long long wcstoint(const wchar_t *p, int base, int is_signed, const wchar_t **endp);

#ifdef __cplusplus
}
#endif

#endif
