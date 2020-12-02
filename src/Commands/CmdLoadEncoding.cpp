// This file is part of BowPad.
//
// Copyright (C) 2013-2017, 2020 - Stefan Kueng
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
#include <string>

extern IUIFramework *g_pFramework;

struct CodePageItem
{
    CodePageItem(UINT codepage, bool bom, const std::wstring& name, int category)
        : codepage(codepage), bom(bom), name(name), category(category) { }

    UINT codepage;
    bool bom;
    std::wstring name;
    int category;
};

namespace
{
// Don't change value, file data depends on them.
const int CP_CATEGORY_MAIN = 0;
const int CP_CATEGORY_MRU = 1;
const int CP_CATEGORY_CODEPAGES = 2;

std::vector<CodePageItem>  codepages;
std::vector<UINT>  mrucodepages;
};

BOOL CALLBACK CodePageEnumerator(LPTSTR lpCodePageString)
{
    if (codepages.empty())
    {
        // insert the main encodings
        codepages.push_back(CodePageItem(GetACP(), false, L"ANSI", CP_CATEGORY_MAIN));
        codepages.push_back(CodePageItem(CP_UTF8, false, L"UTF-8", CP_CATEGORY_MAIN));
        codepages.push_back(CodePageItem(CP_UTF8, true, L"UTF-8 BOM", CP_CATEGORY_MAIN));
        codepages.push_back(CodePageItem(1200, true, L"UTF-16 Little Endian", CP_CATEGORY_MAIN));
        codepages.push_back(CodePageItem(1201, true, L"UTF-16 Big Endian", CP_CATEGORY_MAIN));
        codepages.push_back(CodePageItem(12000, true, L"UTF-32 Little Endian", CP_CATEGORY_MAIN));
        codepages.push_back(CodePageItem(12001, true, L"UTF-32 Big Endian", CP_CATEGORY_MAIN));
        for (int i = 5; i >0; --i)
        {
            auto sKey = CStringUtils::Format(L"%02d", i);
            UINT cp = (UINT)CIniSettings::Instance().GetInt64(L"codepagemru", sKey.c_str(), 0);
            if (cp)
            {
                mrucodepages.push_back(cp);
            }
        }
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
            CPINFOEX cpex = { 0 };
            GetCPInfoEx(codepage, 0, &cpex);
            if (cpex.CodePageName[0])
            {
                std::wstring name = cpex.CodePageName;
                name = name.substr(name.find_first_of(' ') + 1);
                size_t pos = name.find_first_of('(');
                if (pos != std::wstring::npos)
                    name.erase(pos, 1);
                pos = name.find_last_of(')');
                if (pos != std::wstring::npos)
                    name.erase(pos, 1);
                CStringUtils::trim(name);
                codepages.emplace_back(codepage, false, name, CP_CATEGORY_CODEPAGES);
            }
        }
        break;
    }
    return TRUE;
}

HRESULT HandleCategories(const PROPVARIANT* ppropvarCurrentValue)
{
    IUICollectionPtr pCollection;
    auto hr = ppropvarCurrentValue->punkVal->QueryInterface(IID_PPV_ARGS(&pCollection));
    if (CAppUtils::FailedShowMessage(hr))
        return hr;

    pCollection->Clear();

    if (!mrucodepages.empty())
    {
        hr = CAppUtils::AddCategory(pCollection, CP_CATEGORY_MRU, IDS_ENCODING_MRU);
        if (FAILED(hr))
            return hr;
    }
    hr = CAppUtils::AddCategory(pCollection, CP_CATEGORY_MAIN, IDS_ENCODING_MAIN);
    if (FAILED(hr))
        return hr;
    hr = CAppUtils::AddCategory(pCollection, CP_CATEGORY_CODEPAGES, IDS_ENCODING_CODEPAGES);
    if (FAILED(hr))
        return hr;

    return S_OK;
}

HRESULT HandleItemsSource(const PROPVARIANT* ppropvarCurrentValue, bool ignoreUTF8BOM)
{
    IUICollectionPtr pCollection;
    auto hr = ppropvarCurrentValue->punkVal->QueryInterface(IID_PPV_ARGS(&pCollection));
    if (CAppUtils::FailedShowMessage(hr))
        return hr;
    pCollection->Clear();
    if (codepages.empty())
        EnumSystemCodePages(CodePageEnumerator, CP_INSTALLED);

    // populate the dropdown with the code pages
    for (const auto& cp : codepages)
    {
        if (ignoreUTF8BOM && cp.bom && cp.name.compare(L"UTF-8 BOM") == 0)
            continue;

        CAppUtils::AddStringItem(pCollection, cp.name.c_str(), cp.category, nullptr);
    }
    return S_OK;
}

HRESULT HandleSelectedItem(PROPVARIANT* ppropvarNewValue, int encoding, bool ignoreUTF8BOM)
{
    auto hr = S_FALSE;
    if ((encoding == -1) || (encoding == 0))
        hr = UIInitPropertyFromUInt32(UI_PKEY_SelectedItem, (UINT)0, ppropvarNewValue);
        // Return value unused, just set for debugging.
    else
    {
        int offset = 0;
        for (size_t i = 0; i < codepages.size(); ++i)
        {
            if (ignoreUTF8BOM && codepages[i].bom && codepages[i].name.compare(L"UTF-8 BOM") == 0)
            {
                offset = 1;
                continue;
            }

            if ((int)codepages[i].codepage == encoding)
            {
                hr = UIInitPropertyFromUInt32(UI_PKEY_SelectedItem, (UINT)(i - offset), ppropvarNewValue);
                // Return value unused, just set for debugging.
                break;
            }
        }
    }
    return S_OK;
}

