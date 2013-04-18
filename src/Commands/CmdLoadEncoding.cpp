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
#include "CmdLoadEncoding.h"
#include "PropertySet.h"
#include "BowPad.h"
#include "StringUtils.h"

#include <vector>
#include <tuple>

std::vector<std::tuple<UINT, std::wstring>>  codepages;

BOOL CALLBACK CodePageEnumerator(LPTSTR lpCodePageString)
{
    UINT codepage = _wtoi(lpCodePageString);
    CPINFOEX cpex = {0};
    GetCPInfoEx(codepage, 0, &cpex);
    if (cpex.CodePageName[0])
    {
        std::wstring name = cpex.CodePageName;
        name = name.substr(name.find_first_of(' '));
        size_t pos = name.find_first_of('(');
        if (pos != std::wstring::npos)
            name.erase(pos, 1);
        pos = name.find_last_of(')');
        if (pos != std::wstring::npos)
            name.erase(pos, 1);
        CStringUtils::trim(name);
        codepages.push_back(std::make_tuple(codepage, name));
    }
    return TRUE;
}


HRESULT CCmdLoadAsEncoded::IUICommandHandlerUpdateProperty( REFPROPERTYKEY key, const PROPVARIANT* ppropvarCurrentValue, PROPVARIANT* ppropvarNewValue )
{
    HRESULT hr = E_FAIL;

    if(key == UI_PKEY_Categories)
    {
        // A return value of S_FALSE or E_NOTIMPL will result in a gallery with no categories.
        // If you return any error other than E_NOTIMPL, the contents of the gallery will not display.
        hr = S_FALSE;
    }
    else if (key == UI_PKEY_ItemsSource)
    {
        IUICollection* pCollection;
        hr = ppropvarCurrentValue->punkVal->QueryInterface(IID_PPV_ARGS(&pCollection));
        if (FAILED(hr))
        {
            return hr;
        }

        if (codepages.empty())
            EnumSystemCodePages(CodePageEnumerator, CP_INSTALLED);


        // Create an IUIImage from a resource id.
        IUIImage * pImg = nullptr;
        IUIImageFromBitmap * pifbFactory = nullptr;
        hr = CoCreateInstance(CLSID_UIRibbonImageFromBitmapFactory, NULL, CLSCTX_ALL, IID_PPV_ARGS(&pifbFactory));
        if (FAILED(hr))
        {
            return hr;
        }

        // Load the bitmap from the resource file.
        HBITMAP hbm = (HBITMAP) LoadImage(GetModuleHandle(NULL), MAKEINTRESOURCE(IDB_EMPTY), IMAGE_BITMAP, 0, 0, LR_CREATEDIBSECTION);
        if (hbm)
        {
            // Use the factory implemented by the framework to produce an IUIImage.
            hr = pifbFactory->CreateImage(hbm, UI_OWNERSHIP_TRANSFER, &pImg);
            if (FAILED(hr))
            {
                DeleteObject(hbm);
            }
        }
        pifbFactory->Release();

        if (FAILED(hr))
        {
            pCollection->Release();
            return hr;
        }



        // populate the dropdown with the code pages
        for (auto it:codepages)
        {
            // Create a new property set for each item.
            CPropertySet* pItem;
            hr = CPropertySet::CreateInstance(&pItem);
            if (FAILED(hr))
            {
                pCollection->Release();
                return hr;
            }

            pItem->InitializeItemProperties(pImg, std::get<1>(it).c_str(), UI_COLLECTION_INVALIDINDEX);

            // Add the newly-created property set to the collection supplied by the framework.
            pCollection->Add(pItem);

            pItem->Release();
        }
        pImg->Release();
        pCollection->Release();
        hr = S_OK;
    }
    else if (key == UI_PKEY_Enabled)
    {
        // only enabled if the current doc has a path!
        CDocument doc = GetDocument(GetCurrentTabIndex());
        hr = UIInitPropertyFromBoolean(UI_PKEY_Enabled, !doc.m_path.empty(), ppropvarNewValue);
    }
    else if (key == UI_PKEY_SelectedItem)
    {
        CDocument doc = GetDocument(GetCurrentTabIndex());
        hr = S_FALSE;
        for (size_t i = 0; i < codepages.size(); ++i)
        {
            if ((int)std::get<0>(codepages[i]) == doc.m_encoding)
            {
                hr = UIInitPropertyFromUInt32(UI_PKEY_SelectedItem, (UINT)i, ppropvarNewValue);
                break;
            }
        }
        hr = S_FALSE;
    }
    return hr;
}

HRESULT CCmdLoadAsEncoded::IUICommandHandlerExecute( UI_EXECUTIONVERB verb, const PROPERTYKEY* key, const PROPVARIANT* ppropvarValue, IUISimplePropertySet* /*pCommandExecutionProperties*/ )
{
    HRESULT hr = E_FAIL;

    if (verb == UI_EXECUTIONVERB_EXECUTE)
    {
        if ( key && *key == UI_PKEY_SelectedItem)
        {
            UINT selected;
            hr = UIPropertyToUInt32(*key, *ppropvarValue, &selected);
            UINT codepage = std::get<0>(codepages[selected]);
            ReloadTab(GetCurrentTabIndex(), codepage);
            hr = S_OK;
        }
    }
    return hr;
}

