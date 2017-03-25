// This file is part of BowPad.
//
// Copyright (C) 2013-2017 - Stefan Kueng
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

// Overview:
//
// IMPORTANT: Testing this module can be difficult because debug mode
// performance can be drastically slower than release builds.
// Regex in particular can also be slower. See FIXME for more details.
//
// GENERAL OVERVIEW
// When events happen in the editor that effect the state of functions
// such as adding or removing text or changing the lexer etc. then
// EventHappened is called.
//
// EventHappened consolidates and stores events for that document id
// and sets a timer.
//
// When the timer expires, document ids are taken off of the event list
// and documents are scanned for functions for a small duration of time.
//
// If a scan takes a long time it terminates and reschedules itself
// to continue later so that other events can be processed and the
// system stays responsive.This timer stops once the event list becomes empty.
//
// If for a given document scanning is interrupted by another full scan event,
// then all existing events are cleared for that document and scanning
// is restarted.
//
// Modification events are stored but scanning is not acted upon immediately.
// This prevents typing from being interrupted.
//
// The net effect of all this is that closely occurring events for a given
// document are condensed into one event for that document and the most recent
// events are given priority over older events.
//
// Because large files can take a long time to scan, FindFunctions only does
// a timed amount of work in each call and splits the work over a number of timer
// events to find all the functions in a long document.
//
// The splitting of this necessitates a reasonable amount of global state
// which by nature, makes this module a bit brittle. So test changes carefully.
//
// End Overview.
//
// The incremental_search_time parameter (roughly) determines how long each unit
// of function searching lasts for, in 1000th of a second units.
// 1000 = 1 second. 100 = 1/10th of a second.
// The larger the number quicker the overall search will be, but the program
// will feel less responsive.
//
// Experiment with values for this parameter and the SetTimer calls
// to get the best balance of speed vs responsiveness.
// DON'T tune/test these values in Debug builds.
// See Overview at top of document for reasons why debug is problematic.

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

// Turn a duration into something like 1h, 1m, 1s, 1ms.
// Zero value elements are ommitted, e.g. 1h, 1ms.
static std::string DurationToString(const std::chrono::duration<double>& d)
{
    using days = std::chrono::duration
        <int, std::ratio_multiply<std::ratio<24>, std::chrono::hours::period>>;
    days ds = std::chrono::duration_cast<days>(d);
    auto rem = d - ds;
    std::chrono::hours h = std::chrono::duration_cast<std::chrono::hours>(rem);
    rem -= h;
    std::chrono::minutes m = std::chrono::duration_cast<std::chrono::minutes>(rem);
    rem -= m;
    std::chrono::seconds s = std::chrono::duration_cast<std::chrono::seconds>(rem);
    rem -= s;
    std::chrono::milliseconds ms = std::chrono::duration_cast<std::chrono::milliseconds>(rem);

    std::string result;
    if (h.count() > 0)
    {
        if (!result.empty())
            result += ',';
        result += std::to_string(h.count());
        result += 'h';
    }
    if (m.count() > 0)
    {
        if (!result.empty())
            result += ',';
        result += std::to_string(m.count());
        result += 'm';
    }
    if (s.count() > 0)
    {
        if (!result.empty())
            result += ',';
        result += std::to_string(s.count());
        result += 's';
    }
    // Empty check is to ensure at least something prints.
    if (ms.count() > 0 || result.empty())
    {
        if (!result.empty())
            result += ',';
        result += std::to_string(ms.count());
        result += "ms";
    }

    return result;
}

DocEvent::DocEvent()
    : eventType(DocEventType::None), pos(0L), len(0L)
{
}

DocEvent::DocEvent(DocEventType eventType, long pos, long len)
    : eventType(eventType), pos(pos), len(len)
{
}

void DocEvent::Clear()
{
    eventType = DocEventType::None;
}

inline bool DocEvent::Empty() const
{
    return eventType == DocEventType::None;
}

DocWork::DocWork()
    : m_langData(nullptr)
    , m_ttf({ 0 })
{
    m_inProgress = false;
}

void DocWork::ClearEvents()
{
    m_inProgress = false;
    m_events.clear();
}

