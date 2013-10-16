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
#include "CmdFunctions.h"
#include "PropertySet.h"
#include "BowPad.h"
#include "StringUtils.h"
#include "UnicodeUtils.h"
#include "LexStyles.h"

#include <vector>
#include <tuple>
#include <algorithm>

std::vector<std::tuple<size_t,std::wstring>> functions;

HRESULT CCmdFunctions::IUICommandHandlerUpdateProperty( REFPROPERTYKEY key, const PROPVARIANT* ppropvarCurrentValue, PROPVARIANT* ppropvarNewValue )
{
    HRESULT hr = E_FAIL;

    if(key == UI_PKEY_Categories)
    {
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

        pCollection->Clear();
        functions.clear();
        CDocument doc = GetDocument(GetCurrentTabIndex());
        std::string lang = CUnicodeUtils::StdGetUTF8(doc.m_language);
        std::string funcregex = CLexStyles::Instance().GetFunctionRegexForLang(lang);
        int sortmethod = CLexStyles::Instance().GetFunctionRegexSortForLang(lang);
        auto trimtokens = CLexStyles::Instance().GetFunctionRegexTrimForLang(lang);
        if (!funcregex.empty())
        {
            sptr_t findRet = -1;
            Scintilla::Sci_TextToFind ttf = {0};
            long length = (long)ScintillaCall(SCI_GETLENGTH);
            ttf.chrg.cpMin = 0;
            ttf.chrg.cpMax = length;
            ttf.lpstrText = const_cast<char*>(funcregex.c_str());
            do
            {
                ttf.chrgText.cpMin = 0;
                ttf.chrgText.cpMax = 0;
                findRet = ScintillaCall(SCI_FINDTEXT, SCFIND_REGEXP, (sptr_t)&ttf);
                if (findRet >= 0)
                {
                    std::unique_ptr<char[]> strbuf(new char[ttf.chrgText.cpMax - ttf.chrgText.cpMin + 1]);
                    Scintilla::Sci_TextRange funcrange;
                    funcrange.chrg.cpMin = ttf.chrgText.cpMin;
                    funcrange.chrg.cpMax = ttf.chrgText.cpMax;
                    funcrange.lpstrText  = strbuf.get();
                    ScintillaCall(SCI_GETTEXTRANGE, 0, (sptr_t)&funcrange);
                    size_t line = ScintillaCall(SCI_LINEFROMPOSITION, funcrange.chrg.cpMin);
                    std::wstring sf = CUnicodeUtils::StdGetUnicode(strbuf.get());
                    SearchReplace(sf, L"\r", L" ");
                    SearchReplace(sf, L"\n", L"");
                    SearchReplace(sf, L"{", L"");
                    SearchReplace(sf, L"\t", L" ");
                    if (!trimtokens.empty())
                    {
                        for (const auto& token : trimtokens)
                            SearchReplace(sf, CUnicodeUtils::StdGetUnicode(token), L"");
                    }
                    // remove comments
                    size_t pos = sf.find(L"/*");
                    while (pos != std::wstring::npos)
                    {
                        size_t posend = sf.find(L"*/", pos);
                        if (posend != std::wstring::npos)
                        {
                            sf.erase(pos, posend);
                        }
                        pos = sf.find(L"/*");
                    }
                    CStringUtils::trim(sf);

                    // remove unnecessary whitespace inside the string
                    // bool BothAreSpaces(char lhs, char rhs) { return (lhs == rhs) && (lhs == ' '); }
                    std::wstring::iterator new_end = std::unique(sf.begin(), sf.end(), [&] (wchar_t lhs, wchar_t rhs) -> bool
                    {
                        return (lhs == rhs) && (lhs == ' ');
                    });
                    sf.erase(new_end, sf.end());


                    functions.push_back(std::make_tuple(line, sf));
                    ttf.chrg.cpMin = ttf.chrgText.cpMax + 1;
                    ttf.chrg.cpMax = length;
                }
            } while (findRet >= 0);

            if (sortmethod)
            {
                std::sort(functions.begin(), functions.end(), [&](std::tuple<size_t,std::wstring> a, std::tuple<size_t,std::wstring> b)
                {
                    std::wstring sa = std::get<1>(a);
                    std::wstring sb = std::get<1>(b);
                    if (sortmethod == 2)
                    {
                        size_t pa = sa.find_last_of('(');
                        if (pa > 2)
                            pa -= 2;
                        size_t pb = sb.find_last_of('(');
                        if (pb > 2)
                            pb -= 2;
                        size_t flosa = sa.find_last_of(' ', pa);
                        size_t flosb = sb.find_last_of(' ', pb);
                        if (flosa != std::wstring::npos)
                            sa = sa.substr(flosa);
                        if (flosb != std::wstring::npos)
                            sb = sb.substr(flosb);

                        if (!sa.empty() && (sa[0] == '*'))
                            sa = sa.substr(1);
                        if (!sb.empty() && (sb[0] == '*'))
                            sb = sb.substr(1);
                    }

                    return sa < sb;
                });
            }
            // populate the dropdown with the code pages
            for (const auto& func : functions)
            {
                // Create a new property set for each item.
                CPropertySet* pItem;
                hr = CPropertySet::CreateInstance(&pItem);
                if (FAILED(hr))
                {
                    pCollection->Release();
                    return hr;
                }
                std::wstring sf = std::get<1>(func);
                pItem->InitializeItemProperties(NULL, sf.c_str(), UI_COLLECTION_INVALIDINDEX);

                // Add the newly-created property set to the collection supplied by the framework.
                hr = pCollection->Add(pItem);

                pItem->Release();
            }
            if (functions.empty())
            {
                CPropertySet* pItem;
                hr = CPropertySet::CreateInstance(&pItem);
                if (FAILED(hr))
                {
                    pCollection->Release();
                    return hr;
                }
                ResString rs(hRes, IDS_NOFUNCTIONSFOUND);
                pItem->InitializeItemProperties(NULL, rs, UI_COLLECTION_INVALIDINDEX);

                // Add the newly-created property set to the collection supplied by the framework.
                hr = pCollection->Add(pItem);

                pItem->Release();
            }
        }
        //pImg->Release();
        pCollection->Release();
        hr = S_OK;
    }
    else if (key == UI_PKEY_SelectedItem)
    {
        hr = S_FALSE;
        hr = UIInitPropertyFromUInt32(UI_PKEY_SelectedItem, (UINT)UI_COLLECTION_INVALIDINDEX, ppropvarNewValue);
    }
    else if (key == UI_PKEY_Enabled)
    {
        CDocument doc = GetDocument(GetCurrentTabIndex());
        std::string lang = CUnicodeUtils::StdGetUTF8(doc.m_language);
        std::string funcregex = CLexStyles::Instance().GetFunctionRegexForLang(lang);
        return UIInitPropertyFromBoolean(UI_PKEY_Enabled, !funcregex.empty(), ppropvarNewValue);
    }
    return hr;
}

