// This file is part of BowPad.
//
// Copyright (C) 2013-2018, 2020-2021 - Stefan Kueng
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
#include "AppUtils.h"
#include "OnOutOfScope.h"

#include <string>
#include <vector>
#include <algorithm>
#include <regex>

struct FunctionInfo
{
    sptr_t      lineNum;
    std::string sortName;
    std::string displayName;
};

// Turns "Hello/* there */world" into "Helloworld"
static void StripComments(std::string& f)
{
    constexpr char   commentBegin[]  = {"/*"};
    constexpr char   commentEnd[]    = {"*/"};
    constexpr size_t commentBeginLen = std::size(commentBegin) - 1;
    constexpr size_t commentEndLen   = std::size(commentEnd) - 1;

    for (size_t commentBeginPos = 0;;)
    {
        commentBeginPos = f.find(commentBegin, commentBeginPos);
        if (commentBeginPos == std::string::npos)
            break;
        size_t commentEndPos = f.find(commentEnd,
                                      commentBeginPos + commentBeginLen);
        if (commentEndPos == std::string::npos)
            break;
        f.erase(f.begin() + commentBeginPos,
                f.begin() + commentEndPos + commentEndLen);
    }
}

static void Normalize(std::string& f, const std::vector<std::string>& functionRegexTrim)
{
    // Remove certain chars and replace adjacent whitespace inside the string.
    // Remember to patch up the size to reflect what we remove.
    StripComments(f);
    auto e = std::remove_if(f.begin(), f.end(), [](auto c) {
        return c == '\r' || c == '{';
    });
    f.erase(e, f.end());
    std::replace_if(
        f.begin(), f.end(), [](auto c) {
            return c == '\n' || c == '\t';
        },
        ' ');
    auto newEnd = std::unique(f.begin(), f.end(), [](auto lhs, auto rhs) -> bool {
        return (lhs == ' ' && rhs == ' ');
    });
    f.erase(newEnd, f.end());
    for (const auto& token : functionRegexTrim)
        SearchRemoveAll(f, token);
    CStringUtils::trim(f);
}

static bool ParseSignature(const std::string& sig, std::string& name, std::string& nameAndArgs)
{
    static constexpr char   skip[]  = "\t :,.";
    static constexpr size_t skipLen = sizeof(skip) - 1;

    // Look for a ( of perhaps void x::f(whatever)
    if (size_t bracePos = sig.find('('); bracePos != std::string::npos)
    {
        size_t wPos = sig.find_last_of(skip, bracePos - 1, skipLen);
        size_t sPos = (wPos == std::string::npos) ? 0 : wPos + 1;

        // Functions returning pointer or reference will feature these symbols
        // before the name. Ignore them. This logic is a bit C language based.
        while (sPos < bracePos && (sig[sPos] == '*' || sig[sPos] == '&' || sig[sPos] == '^'))
            ++sPos;
        name.assign(sig, sPos, bracePos - sPos);
        nameAndArgs.assign(sig, sPos);
    }
    else
    {
        // Some languages have functions without (), or pseudo-functions like
        // properties in c#. Deal with those here.
        size_t wPos = sig.find_last_of(skip, std::string::npos, skipLen);
        size_t sPos = (wPos == std::string::npos) ? 0 : wPos + 1;

        name.assign(sig, sPos, wPos);
        nameAndArgs.assign(sig, sPos);
    }
    CStringUtils::trim(name);
    return !name.empty();
}

static bool ParseName(const std::string& sig, std::string& name)
{
    // Look for a ( of perhaps void x::f(whatever)
    auto bracePos = sig.find('(');
    if (bracePos != std::string::npos)
    {
        auto   wPos = sig.find_last_of("\t :,.", bracePos - 1, 5);
        size_t sPos = (wPos == std::string::npos) ? 0 : wPos + 1;

        // Functions returning pointer or reference will feature these symbols
        // before the name. Ignore them. This logic is a bit C language based.
        while (sPos < bracePos && (sig[sPos] == '*' || sig[sPos] == '&' || sig[sPos] == '^'))
            ++sPos;
        name.assign(sig, sPos, bracePos - sPos);
        return !name.empty();
    }
    return false;
}

