// This file is part of BowPad.
//
// Copyright (C) 2013, 2015-2016 - Stefan Kueng
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

const int MAX_RESOURCE_LENGTH = 256;
#include <uiribbon.h>

// The implementation of IUISimplePropertySet. This handles all of the properties used for the
// ItemsSource and Categories PKEYs and provides functions to set only the properties required
// for each type of gallery contents.
class CPropertySet : public IUISimplePropertySet
{
public:

    static HRESULT CreateInstance(CPropertySet **ppPropertySet);


    void InitializeCommandProperties(int categoryId, int commandId, UI_COMMANDTYPE commandType);

    void InitializeItemProperties(IUIImage *image, PCWSTR label, int categoryId);

    void InitializeCategoryProperties(PCWSTR label, int categoryId);

    STDMETHOD(GetValue)(REFPROPERTYKEY key, PROPVARIANT *ppropvar);

    // IUnknown
    IFACEMETHODIMP QueryInterface(REFIID iid, void** ppv);
    IFACEMETHODIMP_(ULONG) AddRef();
    IFACEMETHODIMP_(ULONG) Release();

private:
    CPropertySet()
        : m_categoryId(UI_COLLECTION_INVALIDINDEX)
        , m_pimgItem(nullptr)
        , m_commandId(-1)
        , m_commandType(UI_COMMANDTYPE_UNKNOWN)
        , m_cRef(1)
    {
        m_wszLabel[0] = L'\0';
    }

    ~CPropertySet()
    {
        if (m_pimgItem != nullptr)
        {
            m_pimgItem->Release();
        }
    }

    WCHAR           m_wszLabel[MAX_RESOURCE_LENGTH];    // Used for items and categories.
    int             m_categoryId;                       // Used for items, categories, and commands.
    IUIImage *      m_pimgItem;                         // Used for items only.
    int             m_commandId;                        // Used for commands only.
    UI_COMMANDTYPE  m_commandType;                      // Used for commands only.

    LONG            m_cRef;
};
