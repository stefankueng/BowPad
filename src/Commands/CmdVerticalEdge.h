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

class CCmdVerticalEdge : public ICommand
{
public:

    CCmdVerticalEdge(void * obj) : ICommand(obj)
    {
        int ve = (int)CIniSettings::Instance().GetInt64(L"View", L"verticaledge", 0);
        ScintillaCall(SCI_SETEDGECOLUMN, ve);
        ScintillaCall(SCI_SETEDGEMODE, ve ? EDGE_LINE : EDGE_NONE);
        InvalidateUICommand(UI_INVALIDATIONS_PROPERTY, &UI_PKEY_DecimalValue);
    }

    ~CCmdVerticalEdge(void)
    {
    }

    virtual bool Execute() { return true; }

    virtual UINT GetCmdId() { return cmdVerticalEdge; }

    virtual HRESULT IUICommandHandlerUpdateProperty( REFPROPERTYKEY key, const PROPVARIANT* /*ppropvarCurrentValue*/, PROPVARIANT* ppropvarNewValue )
    {
        HRESULT hr = S_OK;
        // Set the minimum value
        if (IsEqualPropertyKey(key, UI_PKEY_MinValue))
        {
            ZeroMemory(ppropvarNewValue, sizeof(*ppropvarNewValue));
            ppropvarNewValue->vt = VT_DECIMAL;
            VarDecFromR8(0, &ppropvarNewValue->decVal);
            hr = S_OK;
        }
        // Set the maximum value
        else if (IsEqualPropertyKey(key, UI_PKEY_MaxValue))
        {
            ZeroMemory(ppropvarNewValue, sizeof(*ppropvarNewValue));
            ppropvarNewValue->vt = VT_DECIMAL;
            VarDecFromR8(500, &ppropvarNewValue->decVal);
            hr = S_OK;
        }
        // Set the increment
        else if (IsEqualPropertyKey(key, UI_PKEY_Increment))
        {
            ZeroMemory(ppropvarNewValue, sizeof(*ppropvarNewValue));
            ppropvarNewValue->vt = VT_DECIMAL;
            VarDecFromR8(1, &ppropvarNewValue->decVal);
            hr = S_OK;
        }
        // Set the number of decimal places
        else if (IsEqualPropertyKey(key, UI_PKEY_DecimalPlaces))
        {
            hr = InitPropVariantFromUInt32(0, ppropvarNewValue);
        }
        // Set the initial value
        else if (IsEqualPropertyKey(key, UI_PKEY_DecimalValue))
        {
            int ve = (int)CIniSettings::Instance().GetInt64(L"View", L"verticaledge", 0);
            ZeroMemory(ppropvarNewValue, sizeof(*ppropvarNewValue));
            ppropvarNewValue->vt = VT_DECIMAL;
            VarDecFromR8(ve, &ppropvarNewValue->decVal);
        }
        return hr;
    }

    virtual HRESULT IUICommandHandlerExecute( UI_EXECUTIONVERB /*verb*/, const PROPERTYKEY* /*key*/, const PROPVARIANT* ppropvarValue, IUISimplePropertySet* /*pCommandExecutionProperties*/ )
    {
        ScintillaCall(SCI_SETEDGECOLUMN, ppropvarValue->intVal);
        ScintillaCall(SCI_SETEDGEMODE, ppropvarValue->intVal ? EDGE_LINE : EDGE_NONE);
        CIniSettings::Instance().SetInt64(L"View", L"verticaledge", ppropvarValue->intVal);
        return S_OK;
    }
};
