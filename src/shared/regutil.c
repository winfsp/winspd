/**
 * @file shared/regutil.c
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

DWORD RegAddEntries(HKEY Key, REGENTRY *Entries, ULONG Count, PBOOLEAN PKeyAdded)
{
    REGENTRY *Entry;
    HKEY EntryKey = 0, TempKey;
    DWORD Disposition, Error;

    if (0 != PKeyAdded)
        *PKeyAdded = FALSE;

    for (ULONG I = 0; Count > I; I++)
    {
        Entry = Entries + I;
        if (0 == Entry->Name && 0 == Entry->Value)
        {
            if (0 != EntryKey)
                RegCloseKey(EntryKey);
            EntryKey = 0;
        }
        else if (0 == Entry->Value)
        {
            if (0 == Entry->Type)
                Error = RegOpenKeyExW(0 != EntryKey ? EntryKey : Key,
                    Entry->Name, 0, KEY_ALL_ACCESS, &TempKey);
            else
            {
                Error = RegCreateKeyExW(0 != EntryKey ? EntryKey : Key,
                    Entry->Name, 0, 0, 0, KEY_ALL_ACCESS, 0, &TempKey, &Disposition);
                if (ERROR_SUCCESS == Error && REG_CREATED_NEW_KEY == Disposition)
                {
                    if (0 != PKeyAdded)
                        *PKeyAdded = TRUE;
                }
            }
            if (ERROR_SUCCESS != Error)
                goto exit;
            if (0 != EntryKey)
                RegCloseKey(EntryKey);
            EntryKey = TempKey;
        }
        else
        {
            Error = RegSetValueExW(0 != EntryKey ? EntryKey : Key,
                Entry->Name, 0, Entry->Type, Entry->Value, Entry->Size);
            if (ERROR_SUCCESS != Error)
                goto exit;
        }
    }

    Error = ERROR_SUCCESS;

exit:
    if (0 != EntryKey)
        RegCloseKey(EntryKey);

    return Error;
}

DWORD RegDeleteEntries(HKEY Key, REGENTRY *Entries, ULONG Count, PBOOLEAN PKeyDeleted)
{
    REGENTRY *Entry;
    HKEY EntryKey = 0, TempKey;
    BOOLEAN Skip = FALSE;
    DWORD Error;

    if (0 != PKeyDeleted)
        *PKeyDeleted = FALSE;

    for (ULONG I = 0; Count > I; I++)
    {
        Entry = Entries + I;
        if (0 == Entry->Name && 0 == Entry->Value)
        {
            if (0 != EntryKey)
                RegCloseKey(EntryKey);
            EntryKey = 0;
            Skip = FALSE;
        }
        else if (0 == Entry->Value)
        {
            if (Skip)
                continue;
            if (0 == Entry->Type)
                Error = RegOpenKeyExW(0 != EntryKey ? EntryKey : Key,
                    Entry->Name, 0, KEY_ALL_ACCESS, &TempKey);
            else
            {
                Error = RegDeleteTree(0 != EntryKey ? EntryKey : Key, Entry->Name);
                if (ERROR_SUCCESS == Error)
                {
                    if (0 != PKeyDeleted)
                        *PKeyDeleted = TRUE;
                }
                else if (ERROR_FILE_NOT_FOUND == Error)
                    Error = ERROR_SUCCESS;
                TempKey = 0;
                Skip = TRUE;
            }
            if (ERROR_SUCCESS != Error)
                goto exit;
            if (0 != EntryKey)
                RegCloseKey(EntryKey);
            EntryKey = TempKey;
        }
        else
        {
            if (Skip)
                continue;
            Error = RegDeleteValue(0 != EntryKey ? EntryKey : Key, Entry->Name);
            if (ERROR_FILE_NOT_FOUND == Error)
                Error = ERROR_SUCCESS;
            if (ERROR_SUCCESS != Error)
                goto exit;
        }
    }

    Error = ERROR_SUCCESS;

exit:
    if (0 != EntryKey)
        RegCloseKey(EntryKey);

    return Error;
}
