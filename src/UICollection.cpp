// This file is part of BowPad.
//
// Copyright (C) 2021 - Stefan Kueng
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
#include "UICollection.h"
#include <UIRibbon.h>
#include <UIRibbonPropertyHelpers.h>

CUICollection::CUICollection()
    : m_cRef(1)

{
}

CUICollection::~CUICollection()
{
}

HRESULT __stdcall CUICollection::QueryInterface(REFIID riid, void** ppvObject)
{
    if (!ppvObject)
        return E_POINTER;

    if (riid == __uuidof(IUnknown))
        *ppvObject = static_cast<IUnknown*>(this);
    else if (riid == __uuidof(IUICollection))
        *ppvObject = static_cast<IUICollection*>(this);
    else
    {
        *ppvObject = nullptr;
        return E_NOINTERFACE;
    }

    AddRef();
    return S_OK;
}

ULONG __stdcall CUICollection::AddRef(void)
{
    return InterlockedIncrement(&m_cRef);
}

ULONG __stdcall CUICollection::Release(void)
{
    LONG cRef = InterlockedDecrement(&m_cRef);
    if (cRef == 0)
        delete this;

    return cRef;
}

HRESULT __stdcall CUICollection::GetCount(UINT32* count)
{
    if (!count)
        return E_POINTER;

    if (count)
        *count = (UINT32)m_data.size();
    return S_OK;
}

HRESULT __stdcall CUICollection::GetItem(UINT32 index, IUnknown** item)
{
    if (index >= m_data.size())
        return E_INVALIDARG;
    *item = m_data[index];
    return S_OK;
}

HRESULT __stdcall CUICollection::Add(IUnknown* item)
{
    if (!item)
        return E_POINTER;
    item->AddRef();
    m_data.push_back(item);
    return S_OK;
}

HRESULT __stdcall CUICollection::Insert(UINT32 index, IUnknown* item)
{
    if (!item)
        return E_POINTER;
    if (index >= m_data.size())
        return E_INVALIDARG;
    item->AddRef();
    m_data.insert(m_data.begin() + index, item);
    return S_OK;
}

HRESULT __stdcall CUICollection::RemoveAt(UINT32 index)
{
    if (index >= m_data.size())
        return E_INVALIDARG;
    m_data.erase(m_data.begin() + index);
    return S_OK;
}

HRESULT __stdcall CUICollection::Replace(UINT32 indexReplaced, IUnknown* itemReplaceWith)
{
    if (!itemReplaceWith)
        return E_POINTER;
    if (indexReplaced >= m_data.size())
        return E_INVALIDARG;
    itemReplaceWith->AddRef();
    m_data[indexReplaced] = itemReplaceWith;
    return S_OK;
}

HRESULT __stdcall CUICollection::Clear(void)
{
    m_data.clear();
    return S_OK;
}
