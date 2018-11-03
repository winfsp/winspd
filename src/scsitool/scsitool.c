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

#include <scsitool/scsitool.h>

#define info(format, ...)               printlog(GetStdHandle(STD_OUTPUT_HANDLE), format, __VA_ARGS__)
#define warn(format, ...)               printlog(GetStdHandle(STD_ERROR_HANDLE), format, __VA_ARGS__)
#define fatal(ExitCode, format, ...)    (warn(format, __VA_ARGS__), ExitProcess(ExitCode))

static void usage(void);

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

static void ScsiPrint(const char *format, void *buf, size_t len)
{
    ScsiLineText(GetStdHandle(STD_OUTPUT_HANDLE), format, buf, len);
}

static void ScsiPrintSenseInfo(UCHAR ScsiStatus, UCHAR SenseInfoBuffer[32])
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
        "u32 INFORMATION\n"
        "u8  ADDITIONAL SENSE LENGTH (n-7)\n"
        "u32 COMMAND-SPECIFIC INFORMATION\n"
        "u8  ADDITIONAL SENSE CODE\n"
        "u8  ADDITIONAL SENSE CODE QUALIFIER\n"
        "u8  FIELD REPLACEABLE UNIT CODE\n"
        "u1  SKSV\n"
        "u23 SENSE KEY SPECIFIC\n"
        "X14 Additional sense bytes",
        SenseInfoBuffer, 32);
}

static int ScsiDataInAndPrint(int argc, wchar_t **argv,
    PCDB Cdb, DWORD DataLength,
    const char *Format)
{
    if (2 > argc || argc > 3)
        usage();

    HANDLE DeviceHandle = INVALID_HANDLE_VALUE;
    DWORD Ptl = 0;
    PVOID DataBuffer = 0;
    UCHAR ScsiStatus;
    UCHAR SenseInfoBuffer[32];
    DWORD Error;

    if (3 == argc)
    {
        const wchar_t *p = argv[2];

        Ptl |= (UCHAR)wcstoint(p, 10, 0, &p);
        if (':' == *p++)
        {
            Ptl <<= 8;
            Ptl |= (UCHAR)wcstoint(p, 10, 0, &p);
            if (':' == *p++)
            {
                Ptl <<= 8;
                Ptl |= (UCHAR)wcstoint(p, 10, 0, &p);
            }
        }
    }

    Error = SpdIoctlOpenDevice(argv[1], &DeviceHandle);
    if (ERROR_SUCCESS != Error)
        goto exit;

    Error = SpdIoctlMemAlignAlloc(DataLength, 511, &DataBuffer);
    if (ERROR_SUCCESS != Error)
        goto exit;

    Error = SpdIoctlScsiExecute(DeviceHandle, Ptl, Cdb, 1/*SCSI_IOCTL_DATA_IN*/,
        DataBuffer, &DataLength, &ScsiStatus, SenseInfoBuffer);
    if (ERROR_SUCCESS != Error)
        goto exit;

    if (SCSISTAT_GOOD == ScsiStatus)
        ScsiPrint(Format, DataBuffer, DataLength);
    else
        ScsiPrintSenseInfo(ScsiStatus, SenseInfoBuffer);

exit:
    SpdIoctlMemAlignFree(DataBuffer);

    if (INVALID_HANDLE_VALUE != DeviceHandle)
        CloseHandle(DeviceHandle);

    return Error;
}

static int devpath(int argc, wchar_t **argv)
{
    if (2 != argc)
        usage();

    WCHAR PathBuf[1024];
    DWORD Error;

    Error = SpdIoctlGetDevicePath(0, argv[1], PathBuf, sizeof PathBuf);
    if (ERROR_SUCCESS != Error)
        goto exit;

    info("%S", PathBuf);

exit:
    return Error;
}

static int inquiry(int argc, wchar_t **argv)
{
    CDB Cdb;
    const char *Format;

    memset(&Cdb, 0, sizeof Cdb);
    Cdb.CDB6INQUIRY3.OperationCode = SCSIOP_INQUIRY;
    Cdb.CDB6INQUIRY3.AllocationLength = VPD_MAX_BUFFER_SIZE;

    Format =
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
        "X160 Vendor specific\n";

    return ScsiDataInAndPrint(argc, argv, &Cdb, VPD_MAX_BUFFER_SIZE, Format);
}

static int inquiry_vpd0(int argc, wchar_t **argv)
{
    CDB Cdb;
    const char *Format;

    memset(&Cdb, 0, sizeof Cdb);
    Cdb.CDB6INQUIRY3.OperationCode = SCSIOP_INQUIRY;
    Cdb.CDB6INQUIRY3.EnableVitalProductData = 1;
    Cdb.CDB6INQUIRY3.PageCode = 0;
    Cdb.CDB6INQUIRY3.AllocationLength = VPD_MAX_BUFFER_SIZE;

    Format =
        "u3  PERIPHERAL QUALIFIER\n"
        "u5  PERIPHERAL DEVICE TYPE\n"
        "u8  PAGE CODE (00h)\n"
        "u8  Reserved\n"
        "u8  PAGE LENGTH (n-3)\n"
        "X255 Supported VPD page list\n";

    return ScsiDataInAndPrint(argc, argv, &Cdb, VPD_MAX_BUFFER_SIZE, Format);
}

