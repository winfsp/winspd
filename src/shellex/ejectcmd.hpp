/**
 * @file shellex/ejectcmd.hpp
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

#ifndef WINSPD_SHELLEX_EJECTCMD_HPP_INCLUDED
#define WINSPD_SHELLEX_EJECTCMD_HPP_INCLUDED

#include <shellex/command.hpp>
#define _NTSCSI_USER_MODE_
#include <scsi.h>
#undef _NTSCSI_USER_MODE_
#include <shared/launch.h>
#include <winspd/ioctl.h>

#define SPD_SHELLEX_EJECT_PROGID        "Drive"
#define SPD_SHELLEX_EJECT_VERB          "WinSpd.Eject"
#define SPD_SHELLEX_EJECT_VERB_DESC     "Eject"

class EjectCommand : public Command
{
public:
    // {18F72BBE-CE30-4EBA-8691-911D158A883C}
    static constexpr CLSID Clsid =
        { 0x18f72bbe, 0xce30, 0x4eba, { 0x86, 0x91, 0x91, 0x1d, 0x15, 0x8a, 0x88, 0x3c } };
    static constexpr PWSTR ThreadingModel = L"Apartment";
    static STDMETHODIMP Register(BOOL Flag)
    {
        WCHAR GuidStr[40];
        StringFromGUID2(Clsid, GuidStr, sizeof GuidStr / sizeof GuidStr[0]);
        REGENTRY Entries[] =
        {
            { L"SOFTWARE\\Classes" },
            { L"" SPD_SHELLEX_EJECT_PROGID },
            { L"shell" },
            { L"" SPD_SHELLEX_EJECT_VERB, 1 },
            { 0, REG_SZ, L"" SPD_SHELLEX_EJECT_VERB_DESC, sizeof L"" SPD_SHELLEX_EJECT_VERB_DESC },
            { L"CommandStateHandler", REG_SZ, GuidStr, (lstrlenW(GuidStr) + 1) * sizeof(WCHAR) },
            { L"MultiSelectModel", REG_SZ, L"Single", sizeof L"Single" },
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
                    Result = Eject(Name);
                    CoTaskMemFree(Name);
                }
                Item->Release();
            }
            Enum->Release();
        }
        return Result;
    }

    /* IExplorerCommandState */
    STDMETHODIMP GetState(IShellItemArray *Array, BOOL OkToBeSlow, EXPCMDSTATE *CmdState)
    {
        *CmdState = ECS_HIDDEN;
        IEnumShellItems *Enum;
        IShellItem *Item;
        PWSTR Name;
        HRESULT Result = S_OK;
        if (0 != Array && S_OK == Array->EnumItems(&Enum))
        {
            while (S_OK == Enum->Next(1, &Item, 0))
            {
                if (S_OK == Item->GetDisplayName(SIGDN_FILESYSPATH, &Name))
                {
                    *CmdState = CanEject(Name) ? ECS_ENABLED : ECS_HIDDEN;
                    CoTaskMemFree(Name);
                }
                Item->Release();
            }
            Enum->Release();
        }
        return Result;
    }

private:
    static DWORD GetVolume(PWSTR Name, PWSTR VolumeBuf, ULONG VolumeBufSize)
    {
        if (4 > VolumeBufSize / sizeof(WCHAR))
            return ERROR_INVALID_PARAMETER;

        if (L'\0' == Name)
            /*
             * Quote from GetVolumePathNameW docs:
             *     If this parameter is an empty string, "",
             *     the function fails but the last error is set to ERROR_SUCCESS.
             */
            return ERROR_FILE_NOT_FOUND;

        if (!GetVolumePathNameW(Name, VolumeBuf + 4, VolumeBufSize / sizeof(WCHAR) - 4))
            return GetLastError();

        int Len = lstrlenW(VolumeBuf + 4);
        if (L'\\' == VolumeBuf[4])
            memmove(VolumeBuf, VolumeBuf + 4, (Len + 1) * sizeof(WCHAR));
        else
        {
            VolumeBuf[0] = L'\\';
            VolumeBuf[1] = L'\\';
            VolumeBuf[2] = L'?';
            VolumeBuf[3] = L'\\';
            Len += 4;
        }
        PWSTR P = VolumeBuf + Len;
        if (VolumeBuf < P && L'\\' == P[-1])
            P[-1] = L'\0';

        return ERROR_SUCCESS;
    }
    BOOL CanEject(PWSTR Name)
    {
        WCHAR Volume[MAX_PATH];
        HANDLE Handle = INVALID_HANDLE_VALUE;
        STORAGE_PROPERTY_QUERY Query;
        DWORD BytesTransferred;
        union
        {
            STORAGE_DEVICE_DESCRIPTOR V;
            UINT8 B[sizeof(STORAGE_DEVICE_DESCRIPTOR) + 1024];
        } DescBuf;
        DWORD Error;
        BOOL Result = FALSE;

        Error = GetVolume(Name, Volume, sizeof Volume);
        if (ERROR_SUCCESS != Error)
            goto exit;

        Handle = CreateFileW(Volume, 0, 0, 0, OPEN_EXISTING, 0, 0);
        if (INVALID_HANDLE_VALUE == Handle)
        {
            Error = GetLastError();
            goto exit;
        }

        Query.PropertyId = StorageDeviceProperty;
        Query.QueryType = PropertyStandardQuery;
        Query.AdditionalParameters[0] = 0;
        memset(&DescBuf, 0, sizeof DescBuf);

        if (!DeviceIoControl(Handle, IOCTL_STORAGE_QUERY_PROPERTY,
            &Query, sizeof Query, &DescBuf, sizeof DescBuf,
            &BytesTransferred, 0))
        {
            Error = GetLastError();
            goto exit;
        }

        if (sizeof DescBuf >= DescBuf.V.Size && 0 != DescBuf.V.VendorIdOffset)
            Result = 0 == strcmp((const char *)((PUINT8)&DescBuf + DescBuf.V.VendorIdOffset),
                SPD_IOCTL_VENDOR_ID);

    exit:
        if (INVALID_HANDLE_VALUE != Handle)
            CloseHandle(Handle);

        return Result;
    }
    HRESULT Eject(PWSTR Name)
    {
        WCHAR Volume[4/*\\?\*/ + MAX_PATH];
        DWORD Error, LauncherError;

        Error = GetVolume(Name, Volume, sizeof Volume);
        if (ERROR_SUCCESS != Error)
            goto exit;

        Error = SpdLaunchStop(L".volume.", Volume, &LauncherError);
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