void DocWork::InitLang(const CDocument& doc)
{
    m_inProgress = false;
    m_ttf = {};
    m_langData = nullptr;
    m_regex.clear();

    ClearEvents();

    m_docLang = doc.GetLanguage();
    if (!m_docLang.empty())
    {
        m_langData = CLexStyles::Instance().GetLanguageData(m_docLang);
        if (m_langData != nullptr)
        {
            if (!m_langData->functionregex.empty() && m_langData->userfunctions > 0)
            {
                m_regex = m_langData->functionregex;
                m_trimtokens = m_langData->functionregextrim;
                m_ttf.lpstrText = const_cast<char*>(m_regex.c_str());
            }
            else
            {
                m_langData = nullptr;
            }
        }
    }
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
    m_autoscanTimed = CIniSettings::Instance().GetInt64(functionsSection, L"autoscantimed", 0) != 0;

    m_timerID = GetTimerID();
    m_docID = DocID();
    m_timerPending = false;
    m_modified = false;

    m_edit.InitScratch(hRes);
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
    if (m_autoscanTimed)
    {
        auto funcProcessingEndTime = std::chrono::steady_clock::now();
        auto funcProcessingPeriod = funcProcessingEndTime - funcProcessingStartTime;
        auto msg = DurationToString(funcProcessingPeriod);
        MessageBoxA(GetHwnd(), msg.c_str(), "Function (Now) Parsing Time", MB_OK);
    }
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
            // We could process modifications a bit more optimally i.e.
            // parse within the change area or something but we risk missing
            // data by only seeing a narrow part of the whole picture then.
            // Performance issues don't seem to warrant missing data yet
            // (at least on my machine) during release mode testing but
            // it is close to warranting it.
            auto docID = GetDocIdOfCurrentTab();
            EventHappened(docID, DocEvent(DocEventType::Modified, pScn->position, pScn->length));
        }
        break;
    }
}

void CCmdFunctions::OnTimer(UINT id)
{
    if (id == m_timerID)
        BackgroundFindFunctions();
}

void CCmdFunctions::OnDocumentOpen(DocID id)
{
    EventHappened(id, DocEventType::Open);
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
    m_work.erase(id);
    if (m_docID.IsValid() && id == m_docID)
    {
        m_edit.Call(SCI_SETDOCPOINTER, 0, 0);
        m_docID = DocID();
    }
}

void CCmdFunctions::OnDocumentSave(DocID id, bool bSaveAs)
{
    if (bSaveAs)
        EventHappened(id, DocEventType::SaveAs);
}

void CCmdFunctions::OnLangChanged()
{
    EventHappened(GetDocIdOfCurrentTab(), DocEventType::LexerChanged);
}

void CCmdFunctions::SetWorkTimer(int ms) // 0 means as fast as possible, not never.
{
    SetTimer(GetHwnd(), m_timerID, ms, nullptr);
    m_timerPending = true;
}

void CCmdFunctions::EventHappened(DocID docID, DocEvent ev)
{
    if (docID.IsValid() && GetDocIdOfCurrentTab() == docID)
        InvalidateFunctionsSource();

    if (!docID.IsValid()) // Ignore anything bogus. It may happen eventually.
    {
        APPVERIFY(false);
        return;
    }

    if (!m_autoscan)
        return;

    // We want at most 1 full scan event (i.e. open or lexer event etc.)
    // to be present followed by N modified events.
    // A full scan event clears partial scan events as it makes them redundant.

    auto& work = m_work[docID];
    const auto& doc = GetDocumentFromID(docID);
    bool addEvent = true;
    switch (ev.eventType)
    {
        case DocEventType::Open: // Set language
        case DocEventType::SaveAs: // Language may change
        case DocEventType::LexerChanged: // Language may change
        work.InitLang(doc);
        break;
        case DocEventType::Modified: // New functions may appear
        if (!doc.m_bIsDirty)
            addEvent = false;
        break;
    }
    m_modified = ev.eventType == DocEventType::Modified;

    // If the language is or changes to something that doesn't support functions,
    // cancel any pending scans.
    if (work.m_langData == nullptr || work.m_regex.empty())
    {
        work.ClearEvents();
        addEvent = false;
        work.m_langData = nullptr;
    }
    else if (addEvent)
        work.m_events.push_back(ev);

    // We don't want to do any work for a while after a modification
    // because other modification events will usuaully imminently follow and we
    // want the system to be responsive to them to avoid slowing typing etc.
    // But otherwise we want to get process events quickly.
    if (addEvent)
    {
        if (m_modified)
            SetWorkTimer(1000);
        else if (!m_timerPending)
            SetWorkTimer(0);
    }
}