static int inquiry_vpd80(int argc, wchar_t **argv)
{
    CDB Cdb;
    const char *Format;

    memset(&Cdb, 0, sizeof Cdb);
    Cdb.CDB6INQUIRY3.OperationCode = SCSIOP_INQUIRY;
    Cdb.CDB6INQUIRY3.EnableVitalProductData = 1;
    Cdb.CDB6INQUIRY3.PageCode = 0x80;
    Cdb.CDB6INQUIRY3.AllocationLength = VPD_MAX_BUFFER_SIZE;

    Format =
        "u3  PERIPHERAL QUALIFIER\n"
        "u5  PERIPHERAL DEVICE TYPE\n"
        "u8  PAGE CODE (80h)\n"
        "u8  Reserved\n"
        "u8  PAGE LENGTH (n-3)\n"
        "X255 PRODUCT SERIAL NUMBER\n";

    return ScsiDataInAndPrint(argc, argv, &Cdb, VPD_MAX_BUFFER_SIZE, Format);
}

static int inquiry_vpd83(int argc, wchar_t **argv)
{
    CDB Cdb;
    const char *Format;

    memset(&Cdb, 0, sizeof Cdb);
    Cdb.CDB6INQUIRY3.OperationCode = SCSIOP_INQUIRY;
    Cdb.CDB6INQUIRY3.EnableVitalProductData = 1;
    Cdb.CDB6INQUIRY3.PageCode = 0x83;
    Cdb.CDB6INQUIRY3.AllocationLength = VPD_MAX_BUFFER_SIZE;

    Format =
        "u3  PERIPHERAL QUALIFIER\n"
        "u5  PERIPHERAL DEVICE TYPE\n"
        "u8  PAGE CODE (83h)\n"
        "u16 PAGE LENGTH (n-3)\n"
        "X255 Identification descriptor list\n";

    return ScsiDataInAndPrint(argc, argv, &Cdb, VPD_MAX_BUFFER_SIZE, Format);
}

static int report_luns(int argc, wchar_t **argv)
{
    CDB Cdb;
    const char *Format;

    memset(&Cdb, 0, sizeof Cdb);
    Cdb.REPORT_LUNS.OperationCode = SCSIOP_REPORT_LUNS;
    Cdb.REPORT_LUNS.AllocationLength[2] = (1024 >> 8) & 0xff;
    Cdb.REPORT_LUNS.AllocationLength[3] = 1024 & 0xff;

    Format =
        "u32 LUN LIST LENGTH (n-7)\n"
        "u32 Reserved\n"
        "*"
        "u64 LUN\n";

    return ScsiDataInAndPrint(argc, argv, &Cdb, 1024, Format);
}

static void usage(void)
{
    fatal(ERROR_INVALID_PARAMETER,
        "usage: %s COMMAND ARGS\n"
        "\n"
        "commands:\n"
        "    devpath device-name\n"
        "    inquiry device-name [b:t:l]\n"
        "    report-luns device-name [b:t:l]\n"
        "    vpd0 device-name [b:t:l]\n"
        "    vpd80 device-name [b:t:l]\n"
        "    vpd83 device-name [b:t:l]\n"
        "",
        PROGNAME);
}

int wmain(int argc, wchar_t **argv)
{
    argc--;
    argv++;

    if (0 == argc)
        usage();

    DWORD Error = ERROR_SUCCESS;

    if (0 == invariant_wcscmp(L"devpath", argv[0]))
        Error = devpath(argc, argv);
    else
    if (0 == invariant_wcscmp(L"inquiry", argv[0]))
        Error = inquiry(argc, argv);
    else
    if (0 == invariant_wcscmp(L"report-luns", argv[0]))
        Error = report_luns(argc, argv);
    else
    if (0 == invariant_wcscmp(L"vpd0", argv[0]))
        Error = inquiry_vpd0(argc, argv);
    else
    if (0 == invariant_wcscmp(L"vpd80", argv[0]))
        Error = inquiry_vpd80(argc, argv);
    else
    if (0 == invariant_wcscmp(L"vpd83", argv[0]))
        Error = inquiry_vpd83(argc, argv);
    else
        usage();

    if (ERROR_SUCCESS != Error)
    {
        char ErrorBuf[512];

        if (0 == FormatMessageA(
            FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_MAX_WIDTH_MASK,
            0, Error, 0, ErrorBuf, sizeof ErrorBuf, 0))
            ErrorBuf[0] = '\0';

        warn("Error %lu%s%s", Error, '\0' != ErrorBuf[0] ? ": " : "", ErrorBuf);
    }

    return Error;
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
