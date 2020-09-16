/**
 * @file shellex/command.hpp
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

#ifndef WINSPD_SHELLEX_COMMAND_HPP_INCLUDED
#define WINSPD_SHELLEX_COMMAND_HPP_INCLUDED

#include <shellex/com.hpp>
#include <shobjidl.h>

class Command : public CoObject<
    IInitializeCommand,
    IObjectWithSelection,
    IExecuteCommand,
    IExplorerCommandState>
{
public:
    /* IInitializeCommand */
    STDMETHODIMP Initialize(LPCWSTR CommandName, IPropertyBag *Bag)
    {
        return S_OK;
    }

    /* IObjectWithSelection */
    STDMETHODIMP SetSelection(IShellItemArray *Array)
    {
        if (0 != _Array)
            _Array->Release();
        if (0 != Array)
            Array->AddRef();
        _Array = Array;
        return S_OK;
    }
    STDMETHODIMP GetSelection(REFIID Iid, void **PObject)
    {
        if (0 != _Array)
            return _Array->QueryInterface(Iid, PObject);
        else
        {
            *PObject = 0;
            return E_NOINTERFACE;
        }
    }

    /* IExecuteCommand */
    STDMETHODIMP SetKeyState(DWORD KeyState)
    {
        return S_OK;
    }
    STDMETHODIMP SetParameters(LPCWSTR Parameters)
    {
        return S_OK;
    }
    STDMETHODIMP SetPosition(POINT Point)
    {
        return S_OK;
    }
    STDMETHODIMP SetShowWindow(int Show)
    {
        return S_OK;
    }
    STDMETHODIMP SetNoShowUI(BOOL NoShowUI)
    {
        return S_OK;
    }
    STDMETHODIMP SetDirectory(LPCWSTR Directory)
    {
        return S_OK;
    }
    STDMETHODIMP Execute()
    {
        return S_OK;
    }

    /* IExplorerCommandState */
    STDMETHODIMP GetState(IShellItemArray *Array, BOOL OkToBeSlow, EXPCMDSTATE *CmdState)
    {
        *CmdState = ECS_ENABLED;
        return S_OK;
    }

    /* internal interface */
    Command() : _Array(0)
    {
    }
    ~Command()
    {
        if (0 != _Array)
            _Array->Release();
    }

protected:
    IShellItemArray *_Array;
};

#endif
