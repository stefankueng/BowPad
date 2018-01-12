// This file is part of BowPad.
//
// Copyright (C) 2013-2018 - Stefan Kueng
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
#include "PathUtils.h"
#include "AppUtils.h"
#include "OnOutOfScope.h"

#include <string>
#include <vector>
#include <algorithm>
#include <regex>

// IMPORTANT: Testing this module can be difficult because debug mode
// performance can be drastically slower than release builds.
// Regex in particular can also be slower.


// Turns "Hello/* there */world" into "Helloworld"
static void StripComments(std::string& f)
{
    constexpr char comment_begin[] = { "/*" };
    constexpr char comment_end[] = { "*/" };
    constexpr size_t comment_begin_len = std::size(comment_begin) - 1;
    constexpr size_t comment_end_len = std::size(comment_end) - 1;
    size_t comment_begin_pos = 0;
    for (;;)
    {
        comment_begin_pos = f.find(comment_begin, comment_begin_pos);
        if (comment_begin_pos == std::string::npos)
            break;
        auto comment_end_pos = f.find(comment_end,
                                      comment_begin_pos + comment_begin_len);
        if (comment_end_pos == std::string::npos)
            break;
        auto e = f.erase(f.begin() + comment_begin_pos,
                         f.begin() + comment_end_pos + comment_end_len);
        auto trash = std::remove(e, f.end(), '\0');
        f.erase(trash, f.end());
    }
}

static void Normalize(std::string& f)
{
    // Remove certain chars and replace adjacent whitespace inside the string.
    // Remember to patch up the size to reflect what we remove.
    auto e = std::remove_if(f.begin(), f.end(), [](auto c)
    {
        return c == '\r' || c == '{';
    });
    f.erase(e, f.end());
    std::replace_if(f.begin(), f.end(), [](auto c)
    {
        return c == '\n' || c == '\t';
    }, ' ');
    auto new_end = std::unique(f.begin(), f.end(), [](auto lhs, auto rhs) -> bool
    {
        return (lhs == ' ' && rhs == ' ');
    });
    f.erase(new_end, f.end());
}

static bool ParseSignature(const std::string& sig, std::string& name, std::string& name_and_args)
{
    bool parsed = false;

    // Look for a ( of perhaps void x::f(whatever)
    auto bracepos = sig.find('(');
    if (bracepos != std::string::npos)
    {
        auto wpos = sig.find_last_of("\t :,.", bracepos - 1, 5);
        size_t spos = (wpos == std::string::npos) ? 0 : wpos + 1;

        // Functions returning pointer or reference will feature these symbols
        // before the name. Ignore them. This logic is a bit C language based.
        while (spos < bracepos && (sig[spos] == '*' || sig[spos] == '&' || sig[spos] == '^'))
            ++spos;
        name.assign(sig, spos, bracepos - spos);
        name_and_args.assign(sig, spos);
    }
    else
    {
        // some languages have functions without (), or pseudo-functions like
        // properties in c#. Deal with those here.
        auto wpos = sig.find_last_of("\t :,.", bracepos - 1, 5);
        size_t spos = (wpos == std::string::npos) ? 0 : wpos + 1;

        name.assign(sig, spos, bracepos - spos);
        name_and_args.assign(sig, spos);
    }
    CStringUtils::trim(name);
    parsed = !name.empty();
    return parsed;
}

static inline bool ParseName(const std::string& sig, std::string& name)
{
    // Look for a ( of perhaps void x::f(whatever)
    auto bracepos = sig.find('(');
    if (bracepos != std::string::npos)
    {
        auto wpos = sig.find_last_of("\t :,.", bracepos - 1, 5);
        size_t spos = (wpos == std::string::npos) ? 0 : wpos + 1;

        // Functions returning pointer or reference will feature these symbols
        // before the name. Ignore them. This logic is a bit C language based.
        while (spos < bracepos && (sig[spos] == '*' || sig[spos] == '&' || sig[spos] == '^'))
            ++spos;
        name.assign(sig, spos, bracepos - spos);
        return !name.empty();
    }
    return false;
}

