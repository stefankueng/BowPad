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
#include "Scintilla.h"
#include "TabBar.h"
#include "Document.h"

#include <vector>
#include <string>
#include <UIRibbon.h>
#include <UIRibbonPropertyHelpers.h>

class CMainWindow;

namespace OpenFlags
{
    const unsigned int AddToMRU = 1;
    const unsigned int AskToCreateIfMissing = 2;
    const unsigned int IgnoreIfMissing = 4;
    const unsigned int OpenIntoActiveTab = 8;
    const unsigned int NoActivate = 16;
    const unsigned int CreateTabOnly = 32;
    const unsigned int CreateIfMissing = 64;
};

class ICommand
{
public:
    ICommand(void * obj);
    virtual ~ICommand() {}

    /// Execute the command
    virtual bool        Execute() = 0;
    virtual UINT        GetCmdId() = 0;

    virtual void        ScintillaNotify(SCNotification * pScn);
    // note: the 'tabOrigin' member of the TBHDR is only valid for TCN_GETCOLOR, TCN_TABDROPPED, TCN_TABDROPPEDOUTSIDE, TCN_ORDERCHANGED
    virtual void        TabNotify(TBHDR * ptbhdr);
    virtual void        OnClose();
    virtual void        AfterInit();
    virtual void        OnLangChanged();
    virtual void        OnThemeChanged(bool bDark);
    virtual void        OnDocumentClose(DocID id);
    virtual void        OnDocumentOpen(DocID id);
    virtual void        OnDocumentSave(DocID id, bool bSaveAs);
    virtual HRESULT     IUICommandHandlerUpdateProperty(REFPROPERTYKEY key, const PROPVARIANT* ppropvarCurrentValue, PROPVARIANT* ppropvarNewValue);
    virtual HRESULT     IUICommandHandlerExecute(UI_EXECUTIONVERB verb, const PROPERTYKEY* key, const PROPVARIANT* ppropvarValue, IUISimplePropertySet* pCommandExecutionProperties);
    virtual void        OnTimer(UINT id);

protected:
    void                SetInsertionIndex(int index);
    void                TabActivateAt(int index);
    void                UpdateTab(int index);
    DocID               GetDocIdOfCurrentTab() const;
    int                 GetActiveTabIndex() const;
    int                 GetTabCount() const;
    std::wstring        GetCurrentTitle() const;
    std::wstring        GetTitleForTabIndex(int index) const;
    std::wstring        GetTitleForDocID(DocID id) const;
    void                SetCurrentTitle(LPCWSTR title);
    int                 GetSrcTab();
    int                 GetDstTab();
    bool                CloseTab(int index, bool bForce);
    DocID               GetDocIDFromTabIndex(int tab) const;
    DocID               GetDocIDFromPath(LPCTSTR path) const;
    int                 GetTabIndexFromDocID(DocID docID) const;

    int                 GetDocumentCount() const;
    bool                HasActiveDocument() const;
    const CDocument&    GetActiveDocument() const;
    CDocument&          GetModActiveDocument();
    bool                HasDocumentID(DocID id) const;
    const CDocument&    GetDocumentFromID(DocID id) const;
    CDocument&          GetModDocumentFromID(DocID id);
    void                RestoreCurrentPos(const CPosData& pos);
    void                SaveCurrentPos(CPosData& pos);

    sptr_t              ScintillaCall(unsigned int iMessage, uptr_t wParam = 0, sptr_t lParam = 0);
    sptr_t              ConstCall(unsigned int iMessage, uptr_t wParam = 0, sptr_t lParam = 0) const;
    LRESULT             SendMessageToMainWnd(UINT msg, WPARAM wParam, LPARAM lParam);
    void                UpdateStatusBar(bool bEverything);
    void                SetupLexerForLang(const std::string& lang);
    void                DocScrollClear(int type);
    void                DocScrollAddLineColor(int type, size_t line, COLORREF clr);
    void                DocScrollUpdate();
    void                DocScrollRemoveLine(int type, size_t line);
    void                GotoLine(long line);
    void                Center(sptr_t startPos, sptr_t endPos);
    void                GotoBrace();
    std::string         GetLine(long line) const;
    std::string         GetTextRange(long startpos, long endpos) const;
    size_t              FindText(const std::string& tofind, long startpos, long endpos);
    std::string         GetSelectedText(bool useCurrentWordIfSelectionEmpty = false) const;
    std::string         GetCurrentWord() const;
    std::string         GetCurrentLine() const;
    std::string         GetWordChars() const;
    void                ShowFileTree(bool bShow);
    bool                IsFileTreeShown() const;
    std::wstring        GetFileTreePath() const;
    void                FileTreeBlockRefresh(bool bBlock);


    HWND                GetHwnd() const;
    HWND                GetScintillaWnd() const;
    UINT                GetTimerID() { return m_nextTimerID++; }
    int                 OpenFile(LPCWSTR file, unsigned int openFlags);
    void                OpenFiles(const std::vector<std::wstring>& paths);
    void                OpenHDROP(HDROP hDrop);
    bool                ReloadTab(int tab, int encoding = -1); // By default reload encoding
    bool                SaveCurrentTab(bool bSaveAs = false);
    bool                SaveDoc(DocID docID, bool bSaveAs = false);
    HRESULT             InvalidateUICommand(UI_INVALIDATIONS flags, const PROPERTYKEY *key);
    HRESULT             InvalidateUICommand(UINT32 cmdId, UI_INVALIDATIONS flags, const PROPERTYKEY *key);
    HRESULT             SetUICommandProperty(UINT32 commandId, REFPROPERTYKEY key, PROPVARIANT value);
    HRESULT             SetUICommandProperty(REFPROPERTYKEY key, PROPVARIANT value);
    long                GetCurrentLineNumber() const;
    void                BlockAllUIUpdates(bool block);
    void                ShowProgressCtrl(UINT delay);
    void                HideProgressCtrl();
    void                SetProgress(DWORD32 pos, DWORD32 end);

private:

protected:
    CMainWindow*        m_pMainWindow;
    static UINT         m_nextTimerID;
};
