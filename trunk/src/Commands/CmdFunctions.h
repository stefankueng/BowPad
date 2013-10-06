// This file is part of BowPad.
//
// Copyright (C) 2013 - Stefan Kueng
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

#include <string>
#include <vector>


class CCmdFunctions : public ICommand
{
public:

    CCmdFunctions(void * obj) : ICommand(obj)
    {
    }

    ~CCmdFunctions(void)
    {
    }

    virtual bool Execute() { return false; }
    virtual UINT GetCmdId() { return cmdFunctions; }


    virtual HRESULT IUICommandHandlerUpdateProperty( REFPROPERTYKEY key, const PROPVARIANT* ppropvarCurrentValue, PROPVARIANT* ppropvarNewValue );

    virtual HRESULT IUICommandHandlerExecute( UI_EXECUTIONVERB verb, const PROPERTYKEY* key, const PROPVARIANT* ppropvarValue, IUISimplePropertySet* pCommandExecutionProperties );

    virtual void TabNotify( TBHDR * ptbhdr );

    virtual void ScintillaNotify( Scintilla::SCNotification * pScn );

};

