/**
 * @file scsitool/scsitext.c
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

void ScsiText(
    void (*fn)(void *data,
        unsigned type, unsigned width, const char *name, size_t namelen,
        unsigned long long uval, void *pval, size_t lval,
        const char *warn),
    void *data,
    const char *format, void *buf, size_t len)
{
    unsigned type, width, nminusv, mminusv, m;
    const char *star = 0;
    const char *name;
    const char *warn;
    unsigned long long uval;
    void *pval;
    size_t lval;
    size_t bitpos = 0;

    m = 1;
    for (const char *p = format; len * 8 > bitpos;)
    {
        nminusv = mminusv = -1;
        warn = 0;

        type = *p;
        if ('*' == type)
        {
            star = p++;
            type = *p;
        }
        if (0 == type)
        {
            if (0 != star)
            {
                p = star;
                continue;
            }

            break;
        }
        p++;

        if ('m' == *p)
        {
            width = m;
            p++;
        }
        else
            width = (unsigned)strtoint(p, 10, 0, &p);

        while (' ' == *p)
            p++;

        name = p;
        while (*p && '\n' != *p)
        {
            const char *q;
            unsigned v;

            if ('(' == p[0] && 'n' == p[1] && '-' == p[2] &&
                (v = (unsigned)strtoint(p + 3, 10, 0, &q), ')' == *q))
                nminusv = v;
            else
            if ('(' == p[0] && 'm' == p[1] && '-' == p[2] &&
                (v = (unsigned)strtoint(p + 3, 10, 0, &q), ')' == *q))
                mminusv = v;

            p++;
        }

        uval = 0;
        pval = 0; lval = 0;
        if ('A' <= type && type <= 'Z')
        {
            bitpos = (bitpos + 7) & ~7;
            pval = ((unsigned char *)buf) + bitpos / 8;
            lval = width;
            if (bitpos / 8 + lval > len)
                lval = (unsigned)(len - bitpos / 8);
            bitpos += lval * 8;

            if (0 != width)
                fn(data, type, width, name, p - name, uval, pval, lval, warn);
        }
        else if ('a' <= type && type <= 'z')
        {
            unsigned loopw = width;
            if (bitpos + loopw > len * 8)
                loopw = (unsigned)(len * 8 - bitpos);
            for (unsigned w; 0 < loopw; loopw -= w)
            {
                w = loopw;
                size_t endpos = (bitpos + 8) & ~7;
                if (bitpos + w > endpos)
                    w = (unsigned)(endpos - bitpos);
                endpos = bitpos + w;

                unsigned char u = ((unsigned char *)buf)[bitpos / 8];
                uval <<= w;
                uval |= ((u << (bitpos & 7)) & 0xff) >> (8 - w);

                bitpos = endpos;
            }

            if (-1 != nminusv)
            {
                size_t newlen = (size_t)(uval + nminusv + 1);
                warn = len != newlen ?
                    "data buffer length mismatch" : 0;
                if (len > newlen)
                    len = newlen;
            }
            else
            if (-1 != mminusv)
                m = (unsigned)uval;

            if (0 == invariant_strncmp("Reserved", name, p - name) && 0 != uval)
                warn = "non-zero reserved value";

            fn(data, type, width, name, p - name, uval, pval, lval, warn);
        }

        if ('\n' == *p)
            p++;
    }
}

struct ScsiTextPrinter
{
    HANDLE h;
    char buf[1024];
    size_t pos;
    size_t col;
};

static void ScsiTextPrint(struct ScsiTextPrinter *printer, const char *buf, size_t len)
{
    DWORD BytesTransferred;

    if (printer->pos + len > sizeof printer->buf)
    {
        memcpy(printer->buf + printer->pos, buf, sizeof printer->buf - printer->pos);
        WriteFile(printer->h, printer->buf, (DWORD)sizeof printer->buf, &BytesTransferred, 0);
        memcpy(printer->buf,
            buf + (sizeof printer->buf - printer->pos), len - (sizeof printer->buf - printer->pos));
        printer->pos = len - (sizeof printer->buf - printer->pos);
    }
    else
    {
        memcpy(printer->buf + printer->pos, buf, len);
        printer->pos += len;
    }
}

static void ScsiTextPrintc(struct ScsiTextPrinter *printer, char c)
{
    ScsiTextPrint(printer, &c, 1);
}

static void ScsiTextPrintf(struct ScsiTextPrinter *printer, const char *format, ...)
{
    va_list ap;
    char buf[1024];
    size_t len;

    va_start(ap, format);
    len = wvsprintfA(buf, format, ap);
    va_end(ap);
    ScsiTextPrint(printer, buf, len);
}

static void ScsiTextFlush(struct ScsiTextPrinter *printer)
{
    DWORD BytesTransferred;

    if (0 != printer->pos)
    {
        WriteFile(printer->h, printer->buf, (DWORD)printer->pos, &BytesTransferred, 0);
        printer->pos = 0;
    }
}

static void ScsiLineTextFn(void *data,
    unsigned type, unsigned width, const char *name, size_t namelen,
    unsigned long long uval, void *pval, size_t lval,
    const char *warn)
{
    struct ScsiTextPrinter *printer = data;
    char c, *sep = "";

    ScsiTextPrint(printer, name, namelen);
    ScsiTextPrintc(printer, '=');
    switch (type)
    {
    case 'u':
        if (0x100000000ULL <= uval)
            ScsiTextPrintf(printer, "%lx:%08lx", (unsigned long)(uval >> 32), (unsigned long)uval);
        else if (10 <= uval)
            ScsiTextPrintf(printer, "%lu (0x%lx)", (unsigned long)uval, (unsigned long)uval);
        else
            ScsiTextPrintf(printer, "%lu", (unsigned long)uval);
        break;

    case 'A':
        for (size_t i = 0; lval > i; i++)
        {
            c = ((char *)pval)[i];
            if (0x20 > c || c >= 0x7f)
                c = '.';
            ScsiTextPrintc(printer, c);
        }
        break;

    case 'X':
        for (size_t i = 0; lval > i; i++)
        {
            ScsiTextPrintf(printer, "%s%02x", sep, ((unsigned char *)pval)[i]);
            sep = " ";
        }
        break;
    }

    if (0 != warn)
        ScsiTextPrintf(printer, " (WARN: %s)", warn);

    ScsiTextPrintc(printer, '\n');
}

void ScsiLineText(HANDLE h, const char *format, void *buf, size_t len)
{
    struct ScsiTextPrinter printer;

    memset(&printer, 0, sizeof printer);
    printer.h = h;

    ScsiText(ScsiLineTextFn, &printer, format, buf, len);
    ScsiTextFlush(&printer);
}

static void ScsiTableTextCenter(struct ScsiTextPrinter *printer, unsigned colwidth)
{
    if (colwidth > printer->pos)
    {
        size_t spacewidth = colwidth - printer->pos;
        memmove(printer->buf + 1 + spacewidth / 2, printer->buf + 1, printer->pos - 1);
        memset(printer->buf + 1, ' ', spacewidth / 2);
        memset(printer->buf + spacewidth / 2 + printer->pos, ' ', spacewidth - spacewidth / 2);
        printer->pos = colwidth;
    }
}

static void ScsiTablePrintBoundary(struct ScsiTextPrinter *printer)
{
    static const char boundary[] =
        "+--------+--------+--------+--------+--------+--------+--------+--------+\n";

    ScsiTextPrint(printer, boundary, sizeof boundary - 1);
    printer->col = 0;
}

static void ScsiTableTextFn(void *data,
    unsigned type, unsigned width, const char *name, size_t namelen,
    unsigned long long uval, void *pval, size_t lval,
    const char *warn0)
{
    struct ScsiTextPrinter *printer = data;
    char c, *sep = "";
    unsigned colwidth;

    if ('u' == type && 0 != printer->col && 8 * 9 < printer->col + 9 * width)
    {
        char spaces[80];
        memset(spaces, ' ', 8 * 9 - printer->col - 1);
        ScsiTextPrintc(printer, '|');
        ScsiTextPrint(printer, spaces, 8 * 9 - printer->col - 1);
        ScsiTextPrint(printer, "|\n", 2);
        for (size_t i = 0; 8 > i; i++)
        {
            if (i * 9 < printer->col)
                ScsiTextPrint(printer, "+--------", 9);
            else if (i * 9 == printer->col)
                ScsiTextPrint(printer, "+        ", 9);
            else
                ScsiTextPrint(printer, "         ", 9);
        }
        ScsiTextPrint(printer, "|\n", 2);
        ScsiTextFlush(printer);
    }

    ScsiTextPrintc(printer, '|');
    ScsiTextPrint(printer, name, namelen);

    switch (type)
    {
    case 'u':
        ScsiTextPrintc(printer, '=');
        if (0x100000000ULL <= uval)
            ScsiTextPrintf(printer, "%lx:%08lx", (unsigned long)(uval >> 32), (unsigned long)uval);
        else if (10 <= uval)
            ScsiTextPrintf(printer, "%lu (0x%lx)", (unsigned long)uval, (unsigned long)uval);
        else
            ScsiTextPrintf(printer, "%lu", (unsigned long)uval);
        colwidth = 9 * (8 > width ? width : 8);
        ScsiTableTextCenter(printer, colwidth);
        if (colwidth < printer->pos)
        {
            for (char *p = printer->buf, *q, *endp = p + printer->pos; endp > p; p++)
                if ('=' == *p)
                {
                    q = p - (printer->pos - colwidth);
                    if (q < printer->buf)
                        q = printer->buf;
                    memmove(q, p, endp - p);
                    printer->pos = (q - printer->buf) + (endp - p);
                    break;
                }
        }
        printer->col += printer->pos;
        if (8 * 9 <= printer->col)
        {
            ScsiTextPrint(printer, "|\n", 2);
            ScsiTablePrintBoundary(printer);
        }
        break;

    case 'A':
        for (size_t i = 0; lval > i; i++)
        {
            if (0 == i % 64)
            {
                ScsiTableTextCenter(printer, 9 * 8);
                ScsiTextPrint(printer, "|\n", 2);
                ScsiTextFlush(printer);
                ScsiTextPrintc(printer, '|');
            }
            c = ((char *)pval)[i];
            if (0x20 > c || c >= 0x7f)
                c = '.';
            ScsiTextPrintc(printer, c);
        }
        ScsiTableTextCenter(printer, 9 * 8);
        ScsiTextPrint(printer, "|\n", 2);
        ScsiTablePrintBoundary(printer);
        break;

    case 'X':
        for (size_t i = 0; lval > i; i++)
        {
            if (0 == i % 16)
            {
                ScsiTableTextCenter(printer, 9 * 8);
                ScsiTextPrint(printer, "|\n", 2);
                ScsiTextFlush(printer);
                ScsiTextPrintc(printer, '|');
            }
            ScsiTextPrintf(printer, "%s%02x", sep, ((unsigned char *)pval)[i]);
            sep = " ";
        }
        ScsiTableTextCenter(printer, 9 * 8);
        ScsiTextPrint(printer, "|\n", 2);
        ScsiTablePrintBoundary(printer);
        break;
    }

    ScsiTextFlush(printer);
}

void ScsiTableText(HANDLE h, const char *format, void *buf, size_t len)
{
    struct ScsiTextPrinter printer;

    memset(&printer, 0, sizeof printer);
    printer.h = h;

    ScsiTablePrintBoundary(&printer);
    ScsiTextFlush(&printer);
    ScsiText(ScsiTableTextFn, &printer, format, buf, len);
}
