// This file is part of BowPad.
//
// Copyright (C) 2013-2014, 2016-2017, 2019-2021 - Stefan Kueng
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
        int wrapmode = static_cast<int>(CIniSettings::Instance().GetInt64(L"View", L"wrapmode", 0));
        ScintillaCall(SCI_SETWRAPMODE, wrapmode);
        InvalidateUICommand(UI_INVALIDATIONS_PROPERTY, &UI_PKEY_BooleanValue);
    }

    UINT GetCmdId() override { return cmdLineWrap; }

    HRESULT IUICommandHandlerUpdateProperty(REFPROPERTYKEY key, const PROPVARIANT* /*pPropVarCurrentValue*/, PROPVARIANT* pPropVarNewValue) override
    {
        if (UI_PKEY_BooleanValue == key)
        {
            return UIInitPropertyFromBoolean(UI_PKEY_BooleanValue, ScintillaCall(SCI_GETWRAPMODE) > 0, pPropVarNewValue);
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
        int wrapIndent = ScintillaCall(SCI_GETWRAPSTARTINDENT) ? 0 : 1;
        ScintillaCall(SCI_SETWRAPSTARTINDENT, wrapIndent ? max(1, ScintillaCall(SCI_GETTABWIDTH) / 2) : 0);
        CIniSettings::Instance().SetInt64(L"View", L"wrapmodeindent", wrapIndent);
        ScintillaCall(SCI_SETWRAPINDENTMODE, wrapIndent ? SC_WRAPINDENT_INDENT : SC_WRAPINDENT_FIXED);
        ScintillaCall(SCI_SETWRAPVISUALFLAGS, wrapIndent ? SC_WRAPVISUALFLAG_START | SC_WRAPVISUALFLAG_END : SC_WRAPVISUALFLAG_END);
        ScintillaCall(SCI_SETWRAPVISUALFLAGSLOCATION, SC_WRAPVISUALFLAGLOC_START_BY_TEXT);
        ScintillaCall(SCI_SETMARGINOPTIONS, SC_MARGINOPTION_SUBLINESELECT);
        InvalidateUICommand(UI_INVALIDATIONS_PROPERTY, &UI_PKEY_BooleanValue);
        return true;
    }

    void BeforeLoad() override
    {
        int wrapIndent = static_cast<int>(CIniSettings::Instance().GetInt64(L"View", L"wrapmodeindent", 0));
        ScintillaCall(SCI_SETWRAPSTARTINDENT, wrapIndent ? max(1, ScintillaCall(SCI_GETTABWIDTH) / 2) : 0);
        ScintillaCall(SCI_SETWRAPINDENTMODE, wrapIndent ? SC_WRAPINDENT_INDENT : SC_WRAPINDENT_FIXED);
        ScintillaCall(SCI_SETWRAPVISUALFLAGS, wrapIndent ? SC_WRAPVISUALFLAG_START | SC_WRAPVISUALFLAG_END : SC_WRAPVISUALFLAG_END);
        ScintillaCall(SCI_SETWRAPVISUALFLAGSLOCATION, SC_WRAPVISUALFLAGLOC_START_BY_TEXT);
        ScintillaCall(SCI_SETMARGINOPTIONS, SC_MARGINOPTION_SUBLINESELECT);
        InvalidateUICommand(UI_INVALIDATIONS_PROPERTY, &UI_PKEY_BooleanValue);
    }

    UINT GetCmdId() override { return cmdLineWrapIndent; }

    HRESULT IUICommandHandlerUpdateProperty(REFPROPERTYKEY key, const PROPVARIANT* /*pPropVarCurrentValue*/, PROPVARIANT* pPropVarNewValue) override
    {
        if (UI_PKEY_BooleanValue == key)
        {
            return UIInitPropertyFromBoolean(UI_PKEY_BooleanValue, ScintillaCall(SCI_GETWRAPSTARTINDENT) > 0, pPropVarNewValue);
        }
        return E_NOTIMPL;
    }
};
