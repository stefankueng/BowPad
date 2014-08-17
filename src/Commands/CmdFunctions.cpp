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
#include "PathUtils.h"
#include "AppUtils.h"

#include <vector>
#include <algorithm>
#include <cassert>

// Overview:
//
// IMPORTANT: This module can't be tested (meaningfully) in debug mode.
// See below for mored details.
//
// GENERAL OVERVIEW
// When events happen in the editor that effect the state of functions
// such as adding text, removing text, changing the language,
// or the current tab, etc. specific events are triggered.
//
// When these specific events are received, they delegate to ScheduleFunctionUpdate.
//
// ScheduleFunctionUpdate stores the document id of the event that has just
// occurred in a "todo" list (m_docIDs), it then sets a timer and the "todo"
// list is processed at a later time when the timer expires.
//
// When the timer expires, a document id is taken off of the todo list
// and it is scanned.
//
// If a scan takes a long time, the scan terminates and reschedules
// itself so it can continue later so that other events can be processed
// and the system stays responsive.
//
// If the scanning is interrupted by another event, the scanning is canceled
// and the interrupting event is handled instead.
//
// The net effect of all this is that closely occurring events for a given
// document are batched into one event for that document and the most recent
// events for any document are given priority over older events.
//
// Processing for any document starts when the timer event elapses.
// When that happens OnTimer is called first and that calls UpdateFunctions
// to do the function scanning.
//
// UpdateFunctions picks a document from the saved "todo"
// list of document id's and performs the scan of that document's functions.
// FindFunctions is called to to iterate through the functions
// in the document.
// If the document being scanned relates to the active document, the
// functions are saved so they can be presented to the user when they click
// the functions button.
//
// Because large files can take a long time to scan, FindFunctions only does
// a timed amount of works in each call, splitting the work over a number of timer
// events to find all the functions in a long document.
//
// The splitting of this work across timers means there is necessarily a reasonable
// amount of global state being used by FindFunctions etc.
// This by nature, makes this module a bit brittle so test changes carefully.
//
// The added complication is that regular expressions used to find functions
// is slow in debug mode. This means testing must be done in release mode
// to be meaningful and bearable. Because of this runtime asserts are used
// to verify that this module is working appropriately.
// These routines can be left enabled or disabled at the developers discretion.
// As the design is brittle, the runtime asserts are currently left on.
//
// End Overview.
//
// The incremental_search_time parameter (roughly) determines how long each unit
// of function searching lasts for, in 1000th of a second units.
// 1000 = 1 second. 100 = 1/10th of a second.
// The larger the number quicker the overall search will be, but the program
// will feel less responsive.
//
// Experiment with values for this parameter and also the reschedule times in
// ScheduleFunctionUpdate to get the best balance of speed vs responsiveness.
// DON'T tune/test these values in Debug builds.
// See Overview at top of document for reasons why debug is problematic.
//
// Work in increments of 20th of a second units.
// An incremental search time of 0 yields an untimed/unlimited search.
static const int incremental_search_time = 50;

