/**
 * @file shared/regutil.c
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

#include <winspd/winspd.h>
#include <shared/minimal.h>
#include <shared/regutil.h>

DWORD RegCreateTree(HKEY Key, REGRECORD *Records, ULONG Count)
{
    REGRECORD *Record;
    HKEY RecordKey = 0, TempKey;
    DWORD Error;

    for (ULONG I = 0; Count > I; I++)
    {
        Record = Records + I;
        if (0 == Record->Name && 0 == Record->Value)
        {
            if (0 != RecordKey)
                RegCloseKey(RecordKey);
            RecordKey = 0;
        }
        else if (0 == Record->Value)
        {
            Error = RegCreateKeyExW(0 != RecordKey ? RecordKey : Key,
                Record->Name, 0, 0, 0, KEY_ALL_ACCESS, 0, &TempKey, 0);
            if (ERROR_SUCCESS != Error)
                goto exit;
            if (0 != RecordKey)
                RegCloseKey(RecordKey);
            RecordKey = TempKey;
        }
        else
        {
            Error = RegSetValueExW(0 != RecordKey ? RecordKey : Key,
                Record->Name, 0, Record->Type, Record->Value, Record->Size);
            if (ERROR_SUCCESS != Error)
                goto exit;
        }
    }

    Error = ERROR_SUCCESS;

exit:
    if (0 != RecordKey)
        RegCloseKey(RecordKey);

    return Error;
}
