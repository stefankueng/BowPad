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
#include "BaseWindow.h"
#include "resource.h"
#include "RichStatusBar.h"
#include "TabBar.h"
#include "DocumentManager.h"
#include "ScintillaWnd.h"
#include "FileTree.h"
#include "TabBtn.h"
#include "ProgressBar.h"
#include "CustomTooltip.h"

#include <UIRibbon.h>
#include <UIRibbonPropertyHelpers.h>
#include <list>

const int COMMAND_TIMER_ID_START = 1000;

enum class ResponseToOutsideModifiedFile
{
    Cancel,
    Reload,
    KeepOurChanges
};

enum class ResponseToCloseTab
{
    StayOpen,
    SaveAndClose,
    CloseWithoutSaving
};


struct HIMAGELIST_Deleter
{
    void operator()(_IMAGELIST* hImageList)
    {
        if (hImageList != nullptr)
        {
            BOOL deleted = ImageList_Destroy(hImageList);
            APPVERIFY(deleted != FALSE);
        }
    }
};

class CMainWindow : public CWindow, public IUIApplication, public IUICommandHandler
{
    friend class ICommand;
public:
    CMainWindow(HINSTANCE hInst, const WNDCLASSEX* wcx = nullptr);
    ~CMainWindow();

    bool                RegisterAndCreateWindow();
    bool                Initialize();
    bool                CreateRibbon();

    // Load/Reload functions
    int                 OpenFile(const std::wstring& file, unsigned int openFlags);
    bool                OpenFileAs( const std::wstring& temppath, const std::wstring& realpath, bool bModified );
    bool                ReloadTab(int tab, int encoding, bool dueToOutsideChanges = false);

    bool                SaveCurrentTab(bool bSaveAs = false);
    bool                SaveDoc(DocID docID, bool bSaveAs = false);
    void                EnsureAtLeastOneTab();
    void                GoToLine(size_t line);
    bool                CloseTab(int tab, bool force = false, bool quitting = false);
    bool                CloseAllTabs(bool quitting = false);
    void                SetFileToOpen(const std::wstring& path, size_t line = (size_t)-1) { m_pathsToOpen[path] = line; }
    void                SetFileOpenMRU(bool bUseMRU) { m_bPathsToOpenMRU = bUseMRU; }
    void                SetElevatedSave(const std::wstring& path, const std::wstring& savepath, long line) { m_elevatepath = path; m_elevatesavepath = savepath; m_initLine = line; }
    void                ElevatedSave(const std::wstring& path, const std::wstring& savepath, long line);
    void                TabMove(const std::wstring& path, const std::wstring& savepath, bool bMod, long line, const std::wstring& title);
    void                SetTabMove(const std::wstring& path, const std::wstring& savepath, bool bMod, long line, const std::wstring& title) { m_tabmovepath = path; m_tabmovesavepath = savepath; m_tabmovemod = bMod; m_initLine = line; m_tabmovetitle = title; }
    void                SetInsertionIndex(int index) { m_insertionIndex = index; }
    std::wstring        GetNewTabName();
    void                ShowFileTree(bool bShow);
    bool                IsFileTreeShown() const { return m_fileTreeVisible; }
    std::wstring        GetFileTreePath() const { return m_fileTree.GetPath(); }
    void                FileTreeBlockRefresh(bool bBlock) { m_fileTree.BlockRefresh(bBlock); }
    void                SetFileTreeWidth(int width);

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

    /// Handles all the WM_COMMAND window messages (e.g. menu commands)
    LRESULT                     DoCommand(int id);

private:
    std::wstring                GetWindowClassName() const;
    std::wstring                GetAppName() const;
    HWND                        FindAppMainWindow(HWND hStartWnd, bool* isThisInstance = nullptr) const;
    void                        ResizeChildWindows();
    void                        UpdateStatusBar(bool bEverything);
    void                        AddHotSpots();

