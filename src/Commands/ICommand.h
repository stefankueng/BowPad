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
#include "Scintilla.h"
#include "TabBar.h"
#include "Document.h"

#include <UIRibbon.h>
#include <UIRibbonPropertyHelpers.h>

namespace OpenFlags
{
    const unsigned int AddToMRU = 1;
    const unsigned int AskToCreateIfMissing = 2;
};

class ICommand
{
public:
    ICommand(void * obj);
    virtual ~ICommand() {}

    /// Execute the command
    virtual bool        Execute() = 0;
    virtual UINT        GetCmdId() = 0;

    virtual void        ScintillaNotify(Scintilla::SCNotification * pScn);
    virtual void        TabNotify(TBHDR * ptbhdr);
    virtual void        OnClose();
    virtual void        AfterInit();
    virtual void        OnDocumentClose(int index);
    virtual void        OnDocumentOpen(int index);
    virtual void        OnDocumentSave(int index, bool bSaveAs);
    virtual HRESULT     IUICommandHandlerUpdateProperty(REFPROPERTYKEY key, const PROPVARIANT* ppropvarCurrentValue, PROPVARIANT* ppropvarNewValue);
    virtual HRESULT     IUICommandHandlerExecute(UI_EXECUTIONVERB verb, const PROPERTYKEY* key, const PROPVARIANT* ppropvarValue, IUISimplePropertySet* pCommandExecutionProperties);
    virtual void        OnTimer(UINT id);

protected:
    void                SetInsertionIndex(int index);
    void                TabActivateAt(int index);
    void                UpdateTab(int index);
    int                 GetDocIdOfCurrentTab();
    int                 GetActiveTabIndex();
    int                 GetTabCount();
    std::wstring        GetCurrentTitle();
    std::wstring        GetTitleForTabIndex(int index);
    std::wstring        GetTitleForDocID(int id);
    void                SetCurrentTitle(LPCWSTR title);
    int                 GetSrcTab();
    int                 GetDstTab();
    bool                CloseTab(int index, bool bForce);
    int                 GetDocIDFromTabIndex(int tab);
    int                 GetDocIDFromPath(LPCTSTR path);
    int                 GetTabIndexFromDocID(int docID);

    int                 GetDocumentCount() const;
    bool                HasActiveDocument() const;
    CDocument           GetActiveDocument() const;
    bool                HasDocumentID(int id);
    CDocument           GetDocumentFromID(int id);
    void                SetDocument(int id, CDocument doc);
    void                RestoreCurrentPos(const CPosData& pos);
    void                SaveCurrentPos(CPosData * pos);

    sptr_t              ScintillaCall(unsigned int iMessage, uptr_t wParam = 0, sptr_t lParam = 0);
    LRESULT             SendMessageToMainWnd(UINT msg, WPARAM wParam, LPARAM lParam);
    void                UpdateStatusBar(bool bEverything);
    void                SetupLexerForLang(const std::wstring& lang);
    void                DocScrollClear(int type);
    void                DocScrollAddLineColor(int type, size_t line, COLORREF clr);
    void                DocScrollUpdate();
    void                DocScrollRemoveLine(int type, size_t line);
    void                GotoLine(long line);
    void                Center(long startPos, long endPos);
    void                GotoBrace();
    std::string         GetLine(long line);
    std::string         GetTextRange(long startpos, long endpos);
    size_t              FindText(const std::string& tofind, long startpos, long endpos);
    std::string         GetSelectedText();
    std::string         GetCurrentLine();


    HWND                GetHwnd();
    HWND                GetScintillaWnd();
    UINT                GetTimerID() { return m_nextTimerID++; }
    bool                OpenFile(LPCWSTR file, unsigned int openFlags);
    bool                ReloadTab(int tab, int encoding = -1); // By default reload encoding
    bool                SaveCurrentTab(bool bSaveAs = false);
    HRESULT             InvalidateUICommand(UI_INVALIDATIONS flags, const PROPERTYKEY *key);
    HRESULT             InvalidateUICommand(UINT32 cmdId, UI_INVALIDATIONS flags, const PROPERTYKEY *key);
    HRESULT             SetUICommandProperty(UINT32 commandId, REFPROPERTYKEY key, PROPVARIANT value);
    HRESULT             SetUICommandProperty(REFPROPERTYKEY key, PROPVARIANT value);

protected:
    void *              m_Obj;
    UINT                m_nextTimerID;
};
