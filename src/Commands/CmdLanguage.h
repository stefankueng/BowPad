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

class CCmdLanguage : public ICommand
{
public:
    CCmdLanguage(void* obj)
        : ICommand(obj)
    {
    }

    ~CCmdLanguage() override = default;

    bool    Execute() override { return false; }
    UINT    GetCmdId() override { return cmdLanguage; }
    bool    IsItemsSourceCommand() override { return true; }

    HRESULT IUICommandHandlerUpdateProperty(REFPROPERTYKEY key, const PROPVARIANT* pPropVarCurrentValue, PROPVARIANT* pPropVarNewValue) override;

    HRESULT IUICommandHandlerExecute(UI_EXECUTIONVERB verb, const PROPERTYKEY* key, const PROPVARIANT* pPropVarValue, IUISimplePropertySet* pCommandExecutionProperties) override;
};
