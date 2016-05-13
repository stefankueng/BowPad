// This file is part of BowPad.
//
// Copyright (C) 2014, 2016 - Stefan Kueng
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

#include "stdafx.h"
#include "CmdWhiteSpace.h"
#include "IniSettings.h"

CCmdWhiteSpace::CCmdWhiteSpace(void * obj) : ICommand(obj)
{
    int ws = (int)CIniSettings::Instance().GetInt64(L"View", L"whitespace", 0);
    ScintillaCall(SCI_SETVIEWWS, ws);
    InvalidateUICommand(UI_INVALIDATIONS_PROPERTY, &UI_PKEY_BooleanValue);
}

CCmdWhiteSpace::~CCmdWhiteSpace()
{
}

bool CCmdWhiteSpace::Execute()
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

HRESULT CCmdWhiteSpace::IUICommandHandlerUpdateProperty(REFPROPERTYKEY key, const PROPVARIANT* /*ppropvarCurrentValue*/, PROPVARIANT* ppropvarNewValue)
{
    if (UI_PKEY_BooleanValue == key)
    {
        return UIInitPropertyFromBoolean(UI_PKEY_BooleanValue, ScintillaCall(SCI_GETVIEWWS) > 0, ppropvarNewValue);
    }
    return E_NOTIMPL;
}

CCmdTabSize::CCmdTabSize(void * obj) : ICommand(obj)
{
    int ve = (int)CIniSettings::Instance().GetInt64(L"View", L"tabsize", 4);
    ScintillaCall(SCI_SETTABWIDTH, ve);
    InvalidateUICommand(UI_INVALIDATIONS_PROPERTY, &UI_PKEY_DecimalValue);
    UpdateStatusBar(false);
}

CCmdTabSize::~CCmdTabSize()
{
}

HRESULT CCmdTabSize::IUICommandHandlerUpdateProperty(REFPROPERTYKEY key, const PROPVARIANT* /*ppropvarCurrentValue*/, PROPVARIANT* ppropvarNewValue)
{
    // REVIEW: ZeroMemory might not be the best tool here. VariantInit
    // or UIInitPropertyFromDecimal or some such routine seems better.
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

HRESULT CCmdTabSize::IUICommandHandlerExecute(UI_EXECUTIONVERB /*verb*/, const PROPERTYKEY* /*key*/, const PROPVARIANT* ppropvarValue, IUISimplePropertySet* /*pCommandExecutionProperties*/)
{
    ScintillaCall(SCI_SETTABWIDTH, ppropvarValue->intVal);
    CIniSettings::Instance().SetInt64(L"View", L"tabsize", ppropvarValue->intVal);
    UpdateStatusBar(false);
    return S_OK;
}

CCmdUseTabs::CCmdUseTabs(void * obj) : ICommand(obj)
{
    int ws = (int)CIniSettings::Instance().GetInt64(L"View", L"usetabs", 1);
    ScintillaCall(SCI_SETUSETABS, ws);
    InvalidateUICommand(UI_INVALIDATIONS_PROPERTY, &UI_PKEY_BooleanValue);
}

CCmdUseTabs::~CCmdUseTabs()
{
}

bool CCmdUseTabs::Execute()
{
    ScintillaCall(SCI_SETUSETABS, ScintillaCall(SCI_GETUSETABS) ? 0 : 1);
    CIniSettings::Instance().SetInt64(L"View", L"usetabs", ScintillaCall(SCI_GETUSETABS));
    InvalidateUICommand(UI_INVALIDATIONS_PROPERTY, &UI_PKEY_BooleanValue);
    UpdateStatusBar(false);
    return true;
}

HRESULT CCmdUseTabs::IUICommandHandlerUpdateProperty(REFPROPERTYKEY key, const PROPVARIANT* /*ppropvarCurrentValue*/, PROPVARIANT* ppropvarNewValue)
{
    if (UI_PKEY_BooleanValue == key)
    {
        return UIInitPropertyFromBoolean(UI_PKEY_BooleanValue, ScintillaCall(SCI_GETUSETABS) > 0, ppropvarNewValue);
    }
    return E_NOTIMPL;
}
