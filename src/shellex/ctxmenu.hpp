/**
 * @file shellex/ctxmenu.hpp
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

#ifndef WINSPD_SHELLEX_CTXMENU_HPP_INCLUDED
#define WINSPD_SHELLEX_CTXMENU_HPP_INCLUDED

#include <shellex/com.hpp>
#include <shlobj.h>
#include <shlwapi.h>

class ContextMenu : public CoObject<
    IShellExtInit,
    IContextMenu>
{
public:
    /* IShellExtInit */
    STDMETHODIMP Initialize(PCIDLIST_ABSOLUTE FolderPidl, IDataObject *DataObject, HKEY ProgidKey)
    {
        ILFree(_FolderPidl);
        _FolderPidl = 0 != FolderPidl ? ILClone(FolderPidl) : 0;

        _FileName[0] = L'\0';
        if (0 != DataObject)
        {
            FORMATETC Format = { CF_HDROP, 0, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
            STGMEDIUM Medium;
            HRESULT Result;

            Result = DataObject->GetData(&Format, &Medium);
            if (SUCCEEDED(Result))
            {
                if (0 < DragQueryFileW((HDROP)Medium.hGlobal, -1, 0, 0))
                    DragQueryFileW((HDROP)Medium.hGlobal, 0,
                        _FileName, sizeof _FileName / sizeof _FileName[0]);
                ReleaseStgMedium(&Medium);
            }
        }

        return S_OK;
    }

    /* IContextMenu */
    STDMETHODIMP QueryContextMenu(HMENU Menu, UINT Index, UINT CmdFirst, UINT CmdLast, UINT Flags)
    {
        if (0 != (Flags & CMF_DEFAULTONLY))
            return MAKE_HRESULT(SEVERITY_SUCCESS, 0, 0);

        if (!CanInvoke())
            return MAKE_HRESULT(SEVERITY_SUCCESS, 0, 0);

        InsertMenuW(Menu, Index, MF_STRING | MF_BYPOSITION, CmdFirst, GetVerbTextW());
        return MAKE_HRESULT(SEVERITY_SUCCESS, 0, 1);
    }
    STDMETHODIMP InvokeCommand(CMINVOKECOMMANDINFO *Info)
    {
        BOOL Unicode = FALSE;
        BOOL Invoked = FALSE;

        if (sizeof(CMINVOKECOMMANDINFOEX) == Info->cbSize)
            Unicode = 0 != (CMIC_MASK_UNICODE & Info->fMask);

        if (!Unicode && HIWORD(Info->lpVerb))
        {
            Invoked = 0 == StrCmpIA(Info->lpVerb, GetVerbNameA());
        }
        else if (Unicode && HIWORD(((CMINVOKECOMMANDINFOEX *)Info)->lpVerbW))
        {
            Invoked = 0 == StrCmpIW(((CMINVOKECOMMANDINFOEX *)Info)->lpVerbW, GetVerbNameW());
        }
        else if (0 == LOWORD(Info->lpVerb))
        {
            Invoked = TRUE;
        }

        return Invoked ? Invoke(Info->hwnd, 0 == (CMIC_MASK_FLAG_NO_UI & Info->fMask)) : E_FAIL;
    }
    STDMETHODIMP GetCommandString(UINT_PTR Cmd, UINT Type, UINT *Reserved, CHAR *Name, UINT Count)
    {
        return E_NOTIMPL;
    }

    /* internal interface */
    ContextMenu() : _FolderPidl(0)
    {
        _FileName[0] = L'\0';
    }
    ~ContextMenu()
    {
        ILFree(_FolderPidl);
    }
    virtual PSTR GetVerbNameA() = 0;
    virtual PWSTR GetVerbNameW() = 0;
    virtual PWSTR GetVerbTextW() = 0;
    virtual BOOL CanInvoke()
    {
        return TRUE;
    }
    virtual HRESULT Invoke(HWND Window, BOOL ShowUI) = 0;

protected:
    PIDLIST_ABSOLUTE _FolderPidl;
    WCHAR _FileName[MAX_PATH];
};

#endif
