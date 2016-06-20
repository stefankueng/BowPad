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
#include "ICommand.h"
#include "MainWindow.h"
#include "StringUtils.h"

extern IUIFramework *g_pFramework;

UINT ICommand::m_nextTimerID = COMMAND_TIMER_ID_START;


ICommand::ICommand(void * obj)
    : m_Obj(obj)
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

void ICommand::ScintillaNotify( Scintilla::SCNotification * /*pScn*/ )
{
}

void ICommand::TabNotify( TBHDR * /*ptbhdr*/ )
{
}

void ICommand::OnClose()
{
}

void ICommand::AfterInit()
{
}

void ICommand::OnDocumentClose( int /*index*/ )
{
}

void ICommand::OnDocumentOpen(int /*index*/)
{}

void ICommand::OnDocumentSave(int /*index*/, bool /*bSaveAs*/)
{}

void ICommand::OnTimer(UINT /*id*/)
{
}

void ICommand::OnThemeChanged(bool /*bDark*/)
{
}

void ICommand::OnLexerChanged(int /*lexer*/)
{
}

void ICommand::TabActivateAt( int index )
{
    CMainWindow* pMainWnd = GetMainWindow();
    pMainWnd->m_TabBar.ActivateAt(index);
}

void ICommand::UpdateTab(int index)
{
    CMainWindow* pMainWnd = GetMainWindow();
    pMainWnd->UpdateTab(GetDocIDFromTabIndex(index));
}

int ICommand::GetActiveTabIndex()
{
    CMainWindow* pMainWnd = GetMainWindow();
    return pMainWnd->m_TabBar.GetCurrentTabIndex();
}

int ICommand::GetDocIdOfCurrentTab()
{
    CMainWindow* pMainWnd = GetMainWindow();
    return pMainWnd->m_TabBar.GetCurrentTabId();
}

std::wstring ICommand::GetCurrentTitle()
{
    CMainWindow* pMainWnd = GetMainWindow();
    return pMainWnd->m_TabBar.GetCurrentTitle();
}

std::wstring ICommand::GetTitleForTabIndex(int index)
{
    CMainWindow* pMainWnd = GetMainWindow();
    return pMainWnd->m_TabBar.GetTitle(index);
}

std::wstring ICommand::GetTitleForDocID(int id)
{
    return GetTitleForTabIndex(GetTabIndexFromDocID(id));
}

void ICommand::SetCurrentTitle(LPCWSTR title)
{
    CMainWindow* pMainWnd = GetMainWindow();
    pMainWnd->m_TabBar.SetCurrentTitle(title);
    pMainWnd->UpdateCaptionBar();
}

int ICommand::GetSrcTab()
{
    CMainWindow* pMainWnd = GetMainWindow();
    return pMainWnd->m_TabBar.GetSrcTab();
}

int ICommand::GetDstTab()
{
    CMainWindow* pMainWnd = GetMainWindow();
    return pMainWnd->m_TabBar.GetDstTab();
}

int ICommand::GetTabCount()
{
    CMainWindow* pMainWnd = GetMainWindow();
    return pMainWnd->m_TabBar.GetItemCount();
}

bool ICommand::CloseTab( int index, bool bForce )
{
    CMainWindow* pMainWnd = GetMainWindow();
    return pMainWnd->CloseTab(index, bForce);
}

sptr_t ICommand::ScintillaCall( unsigned int iMessage, uptr_t wParam /*= 0*/, sptr_t lParam /*= 0*/ )
{
    CMainWindow* pMainWnd = GetMainWindow();
    return pMainWnd->m_editor.Call(iMessage, wParam, lParam);
}

sptr_t ICommand::ConstCall(unsigned int iMessage, uptr_t wParam /*= 0*/, sptr_t lParam /*= 0*/) const
{
    CMainWindow* pMainWnd = GetMainWindow();
    return pMainWnd->m_editor.Call(iMessage, wParam, lParam);
}


HWND ICommand::GetHwnd() const
{
    CMainWindow* pMainWnd = GetMainWindow();
    return *pMainWnd;
}

HWND ICommand::GetScintillaWnd() const
{
    CMainWindow* pMainWnd = GetMainWindow();
    return pMainWnd->m_editor;
}

int ICommand::OpenFile(LPCWSTR file, unsigned int openFlags)
{
    CMainWindow* pMainWnd = GetMainWindow();
    return pMainWnd->OpenFile(file, openFlags);
}

void ICommand::OpenFiles(const std::vector<std::wstring>& paths)
{
    CMainWindow* pMainWnd = GetMainWindow();
    return pMainWnd->OpenFiles(paths);
}

bool ICommand::ReloadTab( int tab, int encoding )
{
    CMainWindow* pMainWnd = GetMainWindow();
    return pMainWnd->ReloadTab(tab, encoding);
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
    CMainWindow* pMainWnd = GetMainWindow();
    return pMainWnd->SaveCurrentTab(bSaveAs);
}

int ICommand::GetDocumentCount() const
{
    CMainWindow* pMainWnd = GetMainWindow();
    return pMainWnd->m_DocManager.GetCount();
}

bool ICommand::HasActiveDocument() const
{
    CMainWindow* pMainWnd = GetMainWindow();
    int id = pMainWnd->m_TabBar.GetCurrentTabId();
    return pMainWnd->m_DocManager.HasDocumentID(id);
}

CDocument ICommand::GetActiveDocument() const
{
    CMainWindow* pMainWnd = GetMainWindow();
    return pMainWnd->m_DocManager.GetDocumentFromID(pMainWnd->m_TabBar.GetCurrentTabId());
}

bool ICommand::HasDocumentID(int id)
{
    CMainWindow* pMainWnd = GetMainWindow();
    return pMainWnd->m_DocManager.HasDocumentID(id);
}

