// This file is part of BowPad.
//
// Copyright (C) 2013-2014 - Stefan Kueng
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
#include "AppUtils.h"
#include "ResString.h"

#include <vector>
#include <tuple>

//                 codepage  BOM   name          category
std::vector<std::tuple<UINT, bool, std::wstring, int>>  codepages;

BOOL CALLBACK CodePageEnumerator(LPTSTR lpCodePageString)
{
    if (codepages.empty())
    {
        // insert the main encodings
        codepages.push_back(std::make_tuple(GetACP(), false, L"ANSI", 0));
        codepages.push_back(std::make_tuple(CP_UTF8, false, L"UTF-8", 0));
        codepages.push_back(std::make_tuple(CP_UTF8, true, L"UTF-8 BOM", 0));
        codepages.push_back(std::make_tuple(1200, true, L"UTF-16 Little Endian", 0));
        codepages.push_back(std::make_tuple(1201, true, L"UTF-16 Big Endian", 0));
        codepages.push_back(std::make_tuple(12000, true, L"UTF-32 Little Endian", 0));
        codepages.push_back(std::make_tuple(12001, true, L"UTF-32 Big Endian", 0));
    }
    UINT codepage = _wtoi(lpCodePageString);
    switch (codepage)
    {
    case 1200:
    case 1201:
    case 12000:
    case 12001:
    case CP_UTF8:
        break;
    default:
        {
            CPINFOEX cpex = {0};
            GetCPInfoEx(codepage, 0, &cpex);
            if (cpex.CodePageName[0])
            {
                std::wstring name = cpex.CodePageName;
                name = name.substr(name.find_first_of(' ')+1);
                size_t pos = name.find_first_of('(');
                if (pos != std::wstring::npos)
                    name.erase(pos, 1);
                pos = name.find_last_of(')');
                if (pos != std::wstring::npos)
                    name.erase(pos, 1);
                CStringUtils::trim(name);
                codepages.push_back(std::make_tuple(codepage, false, std::move(name), 1));
            }
        }
        break;
    }
    return TRUE;
}


