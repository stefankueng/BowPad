// This file is part of BowPad.
//
// Copyright (C) 2013-2018, 2020 - Stefan Kueng
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
#include "ICommand.h"
#include "MainWindow.h"
#include "StringUtils.h"

extern IUIFramework *g_pFramework;

UINT ICommand::m_nextTimerID = COMMAND_TIMER_ID_START;


ICommand::ICommand(void * obj)
    : m_pMainWindow(static_cast<CMainWindow*>(obj))
{
}

HRESULT ICommand::IUICommandHandlerUpdateProperty( REFPROPERTYKEY /*key*/, const PROPVARIANT* /*ppropvarCurrentValue*/, PROPVARIANT* /*ppropvarNewValue*/ )
{
    return E_NOTIMPL;
}

HRESULT ICommand::IUICommandHandlerExecute( UI_EXECUTIONVERB /*verb*/, const PROPERTYKEY* /*key*/, const PROPVARIANT* /*ppropvarValue*/, IUISimplePropertySet* /*pCommandExecutionProperties*/ )
{
    return E_NOTIMPL;
}

void ICommand::ScintillaNotify( SCNotification * /*pScn*/ )
{
}

void ICommand::TabNotify( TBHDR * /*ptbhdr*/ )
{
}

void ICommand::OnClose()
{
}

void ICommand::BeforeLoad()
{
}

void ICommand::AfterInit()
{
}

void ICommand::OnDocumentClose(DocID /*id*/)
{
}

void ICommand::OnDocumentOpen(DocID /*id*/)
{}

void ICommand::OnDocumentSave(DocID /*id*/, bool /*bSaveAs*/)
{}

void ICommand::OnTimer(UINT /*id*/)
{
}

void ICommand::OnThemeChanged(bool /*bDark*/)
{
}

void ICommand::OnLangChanged()
{
}

void ICommand::TabActivateAt( int index )
{
    m_pMainWindow->m_TabBar.ActivateAt(index);
}

void ICommand::UpdateTab(int index)
{
    m_pMainWindow->UpdateTab(GetDocIDFromTabIndex(index));
}

int ICommand::GetActiveTabIndex() const
{
    return m_pMainWindow->m_TabBar.GetCurrentTabIndex();
}

DocID ICommand::GetDocIdOfCurrentTab() const
{
    return m_pMainWindow->m_TabBar.GetCurrentTabId();
}

std::wstring ICommand::GetCurrentTitle() const
{
    return m_pMainWindow->m_TabBar.GetCurrentTitle();
}

std::wstring ICommand::GetTitleForTabIndex(int index) const
{
    return m_pMainWindow->m_TabBar.GetTitle(index);
}

std::wstring ICommand::GetTitleForDocID(DocID id) const
{
    return GetTitleForTabIndex(GetTabIndexFromDocID(id));
}

void ICommand::SetCurrentTitle(LPCWSTR title)
{
    m_pMainWindow->m_TabBar.SetCurrentTitle(title);
    m_pMainWindow->UpdateCaptionBar();
}

void ICommand::SetTitleForDocID(DocID id, LPCWSTR title)
{
    m_pMainWindow->m_TabBar.SetTitle(m_pMainWindow->m_TabBar.GetIndexFromID(id), title);
    m_pMainWindow->UpdateCaptionBar();
}

int ICommand::GetSrcTab()
{
    return m_pMainWindow->m_TabBar.GetSrcTab();
}

int ICommand::GetDstTab()
{
    return m_pMainWindow->m_TabBar.GetDstTab();
}

int ICommand::GetTabCount() const
{
    return m_pMainWindow->m_TabBar.GetItemCount();
}

bool ICommand::CloseTab( int index, bool bForce )
{
    return m_pMainWindow->CloseTab(index, bForce);
}

sptr_t ICommand::ScintillaCall( unsigned int iMessage, uptr_t wParam /*= 0*/, sptr_t lParam /*= 0*/ )
{
    return m_pMainWindow->m_editor.Call(iMessage, wParam, lParam);
}

