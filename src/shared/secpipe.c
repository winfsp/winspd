/**
 * @file shared/secpipe.c
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

DWORD SpdCallNamedPipeSecurely(PWSTR PipeName,
    PVOID InBuffer, ULONG InBufferSize, PVOID OutBuffer, ULONG OutBufferSize,
    PULONG PBytesTransferred, ULONG Timeout,
    PSID Sid)
{
    return SpdCallNamedPipeSecurelyEx(PipeName,
        InBuffer, InBufferSize, OutBuffer, OutBufferSize, PBytesTransferred, Timeout,
        FALSE, Sid);
}

DWORD SpdCallNamedPipeSecurelyEx(PWSTR PipeName,
    PVOID InBuffer, ULONG InBufferSize, PVOID OutBuffer, ULONG OutBufferSize,
    PULONG PBytesTransferred, ULONG Timeout, BOOLEAN AllowImpersonation,
    PSID Sid)
{
    HANDLE Pipe = INVALID_HANDLE_VALUE;
    DWORD PipeMode;
    DWORD Error;

    Pipe = CreateFileW(PipeName,
        GENERIC_READ | FILE_WRITE_DATA | FILE_WRITE_ATTRIBUTES,
        FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING,
        SECURITY_SQOS_PRESENT | (AllowImpersonation ? SECURITY_IMPERSONATION : SECURITY_IDENTIFICATION),
        0);
    if (INVALID_HANDLE_VALUE == Pipe)
    {
        if (ERROR_PIPE_BUSY != GetLastError())
        {
            Error = GetLastError();
            goto exit;
        }

        WaitNamedPipeW(PipeName, Timeout);

        Pipe = CreateFileW(PipeName,
            GENERIC_READ | FILE_WRITE_DATA | FILE_WRITE_ATTRIBUTES,
            FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING,
            SECURITY_SQOS_PRESENT | (AllowImpersonation ? SECURITY_IMPERSONATION : SECURITY_IDENTIFICATION),
            0);
        if (INVALID_HANDLE_VALUE == Pipe)
        {
            Error = GetLastError();
            goto exit;
        }
    }

    if (0 != Sid)
    {
        PSECURITY_DESCRIPTOR SecurityDescriptor = 0;
        PSID OwnerSid;
        union
        {
            UINT8 B[SECURITY_MAX_SID_SIZE];
            SID S;
        } SidBuf;
        DWORD SidSize;

        /* if it is a small number treat it like a well known SID */
        if (1024 > (INT_PTR)Sid)
        {
            SidSize = SECURITY_MAX_SID_SIZE;
            if (!CreateWellKnownSid((INT_PTR)Sid, 0, &SidBuf.S, &SidSize))
            {
                Error = GetLastError();
                goto sid_exit;
            }

            Sid = &SidBuf.S;
        }

        Error = GetSecurityInfo(Pipe, SE_FILE_OBJECT,
            OWNER_SECURITY_INFORMATION, &OwnerSid, 0, 0, 0, &SecurityDescriptor);
        if (0 != Error)
            goto sid_exit;

        if (!EqualSid(OwnerSid, Sid))
        {
            Error = ERROR_ACCESS_DENIED;
            goto sid_exit;
        }

        Error = ERROR_SUCCESS;

    sid_exit:
        LocalFree(SecurityDescriptor);

        if (ERROR_SUCCESS != Error)
            goto exit;
    }

    PipeMode = PIPE_READMODE_MESSAGE | PIPE_WAIT;
    if (!SetNamedPipeHandleState(Pipe, &PipeMode, 0, 0))
    {
        Error = GetLastError();
        goto exit;
    }

    if (!TransactNamedPipe(Pipe, InBuffer, InBufferSize, OutBuffer, OutBufferSize,
        PBytesTransferred, 0))
    {
        Error = GetLastError();
        goto exit;
    }

    Error = ERROR_SUCCESS;

exit:
    if (INVALID_HANDLE_VALUE != Pipe)
        CloseHandle(Pipe);

    return Error;
}
