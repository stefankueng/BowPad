// This file is part of BowPad.
//
// Copyright (C) 2013-2017, 2020-2021 - Stefan Kueng
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
#include <thread>
#include <mutex>

struct FunctionInfo
{
    inline FunctionInfo(sptr_t lineNum, std::string&& sortName, std::string&& displayName)
        : lineNum(lineNum)
        , sortName(std::move(sortName))
        , displayName(std::move(displayName))
    {
    }
    sptr_t      lineNum;
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

struct WorkItem
{
    DocID                    m_id;
    std::string              m_lang;
    std::string              m_regex;
    std::string              m_data;
    std::vector<std::string> m_trimtokens;
};

class CCmdFunctions final : public ICommand
{
public:
    CCmdFunctions(void* obj);
    ~CCmdFunctions() = default;

    bool Execute() override { return false; }
    UINT GetCmdId() override { return cmdFunctions; }
    bool IsItemsSourceCommand() override { return true; }

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
    void OnClose() override;

    std::vector<FunctionInfo> FindFunctionsNow() const;
    void                      InvalidateFunctionsEnabled();
    void                      InvalidateFunctionsSource();
    HRESULT                   PopulateFunctions(IUICollectionPtr& collection);
    void                      FindFunctions(const CDocument& doc, std::function<bool(const std::string&, sptr_t lineNum)>& callback) const;
    void                      SetWorkTimer(int ms);
    void                      ThreadFunc();

private:
    bool                                               m_autoscan;
    size_t                                             m_autoscanlimit;
    UINT                                               m_timerID;
    std::vector<sptr_t>                                m_menuData;
    std::chrono::time_point<std::chrono::steady_clock> m_funcProcessingStartTime;
    CScintillaWnd                                      m_edit;

    std::unordered_set<DocID>                                        m_eventData;
    std::list<WorkItem>                                              m_fileData;
    std::unordered_map<std::string, std::unordered_set<std::string>> m_langdata;
    std::thread                                                      m_thread;
    std::mutex                                                       m_filedatamutex;
    std::condition_variable                                          m_filedatacv;
    std::recursive_mutex                                             m_langdatamutex;
    volatile long                                                    m_bRunThread;
    volatile long                                                    m_bThreadRunning;
};