sptr_t ICommand::ConstCall(unsigned int iMessage, uptr_t wParam /*= 0*/, sptr_t lParam /*= 0*/) const
{
    return m_pMainWindow->m_editor.Call(iMessage, wParam, lParam);
}

HWND ICommand::GetHwnd() const
{
    return *m_pMainWindow;
}

HWND ICommand::GetScintillaWnd() const
{
    return m_pMainWindow->m_editor;
}

int ICommand::OpenFile(LPCWSTR file, unsigned int openFlags)
{
    return m_pMainWindow->OpenFile(file, openFlags);
}

void ICommand::OpenFiles(const std::vector<std::wstring>& paths)
{
    return m_pMainWindow->OpenFiles(paths);
}

bool ICommand::ReloadTab( int tab, int encoding )
{
    return m_pMainWindow->ReloadTab(tab, encoding);
}

HRESULT ICommand::InvalidateUICommand( UI_INVALIDATIONS flags, const PROPERTYKEY *key )
{
    return g_pFramework->InvalidateUICommand(GetCmdId(), flags, key);
}

HRESULT ICommand::InvalidateUICommand( UINT32 cmdId, UI_INVALIDATIONS flags, const PROPERTYKEY *key )
{
    return g_pFramework->InvalidateUICommand(cmdId, flags, key);
}

HRESULT ICommand::SetUICommandProperty( UINT32 commandId, REFPROPERTYKEY key, PROPVARIANT value )
{
    return g_pFramework->SetUICommandProperty(commandId, key, value);
}

HRESULT ICommand::SetUICommandProperty( REFPROPERTYKEY key, PROPVARIANT value )
{
    return g_pFramework->SetUICommandProperty(GetCmdId(), key, value);
}

bool ICommand::SaveCurrentTab(bool bSaveAs /* = false */)
{
    return m_pMainWindow->SaveCurrentTab(bSaveAs);
}

bool ICommand::SaveDoc(DocID docID, bool bSaveAs /* = false */)
{
    return m_pMainWindow->SaveDoc(docID, bSaveAs);
}

bool ICommand::SaveDoc(DocID docID, const std::wstring& path)
{
    return m_pMainWindow->SaveDoc(docID, path);
}

int ICommand::GetDocumentCount() const
{
    return m_pMainWindow->m_DocManager.GetCount();
}

bool ICommand::HasActiveDocument() const
{
    auto id = m_pMainWindow->m_TabBar.GetCurrentTabId();
    return m_pMainWindow->m_DocManager.HasDocumentID(id);
}

const CDocument& ICommand::GetActiveDocument() const
{
    return m_pMainWindow->m_DocManager.GetDocumentFromID(m_pMainWindow->m_TabBar.GetCurrentTabId());
}

CDocument& ICommand::GetModActiveDocument()
{
    return m_pMainWindow->m_DocManager.GetModDocumentFromID(m_pMainWindow->m_TabBar.GetCurrentTabId());
}

bool ICommand::HasDocumentID(DocID id) const
{
    return m_pMainWindow->m_DocManager.HasDocumentID(id);
}

const CDocument& ICommand::GetDocumentFromID(DocID id) const
{
    return m_pMainWindow->m_DocManager.GetDocumentFromID(id);
}

CDocument& ICommand::GetModDocumentFromID(DocID id)
{
    return m_pMainWindow->m_DocManager.GetModDocumentFromID(id);
}

void ICommand::RestoreCurrentPos(const CPosData& pos)
{
    m_pMainWindow->m_editor.RestoreCurrentPos(pos);
}

void ICommand::SaveCurrentPos(CPosData& pos)
{
    return m_pMainWindow->m_editor.SaveCurrentPos(pos);
}

bool ICommand::UpdateFileTime(CDocument& doc, bool bIncludeReadonly)
{
    return m_pMainWindow->m_DocManager.UpdateFileTime(doc, bIncludeReadonly);
}

LRESULT ICommand::SendMessageToMainWnd( UINT msg, WPARAM wParam, LPARAM lParam )
{
    return ::SendMessage(*m_pMainWindow, msg, wParam, lParam);
}

void ICommand::UpdateStatusBar( bool bEverything )
{
    m_pMainWindow->UpdateStatusBar(bEverything);
}

