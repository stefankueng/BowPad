// This file is part of BowPad.
//
// Copyright (C) 2013 - Stefan Kueng
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

void ICommand::TabActivateAt( int index )
{
    CMainWindow * pMainWnd = static_cast<CMainWindow*>(m_Obj);
    pMainWnd->m_TabBar.ActivateAt(index);
}

int ICommand::GetCurrentTabIndex()
{
    CMainWindow * pMainWnd = static_cast<CMainWindow*>(m_Obj);
    return pMainWnd->m_TabBar.GetCurrentTabIndex();
}

int ICommand::GetTabCount()
{
    CMainWindow * pMainWnd = static_cast<CMainWindow*>(m_Obj);
    return pMainWnd->m_TabBar.GetItemCount();
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

bool ICommand::OpenFile( LPCWSTR file )
{
    CMainWindow * pMainWnd = static_cast<CMainWindow*>(m_Obj);
    return pMainWnd->OpenFile(file);
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

CDocument ICommand::GetDocument( int index )
{
    CMainWindow * pMainWnd = static_cast<CMainWindow*>(m_Obj);
    return pMainWnd->m_DocManager.GetDocument(index);
}

void ICommand::SetDocument( int index, CDocument doc )
{
    CMainWindow * pMainWnd = static_cast<CMainWindow*>(m_Obj);
    return pMainWnd->m_DocManager.SetDocument(index, doc);
}

