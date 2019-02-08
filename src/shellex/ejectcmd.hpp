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

#define SPD_SHELLEX_EJECT_PROGID        "Drive"
#define SPD_SHELLEX_EJECT_VERB          "eject"
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
        if (Flag)
            return RegisterEx(Flag,
                L"" SPD_SHELLEX_EJECT_PROGID, L"" SPD_SHELLEX_EJECT_VERB, L"" SPD_SHELLEX_EJECT_VERB_DESC,
                TRUE, Clsid);
        else
            /* only delete Drive\shell\eject */
            return RegisterEx(Flag,
                L"" SPD_SHELLEX_EJECT_PROGID "\\shell\\" SPD_SHELLEX_EJECT_VERB, 0, 0,
                TRUE, Clsid);
    }
};

#endif
