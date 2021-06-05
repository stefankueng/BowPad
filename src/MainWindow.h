// This file is part of BowPad.
//
// Copyright (C) 2013-2018, 2020-2021 - Stefan Kueng
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
#include "RichStatusBar.h"
#include "TabBar.h"
#include "DocumentManager.h"
#include "ScintillaWnd.h"
#include "FileTree.h"
#include "TabBtn.h"
#include "ProgressBar.h"
#include "CustomTooltip.h"
#include "CommandPaletteDlg.h"
#include "AutoComplete.h"

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
    void operator()(_IMAGELIST* hImageList) const
    {
        if (hImageList != nullptr)
        {
            BOOL deleted = ImageList_Destroy(hImageList);
            APPVERIFY(deleted != FALSE);
        }
    }
};

class CMainWindow : public CWindow
    , public IUIApplication
    , public IUICommandHandler
{
    friend class ICommand;
    friend class CAutoComplete;
    friend class CAutoCompleteConfigDlg;

public:
    CMainWindow(HINSTANCE hInst, const WNDCLASSEX* wcx = nullptr);
    virtual ~CMainWindow();

    bool RegisterAndCreateWindow();
    bool Initialize();
    bool CreateRibbon();

    // Load/Reload functions
    int  OpenFile(const std::wstring& file, unsigned int openFlags);
    bool OpenFileAs(const std::wstring& tempPath, const std::wstring& realpath, bool bModified);
    bool ReloadTab(int tab, int encoding, bool dueToOutsideChanges = false);

    bool         SaveCurrentTab(bool bSaveAs = false);
    bool         SaveDoc(DocID docID, bool bSaveAs = false);
    bool         SaveDoc(DocID docID, const std::wstring& path);
    void         EnsureAtLeastOneTab();
    void         GoToLine(size_t line);
    bool         CloseTab(int tab, bool force = false, bool quitting = false);
    bool         CloseAllTabs(bool quitting = false);
    void         SetFileToOpen(const std::wstring& path, size_t line = static_cast<size_t>(-1));
    void         SetFileOpenMRU(bool bUseMRU) { m_bPathsToOpenMRU = bUseMRU; }
    void         SetElevatedSave(const std::wstring& path, const std::wstring& savePath, long line);
    void         ElevatedSave(const std::wstring& path, const std::wstring& savePath, long line);
    void         TabMove(const std::wstring& path, const std::wstring& savePath, bool bMod, long line, const std::wstring& title);
    void         SetTabMove(const std::wstring& path, const std::wstring& savePath, bool bMod, long line, const std::wstring& title);
    void         SetInsertionIndex(int index) { m_insertionIndex = index; }
    std::wstring GetNewTabName();
    void         ShowFileTree(bool bShow);
    bool         IsFileTreeShown() const { return m_fileTreeVisible; }
    std::wstring GetFileTreePath() const { return m_fileTree.GetPath(); }
    void         FileTreeBlockRefresh(bool bBlock) { m_fileTree.BlockRefresh(bBlock); }
    void         SetFileTreeWidth(int width);
    void         IndentToLastLine() const;

    void AddAutoCompleteWords(const std::string& lang, std::map<std::string, AutoCompleteType>&& words);
    void AddAutoCompleteWords(const std::string& lang, const std::map<std::string, AutoCompleteType>& words);
    void AddAutoCompleteWords(const DocID& docID, std::map<std::string, AutoCompleteType>&& words);
    void AddAutoCompleteWords(const DocID& docID, const std::map<std::string, AutoCompleteType>& words);

    // clang-format off
    // IUnknown
    IFACEMETHODIMP QueryInterface(REFIID iid, void** ppv) override;
    IFACEMETHODIMP_(ULONG) AddRef() override;
    IFACEMETHODIMP_(ULONG) Release() override;
    // IUIApplication methods
    STDMETHOD(OnCreateUICommand)(UINT nCmdID, UI_COMMANDTYPE typeID, IUICommandHandler** ppCommandHandler) override;
    STDMETHOD(OnViewChanged)(UINT viewId, UI_VIEWTYPE typeId, IUnknown* pView, UI_VIEWVERB verb, INT uReasonCode)override;
    STDMETHOD(OnDestroyUICommand)(UINT32 commandId, UI_COMMANDTYPE typeID, IUICommandHandler* commandHandler)override;
    // IUICommandHandler methods
    STDMETHOD(UpdateProperty)(UINT nCmdID, REFPROPERTYKEY key, const PROPVARIANT* pPropVarCurrentValue, PROPVARIANT* pPropVarNewValue)override;
    STDMETHOD(Execute)(UINT nCmdID, UI_EXECUTIONVERB verb, const PROPERTYKEY* key, const PROPVARIANT* pPropVarValue, IUISimplePropertySet* pCommandExecutionProperties)override;
    // clang-format on
protected:
    /// the message handler for this window
    LRESULT CALLBACK WinMsgHandler(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) override;

    /// Handles all the WM_COMMAND window messages (e.g. menu commands)
    LRESULT DoCommand(WPARAM wParam, LPARAM lParam);

private:
    std::wstring        GetWindowClassName() const;
    static std::wstring GetAppName();
    HWND                FindAppMainWindow(HWND hStartWnd, bool* isThisInstance = nullptr) const;
    void                ResizeChildWindows();
    void                UpdateStatusBar(bool bEverything);
    void                AddHotSpots() const;

    bool                             AskToCreateNonExistingFile(const std::wstring& path) const;
    bool                             AskToReload(const CDocument& doc) const;
    ResponseToOutsideModifiedFile    AskToReloadOutsideModifiedFile(const CDocument& doc) const;
    bool                             AskAboutOutsideDeletedFile(const CDocument& doc) const;
    bool                             AskToRemoveReadOnlyAttribute() const;
    ResponseToCloseTab               AskToCloseTab() const;
    void                             UpdateTab(DocID docID);
    void                             CloseAllButCurrentTab();
    void                             EnsureNewLineAtEnd(const CDocument& doc) const;
    void                             OpenNewTab();
    void                             CopyCurDocPathToClipboard() const;
    void                             CopyCurDocNameToClipboard() const;
    void                             CopyCurDocDirToClipboard() const;
    void                             ShowCurDocInExplorer() const;
    void                             ShowCurDocExplorerProperties() const;
    void                             PasteHistory();
    void                             About() const;
    void                             ShowCommandPalette();
    bool                             HasOutsideChangesOccurred() const;
    void                             CheckForOutsideChanges();
    void                             UpdateCaptionBar();
    bool                             HandleOutsideDeletedFile(int docID);
    void                             HandleCreate(HWND hwnd);
    void                             HandleAfterInit();
    void                             HandleDropFiles(HDROP hDrop);
    void                             HandleTabDroppedOutside(int tab, POINT pt);
    void                             HandleTabChange(const NMHDR& tbHdr);
    void                             HandleTabChanging(const NMHDR& tbHdr);
    void                             HandleTabDelete(const TBHDR& tbHdr);
    void                             HandleClipboardUpdate();
    void                             HandleGetDispInfo(int tab, LPNMTTDISPINFO lpNmtdi);
    void                             HandleTreePath(const std::wstring& path, bool isDir, bool isDot);
    static std::vector<std::wstring> GetFileListFromGlobPath(const std::wstring& path);

    // Scintilla events.
    void        HandleDwellStart(const SCNotification& scn, bool start);
    LPARAM      HandleMouseMsg(const SCNotification& scn);
    bool        OpenUrlAtPos(Sci_Position pos);
    void        HandleCopyDataCommandLine(const COPYDATASTRUCT& cds);
    bool        HandleCopyDataMoveTab(const COPYDATASTRUCT& cds);
    void        HandleWriteProtectedEdit();
    void        HandleSavePoint(const SCNotification& scn);
    void        HandleUpdateUI(const SCNotification& scn);
    void        HandleAutoIndent(const SCNotification& scn) const;
    HRESULT     LoadRibbonSettings(IUnknown* pView);
    HRESULT     SaveRibbonSettings();
    HRESULT     ResizeToRibbon();
    bool        OnLButtonDown(UINT nFlags, POINT point);
    bool        OnMouseMove(UINT nFlags, POINT point);
    bool        OnLButtonUp(UINT nFlags, POINT point);
    void        HandleStatusBarEOLFormat();
    void        HandleStatusBarZoom();
    void        HandleStatusBar(WPARAM wParam, LPARAM lParam);
    LRESULT     HandleEditorEvents(const NMHDR& nmHdr, WPARAM wParam, LPARAM lParam);
    LRESULT     HandleFileTreeEvents(const NMHDR& nmHdr, WPARAM wParam, LPARAM lParam);
    LRESULT     HandleTabBarEvents(const NMHDR& nmHdr, WPARAM wParam, LPARAM lParam);
    void        ShowTablistDropdown(HWND hWnd, int offsetX, int offsetY);
    int         GetZoomPC() const;
    void        SetZoomPC(int zoomPC) const;
    COLORREF    GetColorForDocument(DocID id);
    void        OpenFiles(const std::vector<std::wstring>& paths);
    void        BlockAllUIUpdates(bool block);
    int         UnblockUI();
    void        ReBlockUI(int blockCount);
    void        ShowProgressCtrl(UINT delay);
    void        HideProgressCtrl();
    void        SetProgress(DWORD32 pos, DWORD32 end);
    static void SetRibbonColors(COLORREF text, COLORREF background, COLORREF highlight);
    static void SetRibbonColorsHSB(UI_HSBCOLOR text, UI_HSBCOLOR background, UI_HSBCOLOR highlight);
    static void GetRibbonColors(UI_HSBCOLOR& text, UI_HSBCOLOR& background, UI_HSBCOLOR& highlight);
    void        SetTheme(bool theme);

private:
    CRichStatusBar                                  m_statusBar;
    CTabBar                                         m_tabBar;
    CScintillaWnd                                   m_editor;
    CFileTree                                       m_fileTree;
    CTabBtn                                         m_tablistBtn;
    CTabBtn                                         m_newTabBtn;
    CTabBtn                                         m_closeTabBtn;
    CProgressBar                                    m_progressBar;
    CCustomToolTip                                  m_custToolTip;
    int                                             m_treeWidth;
    bool                                            m_bDragging;
    POINT                                           m_oldPt;
    bool                                            m_fileTreeVisible;
    CDocumentManager                                m_docManager;
    std::unique_ptr<wchar_t[]>                      m_tooltipBuffer;
    std::list<std::wstring>                         m_clipboardHistory;
    std::map<std::wstring, size_t>                  m_pathsToOpen;
    bool                                            m_bPathsToOpenMRU;
    std::wstring                                    m_elevatePath;
    std::wstring                                    m_elevateSavePath;
    std::wstring                                    m_tabMovePath;
    std::wstring                                    m_tabMoveSavePath;
    std::wstring                                    m_tabMoveTitle;
    bool                                            m_tabMoveMod;
    long                                            m_initLine;
    int                                             m_insertionIndex;
    bool                                            m_windowRestored;
    bool                                            m_inMenuLoop;
    std::unique_ptr<_IMAGELIST, HIMAGELIST_Deleter> m_tabBarImageList;
    CScintillaWnd                                   m_scratchEditor;
    std::map<std::wstring, int>                     m_folderColorIndexes;
    int                                             m_lastFolderColorIndex;
    int                                             m_blockCount;
    UI_HSBCOLOR                                     m_normalThemeText;
    UI_HSBCOLOR                                     m_normalThemeBack;
    UI_HSBCOLOR                                     m_normalThemeHigh;
    std::unique_ptr<CCommandPaletteDlg>             m_commandPaletteDlg;
    CAutoComplete                                   m_autoCompleter;
    Sci_Position                                    m_dwellStartPos;
    bool                                            m_bBlockAutoIndent;

    // status bar icons
    HICON m_hShieldIcon;
    HICON m_hCapsLockIcon;
    HICON m_hLexerIcon;
    HICON m_hZoomIcon;
    HICON m_hZoomDarkIcon;
    HICON m_hEmptyIcon;
    HICON hEditorconfigActive;
    HICON hEditorconfigInactive;

    std::map<int, IUIImagePtr> m_win7PNGWorkaroundData;

    LONG       m_cRef;
    int        m_newCount;
    IUIRibbon* m_pRibbon;
    UINT       m_ribbonHeight;
};
