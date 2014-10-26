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
#include <string>

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
const int CP_CATEGORY_CODEPAGES = 1;

std::vector<CodePageItem>  codepages;
};

BOOL CALLBACK CodePageEnumerator(LPTSTR lpCodePageString)
{
    if (codepages.empty())
    {
        // insert the main encodings
        codepages.push_back(CodePageItem(GetACP(), false, L"ANSI", 0));
        codepages.push_back(CodePageItem(CP_UTF8, false, L"UTF-8", 0));
        codepages.push_back(CodePageItem(CP_UTF8, true, L"UTF-8 BOM", 0));
        codepages.push_back(CodePageItem(1200, true, L"UTF-16 Little Endian", 0));
        codepages.push_back(CodePageItem(1201, true, L"UTF-16 Big Endian", 0));
        codepages.push_back(CodePageItem(12000, true, L"UTF-32 Little Endian", 0));
        codepages.push_back(CodePageItem(12001, true, L"UTF-32 Big Endian", 0));
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
                codepages.push_back(CodePageItem(codepage, false, name, 1));
            }
        }
        break;
    }
    return TRUE;
}


HRESULT CCmdLoadAsEncoded::IUICommandHandlerUpdateProperty( REFPROPERTYKEY key, const PROPVARIANT* ppropvarCurrentValue, PROPVARIANT* ppropvarNewValue )
{
    HRESULT hr = E_FAIL;

    if (key == UI_PKEY_Categories)
    {
        IUICollectionPtr pCollection;
        hr = ppropvarCurrentValue->punkVal->QueryInterface(IID_PPV_ARGS(&pCollection));
        if (CAppUtils::FailedShowMessage(hr))
            return hr;

        hr = CAppUtils::AddCategory(pCollection, CP_CATEGORY_MAIN, IDS_ENCODING_MAIN);
        if (FAILED(hr))
            return hr;
        hr = CAppUtils::AddCategory(pCollection, CP_CATEGORY_CODEPAGES, IDS_ENCODING_CODEPAGES);
        if (FAILED(hr))
            return hr;

        return S_OK;
    }
    else if (key == UI_PKEY_ItemsSource)
    {
        IUICollectionPtr pCollection;
        hr = ppropvarCurrentValue->punkVal->QueryInterface(IID_PPV_ARGS(&pCollection));
        if (CAppUtils::FailedShowMessage(hr))
            return hr;

        if (codepages.empty())
            EnumSystemCodePages(CodePageEnumerator, CP_INSTALLED);

        IUIImagePtr pImg;
        hr = CAppUtils::CreateImage(MAKEINTRESOURCE(IDB_EMPTY),pImg);
        CAppUtils::FailedShowMessage(hr); // Report any problem, don't let it stop us.

        // populate the dropdown with the code pages
        for (const auto& cp : codepages)
        {
            if (cp.bom && cp.name.compare(L"UTF-8 BOM")==0)
                continue;

            CAppUtils::AddStringItem(pCollection, cp.name.c_str(), cp.category, pImg);
        }
        return S_OK;
    }
    else if (key == UI_PKEY_Enabled)
    {
        HRESULT hr;
        // only enabled if the current doc has a path!
        if (!HasActiveDocument())
            return E_FAIL;
        CDocument doc = GetActiveDocument();
        hr = UIInitPropertyFromBoolean(UI_PKEY_Enabled, !doc.m_path.empty(), ppropvarNewValue);
        return hr;
    }
    else if (key == UI_PKEY_SelectedItem)
    {
        HRESULT hr = S_FALSE;
        if (!HasActiveDocument())
            return S_FALSE;
        CDocument doc = GetActiveDocument();
        if ((doc.m_encoding == -1)||(doc.m_encoding == 0))
            hr = UIInitPropertyFromUInt32(UI_PKEY_SelectedItem, (UINT)0, ppropvarNewValue);
            // Return value unused, just set for debugging.
        else
        {
            int offset = 0;
            for (size_t i = 0; i < codepages.size(); ++i)
            {
                if (codepages[i].bom && codepages[i].name.compare(L"UTF-8 BOM") == 0)
                {
                    offset = 1;
                    continue;
                }

                if ((int)codepages[i].codepage == doc.m_encoding)
                {
                    hr = UIInitPropertyFromUInt32(UI_PKEY_SelectedItem, (UINT)(i - offset), ppropvarNewValue);
                    // Return value unused, just set for debugging.
                    break;
                }
            }
        }
        return S_OK;
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
    HRESULT hr = E_FAIL;

    if (key == UI_PKEY_Categories)
    {
        IUICollectionPtr pCollection;
        hr = ppropvarCurrentValue->punkVal->QueryInterface(IID_PPV_ARGS(&pCollection));
        if (CAppUtils::FailedShowMessage(hr))
            return hr;

        hr = CAppUtils::AddCategory(pCollection, CP_CATEGORY_MAIN, IDS_ENCODING_MAIN);
        if (FAILED(hr))
            return hr;
        hr = CAppUtils::AddCategory(pCollection, CP_CATEGORY_CODEPAGES, IDS_ENCODING_CODEPAGES);
        if (FAILED(hr))
            return hr;

        return S_OK;
    }
    else if (key == UI_PKEY_ItemsSource)
    {
        IUICollectionPtr pCollection;
        hr = ppropvarCurrentValue->punkVal->QueryInterface(IID_PPV_ARGS(&pCollection));
        if (CAppUtils::FailedShowMessage(hr))
            return hr;

        if (codepages.empty())
            EnumSystemCodePages(CodePageEnumerator, CP_INSTALLED);

        IUIImagePtr pImg;
        hr = CAppUtils::CreateImage(MAKEINTRESOURCE(IDB_EMPTY), pImg);
        CAppUtils::FailedShowMessage(hr); // Report any errors, but don't let it stop us.

        // populate the dropdown with the code pages
        for (const auto& cp : codepages)
            CAppUtils::AddStringItem(pCollection, cp.name.c_str(), cp.category, pImg);

        return S_OK;
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
                    if (((int)codepages[i].codepage == doc.m_encoding)&&(codepages[i].bom == doc.m_bHasBOM))
                    {
                        hr = UIInitPropertyFromUInt32(UI_PKEY_SelectedItem, (UINT)i, ppropvarNewValue);
                        hr = S_OK;
                        break;
                    }
                }
            }
        }
        return hr;
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
                CDocument doc = GetActiveDocument();
                doc.m_encoding = codepage;
                doc.m_bHasBOM = codepages[selected].bom;
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
