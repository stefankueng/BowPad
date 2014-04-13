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
#include "CmdFunctions.h"
#include "PropertySet.h"
#include "BowPad.h"
#include "StringUtils.h"
#include "UnicodeUtils.h"
#include "LexStyles.h"

#include <vector>
#include <algorithm>

namespace
{
    struct func_info
    {
        inline func_info(size_t line, std::wstring&& sort_name, std::wstring&& display_name)
            : line(line), sort_name(sort_name), display_name(display_name)
        {}
        size_t line;
        std::wstring sort_name;
        std::wstring display_name;
    };
};

static std::vector<func_info> functions;
// Using the same timer delay in all situations to avoid repeating it.
static const int timer_delay = 3000;

static void strip_comments(std::wstring& f)
{
    // remove comments
    auto pos = f.find(L"/*");
    while (pos != std::wstring::npos)
    {
        size_t posend = f.find(L"*/", pos);
        if (posend != std::string::npos)
        {
            f.erase(pos, posend);
        }
        pos = f.find(L"/*");
    }
}

static void normalize(std::wstring& f)
{
    auto e = std::remove_if(f.begin(), f.end(), [](wchar_t c)
    {
        return c == L'\r' || c == L'{';
    });
    f.erase(e, f.end());
    std::replace_if(f.begin(), f.end(), [](wchar_t c)
    {
        return c == L'\n' || c == L'\t';
    }, L' ');
    // remove unnecessary whitespace inside the string
    // bool BothAreSpaces(char lhs, char rhs) { return (lhs == rhs) && (lhs == ' '); }
    auto new_end = std::unique(f.begin(), f.end(), [](wchar_t lhs, wchar_t rhs) -> bool
    {
        return (lhs == rhs) && (lhs == L' ');
    });
    f.erase(new_end, f.end());
}

static bool parse_signature(const std::wstring& sig, std::wstring& name, std::wstring& name_and_args)
{
    // Find the name of the function
    name.clear();
    name_and_args.clear();
    bool parsed = false;

    auto bracepos = sig.find(L'(');
    if (bracepos != std::wstring::npos)
    {
        auto wpos = sig.find_last_of(L"\t :,.", bracepos - 1, 5);
        size_t spos = (wpos == std::wstring::npos) ? 0 : wpos + 1;

        // Functions returning pointer or reference will feature these symbols
        // before the name. Ignore them.
        // This whole logic may need to be language specific.
        while (spos < bracepos && (sig[spos] == L'*' || sig[spos] == L'&' || sig[spos] == L'^'))
            ++spos;
        name = sig.substr(spos, bracepos - spos);
        name_and_args = sig.substr(spos);
        CStringUtils::trim(name);
        parsed = !name.empty();
    }
    return parsed;
}

HRESULT CCmdFunctions::IUICommandHandlerUpdateProperty(REFPROPERTYKEY key, const PROPVARIANT* ppropvarCurrentValue, PROPVARIANT* ppropvarNewValue)
{
    HRESULT hr = E_FAIL;

    if (key == UI_PKEY_Categories)
    {
        hr = S_FALSE;
    }
    else if (key == UI_PKEY_ItemsSource)
    {
        IUICollectionPtr pCollection;
        hr = ppropvarCurrentValue->punkVal->QueryInterface(IID_PPV_ARGS(&pCollection));
        if (FAILED(hr))
            return hr;

        pCollection->Clear();
        functions.clear();
        if (!HasActiveDocument())
            return hr;
        // Note: docID is < 0 if the document does not exist,
        // then FindFunctions() will return no functions
        int docID = GetDocIDFromTabIndex(GetActiveTabIndex());
        FindFunctions(docID, false);
        if (!functions.empty())
        {
            CDocument doc = GetActiveDocument();
            std::string lang = CUnicodeUtils::StdGetUTF8(doc.m_language);
            int sortmethod = CLexStyles::Instance().GetFunctionRegexSortForLang(lang);

            if (sortmethod)
            {
                std::sort(functions.begin(), functions.end(),
                          [](const func_info& a, const func_info& b)
                {
                    // sorting the list case insensitively makes
                    // finding things easier for many people.
                    // Could be an additional setting to sort by case or not.
                    return _wcsicmp(a.sort_name.c_str(), b.sort_name.c_str()) < 0;
                });
            }
            // Populate the dropdown with the function details.
            for (const auto& func : functions)
            {
                // Create a new property set for each item.
                CPropertySet* pItem;
                hr = CPropertySet::CreateInstance(&pItem);
                if (FAILED(hr))
                    return hr;

                pItem->InitializeItemProperties(NULL,
                                                func.display_name.c_str(),
                                                UI_COLLECTION_INVALIDINDEX);

                // Add the newly-created property set to the collection supplied by the framework.
                hr = pCollection->Add(pItem);
                pItem->Release();
                if (FAILED(hr))
                    return hr;
            }
        }
        else // No functions
        {
            CPropertySet* pItem;
            hr = CPropertySet::CreateInstance(&pItem);
            if (FAILED(hr))
                return hr;
            ResString rs(hRes, IDS_NOFUNCTIONSFOUND);
            pItem->InitializeItemProperties(NULL, rs, UI_COLLECTION_INVALIDINDEX);

            // Add the newly-created property set to the collection supplied by the framework.
            hr = pCollection->Add(pItem);

            pItem->Release();
        }

        hr = S_OK;
    }
    else if (key == UI_PKEY_SelectedItem)
    {
        hr = S_FALSE;
        hr = UIInitPropertyFromUInt32(UI_PKEY_SelectedItem, (UINT)UI_COLLECTION_INVALIDINDEX, ppropvarNewValue);
    }
    else if (key == UI_PKEY_Enabled)
    {
        if (HasActiveDocument())
        {
            CDocument doc = GetActiveDocument();
            std::string lang = CUnicodeUtils::StdGetUTF8(doc.m_language);
            std::string funcregex = CLexStyles::Instance().GetFunctionRegexForLang(lang);
            return UIInitPropertyFromBoolean(UI_PKEY_Enabled, !funcregex.empty(), ppropvarNewValue);
        }
    }
    return hr;
}

