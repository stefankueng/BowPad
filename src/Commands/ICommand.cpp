// This file is part of BowPad.
//
// Copyright (C) 2013-2018, 2020-2022, 2024 - Stefan Kueng
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
#include "LexStyles.h"
#include "CommandHandler.h"

extern IUIFramework* g_pFramework;

UINT ICommand::m_nextTimerID = COMMAND_TIMER_ID_START;

ICommand::ICommand(void* obj)
    : m_pMainWindow(static_cast<CMainWindow*>(obj))
{
}

HRESULT ICommand::IUICommandHandlerUpdateProperty(REFPROPERTYKEY /*key*/, const PROPVARIANT* /*pPropVarCurrentValue*/, PROPVARIANT* /*pPropVarNewValue*/)
{
    return E_NOTIMPL;
}

HRESULT ICommand::IUICommandHandlerExecute(UI_EXECUTIONVERB /*verb*/, const PROPERTYKEY* /*key*/, const PROPVARIANT* /*pPropVarValue*/, IUISimplePropertySet* /*pCommandExecutionProperties*/)
{
    return E_NOTIMPL;
}

void ICommand::ScintillaNotify(SCNotification* /*pScn*/)
{
}

void ICommand::TabNotify(TBHDR* /*ptbHdr*/)
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
{
}

void ICommand::OnBeforeDocumentSave(DocID /*id*/)
{
}

void ICommand::OnDocumentSave(DocID /*id*/, bool /*bSaveAs*/)
{
}

void ICommand::OnClipboardChanged()
{
}

void ICommand::OnTimer(UINT /*id*/)
{
}

void ICommand::OnPluginNotify(UINT /*cmdId*/, const std::wstring& /*pluginName*/, LPARAM /*data*/)
{
}

void ICommand::OnThemeChanged(bool /*bDark*/)
{
}

void ICommand::OnLangChanged()
{
}

void ICommand::OnStylesSet()
{
}

void ICommand::TabActivateAt(int index) const
{
    m_pMainWindow->m_tabBar.ActivateAt(index);
}

void ICommand::UpdateTab(int index) const
{
    m_pMainWindow->UpdateTab(GetDocIDFromTabIndex(index));
}

int ICommand::GetActiveTabIndex() const
{
    return m_pMainWindow->m_tabBar.GetCurrentTabIndex();
}

DocID ICommand::GetDocIdOfCurrentTab() const
{
    return m_pMainWindow->m_tabBar.GetCurrentTabId();
}

std::wstring ICommand::GetCurrentTitle() const
{
    return m_pMainWindow->m_tabBar.GetCurrentTitle();
}

std::wstring ICommand::GetTitleForTabIndex(int index) const
{
    return m_pMainWindow->m_tabBar.GetTitle(index);
}

std::wstring ICommand::GetTitleForDocID(DocID id) const
{
    return GetTitleForTabIndex(GetTabIndexFromDocID(id));
}

void ICommand::SetCurrentTitle(LPCWSTR title) const
{
    m_pMainWindow->m_tabBar.SetCurrentTitle(title);
    m_pMainWindow->UpdateCaptionBar();
}

void ICommand::SetTitleForDocID(DocID id, LPCWSTR title) const
{
    m_pMainWindow->m_tabBar.SetTitle(m_pMainWindow->m_tabBar.GetIndexFromID(id), title);
    m_pMainWindow->UpdateCaptionBar();
}

int ICommand::GetSrcTab() const
{
    return m_pMainWindow->m_tabBar.GetSrcTab();
}

int ICommand::GetDstTab() const
{
    return m_pMainWindow->m_tabBar.GetDstTab();
}

int ICommand::GetTabCount() const
{
    return m_pMainWindow->m_tabBar.GetItemCount();
}

bool ICommand::CloseTab(int index, bool bForce) const
{
    return m_pMainWindow->CloseTab(index, bForce);
}

Scintilla::ScintillaCall& ICommand::Scintilla() const
{
    return m_pMainWindow->m_editor.Scintilla();
}

HWND ICommand::GetHwnd() const
{
    return *m_pMainWindow;
}

HWND ICommand::GetScintillaWnd() const
{
    return m_pMainWindow->m_editor;
}

int ICommand::OpenFile(LPCWSTR file, unsigned int openFlags) const
{
    return m_pMainWindow->OpenFile(file, openFlags);
}

void ICommand::OpenFiles(const std::vector<std::wstring>& paths) const
{
    return m_pMainWindow->OpenFiles(paths);
}

