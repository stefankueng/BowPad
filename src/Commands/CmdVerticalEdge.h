// This file is part of BowPad.
//
// Copyright (C) 2013-2014, 2016-2017, 2021 - Stefan Kueng
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
    CCmdVerticalEdge(void* obj)
        : ICommand(obj)
    {
    }

    ~CCmdVerticalEdge() = default;

    void AfterInit() override
    {
        int ve = static_cast<int>(CIniSettings::Instance().GetInt64(L"View", L"verticaledge", 0));
        Scintilla().SetEdgeColumn(ve);
        Scintilla().SetEdgeMode(ve ? Scintilla::EdgeVisualStyle::Line : Scintilla::EdgeVisualStyle::None);
        InvalidateUICommand(UI_INVALIDATIONS_PROPERTY, &UI_PKEY_DecimalValue);
    }
    bool Execute() override { return true; }

    UINT GetCmdId() override { return cmdVerticalEdge; }

    HRESULT IUICommandHandlerUpdateProperty(REFPROPERTYKEY key, const PROPVARIANT* /*pPropVarCurrentValue*/, PROPVARIANT* pPropVarNewValue) override
    {
        HRESULT hr = E_FAIL;
        // Set the minimum value
        if (IsEqualPropertyKey(key, UI_PKEY_MinValue))
        {
            DECIMAL decOut;
            VarDecFromI4(0, &decOut);
            hr = UIInitPropertyFromDecimal(UI_PKEY_DecimalValue, decOut, pPropVarNewValue);
        }
        // Set the maximum value
        else if (IsEqualPropertyKey(key, UI_PKEY_MaxValue))
        {
            DECIMAL decOut;
            VarDecFromI4(500, &decOut);
            hr = UIInitPropertyFromDecimal(UI_PKEY_DecimalValue, decOut, pPropVarNewValue);
        }
        // Set the increment
        else if (IsEqualPropertyKey(key, UI_PKEY_Increment))
        {
            DECIMAL decOut;
            VarDecFromI4(1, &decOut);
            hr = UIInitPropertyFromDecimal(UI_PKEY_DecimalValue, decOut, pPropVarNewValue);
        }
        // Set the number of decimal places
        else if (IsEqualPropertyKey(key, UI_PKEY_DecimalPlaces))
        {
            hr = InitPropVariantFromUInt32(0, pPropVarNewValue);
        }
        // Set the initial value
        else if (IsEqualPropertyKey(key, UI_PKEY_DecimalValue))
        {
            int     ve = static_cast<int>(CIniSettings::Instance().GetInt64(L"View", L"verticaledge", 0));
            DECIMAL decOut;
            VarDecFromI4(ve, &decOut);
            hr = UIInitPropertyFromDecimal(UI_PKEY_DecimalValue, decOut, pPropVarNewValue);
        }
        return hr;
    }

    HRESULT IUICommandHandlerExecute(UI_EXECUTIONVERB /*verb*/, const PROPERTYKEY* /*key*/, const PROPVARIANT* pPropVarValue, IUISimplePropertySet* /*pCommandExecutionProperties*/) override
    {
        Scintilla().SetEdgeColumn(pPropVarValue->intVal);
        Scintilla().SetEdgeMode(pPropVarValue->intVal ? Scintilla::EdgeVisualStyle::Line : Scintilla::EdgeVisualStyle::None);
        CIniSettings::Instance().SetInt64(L"View", L"verticaledge", pPropVarValue->intVal);
        return S_OK;
    }
};
