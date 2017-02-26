// This file is part of BowPad.
//
// Copyright (C) 2013-2014, 2016-2017 - Stefan Kueng
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
    }

    ~CCmdVerticalEdge() = default;

    void AfterInit() override
    {
        int ve = (int)CIniSettings::Instance().GetInt64(L"View", L"verticaledge", 0);
        ScintillaCall(SCI_SETEDGECOLUMN, ve);
        ScintillaCall(SCI_SETEDGEMODE, ve ? EDGE_LINE : EDGE_NONE);
        InvalidateUICommand(UI_INVALIDATIONS_PROPERTY, &UI_PKEY_DecimalValue);
    }
    bool Execute() override { return true; }

    UINT GetCmdId() override { return cmdVerticalEdge; }

    HRESULT IUICommandHandlerUpdateProperty(REFPROPERTYKEY key, const PROPVARIANT* /*ppropvarCurrentValue*/, PROPVARIANT* ppropvarNewValue) override
    {
        HRESULT hr = E_FAIL;
        // Set the minimum value
        if (IsEqualPropertyKey(key, UI_PKEY_MinValue))
        {
            DECIMAL decout;
            VarDecFromI4(0, &decout);
            hr = UIInitPropertyFromDecimal(UI_PKEY_DecimalValue, decout, ppropvarNewValue);
        }
        // Set the maximum value
        else if (IsEqualPropertyKey(key, UI_PKEY_MaxValue))
        {
            DECIMAL decout;
            VarDecFromI4(500, &decout);
            hr = UIInitPropertyFromDecimal(UI_PKEY_DecimalValue, decout, ppropvarNewValue);
        }
        // Set the increment
        else if (IsEqualPropertyKey(key, UI_PKEY_Increment))
        {
            DECIMAL decout;
            VarDecFromI4(1, &decout);
            hr = UIInitPropertyFromDecimal(UI_PKEY_DecimalValue, decout, ppropvarNewValue);
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
            DECIMAL decout;
            VarDecFromI4(ve, &decout);
            hr = UIInitPropertyFromDecimal(UI_PKEY_DecimalValue, decout, ppropvarNewValue);
        }
        return hr;
    }

    HRESULT IUICommandHandlerExecute(UI_EXECUTIONVERB /*verb*/, const PROPERTYKEY* /*key*/, const PROPVARIANT* ppropvarValue, IUISimplePropertySet* /*pCommandExecutionProperties*/) override
    {
        ScintillaCall(SCI_SETEDGECOLUMN, ppropvarValue->intVal);
        ScintillaCall(SCI_SETEDGEMODE, ppropvarValue->intVal ? EDGE_LINE : EDGE_NONE);
        CIniSettings::Instance().SetInt64(L"View", L"verticaledge", ppropvarValue->intVal);
        return S_OK;
    }
};
