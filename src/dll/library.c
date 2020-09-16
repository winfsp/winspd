/**
 * @file dll/library.c
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

DWORD SpdVersion(PUINT32 PVersion)
{
    static UINT32 Version;

    if (0 == Version)
    {
        /*
         * This check is not thread-safe, but that should be ok.
         * Two threads competing to read the version will read
         * the same value from the Version resource.
         */
        *PVersion = 0;

        HMODULE Module;
        WCHAR ModuleFileName[MAX_PATH];
        PVOID VersionInfo;
        DWORD Size;
        VS_FIXEDFILEINFO *FixedFileInfo = 0;
        if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, (PWSTR)SpdVersion, &Module) &&
            0 != GetModuleFileNameW(Module, ModuleFileName, MAX_PATH))
        {
            Size = GetFileVersionInfoSizeW(ModuleFileName, &Size/*dummy*/);
            if (0 < Size)
            {
                VersionInfo = MemAlloc(Size);
                if (0 != VersionInfo &&
                    GetFileVersionInfoW(ModuleFileName, 0, Size, VersionInfo) &&
                    VerQueryValueW(VersionInfo, L"\\", &FixedFileInfo, &Size))
                {
                    /* 32-bit store should be atomic! */
                    Version = FixedFileInfo->dwFileVersionMS;
                }

                MemFree(VersionInfo);
            }
        }

        if (0 == FixedFileInfo)
            return ERROR_GEN_FAILURE;
    }

    *PVersion = Version;

    return ERROR_SUCCESS;
}

BOOL WINAPI _DllMainCRTStartup(HINSTANCE Instance, DWORD Reason, PVOID Reserved)
{
    return TRUE;
}
