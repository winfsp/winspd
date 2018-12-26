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

static void usage(void)
{
    warn(
        "usage: %s PIPENAME BTL\n",
        PROGNAME);
    warn(
        "message syntax (RFC 5234 ABNF):\n");
    warn(
        "    request     = read-req / write-req / flush-req / unmap-req\n"
        "    read-req    = R SP hint SP address SP count SP fua NL\n"
        "    write-req   = W SP hint SP address SP count SP fua SP datalen NL databuf\n"
        "    flush-req   = F SP hint SP address SP count NL\n"
        "    unmap-req   = U SP hint SP count NL *(address SP count NL)\n");
    warn(
        "    response    = read-rsp / write-rsp / flush-rsp / unmap-rsp\n"
        "    read-rsp    = (R SP hint SP status-ok SP datalen NL databuf) /\n"
        "                  (R SP hint SP status-ko NL)\n"
        "    write-rsp   = W SP hint SP status NL\n"
        "    flush-rsp   = F SP hint SP status NL\n"
        "    unmap-rsp   = U SP hint SP status NL\n");
    warn(
        "    status      = status-ok / status-ko\n"
        "    status-ok   = OK\n"
        "    status-ko   = KO SP SenseKey SP ASC SP ASCQ [SP Information]\n"
        "\n"
        "    R           = %%x52          ; Read\n"
        "    W           = %%x57          ; Write\n"
        "    F           = %%x46          ; Flush\n"
        "    U           = %%x55          ; Unmap\n"
        "\n"
        "    hint        = 1*DIGIT       ; request/response id\n"
        "    address     = 1*DIGIT       ; block address (LBA)\n"
        "    count       = 1*DIGIT       ; block count\n"
        "    fua         = \"0\" / \"1\"     ; force unit access\n"
        "    datalen     = 1*DIGIT       ; data buffer length\n"
        "    databuf     = *%%x00-ff      ; data buffer\n"
        "\n"
        "    OK          = \"0\"\n"
        "    KO          = \"2\"\n"
        "    SenseKey    = 1*DIGIT\n"
        "    ASC         = 1*DIGIT\n"
        "    ASCQ        = 1*DIGIT\n"
        "    Information = 1*DIGIT\n");

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