static bool FindNext(
    CScintillaWnd& edit,
    Sci_PositionCR searchStart, Sci_PositionCR searchEnd,
    const char*     searchTarget,
    Sci_PositionCR& foundStart, Sci_PositionCR& foundEnd,
    std::string& foundText, sptr_t& lineNum)
{
    // In debug mode, regex takes a *long* time.
    Sci_TextToFind ttf{{searchStart, searchEnd}, searchTarget};
    auto           findRet = edit.Scintilla().FindText(Scintilla::FindOption::RegExp | Scintilla::FindOption::Cxx11RegEx, &ttf);
    if (findRet < 0)
        return false;
    foundStart = ttf.chrgText.cpMin;
    char c     = static_cast<char>(edit.Scintilla().CharAt(foundStart));
    foundEnd   = ttf.chrgText.cpMax;
    auto e     = foundStart;
    while (foundStart < foundEnd &&
           (c == '\r' || c == '\n' || c == ';' || c == '}' || c == ' ' || c == '\t'))
    {
        ++e;
        c = static_cast<char>(edit.Scintilla().CharAt(e));
    }
    foundStart    = e;
    auto matchLen = foundEnd - foundStart;
    if (matchLen < 0)
        return false;
    foundText.resize(matchLen + 1LL);
    Sci_TextRange foundRange{foundStart, foundEnd, foundText.data()};
    edit.Scintilla().GetTextRange(&foundRange);
    foundText.resize(matchLen);
    lineNum = static_cast<int>(edit.Scintilla().LineFromPosition(foundStart));
    return true;
}

CCmdFunctions::CCmdFunctions(void* obj)
    : ICommand(obj)
    , m_autoScanLimit(static_cast<size_t>(-1))
    , m_edit(g_hRes)
{
    // Need to restart BP if you change these settings but helps
    // performance a little to inquire them here.
    static constexpr wchar_t functionsSection[] = {L"functions"};
    m_autoScan                                  = CIniSettings::Instance().GetInt64(functionsSection, L"autoscan", 1) != 0;
    m_autoScanLimit                             = static_cast<long>(CIniSettings::Instance().GetInt64(functionsSection, L"autoscanlimit", 1024000));
    if (!m_autoScanLimit)
        m_autoScan = false;

    m_timerID = GetTimerID();
    m_edit.InitScratch(g_hRes);

    InterlockedExchange(&m_bThreadRunning, FALSE);
    InterlockedExchange(&m_bRunThread, TRUE);
    m_thread = std::thread(&CCmdFunctions::ThreadFunc, this);
    m_thread.detach();
}

HRESULT CCmdFunctions::IUICommandHandlerUpdateProperty(REFPROPERTYKEY key, const PROPVARIANT* pPropVarCurrentValue, PROPVARIANT* pPropVarNewValue)
{
    if (key == UI_PKEY_Categories)
        return S_FALSE;

    if (key == UI_PKEY_ItemsSource)
    {
        IUICollectionPtr pCollection;
        HRESULT          hr = pPropVarCurrentValue->punkVal->QueryInterface(IID_PPV_ARGS(&pCollection));
        if (CAppUtils::FailedShowMessage(hr))
            return hr;
        PopulateFunctions(pCollection);
        return S_OK;
    }

    if (key == UI_PKEY_SelectedItem)
    {
        return UIInitPropertyFromUInt32(UI_PKEY_SelectedItem, static_cast<UINT>(UI_COLLECTION_INVALIDINDEX), pPropVarNewValue);
    }

    if (key == UI_PKEY_Enabled)
    {
        if (HasActiveDocument())
        {
            const auto& doc       = GetActiveDocument();
            auto        funcRegex = CLexStyles::Instance().GetFunctionRegexForLang(doc.GetLanguage());
            return UIInitPropertyFromBoolean(UI_PKEY_Enabled, !funcRegex.empty(), pPropVarNewValue);
        }
        return UIInitPropertyFromBoolean(UI_PKEY_Enabled, false, pPropVarNewValue);
    }

    return E_NOTIMPL;
}

