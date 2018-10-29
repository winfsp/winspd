/**
 * @file scsitool/scsitool.c
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

#define PROGNAME                        "scsitool"

#define GLOBAL                          L"\\\\?\\"
#define GLOBALROOT                      L"\\\\?\\GLOBALROOT"

#define info(format, ...)               printlog(GetStdHandle(STD_OUTPUT_HANDLE), format, __VA_ARGS__)
#define warn(format, ...)               printlog(GetStdHandle(STD_ERROR_HANDLE), format, __VA_ARGS__)
#define fatal(ExitCode, format, ...)    (warn(format, __VA_ARGS__), ExitProcess(ExitCode))

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

static void usage(void)
{
    fatal(ERROR_INVALID_PARAMETER,
        "usage: %s COMMAND ARGS\n"
        "\n"
        "commands:\n"
        "    devpath device-name\n"
        "    inquiry device-name\n"
        "    report-luns device-name\n",
        PROGNAME);
}

static DWORD ScsiControl(PWSTR DeviceName,
    DWORD Ptl, PCDB Cdb, UCHAR DataDirection, PVOID DataBuffer, PDWORD PDataLength,
    PUCHAR PScsiStatus, UCHAR SenseInfoBuffer[32])
{
    HANDLE DeviceHandle = INVALID_HANDLE_VALUE;
    DWORD Error;

    Error = SpdOpenDevice(DeviceName, &DeviceHandle);
    if (ERROR_SUCCESS != Error)
        goto exit;

    Error = SpdScsiControl(DeviceHandle, Ptl, Cdb,
        DataDirection, DataBuffer, PDataLength, PScsiStatus, SenseInfoBuffer);

exit:
    if (INVALID_HANDLE_VALUE != DeviceHandle)
        CloseHandle(DeviceHandle);

    return Error;
}

static void ScsiPrint(const char *format, void *buf, size_t len)
{
    void ScsiLineText(HANDLE h, const char *format, void *buf, size_t len);

    ScsiLineText(GetStdHandle(STD_OUTPUT_HANDLE), format, buf, len);
}

static void PrintSenseInfo(UCHAR ScsiStatus, UCHAR SenseInfoBuffer[32])
{
    info("ScsiStatus=%u", ScsiStatus);

    ScsiPrint(
        "u1  VALID\n"
        "u7  RESPONSE CODE (70h or 71h)\n"
        "u8  Obsolete\n"
        "u1  FILEMARK\n"
        "u1  EOM\n"
        "u1  ILI\n"
        "u1  Reserved\n"
        "u4  SENSE KEY\n"
        "u16 INFORMATION\n"
        "u8  ADDITIONAL SENSE LENGTH (n-7)\n"
        "u16 COMMAND-SPECIFIC INFORMATION\n"
        "u8  ADDITIONAL SENSE CODE\n"
        "u8  ADDITIONAL SENSE CODE QUALIFIER\n"
        "u8  FIELD REPLACEABLE UNIT CODE\n"
        "u1  SKSV\n"
        "u15 SENSE KEY SPECIFIC\n",
        SenseInfoBuffer, 32);
}

static int devpath(int argc, wchar_t **argv)
{
    if (2 != argc)
        usage();

    WCHAR PathBuf[1024];
    DWORD Error;

    Error = SpdGetDevicePath(argv[1], PathBuf, sizeof PathBuf);
    if (ERROR_SUCCESS != Error)
        goto exit;

    info("%S", PathBuf);

exit:
    return Error;
}

static int inquiry(int argc, wchar_t **argv)
{
    if (2 != argc)
        usage();

    CDB Cdb;
    PVOID DataBuffer = 0;
    DWORD DataLength = VPD_MAX_BUFFER_SIZE;
    UCHAR ScsiStatus;
    UCHAR SenseInfoBuffer[32];
    DWORD Error;

    Error = SpdMemAlignAlloc(DataLength, 511, &DataBuffer);
    if (ERROR_SUCCESS != Error)
        goto exit;

    memset(&Cdb, 0, sizeof Cdb);
    Cdb.CDB6INQUIRY3.OperationCode = SCSIOP_INQUIRY;
    Cdb.CDB6INQUIRY3.AllocationLength = VPD_MAX_BUFFER_SIZE;

    Error = ScsiControl(argv[1], 0, &Cdb, 1/*SCSI_IOCTL_DATA_IN*/,
        DataBuffer, &DataLength, &ScsiStatus, SenseInfoBuffer);
    if (ERROR_SUCCESS != Error)
        goto exit;

    if (SCSISTAT_GOOD == ScsiStatus)
    {
        ScsiPrint(
            "u3  PERIPHERAL QUALIFIER\n"
            "u5  PERIPHERAL DEVICE TYPE\n"
            "u1  RMB\n"
            "u7  Reserved\n"
            "u8  VERSION\n"
            "u1  Obsolete\n"
            "u1  Obsolete\n"
            "u1  NORMACA\n"
            "u1  HISUP\n"
            "u4  RESPONSE DATA FORMAT\n"
            "u8  ADDITIONAL LENGTH (n-4)\n"
            "u1  SCCS\n"
            "u1  ACC\n"
            "u2  TPGS\n"
            "u1  3PC\n"
            "u2  Reserved\n"
            "u1  PROTECT\n"
            "u1  BQUE\n"
            "u1  ENCSERV\n"
            "u1  VS\n"
            "u1  MULTIP\n"
            "u1  MCHNGR\n"
            "u1  Obsolete\n"
            "u1  Obsolete\n"
            "u1  ADDR16\n"
            "u1  Obsolete\n"
            "u1  Obsolete\n"
            "u1  WBUS16\n"
            "u1  SYNC\n"
            "u1  LINKED\n"
            "u1  Obsolete\n"
            "u1  CMDQUE\n"
            "u1  VS\n"
            "S8  T10 VENDOR IDENTIFICATION\n"
            "S16 PRODUCT IDENTIFICATION\n"
            "S8  PRODUCT REVISION LEVEL\n"
            "X20 Vendor specific\n"
            "u4  Reserved\n"
            "u2  CLOCKING\n"
            "u1  QAS\n"
            "u1  IUS\n"
            "u8  Reserved\n"
            "u16 VERSION DESCRIPTOR 1\n"
            "u16 VERSION DESCRIPTOR 2\n"
            "u16 VERSION DESCRIPTOR 3\n"
            "u16 VERSION DESCRIPTOR 4\n"
            "u16 VERSION DESCRIPTOR 5\n"
            "u16 VERSION DESCRIPTOR 6\n"
            "u16 VERSION DESCRIPTOR 7\n"
            "u16 VERSION DESCRIPTOR 8\n"
            "X22 Reserved\n"
            "",
            DataBuffer, DataLength);
    }
    else
        PrintSenseInfo(ScsiStatus, SenseInfoBuffer);

exit:
    SpdMemAlignFree(DataBuffer);

    return Error;
}

static int report_luns(int argc, wchar_t **argv)
{
    if (2 != argc)
        usage();

    return 0;
}

int wmain(int argc, wchar_t **argv)
{
    argc--;
    argv++;

    if (0 == argc)
        usage();

    if (0 == invariant_wcscmp(L"devpath", argv[0]))
        return devpath(argc, argv);
    else
    if (0 == invariant_wcscmp(L"inquiry", argv[0]))
        return inquiry(argc, argv);
    else
    if (0 == invariant_wcscmp(L"report-luns", argv[0]))
        return report_luns(argc, argv);
    else
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
