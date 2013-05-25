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
#include "BaseWindow.h"
#include "resource.h"
#include "StatusBar.h"
#include "TabBar.h"
#include "DocumentManager.h"
#include "ScintillaWnd.h"

#include <UIRibbon.h>
#include <UIRibbonPropertyHelpers.h>

class CMainWindow : public CWindow, public IUIApplication, public IUICommandHandler
{
    friend class ICommand;
public:
    CMainWindow(HINSTANCE hInst, const WNDCLASSEX* wcx = NULL);
    ~CMainWindow(void);

    /**
     * Registers the window class and creates the window.
     */
    bool                RegisterAndCreateWindow();

    bool                Initialize();
    bool                OpenFile(const std::wstring& file);
    bool                ReloadTab(int tab, int encoding);
    bool                SaveCurrentTab(bool bSaveAs = false);
    void                EnsureAtLeastOneTab();
    void                GoToLine( size_t line );
    bool                CloseTab(int tab);
    bool                CloseAllTabs();
    bool                HandleOutsideModifications(int index = -1);

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
    LRESULT CALLBACK    WinMsgHandler(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

    void AutoIndent( Scintilla::SCNotification * pScn );

    /// Handles all the WM_COMMAND window messages (e.g. menu commands)
    LRESULT             DoCommand(int id);

private:
    void                ResizeChildWindows();
    void                UpdateStatusBar(bool bEverything);
    void                InitCommands();
    void                AddHotSpots();

private:
    LONG                m_cRef;
    IUIRibbon *         m_pRibbon;
    UINT                m_RibbonHeight;

    CStatusBar          m_StatusBar;
    CTabBar             m_TabBar;
    CScintillaWnd       m_scintilla;
    CDocumentManager    m_DocManager;
};
