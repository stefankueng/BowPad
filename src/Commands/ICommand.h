// This file is part of BowPad.
//
// Copyright (C) 2013-2018, 2020-2022 - Stefan Kueng
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
#include "../AutoComplete.h"

#include <vector>
#include <string>
#include <UIRibbon.h>
#include <UIRibbonPropertyHelpers.h>

class CMainWindow;

namespace OpenFlags
{
constexpr unsigned int AddToMRU             = 1;
constexpr unsigned int AskToCreateIfMissing = 2;
constexpr unsigned int IgnoreIfMissing      = 4;
constexpr unsigned int OpenIntoActiveTab    = 8;
constexpr unsigned int NoActivate           = 16;
constexpr unsigned int CreateTabOnly        = 32;
constexpr unsigned int CreateIfMissing      = 64;
constexpr unsigned int NewIfMissing         = 128;
}; // namespace OpenFlags

class ICommand
{
public:
    ICommand(void* obj);
    virtual ~ICommand() {}

    /// Execute the command
    virtual bool    Execute()  = 0;
    virtual UINT    GetCmdId() = 0;
    virtual bool    IsItemsSourceCommand() { return false; }

    virtual void    ScintillaNotify(SCNotification* pScn);
    // note: the 'tabOrigin' member of the TBHDR is only valid for TCN_GETCOLOR, TCN_TABDROPPED, TCN_TABDROPPEDOUTSIDE, TCN_ORDERCHANGED
    virtual void    TabNotify(TBHDR* ptbHdr);
    virtual void    OnClose();
    virtual void    BeforeLoad();
    virtual void    AfterInit();
    virtual void    OnLangChanged();
    virtual void    OnStylesSet();
    virtual void    OnThemeChanged(bool bDark);
    virtual void    OnDocumentClose(DocID id);
    virtual void    OnDocumentOpen(DocID id);
    virtual void    OnBeforeDocumentSave(DocID id);
    virtual void    OnDocumentSave(DocID id, bool bSaveAs);
    virtual void    OnClipboardChanged();
    virtual HRESULT IUICommandHandlerUpdateProperty(REFPROPERTYKEY key, const PROPVARIANT* pPropVarCurrentValue, PROPVARIANT* pPropVarNewValue);
    virtual HRESULT IUICommandHandlerExecute(UI_EXECUTIONVERB verb, const PROPERTYKEY* key, const PROPVARIANT* pPropVarValue, IUISimplePropertySet* pCommandExecutionProperties);
    virtual void    OnTimer(UINT id);
    virtual void    OnPluginNotify(UINT cmdId, const std::wstring& pluginName, LPARAM data);

protected:
    void                      SetInsertionIndex(int index) const;
    void                      TabActivateAt(int index) const;
    void                      UpdateTab(int index) const;
    DocID                     GetDocIdOfCurrentTab() const;
    int                       GetActiveTabIndex() const;
    int                       GetTabCount() const;
    std::wstring              GetCurrentTitle() const;
    std::wstring              GetTitleForTabIndex(int index) const;
    std::wstring              GetTitleForDocID(DocID id) const;
    void                      SetCurrentTitle(LPCWSTR title) const;
    void                      SetTitleForDocID(DocID id, LPCWSTR title) const;
    int                       GetSrcTab() const;
    int                       GetDstTab() const;
    bool                      CloseTab(int index, bool bForce) const;
    DocID                     GetDocIDFromTabIndex(int tab) const;
    DocID                     GetDocIDFromPath(LPCTSTR path) const;
    int                       GetTabIndexFromDocID(DocID docID) const;
    void                      OpenNewTab() const;

    int                       GetDocumentCount() const;
    bool                      HasActiveDocument() const;
    const CDocument&          GetActiveDocument() const;
    CDocument&                GetModActiveDocument() const;
    bool                      HasDocumentID(DocID id) const;
    const CDocument&          GetDocumentFromID(DocID id) const;
    CDocument&                GetModDocumentFromID(DocID id) const;
    void                      RestoreCurrentPos(const CPosData& pos) const;
    void                      SaveCurrentPos(CPosData& pos) const;
    bool                      UpdateFileTime(CDocument& doc, bool bIncludeReadonly) const;