void AddToMRU(const CodePageItem& cp)
{
    if (cp.category == CP_CATEGORY_MAIN)
        return; // ignore items from the main category for the MRU list

    // Erase the MRU item that might already exist
    for (auto it = mrucodepages.begin(); it != mrucodepages.end(); ++it)
    {
        if (cp.codepage == (*it))
        {
            mrucodepages.erase(it);
            break;
        }
    }
    // keep the MRU small, clear out old entries
    if (mrucodepages.size() >= 5)
        mrucodepages.erase(mrucodepages.begin());

    mrucodepages.push_back(cp.codepage);

    // go through all codepages and adjust the category
    for (auto& cpentry : codepages)
    {
        if (cpentry.category == CP_CATEGORY_MRU)
            cpentry.category = CP_CATEGORY_CODEPAGES;
    }
    int mruindex = 1;
    for (const auto& mru : mrucodepages)
    {
        auto sKey = CStringUtils::Format(L"%02d", mruindex++);
        CIniSettings::Instance().SetInt64(L"codepagemru", sKey.c_str(), mru);
        for (auto& cpentry : codepages)
        {
            if (cpentry.codepage == mru)
                cpentry.category = CP_CATEGORY_MRU;
        }
    }
    g_pFramework->InvalidateUICommand(cmdLoadAsEncoding, UI_INVALIDATIONS_PROPERTY, &UI_PKEY_Categories);
    g_pFramework->InvalidateUICommand(cmdLoadAsEncoding, UI_INVALIDATIONS_PROPERTY, &UI_PKEY_ItemsSource);
    g_pFramework->InvalidateUICommand(cmdConvertEncoding, UI_INVALIDATIONS_PROPERTY, &UI_PKEY_Categories);
    g_pFramework->InvalidateUICommand(cmdConvertEncoding, UI_INVALIDATIONS_PROPERTY, &UI_PKEY_ItemsSource);
}

HRESULT CCmdLoadAsEncoded::IUICommandHandlerUpdateProperty( REFPROPERTYKEY key, const PROPVARIANT* ppropvarCurrentValue, PROPVARIANT* ppropvarNewValue )
{
    if (key == UI_PKEY_Categories)
    {
        return HandleCategories(ppropvarCurrentValue);
    }
    else if (key == UI_PKEY_ItemsSource)
    {
        auto hr = HandleItemsSource(ppropvarCurrentValue, true);
        InvalidateUICommand(UI_INVALIDATIONS_PROPERTY, &UI_PKEY_SelectedItem);
        return hr;
    }
    else if (key == UI_PKEY_Enabled)
    {
        // only enabled if the current doc has a path!
        if (!HasActiveDocument())
            return E_FAIL;
        const auto& doc = GetActiveDocument();
        return  UIInitPropertyFromBoolean(UI_PKEY_Enabled, !doc.m_path.empty(), ppropvarNewValue);
    }
    else if (key == UI_PKEY_SelectedItem)
    {
        if (!HasActiveDocument())
            return S_FALSE;
        const auto& doc = GetActiveDocument();
        return HandleSelectedItem(ppropvarNewValue, doc.m_encoding, true);
    }

    return E_FAIL;
}

HRESULT CCmdLoadAsEncoded::IUICommandHandlerExecute( UI_EXECUTIONVERB verb, const PROPERTYKEY* key, const PROPVARIANT* ppropvarValue, IUISimplePropertySet* /*pCommandExecutionProperties*/ )
{
    if (verb == UI_EXECUTIONVERB_EXECUTE)
    {
        if (key && *key == UI_PKEY_SelectedItem)
        {
            UINT selected;
            HRESULT hr = UIPropertyToUInt32(*key, *ppropvarValue, &selected);
            if (selected > 1)
                ++selected; // offset for missing "utf-8 BOM"
            UINT codepage = codepages[selected].codepage;
            AddToMRU(codepages[selected]);
            ReloadTab(GetActiveTabIndex(), codepage);
            return hr;
        }
    }
    return E_FAIL;
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
    if (key == UI_PKEY_Categories)
    {
        return HandleCategories(ppropvarCurrentValue);
    }
    else if (key == UI_PKEY_ItemsSource)
    {
        return HandleItemsSource(ppropvarCurrentValue, false);
    }
    else if (key == UI_PKEY_SelectedItem)
    {
        if (!HasActiveDocument())
            return S_FALSE;
        const auto& doc = GetActiveDocument();
        return HandleSelectedItem(ppropvarNewValue, doc.m_encoding, false);
    }

    return E_FAIL;
}

HRESULT CCmdConvertEncoding::IUICommandHandlerExecute( UI_EXECUTIONVERB verb, const PROPERTYKEY* key, const PROPVARIANT* ppropvarValue, IUISimplePropertySet* /*pCommandExecutionProperties*/ )
{
    HRESULT hr = E_FAIL;

    if (verb == UI_EXECUTIONVERB_EXECUTE)
    {
        if (key && *key == UI_PKEY_SelectedItem)
        {
            UINT selected;
            hr = UIPropertyToUInt32(*key, *ppropvarValue, &selected);
            UINT codepage = codepages[selected].codepage;
            if (HasActiveDocument())
            {
                auto& doc = GetModActiveDocument();
                doc.m_encoding = codepage;
                doc.m_bHasBOM = codepages[selected].bom;
                doc.m_encodingSaving = -1;
                doc.m_bHasBOMSaving = false;
                doc.m_bIsDirty = true;
                doc.m_bNeedsSaving = true;
                // the next two calls are only here to trigger SCN_SAVEPOINTLEFT/SCN_SAVEPOINTREACHED messages
                ScintillaCall(SCI_ADDUNDOACTION, 0,0);
                ScintillaCall(SCI_UNDO);
                UpdateStatusBar(true);
                AddToMRU(codepages[selected]);
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