static bool FindNext(CScintillaWnd& edit, const Sci_TextToFind& ttf,
                     std::string& foundText, int* lineNum)
{
    if (ttf.chrg.cpMax - ttf.chrg.cpMin <= 0)
        return false;
    // In debug mode, regex takes a *long* time.
    auto findRet = edit.Call(SCI_FINDTEXT, SCFIND_REGEXP | SCFIND_CXX11REGEX, (sptr_t)&ttf);
    if (findRet < 0)
        return false;
    // Skip newlines, whitespaces and possible leftover closing braces from
    // the start of the matched function text.
    char c = (char)edit.Call(SCI_GETCHARAT, ttf.chrgText.cpMin);
    auto cpmin = ttf.chrgText.cpMin;
    while ((cpmin < ttf.chrgText.cpMax) &&
        (c == '\r' || c == '\n' || c == ';' || c == '}' || c == ' ' || c == '\t'))
    {
        ++cpmin;
        c = (char)edit.Call(SCI_GETCHARAT, cpmin);
    }
    auto len = ttf.chrgText.cpMax - cpmin;
    if (len < 0)
        return false;
    foundText.resize(len + 1);
    Sci_TextRange r{ cpmin, ttf.chrgText.cpMax, &foundText[0] };
    edit.Call(SCI_GETTEXTRANGE, 0, (sptr_t)&r);
    foundText.resize(len);

    *lineNum = (int)edit.Call(SCI_LINEFROMPOSITION, cpmin);
    return true;
}

CCmdFunctions::CCmdFunctions(void* obj)
    : ICommand(obj)
    , m_autoscanlimit(-1)
    , m_edit(hRes)
{
    // Need to restart BP if you change these settings but helps
    // performance a little to inquire them here.
    static constexpr wchar_t functionsSection[] = { L"functions" };
    m_autoscan = CIniSettings::Instance().GetInt64(functionsSection, L"autoscan", 1) != 0;
    m_autoscanlimit = (long)CIniSettings::Instance().GetInt64(functionsSection, L"autoscanlimit", 1024000);
    if (!m_autoscanlimit)
        m_autoscan = false;

    m_timerID = GetTimerID();

    m_edit.InitScratch(hRes);

    InterlockedExchange(&m_bThreadRunning, FALSE);

    InterlockedExchange(&m_bRunThread, TRUE);
    m_thread = std::thread(&CCmdFunctions::ThreadFunc, this);
    m_thread.detach();
}

HRESULT CCmdFunctions::IUICommandHandlerUpdateProperty(REFPROPERTYKEY key, const PROPVARIANT* ppropvarCurrentValue, PROPVARIANT* ppropvarNewValue)
{
    HRESULT hr;

    if (key == UI_PKEY_Categories)
        return S_FALSE;

    if (key == UI_PKEY_ItemsSource)
    {
        IUICollectionPtr pCollection;
        hr = ppropvarCurrentValue->punkVal->QueryInterface(IID_PPV_ARGS(&pCollection));
        if (CAppUtils::FailedShowMessage(hr))
            return hr;
        return PopulateFunctions(pCollection);
    }

    if (key == UI_PKEY_SelectedItem)
    {
        return UIInitPropertyFromUInt32(UI_PKEY_SelectedItem, (UINT)UI_COLLECTION_INVALIDINDEX, ppropvarNewValue);
    }

    if (key == UI_PKEY_Enabled)
    {
        if (HasActiveDocument())
        {
            const auto& doc = GetActiveDocument();
            auto funcRegex = CLexStyles::Instance().GetFunctionRegexForLang(doc.GetLanguage());
            return UIInitPropertyFromBoolean(UI_PKEY_Enabled, !funcRegex.empty(), ppropvarNewValue);
        }
        return UIInitPropertyFromBoolean(UI_PKEY_Enabled, false, ppropvarNewValue);
    }

    return E_NOTIMPL;
}

