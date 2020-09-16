/**
 * @file shared/strtoint.c
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

long long strtoint(const char *p, int base, int is_signed, const char **endp)
{
    long long v;
    int maxdig, maxalp, sign = +1;

    if (is_signed)
    {
        if ('+' == *p)
            p++;
        else if ('-' == *p)
            p++, sign = -1;
    }

    if (0 == base)
    {
        if ('0' == *p)
        {
            p++;
            if ('x' == *p || 'X' == *p)
            {
                p++;
                base = 16;
            }
            else
                base = 8;
        }
        else
        {
            base = 10;
        }
    }

    maxdig = 10 < base ? '9' : (base - 1) + '0';
    maxalp = 10 < base ? (base - 1 - 10) + 'a' : 0;

    for (v = 0; *p; p++)
    {
        int c = *p;

        if ('0' <= c && c <= maxdig)
            v = base * v + (c - '0');
        else
        {
            c |= 0x20;
            if ('a' <= c && c <= maxalp)
                v = base * v + (c - 'a') + 10;
            else
                break;
        }
    }

    *endp = p;

    return sign * v;
}

long long wcstoint(const wchar_t *p, int base, int is_signed, const wchar_t **endp)
{
    long long v;
    int maxdig, maxalp, sign = +1;

    if (is_signed)
    {
        if ('+' == *p)
            p++;
        else if ('-' == *p)
            p++, sign = -1;
    }

    if (0 == base)
    {
        if ('0' == *p)
        {
            p++;
            if ('x' == *p || 'X' == *p)
            {
                p++;
                base = 16;
            }
            else
                base = 8;
        }
        else
        {
            base = 10;
        }
    }

    maxdig = 10 < base ? '9' : (base - 1) + '0';
    maxalp = 10 < base ? (base - 1 - 10) + 'a' : 0;

    for (v = 0; *p; p++)
    {
        int c = *p;

        if ('0' <= c && c <= maxdig)
            v = base * v + (c - '0');
        else
        {
            c |= 0x20;
            if ('a' <= c && c <= maxalp)
                v = base * v + (c - 'a') + 10;
            else
                break;
        }
    }

    *endp = p;

    return sign * v;
}
