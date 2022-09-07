// This file is part of BowPad.
//
// Copyright (C) 2013-2014, 2016-2017, 2021-2022 - Stefan Kueng
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

class CCmdPrevNext : public ICommand
{
public:
    CCmdPrevNext(void* obj)
        : ICommand(obj)
    {
    }

    ~CCmdPrevNext() override = default;

    bool Execute() override { return false; }
    UINT GetCmdId() override { return cmdPrevNext; }

    void ScintillaNotify(SCNotification* pScn) override;

    void TabNotify(TBHDR* ptbHdr) override;
    void OnDocumentClose(DocID id) override;
};

class CCmdPrevious : public ICommand
{
public:
    CCmdPrevious(void* obj)
        : ICommand(obj)
    {
    }

    ~CCmdPrevious() override = default;

    bool    Execute() override;
    UINT    GetCmdId() override { return cmdPrevious; }

    HRESULT IUICommandHandlerUpdateProperty(
        REFPROPERTYKEY key, const PROPVARIANT* pPropVarCurrentValue, PROPVARIANT* pPropVarNewValue) override;
};

class CCmdNext : public ICommand
{
public:
    CCmdNext(void* obj)
        : ICommand(obj)
    {
    }

    ~CCmdNext() override = default;

    bool    Execute() override;
    UINT    GetCmdId() override { return cmdNext; }

    HRESULT IUICommandHandlerUpdateProperty(
        REFPROPERTYKEY key, const PROPVARIANT* pPropVarCurrentValue, PROPVARIANT* pPropVarNewValue) override;
};