static bool SortByFunctionNameAndLineNum(const FunctionInfo& lhs, const FunctionInfo& rhs)
{
    // Sort by name, then by line number.
    // Assume something starting with a tilde is a
    // C++/C# Destructor and cause those to sort
    // as if the tilde was not present.
    // This puts like named entities together
    // regardless of either is a destructor.
    const char* lhName = lhs.sortName.c_str();
    const char* rhName = rhs.sortName.c_str();
    if (*lhName == '~')
        ++lhName;
    if (*rhName == '~')
        ++rhName;
    auto result = _stricmp(lhName, rhName);
    if (result < 0)
        return true;
    if (result == 0)
        if (lhs.lineNum < rhs.lineNum)
            return true;
    return false;
}

void CCmdFunctions::PopulateFunctions(IUICollectionPtr& collection)
{
    // The list will retain whatever from last time so clear it.
    collection->Clear();
    m_menuData.clear();
    OnOutOfScope(
        UINT collectionSize;
        if (collection->GetCount(&collectionSize) == S_OK && !collectionSize) {
            HRESULT hr = CAppUtils::AddResStringItem(collection,
                                                     IDS_NOFUNCTIONSFOUND, UI_COLLECTION_INVALIDINDEX, EMPTY_IMAGE);
            CAppUtils::FailedShowMessage(hr);
        });
    if (!HasActiveDocument())
        return;
    const auto& doc     = GetActiveDocument();
    const auto& docLang = doc.GetLanguage();
    if (docLang.empty())
        return;
    auto* langData = CLexStyles::Instance().GetLanguageData(docLang);
    if (!langData)
        return;
    if (langData->functionRegex.empty())
        return;

    std::vector<FunctionInfo> functions;
    {
        CScintillaWnd edit(g_hRes);
        edit.InitScratch(g_hRes);
        edit.Scintilla().SetDocPointer(nullptr);
        edit.Scintilla().SetStatus(Scintilla::Status::Ok);
        edit.Scintilla().ClearAll();
        edit.Scintilla().SetDocPointer(doc.m_document);
        OnOutOfScope(
            edit.Scintilla().SetDocPointer(nullptr););

#if defined(_DEBUG) || defined(PROFILING)
        ProfileTimer profileTimer(L"FunctionParse");
#endif
        sptr_t         lineNum = 0;
        std::string    sig;
        Sci_PositionCR docLength = static_cast<Sci_PositionCR>(edit.Scintilla().Length());
        for (Sci_PositionCR searchStart = 0, foundStart, foundEnd;
             FindNext(edit, searchStart, docLength,
                      langData->functionRegex.c_str(),
                      foundStart, foundEnd, sig, lineNum);
             searchStart = foundEnd + 1)
        {
            Normalize(sig, langData->functionRegexTrim);
            std::string name;
            std::string nameAndArgs;
            if (ParseSignature(sig, name, nameAndArgs))
                functions.emplace_back(lineNum, std::move(name), std::move(nameAndArgs));
        }
    }
    if (functions.empty())
        return;
    std::sort(functions.begin(), functions.end(), SortByFunctionNameAndLineNum);
    m_menuData.reserve(functions.size());
    for (const auto& func : functions)
    {
        HRESULT hr = CAppUtils::AddStringItem(collection,
                                              CUnicodeUtils::StdGetUnicode(func.displayName).c_str(), UI_COLLECTION_INVALIDINDEX, EMPTY_IMAGE);
        if (CAppUtils::FailedShowMessage(hr))
            return;
        m_menuData.push_back(func.lineNum);
    }
}

