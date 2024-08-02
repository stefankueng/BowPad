// This file is part of BowPad.
//
// Copyright (C) 2013-2014, 2016-2017, 2021-2022, 2024 - Stefan Kueng
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
#include "../Theme.h"

class CCmdDelete : public ICommand
{
public:
    CCmdDelete(void* obj)
        : ICommand(obj)
    {
    }

    ~CCmdDelete() override = default;

    bool Execute() override
    {
        Scintilla().Clear();
        return true;
    }

    UINT GetCmdId() override { return cmdDelete; }
};

class CCmdSelectAll : public ICommand
{
public:
    CCmdSelectAll(void* obj)
        : ICommand(obj)
    {
    }

    ~CCmdSelectAll() override = default;

    bool Execute() override
    {
        Scintilla().SelectAll();
        return true;
    }

    UINT GetCmdId() override { return cmdSelectAll; }
};

class CCmdGotoBrace : public ICommand
{
public:
    CCmdGotoBrace(void* obj)
        : ICommand(obj)
    {
    }

    ~CCmdGotoBrace() override = default;

    bool Execute() override
    {
        GotoBrace();
        return true;
    }

    UINT GetCmdId() override { return cmdGotoBrace; }
};

class CCmdThemeBase : public ICommand
{
public:
    CCmdThemeBase(void* obj);
    ~CCmdThemeBase() override = default;

    bool    Execute() override;

    void    AfterInit() override;

    HRESULT IUICommandHandlerUpdateProperty(REFPROPERTYKEY key, const PROPVARIANT* /*pPropVarCurrentValue*/, PROPVARIANT* pPropVarNewValue) override;

    virtual Theme GetTheme() const = 0;
};

class CCmdThemeDark : public CCmdThemeBase
{
public:
    CCmdThemeDark(void* obj)
        : CCmdThemeBase(obj) {}

    UINT GetCmdId() override { return cmdThemeDark; }
    Theme GetTheme() const override { return Theme::Dark; }
};
class CCmdThemeLight : public CCmdThemeBase
{
public:
    CCmdThemeLight(void* obj)
        : CCmdThemeBase(obj) {}

    UINT GetCmdId() override { return cmdThemeLight; }
    Theme GetTheme() const override { return Theme::Light; }
};
class CCmdThemeSystem : public CCmdThemeBase
{
public:
    CCmdThemeSystem(void* obj)
        : CCmdThemeBase(obj) {}

    UINT GetCmdId() override { return cmdThemeSystem; }
    Theme GetTheme() const override { return Theme::System; }
};

class CCmdConfigShortcuts : public ICommand
{
public:
    CCmdConfigShortcuts(void* obj)
        : ICommand(obj)
    {
    }

    ~CCmdConfigShortcuts() override = default;

    bool Execute() override;

    UINT GetCmdId() override { return cmdConfigShortcuts; }
};

class CCmdAutoBraces : public ICommand
{
public:
    CCmdAutoBraces(void* obj);

    ~CCmdAutoBraces() override = default;

    bool    Execute() override;

    UINT    GetCmdId() override { return cmdAutoBraces; }
    void    AfterInit() override;

    HRESULT IUICommandHandlerUpdateProperty(REFPROPERTYKEY key, const PROPVARIANT* /*pPropVarCurrentValue*/, PROPVARIANT* pPropVarNewValue) override;
};

class CCmdViewFileTree : public ICommand
{
public:
    CCmdViewFileTree(void* obj);

    ~CCmdViewFileTree() override = default;

    bool    Execute() override;

    UINT    GetCmdId() override { return cmdFileTree; }
    void    AfterInit() override;

    HRESULT IUICommandHandlerUpdateProperty(REFPROPERTYKEY key, const PROPVARIANT* /*pPropVarCurrentValue*/, PROPVARIANT* pPropVarNewValue) override;
};

class CCmdWriteProtect : public ICommand
{
public:
    CCmdWriteProtect(void* obj);
    ~CCmdWriteProtect() override = default;

    bool    Execute() override;

    UINT    GetCmdId() override { return cmdWriteProtect; }
    void    AfterInit() override;

    HRESULT IUICommandHandlerUpdateProperty(REFPROPERTYKEY key, const PROPVARIANT* /*pPropVarCurrentValue*/, PROPVARIANT* pPropVarNewValue) override;
    void    TabNotify(TBHDR* ptbHdr) override;
    void    ScintillaNotify(SCNotification* pScn) override;
};

class CCmdAutoComplete : public ICommand
{
public:
    CCmdAutoComplete(void* obj);

    ~CCmdAutoComplete() override = default;

    bool    Execute() override;

    UINT    GetCmdId() override { return cmdAutocomplete; }
    void    AfterInit() override;

    HRESULT IUICommandHandlerUpdateProperty(REFPROPERTYKEY key, const PROPVARIANT* /*pPropVarCurrentValue*/, PROPVARIANT* pPropVarNewValue) override;
};