CDocument ICommand::GetDocumentFromID( int id )
{
    CMainWindow* pMainWnd = GetMainWindow();
    return pMainWnd->m_DocManager.GetDocumentFromID(id);
}

void ICommand::SetDocument( int id, const CDocument& doc )
{
    CMainWindow* pMainWnd = GetMainWindow();
    return pMainWnd->m_DocManager.SetDocument(id, doc);
}

void ICommand::RestoreCurrentPos(const CPosData& pos)
{
    CMainWindow* pMainWnd = GetMainWindow();
    pMainWnd->m_editor.RestoreCurrentPos(pos);
}

void ICommand::SaveCurrentPos(CPosData& pos)
{
    CMainWindow* pMainWnd = GetMainWindow();
    return pMainWnd->m_editor.SaveCurrentPos(pos);
}

LRESULT ICommand::SendMessageToMainWnd( UINT msg, WPARAM wParam, LPARAM lParam )
{
    CMainWindow* pMainWnd = GetMainWindow();
    return ::SendMessage(*pMainWnd, msg, wParam, lParam);
}

void ICommand::UpdateStatusBar( bool bEverything )
{
    CMainWindow* pMainWnd = GetMainWindow();
    pMainWnd->UpdateStatusBar(bEverything);
}

void ICommand::SetupLexerForLang( const std::wstring& lang )
{
    CMainWindow* pMainWnd = GetMainWindow();
    return pMainWnd->m_editor.SetupLexerForLang(lang);
}

void ICommand::DocScrollClear( int type )
{
    CMainWindow* pMainWnd = GetMainWindow();
    pMainWnd->m_editor.DocScrollClear(type);
}

void ICommand::DocScrollAddLineColor( int type, size_t line, COLORREF clr )
{
    CMainWindow* pMainWnd = GetMainWindow();
    pMainWnd->m_editor.DocScrollAddLineColor(type, line, clr);
}

void ICommand::DocScrollRemoveLine( int type, size_t line )
{
    CMainWindow* pMainWnd = GetMainWindow();
    pMainWnd->m_editor.DocScrollRemoveLine(type, line);
}

void ICommand::DocScrollUpdate()
{
    CMainWindow* pMainWnd = GetMainWindow();
    pMainWnd->m_editor.DocScrollUpdate();
}

void ICommand::GotoLine(long line)
{
    CMainWindow* pMainWnd = GetMainWindow();
    pMainWnd->m_editor.GotoLine(line);
}

void ICommand::Center( long startPos, long endPos )
{
    CMainWindow* pMainWnd = GetMainWindow();
    pMainWnd->m_editor.Center(startPos, endPos);
}

void ICommand::GotoBrace()
{
    CMainWindow* pMainWnd = GetMainWindow();
    pMainWnd->m_editor.GotoBrace();
}

int ICommand::GetDocIDFromTabIndex( int tab )
{
    CMainWindow* pMainWnd = GetMainWindow();
    return pMainWnd->m_TabBar.GetIDFromIndex(tab);
}

int ICommand::GetTabIndexFromDocID( int docID )
{
    CMainWindow* pMainWnd = GetMainWindow();
    return pMainWnd->m_TabBar.GetIndexFromID(docID);
}

int ICommand::GetDocIDFromPath( LPCTSTR path )
{
    CMainWindow* pMainWnd = GetMainWindow();
    return pMainWnd->m_DocManager.GetIdForPath(path);
}

void ICommand::SetInsertionIndex(int index)
{
    CMainWindow* pMainWnd = GetMainWindow();
    return pMainWnd->SetInsertionIndex(index);
}

std::string ICommand::GetLine(long line) const
{
    CMainWindow* pMainWnd = GetMainWindow();
    return pMainWnd->m_editor.GetLine(line);
}

std::string ICommand::GetTextRange(long startpos, long endpos) const
{
    CMainWindow* pMainWnd = GetMainWindow();
    return pMainWnd->m_editor.GetTextRange(startpos, endpos);
}

size_t ICommand::FindText(const std::string& tofind, long startpos, long endpos)
{
    CMainWindow* pMainWnd = GetMainWindow();
    return pMainWnd->m_editor.FindText(tofind, startpos, endpos);
}

std::string ICommand::GetSelectedText(bool useCurrentWordIfSelectionEmpty) const
{
    CMainWindow* pMainWnd = GetMainWindow();
    return pMainWnd->m_editor.GetSelectedText(useCurrentWordIfSelectionEmpty);
}

std::string ICommand::GetCurrentLine() const
{
    CMainWindow* pMainWnd = GetMainWindow();
    return pMainWnd->m_editor.GetCurrentLine();
}

void ICommand::OpenHDROP(HDROP hDrop)
{
    CMainWindow* pMainWnd = GetMainWindow();
    return pMainWnd->HandleDropFiles(hDrop);
}

void ICommand::ShowFileTree(bool bShow)
{
    CMainWindow* pMainWnd = GetMainWindow();
    return pMainWnd->ShowFileTree(bShow);
}

bool ICommand::IsFileTreeShown()
{
    CMainWindow* pMainWnd = GetMainWindow();
    return pMainWnd->IsFileTreeShown();
}

std::wstring ICommand::GetFileTreePath()
{
    CMainWindow* pMainWnd = GetMainWindow();
    return pMainWnd->GetFileTreePath();
}

void ICommand::FileTreeBlockRefresh(bool bBlock)
{
    CMainWindow* pMainWnd = GetMainWindow();
    return pMainWnd->FileTreeBlockRefresh(bBlock);
}

long ICommand::GetCurrentLineNumber() const
{
    CMainWindow* pMainWnd = GetMainWindow();
    return pMainWnd->m_editor.GetCurrentLineNumber();
}