HRESULT CCmdFunctions::IUICommandHandlerExecute(UI_EXECUTIONVERB verb, const PROPERTYKEY* key, const PROPVARIANT* pPropVarValue, IUISimplePropertySet* /*pCommandExecutionProperties*/)
{
    if (verb == UI_EXECUTIONVERB_EXECUTE)
    {
        if (key && (*key == UI_PKEY_SelectedItem))
        {
            // Happens when a highlighted item is selected from the drop down
            // and clicked.
            UINT    selected;
            HRESULT hr = UIPropertyToUInt32(*key, *pPropVarValue, &selected);
            if (CAppUtils::FailedShowMessage(hr))
                return hr;

            // The user selected a function to goto, we don't want that function
            // to remain selected because the user is supposed to
            // reselect a new one each time,so clear the selection status.
            InvalidateUICommand(UI_INVALIDATIONS_VALUE, &UI_PKEY_SelectedItem);
            hr = InvalidateUICommand(UI_INVALIDATIONS_PROPERTY, &UI_PKEY_SelectedItem);
            if (CAppUtils::FailedShowMessage(hr))
                return hr;

            auto docId = GetDocIdOfCurrentTab();
            if (!docId.IsValid())
                return S_FALSE;

            // Type of selected is unsigned which prevents negative tests.
            if (selected < m_menuData.size())
            {
                auto line = m_menuData[selected];
                GotoLine(line);
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

void CCmdFunctions::TabNotify(TBHDR* ptbHdr)
{
    // Switching to this document.
    if (ptbHdr->hdr.code == TCN_SELCHANGE)
    {
        InvalidateFunctionsSource();

        auto                         docID = GetDocIdOfCurrentTab();
        std::unique_lock<std::mutex> lock(m_fileDataMutex);
        auto                         found = std::find_if(m_fileData.begin(), m_fileData.end(), [docID](const WorkItem& wi) {
            return wi.m_id == docID;
        });
        if (found != m_fileData.end())
        {
            auto workItem = *found;
            m_fileData.erase(found);
            m_fileData.push_back(std::move(workItem));
        }
        else
        {
            auto foundEvent = std::find(m_eventData.begin(), m_eventData.end(), docID);
            if (foundEvent != m_eventData.end())
            {
                m_eventData.erase(foundEvent);
                m_eventData.push_front(docID);
            }
        }
    }
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
                auto        docID = GetDocIdOfCurrentTab();
                const auto& doc   = GetDocumentFromID(docID);
                if (doc.m_bIsDirty)
                {
                    m_eventData.push_front(docID);
                    SetWorkTimer(1000);
                    InvalidateFunctionsSource();
                }
            }
            break;
        default:
            break;
    }
}

void CCmdFunctions::OnTimer(UINT id)
{
    if (id == m_timerID)
    {
        const bool limitedScan = m_autoScanLimit != static_cast<size_t>(-1);
        // first go through all events and create a WorkItem
        // for each of them, and add them to the thread data list
        // if necessary.
        // If data is added to the thread data list, wake up the thread.
        bool bWakeupThread = false;
        for (const auto docId : m_eventData)
        {
            const auto& doc  = GetDocumentFromID(docId);
            auto        lang = doc.GetLanguage();
            if (lang.empty())
                continue;
            auto langData = CLexStyles::Instance().GetLanguageData(lang);
            if (!langData ||
                langData->functionRegex.empty() ||
                langData->userFunctions <= 0 ||
                langData->autoCompleteRegex.empty())
                continue;
            // Profile
            //#if defined(_DEBUG) || defined(PROFILING)
            ProfileTimer p(L"getting doc content");
            //#endif
            m_edit.Scintilla().SetStatus(Scintilla::Status::Ok);
            m_edit.Scintilla().ClearAll();
            m_edit.Scintilla().SetDocPointer(doc.m_document);
            OnOutOfScope(
                m_edit.Scintilla().SetDocPointer(nullptr););
            size_t lengthDoc = m_edit.Scintilla().Length();
            if (limitedScan && lengthDoc > m_autoScanLimit)
                continue;
            WorkItem w;
            w.m_lang = lang;
            w.m_id   = docId;
            if (GetDocIdOfCurrentTab() == docId)
                w.m_currentPos = Scintilla().CurrentPos();
            w.m_regex      = langData->functionRegex;
            w.m_trimTokens = langData->functionRegexTrim;
            w.m_autoCRegex = langData->autoCompleteRegex;
            // get characters directly from Scintilla buffer
            const char* buf = static_cast<const char*>(m_edit.Scintilla().CharacterPointer());
            w.m_data        = std::string(buf, lengthDoc);

            std::unique_lock<std::mutex> lock(m_fileDataMutex);
            // if there's already a work item queued up for this document,
            // remove it and add the new one
            std::erase_if(m_fileData, [docId](const WorkItem& wi) {
                return wi.m_id == docId;
            });
            m_fileData.push_back(std::move(w));
            bWakeupThread = true;
        }
        m_eventData.clear();
        if (bWakeupThread)
            m_fileDataCv.notify_one();

        // now go through the lang data and see if we have to update those.
        {
            std::lock_guard<std::recursive_mutex> lock(m_langDataMutex);
            for (const auto& [lang, words] : m_langData)
            {
                if (auto langData = CLexStyles::Instance().GetLanguageData(lang); langData)
                {
                    auto size1 = langData->userKeyWords.size();
                    langData->userKeyWords.insert(words.begin(), words.end());
                    auto size2 = langData->userKeyWords.size();
                    if (size1 != size2)
                        langData->userKeyWordsUpdated = true;
                }
            }
            m_langData.clear();
        }

        // check if the userKeyWords were updated for the active document:
        // if it was, then update the lexer data
        if (HasActiveDocument())
        {
            const auto& activeDoc = GetActiveDocument();
            auto        langData  = CLexStyles::Instance().GetLanguageData(activeDoc.GetLanguage());
            if (langData != nullptr && langData->userKeyWordsUpdated)
                SetupLexerForLang(activeDoc.GetLanguage());
        }
        KillTimer(GetHwnd(), m_timerID);
    }
}

void CCmdFunctions::OnDocumentOpen(DocID id)
{
    m_eventData.erase(std::remove(m_eventData.begin(), m_eventData.end(), id), m_eventData.end());
    m_eventData.push_front(id);
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
        auto                                 tabCount = GetTabCount();
        std::unordered_map<std::string, int> counts;
        for (decltype(tabCount) ti = 0; ti < tabCount; ++ti)
        {
            auto        docID = GetDocIDFromTabIndex(ti);
            const auto& doc   = GetDocumentFromID(docID);
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
                if (!langData->userKeyWords.empty())
                {
                    langData->userKeyWords.clear();
                    langData->userKeyWordsUpdated = true;
                }
            }
        }
    }
    m_eventData.erase(std::remove(m_eventData.begin(), m_eventData.end(), id), m_eventData.end());
}

