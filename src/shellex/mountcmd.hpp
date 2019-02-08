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
        if (Flag)
        {
            WCHAR GuidStr[40];
            REGRECORD Records[] =
            {
                { L"SOFTWARE\\Classes" },
                { L"" SPD_SHELLEX_MOUNT_PROGID },
                { L"shell" },
                { L"" SPD_SHELLEX_MOUNT_VERB },
                { 0, REG_SZ, L"" SPD_SHELLEX_MOUNT_VERB_DESC, sizeof L"" SPD_SHELLEX_MOUNT_VERB_DESC },
                { L"MultiSelectModel", REG_SZ, L"Document", sizeof L"Document" },
                { L"command" },
                { L"DelegateExecute", REG_SZ, GuidStr, (lstrlenW(GuidStr) + 1) * sizeof(WCHAR) },
            };
            StringFromGUID2(Clsid, GuidStr, sizeof GuidStr / sizeof GuidStr[0]);
            return HRESULT_FROM_WIN32(RegCreateTree(HKEY_LOCAL_MACHINE,
                Records, sizeof Records / sizeof Records[0]));
        }
        else
        {
            RegDeleteTree(HKEY_LOCAL_MACHINE,
                L"SOFTWARE\\Classes\\" SPD_SHELLEX_MOUNT_PROGID);
            return S_OK;
        }
    }
};

#endif