HRESULT CCmdFunctions::IUICommandHandlerExecute( UI_EXECUTIONVERB verb, const PROPERTYKEY* key, const PROPVARIANT* ppropvarValue, IUISimplePropertySet* /*pCommandExecutionProperties*/ )
{
    HRESULT hr = E_FAIL;

    if (verb == UI_EXECUTIONVERB_EXECUTE)
    {
        if ( key && (*key == UI_PKEY_SelectedItem) && !functions.empty())
        {
            UINT selected;
            hr = UIPropertyToUInt32(*key, *ppropvarValue, &selected);
            if (SUCCEEDED(hr))
            {
                size_t line = std::get<0>(functions[selected]);
                size_t linepos = ScintillaCall(SCI_POSITIONFROMLINE, line);

                // to make sure the found result is visible
                // When searching up, the beginning of the (possible multiline) result is important, when scrolling down the end
                ScintillaCall(SCI_SETCURRENTPOS, linepos);
                long currentlineNumberDoc = (long)ScintillaCall(SCI_LINEFROMPOSITION, linepos);
                long currentlineNumberVis = (long)ScintillaCall(SCI_VISIBLEFROMDOCLINE, currentlineNumberDoc);
                ScintillaCall(SCI_ENSUREVISIBLE, currentlineNumberDoc);    // make sure target line is unfolded

                long firstVisibleLineVis =   (long)ScintillaCall(SCI_GETFIRSTVISIBLELINE);
                long linesVisible =          (long)ScintillaCall(SCI_LINESONSCREEN) - 1; //-1 for the scrollbar
                long lastVisibleLineVis =    (long)linesVisible + firstVisibleLineVis;

                // if out of view vertically, scroll line into (center of) view
                int linesToScroll = 0;
                if (currentlineNumberVis < firstVisibleLineVis)
                {
                    linesToScroll = currentlineNumberVis - firstVisibleLineVis;
                    // use center
                    linesToScroll -= linesVisible/2;
                }
                else if (currentlineNumberVis > lastVisibleLineVis)
                {
                    linesToScroll = currentlineNumberVis - lastVisibleLineVis;
                    // use center
                    linesToScroll += linesVisible/2;
                }
                ScintillaCall(SCI_LINESCROLL, 0, linesToScroll);

                // Make sure the caret is visible, scroll horizontally
                ScintillaCall(SCI_GOTOPOS, linepos);

                InvalidateUICommand(UI_INVALIDATIONS_PROPERTY, &UI_PKEY_SelectedItem);

                hr = S_OK;
            }
        }
    }
    return hr;
}

void CCmdFunctions::TabNotify( TBHDR * ptbhdr )
{
    if (ptbhdr->hdr.code == TCN_SELCHANGE)
    {
        InvalidateUICommand(UI_INVALIDATIONS_PROPERTY, &UI_PKEY_Enabled);
        InvalidateUICommand(UI_INVALIDATIONS_PROPERTY, &UI_PKEY_ItemsSource);
    }
}

void CCmdFunctions::ScintillaNotify( Scintilla::SCNotification * pScn )
{
    switch (pScn->nmhdr.code)
    {
    case SCN_MODIFIED:
        InvalidateUICommand(UI_INVALIDATIONS_PROPERTY, &UI_PKEY_ItemsSource);
        break;
    }
}

