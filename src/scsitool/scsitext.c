/**
 * @file scsitool/scsitext.c
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

void ScsiLineTextFn(void *data,
    unsigned type, unsigned width, const char *name, size_t namelen,
    unsigned long long uval, void *pval, size_t lval,
    const char *warn)
{
    char buf[1024], *mem = 0, *bufp = buf;
    size_t size, warnlen;
    const char *sep = "";
    DWORD BytesTransferred;

    warnlen = 0 != warn ? sizeof " (WARN: )" - 1 + lstrlenA(warn) : 0;
    switch (type)
    {
    case 'u':
        size = namelen + 1 + 64 + warnlen;
        break;

    case 'A':
        size = namelen + 1 + lval + warnlen;
        break;

    case 'X':
        size = namelen + 1 + 3 * lval - 1 + warnlen;
        break;

    default:
        goto exit;
    }

    if (sizeof buf < size)
    {
        mem = MemAlloc(size);
        if (0 == mem)
            return;
        bufp = mem;
    }

    memcpy(bufp, name, namelen);
    bufp += namelen;

    *bufp++ = '=';

    switch (type)
    {
    case 'u':
        if (0xffffffffULL < uval)
        {
            bufp += wsprintfA(bufp, "%lx", (unsigned long)(uval >> 32));
            bufp += wsprintfA(bufp, "%08lxh", (unsigned long)uval);
        }
        else if (10 > uval)
            bufp += wsprintfA(bufp, "%lu", (unsigned long)uval);
        else
            bufp += wsprintfA(bufp, "%lu %lxh", (unsigned long)uval, (unsigned long)uval);
        break;

    case 'A':
        for (size_t i = 0; lval > i; i++)
        {
            char c = ((char *)pval)[i];
            *bufp++ = 0x20 <= c && c < 0x7f ? c : '.';
        }
        break;

    case 'X':
        for (size_t i = 0; lval > i; i++)
        {
            bufp += wsprintfA(bufp, "%s%02x", sep, ((unsigned char *)pval)[i]);
            sep = " ";
        }
        break;

    default:
        goto exit;
    }

    if (0 != warn)
        bufp += wsprintfA(bufp, " (WARN: %s)", warn);

    *bufp++ = '\n';

    WriteFile((HANDLE)data, buf, (DWORD)(bufp - buf), &BytesTransferred, 0);

exit:
    if (0 != mem)
        MemFree(mem);
}

void ScsiLineText(HANDLE h, const char *format, void *buf, size_t len)
{
    ScsiText(ScsiLineTextFn, h, format, buf, len);
}
