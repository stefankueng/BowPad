// This file is part of BowPad.
//
// Copyright (C) 2013-2017, 2020-2021 - Stefan Kueng
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

#include <vector>
#include <string>

extern IUIFramework* g_pFramework;

struct CodePageItem
{
    CodePageItem(UINT codepage, bool bom, const std::wstring& name, int category)
        : codepage(codepage)
        , bom(bom)
        , name(name)
        , category(category)
    {
    }

    UINT         codepage;
    bool         bom;
    std::wstring name;
    int          category;
};

namespace
{
// Don't change value, file data depends on them.
const int CP_CATEGORY_MAIN      = 0;
const int CP_CATEGORY_MRU       = 1;
const int CP_CATEGORY_CODEPAGES = 2;

std::vector<CodePageItem> codepages;
std::vector<UINT>         mrucodepages;
}; // namespace

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
        for (int i = 5; i > 0; --i)
        {
            auto sKey = CStringUtils::Format(L"%02d", i);
            UINT cp   = static_cast<UINT>(CIniSettings::Instance().GetInt64(L"codepagemru", sKey.c_str(), 0));
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
            CPINFOEX cpEx = {0};
            GetCPInfoEx(codepage, 0, &cpEx);
            if (cpEx.CodePageName[0])
            {
                std::wstring name = cpEx.CodePageName;
                name              = name.substr(name.find_first_of(' ') + 1);
                size_t pos        = name.find_first_of('(');
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

HRESULT HandleCategories(const PROPVARIANT* pPropVarCurrentValue)
{
    IUICollectionPtr pCollection;
    auto             hr = pPropVarCurrentValue->punkVal->QueryInterface(IID_PPV_ARGS(&pCollection));
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

HRESULT HandleItemsSource(const PROPVARIANT* pPropVarCurrentValue, bool ignoreUtf8BOM)
{
    IUICollectionPtr pCollection;
    auto             hr = pPropVarCurrentValue->punkVal->QueryInterface(IID_PPV_ARGS(&pCollection));
    if (CAppUtils::FailedShowMessage(hr))
        return hr;
    pCollection->Clear();
    if (codepages.empty())
        EnumSystemCodePages(CodePageEnumerator, CP_INSTALLED);

    // populate the dropdown with the code pages
    for (const auto& cp : codepages)
    {
        if (ignoreUtf8BOM && cp.bom && cp.name.compare(L"UTF-8 BOM") == 0)
            continue;

        CAppUtils::AddStringItem(pCollection, cp.name.c_str(), cp.category, EMPTY_IMAGE);
    }
    return S_OK;
}

HRESULT HandleSelectedItem(PROPVARIANT* pPropVarNewValue, int encoding, bool ignoreUtf8BOM)
{
    auto hr = S_OK;
    if ((encoding == -1) || (encoding == 0))
        hr = UIInitPropertyFromUInt32(UI_PKEY_SelectedItem, static_cast<UINT>(0), pPropVarNewValue);
    // Return value unused, just set for debugging.
    else
    {
        int offset = 0;
        for (size_t i = 0; i < codepages.size(); ++i)
        {
            if (ignoreUtf8BOM && codepages[i].bom && codepages[i].name.compare(L"UTF-8 BOM") == 0)
            {
                offset = 1;
                continue;
            }

            if (static_cast<int>(codepages[i].codepage) == encoding)
            {
                hr = UIInitPropertyFromUInt32(UI_PKEY_SelectedItem, static_cast<UINT>(i - offset), pPropVarNewValue);
                // Return value unused, just set for debugging.
                break;
            }
        }
    }
    return hr;
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
    for (auto& cpEntry : codepages)
    {
        if (cpEntry.category == CP_CATEGORY_MRU)
            cpEntry.category = CP_CATEGORY_CODEPAGES;
    }
    int mruIndex = 1;
    for (const auto& mru : mrucodepages)
    {
        auto sKey = CStringUtils::Format(L"%02d", mruIndex++);
        CIniSettings::Instance().SetInt64(L"codepagemru", sKey.c_str(), mru);
        for (auto& cpEntry : codepages)
        {
            if (cpEntry.codepage == mru)
                cpEntry.category = CP_CATEGORY_MRU;
        }
    }
    g_pFramework->InvalidateUICommand(cmdLoadAsEncoding, UI_INVALIDATIONS_PROPERTY, &UI_PKEY_Categories);
    g_pFramework->InvalidateUICommand(cmdLoadAsEncoding, UI_INVALIDATIONS_PROPERTY, &UI_PKEY_ItemsSource);
    g_pFramework->InvalidateUICommand(cmdConvertEncoding, UI_INVALIDATIONS_PROPERTY, &UI_PKEY_Categories);
    g_pFramework->InvalidateUICommand(cmdConvertEncoding, UI_INVALIDATIONS_PROPERTY, &UI_PKEY_ItemsSource);
}

HRESULT CCmdLoadAsEncoded::IUICommandHandlerUpdateProperty(REFPROPERTYKEY key, const PROPVARIANT* pPropVarCurrentValue, PROPVARIANT* pPropVarNewValue)
{
    if (key == UI_PKEY_Categories)
    {
        return HandleCategories(pPropVarCurrentValue);
    }
    else if (key == UI_PKEY_ItemsSource)
    {
        auto hr = HandleItemsSource(pPropVarCurrentValue, true);
        InvalidateUICommand(UI_INVALIDATIONS_PROPERTY, &UI_PKEY_SelectedItem);
        InvalidateUICommand(UI_INVALIDATIONS_VALUE, &UI_PKEY_SelectedItem);
        return hr;
    }
    else if (key == UI_PKEY_Enabled)
    {
        // only enabled if the current doc has a path!
        if (!HasActiveDocument())
            return E_FAIL;
        const auto& doc = GetActiveDocument();
        return UIInitPropertyFromBoolean(UI_PKEY_Enabled, !doc.m_path.empty(), pPropVarNewValue);
    }
    else if (key == UI_PKEY_SelectedItem)
    {
        if (!HasActiveDocument())
            return S_FALSE;
        const auto& doc = GetActiveDocument();
        return HandleSelectedItem(pPropVarNewValue, doc.m_encoding, true);
    }

    return E_FAIL;
}

HRESULT CCmdLoadAsEncoded::IUICommandHandlerExecute(UI_EXECUTIONVERB verb, const PROPERTYKEY* key, const PROPVARIANT* pPropVarValue, IUISimplePropertySet* /*pCommandExecutionProperties*/)
{
    if (verb == UI_EXECUTIONVERB_EXECUTE)
    {
        if (key && *key == UI_PKEY_SelectedItem)
        {
            UINT    selected;
            HRESULT hr = UIPropertyToUInt32(*key, *pPropVarValue, &selected);
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

void CCmdLoadAsEncoded::TabNotify(TBHDR* ptbHdr)
{
    if (ptbHdr->hdr.code == TCN_SELCHANGE)
    {
        InvalidateUICommand(UI_INVALIDATIONS_PROPERTY, &UI_PKEY_Enabled);
        InvalidateUICommand(UI_INVALIDATIONS_PROPERTY, &UI_PKEY_SelectedItem);
        InvalidateUICommand(UI_INVALIDATIONS_VALUE, &UI_PKEY_SelectedItem);
    }
}

HRESULT CCmdConvertEncoding::IUICommandHandlerUpdateProperty(REFPROPERTYKEY key, const PROPVARIANT* pPropVarCurrentValue, PROPVARIANT* pPropVarNewValue)
{
    if (key == UI_PKEY_Categories)
    {
        return HandleCategories(pPropVarCurrentValue);
    }
    else if (key == UI_PKEY_ItemsSource)
    {
        return HandleItemsSource(pPropVarCurrentValue, false);
    }
    else if (key == UI_PKEY_SelectedItem)
    {
        if (!HasActiveDocument())
            return S_FALSE;
        const auto& doc = GetActiveDocument();
        return HandleSelectedItem(pPropVarNewValue, doc.m_encoding, false);
    }

    return E_FAIL;
}

HRESULT CCmdConvertEncoding::IUICommandHandlerExecute(UI_EXECUTIONVERB verb, const PROPERTYKEY* key, const PROPVARIANT* pPropVarValue, IUISimplePropertySet* /*pCommandExecutionProperties*/)
{
    HRESULT hr = E_FAIL;

    if (verb == UI_EXECUTIONVERB_EXECUTE)
    {
        if (key && *key == UI_PKEY_SelectedItem)
        {
            UINT selected;
            hr            = UIPropertyToUInt32(*key, *pPropVarValue, &selected);
            UINT codepage = codepages[selected].codepage;
            if (HasActiveDocument())
            {
                auto& doc            = GetModActiveDocument();
                doc.m_encoding       = codepage;
                doc.m_bHasBOM        = codepages[selected].bom;
                doc.m_encodingSaving = -1;
                doc.m_bHasBOMSaving  = false;
                doc.m_bIsDirty       = true;
                doc.m_bNeedsSaving   = true;
                // the next two calls are only here to trigger SCN_SAVEPOINTLEFT/SCN_SAVEPOINTREACHED messages
                ScintillaCall(SCI_ADDUNDOACTION, 0, 0);
                ScintillaCall(SCI_UNDO);
                UpdateStatusBar(true);
                AddToMRU(codepages[selected]);
            }
            hr = S_OK;
        }
    }
    return hr;
}

void CCmdConvertEncoding::TabNotify(TBHDR* ptbHdr)
{
    if (ptbHdr->hdr.code == TCN_SELCHANGE)
    {
        InvalidateUICommand(UI_INVALIDATIONS_PROPERTY, &UI_PKEY_Enabled);
        InvalidateUICommand(UI_INVALIDATIONS_PROPERTY, &UI_PKEY_SelectedItem);
        InvalidateUICommand(UI_INVALIDATIONS_VALUE, &UI_PKEY_SelectedItem);
    }
}