bool CCmdFunctions::BackgroundFindFunctions()
{
    const auto startTime = std::chrono::steady_clock::now();

    if (!m_funcProcessingStarted)
    {
        if (m_autoscanTimed)
            m_funcProcessingStartTime = startTime;
        m_funcProcessingStarted = true;
    }

    bool more = true, timeup = false;
    int lineNum;
    std::string sig, name;
    DocWork* pWork = nullptr;
    constexpr auto incremental_search_time = std::chrono::milliseconds(20);
    for (;;)
    {
        // Prefer to stick to the same document until it's done as it's more efficient.
        if (m_docID.IsValid())
            pWork = &m_work[m_docID];
        // If we've not started or finished work, look for the next thing.
        if (!pWork || pWork->m_events.empty())
        {
            auto nextWorkIt = std::find_if(m_work.begin(), m_work.end(),
                                           [](const auto& item)->bool
            {
                const DocWork& work = item.second;
                return !work.m_events.empty();
            });

            // Stop if we've nothing left/ready to do.
            // Note we leave the last document active in our scratch edit.
            if (nextWorkIt == m_work.end())
            {
                more = false;
                break;
            }

            pWork = &nextWorkIt->second;
            auto nextDocID = nextWorkIt->first;

            // If we are going to work on something new release
            // what we had (if anything) and make the new thing current.
            if (!m_docID.IsValid() || m_docID != nextDocID)
            {
                if (m_docID.IsValid())
                    m_edit.Call(SCI_SETDOCPOINTER, 0, 0);
                m_docID = nextDocID;
                m_edit.Call(SCI_SETSTATUS, SC_STATUS_OK);
                m_edit.Call(SCI_CLEARALL);
                const auto& doc = GetDocumentFromID(m_docID);
                m_edit.Call(SCI_SETDOCPOINTER, 0, doc.m_document);
            }
        }
        if (m_autoscanlimit != -1)
        {
            // If there is a size limit active, don't work on any document
            // that is or has gotten too big.
            auto lengthInBytes = (long)m_edit.Call(SCI_GETLENGTH);
            if (lengthInBytes > m_autoscanlimit)
            {
                pWork->ClearEvents();
                continue;
            }
        }

        if (!pWork->m_inProgress && !pWork->m_events.empty())
            SetSearchScope(*pWork, pWork->m_events.front());

        for (;;)
        {
            if (!FindNext(m_edit, pWork->m_ttf, sig, &lineNum))
            {
                // Break to outer loop to get next work for this or another document.
                pWork->m_inProgress = false;
                pWork->m_events.pop_front();
                break;
            }
            pWork->m_ttf.chrg.cpMin = pWork->m_ttf.chrgText.cpMax + 1;

            StripComments(sig);
            Normalize(sig);
            for (const auto& token : pWork->m_trimtokens)
                SearchRemoveAll(sig, token);
            CStringUtils::trim(sig);

            if (ParseName(sig, name))
            {
                auto result = pWork->m_langData->userkeywords.insert(std::move(name));
                if (result.second)
                    pWork->m_langData->userkeywordsupdated = true;
            }

            auto ellapsedPeriod = std::chrono::steady_clock::now() - startTime;
            if (ellapsedPeriod >= incremental_search_time)
            {
                timeup = true;
                break;
            }
        } // Loop searching for items within scope of current event.
        if (timeup)
            break;
    }
    if (more)
    {
        // If there is more work to do then schedule a wakeup to finish it unless
        // a wakeup is already queued. But if it is and relates to a modification,
        // then assume the timer is a long timer that we have already waited
        // so reset it to a shorter time so we can finish the remainding work sooner.
        // Any new modification will make the wait a long time again.
        // We try not to cancel or reset timers unless we have to.
        if (!m_timerPending || (m_timerPending && m_modified))
            SetWorkTimer(0);
    }
    else
    {
        if (m_timerPending)
        {
            KillTimer(GetHwnd(), m_timerID);
            m_timerPending = false;
        }
        m_funcProcessingStarted = false;
        if (HasActiveDocument())
        {
            const auto& activeDoc = GetActiveDocument();
            auto langData = CLexStyles::Instance().GetLanguageData(activeDoc.GetLanguage());
            if (langData != nullptr && langData->userkeywordsupdated)
                SetupLexerForLang(activeDoc.GetLanguage());
        }
        if (m_autoscanTimed)
        {
            auto funcProcessingEndTime = std::chrono::steady_clock::now();
            auto funcProcessingPeriod = funcProcessingEndTime - m_funcProcessingStartTime;
            auto msg = DurationToString(funcProcessingPeriod);
            MessageBoxA(GetHwnd(), msg.c_str(), "Autoscan Function Parsing Time", MB_OK);
        }
    }
    m_modified = false;

    return more;
}

void CCmdFunctions::SetSearchScope(DocWork& work, const DocEvent& ev) const
{
    // The look ahead and behind values are arbitary. We just want to make
    // sure we capture the whole area around any line where a change happened
    // to handle something like:
    // static
    // void
    // myfunc( /* imagine a change occured on this line */
    //     char* myarg1,
    //     char* myarg2
    // );
    const Sci_PositionCR lookBehind = 256;
    const Sci_PositionCR lookAhead = 512;

    // Set the search scope to the region indicated in the event,
    // which is specified as a starting position and a length.
    // A length of 0 means "to the end".
    const Sci_PositionCR length = (Sci_PositionCR)m_edit.ConstCall(SCI_GETLENGTH);
    Sci_PositionCR spos = ev.pos;
    Sci_PositionCR epos = ev.pos + (ev.len == 0L ? length : ev.len);
    spos -= lookBehind;
    if (spos < 0L)
        spos = 0L;
    epos += lookAhead;
    if (epos > length)
        epos = length;

    work.m_ttf.chrg.cpMin = spos;
    work.m_ttf.chrg.cpMax = epos;
    work.m_inProgress = true;
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
