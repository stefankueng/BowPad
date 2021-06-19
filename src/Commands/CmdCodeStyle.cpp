// This file is part of BowPad.
//
// Copyright (C) 2013-2014, 2016, 2020-2021 - Stefan Kueng
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
#include "UnicodeUtils.h"
#include "AppUtils.h"
#include "LexStyles.h"

namespace
{
std::vector<std::wstring> langs;
}

HRESULT CCmdCodeStyle::IUICommandHandlerUpdateProperty(REFPROPERTYKEY key, const PROPVARIANT* pPropVarCurrentValue, PROPVARIANT* pPropVarNewValue)
{
    HRESULT hr = E_FAIL;

    if (key == UI_PKEY_Categories)
    {
        IUICollectionPtr pCollection;
        hr = pPropVarCurrentValue->punkVal->QueryInterface(IID_PPV_ARGS(&pCollection));
        if (CAppUtils::FailedShowMessage(hr))
            return hr;
        pCollection->Clear();
        if (langs.empty())
        {
            auto ls = CLexStyles::Instance().GetLanguages();
            for (const auto& l : ls)
            {
                if (!CLexStyles::Instance().IsLanguageHidden(l))
                    langs.push_back(l);
            }
        }

        for (wchar_t i = 'A'; i <= 'Z'; ++i)
        {
            // Create a property set for the category.
            CPropertySet* pCat = nullptr;
            hr                 = CPropertySet::CreateInstance(&pCat);
            if (FAILED(hr))
            {
                return hr;
            }

            wchar_t sName[2] = {i, L'\0'};
            // Initialize the property set with the label that was just loaded and a category id of 0.
            pCat->InitializeCategoryProperties(sName, i - 'A');
            pCollection->Add(pCat);
            pCat->Release();
        }
    }
    else if (key == UI_PKEY_ItemsSource)
    {
        IUICollectionPtr pCollection;
        hr = pPropVarCurrentValue->punkVal->QueryInterface(IID_PPV_ARGS(&pCollection));
        if (CAppUtils::FailedShowMessage(hr))
            return hr;

        pCollection->Clear();
        if (langs.empty())
        {
            auto ls = CLexStyles::Instance().GetLanguages();
            for (const auto& l : ls)
            {
                if (!CLexStyles::Instance().IsLanguageHidden(l))
                    langs.push_back(l);
            }
        }

        // Not a concern if it fails, just show the list without images.
        CAppUtils::FailedShowMessage(hr);
        // populate the dropdown with the code pages
        for (const auto& lang : langs)
        {
            int catId = lang.c_str()[0] - 'A';
            CAppUtils::AddStringItem(pCollection, lang.c_str(), catId, EMPTY_IMAGE);
        }
        hr = S_OK;
        InvalidateUICommand(UI_INVALIDATIONS_PROPERTY, &UI_PKEY_SelectedItem);
        InvalidateUICommand(UI_INVALIDATIONS_VALUE, &UI_PKEY_SelectedItem);
    }
    else if (key == UI_PKEY_SelectedItem)
    {
        if (HasActiveDocument())
        {
            const auto& doc     = GetActiveDocument();
            auto        docLang = CUnicodeUtils::StdGetUnicode(doc.GetLanguage());
            hr                  = S_FALSE;
            for (size_t i = 0; i < langs.size(); ++i)
            {
                if (langs[i] == docLang)
                {
                    hr = UIInitPropertyFromUInt32(UI_PKEY_SelectedItem, static_cast<UINT>(i), pPropVarNewValue);
                    break;
                }
            }
        }
    }
    return hr;
}

HRESULT CCmdCodeStyle::IUICommandHandlerExecute(UI_EXECUTIONVERB verb, const PROPERTYKEY* key, const PROPVARIANT* pPropVarValue, IUISimplePropertySet* /*pCommandExecutionProperties*/)
{
    HRESULT hr = E_FAIL;

    if (verb == UI_EXECUTIONVERB_EXECUTE)
    {
        if (key && *key == UI_PKEY_SelectedItem)
        {
            UINT selected = 0;
            hr            = UIPropertyToUInt32(*key, *pPropVarValue, &selected);
            if (HasActiveDocument())
            {
                InvalidateUICommand(cmdFunctions, UI_INVALIDATIONS_PROPERTY, &UI_PKEY_Enabled);
                auto& doc  = GetModActiveDocument();
                auto  lang = CUnicodeUtils::StdGetUTF8(langs[selected]);
                SetupLexerForLang(lang);
                CLexStyles::Instance().SetLangForPath(doc.m_path, lang);
                // set the language last, so that the OnLanguageChanged events happen last:
                // otherwise the SetLangForPath() invalidates the LanguageData pointers after
                // commands re-evaluated those!
                doc.SetLanguage(lang);
                UpdateStatusBar(true);
            }
            hr = S_OK;
        }
    }
    return hr;
}

void CCmdCodeStyle::TabNotify(TBHDR* ptbHdr)
{
    if (ptbHdr->hdr.code == TCN_SELCHANGE)
    {
        InvalidateUICommand(UI_INVALIDATIONS_PROPERTY, &UI_PKEY_Enabled);
        InvalidateUICommand(UI_INVALIDATIONS_PROPERTY, &UI_PKEY_SelectedItem);
        InvalidateUICommand(UI_INVALIDATIONS_VALUE, &UI_PKEY_SelectedItem);
    }
}

void CCmdCodeStyle::OnPluginNotify(UINT cmdId, const std::wstring& /*pluginName*/, LPARAM /*data*/)
{
    if (cmdId == cmdStyleConfigurator)
    {
        langs.clear();
        InvalidateUICommand(UI_INVALIDATIONS_PROPERTY, &UI_PKEY_SelectedItem);
        InvalidateUICommand(UI_INVALIDATIONS_VALUE, &UI_PKEY_SelectedItem);
        InvalidateUICommand(UI_INVALIDATIONS_PROPERTY, &UI_PKEY_ItemsSource);
        InvalidateUICommand(UI_INVALIDATIONS_PROPERTY, &UI_PKEY_Categories);
    }
}