bool ICommand::ReloadTab(int tab, int encoding) const
{
    return m_pMainWindow->ReloadTab(tab, encoding);
}

HRESULT ICommand::InvalidateUICommand(UI_INVALIDATIONS flags, const PROPERTYKEY* key)
{
    return g_pFramework->InvalidateUICommand(GetCmdId(), flags, key);
}

HRESULT ICommand::InvalidateUICommand(UINT32 cmdId, UI_INVALIDATIONS flags, const PROPERTYKEY* key)
{
    return g_pFramework->InvalidateUICommand(cmdId, flags, key);
}

HRESULT ICommand::SetUICommandProperty(UINT32 commandId, REFPROPERTYKEY key, PROPVARIANT value)
{
    return g_pFramework->SetUICommandProperty(commandId, key, value);
}

HRESULT ICommand::SetUICommandProperty(REFPROPERTYKEY key, PROPVARIANT value)
{
    return g_pFramework->SetUICommandProperty(GetCmdId(), key, value);
}

bool ICommand::SaveCurrentTab(bool bSaveAs /* = false */) const
{
    return m_pMainWindow->SaveCurrentTab(bSaveAs);
}

bool ICommand::SaveDoc(DocID docID, bool bSaveAs /* = false */) const
{
    return m_pMainWindow->SaveDoc(docID, bSaveAs);
}

bool ICommand::SaveDoc(DocID docID, const std::wstring& path) const
{
    return m_pMainWindow->SaveDoc(docID, path);
}

int ICommand::GetDocumentCount() const
{
    return m_pMainWindow->m_docManager.GetCount();
}

bool ICommand::HasActiveDocument() const
{
    auto id = m_pMainWindow->m_tabBar.GetCurrentTabId();
    return m_pMainWindow->m_docManager.HasDocumentID(id);
}

const CDocument& ICommand::GetActiveDocument() const
{
    return m_pMainWindow->m_docManager.GetDocumentFromID(m_pMainWindow->m_tabBar.GetCurrentTabId());
}

CDocument& ICommand::GetModActiveDocument() const
{
    return m_pMainWindow->m_docManager.GetModDocumentFromID(m_pMainWindow->m_tabBar.GetCurrentTabId());
}

bool ICommand::HasDocumentID(DocID id) const
{
    return m_pMainWindow->m_docManager.HasDocumentID(id);
}

const CDocument& ICommand::GetDocumentFromID(DocID id) const
{
    return m_pMainWindow->m_docManager.GetDocumentFromID(id);
}

CDocument& ICommand::GetModDocumentFromID(DocID id) const
{
    return m_pMainWindow->m_docManager.GetModDocumentFromID(id);
}

void ICommand::RestoreCurrentPos(const CPosData& pos) const
{
    m_pMainWindow->m_editor.RestoreCurrentPos(pos);
}

void ICommand::SaveCurrentPos(CPosData& pos) const
{
    return m_pMainWindow->m_editor.SaveCurrentPos(pos);
}

bool ICommand::UpdateFileTime(CDocument& doc, bool bIncludeReadonly) const
{
    return m_pMainWindow->m_docManager.UpdateFileTime(doc, bIncludeReadonly);
}

std::vector<char> ICommand::ReadNewData(CDocument& doc) const
{
    return m_pMainWindow->m_docManager.ReadNewData(doc);
}

LRESULT ICommand::SendMessageToMainWnd(UINT msg, WPARAM wParam, LPARAM lParam) const
{
    return ::SendMessage(*m_pMainWindow, msg, wParam, lParam);
}

void ICommand::UpdateStatusBar(bool bEverything) const
{
    m_pMainWindow->UpdateStatusBar(bEverything);
}

void ICommand::SetupLexerForLang(const std::string& lang) const
{
    return m_pMainWindow->m_editor.SetupLexerForLang(lang);
}

std::string ICommand::GetCurrentLanguage() const
{
    return GetActiveDocument().GetLanguage();
}

void ICommand::DocScrollClear(int type) const
{
    m_pMainWindow->m_editor.DocScrollClear(type);
}

void ICommand::DocScrollAddLineColor(int type, size_t line, COLORREF clr) const
{
    m_pMainWindow->m_editor.DocScrollAddLineColor(type, line, clr);
}

void ICommand::DocScrollRemoveLine(int type, size_t line) const
{
    m_pMainWindow->m_editor.DocScrollRemoveLine(type, line);
}

void ICommand::UpdateLineNumberWidth() const
{
    m_pMainWindow->m_editor.UpdateLineNumberWidth();
}