HRESULT CCmdFunctions::PopulateFunctions(IUICollectionPtr& collection)
{
    HRESULT hr = S_OK;
    // The list will retain whatever from last time so clear it.
    collection->Clear();
    m_menuData.clear();

    auto docId = GetDocIdOfCurrentTab();
    if (!docId.IsValid())
        return CAppUtils::AddResStringItem(collection, IDS_NOFUNCTIONSFOUND);

#if defined(_DEBUG) || defined(PROFILING)
    ProfileTimer profileTimer(L"FunctionParse");
#endif
    auto funcProcessingStartTime = std::chrono::steady_clock::now();
    auto functions = FindFunctionsNow();
    if (functions.empty())
        return CAppUtils::AddResStringItem(collection, IDS_NOFUNCTIONSFOUND);

    // Populate the dropdown with the function details.
    for (const auto& func : functions)
    {
        hr = CAppUtils::AddStringItem(collection, CUnicodeUtils::StdGetUnicode(func.displayName).c_str());
        // If we fail to add a function, give up and assume
        // no others adds will work though try to hint at that.
        // Logically though, that might well not work either.
        if (CAppUtils::FailedShowMessage(hr))
        {
            HRESULT hrMissingHint = CAppUtils::AddStringItem(collection, L"...");
            CAppUtils::FailedShowMessage(hrMissingHint);
            return S_OK;
        }
        m_menuData.push_back(func.lineNum);
    }

    return S_OK;
}

HRESULT CCmdFunctions::IUICommandHandlerExecute(UI_EXECUTIONVERB verb, const PROPERTYKEY* key, const PROPVARIANT* ppropvarValue, IUISimplePropertySet* /*pCommandExecutionProperties*/)
{
    HRESULT hr;

    if (verb == UI_EXECUTIONVERB_EXECUTE)
    {
        if (key && (*key == UI_PKEY_SelectedItem))
        {
            // Happens when a highlighted item is selected from the drop down
            // and clicked.
            UINT selected;
            hr = UIPropertyToUInt32(*key, *ppropvarValue, &selected);
            if (CAppUtils::FailedShowMessage(hr))
                return hr;

            // The user selected a function to goto, we don't want that function
            // to remain selected because the user is supposed to
            // reselect a new one each time,so clear the selection status.
            hr = InvalidateUICommand(UI_INVALIDATIONS_PROPERTY, &UI_PKEY_SelectedItem);
            if (CAppUtils::FailedShowMessage(hr))
                return hr;

            auto docId = GetDocIdOfCurrentTab();
            if (!docId.IsValid())
                return S_FALSE;

            // Type of selected is unsigned which prevents negative tests.
            if (selected < m_menuData.size())
            {
                size_t line = m_menuData[selected];
                GotoLine((long)line);
                return S_OK;
            }
            // A "..." indicator may be present, assume it's that.
            else if (selected == m_menuData.size())
                return S_OK;
            else
            {
                // If we reach here, our internal list and menu may be out of
                // sync. It shouldn't happen but don't crash if it does.
                APPVERIFYM(false, "internal list and menu might be out of sync");
                return E_FAIL;
            }
        }
    }
    return E_FAIL;
}

void CCmdFunctions::TabNotify(TBHDR* ptbhdr)
{
    // Switching to this document.
    if (ptbhdr->hdr.code == TCN_SELCHANGE)
        InvalidateFunctionsSource();
}