    bool                        AskToCreateNonExistingFile(const std::wstring& path) const;
    bool                        AskToReload(const CDocument& doc) const;
    ResponseToOutsideModifiedFile
                                AskToReloadOutsideModifiedFile(const CDocument& doc) const;
    bool                        AskAboutOutsideDeletedFile(const CDocument& doc) const;
    bool                        AskToRemoveReadOnlyAttribute() const;
    ResponseToCloseTab          AskToCloseTab() const;
    void                        UpdateTab(DocID docID);
    void                        CloseAllButCurrentTab();
    void                        EnsureNewLineAtEnd(const CDocument& doc);
    void                        OpenNewTab();
    void                        CopyCurDocPathToClipboard() const;
    void                        CopyCurDocNameToClipboard() const;
    void                        CopyCurDocDirToClipboard() const;
    void                        ShowCurDocInExplorer() const;
    void                        ShowCurDocExplorerProperties() const;
    void                        PasteHistory();
    void                        About() const;
    bool                        HasOutsideChangesOccurred() const;
    void                        CheckForOutsideChanges();
    void                        UpdateCaptionBar();
    bool                        HandleOutsideDeletedFile(int docID);
    void                        HandleCreate(HWND hwnd);
    void                        HandleAfterInit();
    void                        HandleDropFiles(HDROP hDrop);
    void                        HandleTabDroppedOutside(int tab, POINT pt);
    void                        HandleTabChange(const NMHDR& tbhdr);
    void                        HandleTabChanging(const NMHDR& tbhdr);
    void                        HandleTabDelete(const TBHDR& tbhdr);
    void                        HandleClipboardUpdate();
    void                        HandleGetDispInfo(int tab, LPNMTTDISPINFO lpnmtdi);
    // Scintilla events.
    void                        HandleDwellStart(const SCNotification& scn);
    bool                        HandleDoubleClick(const SCNotification& scn);
    void                        HandleCopyDataCommandLine(const COPYDATASTRUCT& cds);
    bool                        HandleCopyDataMoveTab(const COPYDATASTRUCT& cds);
    void                        HandleWriteProtectedEdit();
    void                        HandleSavePoint(const SCNotification& scn);
    void                        HandleUpdateUI(const SCNotification& scn);
    void                        HandleAutoIndent(const SCNotification &scn);
    HRESULT                     LoadRibbonSettings(IUnknown* pView);
    HRESULT                     SaveRibbonSettings();
    HRESULT                     ResizeToRibbon();
    bool                        OnLButtonDown(UINT nFlags, POINT point);
    bool                        OnMouseMove(UINT nFlags, POINT point);
    bool                        OnLButtonUp(UINT nFlags, POINT point);
    void                        HandleStatusBarEOLFormat();
    void                        HandleStatusBarZoom();
    void                        HandleStatusBar(WPARAM wParam, LPARAM lParam);
    LRESULT                     HandleEditorEvents(const NMHDR& nmhdr, WPARAM wParam, LPARAM lParam);
    LRESULT                     HandleFileTreeEvents(const NMHDR& nmhdr, WPARAM wParam, LPARAM lParam);
    LRESULT                     HandleTabBarEvents(const NMHDR& nmhdr, WPARAM wParam, LPARAM lParam);
    int                         GetZoomPC() const;
    void                        SetZoomPC(int zoomPC);
    COLORREF                    GetColorForDocument(DocID id);
    void                        OpenFiles(const std::vector<std::wstring>& paths);
    void                        BlockAllUIUpdates(bool block);
    int                         UnblockUI();
    void                        ReBlockUI(int blockCount);
    void                        ShowProgressCtrl(UINT delay);
    void                        HideProgressCtrl();
    void                        SetProgress(DWORD32 pos, DWORD32 end);
    void                        SetRibbonColors(COLORREF text, COLORREF background, COLORREF highlight);
    void                        SetRibbonColorsHSB(UI_HSBCOLOR text, UI_HSBCOLOR background, UI_HSBCOLOR highlight);
    void                        GetRibbonColors(UI_HSBCOLOR& text, UI_HSBCOLOR& background, UI_HSBCOLOR& highlight) const;
    void                        SetTheme(bool theme);

private:
    LONG                        m_cRef;
    int                         m_newCount;
    IUIRibbon*                  m_pRibbon;
    UINT                        m_RibbonHeight;

    CRichStatusBar              m_StatusBar;
    CTabBar                     m_TabBar;
    CScintillaWnd               m_editor;
    CFileTree                   m_fileTree;
    CTabBtn                     m_newTabBtn;
    CTabBtn                     m_closeTabBtn;
    CProgressBar                m_progressBar;
    CCustomToolTip              m_custToolTip;
    int                         m_treeWidth;
    bool                        m_bDragging;
    POINT                       m_oldPt;
    bool                        m_fileTreeVisible;
    CDocumentManager            m_DocManager;
    std::unique_ptr<wchar_t[]>  m_tooltipbuffer;
    std::list<std::wstring>     m_ClipboardHistory;
    std::map<std::wstring, size_t> m_pathsToOpen;
    bool                        m_bPathsToOpenMRU;
    std::wstring                m_elevatepath;
    std::wstring                m_elevatesavepath;
    std::wstring                m_tabmovepath;
    std::wstring                m_tabmovesavepath;
    std::wstring                m_tabmovetitle;
    bool                        m_tabmovemod;
    long                        m_initLine;
    int                         m_insertionIndex;
    bool                        m_windowRestored;
    bool                        m_inMenuLoop;
    std::unique_ptr<_IMAGELIST, HIMAGELIST_Deleter> m_TabBarImageList;
    CScintillaWnd               m_scratchEditor;
    std::map<std::wstring, int> m_foldercolorindexes;
    int                         m_lastfoldercolorindex;
    int                         m_blockCount;
    UI_HSBCOLOR                 m_normalThemeText;
    UI_HSBCOLOR                 m_normalThemeBack;
    UI_HSBCOLOR                 m_normalThemeHigh;
    // status bar icons
    HICON                       m_hShieldIcon;
    HICON                       m_hCapslockIcon;
    HICON                       m_hLexerIcon;
    HICON                       m_hZoomIcon;
    HICON                       m_hZoomDarkIcon;
    HICON                       m_hEmptyIcon;
    HICON                       hEditorconfigActive;
    HICON                       hEditorconfigInactive;
};
