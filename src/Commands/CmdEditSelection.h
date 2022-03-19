// This file is part of BowPad.
//
// Copyright (C) 2020-2022 - Stefan Kueng
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

class CCmdEditSelection : public ICommand
{
public:
    CCmdEditSelection(void* obj)
        : ICommand(obj)
    {
    }

    ~CCmdEditSelection() override = default;

    bool Execute() override;

    UINT GetCmdId() override { return cmdEditSelection; }

    void    ScintillaNotify(SCNotification* pScn) override;
    HRESULT IUICommandHandlerUpdateProperty(REFPROPERTYKEY key, const PROPVARIANT* pPropVarCurrentValue, PROPVARIANT* pPropVarNewValue) override;
};

class CCmdEditEndOfSelectedLines : public ICommand
{
public:
    CCmdEditEndOfSelectedLines(void* obj)
        : ICommand(obj)
    {
    }

    ~CCmdEditEndOfSelectedLines() override = default;

    bool    Execute() override;

    UINT    GetCmdId() override { return cmdEditEndOfSelectedLines; }
};
