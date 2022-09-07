// This file is part of BowPad.
//
// Copyright (C) 2013-2017, 2021-2022 - Stefan Kueng
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

class CCmdOpen : public ICommand
{
public:
    CCmdOpen(void* obj)
        : ICommand(obj)
    {
    }

    ~CCmdOpen() override = default;

    bool Execute() override;
    UINT GetCmdId() override { return cmdOpen; }
};

class CCmdSave : public ICommand
{
public:
    CCmdSave(void* obj)
        : ICommand(obj)
    {
    }
    ~CCmdSave() override = default;

    bool Execute() override;
    UINT GetCmdId() override { return cmdSave; }
    void AfterInit() override
    {
        InvalidateUICommand(UI_INVALIDATIONS_STATE, nullptr);
    }

    void    ScintillaNotify(SCNotification* pScn) override;
    void    TabNotify(TBHDR* ptbHdr) override;

    HRESULT IUICommandHandlerUpdateProperty(REFPROPERTYKEY key, const PROPVARIANT* pPropVarCurrentValue, PROPVARIANT* pPropVarNewValue) override;
};

class CCmdSaveAll : public ICommand
{
public:
    CCmdSaveAll(void* obj)
        : ICommand(obj)
    {
    }
    ~CCmdSaveAll() override = default;

    bool Execute() override;
    UINT GetCmdId() override { return cmdSaveAll; }
    void AfterInit() override
    {
        InvalidateUICommand(UI_INVALIDATIONS_STATE, nullptr);
    }

    void    ScintillaNotify(SCNotification* pScn) override;
    void    TabNotify(TBHDR* ptbHdr) override;

    HRESULT IUICommandHandlerUpdateProperty(REFPROPERTYKEY key, const PROPVARIANT* pPropVarCurrentValue, PROPVARIANT* pPropVarNewValue) override;
};

class CCmdSaveAuto : public ICommand
{
public:
    CCmdSaveAuto(void* obj)
        : ICommand(obj)
    {
    }
    ~CCmdSaveAuto() override = default;

    bool    Execute() override;
    UINT    GetCmdId() override { return cmdSaveAuto; }

    HRESULT IUICommandHandlerUpdateProperty(REFPROPERTYKEY key, const PROPVARIANT* pPropVarCurrentValue, PROPVARIANT* pPropVarNewValue) override;
    void    TabNotify(TBHDR* ptbHdr) override;
    void    ScintillaNotify(SCNotification* pScn) override;

private:
    void Save() const;
};

class CCmdSaveAs : public ICommand
{
public:
    CCmdSaveAs(void* obj)
        : ICommand(obj)
    {
    }
    ~CCmdSaveAs() override = default;

    bool Execute() override;
    UINT GetCmdId() override { return cmdSaveAs; }
};

class CCmdReload : public ICommand
{
public:
    CCmdReload(void* obj)
        : ICommand(obj)
    {
    }
    ~CCmdReload() override = default;

    bool Execute() override
    {
        if (HasActiveDocument())
        {
            ReloadTab(GetActiveTabIndex());
        }
        return true;
    }
    UINT    GetCmdId() override { return cmdReload; }

    HRESULT IUICommandHandlerUpdateProperty(REFPROPERTYKEY key, const PROPVARIANT* pPropVarCurrentValue, PROPVARIANT* pPropVarNewValue) override;

    void    TabNotify(TBHDR* ptbHdr) override;
};

class CCmdFileDelete : public ICommand
{
public:
    CCmdFileDelete(void* obj)
        : ICommand(obj)
    {
    }
    ~CCmdFileDelete() override = default;

    bool    Execute() override;

    UINT    GetCmdId() override { return cmdFileDelete; }

    HRESULT IUICommandHandlerUpdateProperty(REFPROPERTYKEY key, const PROPVARIANT* pPropVarCurrentValue, PROPVARIANT* pPropVarNewValue) override;

    void    TabNotify(TBHDR* ptbHdr) override;
};
