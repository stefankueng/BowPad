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

#pragma once
#include "ICommand.h"
#include "BowPadUI.h"
#include "BowPad.h"
#include "ScintillaWnd.h"

#include <string>
#include <vector>

// Enable to test function scanning performance. If set to 1 a
// a dialog box is shown showing how long each document took to scan
// and how any many pieces it was scanned in.
#define TIMED_FUNCTIONS 0
#include <chrono>

enum class FindFunctionsStatus
{
    NotStarted,
    InProgress,
    Finished,
    Failed
};

enum class FunctionDisplayMode
{
    NameAndArgs,
    Name,
    Signature
};

struct FunctionInfo
{
    inline FunctionInfo(size_t line, std::wstring&& sort_name, std::wstring&& display_name)
        : line(std::move(line)), sort_name(std::move(sort_name)), display_name(std::move(display_name))
    {}
    size_t line;
    std::wstring sort_name;
    std::wstring display_name;
};

enum class FunctionUpdateReason
{
    Unknown,
    TabChange,
    DocOpen,
    DocModified,
    DocSave,
    DocProgress,
    DocNext
};

class CCmdFunctions : public ICommand
{
public:
    CCmdFunctions(void * obj);

    ~CCmdFunctions(void)
    {}

    virtual bool Execute() override { return false; }
    virtual UINT GetCmdId() override { return cmdFunctions; }

    virtual HRESULT IUICommandHandlerUpdateProperty(REFPROPERTYKEY key, const PROPVARIANT* ppropvarCurrentValue, PROPVARIANT* ppropvarNewValue) override;

    virtual HRESULT IUICommandHandlerExecute(UI_EXECUTIONVERB verb, const PROPERTYKEY* key, const PROPVARIANT* ppropvarValue, IUISimplePropertySet* pCommandExecutionProperties) override;

    virtual void TabNotify(TBHDR * ptbhdr) override;

    virtual void ScintillaNotify(Scintilla::SCNotification * pScn) override;

    virtual void OnTimer(UINT id) override;

    virtual void OnDocumentOpen(int id) override;

    virtual void OnDocumentSave(int index, bool bSaveAs) override;

private:

    void FindFunctions(int docID);
    void SaveFunctionForActiveDocument(FunctionDisplayMode fdm, size_t line_no,
                                       bool parsed, std::wstring&& sig, std::wstring&& name, std::wstring&& name_and_args);
    void InvalidateFunctionsEnabled();
    void InvalidateFunctionsSource();
    HRESULT PopulateFunctions(IUICollectionPtr& collection);
    void ScheduleFunctionUpdate(int docId, FunctionUpdateReason reason);
    void UpdateFunctions();
    void RemoveNonExistantDocuments();
    void AddDocumentToScan(int docId);
    void FunctionScanningComplet(int docId);
    void DocumentScanFinished(int docId);
    void DocumentScanInterrupted(int docId, int interruptingDocId);
    void DocumentScanProgressing(int docId);
    int TopDocumentId() const;

private:
    UINT m_timerID;
    std::vector<int> m_docIDs;
    CScintillaWnd m_edit;
    std::vector<FunctionInfo> m_functions;
    std::string m_funcRegex;
    Scintilla::Sci_TextToFind m_ttf;
    FindFunctionsStatus m_searchStatus;
    FindFunctionsStatus m_functionsStatus;
    bool m_autoscan;
    FunctionDisplayMode m_functionDisplayMode;

    int m_timedParts;
    std::chrono::steady_clock::time_point m_startTime;
    std::chrono::steady_clock::time_point m_endTime;
};

