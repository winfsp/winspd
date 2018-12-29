/**
 * @file stgpipe/stgpipe.c
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

#define PROGNAME                        "stgpipe"

#define info(format, ...)               printlog(GetStdHandle(STD_OUTPUT_HANDLE), format, __VA_ARGS__)
#define warn(format, ...)               printlog(GetStdHandle(STD_ERROR_HANDLE), format, __VA_ARGS__)
#define fail(ExitCode, format, ...)     (warn(format, __VA_ARGS__), ExitProcess(ExitCode))

static void vprintlog(HANDLE h, const char *format, va_list ap)
{
    char buf[1024];
        /* wvsprintf is only safe with a 1024 byte buffer */
    size_t len;
    DWORD BytesTransferred;

    wvsprintfA(buf, format, ap);
    buf[sizeof buf - 1] = '\0';

    len = lstrlenA(buf);
    buf[len++] = '\n';

    WriteFile(h, buf, (DWORD)len, &BytesTransferred, 0);
}

static void printlog(HANDLE h, const char *format, ...)
{
    va_list ap;

    va_start(ap, format);
    vprintlog(h, format, ap);
    va_end(ap);
}

typedef struct
{
    SPD_IOCTL_TRANSACT_REQ Req;
    SPD_IOCTL_TRANSACT_RSP Rsp;
} TRANSACT_MSG;

static DWORD StgPipeOpen(PWSTR PipeName, ULONG Timeout,
    PHANDLE PHandle, SPD_IOCTL_STORAGE_UNIT_PARAMS *StorageUnitParams)
{
    HANDLE Handle = INVALID_HANDLE_VALUE;
    DWORD PipeMode;
    DWORD BytesTransferred;
    DWORD Error;

    Handle = CreateFileW(PipeName,
        GENERIC_READ | FILE_WRITE_DATA | FILE_WRITE_ATTRIBUTES,
        FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING,
        FILE_FLAG_OVERLAPPED | SECURITY_SQOS_PRESENT | SECURITY_IDENTIFICATION,
        0);
    if (INVALID_HANDLE_VALUE == Handle)
    {
        Error = GetLastError();
        if (ERROR_PIPE_BUSY != Error)
            goto exit;

        WaitNamedPipeW(PipeName, Timeout);

        Handle = CreateFileW(PipeName,
            GENERIC_READ | FILE_WRITE_DATA | FILE_WRITE_ATTRIBUTES,
            FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING,
            FILE_FLAG_OVERLAPPED | SECURITY_SQOS_PRESENT | SECURITY_IDENTIFICATION,
            0);
        if (INVALID_HANDLE_VALUE == Handle)
        {
            Error = GetLastError();
            goto exit;
        }
    }

    PipeMode = PIPE_READMODE_MESSAGE | PIPE_WAIT;
    if (!SetNamedPipeHandleState(Handle, &PipeMode, 0, 0))
    {
        Error = GetLastError();
        goto exit;
    }

    /* ok to use non-overlapped ReadFile with overlapped handle, because no other I/O is posted on it */
    if (!ReadFile(Handle, StorageUnitParams, sizeof *StorageUnitParams, &BytesTransferred, 0))
    {
        Error = GetLastError();
        goto exit;
    }
    if (sizeof *StorageUnitParams > BytesTransferred)
    {
        Error = ERROR_IO_DEVICE;
        goto exit;
    }

    Error = ERROR_SUCCESS;

exit:
    if (ERROR_SUCCESS != Error)
    {
        if (INVALID_HANDLE_VALUE != Handle)
            CloseHandle(Handle);
    }

    return Error;
}

static inline DWORD WaitOverlappedResult(BOOL Success,
    HANDLE Handle, OVERLAPPED *Overlapped, PDWORD PBytesTransferred)
{
    if (!Success && ERROR_IO_PENDING != GetLastError())
        return GetLastError();

    if (!GetOverlappedResult(Handle, Overlapped, PBytesTransferred, TRUE))
        return GetLastError();

    return ERROR_SUCCESS;
}