void CCmdFunctions::ScintillaNotify(SCNotification* pScn)
{
    switch (pScn->nmhdr.code)
    {
        case SCN_MODIFIED:
        if ((pScn->modificationType & (SC_MOD_INSERTTEXT | SC_MOD_DELETETEXT)) != 0)
        {
            // We ignore modifications that occur before the document is dirty
            // on the assumption that the modifications are just the result of
            // loading the file initially.
            //
            // We could process modifications a bit more optimally i.e.
            // parse within the change area or something but we risk missing
            // data by only seeing a narrow part of the whole picture then.
            // Performance issues don't seem to warrant missing data yet
            // (at least on my machine) during release mode testing but
            // it is close to warranting it.
            auto docID = GetDocIdOfCurrentTab();
            auto doc = GetDocumentFromID(docID);
            if (doc.m_bIsDirty)
            {
                m_eventData.insert(docID);
                SetWorkTimer(1000);
                InvalidateFunctionsSource();
            }
        }
        break;
    }
}

void CCmdFunctions::OnTimer(UINT id)
{
    if (id == m_timerID)
    {
        // first go through all events and create a WorkItem
        // for each of them, and add them to the thread data list
        // if necessary.
        // If data is added to the thread data list, wake up the thread.
        bool bWakeupThread = false;
        for (const auto& docid : m_eventData)
        {
            auto doc = GetDocumentFromID(docid);
            m_edit.Call(SCI_SETSTATUS, SC_STATUS_OK);
            m_edit.Call(SCI_CLEARALL);
            m_edit.Call(SCI_SETDOCPOINTER, 0, doc.m_document);
            OnOutOfScope(
                m_edit.Call(SCI_SETDOCPOINTER, 0, 0);
            );

            WorkItem w;
            w.m_lang = doc.GetLanguage();
            w.m_id = docid;
            if (!w.m_lang.empty())
            {
                auto langData = CLexStyles::Instance().GetLanguageData(w.m_lang);
                if (langData != nullptr)
                {
                    if (!langData->functionregex.empty() && langData->userfunctions > 0)
                    {
                        w.m_regex = langData->functionregex;
                        w.m_trimtokens = langData->functionregextrim;

                        ProfileTimer p(L"getting doc content");
                        size_t lengthDoc = m_edit.Call(SCI_GETLENGTH);
                        if ((lengthDoc <= m_autoscanlimit) || (m_autoscanlimit == -1))
                        {
                            // get characters directly from Scintilla buffer
                            char* buf = (char*)m_edit.Call(SCI_GETCHARACTERPOINTER);
                            w.m_data = std::string(buf, lengthDoc);

                            std::unique_lock<std::mutex> lock(m_filedatamutex);
                            // if there's already a work item queued up for this document,
                            // remove it and add the new one
                            auto found = std::find_if(m_fileData.begin(), m_fileData.end(), [w](const WorkItem & wi)
                            {
                                return wi.m_id == w.m_id;
                            });
                            if (found != m_fileData.end())
                                m_fileData.erase(found);

                            m_fileData.push_back(std::move(w));
                            bWakeupThread = true;
                        }
                    }
                }
            }
        }
        m_eventData.clear();
        if (bWakeupThread)
            m_filedatacv.notify_one();

        // now go through the lang data and see if we have to update those.
        {
            std::lock_guard<std::recursive_mutex> lock(m_langdatamutex);
            for (const auto& data : m_langdata)
            {
                auto langData = CLexStyles::Instance().GetLanguageData(data.first);
                if (langData)
                {
                    auto size1 = langData->userkeywords.size();
                    langData->userkeywords.insert(data.second.begin(), data.second.end());
                    auto size2 = langData->userkeywords.size();
                    if (size1 != size2)
                        langData->userkeywordsupdated = true;
                }
            }
            m_langdata.clear();
        }

        // check if the userkeywords were updated for the active document:
        // if it was, then update the lexer data
        if (HasActiveDocument())
        {
            const auto& activeDoc = GetActiveDocument();
            auto langData = CLexStyles::Instance().GetLanguageData(activeDoc.GetLanguage());
            if (langData != nullptr && langData->userkeywordsupdated)
                SetupLexerForLang(activeDoc.GetLanguage());
        }
        KillTimer(GetHwnd(), m_timerID);
    }
}

