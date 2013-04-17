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
#include "stdafx.h"
#include "PropertySet.h"

#include <windows.h>
#include <strsafe.h>
#include <uiribbonpropertyhelpers.h>

void CPropertySet::InitializeCommandProperties(int categoryId, int commandId, UI_COMMANDTYPE commandType)
{
    m_categoryId = categoryId;
    m_commandId = commandId;
    m_commandType = commandType;
}

void CPropertySet::InitializeItemProperties(IUIImage *image, PCWSTR label, int categoryId)
{
    m_pimgItem = image;
    if (m_pimgItem)
    {
        m_pimgItem->AddRef();
    }
    StringCchCopyW(m_wszLabel, MAX_RESOURCE_LENGTH, label);
    m_categoryId = categoryId;
}

void CPropertySet::InitializeCategoryProperties(PCWSTR label, int categoryId)
{
    StringCchCopyW(m_wszLabel, MAX_RESOURCE_LENGTH, label);
    m_categoryId = categoryId;
}

STDMETHODIMP CPropertySet::GetValue(REFPROPERTYKEY key, PROPVARIANT *ppropvar)
{
    if (key == UI_PKEY_ItemImage)
    {
        if (m_pimgItem)
        {
            return UIInitPropertyFromImage(UI_PKEY_ItemImage, m_pimgItem, ppropvar);
        }
        return S_FALSE;
    }
    else if (key == UI_PKEY_Label)
    {
        return UIInitPropertyFromString(UI_PKEY_Label, m_wszLabel, ppropvar);
    }
    else if (key == UI_PKEY_CategoryId)
    {
        return UIInitPropertyFromUInt32(UI_PKEY_CategoryId, m_categoryId, ppropvar);
    }
    else if (key == UI_PKEY_CommandId)
    {
        if(m_commandId != -1)
        {
            return UIInitPropertyFromUInt32(UI_PKEY_CommandId, m_commandId, ppropvar);
        }
        return S_FALSE;
    }
    else if (key == UI_PKEY_CommandType)
    {
        return UIInitPropertyFromUInt32(UI_PKEY_CommandType, m_commandType, ppropvar);
    }
    return E_FAIL;
}

HRESULT CPropertySet::CreateInstance(CPropertySet **ppPropertySet)
{
    if (!ppPropertySet)
    {
        return E_POINTER;
    }

    *ppPropertySet = NULL;

    HRESULT hr = S_OK;

    CPropertySet* pPropertySet = new CPropertySet();

    if (pPropertySet == NULL)
    {
        hr = E_OUTOFMEMORY;
    }

    if (SUCCEEDED(hr))
    {
        *ppPropertySet = pPropertySet;
    }

    return hr;
}

// IUnknown methods.
STDMETHODIMP_(ULONG) CPropertySet::AddRef()
{
    return InterlockedIncrement(&m_cRef);
}

STDMETHODIMP_(ULONG) CPropertySet::Release()
{
    LONG cRef = InterlockedDecrement(&m_cRef);
    if (cRef == 0)
    {
        delete this;
    }

    return cRef;
}

STDMETHODIMP CPropertySet::QueryInterface(REFIID iid, void** ppv)
{
    if (!ppv)
    {
        return E_POINTER;
    }

    if (iid == __uuidof(IUnknown))
    {
        *ppv = static_cast<IUnknown*>(this);
    }
    else if (iid == __uuidof(IUISimplePropertySet))
    {
        *ppv = static_cast<IUISimplePropertySet*>(this);
    }
    else 
    {
        *ppv = NULL;
        return E_NOINTERFACE;
    }

    AddRef();
    return S_OK;
}