void ICommand::SetupLexerForLang( const std::string& lang )
{
    return m_pMainWindow->m_editor.SetupLexerForLang(lang);
}

void ICommand::DocScrollClear( int type )
{
    m_pMainWindow->m_editor.DocScrollClear(type);
}

void ICommand::DocScrollAddLineColor( int type, size_t line, COLORREF clr )
{
    m_pMainWindow->m_editor.DocScrollAddLineColor(type, line, clr);
}

void ICommand::DocScrollRemoveLine( int type, size_t line )
{
    m_pMainWindow->m_editor.DocScrollRemoveLine(type, line);
}

void ICommand::UpdateLineNumberWidth()
{
    m_pMainWindow->m_editor.UpdateLineNumberWidth();
}

void ICommand::DocScrollUpdate()
{
    m_pMainWindow->m_editor.DocScrollUpdate();
}

void ICommand::GotoLine(long line)
{
    m_pMainWindow->m_editor.GotoLine(line);
}

void ICommand::Center(sptr_t startPos, sptr_t endPos )
{
    m_pMainWindow->m_editor.Center(startPos, endPos);
}

void ICommand::GotoBrace()
{
    m_pMainWindow->m_editor.GotoBrace();
}

DocID ICommand::GetDocIDFromTabIndex( int tab ) const
{
    return m_pMainWindow->m_TabBar.GetIDFromIndex(tab);
}

int ICommand::GetTabIndexFromDocID(DocID docID ) const
{
    return m_pMainWindow->m_TabBar.GetIndexFromID(docID);
}

DocID ICommand::GetDocIDFromPath( LPCTSTR path ) const
{
    return m_pMainWindow->m_DocManager.GetIdForPath(path);
}

void ICommand::SetInsertionIndex(int index)
{
    return m_pMainWindow->SetInsertionIndex(index);
}

std::string ICommand::GetLine(long line) const
{
    return m_pMainWindow->m_editor.GetLine(line);
}

std::string ICommand::GetTextRange(long startpos, long endpos) const
{
    return m_pMainWindow->m_editor.GetTextRange(startpos, endpos);
}

size_t ICommand::FindText(const std::string& tofind, long startpos, long endpos)
{
    return m_pMainWindow->m_editor.FindText(tofind, startpos, endpos);
}

std::string ICommand::GetSelectedText(bool useCurrentWordIfSelectionEmpty) const
{
    return m_pMainWindow->m_editor.GetSelectedText(useCurrentWordIfSelectionEmpty);
}

std::string ICommand::GetCurrentWord() const
{
    return m_pMainWindow->m_editor.GetCurrentWord();
}

std::string ICommand::GetCurrentLine() const
{
    return m_pMainWindow->m_editor.GetCurrentLine();
}

std::string ICommand::GetWordChars() const
{
    return m_pMainWindow->m_editor.GetWordChars();
}

void ICommand::OpenHDROP(HDROP hDrop)
{
    return m_pMainWindow->HandleDropFiles(hDrop);
}

void ICommand::ShowFileTree(bool bShow)
{
    return m_pMainWindow->ShowFileTree(bShow);
}

bool ICommand::IsFileTreeShown() const
{
    return m_pMainWindow->IsFileTreeShown();
}

std::wstring ICommand::GetFileTreePath() const
{
    return m_pMainWindow->GetFileTreePath();
}

void ICommand::FileTreeBlockRefresh(bool bBlock)
{
    return m_pMainWindow->FileTreeBlockRefresh(bBlock);
}

long ICommand::GetCurrentLineNumber() const
{
    return m_pMainWindow->m_editor.GetCurrentLineNumber();
}

void ICommand::BlockAllUIUpdates(bool block)
{
    m_pMainWindow->BlockAllUIUpdates(block);
}

void ICommand::ShowProgressCtrl(UINT delay)
{
    m_pMainWindow->ShowProgressCtrl(delay);
}

void ICommand::HideProgressCtrl()
{
    m_pMainWindow->HideProgressCtrl();
}

void ICommand::SetProgress(DWORD32 pos, DWORD32 end)
{
    m_pMainWindow->SetProgress(pos, end);
}