void ICommand::DocScrollUpdate() const
{
    m_pMainWindow->m_editor.DocScrollUpdate();
}

void ICommand::GotoLine(sptr_t line) const
{
    m_pMainWindow->m_editor.GotoLine(line);
}

void ICommand::Center(sptr_t startPos, sptr_t endPos) const
{
    m_pMainWindow->m_editor.Center(startPos, endPos);
}

void ICommand::GotoBrace() const
{
    m_pMainWindow->m_editor.GotoBrace();
}

DocID ICommand::GetDocIDFromTabIndex(int tab) const
{
    return m_pMainWindow->m_tabBar.GetIDFromIndex(tab);
}

int ICommand::GetTabIndexFromDocID(DocID docID) const
{
    return m_pMainWindow->m_tabBar.GetIndexFromID(docID);
}

void ICommand::OpenNewTab() const
{
    return m_pMainWindow->OpenNewTab();
}

DocID ICommand::GetDocIDFromPath(LPCTSTR path) const
{
    return m_pMainWindow->m_docManager.GetIdForPath(path);
}

void ICommand::SetInsertionIndex(int index) const
{
    return m_pMainWindow->SetInsertionIndex(index);
}

std::string ICommand::GetLine(sptr_t line) const
{
    return m_pMainWindow->m_editor.GetLine(line);
}

std::string ICommand::GetTextRange(sptr_t startPos, sptr_t endPos) const
{
    return m_pMainWindow->m_editor.GetTextRange(startPos, endPos);
}

size_t ICommand::FindText(const std::string& toFind, sptr_t startPos, sptr_t endPos) const
{
    return m_pMainWindow->m_editor.FindText(toFind, startPos, endPos);
}

std::string ICommand::GetSelectedText(SelectionHandling handling) const
{
    return m_pMainWindow->m_editor.GetSelectedText(handling);
}

std::string ICommand::GetCurrentWord(bool select) const
{
    return m_pMainWindow->m_editor.GetCurrentWord(select);
}

std::string ICommand::GetCurrentLine() const
{
    return m_pMainWindow->m_editor.GetCurrentLine();
}

std::string ICommand::GetWordChars() const
{
    return m_pMainWindow->m_editor.GetWordChars();
}

void ICommand::MarkSelectedWord(bool clear, bool edit) const
{
    return m_pMainWindow->m_editor.MarkSelectedWord(clear, edit);
}

void ICommand::OpenHDROP(HDROP hDrop) const
{
    return m_pMainWindow->HandleDropFiles(hDrop);
}

void ICommand::ShowFileTree(bool bShow) const
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

void ICommand::FileTreeBlockRefresh(bool bBlock) const
{
    return m_pMainWindow->FileTreeBlockRefresh(bBlock);
}

void ICommand::AddAutoCompleteWords(const std::string& lang, std::map<std::string, AutoCompleteType>&& words) const
{
    m_pMainWindow->AddAutoCompleteWords(lang, std::move(words));
}

void ICommand::AddAutoCompleteWords(const std::string& lang, const std::map<std::string, AutoCompleteType>& words) const
{
    m_pMainWindow->AddAutoCompleteWords(lang, words);
}

void ICommand::AddAutoCompleteWords(const DocID& docID, std::map<std::string, AutoCompleteType>&& words) const
{
    m_pMainWindow->AddAutoCompleteWords(docID, std::move(words));
}

void ICommand::AddAutoCompleteWords(const DocID& docID, const std::map<std::string, AutoCompleteType>& words) const
{
    m_pMainWindow->AddAutoCompleteWords(docID, words);
}

sptr_t ICommand::GetCurrentLineNumber() const
{
    return m_pMainWindow->m_editor.GetCurrentLineNumber();
}

void ICommand::BlockAllUIUpdates(bool block) const
{
    m_pMainWindow->BlockAllUIUpdates(block);
}

void ICommand::ShowProgressCtrl(UINT delay) const
{
    m_pMainWindow->ShowProgressCtrl(delay);
}

void ICommand::HideProgressCtrl() const
{
    m_pMainWindow->HideProgressCtrl();
}

void ICommand::SetProgress(DWORD32 pos, DWORD32 end) const
{
    m_pMainWindow->SetProgress(pos, end);
}

void ICommand::NotifyPlugins(const std::wstring& pluginName, LPARAM data)
{
    CCommandHandler::Instance().PluginNotify(GetCmdId(), pluginName, data);
}