namespace {

// Debug build testing is not practical with this module and we need to be
// sure the release tests are passing too as the performance is so different.

// Turns "Hello/* there */world" into "Helloworld"
void StripComments(std::wstring& f)
{
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

void Normalize(std::wstring& f)
{
    // Remove certain chars and replace adjacent whitespace inside the string.
    // Remember to patch up the size to reflect what we remove.
    auto e = std::remove_if(f.begin(), f.end(), [](wchar_t c)
    {
        return c == L'\r' || c == L'{';
    });
    f.erase(e, f.end());
    std::replace_if(f.begin(), f.end(), [](wchar_t c)
    {
        return c == L'\n' || c == L'\t';
    }, L' ');
    auto new_end = std::unique(f.begin(), f.end(), [](wchar_t lhs, wchar_t rhs) -> bool
    {
        return (lhs == rhs) && (lhs == L' ');
    });
    f.erase(new_end, f.end());
}

// Note: regexp incorrectly identifies functions so not everything that comes
// here will look exactly like a function. But we have to deal with it unless
// we want to write compilers for each supported language.
// So we might get a switch like this: "case ISD::SHL: switch (VT.SimpleTy)"
bool ParseSignature(const std::wstring& sig, std::wstring& name, std::wstring& name_and_args)
{
    // Find the name of the function
    name.clear();
    name_and_args.clear();
    bool parsed = false;

    // Look for a ( of perhaps void x::f(whatever)
    auto bracepos = sig.find(L'(');
    if (bracepos != std::wstring::npos)
    {
        auto wpos = sig.find_last_of(L"\t :,.", bracepos - 1, 5);
        size_t spos = (wpos == std::wstring::npos) ? 0 : wpos + 1;

        // Functions returning pointer or reference will feature these symbols
        // before the name. Ignore them. This logic is a bit C language based.
        while (spos < bracepos && (sig[spos] == L'*' || sig[spos] == L'&' || sig[spos] == L'^'))
            ++spos;
        name = sig.substr(spos, bracepos - spos);
        name_and_args = sig.substr(spos);
        CStringUtils::trim(name);
        parsed = !name.empty();
    }
    return parsed;
}

bool FindNext(CScintillaWnd& edit, const Scintilla::Sci_TextToFind& ttf,
    std::string& found_text, size_t* line_no)
{
    found_text.clear();
    *line_no = 0;
    // FIXME! In debug mode, regex takes a *long* time.
    auto findRet = edit.Call(SCI_FINDTEXT, SCFIND_REGEXP, (sptr_t)&ttf);
    if (findRet < 0)
        return false;
    // skip newlines, whitespaces and possible leftover closing braces from
    // the start of the matched function text
    char c = (char)edit.Call(SCI_GETCHARAT, ttf.chrgText.cpMin);
    long cpmin = ttf.chrgText.cpMin;
    while ((cpmin < ttf.chrgText.cpMax) && ((c == '\r') || (c == '\n') || (c == ';') || (c == '}') || (c == ' ') || (c == '\t')))
    {
        ++cpmin;
        c = (char)edit.Call(SCI_GETCHARAT, cpmin);
    }
    found_text = edit.GetTextRange(cpmin, ttf.chrgText.cpMax);
    *line_no = edit.Call(SCI_LINEFROMPOSITION, cpmin);
    return true;
}

}; // unnamed namespace

CCmdFunctions::CCmdFunctions(void * obj)
        : ICommand(obj)
        , m_edit(hRes)
        , m_searchStatus(FindFunctionsStatus::NotStarted)
        , m_functionsStatus(FindFunctionsStatus::NotStarted)
        , m_timedParts(0)
{
    m_timerID = GetTimerID();
    m_edit.InitScratch(hRes);
    // Need to restart BP if you change these settings but helps
    // performance a little to inquire them here.
    // TODO: Ready for on by default?
    m_autoscan = (CIniSettings::Instance().GetInt64(L"functions", L"autoscan", 0) != 0);
    m_functionDisplayMode = static_cast<FunctionDisplayMode>(
        CIniSettings::Instance().GetInt64(L"functions", L"function_display_mode",
        (int) FunctionDisplayMode::NameAndArgs));
    m_ttf = {};
    m_startTime = m_endTime = std::chrono::steady_clock::now();
}

int CCmdFunctions::TopDocumentId() const
{
    APPVERIFY(! m_docIDs.empty());
    return *(std::cend(m_docIDs)-1);
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
        bool enabled = false;
        // Don't ever enable the functions list unless there is a chance that
        // functions can be found.
        // If there is no regexp to find them, they can't ever be found.
        // Even if a regexp does exist to find them with, don't actually enable
        // them the functions list until we have tried and finished searching with it.
        if (HasActiveDocument())
        {
            CDocument doc = GetActiveDocument();
            std::string lang = CUnicodeUtils::StdGetUTF8(doc.m_language);
            std::string funcregex = CLexStyles::Instance().GetFunctionRegexForLang(lang);
            if (!funcregex.empty())
            {
                if (m_functionsStatus == FindFunctionsStatus::Finished)
                    enabled = true;
            }
        }
        return UIInitPropertyFromBoolean(UI_PKEY_Enabled, enabled, ppropvarNewValue);
    }

    return E_NOTIMPL;
}

