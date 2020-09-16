/**
 * @file scsitool/scsitool.c
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

#include <scsitool/scsitool.h>

static void usage(void);

static int ScsiPrintCanonical = 0;
static void ScsiPrint(const char *format, void *buf, size_t len)
{
    if (ScsiPrintCanonical)
        ScsiLineText(GetStdHandle(STD_OUTPUT_HANDLE), format, buf, len);
    else
        ScsiTableText(GetStdHandle(STD_OUTPUT_HANDLE), format, buf, len);
}

static void ScsiPrintSenseInfo(UCHAR ScsiStatus, UCHAR SenseInfoBuffer[32])
{
    info(L"ScsiStatus=%u", ScsiStatus);

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
    UINT32 Btl = 0;
    PVOID DataBuffer = 0;
    UCHAR ScsiStatus;
    UCHAR SenseInfoBuffer[32];
    DWORD Error;

    if (3 == argc)
    {
        const wchar_t *p = argv[2];

        Btl |= (UCHAR)wcstoint(p, 10, 0, &p);
        if (':' == *p++)
        {
            Btl <<= 8;
            Btl |= (UCHAR)wcstoint(p, 10, 0, &p);
            if (':' == *p++)
            {
                Btl <<= 8;
                Btl |= (UCHAR)wcstoint(p, 10, 0, &p);
            }
        }
    }

    Error = SpdIoctlOpenDevice(argv[1], &DeviceHandle);
    if (ERROR_SUCCESS != Error)
        goto exit;

    Error = SpdIoctlMemAlignAlloc(DataLength, 511, &DataBuffer);
    if (ERROR_SUCCESS != Error)
        goto exit;

    Error = SpdIoctlScsiExecute(DeviceHandle, Btl, Cdb, +1,
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

    info(L"%s", PathBuf);

exit:
    return Error;
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
        "A8  T10 VENDOR IDENTIFICATION\n"
        "A16 PRODUCT IDENTIFICATION\n"
        "A4  PRODUCT REVISION LEVEL\n"
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
        "A255 PRODUCT SERIAL NUMBER\n";

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
        "*"
        "u4  PROTOCOL IDENTIFIER\n"
        "u4  CODE SET\n"
        "u1  PIV\n"
        "u1  Reserved\n"
        "u2  ASSOCIATION\n"
        "u4  IDENTIFIER TYPE\n"
        "u8  Reserved\n"
        "u8  IDENTIFIER LENGTH (m-3)\n"
        "Am  IDENTIFIER\n";

    return ScsiDataInAndPrint(argc, argv, &Cdb, VPD_MAX_BUFFER_SIZE, Format);
}

static int inquiry_vpdb0(int argc, wchar_t **argv)
{
    CDB Cdb;
    const char *Format;

    memset(&Cdb, 0, sizeof Cdb);
    Cdb.CDB6INQUIRY3.OperationCode = SCSIOP_INQUIRY;
    Cdb.CDB6INQUIRY3.EnableVitalProductData = 1;
    Cdb.CDB6INQUIRY3.PageCode = 0xb0;
    Cdb.CDB6INQUIRY3.AllocationLength = VPD_MAX_BUFFER_SIZE;

    Format =
        "u3  PERIPHERAL QUALIFIER\n"
        "u5  PERIPHERAL DEVICE TYPE\n"
        "u8  PAGE CODE (B0h)\n"
        "u16 PAGE LENGTH (003Ch)\n"
        "u8  Reserved\n"
        "u8  MAXIMUM COMPARE AND WRITE LENGTH\n"
        "u16 OPTIMAL TRANSFER LENGTH GRANULARITY\n"
        "u32 MAXIMUM TRANSFER LENGTH\n"
        "u32 OPTIMAL TRANSFER LENGTH\n"
        "u32 MAXIMUM PREFETCH XDREAD XDWRITE TRANSFER LENGTH\n"
        "u32 MAXIMUM UNMAP LBA COUNT\n"
        "u32 MAXIMUM UNMAP BLOCK DESCRIPTOR COUNT\n"
        "u32 OPTIMAL UNMAP GRANULARITY\n"
        "u1  UGAVALID\n"
        "u31 UNMAP GRANULARITY ALIGNMENT\n"
        "X28 Reserved\n";

    return ScsiDataInAndPrint(argc, argv, &Cdb, VPD_MAX_BUFFER_SIZE, Format);
}

static int inquiry_vpdb2(int argc, wchar_t **argv)
{
    CDB Cdb;
    const char *Format;

    memset(&Cdb, 0, sizeof Cdb);
    Cdb.CDB6INQUIRY3.OperationCode = SCSIOP_INQUIRY;
    Cdb.CDB6INQUIRY3.EnableVitalProductData = 1;
    Cdb.CDB6INQUIRY3.PageCode = 0xb2;
    Cdb.CDB6INQUIRY3.AllocationLength = VPD_MAX_BUFFER_SIZE;

    Format =
        "u3  PERIPHERAL QUALIFIER\n"
        "u5  PERIPHERAL DEVICE TYPE\n"
        "u8  PAGE CODE (B2h)\n"
        "u16 PAGE LENGTH (n-3)\n"
        "u8  THRESHOLD EXPONENT\n"
        "u1  LBPU\n"
        "u1  LBPWS\n"
        "u4  Reserved\n"
        "u1  ANC_SUP\n"
        "u1  DP\n"
        "u5  Reserved\n"
        "u3  PROVISIONING TYPE\n"
        "u8  Reserved\n"
        "X255 PROVISIONING GROUP DESCRIPTOR\n";

    return ScsiDataInAndPrint(argc, argv, &Cdb, VPD_MAX_BUFFER_SIZE, Format);
}

static int mode_sense(int argc, wchar_t **argv)
{
    CDB Cdb;
    const char *Format;

    memset(&Cdb, 0, sizeof Cdb);
    Cdb.MODE_SENSE.OperationCode = SCSIOP_MODE_SENSE;
    Cdb.MODE_SENSE.Pc = MODE_SENSE_CURRENT_VALUES;
    Cdb.MODE_SENSE.PageCode = MODE_SENSE_RETURN_ALL;
    Cdb.MODE_SENSE.AllocationLength = 255;

    Format =
        "u8  MODE DATA LENGTH (n-0)\n"
        "u8  MEDIUM TYPE\n"
        "u1  WP\n"
        "u2  Reserved\n"
        "u1  DPOFUA\n"
        "u4  Reserved\n"
        "u8  BLOCK DESCRIPTOR LENGTH (m-0)\n"
        "Xm  BLOCK DESCRIPTORS\n"
        "X255 MODE PAGES\n";

    return ScsiDataInAndPrint(argc, argv, &Cdb, 255, Format);
}

static int mode_caching(int argc, wchar_t **argv)
{
    CDB Cdb;
    const char *Format;

    memset(&Cdb, 0, sizeof Cdb);
    Cdb.MODE_SENSE.OperationCode = SCSIOP_MODE_SENSE;
    Cdb.MODE_SENSE.Pc = MODE_SENSE_CURRENT_VALUES;
    Cdb.MODE_SENSE.PageCode = MODE_PAGE_CACHING;
    Cdb.MODE_SENSE.AllocationLength = 255;

    Format =
        "u8  MODE DATA LENGTH (n-0)\n"
        "u8  MEDIUM TYPE\n"
        "u1  WP\n"
        "u2  Reserved\n"
        "u1  DPOFUA\n"
        "u4  Reserved\n"
        "u8  BLOCK DESCRIPTOR LENGTH (m-0)\n"
        "Xm  BLOCK DESCRIPTORS\n"
        "u1  PS\n"
        "u1  SPF\n"
        "u6  PAGE CODE (08h)\n"
        "u8  PAGE LENGTH (12h)\n"
        "u1  IC\n"
        "u1  ABPF\n"
        "u1  CAP\n"
        "u1  DISC\n"
        "u1  SIZE\n"
        "u1  WCE\n"
        "u1  MF\n"
        "u1  RCD\n"
        "u4  DEMAND READ RETENTION PRIORITY\n"
        "u4  WRITE RETENTION PRIORITY\n"
        "u8  DISABLE PRE-FETCH TRANSFER LENGTH\n"
        "u8  MINIMUM PRE-FETCH\n"
        "u8  MAXIMUM PRE-FETCH\n"
        "u8  MAXIMUM PRE-FETCH CEILING\n"
        "u1  FSW\n"
        "u1  LBCSS\n"
        "u1  DRA\n"
        "u2  vendor specific\n"
        "u2  Reserved\n"
        "u1  NV_DIS\n"
        "u8  NUMBER OF CACHE SEGMENTS\n"
        "u16 CACHE SEGMENT SIZE\n"
        "u8  Reserved\n"
        "u24 Obsolete\n";

    return ScsiDataInAndPrint(argc, argv, &Cdb, 255, Format);
}

static int capacity(int argc, wchar_t **argv)
{
    CDB Cdb;
    const char *Format;

    memset(&Cdb, 0, sizeof Cdb);
    Cdb.CDB10.OperationCode = SCSIOP_READ_CAPACITY;

    Format =
        "u32 RETURNED LOGICAL BLOCK ADDRESS\n"
        "u32 LOGICAL BLOCK LENGTH IN BYTES\n";

    return ScsiDataInAndPrint(argc, argv, &Cdb, 255, Format);
}

static int capacity16(int argc, wchar_t **argv)
{
    CDB Cdb;
    const char *Format;

    memset(&Cdb, 0, sizeof Cdb);
    Cdb.READ_CAPACITY16.OperationCode = SCSIOP_SERVICE_ACTION_IN16;
    Cdb.READ_CAPACITY16.ServiceAction = SERVICE_ACTION_READ_CAPACITY16;
    Cdb.READ_CAPACITY16.AllocationLength[3] = 255;

    Format =
        "u64 RETURNED LOGICAL BLOCK ADDRESS\n"
        "u32 LOGICAL BLOCK LENGTH IN BYTES\n"
        "u4  Reserved\n"
        "u3  P_TYPE\n"
        "u1  PROT_EN\n"
        "u4  P_I_EXPONENT\n"
        "u4  LOGICAL BLOCKS PER PHYSICAL BLOCK EXPONENT\n"
        "u1  TPE\n"
        "u1  TPRZ\n"
        "u14 LOWEST ALIGNED LOGICAL BLOCK ADDRESS\n"
        "X16 Reserved\n";

    return ScsiDataInAndPrint(argc, argv, &Cdb, 255, Format);
}

static void usage(void)
{
    fail(ERROR_INVALID_PARAMETER, L""
        "usage: %s COMMAND ARGS\n"
        "\n"
        "commands:\n"
        "    devpath device-name\n"
        "    luns device-name [b:t:l]\n"
        "    inquiry device-name [b:t:l]\n"
        "    vpd0 device-name [b:t:l]\n"
        "    vpd80 device-name [b:t:l]\n"
        "    vpd83 device-name [b:t:l]\n"
        "    vpdb0 device-name [b:t:l]\n"
        "    vpdb2 device-name [b:t:l]\n"
        "    mode-sense device-name [b:t:l]\n"
        "    mode-caching device-name [b:t:l]\n"
        "    capacity device-name [b:t:l]\n"
        "    capacity16 device-name [b:t:l]\n",
        L"" PROGNAME);
}

int wmain(int argc, wchar_t **argv)
{
    argc--;
    argv++;

    if (0 != argc && 0 == invariant_wcscmp(L"-c", argv[0]))
    {
        ScsiPrintCanonical = 1;
        argc--;
        argv++;
    }

    if (0 == argc)
        usage();

    DWORD Error = ERROR_SUCCESS;

    if (0 == invariant_wcscmp(L"devpath", argv[0]))
        Error = devpath(argc, argv);
    else
    if (0 == invariant_wcscmp(L"luns", argv[0]))
        Error = report_luns(argc, argv);
    else
    if (0 == invariant_wcscmp(L"inquiry", argv[0]))
        Error = inquiry(argc, argv);
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
    if (0 == invariant_wcscmp(L"vpdb0", argv[0]))
        Error = inquiry_vpdb0(argc, argv);
    else
    if (0 == invariant_wcscmp(L"vpdb2", argv[0]))
        Error = inquiry_vpdb2(argc, argv);
    else
    if (0 == invariant_wcscmp(L"mode-sense", argv[0]))
        Error = mode_sense(argc, argv);
    else
    if (0 == invariant_wcscmp(L"mode-caching", argv[0]))
        Error = mode_caching(argc, argv);
    else
    if (0 == invariant_wcscmp(L"capacity", argv[0]))
        Error = capacity(argc, argv);
    else
    if (0 == invariant_wcscmp(L"capacity16", argv[0]))
        Error = capacity16(argc, argv);
    else
        usage();

    if (ERROR_SUCCESS != Error)
    {
        WCHAR ErrorBuf[512];

        if (0 == FormatMessageW(
            FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_MAX_WIDTH_MASK,
            0, Error, 0, ErrorBuf, sizeof ErrorBuf / sizeof ErrorBuf[0], 0))
            ErrorBuf[0] = '\0';

        warn(L"Error %lu%s%s", Error, '\0' != ErrorBuf[0] ? L": " : L"", ErrorBuf);
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
