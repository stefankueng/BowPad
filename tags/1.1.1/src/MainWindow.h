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
#include "BaseWindow.h"
#include "resource.h"
#include "StatusBar.h"
#include "TabBar.h"
#include "DocumentManager.h"
#include "ScintillaWnd.h"

#include <UIRibbon.h>
#include <UIRibbonPropertyHelpers.h>
#include <list>

class CMainWindow : public CWindow, public IUIApplication, public IUICommandHandler
{
    friend class ICommand;
public:
    CMainWindow(HINSTANCE hRes, const WNDCLASSEX* wcx = NULL);
    ~CMainWindow(void);

    /**
     * Registers the window class and creates the window.
     */
    bool                RegisterAndCreateWindow();

    bool                Initialize();
    bool                OpenFile(const std::wstring& file, bool bAddToMRU);
    bool                ReloadTab(int tab, int encoding);
    bool                SaveCurrentTab(bool bSaveAs = false);
    void                EnsureAtLeastOneTab();
    void                GoToLine(size_t line);
    bool                CloseTab(int tab, bool force = false);
    bool                CloseAllTabs();
    bool                HandleOutsideModifications(int id = -1);
    void                SetFileToOpen(const std::wstring& path, size_t line = (size_t)-1) { m_pathsToOpen[path] = line; }
    void                SetFileOpenMRU(bool bUseMRU) { m_bPathsToOpenMRU = bUseMRU; }
    void                SetElevatedSave(const std::wstring& path, const std::wstring& savepath, long line) { m_elevatepath = path; m_elevatesavepath = savepath; m_initLine = line; }
    void                ElevatedSave(const std::wstring& path, const std::wstring& savepath, long line);
    void                TabMove(const std::wstring& path, const std::wstring& savepath, bool bMod, long line);
    void                SetTabMove(const std::wstring& path, const std::wstring& savepath, bool bMod, long line) { m_tabmovepath = path; m_tabmovesavepath = savepath; m_tabmovemod = bMod; m_initLine = line; }

    // IUnknown
    IFACEMETHODIMP QueryInterface(REFIID iid, void** ppv);
    IFACEMETHODIMP_(ULONG) AddRef();
    IFACEMETHODIMP_(ULONG) Release();
    // IUIApplication methods
    STDMETHOD(OnCreateUICommand)(UINT nCmdID, UI_COMMANDTYPE typeID, IUICommandHandler** ppCommandHandler);
    STDMETHOD(OnViewChanged)(UINT viewId, UI_VIEWTYPE typeId, IUnknown* pView, UI_VIEWVERB verb, INT uReasonCode);
    STDMETHOD(OnDestroyUICommand)(UINT32 commandId, UI_COMMANDTYPE typeID, IUICommandHandler* commandHandler);
    // IUICommandHandler methods
    STDMETHOD(UpdateProperty)(UINT nCmdID, REFPROPERTYKEY key, const PROPVARIANT* ppropvarCurrentValue, PROPVARIANT* ppropvarNewValue);
    STDMETHOD(Execute)(UINT nCmdID, UI_EXECUTIONVERB verb, const PROPERTYKEY* key, const PROPVARIANT* ppropvarValue, IUISimplePropertySet* pCommandExecutionProperties);

protected:
    /// the message handler for this window
    LRESULT CALLBACK            WinMsgHandler(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

    void                        AutoIndent(Scintilla::SCNotification * pScn);

    /// Handles all the WM_COMMAND window messages (e.g. menu commands)
    LRESULT                     DoCommand(int id);

private:
    void                        ResizeChildWindows();
    void                        UpdateStatusBar(bool bEverything);
    void                        InitCommands();
    void                        AddHotSpots();
private:
    LONG                        m_cRef;
    IUIRibbon *                 m_pRibbon;
    UINT                        m_RibbonHeight;

    CStatusBar                  m_StatusBar;
    CTabBar                     m_TabBar;
    CScintillaWnd               m_scintilla;
    CDocumentManager            m_DocManager;
    std::unique_ptr<wchar_t[]>  m_tooltipbuffer;
    HICON                       m_hShieldIcon;
    std::list<std::wstring>     m_ClipboardHistory;
    std::map<std::wstring, size_t> m_pathsToOpen;
    bool                        m_bPathsToOpenMRU;
    std::wstring                m_elevatepath;
    std::wstring                m_elevatesavepath;
    std::wstring                m_tabmovepath;
    std::wstring                m_tabmovesavepath;
    bool                        m_tabmovemod;
    long                        m_initLine;
};