HRESULT CCmdLoadAsEncoded::IUICommandHandlerUpdateProperty( REFPROPERTYKEY key, const PROPVARIANT* ppropvarCurrentValue, PROPVARIANT* ppropvarNewValue )
{
    HRESULT hr = E_FAIL;

    if(key == UI_PKEY_Categories)
    {
        IUICollectionPtr pCollection;
        hr = ppropvarCurrentValue->punkVal->QueryInterface(IID_PPV_ARGS(&pCollection));
        if (FAILED(hr))
        {
            return hr;
        }

        // Create a property set for the main category.
        CPropertySet *pMain;
        hr = CPropertySet::CreateInstance(&pMain);
        if (FAILED(hr))
        {
            return hr;
        }

        // Load the label for the main category from the resource file.
        ResString sMain(hRes, IDS_ENCODING_MAIN);
        // Initialize the property set with the label that was just loaded and a category id of 0.
        pMain->InitializeCategoryProperties(sMain, 0);
        pCollection->Add(pMain);
        pMain->Release();


        // Create a property set for the codepages category.
        CPropertySet *pCodepages;
        hr = CPropertySet::CreateInstance(&pCodepages);
        if (FAILED(hr))
        {
            return hr;
        }

        ResString sCP(hRes, IDS_ENCODING_CODEPAGES);
        // Initialize the property set with the label that was just loaded and a category id of 0.
        pCodepages->InitializeCategoryProperties(sCP, 1);
        pCollection->Add(pCodepages);
        pCodepages->Release();

        hr = S_OK;
    }
    else if (key == UI_PKEY_ItemsSource)
    {
        IUICollectionPtr pCollection;
        hr = ppropvarCurrentValue->punkVal->QueryInterface(IID_PPV_ARGS(&pCollection));
        if (FAILED(hr))
        {
            return hr;
        }

        if (codepages.empty())
            EnumSystemCodePages(CodePageEnumerator, CP_INSTALLED);


        // Create an IUIImage from a resource id.
        IUIImagePtr pImg;
        IUIImageFromBitmapPtr pifbFactory;
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

        if (FAILED(hr))
        {
            return hr;
        }



        // populate the dropdown with the code pages
        for (auto it:codepages)
        {
            if ((std::get<1>(it)) && (std::get<2>(it).compare(L"UTF-8 BOM")==0))
                continue;

            CAppUtils::AddStringItem(pCollection, std::get<2>(it).c_str(), std::get<3>(it), pImg);
        }
        hr = S_OK;
    }
    else if (key == UI_PKEY_Enabled)
    {
        // only enabled if the current doc has a path!
        if (HasActiveDocument())
        {
            CDocument doc = GetActiveDocument();
            hr = UIInitPropertyFromBoolean(UI_PKEY_Enabled, !doc.m_path.empty(), ppropvarNewValue);
        }
    }
    else if (key == UI_PKEY_SelectedItem)
    {
        hr = S_FALSE;
        if (HasActiveDocument())
        {
            CDocument doc = GetActiveDocument();
            if ((doc.m_encoding == -1)||(doc.m_encoding == 0))
            {
                hr = UIInitPropertyFromUInt32(UI_PKEY_SelectedItem, (UINT)0, ppropvarNewValue);
                hr = S_OK;
            }
            else
            {
                for (size_t i = 0; i < codepages.size(); ++i)
                {
                    if ((int)std::get<0>(codepages[i]) == doc.m_encoding)
                    {
                        hr = UIInitPropertyFromUInt32(UI_PKEY_SelectedItem, (UINT)i, ppropvarNewValue);
                        hr = S_OK;
                        break;
                    }
                }
            }
        }
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
            ReloadTab(GetActiveTabIndex(), codepage);
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
        IUICollectionPtr pCollection;
        hr = ppropvarCurrentValue->punkVal->QueryInterface(IID_PPV_ARGS(&pCollection));
        if (FAILED(hr))
        {
            return hr;
        }

        // Create a property set for the main category.
        CPropertySet *pMain;
        hr = CPropertySet::CreateInstance(&pMain);
        if (FAILED(hr))
        {
            return hr;
        }

        // Load the label for the main category from the resource file.
        ResString sMain(hRes, IDS_ENCODING_MAIN);
        // Initialize the property set with the label that was just loaded and a category id of 0.
        pMain->InitializeCategoryProperties(sMain, 0);
        pCollection->Add(pMain);
        pMain->Release();


        // Create a property set for the codepages category.
        CPropertySet *pCodepages;
        hr = CPropertySet::CreateInstance(&pCodepages);
        if (FAILED(hr))
        {
            return hr;
        }

        ResString sCP(hRes, IDS_ENCODING_CODEPAGES);
        // Initialize the property set with the label that was just loaded and a category id of 0.
        pCodepages->InitializeCategoryProperties(sCP, 1);
        pCollection->Add(pCodepages);
        pCodepages->Release();

        hr = S_OK;
    }
    else if (key == UI_PKEY_ItemsSource)
    {
        IUICollectionPtr pCollection;
        hr = ppropvarCurrentValue->punkVal->QueryInterface(IID_PPV_ARGS(&pCollection));
        if (FAILED(hr))
        {
            return hr;
        }

        if (codepages.empty())
            EnumSystemCodePages(CodePageEnumerator, CP_INSTALLED);


        // Create an IUIImage from a resource id.
        IUIImagePtr pImg;
        IUIImageFromBitmapPtr pifbFactory;
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

        if (FAILED(hr))
        {
            return hr;
        }

        // populate the dropdown with the code pages
        for (auto it:codepages)
        {
            CAppUtils::AddStringItem(pCollection, std::get<2>(it).c_str(), std::get<3>(it), pImg);
        }
        hr = S_OK;
    }
    else if (key == UI_PKEY_SelectedItem)
    {
        hr = S_FALSE;
        if (HasActiveDocument())
        {
            CDocument doc = GetActiveDocument();
            if ((doc.m_encoding == -1)||(doc.m_encoding == 0))
            {
                hr = UIInitPropertyFromUInt32(UI_PKEY_SelectedItem, (UINT)0, ppropvarNewValue);
                hr = S_OK;
            }
            else
            {
                for (size_t i = 0; i < codepages.size(); ++i)
                {
                    if (((int)std::get<0>(codepages[i]) == doc.m_encoding)&&(std::get<1>(codepages[i]) == doc.m_bHasBOM))
                    {
                        hr = UIInitPropertyFromUInt32(UI_PKEY_SelectedItem, (UINT)i, ppropvarNewValue);
                        hr = S_OK;
                        break;
                    }
                }
            }
        }
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
            if (HasActiveDocument())
            {
                CDocument doc = GetActiveDocument();
                doc.m_encoding = codepage;
                doc.m_bHasBOM = std::get<1>(codepages[selected]);
                doc.m_bIsDirty = true;
                doc.m_bNeedsSaving = true;
                SetDocument(GetDocIdOfCurrentTab(), doc);
                // the next to calls are only here to trigger SCN_SAVEPOINTLEFT/SCN_SAVEPOINTREACHED messages
                ScintillaCall(SCI_ADDUNDOACTION, 0,0);
                ScintillaCall(SCI_UNDO);
                UpdateStatusBar(true);
            }
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
