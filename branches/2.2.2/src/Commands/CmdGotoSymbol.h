// This file is part of BowPad.
//
// Copyright (C) 2016 - Stefan Kueng
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

class CCmdGotoSymbol : public ICommand
{
public:

    CCmdGotoSymbol(void* obj);
    ~CCmdGotoSymbol();

    bool Execute() override;

    UINT GetCmdId() override { return cmdGotoSymbol; }

    HRESULT IUICommandHandlerUpdateProperty(REFPROPERTYKEY key, const PROPVARIANT* ppropvarCurrentValue, PROPVARIANT* ppropvarNewValue) override;
    void TabNotify(TBHDR* ptbhdr) override;

};
