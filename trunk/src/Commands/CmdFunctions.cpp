// This file is part of BowPad.
//
// Copyright (C) 2013-2016 - Stefan Kueng
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

#include <vector>
#include <algorithm>

// Overview:
//
// IMPORTANT: This module can't be tested in debug mode because the
// performance is drastically different between release and debug builds.
// See FIXME for more details.
//
// GENERAL OVERVIEW
// When events happen in the editor that effect the state of functions
// such as adding or emoving text, or changing the lexer etc,
// these events delegated to the EventHappened function to handle.
//
// EventHappened stores the document id of the event that has just
// occurred in a "todo" list and sets a timer.
// When the timer expires, a document id is taken off of the todo list
// and it is scanned for functions.
//
// If a scan takes a long time, the scan terminates and reschedules
// itself so it can continue later so that other events can be processed
// and the system stays responsive.
// This timer continues until the list becomes empty.
//
// If the scanning is interrupted by another event, the scanning is canceled
// and the interrupting event is handled instead.
//
// The net effect of all this is that closely occurring events for a given
// document are condensed into one event for that document and the most recent
// events are given priority over older events.
//
// Because large files can take a long time to scan, FindFunctions only does
// a timed amount of works in each call, splitting the work over a number of timer
// events to find all the functions in a long document.
//
// The splitting of this work across timers means there is necessarily a reasonable
// amount of global state being used by FindFunctions etc.
// This by nature, makes this module a bit brittle so test changes carefully.
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
// EventHappened to get the best balance of speed vs responsiveness.
// DON'T tune/test these values in Debug builds.
// See Overview at top of document for reasons why debug is problematic.

namespace {

// Work in n millisecond units.
// An incremental search time of 0 yields an untimed/unlimited search.
const constexpr auto incremental_search_time = std::chrono::milliseconds(20);

// Debug build testing is not practical with this module and we need to be
// sure the release tests are passing too as the performance is so different.

// Turns "Hello/* there */world" into "Helloworld"
void StripComments(std::wstring& f)
{
    static const wchar_t comment_begin[] = { L"/*" };
    static const wchar_t comment_end[] = { L"*/" };
    const size_t comment_begin_len = wcslen(comment_begin);
    const size_t comment_end_len = wcslen(comment_end);
    size_t comment_begin_pos = 0;
    for (;;)
    {
        comment_begin_pos = f.find(comment_begin, comment_begin_pos);
        if (comment_begin_pos == std::wstring::npos)
            break;
        auto comment_end_pos = f.find(comment_end,
            comment_begin_pos + comment_begin_len);
        if (comment_end_pos == std::wstring::npos)
            break;
        auto e = f.erase(f.begin() + comment_begin_pos,
            f.begin() + comment_end_pos + comment_end_len);
        std::remove(e, f.end(), L'\0');
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
        return (lhs == L' ' && rhs == L' ');
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
        name.assign(sig, spos, bracepos - spos);
        name_and_args.assign(sig, spos);
        CStringUtils::trim(name);
        parsed = !name.empty();
    }
    return parsed;
}

bool ParseName(const std::wstring& sig, std::wstring& name)
{
    name.clear();
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
        name.assign(sig, spos, bracepos - spos);
        return !name.empty();
    }
    return false;
}

bool FindNext(CScintillaWnd& edit, const Scintilla::Sci_TextToFind& ttf,
    std::string& foundText, long* lineNum)
{
    foundText.clear();
    *lineNum = 0L;
    // FIXME! In debug mode, regex takes a *long* time.
    auto findRet = edit.Call(SCI_FINDTEXT, SCFIND_REGEXP, (sptr_t)&ttf);
    if (findRet < 0)
        return false;
    // Skip newlines, whitespaces and possible leftover closing braces from
    // the start of the matched function text.
    char c = (char)edit.Call(SCI_GETCHARAT, ttf.chrgText.cpMin);
    long cpmin = ttf.chrgText.cpMin;
    while ((cpmin < ttf.chrgText.cpMax) && 
        (c == '\r' || c == '\n' || c == ';' || c == '}' || c == ' ' || c == '\t'))
    {
        ++cpmin;
        c = (char)edit.Call(SCI_GETCHARAT, cpmin);
    }
    foundText = edit.GetTextRange(cpmin, ttf.chrgText.cpMax);
    *lineNum = (long) edit.Call(SCI_LINEFROMPOSITION, cpmin);
    return true;
}

}; // unnamed namespace

