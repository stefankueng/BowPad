// This file is part of BowPad.
//
// Copyright (C) 2013-2014, 2016-2017, 2019-2020 - Stefan Kueng
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// See <http://www.gnu.org/licenses/> for a copy of the full license text
//

#pragma once
#include "ICommand.h"
#include "BowPadUI.h"

class CCmdLineWrap : public ICommand
{
public:
    CCmdLineWrap(void* obj)
        : ICommand(obj)
    {
    }

    ~CCmdLineWrap() = default;

    bool Execute() override
    {
        ScintillaCall(SCI_SETWRAPMODE, ScintillaCall(SCI_GETWRAPMODE) ? 0 : SC_WRAP_WORD);
        CIniSettings::Instance().SetInt64(L"View", L"wrapmode", ScintillaCall(SCI_GETWRAPMODE));
        InvalidateUICommand(UI_INVALIDATIONS_PROPERTY, &UI_PKEY_BooleanValue);
        return true;
    }

    void BeforeLoad() override
    {
        int wrapmode = (int)CIniSettings::Instance().GetInt64(L"View", L"wrapmode", 0);
        ScintillaCall(SCI_SETWRAPMODE, wrapmode);
        InvalidateUICommand(UI_INVALIDATIONS_PROPERTY, &UI_PKEY_BooleanValue);
    }

    UINT GetCmdId() override { return cmdLineWrap; }

    HRESULT IUICommandHandlerUpdateProperty(REFPROPERTYKEY key, const PROPVARIANT* /*ppropvarCurrentValue*/, PROPVARIANT* ppropvarNewValue) override
    {
        if (UI_PKEY_BooleanValue == key)
        {
            return UIInitPropertyFromBoolean(UI_PKEY_BooleanValue, ScintillaCall(SCI_GETWRAPMODE) > 0, ppropvarNewValue);
        }
        return E_NOTIMPL;
    }
};

class CCmdLineWrapIndent : public ICommand
{
public:
    CCmdLineWrapIndent(void* obj)
        : ICommand(obj)
    {
    }

    ~CCmdLineWrapIndent() = default;

    bool Execute() override
    {
        ScintillaCall(SCI_SETWRAPSTARTINDENT, ScintillaCall(SCI_GETWRAPSTARTINDENT) ? 0 : max(1, ScintillaCall(SCI_GETTABWIDTH) / 2));
        CIniSettings::Instance().SetInt64(L"View", L"wrapmodeindent", ScintillaCall(SCI_GETWRAPSTARTINDENT) ? 1 : 0);
        ScintillaCall(SCI_SETWRAPINDENTMODE, SC_WRAPINDENT_INDENT);
        ScintillaCall(SCI_SETWRAPVISUALFLAGS, SC_WRAPVISUALFLAG_MARGIN | SC_WRAPVISUALFLAG_START | SC_WRAPVISUALFLAG_END);
        ScintillaCall(SCI_SETWRAPVISUALFLAGSLOCATION, SC_WRAPVISUALFLAGLOC_START_BY_TEXT);
        ScintillaCall(SCI_SETMARGINOPTIONS, SC_MARGINOPTION_SUBLINESELECT);
        InvalidateUICommand(UI_INVALIDATIONS_PROPERTY, &UI_PKEY_BooleanValue);
        return true;
    }

    void BeforeLoad() override
    {
        int wrapIndent = (int)CIniSettings::Instance().GetInt64(L"View", L"wrapmodeindent", 0);
        ScintillaCall(SCI_SETWRAPSTARTINDENT, wrapIndent ? max(1, ScintillaCall(SCI_GETTABWIDTH) / 2) : 0);
        ScintillaCall(SCI_SETWRAPINDENTMODE, SC_WRAPINDENT_INDENT);
        ScintillaCall(SCI_SETWRAPVISUALFLAGS, SC_WRAPVISUALFLAG_MARGIN | SC_WRAPVISUALFLAG_START | SC_WRAPVISUALFLAG_END);
        ScintillaCall(SCI_SETWRAPVISUALFLAGSLOCATION, SC_WRAPVISUALFLAGLOC_START_BY_TEXT);
        ScintillaCall(SCI_SETMARGINOPTIONS, SC_MARGINOPTION_SUBLINESELECT);
        InvalidateUICommand(UI_INVALIDATIONS_PROPERTY, &UI_PKEY_BooleanValue);
    }

    UINT GetCmdId() override { return cmdLineWrapIndent; }

    HRESULT IUICommandHandlerUpdateProperty(REFPROPERTYKEY key, const PROPVARIANT* /*ppropvarCurrentValue*/, PROPVARIANT* ppropvarNewValue) override
    {
        if (UI_PKEY_BooleanValue == key)
        {
            return UIInitPropertyFromBoolean(UI_PKEY_BooleanValue, ScintillaCall(SCI_GETWRAPSTARTINDENT) > 0, ppropvarNewValue);
        }
        return E_NOTIMPL;
    }
};
