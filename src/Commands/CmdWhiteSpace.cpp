﻿// This file is part of BowPad.
//
// Copyright (C) 2014, 2016-2017, 2020-2021, 2024 - Stefan Kueng
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
#include "EditorConfigHandler.h"

CCmdWhiteSpace::CCmdWhiteSpace(void* obj)
    : ICommand(obj)
{
}

bool CCmdWhiteSpace::Execute()
{
    bool bShown = Scintilla().ViewWS() != Scintilla::WhiteSpace::Invisible;
    Scintilla().SetViewWS(bShown ? Scintilla::WhiteSpace::Invisible : Scintilla::WhiteSpace::VisibleAlways);
    CIniSettings::Instance().SetInt64(L"View", L"whitespace", static_cast<int>(Scintilla().ViewWS()));
    if (bShown || ((GetKeyState(VK_SHIFT) & 0x8000) == 0))
        Scintilla().SetViewEOL(false);
    else
        Scintilla().SetViewEOL(true);
    InvalidateUICommand(UI_INVALIDATIONS_PROPERTY, &UI_PKEY_BooleanValue);
    return true;
}

void CCmdWhiteSpace::AfterInit()
{
    auto ws = static_cast<Scintilla::WhiteSpace>(CIniSettings::Instance().GetInt64(L"View", L"whitespace", 0));
    Scintilla().SetViewWS(ws);
    InvalidateUICommand(UI_INVALIDATIONS_PROPERTY, &UI_PKEY_BooleanValue);
}

HRESULT CCmdWhiteSpace::IUICommandHandlerUpdateProperty(REFPROPERTYKEY key, const PROPVARIANT* /*pPropVarCurrentValue*/, PROPVARIANT* pPropVarNewValue)
{
    if (UI_PKEY_BooleanValue == key)
    {
        return UIInitPropertyFromBoolean(UI_PKEY_BooleanValue, Scintilla().ViewWS() > Scintilla::WhiteSpace::Invisible, pPropVarNewValue);
    }
    return E_NOTIMPL;
}

CCmdTabSize::CCmdTabSize(void* obj)
    : ICommand(obj)
{
}

void CCmdTabSize::AfterInit()
{
    int   ve         = static_cast<int>(CIniSettings::Instance().GetInt64(L"View", L"tabsize", 4));
    auto& doc        = GetActiveDocument();
    auto  tabSizeSet = CEditorConfigHandler::Instance().HasTabSize(doc.m_path);
    if (!tabSizeSet)
        Scintilla().SetTabWidth(ve);
    InvalidateUICommand(UI_INVALIDATIONS_PROPERTY, &UI_PKEY_DecimalValue);
    UpdateStatusBar(false);
}

void CCmdTabSize::TabNotify(TBHDR* ptbHdr)
{
    if (ptbHdr->hdr.code == TCN_SELCHANGE)
    {
        InvalidateUICommand(UI_INVALIDATIONS_PROPERTY, &UI_PKEY_DecimalValue);
    }
}

HRESULT CCmdTabSize::IUICommandHandlerUpdateProperty(REFPROPERTYKEY key, const PROPVARIANT* /*pPropVarCurrentValue*/, PROPVARIANT* pPropVarNewValue)
{
    HRESULT hr = S_OK;
    // Set the minimum value
    if (IsEqualPropertyKey(key, UI_PKEY_MinValue))
    {
        DECIMAL decOut;
        VarDecFromI4(1, &decOut);
        hr = UIInitPropertyFromDecimal(UI_PKEY_DecimalValue, decOut, pPropVarNewValue);
    }
    // Set the maximum value
    else if (IsEqualPropertyKey(key, UI_PKEY_MaxValue))
    {
        DECIMAL decOut;
        VarDecFromI4(20, &decOut);
        hr = UIInitPropertyFromDecimal(UI_PKEY_DecimalValue, decOut, pPropVarNewValue);
    }
    // Set the increment
    else if (IsEqualPropertyKey(key, UI_PKEY_Increment))
    {
        DECIMAL decOut;
        VarDecFromI4(1, &decOut);
        hr = UIInitPropertyFromDecimal(UI_PKEY_DecimalValue, decOut, pPropVarNewValue);
    }
    // Set the number of decimal places
    else if (IsEqualPropertyKey(key, UI_PKEY_DecimalPlaces))
    {
        hr = InitPropVariantFromUInt32(0, pPropVarNewValue);
    }
    // Set the initial value
    else if (IsEqualPropertyKey(key, UI_PKEY_DecimalValue))
    {
        DECIMAL decOut;
        VarDecFromI4(Scintilla().TabWidth(), &decOut);
        hr = UIInitPropertyFromDecimal(UI_PKEY_DecimalValue, decOut, pPropVarNewValue);
    }
    return hr;
}

HRESULT CCmdTabSize::IUICommandHandlerExecute(UI_EXECUTIONVERB /*verb*/, const PROPERTYKEY* /*key*/, const PROPVARIANT* pPropVarValue, IUISimplePropertySet* /*pCommandExecutionProperties*/)
{
    Scintilla().SetTabWidth(pPropVarValue->intVal);
    CIniSettings::Instance().SetInt64(L"View", L"tabsize", pPropVarValue->intVal);
    UpdateStatusBar(false);
    return S_OK;
}

// each document stores its own setting of tabs/spaces.
// the 'use tabs' command toggles both the setting of the
// current document as well as the global default.
// When a document is opened the first time, it's setting
// is set to 'default' and stays that way until the user
// executes this command.
// While the document has its setting set to 'default', it will
// always use the global default setting or the settings set
// by an editorconfig file.
//
// to think about:
// * only toggle the current doc settings, have the global settings toggled via a settings dialog or a separate dropdown button
// * allow to configure the tab/space setting with file extension masks, e.g. space for all *.cpp files but tabs for all *.py files
CCmdUseTabs::CCmdUseTabs(void* obj)
    : ICommand(obj)
{
}

bool CCmdUseTabs::Execute()
{
    if (HasActiveDocument())
    {
        auto& doc = GetModActiveDocument();
        if (doc.m_tabSpace == TabSpace::Default)
        {
            Scintilla().SetUseTabs(Scintilla().UseTabs() ? 0 : 1);
        }
        else
        {
            Scintilla().SetUseTabs(doc.m_tabSpace == TabSpace::Tabs ? 0 : 1);
        }
        doc.m_tabSpace = Scintilla().UseTabs() ? TabSpace::Tabs : TabSpace::Spaces;
        CIniSettings::Instance().SetInt64(L"View", L"usetabs", Scintilla().UseTabs());
        InvalidateUICommand(UI_INVALIDATIONS_PROPERTY, &UI_PKEY_BooleanValue);
        UpdateStatusBar(false);
        return true;
    }
    return false;
}

void CCmdUseTabs::TabNotify(TBHDR* ptbHdr)
{
    if (ptbHdr->hdr.code == TCN_SELCHANGE)
    {
        InvalidateUICommand(UI_INVALIDATIONS_PROPERTY, &UI_PKEY_BooleanValue);
    }
}

HRESULT CCmdUseTabs::IUICommandHandlerUpdateProperty(REFPROPERTYKEY key, const PROPVARIANT* /*pPropVarCurrentValue*/, PROPVARIANT* pPropVarNewValue)
{
    if (UI_PKEY_BooleanValue == key)
    {
        return UIInitPropertyFromBoolean(UI_PKEY_BooleanValue, Scintilla().UseTabs(), pPropVarNewValue);
    }
    return E_NOTIMPL;
}