void CCmdFunctions::OnClose()
{
    InterlockedExchange(&m_bRunThread, FALSE);
    {
        std::unique_lock<std::mutex> lock(m_fileDataMutex);
        m_fileData.push_back(WorkItem());
        m_fileDataCv.notify_one();
    }
    // Wait for function processing to finish as exiting while a thread
    // is running can (and has been observed to) cause a crash that can leave
    // modified files half saved.
    // Function processing can take a long time (though usually only in debug mode)
    // in some situations.
    // e.g. when loading a large number of files from a saved session and exiting or
    // due to opening several tabs fairly quickly, such as when cursoring through files
    // in the find/replace dialog's list view (using the find files button).
    constexpr std::chrono::microseconds sleepPeriod(100);
    while (InterlockedExchange(&m_bThreadRunning, m_bThreadRunning))
        std::this_thread::sleep_for(sleepPeriod);
}

void CCmdFunctions::OnDocumentSave(DocID id, bool bSaveAs)
{
    if (bSaveAs)
    {
        m_eventData.push_front(id);
        InvalidateFunctionsSource();
        SetWorkTimer(1000);
    }
}

void CCmdFunctions::OnLangChanged()
{
    m_eventData.push_front(GetDocIdOfCurrentTab());
    InvalidateFunctionsSource();
    SetWorkTimer(1000);
}

void CCmdFunctions::SetWorkTimer(int ms) const
// 0 means as fast as possible, not never.
{
    SetTimer(GetHwnd(), m_timerID, ms, nullptr);
}

