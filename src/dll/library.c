/**
 * @file dll/library.c
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

VOID SpdStorageUnitFinalize(BOOLEAN Dynamic);

HINSTANCE DllInstance;

BOOL WINAPI DllMain(HINSTANCE Instance, DWORD Reason, PVOID Reserved)
{
    BOOLEAN Dynamic;

    switch (Reason)
    {
    case DLL_PROCESS_ATTACH:
        DllInstance = Instance;
        break;

    case DLL_PROCESS_DETACH:
        Dynamic = 0 == Reserved;
        SpdStorageUnitFinalize(Dynamic);
        break;
    }

    return TRUE;
}

BOOL WINAPI _DllMainCRTStartup(HINSTANCE Instance, DWORD Reason, PVOID Reserved)
{
    return DllMain(Instance, Reason, Reserved);
}
