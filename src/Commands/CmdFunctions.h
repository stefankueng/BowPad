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

#pragma once
#include "ICommand.h"
#include "BowPadUI.h"
#include "BowPad.h"
#include "ScintillaWnd.h"

#include <string>
#include <vector>
#include <unordered_map>
#include <chrono>
#include <functional>
#include <set>

enum class FunctionDataStatus
{
    Invalid, InProgress, Ready
};

struct FunctionInfo
{
    long lineNum;
    std::wstring displayName;
};

enum class DocEventType
{
    // When tab changes we must know in order to:
    // 1. Clear current list of functions as they relate to the "previous" document
    // 2. Disable the functions button so the user can't push it when there are no
    // functions available yet.
    // 3. Ensure we start finding functions for this document.
    // 4  Re-enable the document.
    DocOpen,
    // When a document opens what must we do?
    DocModified,
    // When a document is modified, it may introduce new functions so we must
    // scan for them.
    LexerChanged,
    // When the lexer is changed, the regex expression for finding functions
    // changes so we must look for them again.
    DocSave
    // If the user saves, a SaveAs might change the language/lexer type in
    // the process? If so the function list needs to be rebuilt.
};

struct DocEvent
{
    int docID;
    DocEventType eventID;
};

struct CaseInsensitiveLess
{
    bool operator()(const std::wstring& s1, const std::wstring& s2) const
    {
        return _wcsicmp(s1.c_str(), s2.c_str()) < 0;
    }
};

struct FunctionData
{
    FunctionDataStatus status = FunctionDataStatus::Invalid;
    std::map<std::wstring,std::vector<FunctionInfo>, CaseInsensitiveLess> functions;
};


class CCmdFunctions : public ICommand
{
public:
    CCmdFunctions(void* obj);
    ~CCmdFunctions();

    bool Execute() override { return false; }
    UINT GetCmdId() override { return cmdFunctions; }
    bool GotoSymbol(const std::wstring& symbolName);

private:
    HRESULT IUICommandHandlerUpdateProperty(REFPROPERTYKEY key, const PROPVARIANT* ppropvarCurrentValue, PROPVARIANT* ppropvarNewValue) override;
    HRESULT IUICommandHandlerExecute(UI_EXECUTIONVERB verb, const PROPERTYKEY* key, const PROPVARIANT* ppropvarValue, IUISimplePropertySet* pCommandExecutionProperties) override;

    void TabNotify(TBHDR* ptbhdr) override;
    void ScintillaNotify(Scintilla::SCNotification* pScn) override;
    void OnTimer(UINT id) override;
    void OnDocumentOpen(int index) override;
    void OnDocumentSave(int index, bool bSaveAs) override;
    void OnLexerChanged(int lexer) override;
    void OnDocumentClose(int index) override;
    void FindAllFunctions();
    bool FindAllFunctionsInternal();
    FunctionData FindFunctionsNow() const;
    void InvalidateFunctionsEnabled();
    void InvalidateFunctionsSource();
    HRESULT PopulateFunctions(IUICollectionPtr& collection);
    void EventHappened(int docID, DocEventType eventType);
    void FindFunctions(const CDocument& doc, std::function<bool(const std::wstring&, long lineNum)>& callback) const;

private:
    bool m_autoscan;
    bool m_autoscanTimed;
    UINT m_timerID;
    CScintillaWnd m_edit;
    std::vector<DocEvent> m_events;
    Scintilla::Sci_TextToFind m_ttf;
    std::string m_funcRegex;
    std::string m_docLang;
    int m_docID = -1;
    std::vector<std::wstring> m_wtrimtokens;
    std::vector<int> m_menuData;
    std::chrono::time_point<std::chrono::steady_clock> m_funcProcessingStartTime;
    bool m_funcProcessingStarted = false;
    std::set<std::string> m_languagesUpdated;
};

