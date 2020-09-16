/**
 * @file shellex/library.cpp
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

#include <shellex/com.hpp>
#include <shellex/mountcmd.hpp>
#include <shellex/ejectmnu.hpp>
#include <shlobj.h>

CoClassOf<MountCommand> MountCommandClass;
CoClassOf<EjectContextMenu> EjectContextMenuClass;

STDAPI DllGetClassObject(REFCLSID Clsid, REFIID Iid, LPVOID *PObject)
{
    return CoClass::GetClassObject(Clsid, Iid, PObject);
}

STDAPI DllCanUnloadNow()
{
    return CoClass::CanUnloadNow();
}

STDAPI DllRegisterServer()
{
    HRESULT Result;
    Result = CoClass::RegisterServer();
    if (S_OK == Result)
        SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, 0, 0);
    return Result;
}

STDAPI DllUnregisterServer()
{
    HRESULT Result;
    Result = CoClass::UnregisterServer();
    if (S_OK == Result)
        SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, 0, 0);
    return Result;
}