HRESULT CCmdFunctions::PopulateFunctions(IUICollectionPtr& collection)
{
    HRESULT hr = S_OK;
    // The list will retain whatever from last time so clear it.
    collection->Clear();
    // We should be disabled until the data is ready, but if somehow
    // it transpires we arrive here and we're not ready, indicate that.
    if (m_functionsStatus != FindFunctionsStatus::Finished)
    {
        UpdateFunctions(true);
    }
    // If the data is ready, but there isn't any, indicate that.
    if (m_functions.empty())
        return CAppUtils::AddResStringItem(collection, IDS_NOFUNCTIONSFOUND);

    // Populate the dropdown with the function details.
    for (const auto& func : m_functions)
    {
        hr = CAppUtils::AddStringItem(collection, func.display_name.c_str());
        // If we fail to add a function, give up and assume
        // no others adds will work though try to hint at that.
        // Logically though, that might well not work either.
        if (CAppUtils::FailedShowMessage(hr))
        {
            HRESULT hrMissingHint = CAppUtils::AddStringItem(collection, L"...");
            CAppUtils::FailedShowMessage(hrMissingHint);
            return S_OK;
        }
    }

    return S_OK;
}

HRESULT CCmdFunctions::IUICommandHandlerExecute(UI_EXECUTIONVERB verb, const PROPERTYKEY* key, const PROPVARIANT* ppropvarValue, IUISimplePropertySet* /*pCommandExecutionProperties*/)
{
    HRESULT hr = E_FAIL;

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

            // Type of selected is unsigned which prevents negative tests.
            if (selected < m_functions.size())
            {
                size_t line = m_functions[selected].line;
                GotoLine((long)line);
                hr = S_OK;
            }
            // A "..." indicator may be present, assume it's that.
            else if (selected == m_functions.size())
                return S_OK;
            else
            {
                // If we reach here, our internal list and menu may be out of
                // sync. It shouldn't happen but don't crash if it does.
                hr = E_FAIL;
                APPVERIFYM(false, "internal list and menu might be out of sync");
            }
        }
    }
    return hr;
}

void CCmdFunctions::TabNotify(TBHDR * ptbhdr)
{
    // Switching to this document.
    if (ptbhdr->hdr.code == TCN_SELCHANGE)
    {
        int docId = GetDocIdOfCurrentTab();
        ScheduleFunctionUpdate(docId, FunctionUpdateReason::TabChange);
    }
}

void CCmdFunctions::ScintillaNotify(Scintilla::SCNotification * pScn)
{
    switch (pScn->nmhdr.code)
    {
        case SCN_MODIFIED:
            if (pScn->modificationType & (SC_MOD_INSERTTEXT|SC_MOD_DELETETEXT))
            {
                int docId = GetDocIdOfCurrentTab();
                if (docId < 0)
                    return;
                const CDocument& doc = this->GetDocumentFromID(docId);
                // Ignore modifications that occur before the document is dirty
                // on the assumption that the modifications are just the result of
                // loading the file initially.
                if (!doc.m_bIsDirty)
                    return;
                ScheduleFunctionUpdate(docId, FunctionUpdateReason::DocModified);
            }
            break;
    }
}

void CCmdFunctions::OnTimer(UINT id)
{
    if (id == m_timerID)
    {
        // Kill this timer now because timers re-occur by default but even if
        // we wanted that the period would likely be different anyway.
        KillTimer(GetHwnd(), id);
        if (m_autoscan)
            UpdateFunctions(false);
        else
            m_docIDs.clear();
    }
}

void CCmdFunctions::OnDocumentOpen(int index)
{
    ScheduleFunctionUpdate(GetDocIDFromTabIndex(index), FunctionUpdateReason::DocOpen);
}

void CCmdFunctions::OnDocumentSave(int index, bool bSaveAs)
{
    if (bSaveAs)
        ScheduleFunctionUpdate(GetDocIDFromTabIndex(index), FunctionUpdateReason::DocSave);
}

void CCmdFunctions::UpdateFunctions( bool bForce )
{
    // First remove any invalid documents that might exist. Perhaps
    // documents some closed before we got to them.
    RemoveNonExistantDocuments();
    // No documents, then nothing to do.
    if ( !bForce && m_docIDs.empty() )
        return;
    // Process documents on a last in first out basis,
    // roughly hoping to process the most recently changed first.
    int docId = bForce ? GetDocIdOfCurrentTab() : TopDocumentId();

    FindFunctions(docId, bForce);
    // The processing of the active document makes the functions list
    // presented to the user available.
    if (docId == GetDocIdOfCurrentTab())
        m_functionsStatus = m_searchStatus;
    if (m_searchStatus == FindFunctionsStatus::InProgress)
        ScheduleFunctionUpdate(docId,FunctionUpdateReason::DocProgress);
    else
    {
        DocumentScanFinished(docId, bForce);
        if (! m_docIDs.empty())
        {
            int nextDocId = TopDocumentId();
            ScheduleFunctionUpdate(nextDocId,FunctionUpdateReason::DocNext);
        }
    }
}