void CCmdFunctions::OnDocumentOpen(DocID id)
{
    m_eventData.insert(id);
    InvalidateFunctionsSource();
    SetWorkTimer(1000);
}

void CCmdFunctions::OnDocumentClose(DocID id)
{
    const auto& closingDoc = GetDocumentFromID(id);

    // Purge the user keywords once there are no more 
    // documents of this language open.

    if (!closingDoc.GetLanguage().empty())
    {
        // Count the number of open documents by language.
        auto tabCount = GetTabCount();
        std::unordered_map<std::string, int> counts;
        for (decltype(tabCount) ti = 0; ti < tabCount; ++ti)
        {
            auto docID = GetDocIDFromTabIndex(ti);
            const auto& doc = GetDocumentFromID(docID);
            if (!doc.GetLanguage().empty())
                ++counts[doc.GetLanguage()];
        }
        // We assume 1 is this document so we are testing that
        // the closing document is the last of it's type.
        // If so, purge the user keywords we added. And if it relates

        if (counts[closingDoc.GetLanguage()] == 1)
        {
            auto langData = CLexStyles::Instance().GetLanguageData(closingDoc.GetLanguage());
            if (langData != nullptr)
            {
                if (!langData->userkeywords.empty())
                {
                    langData->userkeywords.clear();
                    langData->userkeywordsupdated = true;
                }
            }
        }
    }
    m_eventData.erase(id);
}

void CCmdFunctions::OnClose()
{
    InterlockedExchange(&m_bRunThread, FALSE);
    {
        std::unique_lock<std::mutex> lock(m_filedatamutex);
        m_fileData.push_back(WorkItem());
        m_filedatacv.notify_one();
    }
    int count = 50;
    while (InterlockedExchange(&m_bThreadRunning, m_bThreadRunning) && --count)
        Sleep(1000);
}

void CCmdFunctions::OnDocumentSave(DocID id, bool bSaveAs)
{
    if (bSaveAs)
    {
        m_eventData.insert(id);
        InvalidateFunctionsSource();
        SetWorkTimer(1000);
    }
}

void CCmdFunctions::OnLangChanged()
{
    m_eventData.insert(GetDocIdOfCurrentTab());
    InvalidateFunctionsSource();
    SetWorkTimer(1000);
}

void CCmdFunctions::SetWorkTimer(int ms) // 0 means as fast as possible, not never.
{
    SetTimer(GetHwnd(), m_timerID, ms, nullptr);
}

void CCmdFunctions::ThreadFunc()
{
    InterlockedExchange(&m_bThreadRunning, TRUE);
    do
    {
        WorkItem work;
        {
            std::unique_lock<std::mutex> lock(m_filedatamutex);
            m_filedatacv.wait(lock, [&]
            {
                return !m_fileData.empty();
            });
            if (!m_fileData.empty())
            {
                work = std::move(m_fileData.front());
                m_fileData.pop_front();
            }
        }
        if (!InterlockedExchange(&m_bRunThread, m_bRunThread))
            break;
        if (!work.m_regex.empty())
        {
            auto sRegex = CUnicodeUtils::StdGetUnicode(work.m_regex);
            auto sData = CUnicodeUtils::StdGetUnicode(work.m_data);

            try
            {
                std::wregex regex(sRegex, std::regex_constants::icase | std::regex_constants::ECMAScript);
                const std::wsregex_token_iterator End;
                ProfileTimer timer(L"parsing functions");
                for (std::wsregex_token_iterator match(sData.begin(), sData.end(), regex, 0); match != End; ++match)
                {
                    if (!InterlockedExchange(&m_bRunThread, m_bRunThread))
                        break;

                    auto sig = CUnicodeUtils::StdGetUTF8(match->str());

                    StripComments(sig);
                    Normalize(sig);
                    for (const auto& token : work.m_trimtokens)
                        SearchRemoveAll(sig, token);
                    CStringUtils::trim(sig);
                    std::string name;
                    if (ParseName(sig, name))
                    {
                        std::lock_guard<std::recursive_mutex> lock(m_langdatamutex);
                        m_langdata[work.m_lang].insert(std::move(name));
                    }
                }
            }
            catch (const std::exception&)
            {

            }
        }
        SetWorkTimer(0);

    } while (InterlockedExchange(&m_bRunThread, m_bRunThread));
    InterlockedExchange(&m_bThreadRunning, FALSE);
}