CCmdFunctions::CCmdFunctions(void* obj)
        : ICommand(obj)
        , m_edit(hRes)
{
    m_timerID = GetTimerID();
    m_edit.InitScratch(hRes);
    // Need to restart BP if you change these settings but helps
    // performance a little to inquire them here.
    m_autoscan = (CIniSettings::Instance().GetInt64(L"functions", L"autoscan", 1) != 0);
    m_ttf = {};
}

CCmdFunctions::~CCmdFunctions()
{
    if (m_docID != -1)
        m_edit.Call(SCI_SETDOCPOINTER, 0, 0);
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

    //if (key == UI_PKEY_Enabled)
        //return UIInitPropertyFromBoolean(UI_PKEY_Enabled, enabled, ppropvarNewValue);

    return E_NOTIMPL;
}

HRESULT CCmdFunctions::PopulateFunctions(IUICollectionPtr& collection)
{
    HRESULT hr = S_OK;
    // The list will retain whatever from last time so clear it.
    collection->Clear();
    menuData.clear();
    if (!HasActiveDocument())
        return CAppUtils::AddResStringItem(collection, IDS_NOFUNCTIONSFOUND);
 
    auto docId = GetDocIdOfCurrentTab();
    if (docId < 0)
        return S_FALSE;
    auto doc = GetActiveDocument();
    auto functionData = FindFunctionsNow();

    // If the data is ready, but there isn't any, indicate that.
    if (functionData.functions.empty())
        return CAppUtils::AddResStringItem(collection, IDS_NOFUNCTIONSFOUND);

    // Populate the dropdown with the function details.
    for (const auto& func : functionData.functions)
    {
        for (const auto& funcInfo : func.second)
        {
            hr = CAppUtils::AddStringItem(collection, funcInfo.displayName.c_str());
            // If we fail to add a function, give up and assume
            // no others adds will work though try to hint at that.
            // Logically though, that might well not work either.
            if (CAppUtils::FailedShowMessage(hr))
            {
                HRESULT hrMissingHint = CAppUtils::AddStringItem(collection, L"...");
                CAppUtils::FailedShowMessage(hrMissingHint);
                return S_OK;
            }
            menuData.push_back(funcInfo.lineNum);
        }
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
            if (docId < 0)
                return S_FALSE;

            // Type of selected is unsigned which prevents negative tests.
            if (selected < menuData.size())
            {
                size_t line = menuData[selected];
                GotoLine((long)line);
                return S_OK;
            }
            // A "..." indicator may be present, assume it's that.
            else if (selected == menuData.size())
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

void CCmdFunctions::TabNotify(TBHDR * ptbhdr)
{
    // Switching to this document.
    if (ptbhdr->hdr.code == TCN_SELCHANGE)
        InvalidateFunctionsSource();
}

void CCmdFunctions::ScintillaNotify(Scintilla::SCNotification * pScn)
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
                const auto& doc = GetDocumentFromID(docID);
                if (doc.m_bIsDirty)
                    EventHappened(docID, DocEventType::DocModified);
            }
            break;
    }
}

void CCmdFunctions::OnTimer(UINT id)
{
    if (id == m_timerID)
    {
        KillTimer(GetHwnd(), id);
        FindAllFunctions();
    }
}

void CCmdFunctions::OnDocumentOpen(int index)
{
    EventHappened(GetDocIDFromTabIndex(index), DocEventType::DocOpen);
}

void CCmdFunctions::OnDocumentClose(int index)
{
    auto docID = GetDocIDFromTabIndex(index);
    auto newEnd = std::remove_if(m_events.begin(), m_events.end(),
        [&](const DocEvent& e) { return e.docID == docID; });
    m_events.erase(newEnd, m_events.end());
}

void CCmdFunctions::OnDocumentSave(int index, bool bSaveAs)
{
    if (bSaveAs)
        EventHappened(GetDocIDFromTabIndex(index), DocEventType::DocSave);
}