void CCmdFunctions::FindFunctions(int docID, bool bForce)
{
    if (m_searchStatus != FindFunctionsStatus::InProgress)
    {
        m_timedParts = 1;
        m_startTime = std::chrono::steady_clock::now();
    }
    else
        ++m_timedParts;
    if (docID < 0) // Need a a valid document to scan.
    {
        m_searchStatus = FindFunctionsStatus::Failed;
        return;
    }

    int activeDocId = GetDocIdOfCurrentTab();
    if (activeDocId < 0) // Need to know what the active document is.
    {
        m_searchStatus = FindFunctionsStatus::Failed;
        return;
    }

    // Detect if the scanned document is the active document.
    auto forActiveDoc = (docID == activeDocId);

    if ((forActiveDoc && m_searchStatus != FindFunctionsStatus::InProgress) || bForce)
    {
        m_functions.clear();
        m_functionsStatus = FindFunctionsStatus::NotStarted;
    }

    CDocument doc = GetDocumentFromID(docID);
    auto docLang = CUnicodeUtils::StdGetUTF8(doc.m_language);
    // Can't find functions in a document that has no language.
    if (docLang.empty())
    {
        m_searchStatus = FindFunctionsStatus::Finished;
        return;
    }

    CDocument activeDoc = GetDocumentFromID(activeDocId);
    auto activeDocLang = CUnicodeUtils::StdGetUTF8(activeDoc.m_language);

    // If starting the search, as opposed to continuing it,
    // reset everything to point to the start of the document.
    if ((m_searchStatus != FindFunctionsStatus::InProgress) || bForce)
    {
        m_edit.Call(SCI_SETSTATUS, SC_STATUS_OK);
        m_edit.Call(SCI_CLEARALL);
        m_edit.Call(SCI_SETCODEPAGE, CP_UTF8);
        m_edit.Call(SCI_SETDOCPOINTER, 0, doc.m_document);
        // The length shouldn't change without us restarting the search.
        // If it does that's a bug.
        long length = (long) m_edit.Call(SCI_GETLENGTH);
        m_funcRegex = CLexStyles::Instance().GetFunctionRegexForLang(docLang);
        m_ttf = { };
        m_ttf.chrg.cpMax = length;
        m_ttf.lpstrText = const_cast<char*>(m_funcRegex.c_str());
        if (m_funcRegex.empty())
        {
            // Nothing to do if this doc type doesn't support functions.
            m_searchStatus = FindFunctionsStatus::Finished;
            m_edit.Call(SCI_SETDOCPOINTER, 0, 0);
            return;
        }
        m_searchStatus = FindFunctionsStatus::InProgress;
    }
    else
        m_edit.Call(SCI_SETDOCPOINTER, 0, doc.m_document);

    unsigned long long start_time_point = 0;
    if (incremental_search_time > 0)
        start_time_point = GetTickCount64();
    auto trimtokens = CLexStyles::Instance().GetFunctionRegexTrimForLang(docLang);
    std::vector<std::wstring> wtrimtokens;
    for (const auto& token : trimtokens)
        wtrimtokens.push_back(CUnicodeUtils::StdGetUnicode(token));

    bool timeUp = false;
    bool updateLexer = false;
    std::string text_found;
    size_t line_no;
    std::wstring sig;
    std::wstring name;
    std::wstring name_and_args;

    for (;;)
    {
        m_ttf.chrgText.cpMin = 0;
        m_ttf.chrgText.cpMax = 0;

        // What's found isn't always a function signature. See ParseSignature.
        if (!FindNext(m_edit,m_ttf,text_found,&line_no))
            break;
        m_ttf.chrg.cpMin = m_ttf.chrgText.cpMax + 1;

        sig = CUnicodeUtils::StdGetUnicode(text_found);
        StripComments(sig);
        Normalize(sig);
        for (const auto& token : wtrimtokens)
            SearchReplace(sig, token, L"");
        CStringUtils::trim(sig);

        bool parsed = ParseSignature(sig, name, name_and_args);
        if (parsed)
        {
            if (CLexStyles::Instance().AddUserFunctionForLang(
                docLang, CUnicodeUtils::StdGetUTF8(name)))
                updateLexer = true;
        }
        // Save functions when scanning the active document,
        // as they're presented to the user.
        if (forActiveDoc)
            SaveFunctionForActiveDocument(m_functionDisplayMode, line_no,
                parsed, std::move(sig), std::move(name), std::move(name_and_args));
        if (incremental_search_time > 0)
        {
            auto end_time_point = GetTickCount64();
            auto ellapsed_time = end_time_point - start_time_point;
            if (!bForce && (ellapsed_time > incremental_search_time))
            {
                timeUp = true;
                break;
            }
        }
    }

    // If we added some functions for this document and this document
    // is of the same language as the active doc then call setup lexer
    // so the current doc can reflect those additions.
    if (updateLexer && docLang == activeDocLang)
        SetupLexerForLang(doc.m_language);

    // If not time up, we must have finished.
    if (!timeUp)
    {
        m_searchStatus = FindFunctionsStatus::Finished;
        if (forActiveDoc)
        {
            // From %appdata%\bowpad\userconfig, e.g.
            // [lang_C/C++]
            // FunctionRegexSort=X
            int sortmethod = CLexStyles::Instance().GetFunctionRegexSortForLang(activeDocLang);
            if (sortmethod)
            {
                std::sort(m_functions.begin(), m_functions.end(),
                            [](const FunctionInfo& a, const FunctionInfo& b)
                {
                    return _wcsicmp(a.sort_name.c_str(), b.sort_name.c_str()) < 0;
                });
            }
        }
    }
    m_edit.Call(SCI_SETDOCPOINTER, 0, 0);
}

