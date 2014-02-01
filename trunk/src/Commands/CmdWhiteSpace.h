// This file is part of BowPad.
//
// Copyright (C) 2013-2014 - Stefan Kueng
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

class CCmdWhiteSpace : public ICommand
{
public:

    CCmdWhiteSpace(void * obj) : ICommand(obj)
    {
        int ws = (int)CIniSettings::Instance().GetInt64(L"View", L"whitespace", 0);
        ScintillaCall(SCI_SETVIEWWS, ws);
        InvalidateUICommand(UI_INVALIDATIONS_PROPERTY, &UI_PKEY_BooleanValue);
    }

    ~CCmdWhiteSpace(void)
    {
    }

    virtual bool Execute() override
    {
        bool bShown = ScintillaCall(SCI_GETVIEWWS) != 0;
        ScintillaCall(SCI_SETVIEWWS, bShown ? 0 : 1);
        CIniSettings::Instance().SetInt64(L"View", L"whitespace", ScintillaCall(SCI_GETVIEWWS));
        if (bShown || ((GetKeyState(VK_SHIFT) & 0x8000)==0))
            ScintillaCall(SCI_SETVIEWEOL, false);
        else
            ScintillaCall(SCI_SETVIEWEOL, true);
        InvalidateUICommand(UI_INVALIDATIONS_PROPERTY, &UI_PKEY_BooleanValue);
        return true;
    }

    virtual UINT GetCmdId() override { return cmdWhiteSpace; }

    virtual HRESULT IUICommandHandlerUpdateProperty(REFPROPERTYKEY key, const PROPVARIANT* /*ppropvarCurrentValue*/, PROPVARIANT* ppropvarNewValue) override
    {
        if (UI_PKEY_BooleanValue == key)
        {
            return UIInitPropertyFromBoolean(UI_PKEY_BooleanValue, ScintillaCall(SCI_GETVIEWWS) > 0, ppropvarNewValue);
        }
        return E_NOTIMPL;
    }

};

class CCmdTabSize : public ICommand
{
public:

    CCmdTabSize(void * obj) : ICommand(obj)
    {
        int ve = (int)CIniSettings::Instance().GetInt64(L"View", L"tabsize", 4);
        ScintillaCall(SCI_SETTABWIDTH, ve);
        InvalidateUICommand(UI_INVALIDATIONS_PROPERTY, &UI_PKEY_DecimalValue);
    }

    ~CCmdTabSize(void)
    {
    }

    virtual bool Execute() override { return true; }

    virtual UINT GetCmdId() override { return cmdTabSize; }

    virtual HRESULT IUICommandHandlerUpdateProperty(REFPROPERTYKEY key, const PROPVARIANT* /*ppropvarCurrentValue*/, PROPVARIANT* ppropvarNewValue) override
    {
        HRESULT hr = S_OK;
        // Set the minimum value
        if (IsEqualPropertyKey(key, UI_PKEY_MinValue))
        {
            ZeroMemory(ppropvarNewValue, sizeof(*ppropvarNewValue));
            ppropvarNewValue->vt = VT_DECIMAL;
            VarDecFromR8(1, &ppropvarNewValue->decVal);
            hr = S_OK;
        }
        // Set the maximum value
        else if (IsEqualPropertyKey(key, UI_PKEY_MaxValue))
        {
            ZeroMemory(ppropvarNewValue, sizeof(*ppropvarNewValue));
            ppropvarNewValue->vt = VT_DECIMAL;
            VarDecFromR8(20, &ppropvarNewValue->decVal);
            hr = S_OK;
        }
        // Set the increment
        else if (IsEqualPropertyKey(key, UI_PKEY_Increment))
        {
            ZeroMemory(ppropvarNewValue, sizeof(*ppropvarNewValue));
            ppropvarNewValue->vt = VT_DECIMAL;
            VarDecFromR8(1, &ppropvarNewValue->decVal);
            hr = S_OK;
        }
        // Set the number of decimal places
        else if (IsEqualPropertyKey(key, UI_PKEY_DecimalPlaces))
        {
            hr = InitPropVariantFromUInt32(0, ppropvarNewValue);
        }
        // Set the initial value
        else if (IsEqualPropertyKey(key, UI_PKEY_DecimalValue))
        {
            int ve = (int)CIniSettings::Instance().GetInt64(L"View", L"tabsize", 4);
            ZeroMemory(ppropvarNewValue, sizeof(*ppropvarNewValue));
            ppropvarNewValue->vt = VT_DECIMAL;
            VarDecFromR8(ve, &ppropvarNewValue->decVal);
        }
        return hr;
    }

    virtual HRESULT IUICommandHandlerExecute(UI_EXECUTIONVERB /*verb*/, const PROPERTYKEY* /*key*/, const PROPVARIANT* ppropvarValue, IUISimplePropertySet* /*pCommandExecutionProperties*/) override
    {
        ScintillaCall(SCI_SETTABWIDTH, ppropvarValue->intVal);
        CIniSettings::Instance().SetInt64(L"View", L"tabsize", ppropvarValue->intVal);
        return S_OK;
    }
};

class CCmdUseTabs : public ICommand
{
public:

    CCmdUseTabs(void * obj) : ICommand(obj)
    {
        int ws = (int)CIniSettings::Instance().GetInt64(L"View", L"usetabs", 1);
        ScintillaCall(SCI_SETUSETABS, ws);
        InvalidateUICommand(UI_INVALIDATIONS_PROPERTY, &UI_PKEY_BooleanValue);
    }

    ~CCmdUseTabs(void)
    {
    }

    virtual bool Execute() override
    {
        ScintillaCall(SCI_SETUSETABS, ScintillaCall(SCI_GETUSETABS) ? 0 : 1);
        CIniSettings::Instance().SetInt64(L"View", L"usetabs", ScintillaCall(SCI_GETUSETABS));
        InvalidateUICommand(UI_INVALIDATIONS_PROPERTY, &UI_PKEY_BooleanValue);
        return true;
    }

    virtual UINT GetCmdId() override { return cmdUseTabs; }

    virtual HRESULT IUICommandHandlerUpdateProperty(REFPROPERTYKEY key, const PROPVARIANT* /*ppropvarCurrentValue*/, PROPVARIANT* ppropvarNewValue) override
    {
        if (UI_PKEY_BooleanValue == key)
        {
            return UIInitPropertyFromBoolean(UI_PKEY_BooleanValue, ScintillaCall(SCI_GETUSETABS) > 0, ppropvarNewValue);
        }
        return E_NOTIMPL;
    }

};