HRESULT CCmdFunctions::IUICommandHandlerExecute(UI_EXECUTIONVERB verb, const PROPERTYKEY* key, const PROPVARIANT* ppropvarValue, IUISimplePropertySet* /*pCommandExecutionProperties*/)
{
    HRESULT hr = E_FAIL;

    if (verb == UI_EXECUTIONVERB_EXECUTE)
    {
        if (key && (*key == UI_PKEY_SelectedItem) && !functions.empty())
        {
            UINT selected;
            hr = UIPropertyToUInt32(*key, *ppropvarValue, &selected);
            if (SUCCEEDED(hr))
            {
                size_t line = functions[selected].line;
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
                    linesToScroll -= linesVisible / 2;
                }
                else if (currentlineNumberVis > lastVisibleLineVis)
                {
                    linesToScroll = currentlineNumberVis - lastVisibleLineVis;
                    // use center
                    linesToScroll += linesVisible / 2;
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

void CCmdFunctions::TabNotify(TBHDR * ptbhdr)
{
    if (ptbhdr->hdr.code == TCN_SELCHANGE)
    {
        InvalidateUICommand(UI_INVALIDATIONS_PROPERTY, &UI_PKEY_Enabled);
        InvalidateUICommand(UI_INVALIDATIONS_PROPERTY, &UI_PKEY_ItemsSource);
    }
}

void CCmdFunctions::ScintillaNotify(Scintilla::SCNotification * pScn)
{
    switch (pScn->nmhdr.code)
    {
        case SCN_MODIFIED:
            if (pScn->modificationType & (SC_MOD_INSERTTEXT | SC_MOD_DELETETEXT | SC_PERFORMED_USER))
            {
                InvalidateUICommand(UI_INVALIDATIONS_PROPERTY, &UI_PKEY_ItemsSource);
                m_docIDs.insert(GetDocIDFromTabIndex(GetActiveTabIndex()));
                SetTimer(GetHwnd(), m_timerID, timer_delay, NULL);
            }
            break;
    }
}

void CCmdFunctions::OnTimer(UINT id)
{
    if (id == m_timerID)
    {
        if (CIniSettings::Instance().GetInt64(L"functions", L"autoscan", 0))
        {
            for (auto id : m_docIDs)
            {
                FindFunctions(id, true);
                m_docIDs.erase(id);
                if (!m_docIDs.empty())
                    SetTimer(GetHwnd(), m_timerID, 100, NULL);
                break;
            }
            if (m_docIDs.empty())
                KillTimer(GetHwnd(), m_timerID);
        }
        else
        {
            m_docIDs.clear();
            KillTimer(GetHwnd(), m_timerID);
        }
    }
}

void CCmdFunctions::OnDocumentOpen(int id)
{
    int docID = GetDocIDFromTabIndex(id);
    if (docID >= 0)
    {
        m_docIDs.insert(docID);
        SetTimer(GetHwnd(), m_timerID, timer_delay, NULL);
    }
}

void CCmdFunctions::OnDocumentSave(int index, bool bSaveAs)
{
    if (bSaveAs)
    {
        int docID = GetDocIDFromTabIndex(index);
        if (docID >= 0)
        {
            m_docIDs.insert(docID);
            SetTimer(GetHwnd(), m_timerID, timer_delay, NULL);
            InvalidateUICommand(UI_INVALIDATIONS_PROPERTY, &UI_PKEY_Enabled);
        }
    }
}

void CCmdFunctions::FindFunctions(int docID, bool bBackground)
{
    if (HasActiveDocument() || bBackground)
    {
        CDocument doc;
        if (bBackground)
        {
            if (!HasDocumentID(docID))
                return;
            doc = GetDocumentFromID(docID);
        }
        else
            doc = GetActiveDocument();
        auto start = GetTickCount64();
        std::string lang = CUnicodeUtils::StdGetUTF8(doc.m_language);
        std::string funcregex = CLexStyles::Instance().GetFunctionRegexForLang(lang);
        auto trimtokens = CLexStyles::Instance().GetFunctionRegexTrimForLang(lang);
        std::vector<std::wstring> wtrimtokens;
        for (const auto& token : trimtokens)
            wtrimtokens.push_back(CUnicodeUtils::StdGetUnicode(token));
        if (!funcregex.empty())
        {
            m_ScratchScintilla.Call(SCI_SETSTATUS, SC_STATUS_OK);   // reset error status
            m_ScratchScintilla.Call(SCI_CLEARALL);
            m_ScratchScintilla.Call(SCI_SETDOCPOINTER, 0, doc.m_document);
            m_ScratchScintilla.Call(SCI_SETCODEPAGE, CP_UTF8);

            sptr_t findRet = -1;
            Scintilla::Sci_TextToFind ttf = { 0 };
            long length = (long)m_ScratchScintilla.Call(SCI_GETLENGTH);
            if (bBackground && (length > 1000 * 1024))
            {
                m_ScratchScintilla.Call(SCI_SETDOCPOINTER, 0, 0);
                return;
            }
            ttf.chrg.cpMin = 0;
            ttf.chrg.cpMax = length;
            ttf.lpstrText = const_cast<char*>(funcregex.c_str());
            bool bUpdateLexer = false;

            int func_display_mode = static_cast<int>(
                CIniSettings::Instance().GetInt64(L"functions", L"function_display_mode",
                0));

            for (;;)
            {
                ttf.chrgText.cpMin = 0;
                ttf.chrgText.cpMax = 0;
                findRet = m_ScratchScintilla.Call(SCI_FINDTEXT, SCFIND_REGEXP, (sptr_t)&ttf);
                if (findRet < 0)
                    break;

                size_t len = ttf.chrgText.cpMax - ttf.chrgText.cpMin;
                std::string raw;
                raw.resize(len + 1);
                Scintilla::Sci_TextRange funcrange;
                funcrange.chrg.cpMin = ttf.chrgText.cpMin;
                funcrange.chrg.cpMax = ttf.chrgText.cpMax;
                funcrange.lpstrText = &raw[0];
                m_ScratchScintilla.Call(SCI_GETTEXTRANGE, 0, reinterpret_cast<sptr_t>(&funcrange));
                raw.resize(len);
                size_t line = m_ScratchScintilla.Call(SCI_LINEFROMPOSITION, funcrange.chrg.cpMin);

                std::wstring sig = CUnicodeUtils::StdGetUnicode(raw);
                strip_comments(sig);
                normalize(sig);
                for (const auto& token : wtrimtokens)
                    SearchReplace(sig, token, L"");
                CStringUtils::trim(sig);
                ttf.chrg.cpMin = ttf.chrgText.cpMax + 1;
                ttf.chrg.cpMax = length;

                std::wstring name;
                std::wstring name_and_args;

                // Note: regexp identifies functions incorrectly so not
                // everything is a function that comes through here.
                // e.g. a switch looking a lot like a function can appear. e.g.:
                // "case ISD::SHL: switch (VT.SimpleTy)"
                // but we have to deal with it unless we want to write compilers
                // for each supported language
                if (parse_signature(sig, name, name_and_args))
                {
                    if (CLexStyles::Instance().AddUserFunctionForLang(lang, CUnicodeUtils::StdGetUTF8(name)))
                        bUpdateLexer = true;

                    switch (func_display_mode)
                    {
                        case 0: // display name and signature
                        {
                            // Put a space between the function name and the args to enhance readability.
                            auto bracepos = name_and_args.find(L'(');
                            if (bracepos != std::wstring::npos)
                            {
                                name_and_args.insert(bracepos, L" ");
                            }
                            functions.push_back(func_info(line, std::move(name), std::move(name_and_args)));
                        }
                            break;
                        case 1: // name only
                        {
                            std::wstring temp = name;
                            functions.push_back(func_info(line, std::move(name), std::move(temp)));
                        }
                            break;
                        case 2: // display whole thing type name inc.
                        {
                            // Put a space between the function name and the args to enhance readability.
                            auto bracepos = sig.find(L'(');
                            if (bracepos != std::wstring::npos)
                            {
                                sig.insert(bracepos, L" ");
                            }
                            functions.push_back(func_info(line, std::move(name), std::move(sig)));
                        }
                            break;
                    }
                }
                else
                {
                    // Some kind of line we don't understand. It's probably not
                    // a function, but put it in the list so it can be seen, then
                    // someone can file bug reports on why we don't recognize it.
                    name = sig;
                    functions.push_back(func_info(line, std::move(sig), std::move(name)));
                }
                if (bBackground && (GetTickCount64() - start > 500))
                    break;
            }
            m_ScratchScintilla.Call(SCI_SETDOCPOINTER, 0, 0);

            if (bBackground && bUpdateLexer)
            {
                // check whether the current view is of the same language
                // as the one we just scanned
                if (HasActiveDocument())
                {
                    CDocument td = GetActiveDocument();
                    if (td.m_language != doc.m_language)
                        bUpdateLexer = false;
                }
            }

            if (bUpdateLexer)
                SetupLexerForLang(CUnicodeUtils::StdGetUnicode(lang));
        }
    }
}