void CCmdLoadAsEncoded::TabNotify( TBHDR * ptbhdr )
{
    if (ptbhdr->hdr.code == TCN_SELCHANGE)
    {
        InvalidateUICommand(UI_INVALIDATIONS_PROPERTY, &UI_PKEY_Enabled);
        InvalidateUICommand(UI_INVALIDATIONS_PROPERTY, &UI_PKEY_SelectedItem);
    }
}

HRESULT CCmdConvertEncoding::IUICommandHandlerUpdateProperty( REFPROPERTYKEY key, const PROPVARIANT* ppropvarCurrentValue, PROPVARIANT* ppropvarNewValue )
{
    HRESULT hr = E_FAIL;

    if(key == UI_PKEY_Categories)
    {
        // A return value of S_FALSE or E_NOTIMPL will result in a gallery with no categories.
        // If you return any error other than E_NOTIMPL, the contents of the gallery will not display.
        hr = S_FALSE;
    }
    else if (key == UI_PKEY_ItemsSource)
    {
        IUICollection* pCollection;
        hr = ppropvarCurrentValue->punkVal->QueryInterface(IID_PPV_ARGS(&pCollection));
        if (FAILED(hr))
        {
            return hr;
        }

        if (codepages.empty())
            EnumSystemCodePages(CodePageEnumerator, CP_INSTALLED);


        // Create an IUIImage from a resource id.
        IUIImage * pImg = nullptr;
        IUIImageFromBitmap * pifbFactory = nullptr;
        hr = CoCreateInstance(CLSID_UIRibbonImageFromBitmapFactory, NULL, CLSCTX_ALL, IID_PPV_ARGS(&pifbFactory));
        if (FAILED(hr))
        {
            return hr;
        }

        // Load the bitmap from the resource file.
        HBITMAP hbm = (HBITMAP) LoadImage(GetModuleHandle(NULL), MAKEINTRESOURCE(IDB_EMPTY), IMAGE_BITMAP, 0, 0, LR_CREATEDIBSECTION);
        if (hbm)
        {
            // Use the factory implemented by the framework to produce an IUIImage.
            hr = pifbFactory->CreateImage(hbm, UI_OWNERSHIP_TRANSFER, &pImg);
            if (FAILED(hr))
            {
                DeleteObject(hbm);
            }
        }
        pifbFactory->Release();

        if (FAILED(hr))
        {
            pCollection->Release();
            return hr;
        }



        // populate the dropdown with the code pages
        for (auto it:codepages)
        {
            // Create a new property set for each item.
            CPropertySet* pItem;
            hr = CPropertySet::CreateInstance(&pItem);
            if (FAILED(hr))
            {
                pCollection->Release();
                return hr;
            }

            pItem->InitializeItemProperties(pImg, std::get<1>(it).c_str(), UI_COLLECTION_INVALIDINDEX);

            // Add the newly-created property set to the collection supplied by the framework.
            pCollection->Add(pItem);

            pItem->Release();
        }
        pImg->Release();
        pCollection->Release();
        hr = S_OK;
    }
    else if (key == UI_PKEY_SelectedItem)
    {
        CDocument doc = GetDocument(GetCurrentTabIndex());
        hr = S_FALSE;
        for (size_t i = 0; i < codepages.size(); ++i)
        {
            if ((int)std::get<0>(codepages[i]) == doc.m_encoding)
            {
                hr = UIInitPropertyFromUInt32(UI_PKEY_SelectedItem, (UINT)i, ppropvarNewValue);
                break;
            }
        }
        hr = S_FALSE;
    }
    return hr;
}

HRESULT CCmdConvertEncoding::IUICommandHandlerExecute( UI_EXECUTIONVERB verb, const PROPERTYKEY* key, const PROPVARIANT* ppropvarValue, IUISimplePropertySet* /*pCommandExecutionProperties*/ )
{
    HRESULT hr = E_FAIL;

    if (verb == UI_EXECUTIONVERB_EXECUTE)
    {
        if ( key && *key == UI_PKEY_SelectedItem)
        {
            UINT selected;
            hr = UIPropertyToUInt32(*key, *ppropvarValue, &selected);
            UINT codepage = std::get<0>(codepages[selected]);
            CDocument doc = GetDocument(GetCurrentTabIndex());
            doc.m_encoding = codepage;
            doc.m_bIsDirty = true;
            doc.m_bNeedsSaving = true;
            SetDocument(GetCurrentTabIndex(), doc);
            UpdateStatusBar(true);
            hr = S_OK;
        }
    }
    return hr;
}

void CCmdConvertEncoding::TabNotify( TBHDR * ptbhdr )
{
    if (ptbhdr->hdr.code == TCN_SELCHANGE)
    {
        InvalidateUICommand(UI_INVALIDATIONS_PROPERTY, &UI_PKEY_Enabled);
        InvalidateUICommand(UI_INVALIDATIONS_PROPERTY, &UI_PKEY_SelectedItem);
    }
}