    Scintilla::ScintillaCall& Scintilla() const;
    LRESULT                   SendMessageToMainWnd(UINT msg, WPARAM wParam, LPARAM lParam) const;
    void                      UpdateStatusBar(bool bEverything) const;
    void                      SetupLexerForLang(const std::string& lang) const;
    std::string               GetCurrentLanguage() const;
    void                      DocScrollClear(int type) const;
    void                      DocScrollAddLineColor(int type, size_t line, COLORREF clr) const;
    void                      DocScrollUpdate() const;
    void                      DocScrollRemoveLine(int type, size_t line) const;
    void                      UpdateLineNumberWidth() const;
    void                      GotoLine(sptr_t line) const;
    void                      Center(sptr_t startPos, sptr_t endPos) const;
    void                      GotoBrace() const;
    std::string               GetLine(sptr_t line) const;
    std::string               GetTextRange(sptr_t startPos, sptr_t endPos) const;
    size_t                    FindText(const std::string& toFind, sptr_t startPos, sptr_t endPos) const;
    std::string               GetSelectedText(SelectionHandling handling) const;
    std::string               GetCurrentWord(bool select) const;
    std::string               GetCurrentLine() const;
    std::string               GetWordChars() const;
    void                      MarkSelectedWord(bool clear, bool edit) const;
    void                      ShowFileTree(bool bShow) const;
    bool                      IsFileTreeShown() const;
    std::wstring              GetFileTreePath() const;
    void                      FileTreeBlockRefresh(bool bBlock) const;

    void                      AddAutoCompleteWords(const std::string& lang, std::map<std::string, AutoCompleteType>&& words) const;
    void                      AddAutoCompleteWords(const std::string& lang, const std::map<std::string, AutoCompleteType>& words) const;
    void                      AddAutoCompleteWords(const DocID& docID, std::map<std::string, AutoCompleteType>&& words) const;
    void                      AddAutoCompleteWords(const DocID& docID, const std::map<std::string, AutoCompleteType>& words) const;

    HWND                      GetHwnd() const;
    HWND                      GetScintillaWnd() const;
    static UINT               GetTimerID() { return m_nextTimerID++; }
    int                       OpenFile(LPCWSTR file, unsigned int openFlags) const;
    void                      OpenFiles(const std::vector<std::wstring>& paths) const;
    void                      OpenHDROP(HDROP hDrop) const;
    bool                      ReloadTab(int tab, int encoding = -1) const; // By default reload encoding
    bool                      SaveCurrentTab(bool bSaveAs = false) const;
    bool                      SaveDoc(DocID docID, bool bSaveAs = false) const;
    bool                      SaveDoc(DocID docID, const std::wstring& path) const;
    HRESULT                   InvalidateUICommand(UI_INVALIDATIONS flags, const PROPERTYKEY* key);
    static HRESULT            InvalidateUICommand(UINT32 cmdId, UI_INVALIDATIONS flags, const PROPERTYKEY* key);
    static HRESULT            SetUICommandProperty(UINT32 commandId, REFPROPERTYKEY key, PROPVARIANT value);
    HRESULT                   SetUICommandProperty(REFPROPERTYKEY key, PROPVARIANT value);
    sptr_t                    GetCurrentLineNumber() const;
    void                      BlockAllUIUpdates(bool block) const;
    void                      ShowProgressCtrl(UINT delay) const;
    void                      HideProgressCtrl() const;
    void                      SetProgress(DWORD32 pos, DWORD32 end) const;
    void                      NotifyPlugins(const std::wstring& pluginName, LPARAM data);

protected:
    CMainWindow* m_pMainWindow;
    static UINT  m_nextTimerID;
};
