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

constexpr Scintilla::WrapVisualFlag operator|(Scintilla::WrapVisualFlag a, Scintilla::WrapVisualFlag b) noexcept
{
    return static_cast<Scintilla::WrapVisualFlag>(static_cast<int>(a) | static_cast<int>(b));
}

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
        Scintilla().SetWrapMode(Scintilla().WrapMode() != Scintilla::Wrap::None ? Scintilla::Wrap::None : Scintilla::Wrap::Word);
        CIniSettings::Instance().SetInt64(L"View", L"wrapmode", static_cast<int>(Scintilla().WrapMode()));
        InvalidateUICommand(UI_INVALIDATIONS_PROPERTY, &UI_PKEY_BooleanValue);
        return true;
    }

    void BeforeLoad() override
    {
        Scintilla::Wrap wrapmode = static_cast<Scintilla::Wrap>(CIniSettings::Instance().GetInt64(L"View", L"wrapmode", 0));
        Scintilla().SetWrapMode(wrapmode);
        InvalidateUICommand(UI_INVALIDATIONS_PROPERTY, &UI_PKEY_BooleanValue);
    }

    UINT GetCmdId() override { return cmdLineWrap; }

    HRESULT IUICommandHandlerUpdateProperty(REFPROPERTYKEY key, const PROPVARIANT* /*pPropVarCurrentValue*/, PROPVARIANT* pPropVarNewValue) override
    {
        if (UI_PKEY_BooleanValue == key)
        {
            return UIInitPropertyFromBoolean(UI_PKEY_BooleanValue, Scintilla().WrapMode() > Scintilla::Wrap::None, pPropVarNewValue);
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
        int wrapIndent = Scintilla().WrapStartIndent() ? 0 : 1;
        Scintilla().SetWrapStartIndent(wrapIndent ? max(1, Scintilla().TabWidth() / 2) : 0);
        CIniSettings::Instance().SetInt64(L"View", L"wrapmodeindent", wrapIndent);
        Scintilla().SetWrapIndentMode(wrapIndent ? Scintilla::WrapIndentMode::Indent : Scintilla::WrapIndentMode::Fixed);
        Scintilla().SetWrapVisualFlags(wrapIndent ? Scintilla::WrapVisualFlag::Start | Scintilla::WrapVisualFlag::End : Scintilla::WrapVisualFlag::End);
        Scintilla().SetWrapVisualFlagsLocation(Scintilla::WrapVisualLocation::StartByText);
        Scintilla().SetMarginOptions(Scintilla::MarginOption::SubLineSelect);
        InvalidateUICommand(UI_INVALIDATIONS_PROPERTY, &UI_PKEY_BooleanValue);
        return true;
    }

    void BeforeLoad() override
    {
        int wrapIndent = static_cast<int>(CIniSettings::Instance().GetInt64(L"View", L"wrapmodeindent", 0));
        Scintilla().SetWrapStartIndent(wrapIndent ? max(1, Scintilla().TabWidth() / 2) : 0);
        Scintilla().SetWrapIndentMode(wrapIndent ? Scintilla::WrapIndentMode::Indent : Scintilla::WrapIndentMode::Fixed);
        Scintilla().SetWrapVisualFlags(wrapIndent ? Scintilla::WrapVisualFlag::Start | Scintilla::WrapVisualFlag::End : Scintilla::WrapVisualFlag::End);
        Scintilla().SetWrapVisualFlagsLocation(Scintilla::WrapVisualLocation::StartByText);
        Scintilla().SetMarginOptions(Scintilla::MarginOption::SubLineSelect);
        InvalidateUICommand(UI_INVALIDATIONS_PROPERTY, &UI_PKEY_BooleanValue);
    }

    UINT GetCmdId() override { return cmdLineWrapIndent; }

    HRESULT IUICommandHandlerUpdateProperty(REFPROPERTYKEY key, const PROPVARIANT* /*pPropVarCurrentValue*/, PROPVARIANT* pPropVarNewValue) override
    {
        if (UI_PKEY_BooleanValue == key)
        {
            return UIInitPropertyFromBoolean(UI_PKEY_BooleanValue, Scintilla().WrapStartIndent() > 0, pPropVarNewValue);
        }
        return E_NOTIMPL;
    }
};
