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
#include "stdafx.h"
#include "ICommand.h"
#include "MainWindow.h"

extern IUIFramework *g_pFramework;

ICommand::ICommand(void * obj)
    : m_Obj(obj)
    , m_nextTimerID(1000)
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

void ICommand::OnTimer(UINT /*id*/)
{
}

void ICommand::TabActivateAt( int index )
{
    CMainWindow * pMainWnd = static_cast<CMainWindow*>(m_Obj);
    pMainWnd->m_TabBar.ActivateAt(index);
}

int ICommand::GetActiveTabIndex()
{
    CMainWindow * pMainWnd = static_cast<CMainWindow*>(m_Obj);
    return pMainWnd->m_TabBar.GetCurrentTabIndex();
}

int ICommand::GetCurrentTabId()
{
    CMainWindow * pMainWnd = static_cast<CMainWindow*>(m_Obj);
    return pMainWnd->m_TabBar.GetCurrentTabId();
}

std::wstring ICommand::GetCurrentTitle()
{
    CMainWindow * pMainWnd = static_cast<CMainWindow*>(m_Obj);
    wchar_t buf[100] = {0};
    pMainWnd->m_TabBar.GetCurrentTitle(buf, _countof(buf));
    return buf;
}

std::wstring ICommand::GetTitleForTabIndex(int index)
{
    CMainWindow * pMainWnd = static_cast<CMainWindow*>(m_Obj);
    wchar_t buf[100] = { 0 };
    pMainWnd->m_TabBar.GetTitle(index, buf, _countof(buf));
    return buf;
}

std::wstring ICommand::GetTitleForDocID(int id)
{
    return GetTitleForTabIndex(GetTabIndexFromDocID(id));
}

int ICommand::GetSrcTab()
{
    CMainWindow * pMainWnd = static_cast<CMainWindow*>(m_Obj);
    return pMainWnd->m_TabBar.GetSrcTab();
}

int ICommand::GetDstTab()
{
    CMainWindow * pMainWnd = static_cast<CMainWindow*>(m_Obj);
    return pMainWnd->m_TabBar.GetDstTab();
}

int ICommand::GetTabCount()
{
    CMainWindow * pMainWnd = static_cast<CMainWindow*>(m_Obj);
    return pMainWnd->m_TabBar.GetItemCount();
}

void ICommand::CloseTab( int index )
{
    CMainWindow * pMainWnd = static_cast<CMainWindow*>(m_Obj);
    pMainWnd->CloseTab(index);
}

sptr_t ICommand::ScintillaCall( unsigned int iMessage, uptr_t wParam /*= 0*/, sptr_t lParam /*= 0*/ )
{
    CMainWindow * pMainWnd = static_cast<CMainWindow*>(m_Obj);
    return pMainWnd->m_scintilla.Call(iMessage, wParam, lParam);
}

HWND ICommand::GetHwnd()
{
    CMainWindow * pMainWnd = static_cast<CMainWindow*>(m_Obj);
    return *pMainWnd;
}

bool ICommand::OpenFile(LPCWSTR file, bool bAddToMRU)
{
    CMainWindow * pMainWnd = static_cast<CMainWindow*>(m_Obj);
    return pMainWnd->OpenFile(file, bAddToMRU);
}

bool ICommand::ReloadTab( int tab, int encoding )
{
    CMainWindow * pMainWnd = static_cast<CMainWindow*>(m_Obj);
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
    CMainWindow * pMainWnd = static_cast<CMainWindow*>(m_Obj);
    return pMainWnd->SaveCurrentTab(bSaveAs);
}

int ICommand::GetDocumentCount()
{
    CMainWindow * pMainWnd = static_cast<CMainWindow*>(m_Obj);
    return pMainWnd->m_DocManager.GetCount();
}

bool ICommand::HasActiveDocument()
{
    CMainWindow * pMainWnd = static_cast<CMainWindow*>(m_Obj);
    int id = pMainWnd->m_TabBar.GetCurrentTabId();
    return pMainWnd->m_DocManager.HasDocumentID(id);
}

CDocument ICommand::GetActiveDocument()
{
    CMainWindow * pMainWnd = static_cast<CMainWindow*>(m_Obj);
    return pMainWnd->m_DocManager.GetDocumentFromID(pMainWnd->m_TabBar.GetCurrentTabId());
}

bool ICommand::HasDocumentID(int id)
{
    CMainWindow * pMainWnd = static_cast<CMainWindow*>(m_Obj);
    return pMainWnd->m_DocManager.HasDocumentID(id);
}

CDocument ICommand::GetDocumentFromID( int id )
{
    CMainWindow * pMainWnd = static_cast<CMainWindow*>(m_Obj);
    return pMainWnd->m_DocManager.GetDocumentFromID(id);
}

void ICommand::SetDocument( int index, CDocument doc )
{
    CMainWindow * pMainWnd = static_cast<CMainWindow*>(m_Obj);
    return pMainWnd->m_DocManager.SetDocument(index, doc);
}

LRESULT ICommand::SendMessageToMainWnd( UINT msg, WPARAM wParam, LPARAM lParam )
{
    CMainWindow * pMainWnd = static_cast<CMainWindow*>(m_Obj);
    return ::SendMessage(*pMainWnd, msg, wParam, lParam);
}

void ICommand::UpdateStatusBar( bool bEverything )
{
    CMainWindow * pMainWnd = static_cast<CMainWindow*>(m_Obj);
    pMainWnd->UpdateStatusBar(bEverything);
}

void ICommand::SetupLexerForLang( const std::wstring& lang )
{
    CMainWindow * pMainWnd = static_cast<CMainWindow*>(m_Obj);
    return pMainWnd->m_scintilla.SetupLexerForLang(lang);
}

void ICommand::DocScrollClear( int type )
{
    CMainWindow * pMainWnd = static_cast<CMainWindow*>(m_Obj);
    pMainWnd->m_scintilla.DocScrollClear(type);
}

void ICommand::DocScrollAddLineColor( int type, size_t line, COLORREF clr )
{
    CMainWindow * pMainWnd = static_cast<CMainWindow*>(m_Obj);
    pMainWnd->m_scintilla.DocScrollAddLineColor(type, line, clr);
}

void ICommand::DocScrollRemoveLine( int type, size_t line )
{
    CMainWindow * pMainWnd = static_cast<CMainWindow*>(m_Obj);
    pMainWnd->m_scintilla.DocScrollRemoveLine(type, line);
}

void ICommand::Center( long startPos, long endPos )
{
    CMainWindow * pMainWnd = static_cast<CMainWindow*>(m_Obj);
    pMainWnd->m_scintilla.Center(startPos, endPos);
}

void ICommand::GotoBrace()
{
    CMainWindow * pMainWnd = static_cast<CMainWindow*>(m_Obj);
    pMainWnd->m_scintilla.GotoBrace();
}

int ICommand::GetDocIDFromTabIndex( int tab )
{
    CMainWindow * pMainWnd = static_cast<CMainWindow*>(m_Obj);
    return pMainWnd->m_TabBar.GetIDFromIndex(tab);
}

int ICommand::GetTabIndexFromDocID( int docID )
{
    CMainWindow * pMainWnd = static_cast<CMainWindow*>(m_Obj);
    return pMainWnd->m_TabBar.GetIndexFromID(docID);
}

int ICommand::GetDocIDFromPath( LPCTSTR path )
{
    CMainWindow * pMainWnd = static_cast<CMainWindow*>(m_Obj);
    return pMainWnd->m_DocManager.GetIdForPath(path);
}


