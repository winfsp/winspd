/**
 * @file shellex/ejectmnu.hpp
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

#ifndef WINSPD_SHELLEX_EJECTMNU_HPP_INCLUDED
#define WINSPD_SHELLEX_EJECTMNU_HPP_INCLUDED

#include <shellex/ctxmenu.hpp>
#include <shared/shared.h>

#define SPD_SHELLEX_EJECT_PROGID        "Drive"
#define SPD_SHELLEX_EJECT_VERB          "WinSpd.Eject"
#define SPD_SHELLEX_EJECT_VERB_DESC     "Eject"

class EjectContextMenu : public ContextMenu
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
            { L"shellex\\ContextMenuHandlers" },
            { L"" SPD_SHELLEX_EJECT_VERB, 1 },
            { 0, REG_SZ, GuidStr, (lstrlenW(GuidStr) + 1) * sizeof(WCHAR) },
        };
        if (Flag)
            return HRESULT_FROM_WIN32(RegAddEntries(HKEY_LOCAL_MACHINE,
                Entries, sizeof Entries / sizeof Entries[0], 0));
        else
            return HRESULT_FROM_WIN32(RegDeleteEntries(HKEY_LOCAL_MACHINE,
                Entries, sizeof Entries / sizeof Entries[0], 0));
    }

    /* IContextMenu */
    STDMETHODIMP QueryContextMenu(HMENU Menu, UINT Index, UINT CmdFirst, UINT CmdLast, UINT Flags)
    {
        if (0 != (Flags & CMF_DEFAULTONLY))
            return MAKE_HRESULT(SEVERITY_SUCCESS, 0, 0);

        if (!CanInvoke())
            return MAKE_HRESULT(SEVERITY_SUCCESS, 0, 0);

        /* look for a separator immediately after our position and try to place ourselves over it */
        for (int I = Index, Count = GetMenuItemCount(Menu); Count > I; I++)
        {
            MENUITEMINFOA MenuInfo;
            memset(&MenuInfo, 0, sizeof MenuInfo);
            MenuInfo.cbSize = sizeof MenuInfo;
            MenuInfo.fMask = MIIM_FTYPE;
            if (GetMenuItemInfoA(Menu, I, MF_BYPOSITION, &MenuInfo) &&
                0 != (MenuInfo.fType & MFT_SEPARATOR))
            {
                Index = I;
                break;
            }
        }

        InsertMenuW(Menu, Index, MF_STRING | MF_BYPOSITION, CmdFirst, GetVerbTextW());
        return MAKE_HRESULT(SEVERITY_SUCCESS, 0, 1);
    }

    /* internal interface */
    PSTR GetVerbNameA()
    {
        return SPD_SHELLEX_EJECT_PROGID;
    }
    PWSTR GetVerbNameW()
    {
        return L"" SPD_SHELLEX_EJECT_PROGID;
    }
    PWSTR GetVerbTextW()
    {
        return L"" SPD_SHELLEX_EJECT_VERB_DESC;
    }
    BOOL CanInvoke()
    {
        return CanEject(_FileName);
    }
    HRESULT Invoke(HWND Window, BOOL ShowUI)
    {
        DWORD Error;

        Error = Eject(_FileName);
        if (ERROR_SUCCESS != Error && ShowUI)
        {
            WCHAR MessageBuf[1024];
            WCHAR ErrorBuf[512];

            if (0 == FormatMessageW(
                FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_MAX_WIDTH_MASK,
                0, Error, 0, ErrorBuf, sizeof ErrorBuf / sizeof ErrorBuf[0], 0))
                ErrorBuf[0] = '\0';
            wsprintfW(MessageBuf, L"The volume %s cannot be ejected.\n%s", _FileName, ErrorBuf);

            MessageBoxW(0, MessageBuf, L"" SPD_SHELLEX_EJECT_VERB_DESC, MB_OK | MB_ICONWARNING);
        }

        return S_OK;
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
    static DWORD GetVolumeOwnerProcessId(PWSTR Volume, PDWORD PProcessId)
    {
        HANDLE Handle = INVALID_HANDLE_VALUE;
        STORAGE_PROPERTY_QUERY Query;
        DWORD BytesTransferred;
        union
        {
            STORAGE_DEVICE_DESCRIPTOR Device;
            STORAGE_DEVICE_ID_DESCRIPTOR DeviceId;
            UINT8 B[1024];
        } DescBuf;
        PSTORAGE_IDENTIFIER Identifier;
        DWORD ProcessId;
        DWORD Error;

        *PProcessId = 0;

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

        Error = ERROR_NO_ASSOCIATION;
        if (sizeof DescBuf >= DescBuf.Device.Size && 0 != DescBuf.Device.VendorIdOffset)
            if (0 == strcmp((const char *)((PUINT8)&DescBuf + DescBuf.Device.VendorIdOffset),
                SPD_IOCTL_VENDOR_ID))
                Error = ERROR_SUCCESS;
        if (ERROR_SUCCESS != Error)
            goto exit;

        Query.PropertyId = StorageDeviceIdProperty;
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

        Error = ERROR_NO_ASSOCIATION;
        if (sizeof DescBuf >= DescBuf.DeviceId.Size)
        {
            Identifier = (PSTORAGE_IDENTIFIER)DescBuf.DeviceId.Identifiers;
            for (ULONG I = 0; DescBuf.DeviceId.NumberOfIdentifiers > I; I++)
            {
                if (StorageIdCodeSetBinary == Identifier->CodeSet &&
                    StorageIdTypeVendorSpecific == Identifier->Type &&
                    StorageIdAssocDevice == Identifier->Association &&
                    8 == Identifier->IdentifierSize &&
                    'P' == Identifier->Identifier[0] &&
                    'I' == Identifier->Identifier[1] &&
                    'D' == Identifier->Identifier[2] &&
                    ' ' == Identifier->Identifier[3])
                {
                    ProcessId =
                        (Identifier->Identifier[4] << 24) |
                        (Identifier->Identifier[5] << 16) |
                        (Identifier->Identifier[6] << 8) |
                        (Identifier->Identifier[7]);
                    Error = ERROR_SUCCESS;
                    break;
                }
                Identifier = (PSTORAGE_IDENTIFIER)((PUINT8)Identifier + Identifier->NextOffset);
            }
        }
        if (ERROR_SUCCESS != Error)
            goto exit;

        *PProcessId = ProcessId;

    exit:
        if (INVALID_HANDLE_VALUE != Handle)
            CloseHandle(Handle);

        return Error;
    }
    BOOL CanEject(PWSTR Name)
    {
        WCHAR Volume[4/*\\?\*/ + MAX_PATH];
        DWORD ProcessId;
        DWORD Error;
        BOOL Result = FALSE;

        Error = GetVolume(Name, Volume, sizeof Volume);
        if (ERROR_SUCCESS != Error)
            goto exit;

        Error = GetVolumeOwnerProcessId(Volume, &ProcessId);
        if (ERROR_SUCCESS != Error)
            goto exit;

        Result = TRUE;

    exit:
        return Result;
    }
    DWORD Eject(PWSTR Name)
    {
        WCHAR Volume[4/*\\?\*/ + MAX_PATH];
        WCHAR ProcessIdStr[16];
        DWORD ProcessId;
        DWORD Error, LauncherError;

        Error = GetVolume(Name, Volume, sizeof Volume);
        if (ERROR_SUCCESS != Error)
            goto exit;

        Error = GetVolumeOwnerProcessId(Volume, &ProcessId);
        if (ERROR_SUCCESS != Error)
            goto exit;

        wsprintfW(ProcessIdStr, L"%lu", ProcessId);
        Error = SpdLaunchStop(L".pid.", ProcessIdStr, &LauncherError);
        if (ERROR_SUCCESS != Error || ERROR_FILE_NOT_FOUND == LauncherError)
        {
            Error = ERROR_NO_ASSOCIATION;
            goto exit;
        }

        Error = LauncherError;

    exit:
        return Error;
    }
};

#endif