void CCmdFunctions::OnLexerChanged(int /*lexer*/)
{
    EventHappened(GetDocIdOfCurrentTab(), DocEventType::LexerChanged);
}

void CCmdFunctions::EventHappened(int docID, DocEventType eventType)
{
    if (docID < 0) // Ignore anything bogus. Is likely to happen eventually.
        return;
    // If we are alreading doing this, cancel it in preperation for redoing it.
    if (m_docID != -1 && m_docID == docID)
    {
        m_edit.Call(SCI_SETDOCPOINTER, 0, 0);
        m_docID = -1;
    }

    KillTimer(GetHwnd(), m_timerID);
    if (docID != -1 && GetDocIdOfCurrentTab() == docID)
        InvalidateFunctionsSource();

    auto newEnd = std::remove_if(m_events.begin(), m_events.end(),
        [&](const DocEvent& e) { return e.docID == docID; });
    m_events.erase(newEnd, m_events.end());
    m_events.push_back({ docID, eventType });
    SetTimer(GetHwnd(), m_timerID, 10, nullptr);
}

void CCmdFunctions::FindAllFunctions()
{
    if (m_docID == -1)
    {
        m_docLang.clear();
        for (;;)
        {
            if (m_events.empty())
                return;
            DocEvent event = m_events.back();
            // Remove all other events relating to this document because we
            // currently process all of them the same way.
            auto newEnd = std::remove_if(m_events.begin(), m_events.end(),
                [&](const DocEvent& e) { return e.docID == event.docID; });
            m_events.erase(newEnd, m_events.end());
            auto docID = event.docID;
            if (!HasDocumentID(docID)) // If it's gone, what can we do.
                continue;

            const auto& doc = GetDocumentFromID(docID);

            m_docLang = CUnicodeUtils::StdGetUTF8(doc.m_language);
            if (m_docLang.empty())
                continue;
            m_funcRegex = CLexStyles::Instance().GetFunctionRegexForLang(m_docLang);
            if (m_funcRegex.empty())
                continue;
            m_docID = docID;
            m_edit.Call(SCI_SETDOCPOINTER, 0, 0);
            m_edit.Call(SCI_SETSTATUS, SC_STATUS_OK);
            m_edit.Call(SCI_CLEARALL);
            m_edit.Call(SCI_SETDOCPOINTER, 0, doc.m_document);
            m_edit.Call(SCI_SETCODEPAGE, CP_UTF8);

            m_ttf = {};
            break;
        }

        long length = (long)m_edit.Call(SCI_GETLENGTH);
        m_ttf.chrg.cpMax = length;
        m_ttf.lpstrText = const_cast<char*>(m_funcRegex.c_str());

        auto trimtokens = CLexStyles::Instance().GetFunctionRegexTrimForLang(m_docLang);
        m_wtrimtokens.clear();
        for (const auto& token : trimtokens)
            m_wtrimtokens.push_back(CUnicodeUtils::StdGetUnicode(token));
    }

    std::string textFound;
    long lineNum;
    std::wstring sig;
    std::wstring name;
    bool addedToLexer = false;
    bool tbc = false;

    auto& lexStyles = CLexStyles::Instance();
    const auto startTime = std::chrono::steady_clock::now();
    for (;;)
    {
        if (!FindNext(m_edit, m_ttf, textFound, &lineNum))
            break;
        m_ttf.chrg.cpMin = m_ttf.chrgText.cpMax + 1;

        sig = CUnicodeUtils::StdGetUnicode(textFound);
        StripComments(sig);
        Normalize(sig);
        for (const auto& token : m_wtrimtokens)
            SearchRemoveAll(sig, token);
        CStringUtils::trim(sig);

        bool parsed = ParseName(sig, name);
        if (parsed)
        {
            if (lexStyles.AddUserFunctionForLang(
                m_docLang, CUnicodeUtils::StdGetUTF8(name)))
                addedToLexer = true;
        }
        auto timeNow = std::chrono::steady_clock::now();
        auto ellapsedPeriod = timeNow - startTime;
        if (ellapsedPeriod >= incremental_search_time)
        {
            tbc = true;
            break;
        }
    }

    if (!tbc)
    {
        m_edit.Call(SCI_SETDOCPOINTER, 0, 0);
        m_docID = -1;
    }

    if (addedToLexer && HasActiveDocument())
    {
        auto activeDoc = GetActiveDocument();
        auto activeDocLang = CUnicodeUtils::StdGetUTF8(activeDoc.m_language);
        if (m_docLang == activeDocLang)
            SetupLexerForLang(activeDoc.m_language);
    }
    // Re-schedule if we need to continue our existing work or there is other work to do.
    if (tbc || ! m_events.empty())
        SetTimer(GetHwnd(), m_timerID, 0, nullptr);
}