void CCmdFunctions::ThreadFunc()
{
    bool bAutoComplete = CIniSettings::Instance().GetInt64(L"View", L"autocomplete", 1) != 0;

    InterlockedExchange(&m_bThreadRunning, TRUE);
    do
    {
        WorkItem work;
        {
            std::unique_lock<std::mutex> lock(m_fileDataMutex);
            m_fileDataCv.wait(lock, [&] {
                return !m_fileData.empty();
            });
            if (!m_fileData.empty())
            {
                work = std::move(m_fileData.back());
                m_fileData.pop_back();
            }
        }
        if (!InterlockedExchange(&m_bRunThread, m_bRunThread))
            break;

        auto sData = CUnicodeUtils::StdGetUnicode(work.m_data);
        if (!work.m_regex.empty())
        {
            auto sRegex = CUnicodeUtils::StdGetUnicode(work.m_regex);

            std::map<std::string, AutoCompleteType> acMap;
            try
            {
                std::wregex                       regex(sRegex, std::regex_constants::icase | std::regex_constants::ECMAScript);
                const std::wsregex_token_iterator end;
                // Profile
                //#if defined(_DEBUG) || defined(PROFILING)
                ProfileTimer timer(L"parsing functions");
                //#endif
                for (std::wsregex_token_iterator match(sData.cbegin(), sData.cend(), regex, 0); match != end; ++match)
                {
                    if (!InterlockedExchange(&m_bRunThread, m_bRunThread))
                        break;
                    if (!match->matched)
                        continue;
                    auto sig = CUnicodeUtils::StdGetUTF8(match->str());
                    Normalize(sig, work.m_trimTokens);
                    if (std::string name; ParseName(sig, name))
                    {
                        acMap[name] = AutoCompleteType::Code;
                        std::lock_guard<std::recursive_mutex> lock(m_langDataMutex);
                        m_langData[work.m_lang].insert(std::move(name));
                    }
                    if (work.m_currentPos >= 0)
                    {
                        auto startPos = std::distance(sData.cbegin(), match->first);
                        auto endPos   = std::distance(sData.cbegin(), match->second);
                        if ((std::abs(startPos - work.m_currentPos) < 10) ||
                            (std::abs(endPos - work.m_currentPos) < 10) ||
                            (startPos < work.m_currentPos && endPos > work.m_currentPos))
                        {
                            continue;
                        }
                    }
                }
                if (!acMap.empty())
                    AddAutoCompleteWords(work.m_lang, std::move(acMap));
            }
            catch (const std::exception&)
            {
            }
        }
        if (!work.m_autoCRegex.empty() && bAutoComplete)
        {
            try
            {
                auto                        sRegex = CUnicodeUtils::StdGetUnicode(work.m_autoCRegex);
                std::wregex                 regex(sRegex, std::regex_constants::icase | std::regex_constants::ECMAScript);
                const std::wsregex_iterator end;
                // Profile
                //#if defined(_DEBUG) || defined(PROFILING)
                ProfileTimer timer(L"parsing words");
                //#endif
                std::map<std::string, AutoCompleteType> acMap;
                for (std::wsregex_iterator match(sData.begin(), sData.end(), regex); match != end; ++match)
                {
                    if (!InterlockedExchange(&m_bRunThread, m_bRunThread))
                        break;

                    for (size_t i = 1; i < match->size(); ++i)
                    {
                        if (work.m_currentPos >= 0)
                        {
                            auto startPos = match->position(i);
                            auto endPos   = startPos + match->length(i);
                            if ((std::abs(startPos - work.m_currentPos) < 10) ||
                                (std::abs(endPos - work.m_currentPos) < 10) ||
                                (startPos < work.m_currentPos && endPos > work.m_currentPos))
                            {
                                continue;
                            }
                        }
                        auto word = CUnicodeUtils::StdGetUTF8(match->str(i));
                        if (word.size() > 1)
                            acMap[std::move(word)] = AutoCompleteType::Code;
                    }
                }
                if (!acMap.empty())
                    AddAutoCompleteWords(work.m_id, std::move(acMap));
            }
            catch (const std::exception&)
            {
            }
        }

        SetWorkTimer(0);

    } while (InterlockedExchange(&m_bRunThread, m_bRunThread));
    InterlockedExchange(&m_bThreadRunning, FALSE);
}

void CCmdFunctions::InvalidateFunctionsSource()
{
    HRESULT hr = InvalidateUICommand(UI_INVALIDATIONS_PROPERTY, &UI_PKEY_ItemsSource);
    CAppUtils::FailedShowMessage(hr);
    InvalidateFunctionsEnabled();
}

void CCmdFunctions::InvalidateFunctionsEnabled()
{
    HRESULT hr = InvalidateUICommand(UI_INVALIDATIONS_PROPERTY, &UI_PKEY_Enabled);
    CAppUtils::FailedShowMessage(hr);
}
