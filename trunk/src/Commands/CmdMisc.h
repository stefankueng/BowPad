// This file is part of BowPad.
//
// Copyright (C) 2013 - Stefan Kueng
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
#include "AppUtils.h"

class CCmdDelete : public ICommand
{
public:

    CCmdDelete(void * obj) : ICommand(obj)
    {
    }

    ~CCmdDelete(void)
    {
    }

    virtual bool Execute()
    {
        ScintillaCall(SCI_CLEAR);
        return true;
    }

    virtual UINT GetCmdId() { return cmdDelete; }
};


class CCmdSelectAll : public ICommand
{
public:

    CCmdSelectAll(void * obj) : ICommand(obj)
    {
    }

    ~CCmdSelectAll(void)
    {
    }

    virtual bool Execute()
    {
        ScintillaCall(SCI_SELECTALL);
        return true;
    }

    virtual UINT GetCmdId() { return cmdSelectAll; }
};

class CCmdGotoBrace : public ICommand
{
public:

    CCmdGotoBrace(void * obj) : ICommand(obj)
    {
    }

    ~CCmdGotoBrace(void)
    {
    }

    virtual bool Execute()
    {
        GotoBrace();
        return true;
    }

    virtual UINT GetCmdId() { return cmdGotoBrace; }
};

class CCmdToggleTheme : public ICommand
{
public:

    CCmdToggleTheme(void * obj) : ICommand(obj)
    {
        CAppUtils::GetRibbonColors(text, back, high);
        int dark = (int)CIniSettings::Instance().GetInt64(L"View", L"darktheme", 0);
        if (dark)
        {
            CAppUtils::SetDarkTheme(!CAppUtils::IsDarkTheme());
            CAppUtils::SetRibbonColors(RGB(255,255,255), RGB(20,20,20), RGB(50,50,50));
        }
        InvalidateUICommand(UI_INVALIDATIONS_PROPERTY, &UI_PKEY_BooleanValue);
    }

    ~CCmdToggleTheme(void)
    {
    }

    virtual bool Execute()
    {
        CAppUtils::SetDarkTheme(!CAppUtils::IsDarkTheme());
        CDocument doc = GetDocument(GetCurrentTabIndex());
        SetupLexerForLang(doc.m_language);
        if (CAppUtils::IsDarkTheme())
            CAppUtils::SetRibbonColors(RGB(255,255,255), RGB(20,20,20), RGB(50,50,50));
        else
            CAppUtils::SetRibbonColorsHSB(text, back, high);

        CIniSettings::Instance().SetInt64(L"View", L"darktheme", CAppUtils::IsDarkTheme() ? 1 : 0);

        InvalidateUICommand(UI_INVALIDATIONS_PROPERTY, &UI_PKEY_BooleanValue);
        return true;
    }

    virtual UINT GetCmdId() { return cmdToggleTheme; }

    virtual HRESULT IUICommandHandlerUpdateProperty( REFPROPERTYKEY key, const PROPVARIANT* /*ppropvarCurrentValue*/, PROPVARIANT* ppropvarNewValue )
    {
        if (UI_PKEY_BooleanValue == key)
        {
            return UIInitPropertyFromBoolean(UI_PKEY_BooleanValue, CAppUtils::IsDarkTheme(), ppropvarNewValue);
        }
        return E_NOTIMPL;
    }
private:
    UI_HSBCOLOR text;
    UI_HSBCOLOR back;
    UI_HSBCOLOR high;
};