std::vector<FunctionInfo> CCmdFunctions::FindFunctionsNow() const
{
    std::vector<FunctionInfo> functions;
    if (!HasActiveDocument())
        return functions;

    std::function<bool(const std::string&, long)> f = [ &functions ](const std::string& sig, long lineNum)->bool
    {
        std::string name;
        std::string nameAndArgs;
        bool parsed = ParseSignature(sig, name, nameAndArgs);
        if (parsed)
            functions.emplace_back(lineNum, std::move(name), std::move(nameAndArgs));
        return true;
    };
    const auto& doc = GetActiveDocument();
    FindFunctions(doc, f);
    // Sort by name then line number.
    std::sort(functions.begin(), functions.end(),
              [](const FunctionInfo& lhs, const FunctionInfo& rhs)
    {
        auto result = _stricmp(lhs.sortName.c_str(), rhs.sortName.c_str());
        if (result != 0)
            return result < 0;
        return lhs.lineNum < rhs.lineNum;
    });

    return functions;
}

void CCmdFunctions::FindFunctions(const CDocument& doc, std::function<bool(const std::string&, long lineNum)>& callback) const
{
    const auto& docLang = doc.GetLanguage();
    if (docLang.empty())
        return;
    auto* langData = CLexStyles::Instance().GetLanguageData(docLang);
    if (!langData)
        return;

    if (langData->functionregex.empty())
        return;

    CScintillaWnd edit(hRes);
    edit.InitScratch(hRes);

    edit.Call(SCI_SETDOCPOINTER, 0, 0);
    edit.Call(SCI_SETSTATUS, SC_STATUS_OK);
    edit.Call(SCI_CLEARALL);
    edit.Call(SCI_SETDOCPOINTER, 0, doc.m_document);
    OnOutOfScope(
        edit.Call(SCI_SETDOCPOINTER, 0, 0);
    );

    int lineNum;
    std::string sig;

    Sci_TextToFind ttf{};
    Sci_PositionCR length = (Sci_PositionCR)edit.Call(SCI_GETLENGTH);
    ttf.chrg.cpMax = length;
    ttf.lpstrText = const_cast<char*>(langData->functionregex.c_str());

    for (;;)
    {
        if (!FindNext(edit, ttf, sig, &lineNum))
            break;
        ttf.chrg.cpMin = ttf.chrgText.cpMax + 1;

        StripComments(sig);
        Normalize(sig);
        if (langData->functionregextrim.empty())
            for (const auto& token : langData->functionregextrim)
                SearchRemoveAll(sig, token);
        CStringUtils::trim(sig);
        if (!callback(sig, lineNum))
            break;
    }
}

void CCmdFunctions::InvalidateFunctionsSource()
{
    HRESULT hr = InvalidateUICommand(UI_INVALIDATIONS_PROPERTY, &UI_PKEY_ItemsSource);
    CAppUtils::FailedShowMessage(hr);
    InvalidateFunctionsEnabled();
}

void CCmdFunctions::InvalidateFunctionsEnabled()
{
    // Note SetUICommandProperty(UI_PKEY_Enabled,en) can be useful but probably
    // isn't what we want; and it fails when the ribbon is hidden anyway.
    HRESULT hr;
    hr = InvalidateUICommand(UI_INVALIDATIONS_PROPERTY, &UI_PKEY_Enabled);
    CAppUtils::FailedShowMessage(hr);
}