void CCmdFunctions::SaveFunctionForActiveDocument(
    FunctionDisplayMode fdm, size_t line_no, bool parsed,
    std::wstring&& sig, std::wstring&& name, std::wstring&& name_and_args)
{
    if (parsed)
    {
        switch (fdm)
        {
        case FunctionDisplayMode::NameAndArgs:
            {
                // Put a space between the function name and the args to enhance readability.
                auto bracepos = name_and_args.find(L'(');
                if (bracepos != std::wstring::npos)
                    name_and_args.insert(bracepos, L" ");
                m_functions.push_back(FunctionInfo(line_no, std::move(name), std::move(name_and_args)));
            }
                break;
        case FunctionDisplayMode::Name:
            {
                std::wstring temp = name;
                m_functions.push_back(FunctionInfo(line_no, std::move(name), std::move(temp)));
            }
                break;
        case FunctionDisplayMode::Signature:
            {
                // Put a space between the function name and the args to enhance readability.
                auto bracepos = sig.find(L'(');
                if (bracepos != std::wstring::npos)
                    sig.insert(bracepos, L" ");
                m_functions.push_back(FunctionInfo(line_no, std::move(name), std::move(sig)));
            }
                break;
        } // switch
    }
    else
    {
        // Some kind of line we don't understand. It's probably not
        // a function, but put it in the list so it can be seen, then
        // someone can file bug reports on why we don't recognize it.
        name = sig;
        m_functions.push_back(FunctionInfo(line_no, std::move(sig), std::move(name)));
    }
}

void CCmdFunctions::AddDocumentToScan(int docId)
{
    APPVERIFY(docId>=0);
    // Remove any duplicate document Id's or non existent ones.
    // A bit paranoid but it's cheap to be paranoid.
    std::vector<int>::iterator foundPos;
    while ( (foundPos = std::find(std::begin(m_docIDs), std::end(m_docIDs), docId)) != std::end(m_docIDs))
        foundPos = m_docIDs.erase(foundPos);
    m_docIDs.push_back(docId);
    RemoveNonExistantDocuments();
}


void CCmdFunctions::RemoveNonExistantDocuments()
{
    for ( auto it = std::begin(m_docIDs); it != std::end(m_docIDs); )
    {
        int docID = *it;
        if (!HasDocumentID(docID))
            it = m_docIDs.erase(it);
        else
            ++it;
    }
}

void CCmdFunctions::InvalidateFunctionsSource()
{
    HRESULT hr = InvalidateUICommand(UI_INVALIDATIONS_PROPERTY, &UI_PKEY_ItemsSource);
    CAppUtils::FailedShowMessage(hr);
}

