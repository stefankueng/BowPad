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

class CCmdWhiteSpace : public ICommand
{
public:

    CCmdWhiteSpace(void * obj) : ICommand(obj)
    {
    }

    ~CCmdWhiteSpace(void)
    {
    }

    virtual bool Execute() 
    {
        ScintillaCall(SCI_SETVIEWWS, ScintillaCall(SCI_GETVIEWWS) ? 0 : 1);
        InvalidateUICommand(UI_INVALIDATIONS_PROPERTY, &UI_PKEY_BooleanValue);
        return true;
    }

    virtual UINT GetCmdId() { return cmdWhiteSpace; }

    virtual HRESULT IUICommandHandlerUpdateProperty( REFPROPERTYKEY key, const PROPVARIANT* /*ppropvarCurrentValue*/, PROPVARIANT* ppropvarNewValue ) 
    {
        if (UI_PKEY_BooleanValue == key)
        {
            return UIInitPropertyFromBoolean(UI_PKEY_BooleanValue, ScintillaCall(SCI_GETVIEWWS) > 0, ppropvarNewValue);
        }
        return E_NOTIMPL;
    }

};

