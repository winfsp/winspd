/**
 * @file shellex/mountcmd.hpp
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

#ifndef WINSPD_SHELLEX_MOUNTCMD_HPP_INCLUDED
#define WINSPD_SHELLEX_MOUNTCMD_HPP_INCLUDED

#include <shellex/command.hpp>
#include <shared/launch.h>
#include <shared/regutil.h>

#define SPD_SHELLEX_MOUNT_PROGID        "WinSpd.Mount"
#define SPD_SHELLEX_MOUNT_VERB          "open"
#define SPD_SHELLEX_MOUNT_VERB_DESC     "Mount"

class MountCommand : public Command
{
public:
    // {62AD20C1-8F38-452A-AB7A-F281004D0283}
    static constexpr CLSID Clsid =
        { 0x62ad20c1, 0x8f38, 0x452a, { 0xab, 0x7a, 0xf2, 0x81, 0x0, 0x4d, 0x2, 0x83 } };
    static constexpr PWSTR ThreadingModel = L"Apartment";
    static STDMETHODIMP Register(BOOL Flag)
    {
        WCHAR GuidStr[40];
        StringFromGUID2(Clsid, GuidStr, sizeof GuidStr / sizeof GuidStr[0]);
        REGENTRY Entries[] =
        {
            { L"SOFTWARE\\Classes" },
            { L"" SPD_SHELLEX_MOUNT_PROGID, 1 },
            { L"shell", 1 },
            { L"" SPD_SHELLEX_MOUNT_VERB, 1 },
            { 0, REG_SZ, L"" SPD_SHELLEX_MOUNT_VERB_DESC, sizeof L"" SPD_SHELLEX_MOUNT_VERB_DESC },
            { L"MultiSelectModel", REG_SZ, L"Document", sizeof L"Document" },
            { L"command", 1 },
            { L"DelegateExecute", REG_SZ, GuidStr, (lstrlenW(GuidStr) + 1) * sizeof(WCHAR) },
        };
        if (Flag)
            return HRESULT_FROM_WIN32(RegAddEntries(HKEY_LOCAL_MACHINE,
                Entries, sizeof Entries / sizeof Entries[0], 0));
        else
            return HRESULT_FROM_WIN32(RegDeleteEntries(HKEY_LOCAL_MACHINE,
                Entries, sizeof Entries / sizeof Entries[0], 0));
    }

    /* IExecuteCommand */
    STDMETHODIMP Execute()
    {
        IEnumShellItems *Enum;
        IShellItem *Item;
        PWSTR Name;
        HRESULT Result = S_OK;
        if (0 != _Array && S_OK == _Array->EnumItems(&Enum))
        {
            while (S_OK == Enum->Next(1, &Item, 0))
            {
                if (S_OK == Item->GetDisplayName(SIGDN_FILESYSPATH, &Name))
                {
                    Result = Mount(Name);
                    CoTaskMemFree(Name);
                }
                Item->Release();
            }
            Enum->Release();
        }
        return Result;
    }

private:
    DWORD GetFinalPathName(PWSTR Name, PWCHAR Buf, ULONG Size)
    {
        HANDLE Handle = INVALID_HANDLE_VALUE;
        DWORD Error;

        Handle = CreateFileW(Name, 0, 0, 0, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, 0);
        if (INVALID_HANDLE_VALUE == Handle)
            goto exit;

        Size = sizeof(WCHAR) <= Size ? (Size - sizeof(WCHAR)) / sizeof(WCHAR) : 0;
        Error = GetFinalPathNameByHandle(Handle, Buf, Size,
            FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
        if (0 == Error || Size < Error)
        {
            Error = GetLastError();
            goto exit;
        }

        Error = ERROR_SUCCESS;

    exit:
        if (INVALID_HANDLE_VALUE != Handle)
            CloseHandle(Handle);

        return Error;
    }
    HRESULT Mount(PWSTR Name)
    {
        PWSTR ClassName = 0;
        WCHAR FinalPathName[4/*\\?\*/ + MAX_PATH];
        DWORD Error, LauncherError;

        Error = GetFinalPathName(Name, FinalPathName, sizeof FinalPathName);
        if (ERROR_SUCCESS != Error)
            goto exit;
        Name = FinalPathName;

        for (PWSTR P = Name; L'\0' != *P; P++)
            if (L'.' == *P)
                ClassName = P + 1;
        if (0 == ClassName || L'\0' == ClassName)
        {
            Error = ERROR_NO_ASSOCIATION;
            goto exit;
        }

        Error = SpdLaunchStart(ClassName, Name, 1, &Name, &LauncherError);
        if (ERROR_SUCCESS != Error || ERROR_FILE_NOT_FOUND == LauncherError)
        {
            Error = ERROR_NO_ASSOCIATION;
            goto exit;
        }

        Error = LauncherError;

    exit:
        return HRESULT_FROM_WIN32(Error);
    }
};

#endif
