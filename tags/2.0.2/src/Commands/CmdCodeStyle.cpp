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
#include "CmdCodeStyle.h"
#include "PropertySet.h"
#include "BowPad.h"
#include "StringUtils.h"
#include "AppUtils.h"
#include "LexStyles.h"

#include <vector>
#include <tuple>

std::vector<std::wstring> langs;

HRESULT CCmdCodeStyle::IUICommandHandlerUpdateProperty( REFPROPERTYKEY key, const PROPVARIANT* ppropvarCurrentValue, PROPVARIANT* ppropvarNewValue )
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

        if (langs.empty())
            langs = CLexStyles::Instance().GetLanguages();

        for (wchar_t i = 'A'; i <= 'Z'; ++i)
        {
            // Create a property set for the category.
            CPropertySet *pCat;
            hr = CPropertySet::CreateInstance(&pCat);
            if (FAILED(hr))
            {
                return hr;
            }

            wchar_t sName[2] = {0};
            sName[0] = i;
            // Initialize the property set with the label that was just loaded and a category id of 0.
            pCat->InitializeCategoryProperties(sName, i-'A');
            pCollection->Add(pCat);
            pCat->Release();
        }
    }
    else if (key == UI_PKEY_ItemsSource)
    {
        IUICollectionPtr pCollection;
        hr = ppropvarCurrentValue->punkVal->QueryInterface(IID_PPV_ARGS(&pCollection));
        if (FAILED(hr))
        {
            return hr;
        }

        if (langs.empty())
            langs = CLexStyles::Instance().GetLanguages();

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
        for (auto it:langs)
        {
            int catId = it.c_str()[0] - 'A';
            CAppUtils::AddStringItem(pCollection, it.c_str(), catId, pImg);
        }
        hr = S_OK;
    }
    else if (key == UI_PKEY_SelectedItem)
    {
        if (HasActiveDocument())
        {
            CDocument doc = GetActiveDocument();
            hr = S_FALSE;
            for (size_t i = 0; i < langs.size(); ++i)
            {
                if (langs[i] == doc.m_language)
                {
                    hr = UIInitPropertyFromUInt32(UI_PKEY_SelectedItem, (UINT)i, ppropvarNewValue);
                    break;
                }
            }
        }
    }
    return hr;
}

HRESULT CCmdCodeStyle::IUICommandHandlerExecute( UI_EXECUTIONVERB verb, const PROPERTYKEY* key, const PROPVARIANT* ppropvarValue, IUISimplePropertySet* /*pCommandExecutionProperties*/ )
{
    HRESULT hr = E_FAIL;

    if (verb == UI_EXECUTIONVERB_EXECUTE)
    {
        if ( key && *key == UI_PKEY_SelectedItem)
        {
            UINT selected;
            hr = UIPropertyToUInt32(*key, *ppropvarValue, &selected);
            if (HasActiveDocument())
            {
                InvalidateUICommand(cmdFunctions, UI_INVALIDATIONS_PROPERTY, &UI_PKEY_Enabled);
                CDocument doc = GetActiveDocument();
                doc.m_language = langs[selected];
                SetDocument(GetDocIdOfCurrentTab(), doc);
                SetupLexerForLang(doc.m_language);
                CLexStyles::Instance().SetLangForPath(doc.m_path, doc.m_language);
                UpdateStatusBar(true);
            }
            hr = S_OK;
        }
    }
    return hr;
}

void CCmdCodeStyle::TabNotify( TBHDR * ptbhdr )
{
    if (ptbhdr->hdr.code == TCN_SELCHANGE)
    {
        InvalidateUICommand(UI_INVALIDATIONS_PROPERTY, &UI_PKEY_Enabled);
        InvalidateUICommand(UI_INVALIDATIONS_PROPERTY, &UI_PKEY_SelectedItem);
    }
}