void CCmdFunctions::InvalidateFunctionsEnabled()
{
    // Note SetUICommandProperty(UI_PKEY_Enabled,en) can be useful but probably
    // isn't what we want; and it fails when the ribbon is hidden anyway.
    HRESULT hr;
    hr = InvalidateUICommand(UI_INVALIDATIONS_PROPERTY,&UI_PKEY_Enabled);
    CAppUtils::FailedShowMessage(hr);
}

void CCmdFunctions::DocumentScanInterrupted(int docId, int /*interruptingDocId*/)
{
    // Make sure we interrupted what we think we should have interrupted.
    APPVERIFY(TopDocumentId() == docId);
}

void CCmdFunctions::DocumentScanProgressing(int docId)
{
    APPVERIFY(docId == TopDocumentId());
}

void CCmdFunctions::DocumentScanFinished(int docId, bool bForce)
{
    APPVERIFY(bForce || (docId == TopDocumentId()));
    // After finishing, remove the document processed then signal
    // a new one, but ensure things start a-new.
    auto whereFound = std::find(std::begin(m_docIDs),std::end(m_docIDs), docId);
    if (whereFound != std::end(m_docIDs))
        m_docIDs.erase(whereFound);
    else
        assert(bForce);
    if (!bForce && (docId == GetDocIdOfCurrentTab()))
    {
        InvalidateFunctionsSource();
        InvalidateFunctionsEnabled();
    }
    m_endTime = std::chrono::steady_clock::now();
    std::chrono::duration<double> ellapsed = m_endTime - m_startTime;
#if TIMED_FUNCTIONS
    MessageBoxA(NULL, CStringUtils::Format("Completed in %d parts. Time taken: %f seconds\n",
        m_timedParts, ellapsed.count() / std::chrono::seconds(1).count()).c_str(), "", MB_OK);
#else
    CTraceToOutputDebugString::Instance()(_T(__FUNCTION__) L" : Completed in %d parts. Time taken: %f seconds\n", m_timedParts, ellapsed.count() / std::chrono::seconds(1).count());
#endif
}

// Called when any significant function event occurs.
// e.g. update,modified,finished,continuing etc.
// This provides one place to set a break point and know why we have come here.
void CCmdFunctions::ScheduleFunctionUpdate(int docId, FunctionUpdateReason reason)
{
    APPVERIFY(docId >= 0); // Should be a sensible value.

    // Turning autoscan off disables function scanning
    // so don't set any timers or anything just do nothing.
    if (!m_autoscan)
    {
        m_functionsStatus = FindFunctionsStatus::NotStarted;
        InvalidateFunctionsSource();
        return;
    }

    // -1 means don't set timer. 0 means trigger soon as possible.
    int updateWhen = -1;
    int activeDocId = GetDocIdOfCurrentTab();

    // If we were expecting progress but didn't receive it because
    // we received some other non progress event or
    // an event from some id other than the last event which
    // should be the id we wanted, then we must have been interrupted
    // before we could finish that document.
    if (m_searchStatus == FindFunctionsStatus::InProgress &&
        (reason != FunctionUpdateReason::DocProgress || docId != TopDocumentId()))
    {
        DocumentScanInterrupted(TopDocumentId(),docId);
        m_searchStatus = FindFunctionsStatus::NotStarted;
    }
    if (reason != FunctionUpdateReason::DocProgress)
        m_searchStatus = FindFunctionsStatus::NotStarted;

    switch (reason)
    {
    case FunctionUpdateReason::TabChange:
    case FunctionUpdateReason::DocOpen:
    case FunctionUpdateReason::DocModified:
    case FunctionUpdateReason::DocSave:
        // Throw the data we have away if we are about to process
        // the current document.
        if (docId == activeDocId)
        {
            m_functions.clear();
            m_functionsStatus = FindFunctionsStatus::NotStarted;
            InvalidateFunctionsEnabled();
        }
        AddDocumentToScan(docId);
        updateWhen = 200;
        break;
    case FunctionUpdateReason::DocNext:
        updateWhen = 50;
        break;
    case FunctionUpdateReason::DocProgress:
        DocumentScanProgressing(docId);
        updateWhen = 50;
        break;
    default:
        assert(false);
        break;
    }
    if (updateWhen >= 0)
        SetTimer(GetHwnd(), m_timerID, updateWhen, NULL);
}

