/**
 * @file scsitool/scsitext.c
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

static unsigned scanint(const char *p, const char **endp)
{
    unsigned v;

    for (v = 0; *p; p++)
    {
        unsigned c = *p;

        if ('0' <= c && c <= '9')
            v = 10 * v + (c - '0');
        else
            break;
    }

    *endp = p;

    return v;
}

void ScsiText(
    void (*fn)(void *data,
        unsigned type, unsigned width, const char *name, size_t namelen,
        unsigned long long uval, void *pval, size_t lval),
    void *data,
    const char *format, void *buf, size_t len)
{
    unsigned type, width;
    const char *name;
    unsigned long long uval;
    void *pval;
    size_t lval;
    size_t bitpos = 0;

    for (const char *p = format; len * 8 > bitpos;)
    {
        type = *p;
        if (0 == type)
            break;
        p++;

        width = scanint(p, &p);

        while (' ' == *p)
            p++;

        name = p;
        while (*p && '\n' != *p)
            p++;

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

            fn(data, type, width, name, p - name, uval, pval, lval);
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

            fn(data, type, width, name, p - name, uval, pval, lval);
        }

        if ('\n' == *p)
            p++;
    }
}

void ScsiLineTextFn(void *data,
    unsigned type, unsigned width, const char *name, size_t namelen,
    unsigned long long uval, void *pval, size_t lval)
{
    char buf[1024], *mem = 0, *bufp = buf;
    size_t size;
    const char *sep = "";
    DWORD BytesTransferred;

    switch (type)
    {
    case 'u':
        size = namelen + 1 + 64;
        break;

    case 'S':
        size = namelen + 1 + lval;
        break;

    case 'X':
        size = namelen + 1 + 3 * lval - 1;
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
            bufp += wsprintfA(bufp, "%08lx", (unsigned long)uval);
        }
        else
            bufp += wsprintfA(bufp, "%lx", (unsigned long)uval);
        break;

    case 'S':
        memcpy(bufp, pval, lval);
        bufp += lval;
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
