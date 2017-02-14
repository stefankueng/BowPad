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

#pragma once
#include "ICommand.h"
#include "BowPadUI.h"
#include "BowPad.h"
#include "ScintillaWnd.h"
#include "LexStyles.h"

#include <string>
#include <vector>
#include <chrono>
#include <functional>
#include <unordered_set>
#include <unordered_map>
#include <deque>

struct FunctionInfo
{
    inline FunctionInfo(int lineNum, std::string&& sortName, std::string&& displayName)
        : lineNum(lineNum), sortName(std::move(sortName)), displayName(std::move(displayName))
    {
    }
    int lineNum;
    std::string sortName;
    std::string displayName;
};

enum class DocEventType
{
    None,
    // When tab changes we must know in order to:
    // 1. Clear current list of functions as they relate to the "previous" document
    // 2. Disable the functions button so the user can't push it when there are no
    // functions available yet.
    // 3. Ensure we start finding functions for this document.
    // 4  Re-enable the document.
    Open,
    // When a document opens what must we do?
    Modified,
    // When a document is modified, it may introduce new functions so we must
    // scan for them.
    LexerChanged,
    // When the lexer is changed, the regex expression for finding functions
    // changes so we must look for them again.
    SaveAs
    // If the user saves, a SaveAs might change the language/lexer type in
    // the process? If so the function list needs to be rebuilt.
};

class DocEvent
{
public:
    DocEvent();
    DocEvent(DocEventType eventType, long pos = 0L, long len = 0L);
    void Clear();
    bool Empty() const;

public:
    DocEventType eventType;
    long pos;
    long len;
};

class DocWork
{
public:
    DocWork();
    void InitLang(const CDocument& doc);
    void ClearEvents();

public:
    std::deque<DocEvent> m_events;
    Sci_TextToFind m_ttf;
    std::string m_docLang;
    std::string m_regex;
    std::vector<std::string> m_trimtokens;
    LanguageData* m_langData;
    bool m_inProgress;
};

class CCmdFunctions final : public ICommand
{
public:
    CCmdFunctions(void* obj);
    ~CCmdFunctions() = default;

    bool Execute() override { return false; }
    UINT GetCmdId() override { return cmdFunctions; }

private:
    HRESULT IUICommandHandlerUpdateProperty(REFPROPERTYKEY key, const PROPVARIANT* ppropvarCurrentValue, PROPVARIANT* ppropvarNewValue) override;
    HRESULT IUICommandHandlerExecute(UI_EXECUTIONVERB verb, const PROPERTYKEY* key, const PROPVARIANT* ppropvarValue, IUISimplePropertySet* pCommandExecutionProperties) override;

    void TabNotify(TBHDR* ptbhdr) override;
    void ScintillaNotify(SCNotification* pScn) override;
    void OnTimer(UINT id) override;
    void OnDocumentOpen(DocID id) override;
    void OnDocumentSave(DocID id, bool bSaveAs) override;
    void OnLangChanged() override;
    void OnDocumentClose(DocID id) override;
    bool BackgroundFindFunctions();
    std::vector<FunctionInfo> FindFunctionsNow() const;
    void InvalidateFunctionsEnabled();
    void InvalidateFunctionsSource();
    HRESULT PopulateFunctions(IUICollectionPtr& collection);
    void EventHappened(DocID docID, DocEvent ev);
    void FindFunctions(const CDocument& doc, std::function<bool(const std::string&, long lineNum)>& callback) const;
    void SetSearchScope(DocWork& work, const DocEvent& event) const;
    void SetWorkTimer(int ms);

private:
    bool m_autoscan;
    long m_autoscanlimit;
    bool m_autoscanTimed;
    UINT m_timerID;
    std::vector<int> m_menuData;
    std::chrono::time_point<std::chrono::steady_clock> m_funcProcessingStartTime;
    bool m_funcProcessingStarted = false;
    std::unordered_map<DocID, DocWork> m_work;
    DocID m_docID;
    CScintillaWnd m_edit;
    bool m_timerPending;
    bool m_modified;
};

