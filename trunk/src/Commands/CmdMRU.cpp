// This file is part of BowPad.
//
// Copyright (C) 2014 - Stefan Kueng
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

#include "stdafx.h"
#include "CmdMRU.h"
#include "ICommand.h"
#include "BowPadUI.h"
#include "MRU.h"
#include "PathUtils.h"
#include "AppUtils.h"

HRESULT CCmdMRU::IUICommandHandlerUpdateProperty(REFPROPERTYKEY key, const PROPVARIANT* /*ppropvarCurrentValue*/, PROPVARIANT* ppropvarNewValue)
{
    if (UI_PKEY_RecentItems == key)
    {
        return CMRU::Instance().PopulateRibbonRecentItems(ppropvarNewValue);
    }
    return E_NOTIMPL;
}

HRESULT CCmdMRU::IUICommandHandlerExecute(UI_EXECUTIONVERB /*verb*/, const PROPERTYKEY* key, const PROPVARIANT* ppropvarValue, IUISimplePropertySet* pCommandExecutionProperties)
{
    HRESULT hr = E_NOTIMPL;
    if (*key == UI_PKEY_RecentItems)
    {
        if (ppropvarValue)
        {
            SAFEARRAY * psa = V_ARRAY(ppropvarValue);
            LONG lstart, lend;
            hr = SafeArrayGetLBound( psa, 1, &lstart );
            if (CAppUtils::FailedShowMessage(hr))
                return hr;
            hr = SafeArrayGetUBound( psa, 1, &lend );
            if (CAppUtils::FailedShowMessage(hr))
                return hr;
            IUISimplePropertySet ** data;
            hr = SafeArrayAccessData(psa,(void **)&data);
            for (LONG idx = lstart; idx <= lend; ++idx)
            {
                IUISimplePropertySet * ppset = (IUISimplePropertySet *)data[idx];
                if (ppset)
                {
                    PROPVARIANT var;
                    ppset->GetValue(UI_PKEY_LabelDescription, &var);
                    std::wstring path = var.bstrVal;
                    PropVariantClear(&var);
                    ppset->GetValue(UI_PKEY_Pinned, &var);
                    bool bPinned = VARIANT_TRUE==var.boolVal;
                    PropVariantClear(&var);
                    CMRU::Instance().PinPath(path, bPinned);
                }
            }
            hr = SafeArrayUnaccessData(psa);
            return hr;
        }
    }
    if (*key == UI_PKEY_SelectedItem)
    {
        if (pCommandExecutionProperties)
        {
            PROPVARIANT var;
            pCommandExecutionProperties->GetValue(UI_PKEY_LabelDescription, &var);
            std::wstring path = var.bstrVal;
            PropVariantClear(&var);
            CPathUtils::NormalizeFolderSeparators(path);
            OpenFile(path.c_str(), OpenFlags::AddToMRU);
            return S_OK;
        }
    }
    return E_NOTIMPL;
}