FunctionData CCmdFunctions::FindFunctionsNow() const
{
    FunctionData functions;
    functions.status = FunctionDataStatus::Ready;
    if (!HasActiveDocument())
        return functions;

    auto doc = GetActiveDocument();

    std::wstring name;
    std::wstring nameAndArgs;

    std::function<bool(const std::wstring&, long)> f = [&](const std::wstring& sig, long lineNum)->bool
    {
        bool parsed = ParseSignature(sig, name, nameAndArgs);
        if (parsed)
            functions.functions[name].push_back({ lineNum, std::move(nameAndArgs) });
        return true;
    };
    FindFunctions(doc, f);

    return functions;
}

void CCmdFunctions::FindFunctions(const CDocument& doc, std::function<bool(const std::wstring&, long lineNum)>& callback) const
{
    auto docLang = CUnicodeUtils::StdGetUTF8(doc.m_language);
    if (docLang.empty())
        return;
    auto funcRegex = CLexStyles::Instance().GetFunctionRegexForLang(docLang);
    if (funcRegex.empty())
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
    edit.Call(SCI_SETCODEPAGE, CP_UTF8);

    auto trimtokens = CLexStyles::Instance().GetFunctionRegexTrimForLang(docLang);
    std::vector<std::wstring> wtrimtokens;
    for (const auto& token : trimtokens)
        wtrimtokens.push_back(CUnicodeUtils::StdGetUnicode(token));

    std::string textFound;
    long lineNum;
    std::wstring sig;

    Scintilla::Sci_TextToFind ttf = {};
    long length = (long)edit.Call(SCI_GETLENGTH);
    ttf.chrg.cpMax = length;
    ttf.lpstrText = const_cast<char*>(funcRegex.c_str());

    for (;;)
    {
        if (!FindNext(edit, ttf, textFound, &lineNum))
            break;
        ttf.chrg.cpMin = ttf.chrgText.cpMax + 1;

        sig = CUnicodeUtils::StdGetUnicode(textFound);
        StripComments(sig);
        Normalize(sig);
        for (const auto& token : wtrimtokens)
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
}

void CCmdFunctions::InvalidateFunctionsEnabled()
{
    // Note SetUICommandProperty(UI_PKEY_Enabled,en) can be useful but probably
    // isn't what we want; and it fails when the ribbon is hidden anyway.
    HRESULT hr;
    hr = InvalidateUICommand(UI_INVALIDATIONS_PROPERTY,&UI_PKEY_Enabled);
    CAppUtils::FailedShowMessage(hr);
}

bool CCmdFunctions::GotoSymbol(const std::wstring& symbolName)
{
    bool done = false;
    std::wstring sig;
    std::wstring name;
    std::wstring nameAndArgs;
    int tabCount = GetTabCount();
    for (int i = 0; i < tabCount; ++i)
    {
        int docId = this->GetDocIDFromTabIndex(i);
        const auto& doc = GetDocumentFromID(docId);
        std::function<bool(const std::wstring& sig, long lineNum)> f =
        [&](const std::wstring& sig, long lineNum)->bool
        {
            bool parsed = ParseSignature(sig, name, nameAndArgs);
            // TODO! Where multiple matches exist offer up the Find dialog to pick one,
            // instead of automatically picking the first one.
            if (parsed && _wcsicmp(name.c_str(), symbolName.c_str()) == 0)
            {
                if (!OpenFile(doc.m_path.c_str(), 0))
                    return false;
                GotoLine(lineNum);
                done = true;
                return false;
            }
            return true;
        };

        FindFunctions(doc, f);
    }

    if (!done)
        MessageBeep(~0U);

    return done;
}