DWORD StgPipeTransact(HANDLE Handle,
    SPD_IOCTL_TRANSACT_REQ *Req,
    SPD_IOCTL_TRANSACT_RSP *Rsp,
    PVOID DataBuffer,
    const SPD_IOCTL_STORAGE_UNIT_PARAMS *StorageUnitParams)
{
    ULONG DataLength;
    TRANSACT_MSG *Msg = 0;
    OVERLAPPED Overlapped;
    DWORD BytesTransferred;
    DWORD Error;

    if (0 == Req || 0 == Rsp)
    {
        Error = ERROR_INVALID_PARAMETER;
        goto exit;
    }

    memset(&Overlapped, 0, sizeof Overlapped);

    Msg = MemAlloc(
        sizeof(TRANSACT_MSG) + StorageUnitParams->MaxTransferLength);
    if (0 == Msg)
    {
        Error = ERROR_NO_SYSTEM_RESOURCES;
        goto exit;
    }

    Overlapped.hEvent = CreateEventW(0, TRUE, TRUE, 0);
    if (0 == Overlapped.hEvent)
    {
        Error = GetLastError();
        goto exit;
    }

    DataLength = 0;
    if (0 != DataBuffer)
        switch (Req->Kind)
        {
        case SpdIoctlTransactWriteKind:
            DataLength = Msg->Req.Op.Write.BlockCount * StorageUnitParams->BlockLength;
            break;
        case SpdIoctlTransactUnmapKind:
            DataLength = Msg->Req.Op.Unmap.Count * sizeof(SPD_IOCTL_UNMAP_DESCRIPTOR);
            break;
        default:
            break;
        }
    memcpy(Msg, Req, sizeof *Req);
    if (0 != DataLength)
        memcpy(Msg + 1, DataBuffer, DataLength);
    Error = WaitOverlappedResult(
        WriteFile(Handle, Msg, sizeof(TRANSACT_MSG) + DataLength, 0, &Overlapped),
        Handle, &Overlapped, &BytesTransferred);
    if (ERROR_SUCCESS != Error)
        goto exit;

    Error = WaitOverlappedResult(
        ReadFile(Handle,
            Msg, sizeof(TRANSACT_MSG) + StorageUnitParams->MaxTransferLength, 0, &Overlapped),
        Handle, &Overlapped, &BytesTransferred);
    if (ERROR_SUCCESS != Error)
        goto exit;
    if (sizeof(TRANSACT_MSG) > BytesTransferred || Req->Hint != Msg->Rsp.Hint)
    {
        Error = ERROR_IO_DEVICE;
        goto exit;
    }
    if (SpdIoctlTransactReadKind == Msg->Rsp.Kind && SCSISTAT_GOOD == Msg->Rsp.Status.ScsiStatus)
    {
        DataLength = Msg->Req.Op.Read.BlockCount * StorageUnitParams->BlockLength;
        if (DataLength > StorageUnitParams->MaxTransferLength)
        {
            Error = ERROR_IO_DEVICE;
            goto exit;
        }
        BytesTransferred -= sizeof(TRANSACT_MSG);
        if (BytesTransferred > DataLength)
            BytesTransferred = DataLength;
        memcpy(DataBuffer, Msg + 1, BytesTransferred);
        memset((PUINT8)(DataBuffer) + BytesTransferred, 0, DataLength - BytesTransferred);
    }
    memcpy(Req, &Msg->Req, sizeof *Req);

    Error = ERROR_SUCCESS;

exit:
    if (0 != Overlapped.hEvent)
        CloseHandle(Overlapped.hEvent);

    MemFree(Msg);

    return Error;
}

static void usage(void)
{
    warn(
        "usage: %s PIPENAME N [RWFU]\n",
        PROGNAME);

    ExitProcess(ERROR_INVALID_PARAMETER);
}

int wmain(int argc, wchar_t **argv)
{
    usage();

    return 0;
}

void wmainCRTStartup(void)
{
    DWORD Argc;
    PWSTR *Argv;

    Argv = CommandLineToArgvW(GetCommandLineW(), &Argc);
    if (0 == Argv)
        ExitProcess(GetLastError());

    ExitProcess(wmain(Argc, Argv));
}
