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
#include "MainWindow.h"
#include "BowPad.h"
#include "BowPadUI.h"
#include "AboutDlg.h"
#include "StringUtils.h"
#include "UnicodeUtils.h"
#include "TempFile.h"
#include "CommandHandler.h"
#include "MRU.h"
#include "KeyboardShortcutHandler.h"
#include "AppUtils.h"
#include "PathUtils.h"
#include "Theme.h"
#include "PreserveChdir.h"
#include "CmdLineParser.h"
#include "SysInfo.h"
#include "ClipboardHelper.h"
#include "EditorConfigHandler.h"
#include "DirFileEnum.h"
#include "LexStyles.h"
#include "OnOutOfScope.h"

#include <memory>
#include <cassert>
#include <type_traits>
#include <future>
#include <Shobjidl.h>
#include <initializer_list>

IUIFramework *g_pFramework = nullptr;  // Reference to the Ribbon framework.

namespace
{
    const int STATUSBAR_DOC_TYPE        = 0;
    const int STATUSBAR_CUR_POS         = 1;
    const int STATUSBAR_SEL             = 2;
    const int STATUSBAR_EOL_FORMAT      = 3;
    const int STATUSBAR_TABSPACE        = 4;
    const int STATUSBAR_UNICODE_TYPE    = 5;
    const int STATUSBAR_TYPING_MODE     = 6;
    const int STATUSBAR_CAPS            = 7;
    const int STATUSBAR_TABS            = 8;
    const int STATUSBAR_ZOOM            = 9;

    static constexpr char URL_REG_EXPR[] = { "\\b[A-Za-z+]{3,9}://[A-Za-z0-9_\\-+~.:?&@=/%#,;{}()[\\]|*!\\\\]+\\b" };
    static constexpr size_t URL_REG_EXPR_LENGTH = _countof(URL_REG_EXPR) - 1;

    const int TIMER_UPDATECHECK = 101;
    const int TIMER_ZOOM = 102;

    static ResponseToOutsideModifiedFile responsetooutsidemodifiedfile = ResponseToOutsideModifiedFile::Reload;
    static BOOL                          responsetooutsidemodifiedfiledoall = FALSE;

    static bool docloseall = false;
    static BOOL closealldoall = FALSE;
    static ResponseToCloseTab responsetoclosetab = ResponseToCloseTab::CloseWithoutSaving;
}

CMainWindow::CMainWindow(HINSTANCE hInst, const WNDCLASSEX* wcx /* = NULL*/)
    : CWindow(hInst, wcx)
    , m_StatusBar(hInst)
    , m_fileTree(hInst, this)
    , m_treeWidth(0)
    , m_bDragging(false)
    , m_fileTreeVisible(true)
    , m_TabBar(hInst)
    , m_editor(hInst)
    , m_newCount(0)
    , m_cRef(1)
    , m_hShieldIcon(nullptr)
    , m_tabmovemod(false)
    , m_initLine(0)
    , m_bPathsToOpenMRU(true)
    , m_insertionIndex(-1)
    , m_windowRestored(false)
    , m_pRibbon(nullptr)
    , m_RibbonHeight(0)
    , m_scratchEditor(hResource)
{
    m_hShieldIcon = (HICON)::LoadImage(hResource, MAKEINTRESOURCE(IDI_ELEVATED), IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR);
    m_fileTreeVisible = CIniSettings::Instance().GetInt64(L"View", L"FileTree", 1) != 0;
    m_oldPt.x = m_oldPt.y = 0;
    m_scratchEditor.InitScratch(hRes);
}

extern void FindReplace_Finish();

CMainWindow::~CMainWindow()
{
    FindReplace_Finish();
    DestroyIcon(m_hShieldIcon);
}

// IUnknown method implementations.
STDMETHODIMP_(ULONG) CMainWindow::AddRef()
{
    return InterlockedIncrement(&m_cRef);
}

STDMETHODIMP_(ULONG) CMainWindow::Release()
{
    LONG cRef = InterlockedDecrement(&m_cRef);
    if (cRef == 0)
    {
        DebugBreak();
    }

    return cRef;
}

STDMETHODIMP CMainWindow::QueryInterface(REFIID iid, void** ppv)
{
    if (iid == __uuidof(IUnknown))
    {
        *ppv = static_cast<IUnknown*>(static_cast<IUIApplication*>(this));
    }
    else if (iid == __uuidof(IUIApplication))
    {
        *ppv = static_cast<IUIApplication*>(this);
    }
    else if (iid == __uuidof(IUICommandHandler))
    {
        *ppv = static_cast<IUICommandHandler*>(this);
    }
    else
    {
        *ppv = nullptr;
        return E_NOINTERFACE;
    }

    AddRef();
    return S_OK;
}

HRESULT CMainWindow::LoadRibbonSettings(IUnknown* pView)
{
    HRESULT hr = pView->QueryInterface(IID_PPV_ARGS(&m_pRibbon));
    if (!CAppUtils::FailedShowMessage(hr))
    {
        IStreamPtr pStrm;
        std::wstring ribbonsettingspath = CAppUtils::GetDataPath() + L"\\ribbonsettings";
        hr = SHCreateStreamOnFileEx(ribbonsettingspath.c_str(), STGM_READ, 0, FALSE, nullptr, &pStrm);
        if (!CAppUtils::FailedShowMessage(hr))
        {
            hr = m_pRibbon->LoadSettingsFromStream(pStrm);
            CAppUtils::FailedShowMessage(hr);
        }
    }
    return hr;
}

HRESULT CMainWindow::SaveRibbonSettings()
{
    IStreamPtr pStrm;
    std::wstring ribbonsettingspath = CAppUtils::GetDataPath() + L"\\ribbonsettings";
    HRESULT hr = SHCreateStreamOnFileEx(ribbonsettingspath.c_str(),
        STGM_WRITE|STGM_CREATE, FILE_ATTRIBUTE_NORMAL, TRUE, nullptr, &pStrm);
    if (!CAppUtils::FailedShowMessage(hr))
    {
        LARGE_INTEGER liPos;
        ULARGE_INTEGER uliSize;

        liPos.QuadPart = 0;
        uliSize.QuadPart = 0;

        pStrm->Seek(liPos, STREAM_SEEK_SET, nullptr);
        pStrm->SetSize(uliSize);

        hr = m_pRibbon->SaveSettingsToStream(pStrm);
        CAppUtils::FailedShowMessage(hr);
    }
    m_pRibbon->Release();
    m_pRibbon = nullptr;
    return hr;
}

HRESULT CMainWindow::ResizeToRibbon()
{
    HRESULT hr = E_FAIL;
    if (m_pRibbon)
    {
        // Call to the framework to determine the desired height of the Ribbon.
        hr = m_pRibbon->GetHeight(&m_RibbonHeight);
        if (!CAppUtils::FailedShowMessage((hr)))
            ResizeChildWindows();
    }
    return hr;
}

//  FUNCTION: OnCreateUICommand(UINT, UI_COMMANDTYPE, IUICommandHandler)
//
//  PURPOSE: Called by the Ribbon framework for each command specified in markup, to allow
//           the host application to bind a command handler to that command.
STDMETHODIMP CMainWindow::OnCreateUICommand(
    UINT nCmdID,
    UI_COMMANDTYPE typeID,
    IUICommandHandler** ppCommandHandler)
{
    UNREFERENCED_PARAMETER(typeID);
    UNREFERENCED_PARAMETER(nCmdID);

    return QueryInterface(IID_PPV_ARGS(ppCommandHandler));
}

//  FUNCTION: OnViewChanged(UINT, UI_VIEWTYPE, IUnknown*, UI_VIEWVERB, INT)
//
//  PURPOSE: Called when the state of a View (Ribbon is a view) changes, for example, created, destroyed, or resized.
STDMETHODIMP CMainWindow::OnViewChanged(
    UINT viewId,
    UI_VIEWTYPE typeId,
    IUnknown* pView,
    UI_VIEWVERB verb,
    INT uReasonCode)
{
    UNREFERENCED_PARAMETER(uReasonCode);
    UNREFERENCED_PARAMETER(viewId);

    HRESULT hr = E_NOTIMPL;

    // Checks to see if the view that was changed was a Ribbon view.
    if (UI_VIEWTYPE_RIBBON == typeId)
    {
        switch (verb)
        {
            // The view was newly created.
        case UI_VIEWVERB_CREATE:
            hr = LoadRibbonSettings(pView);
            break;
            // The view has been resized.  For the Ribbon view, the application should
            // call GetHeight to determine the height of the ribbon.
        case UI_VIEWVERB_SIZE:
            hr = ResizeToRibbon();
            break;
            // The view was destroyed.
        case UI_VIEWVERB_DESTROY:
            hr = SaveRibbonSettings();
            break;
        }
    }

    return hr;
}

STDMETHODIMP CMainWindow::OnDestroyUICommand(
    UINT32 nCmdID,
    UI_COMMANDTYPE typeID,
    IUICommandHandler* commandHandler)
{
    UNREFERENCED_PARAMETER(commandHandler);
    UNREFERENCED_PARAMETER(typeID);
    UNREFERENCED_PARAMETER(nCmdID);

    return E_NOTIMPL;
}

STDMETHODIMP CMainWindow::UpdateProperty(
    UINT nCmdID,
    REFPROPERTYKEY key,
    const PROPVARIANT* ppropvarCurrentValue,
    PROPVARIANT* ppropvarNewValue)
{
    UNREFERENCED_PARAMETER(ppropvarCurrentValue);

    HRESULT hr = E_NOTIMPL;
    ICommand * pCmd = CCommandHandler::Instance().GetCommand(nCmdID);
    if (pCmd)
        hr = pCmd->IUICommandHandlerUpdateProperty(key, ppropvarCurrentValue, ppropvarNewValue);

    if (key == UI_PKEY_TooltipTitle)
    {
        std::wstring shortkey = CKeyboardShortcutHandler::Instance().GetTooltipTitleForCommand((WORD)nCmdID);
        if (!shortkey.empty())
        {
            hr = UIInitPropertyFromString(UI_PKEY_TooltipTitle, shortkey.c_str(), ppropvarNewValue);
            CKeyboardShortcutHandler::Instance().ToolTipUpdated((WORD)nCmdID);
        }
    }
    // the ribbon UI is really buggy: Invalidating a lot of properties at once
    // won't update all the properties but only the first few.
    // Since there's no way to invalidate everything that's necessary at once,
    // we do invalidate all the remaining tooltip properties here until all
    // of them have been updated (see the call to TooltipUpdated() above).
    CKeyboardShortcutHandler::Instance().UpdateTooltips(false);
    return hr;
}

STDMETHODIMP CMainWindow::Execute(
    UINT nCmdID,
    UI_EXECUTIONVERB verb,
    const PROPERTYKEY* key,
    const PROPVARIANT* ppropvarValue,
    IUISimplePropertySet* pCommandExecutionProperties)
{
    HRESULT hr = S_OK;

    ICommand * pCmd = CCommandHandler::Instance().GetCommand(nCmdID);
    if (pCmd)
    {
        hr = pCmd->IUICommandHandlerExecute(verb, key, ppropvarValue, pCommandExecutionProperties);
        if (hr == E_NOTIMPL)
        {
            hr = S_OK;
            if (verb == UI_EXECUTIONVERB_EXECUTE)
                DoCommand(nCmdID);
        }
    }
    else
        DoCommand(nCmdID);
    return hr;
}

void CMainWindow::About() const
{
    CAboutDlg dlg(*this);
    dlg.DoModal(hRes, IDD_ABOUTBOX, *this);
}

std::wstring CMainWindow::GetAppName() const
{
   auto title = LoadResourceWString(hRes, IDS_APP_TITLE);
   return title;
}

std::wstring CMainWindow::GetWindowClassName() const
{
    auto className = LoadResourceWString(hResource, IDC_BOWPAD);
    className += CAppUtils::GetSessionID();
    return className;
}

std::wstring CMainWindow::GetNewTabName()
{
    // Tab's start at 1, m_mewCount left at value of last ticket used.
    int newCount = ++m_newCount;
    ResString newRes(hRes, IDS_NEW_TABTITLE);
    std::wstring tabName = CStringUtils::Format(newRes, newCount);
    return tabName;
}

HWND CMainWindow::FindAppMainWindow(HWND hStartWnd, bool* isThisInstance) const
{
    std::wstring myClassName = GetWindowClassName();
    while (hStartWnd)
    {
        wchar_t classname[257]; // docs for WNDCLASS state that a class name is max 256 chars.
        GetClassName(hStartWnd, classname, _countof(classname));
        if (myClassName.compare(classname) == 0)
            break;
        hStartWnd = GetParent(hStartWnd);
    }
    if (isThisInstance)
        *isThisInstance = hStartWnd != nullptr && hStartWnd == *this;
    return hStartWnd;
}

bool CMainWindow::RegisterAndCreateWindow()
{
    WNDCLASSEX wcx = { sizeof(WNDCLASSEX) }; // Set size and zero out rest.

    // Fill in the window class structure with default parameters
    //wcx.style = 0; // Don't use CS_HREDRAW or CS_VREDRAW with a Ribbon
    wcx.lpfnWndProc = CWindow::stWinMsgHandler;
    wcx.hInstance = hResource;
    std::wstring clsName = GetWindowClassName();
    wcx.lpszClassName = clsName.c_str();
    wcx.hIcon = LoadIcon(hResource, MAKEINTRESOURCE(IDI_BOWPAD));
    wcx.hbrBackground = (HBRUSH)(COLOR_3DFACE+1);
    wcx.hIconSm = LoadIcon(wcx.hInstance, MAKEINTRESOURCE(IDI_BOWPAD));
    wcx.hCursor = LoadCursor(NULL, (LPTSTR)IDC_SIZEWE);
    if (RegisterWindow(&wcx))
    {
        if (CreateEx(WS_EX_ACCEPTFILES, WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN | WS_VISIBLE, nullptr))
            return true;
    }
    return false;
}

LRESULT CALLBACK CMainWindow::WinMsgHandler(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_CREATE:
        HandleCreate(hwnd);
        break;
    case WM_COMMAND:
        return DoCommand(LOWORD(wParam));
        break;
    case WM_SIZE:
        ResizeChildWindows();
        break;
    case WM_GETMINMAXINFO:
        {
            MINMAXINFO& mmi = *(MINMAXINFO*)lParam;
            mmi.ptMinTrackSize.x = 400;
            mmi.ptMinTrackSize.y = 100;
            return 0;
        }
        break;
    case WM_DRAWITEM:
        {
            const DRAWITEMSTRUCT& dis = *(DRAWITEMSTRUCT *)lParam;
            if (dis.CtlType == ODT_TAB)
            {
                return ::SendMessage(dis.hwndItem, WM_DRAWITEM, wParam, lParam);
            }
        }
        break;
    case WM_MOUSEMOVE:
        return OnMouseMove((UINT)wParam, { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) });

    case WM_LBUTTONDOWN:
        return OnLButtonDown((UINT)wParam, { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) });

    case WM_LBUTTONUP:
        return OnLButtonUp((UINT)wParam, { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) });

    case WM_DROPFILES:
        HandleDropFiles(reinterpret_cast<HDROP>(wParam));
        break;
    case WM_COPYDATA:
        {
            if (lParam == 0)
                return 0;
            const COPYDATASTRUCT& cds = *reinterpret_cast<const COPYDATASTRUCT *>(lParam);
            switch (cds.dwData)
            {
                case CD_COMMAND_LINE:
                    HandleCopyDataCommandLine(cds);
                    break;
                case CD_COMMAND_MOVETAB:
                    return (LRESULT) HandleCopyDataMoveTab(cds);
                    break;
            }
        }
        break;
    case WM_UPDATEAVAILABLE:
        CAppUtils::ShowUpdateAvailableDialog(*this);
        break;
    case WM_AFTERINIT:
        HandleAfterInit();
        break;
    case WM_NOTIFY:
        {
            LPNMHDR pnmhdr = reinterpret_cast<LPNMHDR>(lParam);
            APPVERIFY(pnmhdr != nullptr);
            if (pnmhdr == nullptr)
                return 0;
            const NMHDR& nmhdr = *pnmhdr;

            switch (nmhdr.code)
            {
            case TTN_GETDISPINFO:
                {
                LPNMTTDISPINFO lpnmtdi = reinterpret_cast<LPNMTTDISPINFO>(lParam);
                HandleGetDispInfo((int)nmhdr.idFrom, lpnmtdi);
                }
                break;
            }

            if (nmhdr.idFrom == (UINT_PTR)&m_TabBar || nmhdr.hwndFrom == m_TabBar)
            {
                return HandleTabBarEvents(nmhdr, wParam, lParam);
            }
            else if (nmhdr.idFrom == (UINT_PTR)&m_editor || nmhdr.hwndFrom == m_editor)
            {
                return HandleEditorEvents(nmhdr, wParam, lParam);
            }
            else if (nmhdr.idFrom == (UINT_PTR)&m_fileTree || nmhdr.hwndFrom == m_fileTree)
            {
                return HandleFileTreeEvents(nmhdr, wParam, lParam);
            }
        }
        break;

    case WM_ACTIVATEAPP:
    {
        // Only restore the window position on activation as that's when we'll
        // be showing any window and needing to use it.
        if (!m_windowRestored)
        {
            m_windowRestored = true; // Do now to avoid recursion which I've seen occasionaly.
            CIniSettings::Instance().RestoreWindowPos(L"MainWindow", *this, 0);
        }
    }
        break;
    case WM_ACTIVATE:
        // Ensure proper focus handling occurs such as
        // making sure WM_SETFOCUS is generated in all situations.
        return DefWindowProc(hwnd,uMsg,wParam,lParam);

    case WM_SETFOCUS: // lParam HWND that is losing focus.
    {
        SetFocus(m_editor);
        m_editor.Call(SCI_SETFOCUS, true);
        // the update check can show a dialog. Doing this in the
        // WM_SETFOCUS handler causes problems due to the dialog
        // having its own message queue.
        // See issue #129 https://sourceforge.net/p/bowpad-sk/tickets/129/
        // To avoid these problems, set a timer instead. The timer
        // will fire after all messages related to the focus change have
        // been handled, and then it is save to show a message box dialog.
        SetTimer(*this, TIMER_UPDATECHECK, 200, NULL);
    }
        break;
    case WM_CLIPBOARDUPDATE:
        HandleClipboardUpdate();
        break;
    case WM_TIMER:
        if (wParam >= COMMAND_TIMER_ID_START)
            CCommandHandler::Instance().OnTimer((UINT)wParam);
        if (wParam == TIMER_UPDATECHECK)
        {
            KillTimer(*this, TIMER_UPDATECHECK);
            CheckForOutsideChanges();
        }
        else if (wParam == TIMER_ZOOM)
        {
            KillTimer(*this, TIMER_ZOOM);
            HandleStatusBarZoom();
        }
        break;
    case WM_DESTROY:
        g_pFramework->Destroy();
        PostQuitMessage(0);
        break;
    case WM_CLOSE:
        CCommandHandler::Instance().OnClose();
        // Close all tabs, don't leave any open even a blank one.
        if (CloseAllTabs(false))
        {
            CIniSettings::Instance().SaveWindowPos(L"MainWindow", *this);
            ::DestroyWindow(m_hwnd);
        }
        break;
    case WM_STATUSBAR_MSG:
        HandleStatusBar(wParam, lParam);
        break;
    case WM_ENTERMENULOOP:
        m_inMenuLoop = true;
        break;
    case WM_EXITMENULOOP:
        m_inMenuLoop = false;
        break;
    case WM_CANHIDECURSOR:
    {
        BOOL* result = reinterpret_cast<BOOL*>(lParam);
        *result = m_inMenuLoop ? FALSE : TRUE;
        break;
    }
    default:
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }

    return 0;
}

LRESULT CMainWindow::HandleTabBarEvents(const NMHDR& nmhdr, WPARAM /*wParam*/, LPARAM lParam)
{
    TBHDR tbh = {};
    if (nmhdr.idFrom != (UINT_PTR)&m_TabBar)
    {
        // Events that are not from CTabBar might be
        // lower level and of type HMHDR, not TBHDR
        // and therefore missing tabOrigin.
        // In case they are NMHDR, map them to TBHDR and set
        // an obviously bogus value for tabOrigin so it isn't
        // used by mistake.
        // We need a TBHDR type to notify commands with.
        tbh.hdr = nmhdr;
        tbh.tabOrigin = ~0; // Obviously bogus value.
        lParam = (LPARAM)&tbh;
    }

    TBHDR* ptbhdr = reinterpret_cast<TBHDR*>(lParam);
    const TBHDR& tbhdr = *ptbhdr;
    assert(tbhdr.hdr.code == nmhdr.code);

    CCommandHandler::Instance().TabNotify(ptbhdr);

    switch (nmhdr.code)
    {
    case TCN_GETCOLOR:
    {
        if (tbhdr.tabOrigin >= 0 && tbhdr.tabOrigin < m_TabBar.GetItemCount())
        {
            int docId = m_TabBar.GetIDFromIndex(tbhdr.tabOrigin);
            APPVERIFY(docId >= 0);
            if (m_DocManager.HasDocumentID(docId))
            {
                auto clr = m_DocManager.GetColorForDocument(docId);
                return CTheme::Instance().GetThemeColor(clr);
            }
        }
        else
            APPVERIFY(false);
        break;
    }
    case TCN_SELCHANGE:
        HandleTabChange(nmhdr);
        InvalidateRect(m_fileTree, NULL, TRUE);
        break;
    case TCN_SELCHANGING:
        HandleTabChanging(nmhdr);
        break;
    case TCN_TABDELETE:
        HandleTabDelete(tbhdr);
        break;
    case TCN_TABDROPPEDOUTSIDE:
    {
        DWORD pos = GetMessagePos();
        POINT pt{ GET_X_LPARAM(pos), pt.y = GET_Y_LPARAM(pos) };
        HandleTabDroppedOutside(ptbhdr->tabOrigin, pt);
    }
    break;
    case TCN_GETDROPICON:
    {
        DWORD pos = GetMessagePos();
        POINT pt{ GET_X_LPARAM(pos), GET_Y_LPARAM(pos) };
        auto hPtWnd = WindowFromPoint(pt);
        if (hPtWnd == m_fileTree)
        {
            auto docID = m_TabBar.GetIDFromIndex(ptbhdr->tabOrigin);
            CDocument doc = m_DocManager.GetDocumentFromID(docID);
            if (!doc.m_path.empty())
            {
                if (GetKeyState(VK_CONTROL) & 0x8000)
                    return (LRESULT)::LoadCursor(hResource, MAKEINTRESOURCE(IDC_DRAG_COPYFILE));
                else
                    return (LRESULT)::LoadCursor(hResource, MAKEINTRESOURCE(IDC_DRAG_MOVEFILE));
            }
        }
    }
    break;
    }
    return 0;
}

LRESULT CMainWindow::HandleEditorEvents(const NMHDR& nmhdr, WPARAM wParam, LPARAM lParam)
{
    if (nmhdr.code == NM_COOLSB_CUSTOMDRAW)
        return m_editor.HandleScrollbarCustomDraw(wParam, (NMCSBCUSTOMDRAW *)lParam);

    Scintilla::SCNotification* pScn = reinterpret_cast<Scintilla::SCNotification *>(lParam);
    const Scintilla::SCNotification& scn = *pScn;

    CCommandHandler::Instance().ScintillaNotify(pScn);
    switch (scn.nmhdr.code)
    {
    case SCN_PAINTED:
        m_editor.UpdateLineNumberWidth();
        break;
    case SCN_SAVEPOINTREACHED:
    case SCN_SAVEPOINTLEFT:
        HandleSavePoint(scn);
        break;
    case SCN_MARGINCLICK:
        m_editor.MarginClick(pScn);
        break;
    case SCN_UPDATEUI:
        HandleUpdateUI(scn);
        break;
    case SCN_CHARADDED:
        HandleAutoIndent(scn);
        break;
    case SCN_MODIFYATTEMPTRO:
        HandleWriteProtectedEdit();
        break;
    case SCN_DOUBLECLICK:
        return HandleDoubleClick(scn);
    case SCN_DWELLSTART:
        HandleDwellStart(scn);
        break;
    case SCN_DWELLEND:
        m_editor.Call(SCI_CALLTIPCANCEL);
        break;
    case SCN_ZOOM:
        UpdateStatusBar(false);
        break;
    }
    return 0;
}

LRESULT CMainWindow::HandleFileTreeEvents(const NMHDR& nmhdr, WPARAM /*wParam*/, LPARAM lParam)
{
    LPNMTREEVIEW pnmtv = (LPNMTREEVIEW)lParam;
    switch (nmhdr.code)
    {
    case NM_RETURN:
    {
        auto path = m_fileTree.GetFilePathForSelItem();
        if (!path.empty())
        {
            bool control = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
            unsigned int openFlags = OpenFlags::AddToMRU;
            if (control)
                openFlags |= OpenFlags::OpenIntoActiveTab;
            OpenFile(path.c_str(), openFlags);
            return TRUE;
        }
    }
    break;
    case NM_DBLCLK:
    {
        auto path = m_fileTree.GetFilePathForHitItem();
        if (!path.empty())
        {
            bool control = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
            unsigned int openFlags = OpenFlags::AddToMRU;
            if (control)
                openFlags |= OpenFlags::OpenIntoActiveTab;
            OpenFile(path.c_str(), openFlags);
            PostMessage(*this, WM_SETFOCUS, TRUE, 0);
        }
    }
    break;
    case NM_RCLICK:
    {
        // the tree control does not get the WM_CONTEXTMENU message.
        // see http://support.microsoft.com/kb/222905 for details about why
        // so we have to work around this and handle the NM_RCLICK instead
        // and send the WM_CONTEXTMENU message from here.
        SendMessage(m_fileTree, WM_CONTEXTMENU, (WPARAM)m_hwnd, GetMessagePos());
    }
    break;
    case TVN_ITEMEXPANDING:
    {
        if ((pnmtv->action & TVE_EXPAND) != 0)
            m_fileTree.Refresh(pnmtv->itemNew.hItem);
    }
    break;
    case NM_CUSTOMDRAW:
    {
        if (CTheme::Instance().IsDarkTheme())
        {
            LPNMTVCUSTOMDRAW lpNMCustomDraw = (LPNMTVCUSTOMDRAW)lParam;
            // only do custom drawing when in dark theme
            switch (lpNMCustomDraw->nmcd.dwDrawStage)
            {
            case CDDS_PREPAINT:
                return CDRF_NOTIFYITEMDRAW;
            case CDDS_ITEMPREPAINT:
            {
                lpNMCustomDraw->clrText = CTheme::Instance().GetThemeColor(RGB(0, 0, 0));
                lpNMCustomDraw->clrTextBk = CTheme::Instance().GetThemeColor(RGB(255, 255, 255));

                return CDRF_DODEFAULT;
            }
            break;
            }
            return CDRF_DODEFAULT;
        }
    }
    break;
    default:
        break;
    }
    return 0;
}

void CMainWindow::HandleStatusBar(WPARAM wParam, LPARAM lParam)
{
    switch (wParam)
    {
    case WM_LBUTTONDOWN:
    {
        switch (lParam)
        {
        case STATUSBAR_ZOOM:
            SetTimer(*this, TIMER_ZOOM, GetDoubleClickTime(), nullptr);
            break;
        }
        break;
    }
    case WM_LBUTTONDBLCLK:
    {
        switch (lParam)
        {
        case STATUSBAR_TABSPACE:
            m_editor.Call(SCI_SETUSETABS, !m_editor.Call(SCI_GETUSETABS));
            break;
        case STATUSBAR_TYPING_MODE:
            m_editor.Call(SCI_EDITTOGGLEOVERTYPE);
            break;
        case STATUSBAR_ZOOM:
            KillTimer(*this, TIMER_ZOOM);
            m_editor.Call(SCI_SETZOOM, 0);
            break;
        }
        UpdateStatusBar(true);
    }
    break;
    case WM_LBUTTONUP:
    //case WM_CONTEXTMENU:
    {
        switch (lParam)
        {
        case STATUSBAR_EOL_FORMAT:
            HandleStatusBarEOLFormat();
            break;
        //case STATUSBAR_ZOOM:
            //HandleStatusBarZoom();
            //break;
        }
    }
    break;
    }
}

void CMainWindow::HandleStatusBarEOLFormat()
{
    DWORD msgpos = GetMessagePos();
    int xPos = GET_X_LPARAM(msgpos);
    int yPos = GET_Y_LPARAM(msgpos);

    HMENU hPopup = CreatePopupMenu();
    if (!hPopup)
        return;
    OnOutOfScope(
        DestroyMenu(hPopup);
    );
    int currentEolMode = (int) m_editor.Call(SCI_GETEOLMODE);
    EOLFormat currentEolFormat = ToEOLFormat(currentEolMode);
    static const EOLFormat options[] = { WIN_FORMAT, MAC_FORMAT, UNIX_FORMAT };
    const size_t numOptions = std::size(options);
    for (size_t i = 0; i < numOptions; ++i)
    {
        std::wstring eolName = GetEOLFormatDescription(options[i]);
        UINT menuItemFlags = MF_STRING;
        if (options[i] == currentEolFormat)
            menuItemFlags |= MF_CHECKED | MF_DISABLED;
        AppendMenu(hPopup, menuItemFlags, i + 1, eolName.c_str());
    }
    auto result = TrackPopupMenu(hPopup, TPM_RIGHTALIGN | TPM_TOPALIGN | TPM_RETURNCMD, xPos, yPos, 0, *this, NULL);
    if (result != FALSE)
    {
        size_t optionIndex = size_t(result) - 1;
        auto selectedEolFormat = options[optionIndex];
        if (selectedEolFormat != currentEolFormat)
        {
            auto selectedEolMode = ToEOLMode(selectedEolFormat);
            m_editor.Call(SCI_SETEOLMODE, selectedEolMode);
            m_editor.Call(SCI_CONVERTEOLS, selectedEolMode);
            int id = m_TabBar.GetCurrentTabId();
            if (m_DocManager.HasDocumentID(id))
            {
                CDocument doc = m_DocManager.GetDocumentFromID(id);
                doc.m_format = selectedEolFormat;
                m_DocManager.SetDocument(id, doc);
                UpdateStatusBar(true);
            }
        }
    }
}

void CMainWindow::HandleStatusBarZoom()
{
    DWORD msgpos = GetMessagePos();
    int xPos = GET_X_LPARAM(msgpos);
    int yPos = GET_Y_LPARAM(msgpos);

    HMENU hPopup = CreatePopupMenu();
    if (hPopup)
    {
        OnOutOfScope(
            DestroyMenu(hPopup);
        );
        AppendMenu(hPopup, MF_STRING, 20, L"20%");
        AppendMenu(hPopup, MF_STRING, 50, L"50%");
        AppendMenu(hPopup, MF_STRING, 70, L"70%");
        AppendMenu(hPopup, MF_STRING, 100, L"100%");
        AppendMenu(hPopup, MF_STRING, 150, L"150%");
        AppendMenu(hPopup, MF_STRING, 200, L"200%");
        AppendMenu(hPopup, MF_STRING, 400, L"400%");

        auto cmd = TrackPopupMenu(hPopup, TPM_RIGHTALIGN | TPM_TOPALIGN | TPM_RETURNCMD, xPos, yPos, 0, *this, NULL);
        if (cmd != 0)
            SetZoomPC(cmd);
    }
}

LRESULT CMainWindow::DoCommand(int id)
{
    switch (id)
    {
    case cmdExit:
        PostMessage(m_hwnd, WM_CLOSE, 0, 0);
        break;
    case cmdNew:
        OpenNewTab();
        break;
    case cmdClose:
        // TODO:
        // Clicking a tab header's X generates an TCN_TABDELETE event,
        // that is forwarded to the commands through TabNotify,
        // and results eventually in CloseTab being called.
        // But closing this way (Ctrl-F4) and just call CloseTab,
        // No TCN_TABDELETE message is sent to the children.
        // If we want that we could call
        // m_TabBar.NotifyTabDelete(m_TabBar.GetCurrentTabIndex());
        // instead which would do that. But I'm leaving things
        // as they are for now as nothing (now) uses TCN_TABDELETE
        // and it still isn't generated in every circumstance
        // anyway even if we catch this instance and do it here.
        // But I want to record this fact as this area needs some
        // attention, but later. I don't want to risk breaking
        // anything yet until the topic gets more thought / discussion.
        // But ultimate TCN_TABDELETE probably wants to be generated
        // here as a "Hey command, I'm thinking about closing,
        // are you cool with that", kind of thing.
        CloseTab(m_TabBar.GetCurrentTabIndex());
        break;
    case cmdCloseAll:
        CloseAllTabs();
        break;
    case cmdCloseAllButThis:
        CloseAllButCurrentTab();
        break;
    case cmdCopyPath:
        CopyCurDocPathToClipboard();
        break;
    case cmdCopyName:
        CopyCurDocNameToClipboard();
        break;
    case cmdCopyDir:
        CopyCurDocDirToClipboard();
        break;
    case cmdExplore:
        ShowCurDocInExplorer();
        break;
    case cmdExploreProperties:
        ShowCurDocExplorerProperties();
        break;
    case cmdPasteHistory:
        PasteHistory();
        break;
    case cmdAbout:
        About();
        break;
    default:
        {
            ICommand* pCmd = CCommandHandler::Instance().GetCommand(id);
            if (pCmd)
                pCmd->Execute();
        }
        break;
    }
    return 1;
}

bool CMainWindow::Initialize()
{
    // Tell UAC that lower integrity processes are allowed to send WM_COPYDATA messages to this process (or window)
    HMODULE hDll = GetModuleHandle(TEXT("user32.dll"));
    if (hDll)
    {
        // first try ChangeWindowMessageFilterEx, if it's not available (i.e., running on Vista), then
        // try ChangeWindowMessageFilter.
        typedef BOOL (WINAPI *MESSAGEFILTERFUNCEX)(HWND hWnd,UINT message,DWORD action,VOID* pChangeFilterStruct);
        MESSAGEFILTERFUNCEX func = (MESSAGEFILTERFUNCEX)::GetProcAddress( hDll, "ChangeWindowMessageFilterEx" );

        if (func)
        {
            func(*this,       WM_COPYDATA, MSGFLT_ALLOW, nullptr );
            func(m_editor,    WM_COPYDATA, MSGFLT_ALLOW, nullptr );
            func(m_TabBar,    WM_COPYDATA, MSGFLT_ALLOW, nullptr );
            func(m_StatusBar, WM_COPYDATA, MSGFLT_ALLOW, nullptr );
            func(m_fileTree, WM_COPYDATA, MSGFLT_ALLOW, nullptr );
        }
        else
        {
            ChangeWindowMessageFilter(WM_COPYDATA, MSGFLT_ADD);
        }
    }

    m_fileTree.Init(hResource, *this);
    ShowWindow(m_fileTree, m_fileTreeVisible ? SW_SHOW : SW_HIDE);
    CCommandHandler::Instance().AddCommand(&m_fileTree);
    m_treeWidth = (int)CIniSettings::Instance().GetInt64(L"View", L"FileTreeWidth", 200);
    m_treeWidth = max(50, m_treeWidth);
    RECT rc;
    GetClientRect(*this, &rc);
    m_treeWidth = min(m_treeWidth, rc.right - rc.left - 500);
    m_editor.Init(hResource, *this);
    // Each value is the right edge of each status bar element.
    m_StatusBar.Init(hResource, *this, {100, 300, 550, 650, 700, 800, 830, 865, 925, 1010});
    m_TabBar.Init(hResource, *this);
    // Note DestroyIcon not technically needed here but we may as well leave in
    // in case someone changes things to load a non static resource.
    HIMAGELIST hImgList = ImageList_Create(13, 13, ILC_COLOR32 | ILC_MASK, 0, 3);
    m_TabBarImageList.reset(hImgList);
    HICON hIcon = ::LoadIcon(hResource, MAKEINTRESOURCE(IDI_SAVED_ICON));
    assert(hIcon != nullptr);
    ImageList_AddIcon(hImgList, hIcon);
    ::DestroyIcon(hIcon);
    hIcon = ::LoadIcon(hResource, MAKEINTRESOURCE(IDI_UNSAVED_ICON));
    assert(hIcon != nullptr);
    ImageList_AddIcon(hImgList, hIcon);
    ::DestroyIcon(hIcon);
    hIcon = ::LoadIcon(hResource, MAKEINTRESOURCE(IDI_READONLY_ICON));
    assert(hIcon != nullptr);
    ImageList_AddIcon(hImgList, hIcon);
    ::DestroyIcon(hIcon);
    m_TabBar.SetImageList(hImgList);

    if (!CreateRibbon())
        return false;

    CCommandHandler::Instance().Init(this);
    CKeyboardShortcutHandler::Instance().UpdateTooltips(true);
    AddClipboardFormatListener(*this);
    return true;
}

bool CMainWindow::CreateRibbon()
{
    assert(!g_pFramework); // Not supporting re-initializing.
    // Here we instantiate the Ribbon framework object.
    HRESULT hr = CoCreateInstance(CLSID_UIRibbonFramework, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&g_pFramework));
    if (CAppUtils::FailedShowMessage(hr))
        return false;

    hr = g_pFramework->Initialize(*this, this);
    if (CAppUtils::FailedShowMessage(hr))
        return false;

    // Finally, we load the binary markup.  This will initiate callbacks to the IUIApplication object
    // that was provided to the framework earlier, allowing command handlers to be bound to individual
    // commands.
    hr = g_pFramework->LoadUI(hRes, L"BOWPAD_RIBBON");
    if (CAppUtils::FailedShowMessage(hr))
        return false;
    return true;
}

void CMainWindow::HandleCreate(HWND hwnd)
{
    m_hwnd = hwnd;
    Initialize();

    PostMessage(m_hwnd, WM_AFTERINIT, 0, 0);

    if (SysInfo::Instance().IsUACEnabled() && SysInfo::Instance().IsElevated())
    {
        // in case we're running elevated, use a BowPad icon with a shield
        HICON hIcon = (HICON)::LoadImage(hResource, MAKEINTRESOURCE(IDI_BOWPAD_ELEVATED), IMAGE_ICON, 0, 0, LR_DEFAULTSIZE|LR_SHARED);
        ::SendMessage(m_hwnd, WM_SETICON, ICON_BIG, (LPARAM)hIcon);
        ::SendMessage(m_hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
    }

    if (SysInfo::Instance().IsUACEnabled() && SysInfo::Instance().IsElevated())
    {
        ITaskbarList3Ptr pTaskbarInterface;
        HRESULT hr = CoCreateInstance(CLSID_TaskbarList, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&pTaskbarInterface));
        if (SUCCEEDED(hr))
        {
            pTaskbarInterface->SetOverlayIcon(m_hwnd, m_hShieldIcon, L"elevated");
        }
    }
}

void CMainWindow::HandleAfterInit()
{
    UpdateWindow(*this);
    FileTreeBlockRefresh(true);
    OnOutOfScope(FileTreeBlockRefresh(false));
    CCommandHandler::Instance().AfterInit();
    
    if ((m_pathsToOpen.size() == 1) && (PathIsDirectory(m_pathsToOpen.begin()->first.c_str())))
    {
        if (!m_fileTree.GetPath().empty()) // File tree not empty: create a new empty tab first.
            OpenNewTab();
        m_fileTree.SetPath(m_pathsToOpen.begin()->first);
        ShowFileTree(true);
    }
    else
    {
        for (const auto& path : m_pathsToOpen)
        {
            unsigned int openFlags = OpenFlags::AskToCreateIfMissing;
            if (m_bPathsToOpenMRU)
                openFlags |= OpenFlags::AddToMRU;
            if (OpenFile(path.first, openFlags) >= 0)
            {
                if (path.second != (size_t)-1)
                    GoToLine(path.second);
            }
        }
    }
    m_pathsToOpen.clear();
    m_bPathsToOpenMRU = true;
    if (!m_elevatepath.empty())
    {
        ElevatedSave(m_elevatepath, m_elevatesavepath, m_initLine);
        m_elevatepath.clear();
        m_elevatesavepath.clear();
    }
    if (!m_tabmovepath.empty())
    {
        TabMove(m_tabmovepath, m_tabmovesavepath, m_tabmovemod, m_initLine);
        m_tabmovepath.clear();
        m_tabmovesavepath.clear();
    }
    EnsureAtLeastOneTab();

    auto t = std::thread([ = ]
    {
        bool bNewer = CAppUtils::CheckForUpdate(false);
        if (bNewer)
            PostMessage(m_hwnd, WM_UPDATEAVAILABLE, 0, 0);
    });
    t.detach();
}

void CMainWindow::ResizeChildWindows()
{
    RECT rect;
    GetClientRect(*this, &rect);
    if (!IsRectEmpty(&rect))
    {
        const UINT flags = SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_NOZORDER | SWP_SHOWWINDOW | SWP_NOCOPYBITS;
        const UINT noshowflags = SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_NOZORDER | SWP_NOCOPYBITS;
        HDWP hDwp = BeginDeferWindowPos(4);
        DeferWindowPos(hDwp, m_StatusBar, nullptr, rect.left, rect.bottom - m_StatusBar.GetHeight(), rect.right - rect.left, m_StatusBar.GetHeight(), flags);
        DeferWindowPos(hDwp, m_TabBar, nullptr, rect.left, rect.top + m_RibbonHeight, rect.right - rect.left, rect.bottom - rect.top, flags);
        RECT tabrc;
        TabCtrl_GetItemRect(m_TabBar, 0, &tabrc);
        MapWindowPoints(m_TabBar, *this, (LPPOINT)&tabrc, 2);
        int treeWidth = 0;
        if (m_fileTreeVisible && !m_fileTree.GetPath().empty())
            treeWidth = m_treeWidth;
        DeferWindowPos(hDwp, m_editor, nullptr, rect.left + treeWidth, rect.top + m_RibbonHeight + tabrc.bottom - tabrc.top, rect.right - rect.left - treeWidth, rect.bottom - (m_RibbonHeight + tabrc.bottom - tabrc.top) - m_StatusBar.GetHeight(), flags);
        DeferWindowPos(hDwp, m_fileTree, nullptr, rect.left, rect.top + m_RibbonHeight + tabrc.bottom - tabrc.top, treeWidth ? treeWidth - 5 : 0, rect.bottom - (m_RibbonHeight + tabrc.bottom - tabrc.top) - m_StatusBar.GetHeight(), m_fileTreeVisible ? flags : noshowflags);
        EndDeferWindowPos(hDwp);
        m_StatusBar.Resize();
    }
}

void CMainWindow::EnsureNewLineAtEnd(const CDocument& doc)
{
    size_t endpos = m_editor.Call(SCI_GETLENGTH);
    char c = (char)m_editor.Call(SCI_GETCHARAT, endpos - 1);
    if ((c != '\r') && (c != '\n'))
    {
        switch (doc.m_format)
        {
            case WIN_FORMAT:
                m_editor.AppendText(2, "\r\n");
                break;
            case MAC_FORMAT:
                m_editor.AppendText(1, "\r");
                break;
            case UNIX_FORMAT:
            default:
                m_editor.AppendText(1, "\n");
                break;
        }
    }
}

bool CMainWindow::SaveCurrentTab(bool bSaveAs /* = false */)
{
    struct task_mem_deleter
    {
        void operator()(wchar_t buf[])
        {
            if (buf != nullptr)
                CoTaskMemFree(buf);
        }
    };

    int docID = m_TabBar.GetCurrentTabId();
    if (docID < 0)
        return false;
    if (!m_DocManager.HasDocumentID(docID))
        return false;

    CDocument doc = m_DocManager.GetDocumentFromID(docID);
    if (doc.m_path.empty() || bSaveAs || doc.m_bDoSaveAs)
    {
        bSaveAs = true;
        PreserveChdir keepCWD;

        IFileSaveDialogPtr pfd;

        HRESULT hr = pfd.CreateInstance(CLSID_FileSaveDialog, nullptr, CLSCTX_INPROC_SERVER);
        if (CAppUtils::FailedShowMessage(hr))
            return false;

        // Set the dialog options
        DWORD dwOptions;
        hr = pfd->GetOptions(&dwOptions);
        if (CAppUtils::FailedShowMessage(hr))
            return false;
        hr = pfd->SetOptions(dwOptions | FOS_FORCEFILESYSTEM | FOS_OVERWRITEPROMPT);
        if (CAppUtils::FailedShowMessage(hr))
            return false;

        std::wstring s = GetAppName();
        s += L" - ";
        s += m_TabBar.GetCurrentTitle();
        hr = pfd->SetTitle(s.c_str());
        if (CAppUtils::FailedShowMessage(hr))
            return false;

        // set the default folder to the folder of the current tab
        if (!doc.m_path.empty())
        {
            std::wstring folder = CPathUtils::GetParentDirectory(doc.m_path);
            if (folder.empty())
                folder = CPathUtils::GetCWD();
            std::wstring filename = CPathUtils::GetFileName(doc.m_path);
            IShellItemPtr psiDefFolder = nullptr;
            hr = SHCreateItemFromParsingName(folder.c_str(), nullptr, IID_PPV_ARGS(&psiDefFolder));
            if (CAppUtils::FailedShowMessage(hr))
                return false;
            hr = pfd->SetFolder(psiDefFolder);
            if (CAppUtils::FailedShowMessage(hr))
                return false;
            hr = pfd->SetFileName(filename.c_str());
            if (CAppUtils::FailedShowMessage(hr))
                return false;
        }

        // Show the save file dialog
        hr = pfd->Show(*this);
        if (CAppUtils::FailedShowMessage(hr))
            return false;
        IShellItemPtr psiResult = nullptr;
        hr = pfd->GetResult(&psiResult);
        if (CAppUtils::FailedShowMessage(hr))
            return false;
        PWSTR pszPath = nullptr;
        hr = psiResult->GetDisplayName(SIGDN_FILESYSPATH, &pszPath);
        if (CAppUtils::FailedShowMessage(hr))
            return false;
        std::unique_ptr<wchar_t[], task_mem_deleter> path(pszPath);
        doc.m_path = pszPath;
        CMRU::Instance().AddPath(doc.m_path);
        if (m_fileTree.GetPath().empty())
        {
            m_fileTree.SetPath(CPathUtils::GetParentDirectory(doc.m_path));
            ResizeChildWindows();
        }
    }
    if (!doc.m_path.empty())
    {
        doc.m_bDoSaveAs = false;
        if (doc.m_bTrimBeforeSave)
        {
            auto cmd = CCommandHandler::Instance().GetCommand(cmdTrim);
            cmd->Execute();
        }

        if (doc.m_bEnsureNewlineAtEnd)
            EnsureNewLineAtEnd(doc);

        bool bTabMoved = false;
        if (!m_DocManager.SaveFile(*this, doc, bTabMoved))
        {
            if (bTabMoved)
                CloseTab(m_TabBar.GetCurrentTabIndex(), true);
            return false;
        }

        if (_wcsicmp(CIniSettings::Instance().GetIniPath().c_str(), doc.m_path.c_str()) == 0)
        {
            CIniSettings::Instance().Reload();
        }

        doc.m_bIsDirty = false;
        doc.m_bNeedsSaving = false;
        m_DocManager.UpdateFileTime(doc, false);
        if (bSaveAs)
        {
            doc.m_language = CLexStyles::Instance().GetLanguageForDocument(doc, m_scratchEditor);
            m_editor.SetupLexerForLang(doc.m_language);
        }
        std::wstring sFileName = CPathUtils::GetFileName(doc.m_path);
        m_TabBar.SetCurrentTitle(sFileName.c_str());
        m_DocManager.SetDocument(docID, doc);
        // Update tab so the various states are updated (readonly, modified, ...)
        UpdateTab(docID);
        UpdateCaptionBar();
        UpdateStatusBar(true);
        m_editor.Call(SCI_SETSAVEPOINT);
        CCommandHandler::Instance().OnDocumentSave(m_TabBar.GetCurrentTabIndex(), bSaveAs);
    }
    return true;
}

void CMainWindow::ElevatedSave( const std::wstring& path, const std::wstring& savepath, long line )
{
    std::wstring filepath = CPathUtils::GetLongPathname(path);
    int docID = m_DocManager.GetIdForPath(filepath.c_str());
    if (docID != -1)
    {
        int tab = m_TabBar.GetIndexFromID(docID);
        m_TabBar.ActivateAt(tab);
        CDocument doc = m_DocManager.GetDocumentFromID(docID);
        doc.m_path = CPathUtils::GetLongPathname(savepath);
        m_DocManager.SetDocument(docID, doc);
        SaveCurrentTab();
        UpdateCaptionBar();
        UpdateStatusBar(true);
        GoToLine(line);
        m_fileTree.SetPath(CPathUtils::GetParentDirectory(savepath));
        ResizeChildWindows();
        // delete the temp file used for the elevated save
        DeleteFile(path.c_str());
    }
}

void CMainWindow::EnsureAtLeastOneTab()
{
    if (m_TabBar.GetItemCount() == 0)
        DoCommand(cmdNew);
}

void CMainWindow::GoToLine( size_t line )
{
    m_editor.GotoLine((long)line);
}

int CMainWindow::GetZoomPC() const
{
    int fontsize = (int)m_editor.ConstCall(SCI_STYLEGETSIZE, STYLE_DEFAULT);
    int zoom = (int)m_editor.ConstCall(SCI_GETZOOM);
    int zoomfactor = (fontsize + zoom) * 100 / fontsize;
    if (zoomfactor == 0)
        zoomfactor = 1;
    return zoomfactor;
}

void CMainWindow::SetZoomPC(int zoomPC)
{
    int fontsize = (int)m_editor.Call(SCI_STYLEGETSIZE, STYLE_DEFAULT);
    int zoom = (fontsize * zoomPC / 100) - fontsize;
    m_editor.Call(SCI_SETZOOM, zoom);
}

void CMainWindow::UpdateStatusBar( bool bEverything )
{
    static ResString rsStatusTTDocSize(hRes, IDS_STATUSTTDOCSIZE);
    static ResString rsStatusTTCurPos(hRes, IDS_STATUSTTCURPOS);
    static ResString rsStatusTTEOF(hRes, IDS_STATUSTTEOF);
    static ResString rsStatusTTTyping(hRes, IDS_STATUSTTTYPING);
    static ResString rsStatusTTTypingOvl(hRes, IDS_STATUSTTTYPINGOVL);
    static ResString rsStatusTTTypingIns(hRes, IDS_STATUSTTTYPINGINS);
    static ResString rsStatusTTTabs(hRes, IDS_STATUSTTTABS);
    static ResString rsStatusSelection(hRes, IDS_STATUSSELECTION);
    static ResString rsStatusTTTabSpaces(hRes, IDS_STATUSTTTABSPACES);
    static ResString rsStatusTTEncoding(hRes, IDS_STATUSTTENCODING);
    static ResString rsStatusZoom(hRes, IDS_STATUSZOOM);

    TCHAR strLnCol[128];
    TCHAR strSel[64];
    size_t selByte = 0;
    size_t selLine = 0;
    long selTextMarkerCount = m_editor.GetSelTextMarkerCount();
    if (m_editor.GetSelectedCount(selByte, selLine) && selByte)
        swprintf_s(strSel, rsStatusSelection, selByte, selLine, selTextMarkerCount);
    else
        _tcscpy_s(strSel, L"Sel: N/A");
    auto curPos = m_editor.Call(SCI_GETCURRENTPOS);
    long line = (long)m_editor.Call(SCI_LINEFROMPOSITION, curPos) + 1;
    long column = (long)m_editor.Call(SCI_GETCOLUMN, curPos) + 1;
    swprintf_s(strLnCol, L"Ln: %ld    Col: %ld",
               line, column);
    std::wstring ttcurpos = CStringUtils::Format(rsStatusTTCurPos,
        line, column, selByte, selLine, selTextMarkerCount);

    auto lengthInBytes = m_editor.Call(SCI_GETLENGTH);
    auto lineCount = m_editor.Call(SCI_GETLINECOUNT);
    auto ttdocsize = CStringUtils::Format(rsStatusTTDocSize, lengthInBytes, lineCount);
    m_StatusBar.SetText(strLnCol, ttdocsize.c_str(), STATUSBAR_CUR_POS);
    m_StatusBar.SetText(strSel, ttcurpos.c_str(), STATUSBAR_SEL);

    auto overType = m_editor.Call(SCI_GETOVERTYPE);
    auto tttyping = CStringUtils::Format(rsStatusTTTyping, overType ? rsStatusTTTypingOvl.c_str() : rsStatusTTTypingIns.c_str());
    m_StatusBar.SetText(overType ? L"OVR" : L"INS", tttyping.c_str(), STATUSBAR_TYPING_MODE);
    bool bCapsLockOn = (GetKeyState(VK_CAPITAL)&0x01)!=0;
    m_StatusBar.SetText(bCapsLockOn ? L"CAPS" : L"", nullptr, STATUSBAR_CAPS);
    m_StatusBar.SetText(m_editor.Call(SCI_GETUSETABS) ? L"Tabs" : L"Spaces", rsStatusTTTabSpaces.c_str(), STATUSBAR_TABSPACE);

    int zoomfactor = GetZoomPC();
    auto szoomfactor = CStringUtils::Format(rsStatusZoom, zoomfactor);
    m_StatusBar.SetText(szoomfactor.c_str(), nullptr, STATUSBAR_ZOOM);

    if (bEverything)
    {
        CDocument doc = m_DocManager.GetDocumentFromID(m_TabBar.GetCurrentTabId());
        int eolMode = int(m_editor.Call(SCI_GETEOLMODE));
        APPVERIFY(ToEOLMode(doc.m_format) == eolMode);
        auto eolDesc = GetEOLFormatDescription(doc.m_format);
        m_StatusBar.SetText(doc.m_language.c_str(), nullptr, STATUSBAR_DOC_TYPE);
        auto tteof = CStringUtils::Format(rsStatusTTEOF, eolDesc.c_str());
        m_StatusBar.SetText(eolDesc.c_str(), tteof.c_str(), STATUSBAR_EOL_FORMAT);
        auto ttencoding = CStringUtils::Format(rsStatusTTEncoding, doc.GetEncodingString().c_str());
        m_StatusBar.SetText(doc.GetEncodingString().c_str(), ttencoding.c_str(), STATUSBAR_UNICODE_TYPE);

        auto tabCount = m_TabBar.GetItemCount();
        auto tttabs = CStringUtils::Format(rsStatusTTTabs, tabCount);
        m_StatusBar.SetText(CStringUtils::Format(L"Open: %d", tabCount).c_str(), tttabs.c_str(), STATUSBAR_TABS);

        if (SysInfo::Instance().IsUACEnabled() && SysInfo::Instance().IsElevated())
        {
            // in case we're running elevated, use a BowPad icon with a shield
            SendMessage(m_StatusBar, SB_SETICON, 0, (LPARAM)m_hShieldIcon);
        }
    }
}

bool CMainWindow::CloseTab( int tab, bool force /* = false */, bool leaveEmptyTab )
{
    if ((tab < 0) || (tab >= m_TabBar.GetItemCount()))
        return false;
    CDocument doc = m_DocManager.GetDocumentFromID(m_TabBar.GetIDFromIndex(tab));
    if (!force && (doc.m_bIsDirty || doc.m_bNeedsSaving))
    {
        m_TabBar.ActivateAt(tab);
        if (!docloseall || !closealldoall)
            responsetoclosetab = AskToCloseTab();

        if (responsetoclosetab == ResponseToCloseTab::SaveAndClose)
        {
            if (!SaveCurrentTab()) // Save And (fall through to) Close
                return false;
        }

        else if (responsetoclosetab == ResponseToCloseTab::CloseWithoutSaving)
            ;
        // If you don't want to save and close and
        // you don't want to close without saving, you must want to stay open.
        else
        {
            // Cancel And Stay Open
            // activate the tab: user might have clicked another than
            // the active tab to close: user clicked on that tab so activate that tab now
            m_TabBar.ActivateAt(tab);
            return false;
        }
        // Will Close
    }
    // REVIEW: This seems neccessary?
    else
        m_editor.Call(SCI_SETDOCPOINTER, 0, doc.m_document);

    if (leaveEmptyTab && (m_TabBar.GetItemCount() == 1) &&
        (m_editor.Call(SCI_GETTEXTLENGTH)==0) &&
        (m_editor.Call(SCI_GETMODIFY)==0) &&
        doc.m_path.empty())
        return true;  // leave the empty, new document as is

    CCommandHandler::Instance().OnDocumentClose(tab);

    // Prefer to remove the document after the tab has gone as it supports it
    // and deletion causes events that may expect it to be there.
    int docId = m_TabBar.GetIDFromIndex(tab);
    m_TabBar.DeleteItemAt(tab);
    m_DocManager.RemoveDocument(docId);
    if (leaveEmptyTab)
    {
        if (m_TabBar.GetItemCount() == 0)
            EnsureAtLeastOneTab();
        else
        {
            int nextTab = (tab < m_TabBar.GetItemCount()) ? tab : m_TabBar.GetItemCount() - 1;
            m_TabBar.ActivateAt(nextTab);
        }
    }
    return true;
}

bool CMainWindow::CloseAllTabs(bool leaveEmptyTab)
{
    FileTreeBlockRefresh(true);
    OnOutOfScope(FileTreeBlockRefresh(false));

    closealldoall = FALSE;
    OnOutOfScope(closealldoall = FALSE;);
    docloseall = true;
    OnOutOfScope(docloseall = false;);
    do
    {
        if (CloseTab(m_TabBar.GetItemCount()-1, false, leaveEmptyTab) == false)
            return false;
        if (leaveEmptyTab && m_TabBar.GetItemCount() == 1 &&
            m_editor.Call(SCI_GETTEXTLENGTH) == 0 &&
            m_editor.Call(SCI_GETMODIFY) == 0 &&
            m_DocManager.GetDocumentFromID(m_TabBar.GetIDFromIndex(0)).m_path.empty())
            return true;
    } while (m_TabBar.GetItemCount() > 0);
    return true;
}

void CMainWindow::CloseAllButCurrentTab()
{
    FileTreeBlockRefresh(true);
    OnOutOfScope(FileTreeBlockRefresh(false));
    int count = m_TabBar.GetItemCount();
    int current = m_TabBar.GetCurrentTabIndex();

    closealldoall = FALSE;
    OnOutOfScope(closealldoall = FALSE;);
    docloseall = true;
    OnOutOfScope(docloseall = false;);
    for (int i = count - 1; i >= 0; --i)
    {
        if (i != current)
            CloseTab(i);
    }
}

void CMainWindow::UpdateCaptionBar()
{
    int docID = m_TabBar.GetCurrentTabId();
    std::wstring appName = GetAppName();
    std::wstring elev;
    ResString rElev(hRes, IDS_ELEVATED);
    if (SysInfo::Instance().IsUACEnabled() && SysInfo::Instance().IsElevated())
        elev = (LPCWSTR)rElev;

    if (m_DocManager.HasDocumentID(docID))
    {
        CDocument doc = m_DocManager.GetDocumentFromID(docID);

        std::wstring sTitle = elev;
        if (!elev.empty())
            sTitle += L" : ";
        sTitle += doc.m_path.empty() ? m_TabBar.GetCurrentTitle() : doc.m_path;
        sTitle += L" - ";
        sTitle += appName;
        SetWindowText(*this, sTitle.c_str());
    }
    else
    {
        if (!elev.empty())
            appName += L" : " + elev;
        SetWindowText(*this, appName.c_str());
    }
}

void CMainWindow::UpdateTab(int docID)
{
    CDocument doc = m_DocManager.GetDocumentFromID(docID);
    TCITEM tie;
    tie.lParam = -1;
    tie.mask = TCIF_IMAGE;
    if (doc.m_bIsReadonly || (m_editor.Call(SCI_GETREADONLY) != 0))
        tie.iImage = REDONLY_IMG_INDEX;
    else
        tie.iImage = doc.m_bIsDirty || doc.m_bNeedsSaving ? UNSAVED_IMG_INDEX : SAVED_IMG_INDEX;
    TabCtrl_SetItem(m_TabBar, m_TabBar.GetIndexFromID(docID), &tie);
}

ResponseToCloseTab CMainWindow::AskToCloseTab() const
{
    ResString rTitle(hRes, IDS_HASMODIFICATIONS);
    ResString rQuestion(hRes, IDS_DOYOUWANTOSAVE);
    ResString rSave(hRes, IDS_SAVE);
    ResString rDontSave(hRes, IDS_DONTSAVE);
    std::wstring sQuestion = CStringUtils::Format(rQuestion, m_TabBar.GetCurrentTitle().c_str());

    TASKDIALOGCONFIG tdc = { sizeof(TASKDIALOGCONFIG) };
    TASKDIALOG_BUTTON aCustomButtons[2];
    aCustomButtons[0].nButtonID = 100;
    aCustomButtons[0].pszButtonText = rSave;
    aCustomButtons[1].nButtonID = 101;
    aCustomButtons[1].pszButtonText = rDontSave;
    tdc.pButtons = aCustomButtons;
    tdc.cButtons = _countof(aCustomButtons);
    assert(tdc.cButtons <= _countof(aCustomButtons));
    tdc.nDefaultButton = 100;

    tdc.hwndParent = *this;
    tdc.hInstance = hRes;
    tdc.dwCommonButtons = TDCBF_CANCEL_BUTTON;
    tdc.pszWindowTitle = MAKEINTRESOURCE(IDS_APP_TITLE);
    tdc.pszMainIcon = TD_INFORMATION_ICON;
    tdc.pszMainInstruction = rTitle;
    tdc.pszContent = sQuestion.c_str();
    if (docloseall)
        tdc.pszVerificationText = MAKEINTRESOURCE(IDS_DOITFORALLFILES);
    int nClickedBtn = 0;
    HRESULT hr = TaskDialogIndirect(&tdc, &nClickedBtn, nullptr, &closealldoall);
    if (CAppUtils::FailedShowMessage(hr))
        nClickedBtn = 0;
    ResponseToCloseTab response = ResponseToCloseTab::Cancel;
    switch (nClickedBtn)
    {
    case 100:
        response = ResponseToCloseTab::SaveAndClose;
        break;
    case 101:
        response = ResponseToCloseTab::CloseWithoutSaving;
        break;
    }
    return response;
}

ResponseToOutsideModifiedFile CMainWindow::AskToReloadOutsideModifiedFile(const CDocument& doc) const
{
    if (!responsetooutsidemodifiedfiledoall)
    {
        bool changed = doc.m_bNeedsSaving || doc.m_bIsDirty;
        if (!changed && CIniSettings::Instance().GetInt64(L"View", L"autorefreshifnotmodified", 1))
            return ResponseToOutsideModifiedFile::Reload;
        ResString rTitle(hRes, IDS_OUTSIDEMODIFICATIONS);
        ResString rQuestion(hRes, changed ? IDS_DOYOUWANTRELOADBUTDIRTY : IDS_DOYOUWANTTORELOAD);
        ResString rSave(hRes, IDS_SAVELOSTOUTSIDEMODS);
        ResString rReload(hRes, changed ? IDS_RELOADLOSTMODS : IDS_RELOAD);
        ResString rCancel(hRes, IDS_NORELOAD);
        // Be specific about what they are re-loading.
        std::wstring sQuestion = CStringUtils::Format(rQuestion, doc.m_path.c_str());

        TASKDIALOGCONFIG tdc = { sizeof(TASKDIALOGCONFIG) };
        TASKDIALOG_BUTTON aCustomButtons[3];
        int bi = 0;
        aCustomButtons[bi].nButtonID = 101;
        aCustomButtons[bi++].pszButtonText = rReload;
        if (changed)
        {
            aCustomButtons[bi].nButtonID = 102;
            aCustomButtons[bi++].pszButtonText = rSave;
        }
        aCustomButtons[bi].nButtonID = 100;
        aCustomButtons[bi++].pszButtonText = rCancel;
        tdc.pButtons = aCustomButtons;
        tdc.cButtons = bi;
        assert(tdc.cButtons <= _countof(aCustomButtons));
        tdc.nDefaultButton = 100;  // default to "Cancel" in case the file is modified

        tdc.hwndParent = *this;
        tdc.hInstance = hRes;
        tdc.dwFlags = TDF_USE_COMMAND_LINKS | TDF_POSITION_RELATIVE_TO_WINDOW | TDF_SIZE_TO_CONTENT | TDF_ALLOW_DIALOG_CANCELLATION;
        tdc.pszWindowTitle = MAKEINTRESOURCE(IDS_APP_TITLE);
        tdc.pszMainIcon = changed ? TD_WARNING_ICON : TD_INFORMATION_ICON;
        tdc.pszMainInstruction = rTitle;
        tdc.pszContent = sQuestion.c_str();
        tdc.pszVerificationText = MAKEINTRESOURCE(IDS_DOITFORALLFILES);
        int nClickedBtn = 0;
        HRESULT hr = TaskDialogIndirect(&tdc, &nClickedBtn, nullptr, &responsetooutsidemodifiedfiledoall);
        if (CAppUtils::FailedShowMessage(hr))
            nClickedBtn = 0;
        responsetooutsidemodifiedfile = ResponseToOutsideModifiedFile::Cancel;
        switch (nClickedBtn)
        {
            case 101:
                responsetooutsidemodifiedfile = ResponseToOutsideModifiedFile::Reload;
                break;
            case 102:
                responsetooutsidemodifiedfile = ResponseToOutsideModifiedFile::KeepOurChanges;
                break;
        }
    }
    return responsetooutsidemodifiedfile;
}

bool CMainWindow::AskToReload(const CDocument& doc) const
{
    ResString rTitle(hRes, IDS_HASMODIFICATIONS);
    ResString rQuestion(hRes, IDS_RELOADREALLY);
    ResString rReload(hRes, IDS_DORELOAD);
    ResString rCancel(hRes, IDS_DONTRELOAD);
    // Be specific about what they are re-loading.
    std::wstring sQuestion = CStringUtils::Format(rQuestion, doc.m_path.c_str());

    TASKDIALOGCONFIG tdc = { sizeof(TASKDIALOGCONFIG) };
    TASKDIALOG_BUTTON aCustomButtons[2];
    aCustomButtons[0].nButtonID = 101;
    aCustomButtons[0].pszButtonText = rReload;
    aCustomButtons[1].nButtonID = 100;
    aCustomButtons[1].pszButtonText = rCancel;
    tdc.pButtons = aCustomButtons;
    tdc.cButtons = _countof(aCustomButtons);
    assert(tdc.cButtons <= _countof(aCustomButtons));
    tdc.nDefaultButton = 100; // Default to cancel

    tdc.hwndParent = *this;
    tdc.hInstance = hRes;
    tdc.dwFlags = TDF_USE_COMMAND_LINKS | TDF_ENABLE_HYPERLINKS | TDF_POSITION_RELATIVE_TO_WINDOW | TDF_SIZE_TO_CONTENT | TDF_ALLOW_DIALOG_CANCELLATION;
    tdc.pszWindowTitle = MAKEINTRESOURCE(IDS_APP_TITLE);
    tdc.pszMainIcon = TD_WARNING_ICON; // Warn because we are going to throw away the document.
    tdc.pszMainInstruction = rTitle;
    tdc.pszContent = sQuestion.c_str();
    int nClickedBtn = 0;
    HRESULT hr = TaskDialogIndirect( &tdc, &nClickedBtn, nullptr, nullptr );
    if (CAppUtils::FailedShowMessage(hr))
        nClickedBtn = 0;
    bool reload = (nClickedBtn == 101);
    return reload;
}

bool CMainWindow::AskAboutOutsideDeletedFile(const CDocument& doc) const
{
    ResString rTitle(hRes, IDS_OUTSIDEREMOVEDHEAD);
    ResString rQuestion(hRes, IDS_OUTSIDEREMOVED);
    ResString rKeep(hRes, IDS_OUTSIDEREMOVEDKEEP);
    ResString rClose(hRes, IDS_OUTSIDEREMOVEDCLOSE);
    // Be specific about what they are removing.
    std::wstring sQuestion = CStringUtils::Format(rQuestion, doc.m_path.c_str());

    TASKDIALOGCONFIG tdc = { sizeof(TASKDIALOGCONFIG) };
    TASKDIALOG_BUTTON aCustomButtons[2];
    int bi = 0;
    aCustomButtons[bi].nButtonID = 100;
    aCustomButtons[bi++].pszButtonText = rKeep;
    aCustomButtons[bi].nButtonID = 101;
    aCustomButtons[bi].pszButtonText = rClose;
    tdc.pButtons = aCustomButtons;
    tdc.cButtons = _countof(aCustomButtons);
    tdc.nDefaultButton = 100;

    tdc.hwndParent = *this;
    tdc.hInstance = hRes;
    tdc.dwFlags = TDF_USE_COMMAND_LINKS | TDF_POSITION_RELATIVE_TO_WINDOW | TDF_SIZE_TO_CONTENT | TDF_ALLOW_DIALOG_CANCELLATION;
    tdc.pszWindowTitle = MAKEINTRESOURCE(IDS_APP_TITLE);
    tdc.pszMainIcon = TD_INFORMATION_ICON;
    tdc.pszMainInstruction = rTitle;
    tdc.pszContent = sQuestion.c_str();
    int nClickedBtn = 0;
    HRESULT hr = TaskDialogIndirect( &tdc, &nClickedBtn, nullptr, nullptr );
    if (CAppUtils::FailedShowMessage(hr))
        nClickedBtn = 0;
    bool bKeep = true;
    if (nClickedBtn == 101)
        bKeep = false;
    return bKeep;
}

bool CMainWindow::AskToRemoveReadOnlyAttribute() const
{
    ResString rTitle(hRes, IDS_FILEISREADONLY);
    ResString rQuestion(hRes, IDS_FILEMAKEWRITABLEASK);
    auto rEditFile = LoadResourceWString(hRes, IDS_EDITFILE);
    auto rCancel = LoadResourceWString(hRes, IDS_CANCEL);
    // We remove the short cut accelerators from these buttons because this
    // dialog pops up automatically and it's too easy to be typing into the editor
    // when that happes and accidentally acknowledge a button.
    SearchRemoveAll(rEditFile, L"&");
    SearchRemoveAll(rCancel, L"&");

    TASKDIALOGCONFIG tdc = { sizeof(TASKDIALOGCONFIG) };
    TASKDIALOG_BUTTON aCustomButtons[2];
    aCustomButtons[0].nButtonID = 101;
    aCustomButtons[0].pszButtonText = rEditFile.c_str();
    aCustomButtons[1].nButtonID = 100;
    aCustomButtons[1].pszButtonText = rCancel.c_str();
    tdc.pButtons = aCustomButtons;
    tdc.cButtons = _countof(aCustomButtons);
    assert(tdc.cButtons <= _countof(aCustomButtons));
    tdc.nDefaultButton = 100; // Default to cancel

    tdc.hwndParent = *this;
    tdc.hInstance = hRes;
    tdc.dwFlags = TDF_POSITION_RELATIVE_TO_WINDOW | TDF_SIZE_TO_CONTENT | TDF_ALLOW_DIALOG_CANCELLATION;
    tdc.pszWindowTitle = MAKEINTRESOURCE(IDS_APP_TITLE);
    tdc.pszMainIcon = TD_WARNING_ICON;
    tdc.pszMainInstruction = rTitle;
    tdc.pszContent = rQuestion;
    int nClickedBtn = 0;
    HRESULT hr = TaskDialogIndirect(&tdc, &nClickedBtn, nullptr, nullptr);
    if (CAppUtils::FailedShowMessage(hr))
        nClickedBtn = 0;
    bool bRemoveReadOnlyAttribute = (nClickedBtn == 101);
    return bRemoveReadOnlyAttribute;
}

// Returns true if file exists or was created.
bool CMainWindow::AskToCreateNonExistingFile(const std::wstring& path) const
{
    ResString rTitle(hRes, IDS_FILE_DOESNT_EXIST);
    ResString rQuestion(hRes, IDS_FILE_ASK_TO_CREATE);
    ResString rCreate(hRes, IDS_FILE_CREATE);
    ResString rCancel(hRes, IDS_FILE_CREATE_CANCEL);
    // Show exactly what we are creating.
    std::wstring sQuestion = CStringUtils::Format(rQuestion, path.c_str());

    TASKDIALOGCONFIG tdc = { sizeof(TASKDIALOGCONFIG) };
    TASKDIALOG_BUTTON aCustomButtons[2];
    int bi = 0;
    aCustomButtons[bi].nButtonID = 101;
    aCustomButtons[bi++].pszButtonText = rCreate;
    aCustomButtons[bi].nButtonID = 100;
    aCustomButtons[bi++].pszButtonText = rCancel;
    tdc.pButtons = aCustomButtons;
    tdc.cButtons = bi;
    assert(tdc.cButtons <= _countof(aCustomButtons));
    tdc.nDefaultButton = 101;

    tdc.hwndParent = *this;
    tdc.hInstance = hRes;
    tdc.dwFlags = TDF_USE_COMMAND_LINKS | TDF_POSITION_RELATIVE_TO_WINDOW | TDF_SIZE_TO_CONTENT | TDF_ALLOW_DIALOG_CANCELLATION;
    tdc.pszWindowTitle = MAKEINTRESOURCE(IDS_APP_TITLE);
    tdc.pszMainIcon = TD_INFORMATION_ICON;
    tdc.pszMainInstruction = rTitle;
    tdc.pszContent = sQuestion.c_str();
    int nClickedBtn = 0;
    HRESULT hr = TaskDialogIndirect(&tdc, &nClickedBtn, nullptr, nullptr);
    if (CAppUtils::FailedShowMessage(hr))
        nClickedBtn = 0;
    bool bCreate = (nClickedBtn == 101);
    return bCreate;
}

void CMainWindow::CopyCurDocPathToClipboard() const
{
    int id = m_TabBar.GetCurrentTabId();
    if (m_DocManager.HasDocumentID(id))
    {
        CDocument doc = m_DocManager.GetDocumentFromID(id);
        WriteAsciiStringToClipboard(doc.m_path.c_str(), *this);
    }
}

void CMainWindow::CopyCurDocNameToClipboard() const
{
    int id = m_TabBar.GetCurrentTabId();
    if (m_DocManager.HasDocumentID(id))
    {
        CDocument doc = m_DocManager.GetDocumentFromID(id);
        WriteAsciiStringToClipboard(CPathUtils::GetFileName(doc.m_path).c_str(), *this);
    }
}

void CMainWindow::CopyCurDocDirToClipboard() const
{
    int id = m_TabBar.GetCurrentTabId();
    if (m_DocManager.HasDocumentID(id))
    {
        CDocument doc = m_DocManager.GetDocumentFromID(id);
        WriteAsciiStringToClipboard(CPathUtils::GetParentDirectory(doc.m_path).c_str(), *this);
    }
}

void CMainWindow::ShowCurDocInExplorer() const
{
    int id = m_TabBar.GetCurrentTabId();
    if (m_DocManager.HasDocumentID(id))
    {
        CDocument doc = m_DocManager.GetDocumentFromID(id);
        PCIDLIST_ABSOLUTE __unaligned pidl = ILCreateFromPath(doc.m_path.c_str());
        if (pidl)
        {
            SHOpenFolderAndSelectItems(pidl,0,0,0);
            CoTaskMemFree((LPVOID)pidl);
        }
    }
}

void CMainWindow::ShowCurDocExplorerProperties() const
{
    int id = m_TabBar.GetCurrentTabId();
    if (m_DocManager.HasDocumentID(id))
    {
        // This creates an ugly exception dialog on my machine in debug mode
        // but it seems to work anyway.
        CDocument doc = m_DocManager.GetDocumentFromID(id);
        SHELLEXECUTEINFO info = {sizeof(SHELLEXECUTEINFO)};
        info.lpVerb = L"properties";
        info.lpFile = doc.m_path.c_str();
        info.nShow = SW_NORMAL;
        info.fMask = SEE_MASK_INVOKEIDLIST;
        info.hwnd = *this;
        ShellExecuteEx(&info);
    }
}

void CMainWindow::HandleClipboardUpdate()
{
    std::wstring s;
    if (IsClipboardFormatAvailable(CF_UNICODETEXT))
    {
        CClipboardHelper clipboard;
        if (clipboard.Open(*this))
        {
            HANDLE hData = GetClipboardData(CF_UNICODETEXT);
            if (hData)
            {
                LPCWSTR lptstr = (LPCWSTR)GlobalLock(hData);
                OnOutOfScope(
                    GlobalUnlock(hData);
                );
                if (lptstr != nullptr)
                    s = lptstr;
            }
        }
    }
    if (!s.empty())
    {
        std::wstring sTrimmed = s;
        CStringUtils::trim(sTrimmed);
        if (!sTrimmed.empty())
        {
            for (auto it = m_ClipboardHistory.cbegin(); it != m_ClipboardHistory.cend(); ++it)
            {
                if (it->compare(s) == 0)
                {
                    m_ClipboardHistory.erase(it);
                    break;
                }
            }
            m_ClipboardHistory.push_front(std::move(s));
        }
    }

    size_t maxsize = (size_t)CIniSettings::Instance().GetInt64(L"clipboard", L"maxhistory", 20);
    if (m_ClipboardHistory.size() > maxsize)
        m_ClipboardHistory.pop_back();
}

void CMainWindow::PasteHistory()
{
    if (!m_ClipboardHistory.empty())
    {
        // create a context menu with all the items in the clipboard history
        HMENU hMenu = CreatePopupMenu();
        if (hMenu)
        {
            OnOutOfScope(
                DestroyMenu(hMenu);
            );
            size_t pos = m_editor.Call(SCI_GETCURRENTPOS);
            POINT pt;
            pt.x = (LONG)m_editor.Call(SCI_POINTXFROMPOSITION, 0, pos);
            pt.y = (LONG)m_editor.Call(SCI_POINTYFROMPOSITION, 0, pos);
            ClientToScreen(m_editor, &pt);
            int index = 1;
            size_t maxsize = (size_t)CIniSettings::Instance().GetInt64(L"clipboard", L"maxuilength", 40);
            for (const auto& s : m_ClipboardHistory)
            {
                std::wstring sf = s;
                SearchReplace(sf, L"\t", L" ");
                SearchReplace(sf, L"\n", L" ");
                SearchReplace(sf, L"\r", L" ");
                CStringUtils::trim(sf);
                // remove unnecessary whitespace inside the string
                std::wstring::iterator new_end = std::unique(sf.begin(), sf.end(), [] (wchar_t lhs, wchar_t rhs) -> bool
                {
                    return (lhs == ' ' && rhs == ' ');
                });
                sf.erase(new_end, sf.end());

                AppendMenu(hMenu, MF_ENABLED, index, sf.substr(0, maxsize).c_str());
                ++index;
            }
            int selIndex = TrackPopupMenu(hMenu, TPM_LEFTALIGN|TPM_RETURNCMD, pt.x, pt.y, 0, m_editor, nullptr);
            if (selIndex > 0)
            {
                index = 1;
                for (const auto& s : m_ClipboardHistory)
                {
                    if (index == selIndex)
                    {
                        WriteAsciiStringToClipboard(s.c_str(), *this);
                        m_editor.Call(SCI_PASTE);
                        break;
                    }
                    ++index;
                }
            }
        }
    }
}

// Show Tool Tips and colors for colors and numbers and their
// conversions to hex octal etc.
// e.g. 0xF0F hex == 3855 decimal == 07417 octal.
void CMainWindow::HandleDwellStart(const Scintilla::SCNotification& scn)
{
    // Note style will be zero if no style or past end of the document.
    if ((scn.position >= 0) && m_editor.Call(SCI_INDICATORVALUEAT, INDIC_URLHOTSPOT, scn.position))
    {
        // an url hotspot
        ResString str(hRes, IDS_HOWTOOPENURL);
        std::string strA = CUnicodeUtils::StdGetUTF8(str);
        m_editor.Call(SCI_CALLTIPSHOW, scn.position, (sptr_t)strA.c_str());
        return;
    }
    m_editor.Call(SCI_SETWORDCHARS, 0, (LPARAM)"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_.,#");
    std::string sWord = m_editor.GetTextRange(static_cast<long>(m_editor.Call(SCI_WORDSTARTPOSITION, scn.position, false)), static_cast<long>(m_editor.Call(SCI_WORDENDPOSITION, scn.position, false)));

    m_editor.Call(SCI_SETCHARSDEFAULT);
    if (sWord.empty())
        return;

    // TODO: Extract parsing to utility/functions for clarity.
    // Short form or long form html color e.g. #F0F or #FF00FF
    if (sWord[0] == '#' && (sWord.size() == 4 || sWord.size() == 7))
    {
        bool ok = false;
        COLORREF color = 0;

        // Note: could use std::stoi family of functions but they throw
        // range exceptions etc. and VC reports those in the debugger output
        // window. That's distracting and gives the impression something
        // is drastically wrong when it isn't, so we don't use those.

        std::string strNum = sWord.substr(1); // Drop #
        if (strNum.size() == 3) // short form .e.g. F0F
        {
            ok = CAppUtils::ShortHexStringToCOLORREF(strNum, &color);
        }
        else if (strNum.size() == 6) // normal/long form, e.g. FF00FF
        {
            ok = CAppUtils::HexStringToCOLORREF(strNum, &color);
        }
        if (ok)
        {
            std::string sCallTip = CStringUtils::Format(
                "RGB(%d,%d,%d)\nHex: #%06lX\n####################\n####################\n####################",
                GetRValue(color), GetGValue(color), GetBValue(color), (DWORD) color);
            m_editor.Call(SCI_CALLTIPSETFOREHLT, color);
            m_editor.Call(SCI_CALLTIPSHOW, scn.position, (sptr_t)sCallTip.c_str());
            size_t pos = sCallTip.find_first_of('\n');
            pos = sCallTip.find_first_of('\n', pos+1);
            m_editor.Call(SCI_CALLTIPSETHLT, pos, pos+63); // FIXME! What is 63?
        }
    }
    else
    {
        // Assume a number format determined by the string itself
        // and as recognized as a valid format according to strtoll:
        // e.g. 0xF0F hex == 3855 decimal == 07417 octal.
        // Use long long to yield more conversion range than say just long.        
        char* ep = nullptr;
        // 0 base means determine base from any format in the string.
        errno = 0;
        long long number = strtoll(sWord.c_str(), &ep, 0);
        // Be forgiving if given 100xyz, show 100, but
        // don't accept xyz100, show nothing.
        // BTW: errno seems to be 0 even if nothing is converted.
        // Must convert some digits of string.
        if (errno == 0 && ep != &sWord[0])
        {
            std::string bs = to_bit_string(number,true);
            std::string sCallTip = CStringUtils::Format("Dec: %lld - Hex: %#llX - Oct: %#llo\nBin: %s (%d digits)",
                number, number, number, bs.c_str(), (int)bs.size());
            m_editor.Call(SCI_CALLTIPSHOW, scn.position, (sptr_t)sCallTip.c_str());
        }
    }
}

void CMainWindow::HandleGetDispInfo(int tab, LPNMTTDISPINFO lpnmtdi)
{
    POINT p;
    ::GetCursorPos(&p);
    ::ScreenToClient(*this, &p);
    HWND hWin = ::RealChildWindowFromPoint(*this, p);
    if (hWin == m_TabBar)
    {
        int docId = m_TabBar.GetIDFromIndex(tab);
        if (docId >= 0)
        {
            if (m_DocManager.HasDocumentID(docId))
            {
                CDocument doc = m_DocManager.GetDocumentFromID(docId);
                m_tooltipbuffer = std::make_unique<wchar_t[]>(doc.m_path.size()+1);
                wcscpy_s(m_tooltipbuffer.get(), doc.m_path.size()+1, doc.m_path.c_str());
                lpnmtdi->lpszText = m_tooltipbuffer.get();
                lpnmtdi->hinst = nullptr;
            }
        }
    }
}

bool CMainWindow::HandleDoubleClick(const Scintilla::SCNotification& scn)
{
   if (!(scn.modifiers & SCMOD_CTRL))
       return false;

    m_editor.Call(SCI_SETWORDCHARS, 0, (LPARAM)"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-+.,:;?&@=/%#()");

    long pos = scn.position;
    long startPos = static_cast<long>(m_editor.Call(SCI_WORDSTARTPOSITION, pos, false));
    long endPos = static_cast<long>(m_editor.Call(SCI_WORDENDPOSITION, pos, false));

    m_editor.Call(SCI_SETTARGETSTART, startPos);
    m_editor.Call(SCI_SETTARGETEND, endPos);

    long posFound = (long)m_editor.Call(SCI_SEARCHINTARGET, URL_REG_EXPR_LENGTH, (LPARAM)URL_REG_EXPR);
    if (posFound != -1)
    {
        startPos = int(m_editor.Call(SCI_GETTARGETSTART));
        endPos = int(m_editor.Call(SCI_GETTARGETEND));
    }

    std::string urltext = m_editor.GetTextRange(startPos, endPos);
    // This treatment would fail on some valid URLs where there's actually supposed to be a comma or parenthesis at the end.
    CStringUtils::TrimLeadingAndTrailing(urltext, std::initializer_list<char>{ ',', ')', '(' });
    std::wstring url = CUnicodeUtils::StdGetUnicode(urltext);
    SearchReplace(url, L"&amp;", L"&");

    ::ShellExecute(*this, L"open", url.c_str(), nullptr, nullptr, SW_SHOW);
    m_editor.Call(SCI_SETCHARSDEFAULT);

    return true;
}

void CMainWindow::HandleSavePoint(const Scintilla::SCNotification& scn)
{
    int docID = m_TabBar.GetCurrentTabId();
    if (m_DocManager.HasDocumentID(docID))
    {
        CDocument doc = m_DocManager.GetDocumentFromID(docID);
        doc.m_bIsDirty = scn.nmhdr.code == SCN_SAVEPOINTLEFT;
        m_DocManager.SetDocument(docID, doc);
        UpdateTab(docID);
    }
}

void CMainWindow::HandleWriteProtectedEdit()
{
    // user tried to edit a readonly file: ask whether
    // to make the file writeable
    int docID = m_TabBar.GetCurrentTabId();
    if (m_DocManager.HasDocumentID(docID))
    {
        CDocument doc = m_DocManager.GetDocumentFromID(docID);
        if (doc.m_bIsReadonly || (m_editor.Call(SCI_GETREADONLY) != 0))
        {
             // If the user really wants to edit despite it being read only, let them.
            if (AskToRemoveReadOnlyAttribute())
            {
                doc.m_bIsReadonly = false;
                m_DocManager.SetDocument(docID, doc);
                UpdateTab(docID);
                m_editor.Call(SCI_SETREADONLY, false);
                m_editor.Call(SCI_SETSAVEPOINT);
            }
        }
    }
}

void CMainWindow::AddHotSpots()
{
    long startPos = 0;
    long endPos;

    long firstVisibleLine = (long)m_editor.Call(SCI_GETFIRSTVISIBLELINE);
    startPos = (long)m_editor.Call(SCI_POSITIONFROMLINE, m_editor.Call(SCI_DOCLINEFROMVISIBLE, firstVisibleLine));
    long linesOnScreen = (long)m_editor.Call(SCI_LINESONSCREEN);
    long lineCount = (long)m_editor.Call(SCI_GETLINECOUNT);
    endPos = (long)m_editor.Call(SCI_POSITIONFROMLINE, m_editor.Call(SCI_DOCLINEFROMVISIBLE, firstVisibleLine + min(linesOnScreen, lineCount)));


    // to speed up the search, first search for "://" without using the regex engine
    long fStartPos = startPos;
    long fEndPos = endPos;
    m_editor.Call(SCI_SETSEARCHFLAGS, 0);
    m_editor.Call(SCI_SETTARGETSTART, fStartPos);
    m_editor.Call(SCI_SETTARGETEND, fEndPos);
    LRESULT posFoundColonSlash = m_editor.Call(SCI_SEARCHINTARGET, 3, (LPARAM)"://");
    while (posFoundColonSlash != -1)
    {
        // found a "://"
        long lineFoundcolonSlash = (long)m_editor.Call(SCI_LINEFROMPOSITION, posFoundColonSlash);
        startPos = (long)m_editor.Call(SCI_POSITIONFROMLINE, lineFoundcolonSlash);
        endPos = (long)m_editor.Call(SCI_GETLINEENDPOSITION, lineFoundcolonSlash);
        fStartPos = (long)posFoundColonSlash + 1;

        m_editor.Call(SCI_SETSEARCHFLAGS, SCFIND_REGEXP|SCFIND_POSIX);

        // 20 chars for the url protocol should be enough
        m_editor.Call(SCI_SETTARGETSTART, max(startPos, posFoundColonSlash-20));
        // urls longer than 2048 are not handled by browsers
        m_editor.Call(SCI_SETTARGETEND, min(endPos, posFoundColonSlash + 2048));

        LRESULT posFound = m_editor.Call(SCI_SEARCHINTARGET, URL_REG_EXPR_LENGTH, (LPARAM)URL_REG_EXPR);

        if (posFound != -1)
        {
            long start = long(m_editor.Call(SCI_GETTARGETSTART));
            long end = long(m_editor.Call(SCI_GETTARGETEND));
            long foundTextLen = end - start;


            // reset indicators
            m_editor.Call(SCI_SETINDICATORCURRENT, INDIC_URLHOTSPOT);
            m_editor.Call(SCI_INDICATORCLEARRANGE, start, foundTextLen);
            m_editor.Call(SCI_INDICATORCLEARRANGE, start, foundTextLen - 1);

            m_editor.Call(SCI_INDICATORFILLRANGE, start, foundTextLen);
        }
        m_editor.Call(SCI_SETTARGETSTART, fStartPos);
        m_editor.Call(SCI_SETTARGETEND, fEndPos);
        m_editor.Call(SCI_SETSEARCHFLAGS, 0);
        posFoundColonSlash = (int)m_editor.Call(SCI_SEARCHINTARGET, 3, (LPARAM)"://");
    }
}

void CMainWindow::HandleUpdateUI(const Scintilla::SCNotification& scn)
{
    const unsigned int uiflags = SC_UPDATE_SELECTION |
        SC_UPDATE_H_SCROLL | SC_UPDATE_V_SCROLL;
    if ((scn.updated & uiflags) != 0)
        m_editor.MarkSelectedWord(false);

    m_editor.MatchBraces(Braces);
    m_editor.MatchTags();
    AddHotSpots();
    UpdateStatusBar(false);
}

void CMainWindow::HandleAutoIndent( const Scintilla::SCNotification& scn )
{
    int eolMode = int(m_editor.Call(SCI_GETEOLMODE));
    size_t curLine = m_editor.Call(SCI_LINEFROMPOSITION, m_editor.Call(SCI_GETCURRENTPOS));
    size_t lastLine = curLine - 1;
    int indentAmount = 0;

    if (((eolMode == SC_EOL_CRLF || eolMode == SC_EOL_LF) && scn.ch == '\n') ||
        (eolMode == SC_EOL_CR && scn.ch == '\r'))
    {
        // use the same indentation as the last line
        while (lastLine > 0 && (m_editor.Call(SCI_GETLINEENDPOSITION, lastLine) - m_editor.Call(SCI_POSITIONFROMLINE, lastLine)) == 0)
            lastLine--;

        indentAmount = (int)m_editor.Call(SCI_GETLINEINDENTATION, lastLine);

        if (indentAmount > 0)
        {
            Scintilla::CharacterRange crange;
            crange.cpMin = long(m_editor.Call(SCI_GETSELECTIONSTART));
            crange.cpMax = long(m_editor.Call(SCI_GETSELECTIONEND));
            int posBefore = (int)m_editor.Call(SCI_GETLINEINDENTPOSITION, curLine);
            m_editor.Call(SCI_SETLINEINDENTATION, curLine, indentAmount);
            int posAfter = (int)m_editor.Call(SCI_GETLINEINDENTPOSITION, curLine);
            int posDifference = posAfter - posBefore;
            if (posAfter > posBefore)
            {
                // Move selection on
                if (crange.cpMin >= posBefore)
                    crange.cpMin += posDifference;
                if (crange.cpMax >= posBefore)
                    crange.cpMax += posDifference;
            }
            else if (posAfter < posBefore)
            {
                // Move selection back
                if (crange.cpMin >= posAfter)
                {
                    if (crange.cpMin >= posBefore)
                        crange.cpMin += posDifference;
                    else
                        crange.cpMin = posAfter;
                }
                if (crange.cpMax >= posAfter)
                {
                    if (crange.cpMax >= posBefore)
                        crange.cpMax += posDifference;
                    else
                        crange.cpMax = posAfter;
                }
            }
            m_editor.Call(SCI_SETSEL, crange.cpMin, crange.cpMax);
        }
    }
}

void CMainWindow::OpenNewTab()
{
    CDocument doc;
    doc.m_document = m_editor.Call(SCI_CREATEDOCUMENT);
    doc.m_bHasBOM = CIniSettings::Instance().GetInt64(L"Defaults", L"encodingnewbom", 0) != 0;
    doc.m_encoding = (UINT)CIniSettings::Instance().GetInt64(L"Defaults", L"encodingnew", GetACP());
    doc.m_language = L"Text";
    std::wstring tabName = GetNewTabName();
    int index = -1;
    if (m_insertionIndex >= 0)
        index = m_TabBar.InsertAfter(m_insertionIndex, tabName.c_str());
    else
        index = m_TabBar.InsertAtEnd(tabName.c_str());
    int docID = m_TabBar.GetIDFromIndex(index);
    m_DocManager.AddDocumentAtEnd(doc, docID);
    m_TabBar.ActivateAt(index);
    m_editor.SetupLexerForLang(L"Text");
    m_editor.GotoLine(0);
    m_insertionIndex = -1;
    CCommandHandler::Instance().OnDocumentOpen(index);
}

void CMainWindow::HandleTabChanging(const NMHDR& /*nmhdr*/)
{
    // document is about to be deactivated
    int docID = m_TabBar.GetCurrentTabId();
    if (m_DocManager.HasDocumentID(docID))
    {
        m_editor.MatchBraces(Clear);
        CDocument doc = m_DocManager.GetDocumentFromID(docID);
        m_editor.SaveCurrentPos(doc.m_position);
        m_DocManager.SetDocument(docID, doc);
    }
}

void CMainWindow::HandleTabChange(const NMHDR& /*nmhdr*/)
{
    int curTab = m_TabBar.GetCurrentTabIndex();
    // document got activated
    int docID = m_TabBar.GetCurrentTabId();
    // Can't do much if no document for this tab.
    if (!m_DocManager.HasDocumentID(docID))
        return;

    CDocument doc = m_DocManager.GetDocumentFromID(docID);
    m_editor.Call(SCI_SETDOCPOINTER, 0, doc.m_document);
    m_editor.SetupLexerForLang(doc.m_language);
    m_editor.RestoreCurrentPos(doc.m_position);
    m_editor.SetTabSettings();
    // REVIEW: SetupLexerForLang above calls SetDefaultStyles which calls Call(SCI_SETCODEPAGE, CP_UTF8)
    // and that destroy the bookmark locations so call mark bookarks first.
    CEditorConfigHandler::Instance().ApplySettingsForPath(doc.m_path, &m_editor, doc);
    m_DocManager.SetDocument(docID, doc);
    m_editor.MarkSelectedWord(true);
    m_editor.MarkBookmarksInScrollbar();
    UpdateCaptionBar();
    SetFocus(m_editor);
    m_editor.Call(SCI_GRABFOCUS);
    UpdateStatusBar(true);
    auto ds = m_DocManager.HasFileChanged(docID);
    if (ds == DM_Modified)
    {
        ReloadTab(curTab,-1,true);
    }
    else if (ds == DM_Removed)
    {
        HandleOutsideDeletedFile(curTab);
    }
}

void CMainWindow::HandleTabDelete(const TBHDR& tbhdr)
{
    int tabToDelete = tbhdr.tabOrigin;
    // Close tab will take care of activating any next tab.
    CloseTab(tabToDelete);
}

// Note A: (relates to 'Note A' in the function below):
// We've loaded a new document, now we'd like to activate it.
// ActivateAt will cause a TCN_CHANGING and then a TCN_CHANGE
// event to occur. The change will save the current documents
// cursor position and whatever else it wants to do, then the
// TCN_CHANGE event will fire and that will make the document
// we just loaded current.
// (note commands react (and need to) to both of these events
// so being aware of their expectations is important and tricky.

// If we call m_editor.Call(SCI_SETDOCPOINTER, 0, doc.m_document)
// before activate, we will in effect make the just loaded document
// current and the TCN_CHANGING event will act on the new document
// instead of the current document (thereby saving the new
// documents cursor position as if it were the current document's,
// etc. thereby losing the cursor position and other badness;
// the TCN_CHANGE event will then install the new document as
// active for the second time, possibly creating excess references.

// Letting ActivateAt manage the setting of the new document,
// at least for now but probably for ever, avoids these issues.
// Calling m_editor.Call(SCI_SETDOCPOINTER, 0, doc.m_document) ourselves
// before ActivateAt causes the issues.

// However the issue is complicated by the fact that (it seems)
// we may not always be be able to activate the newly loaded document
// (see existing comment a bit further below).
// But if we don't activate the document we violate the callers expectations
// that the newly loaded document will be made active, not just loaded.
// I've added an else path to keep the code the same as before in this case
// so things are no worse than before, but I think this still incorrect.
// Another issue is that the IsWindowEnabled is probably not enough of a
// test either, and possibly isn't applied rigorously enough in any case
// (see above comment at top of function).

// All in all this all indicates some further thought and restructuring is
// required around this area and these issues; and a lot more testing.
// I don't have time to do all that right now so this comment is a
// reminder of the issues until then.
// I suspect if we can't load a document and call ActivateAt to make it
// active, we should return a false / "not now" status, but that's for
// future analysis to confirm.

// The actual change made in this commit at least fixes the cursor problem
// at hand and probably some other issues by implication; but it is limited
// to addressing just that for now and documenting the issues for later.
// I don't want subsequent changes to lose simple change in this commit
// and the reason for it.

// Note B:
// only activate the new doc tab if the main window is enabled:
// if it's disabled, a modal dialog is shown
// (e.g., the handle-outside-modifications confirmation dialog)
// and we then must not change the active tab.

// Note C:
// only activate the new doc tab if the main window is enabled:
// if it's disabled, a modal dialog is shown
// (e.g., the handle-outside-modifications confirmation dialog)
// and we then must not change the active tab.


int CMainWindow::OpenFile(const std::wstring& file, unsigned int openFlags)
{
    int index = -1;
    bool bAddToMRU = (openFlags & OpenFlags::AddToMRU) != 0;
    bool bAskToCreateIfMissing = (openFlags & OpenFlags::AskToCreateIfMissing) != 0;
    bool bCreateIfMissing = (openFlags & OpenFlags::CreateIfMissing) != 0;
    bool bIgnoreIfMissing = (openFlags & OpenFlags::IgnoreIfMissing) != 0;
    bool bOpenIntoActiveTab = (openFlags & OpenFlags::OpenIntoActiveTab) != 0;
    bool bActivate = (openFlags & OpenFlags::NoActivate) == 0;
    bool bCreateTabOnly = (openFlags & OpenFlags::CreateTabOnly) != 0;

    if (bCreateTabOnly)
    {
        auto fileName = CPathUtils::GetFileName(file);
        CDocument doc;
        doc.m_document = m_editor.Call(SCI_CREATEDOCUMENT);
        doc.m_bHasBOM = CIniSettings::Instance().GetInt64(L"Defaults", L"encodingnewbom", 0) != 0;
        doc.m_encoding = (UINT)CIniSettings::Instance().GetInt64(L"Defaults", L"encodingnew", GetACP());
        doc.m_bNeedsSaving = true;
        doc.m_bDoSaveAs = true;
        doc.m_path = file;
        std::wstring lang = CLexStyles::Instance().GetLanguageForPath(fileName);
        if (lang.empty())
            lang = L"Text";
        doc.m_language = lang;
        index = m_TabBar.InsertAtEnd(fileName.c_str());
        int docID = m_TabBar.GetIDFromIndex(index);
        m_DocManager.AddDocumentAtEnd(doc, docID);
        UpdateTab(docID);
        UpdateStatusBar(true);
        m_TabBar.ActivateAt(index);
        m_editor.SetupLexerForLang(lang);
        m_editor.GotoLine(0);
        CCommandHandler::Instance().OnDocumentOpen(index);

        return index;
    }

    // if we're opening the first file, we have to activate it
    // to ensure all the activation stuff is handled for that first
    // file.
    // This is required since the tab bar automatically marks the
    // first item as the active one even if it's not activated
    // manually.
    if (m_TabBar.GetItemCount() == 0)
        bActivate = true;

    int encoding = -1;
    std::wstring filepath = CPathUtils::GetLongPathname(file);
    if (_wcsicmp(CIniSettings::Instance().GetIniPath().c_str(), filepath.c_str()) == 0)
    {
        CIniSettings::Instance().Save();
    }
    int id = m_DocManager.GetIdForPath(filepath.c_str());
    if (id != -1)
    {
        index = m_TabBar.GetIndexFromID(id);
        // document already open.
        if (IsWindowEnabled(*this) && bActivate)
        {
            // See Note C above.
            m_TabBar.ActivateAt(index);
        }
    }
    else
    {
        bool createIfMissing = bCreateIfMissing;

        // Note PathFileExists returns true for existing folders,
        // not just files.
        if (!PathFileExists(file.c_str()))
        {
            if (bAskToCreateIfMissing)
            {
                if (!AskToCreateNonExistingFile(file))
                    return false;
                createIfMissing = true;
            }
            if (bIgnoreIfMissing)
                return false;
        }

        CDocument doc = m_DocManager.LoadFile(*this, filepath, encoding, createIfMissing);
        if (doc.m_document)
        {
            int activetabid = -1;
            if (bOpenIntoActiveTab)
            {
                activetabid = m_TabBar.GetCurrentTabId();
                CDocument activedoc = m_DocManager.GetDocumentFromID(activetabid);
                if (!activedoc.m_bIsDirty && !activedoc.m_bNeedsSaving)
                    m_DocManager.RemoveDocument(activetabid);
                else
                    activetabid = -1;
            }
            if (activetabid < 0)
            {
                // check if the only tab is empty and if it is, remove it
                auto docID = m_TabBar.GetCurrentTabId();
                if (docID >= 0)
                {
                    CDocument existDoc = m_DocManager.GetDocumentFromID(docID);
                    if (existDoc.m_path.empty() && (m_editor.Call(SCI_GETLENGTH) == 0) && (m_editor.Call(SCI_CANUNDO) == 0))
                    {
                        m_insertionIndex = m_TabBar.GetCurrentTabIndex();
                        m_TabBar.DeleteItemAt(m_insertionIndex);
                        if (m_insertionIndex)
                            --m_insertionIndex;
                        // Prefer to remove the document after the tab has gone as it supports it
                        // and deletion causes events that may expect it to be there.
                        m_DocManager.RemoveDocument(docID);
                    }
                }
            }
            if (bAddToMRU)
                CMRU::Instance().AddPath(filepath);
            std::wstring sFileName = CPathUtils::GetFileName(filepath);
            if (activetabid < 0)
            {
                if (m_insertionIndex >= 0)
                    index = m_TabBar.InsertAfter(m_insertionIndex, sFileName.c_str());
                else
                    index = m_TabBar.InsertAtEnd(sFileName.c_str());
                id = m_TabBar.GetIDFromIndex(index);
            }
            else
            {
                index = m_TabBar.GetCurrentTabIndex();
                m_TabBar.SetCurrentTabId(activetabid);
                id = activetabid;
                m_TabBar.SetCurrentTitle(sFileName.c_str());
            }
            doc.m_language = CLexStyles::Instance().GetLanguageForDocument(doc, m_scratchEditor);
            if ((CPathUtils::PathCompare(filepath, m_tabmovepath) == 0) && m_tabmovemod)
            {
                doc.m_path = m_tabmovesavepath;
                m_DocManager.UpdateFileTime(doc, true);
                filepath = m_tabmovesavepath;
                if (m_tabmovemod)
                {
                    doc.m_bIsDirty = true;
                    doc.m_bNeedsSaving = true;
                }
            }
            m_DocManager.AddDocumentAtEnd(doc, id);
            // See Note A above for comments about this point in the code.

            if (IsWindowEnabled(*this))
            {
                // See Note B above for commentsabout this point in the code.
                bool bResize = m_fileTree.GetPath().empty() && !doc.m_path.empty();
                if (bActivate)
                {
                    m_TabBar.ActivateAt(index);
                    m_editor.SetupLexerForLang(doc.m_language);
                }
                // REVIEW: Seems this is neccessary?
                else
                    m_editor.Call(SCI_SETDOCPOINTER, 0, doc.m_document);
                if (bResize)
                    ResizeChildWindows();
            }
            else // See comment above.
                m_editor.Call(SCI_SETDOCPOINTER, 0, doc.m_document);
            if (bAddToMRU)
                SHAddToRecentDocs(SHARD_PATHW, filepath.c_str());
            CCommandHandler::Instance().OnDocumentOpen(index);
            if (m_fileTree.GetPath().empty())
            {
                m_fileTree.SetPath(CPathUtils::GetParentDirectory(filepath));
                ResizeChildWindows();
            }
        }
        else
        {
            CMRU::Instance().RemovePath(filepath, false);
        }
    }
    m_insertionIndex = -1;
    return index;
}

// TODO: consider continuing merging process,
// how to merge with OpenFileAs with OpenFile
bool CMainWindow::OpenFileAs( const std::wstring& temppath, const std::wstring& realpath, bool bModified )
{
    // If we can't open it, not much we can do.
    if (OpenFile(temppath, 0)<0)
    {
        DeleteFile(temppath.c_str());
        return false;
    }

    // Get the id for the document we just loaded,
    // it'll currently have a temporary name.
    int docID = m_DocManager.GetIdForPath(temppath);
    CDocument doc = m_DocManager.GetDocumentFromID(docID);
    doc.m_path = CPathUtils::GetLongPathname(realpath);
    doc.m_bIsDirty = bModified;
    doc.m_bNeedsSaving = bModified;
    m_DocManager.UpdateFileTime(doc, true);
    std::wstring sFileName = CPathUtils::GetFileName(doc.m_path);
    doc.m_language = CLexStyles::Instance().GetLanguageForDocument(doc, m_scratchEditor);
    m_DocManager.SetDocument(docID, doc);
    m_editor.Call(SCI_SETREADONLY, doc.m_bIsReadonly);
    m_editor.SetupLexerForLang(doc.m_language);
    UpdateTab(docID);
    if (sFileName.empty())
        m_TabBar.SetCurrentTitle(GetNewTabName().c_str());
    else
        m_TabBar.SetCurrentTitle(sFileName.c_str());
    UpdateCaptionBar();
    UpdateStatusBar(true);
    DeleteFile(temppath.c_str());

    return true;
}

void CMainWindow::HandleDropFiles(HDROP hDrop)
{
    if (!hDrop)
        return;
    OnOutOfScope(
        DragFinish(hDrop);
    );
    int filesDropped = DragQueryFile(hDrop, 0xffffffff, nullptr, 0);
    std::vector<std::wstring> files;
    for (int i = 0 ; i < filesDropped ; ++i)
    {
        UINT len = DragQueryFile(hDrop, i, nullptr, 0);
        auto pathBuf = std::make_unique<wchar_t[]>(len+2);
        DragQueryFile(hDrop, i, pathBuf.get(), len+1);
        files.push_back(pathBuf.get());
    }
    FileTreeBlockRefresh(true);
    OnOutOfScope(FileTreeBlockRefresh(false));
    for (const auto& filename : files)
    {
        if (PathIsDirectory(filename.c_str()))
        {
            std::vector<std::wstring> recursefiles;
            CDirFileEnum enumerator(filename);
            bool bIsDir = false;
            std::wstring path;
            while (enumerator.NextFile(path, &bIsDir, false))
            {
                if (!bIsDir)
                    recursefiles.push_back(std::move(path));
            }
            if (recursefiles.size() < 100)
            {
                for (const auto& f : recursefiles)
                    OpenFile(f, 0);
            }
        }
        else
            OpenFile(filename, OpenFlags::AddToMRU);
    }
}

void CMainWindow::HandleCopyDataCommandLine(const COPYDATASTRUCT& cds)
{
    CCmdLineParser parser((LPCWSTR)cds.lpData);
    LPCTSTR path = parser.GetVal(L"path");
    if (path)
    {
        if (PathIsDirectory(path))
        {
            m_fileTree.SetPath(path);
            ShowFileTree(true);
        }
        else if (OpenFile(path, OpenFlags::AddToMRU | OpenFlags::AskToCreateIfMissing) >= 0)
        {
            if (parser.HasVal(L"line"))
            {
                GoToLine(parser.GetLongVal(L"line") - 1);
            }
        }
    }
    else
    {
        // find out if there are paths specified without the key/value pair syntax
        int nArgs;

        LPCWSTR* szArglist = (LPCWSTR*) CommandLineToArgvW((LPCWSTR)cds.lpData, &nArgs);
        OnOutOfScope(LocalFree(szArglist););
        if (!szArglist)
            return;

        FileTreeBlockRefresh(true);
        OnOutOfScope(FileTreeBlockRefresh(false));
        int filesOpened = 0;
        for (int i = 1; i < nArgs; i++)
        {
            if (szArglist[i][0] != '/')
            {
                if (PathIsDirectory(szArglist[i]))
                {
                    m_fileTree.SetPath(szArglist[i]);
                    ShowFileTree(true);
                }
                else if (OpenFile(szArglist[i], OpenFlags::AddToMRU | OpenFlags::AskToCreateIfMissing) >= 0)
                    ++filesOpened;
            }
        }

        if ((filesOpened == 1) && parser.HasVal(L"line"))
        {
            GoToLine(parser.GetLongVal(L"line") - 1);
        }
    }
}

// FIXME/TODO:
// If both instances have the same document loaded,
// things get tricky:
//
// Currently, if one instance instructs another instance to
// load a document the first instance already has open,
// the receiver will switch to it's own tab and copy of the document
// and silently throw the senders document away.
//
// This may cause the sender to lose work if their document was modified
// or more recent.
// This is what happens currently but is arguably wrong.
//
// If the receiver refuses to load the document, the sender might not actually
// have it open themselves and was just instructing the reciever to load it;
// if the receiver refuses, nothing will happen and that would be also wrong.
//
// The receiver could load the senders document but the receiver might have
// modified their version, in which case we don't know if it's
// safe to replace it and if we silently do they may lose something.
//
// Even if the receivers document isn't modified, there is no
// guarantee that the senders document is newer.
//
// The design probably needs to change so that the reciever asks
// the user what to do if they try to move a document to another
// when it's open in two places.
//
// The sender might also need to indicate to the receiver if it's ok
// for the receiver to switch to an already loaded instance.
//
// For now, continue to do what we've always have, which is to
// just switch to any existing document of the same name and
// throw the senders copy away if there is a clash.
//
// Will revisit this later.

// Returns true of the tab was moved in, else false.
// Callers using SendMessage can check if the reciever
// loaded the tab they sent before they close their tab
// it was sent from.

// TODO!: Moving a tab to another instance means losing
// undo history.
// Consider warning about that or if the undo history
// could be saved and restored.

// Called when a Tab is dropped over another instance of BowPad.
bool CMainWindow::HandleCopyDataMoveTab(const COPYDATASTRUCT& cds)
{
    std::wstring paths = std::wstring((const wchar_t*)cds.lpData, cds.cbData / sizeof(wchar_t));
    std::vector<std::wstring> datavec;
    stringtok(datavec, paths, false, L"*");
    // Be a little untrusting of the clipboard data and if it's
    // a mistake by the caller let them know so they
    // don't throw away something we can't open.
    if (datavec.size() != 4)
    {
        APPVERIFY(false); // Assume a bug if testing.
        return false;
    }
    std::wstring realpath = datavec[0];
    std::wstring temppath = datavec[1];
    bool bMod = _wtoi(datavec[2].c_str()) != 0;
    int line = _wtoi(datavec[3].c_str());

    // Unsaved files / New tabs have an empty real path.

    if (!realpath.empty()) // If this is a saved file
    {
        int docID = m_DocManager.GetIdForPath(realpath.c_str());
        if (docID != -1) // If it already exists switch to it.
        {
            // TODO: we can lose work here, see notes above.
            // The document we switch to may be the same.
            // better to reject the move and return false or something.

            int tab = m_TabBar.GetIndexFromID(docID);
            m_TabBar.ActivateAt(tab);
            DeleteFile(temppath.c_str());

            return true;
        }
        // If it doesn't exist, fall through to load it.
    }
    bool opened = OpenFileAs(temppath, realpath, bMod);
    if (opened)
        GoToLine(line);
    return opened;
}

static bool AskToCopyOrMoveFile(HWND hWnd, const std::wstring& filename, const std::wstring& hitpath, bool bCopy)
{
    ResString rTitle(hRes, IDS_FILE_DROP);
    ResString rQuestion(hRes, bCopy ? IDS_FILE_DROP_COPY : IDS_FILE_DROP_MOVE);
    ResString rDoit(hRes, bCopy ? IDS_FILE_DROP_DOCOPY : IDS_FILE_DROP_DOMOVE);
    ResString rCancel(hRes, IDS_FILE_DROP_CANCEL);
    // Show exactly what we are creating.
    std::wstring sQuestion = CStringUtils::Format(rQuestion, filename.c_str(), hitpath.c_str());

    TASKDIALOGCONFIG tdc = { sizeof(TASKDIALOGCONFIG) };
    TASKDIALOG_BUTTON aCustomButtons[2];
    int bi = 0;
    aCustomButtons[bi].nButtonID = 101;
    aCustomButtons[bi++].pszButtonText = rDoit;
    aCustomButtons[bi].nButtonID = 100;
    aCustomButtons[bi++].pszButtonText = rCancel;
    tdc.pButtons = aCustomButtons;
    tdc.cButtons = bi;
    assert(tdc.cButtons <= _countof(aCustomButtons));
    tdc.nDefaultButton = 101;

    tdc.hwndParent = hWnd;
    tdc.hInstance = hRes;
    tdc.dwFlags = TDF_USE_COMMAND_LINKS | TDF_POSITION_RELATIVE_TO_WINDOW | TDF_SIZE_TO_CONTENT | TDF_ALLOW_DIALOG_CANCELLATION;
    tdc.pszWindowTitle = MAKEINTRESOURCE(IDS_APP_TITLE);
    tdc.pszMainIcon = TD_INFORMATION_ICON;
    tdc.pszMainInstruction = rTitle;
    tdc.pszContent = sQuestion.c_str();
    int nClickedBtn = 0;
    HRESULT hr = TaskDialogIndirect(&tdc, &nClickedBtn, nullptr, nullptr);
    if (CAppUtils::FailedShowMessage(hr))
        nClickedBtn = 0;
    bool bDoIt = (nClickedBtn == 101);
    return bDoIt;
}

void CMainWindow::HandleTabDroppedOutside(int tab, POINT pt)
{
    // Start a new instance of BowPad with this dropped tab, or add this tab to
    // the BowPad window the drop was done on. Then close this tab.
    // First save the file to a temp location to ensure all unsaved mods are saved.
    std::wstring temppath = CTempFiles::Instance().GetTempFilePath(true);
    auto docID = m_TabBar.GetIDFromIndex(tab);
    CDocument doc = m_DocManager.GetDocumentFromID(docID);
    HWND hDroppedWnd = WindowFromPoint(pt);
    if (hDroppedWnd)
    {
        // If the drop target identifies a specific instance of BowPad found
        // move the tab to that instance OR
        // if not, create a new instance of BowPad and move the tab to that.

        bool isThisInstance;
        HWND hMainWnd = FindAppMainWindow(hDroppedWnd, &isThisInstance);
        // Dropping on our own window shall create an new BowPad instance
        // because the user doesn't want to be moving a tab around within
        // the same instance of BowPad this way.
        if (hMainWnd && ! isThisInstance)
        {
            // dropped on another BowPad Window, 'move' this tab to that BowPad Window
            CDocument tempdoc = doc;
            tempdoc.m_path = temppath;
            bool bDummy = false;
            m_DocManager.SaveFile(*this, tempdoc, bDummy);

            COPYDATASTRUCT cpd = { 0 };
            cpd.dwData = CD_COMMAND_MOVETAB;
            std::wstring cpdata = doc.m_path + L"*" + temppath + L"*";
            cpdata += (doc.m_bIsDirty || doc.m_bNeedsSaving) ? L"1*" : L"0*";
            cpdata += std::to_wstring((long)(m_editor.Call(SCI_LINEFROMPOSITION, m_editor.Call(SCI_GETCURRENTPOS)) + 1));
            cpd.lpData = (PVOID)cpdata.c_str();
            cpd.cbData = DWORD(cpdata.size()*sizeof(wchar_t));

            // It's an important concept that the receiver can reject/be unable
            // to load / handle the file/tab we are trying to move to them.
            // We don't want the user to lose their work if that happens.
            // So only close this tab if the move was successful.
            bool moved = SendMessage(hMainWnd, WM_COPYDATA, (WPARAM)m_hwnd, (LPARAM)&cpd) ? true : false;
            if (moved)
            {
                // remove the tab
                CloseTab(tab, true);
            }
            else
            {
                // TODO.
                // On error, in theory the sender or receiver or both can display an error.
                // Until it's clear what set of errors and handling is required just throw
                // up a simple dialog box this side. In theory the receiver may even crash
                // with no error, so probably some minimal message might be useful.
                ::MessageBox(*this, L"Failed to move Tab.", GetAppName().c_str(), MB_OK | MB_ICONERROR);
            }
            return;
        }
        else if ((hDroppedWnd == m_fileTree) && (!doc.m_path.empty()))
        {
            // drop over file tree
            auto hitpath = m_fileTree.GetDirPathForHitItem();
            if (!hitpath.empty())
            {
                auto filename = CPathUtils::GetFileName(doc.m_path);
                auto destpath = CPathUtils::Append(hitpath, filename);

                if (CPathUtils::PathCompare(doc.m_path, destpath))
                {
                    bool bCopy = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
                    if (AskToCopyOrMoveFile(*this, filename, destpath, bCopy))
                    {
                        if (bCopy)
                        {
                            CDocument tempdoc = doc;
                            tempdoc.m_path = destpath;
                            bool bDummy = false;
                            m_DocManager.SaveFile(*this, tempdoc, bDummy);
                            OpenFile(destpath, OpenFlags::AddToMRU);
                        }
                        else
                        {
                            if (MoveFile(doc.m_path.c_str(), destpath.c_str()))
                            {
                                doc.m_path = destpath;
                                m_DocManager.SetDocument(docID, doc);
                                m_TabBar.ActivateAt(tab);
                            }
                        }
                    }
                    return;
                }
            }
        }
    }
    CDocument tempdoc = doc;
    tempdoc.m_path = temppath;
    bool bDummy = false;
    m_DocManager.SaveFile(*this, tempdoc, bDummy);

    // Start a new instance and open the tab there.
    std::wstring modpath = CPathUtils::GetModulePath();
    std::wstring cmdline = CStringUtils::Format(L"/multiple /tabmove /savepath:\"%s\" /path:\"%s\" /line:%ld",
                                                doc.m_path.c_str(), temppath.c_str(),
                                                (long)(m_editor.Call(SCI_LINEFROMPOSITION, m_editor.Call(SCI_GETCURRENTPOS)) + 1));
    if (doc.m_bIsDirty || doc.m_bNeedsSaving)
        cmdline += L" /modified";
    SHELLEXECUTEINFO shExecInfo = { };
    shExecInfo.cbSize = sizeof(SHELLEXECUTEINFO);

    shExecInfo.hwnd = *this;
    shExecInfo.lpVerb = L"open";
    shExecInfo.lpFile = modpath.c_str();
    shExecInfo.lpParameters = cmdline.c_str();
    shExecInfo.nShow = SW_NORMAL;

    if (ShellExecuteEx(&shExecInfo))
        CloseTab(tab, true);
}

bool CMainWindow::ReloadTab( int tab, int encoding, bool dueToOutsideChanges )
{
    int docID = m_TabBar.GetIDFromIndex(tab);
    if (docID < 0) // No such tab.
        return false;
    if (!m_DocManager.HasDocumentID(docID)) // No such document
        return false;
    bool bReloadCurrentTab = (tab == m_TabBar.GetCurrentTabIndex());
    CDocument doc = m_DocManager.GetDocumentFromID(docID);

    CScintillaWnd* editor = bReloadCurrentTab ? &m_editor : &m_scratchEditor;
    if (dueToOutsideChanges)
    {
        ResponseToOutsideModifiedFile response = AskToReloadOutsideModifiedFile(doc);

        // Responses: Cancel, Save and Reload, Reload

        if (response == ResponseToOutsideModifiedFile::Reload) // Reload
        {
            // Fall through to reload
        }
        // Save if asked to. Assume we won't have asked them to save if
        // the file isn't dirty or it wasn't appropriate to save.
        else if (response == ResponseToOutsideModifiedFile::KeepOurChanges) // Save
        {
            SaveCurrentTab();
        }
        else // Cancel or failed to ask
        {
            // update the filetime of the document to avoid this warning
            m_DocManager.UpdateFileTime(doc, false);
            // the current content of the tab is possibly different
            // than the content on disk: mark the content as dirty
            // so the user knows he can save the changes.
            doc.m_bIsDirty = true;
            doc.m_bNeedsSaving = true;
            m_DocManager.SetDocument(docID, doc);
            // the next to calls are only here to trigger SCN_SAVEPOINTLEFT/SCN_SAVEPOINTREACHED messages
            editor->Call(SCI_ADDUNDOACTION, 0, 0);
            editor->Call(SCI_UNDO);
            return false;
        }
    }
    else if (doc.m_bIsDirty || doc.m_bNeedsSaving)
    {
        if (!AskToReload(doc)) // User doesn't want to reload.
            return false;
    }

    if (!bReloadCurrentTab)
        editor->Call(SCI_SETDOCPOINTER, 0, doc.m_document);

    // LoadFile increases the reference count, so decrease it here first
    editor->Call(SCI_RELEASEDOCUMENT, 0, doc.m_document);
    CDocument docreload = m_DocManager.LoadFile(*this, doc.m_path.c_str(), encoding, false);
    if (!docreload.m_document)
    {
        // since we called SCI_RELEASEDOCUMENT but LoadFile did not properly load,
        // we have to increase the reference count again. Otherwise the document
        // might get completely released.
        editor->Call(SCI_ADDREFDOCUMENT, 0, doc.m_document);
        return false;
    }

    if (bReloadCurrentTab)
    {
        editor->SaveCurrentPos(doc.m_position);
        // Apply the new one.
        editor->Call(SCI_SETDOCPOINTER, 0, docreload.m_document);
    }

    docreload.m_language = doc.m_language;
    docreload.m_position = doc.m_position;
    m_DocManager.SetDocument(docID, docreload);
    editor->SetupLexerForLang(docreload.m_language);
    editor->RestoreCurrentPos(docreload.m_position);
    if (bReloadCurrentTab)
        UpdateStatusBar(true);
    UpdateTab(docID);
    if (bReloadCurrentTab)
        editor->Call(SCI_SETSAVEPOINT);

    // refresh the file tree
    m_fileTree.SetPath(m_fileTree.GetPath());

    return true;
}

// Return true if requested to removed document.
bool CMainWindow::HandleOutsideDeletedFile(int tab)
{
    assert(tab == m_TabBar.GetCurrentTabIndex());
    int docID = m_TabBar.GetIDFromIndex(tab);
    CDocument doc = m_DocManager.GetDocumentFromID(docID);
    // file was removed. Options are:
    // * keep the file open
    // * close the tab
    if (!AskAboutOutsideDeletedFile(doc))
    {
        // User wishes to close the tab.
        CloseTab(tab);
        return true;
    }

    // keep the file: mark the file as modified
    doc.m_bNeedsSaving = true;
    // update the filetime of the document to avoid this warning
    m_DocManager.UpdateFileTime(doc, false);
    m_DocManager.SetDocument(docID, doc);
    // the next to calls are only here to trigger SCN_SAVEPOINTLEFT/SCN_SAVEPOINTREACHED messages
    m_editor.Call(SCI_ADDUNDOACTION, 0, 0);
    m_editor.Call(SCI_UNDO);
    return false;
}

bool CMainWindow::HasOutsideChangesOccurred() const
{
    int tabCount = m_TabBar.GetItemCount();
    for (int i = 0; i < tabCount; ++i)
    {
        auto docID = m_TabBar.GetIDFromIndex(i);
        auto ds = m_DocManager.HasFileChanged(docID);
        if (ds == DM_Modified || ds == DM_Removed)
            return true;
    }
    return false;
}

void CMainWindow::CheckForOutsideChanges()
{
    static bool bInsideCheckForOutsideChanges = false;
    if (bInsideCheckForOutsideChanges)
        return;

    // See if any doc has been changed externally.
    bInsideCheckForOutsideChanges = true;
    OnOutOfScope(bInsideCheckForOutsideChanges = false;);
    responsetooutsidemodifiedfiledoall = FALSE;
    bool bChangedTab = false;
    int activeTab = m_TabBar.GetCurrentTabIndex();
    for (int i = 0; i < m_TabBar.GetItemCount(); ++i)
    {
        int docID = m_TabBar.GetIDFromIndex(i);
        auto ds = m_DocManager.HasFileChanged(docID);
        if (ds == DM_Modified || ds == DM_Removed)
        {
            CDocument doc = m_DocManager.GetDocumentFromID(docID);
            if ((ds != DM_Removed) && !doc.m_bIsDirty && !doc.m_bNeedsSaving && CIniSettings::Instance().GetInt64(L"View", L"autorefreshifnotmodified", 1))
            {
                ReloadTab(i, -1);
            }
            else
            {
                m_TabBar.ActivateAt(i);
                if (i != activeTab)
                    bChangedTab = true;
            }
        }
    }
    responsetooutsidemodifiedfiledoall = FALSE;

    if (bChangedTab)
        m_TabBar.ActivateAt(activeTab);
}

// TODO! Get rid of TabMove, make callers use OpenFileAs

void CMainWindow::TabMove(const std::wstring& path, const std::wstring& savepath, bool bMod, long line)
{
    std::wstring filepath = CPathUtils::GetLongPathname(path);

    int docID = m_DocManager.GetIdForPath(filepath.c_str());
    if (docID < 0)
        return;

    int tab = m_TabBar.GetIndexFromID(docID);
    m_TabBar.ActivateAt(tab);
    CDocument doc = m_DocManager.GetDocumentFromID(docID);
    doc.m_path = CPathUtils::GetLongPathname(savepath);
    doc.m_bIsDirty = bMod;
    doc.m_bNeedsSaving = bMod;
    m_DocManager.UpdateFileTime(doc, true);
    m_editor.Call(SCI_SETREADONLY, doc.m_bIsReadonly);

    std::wstring sFileName = CPathUtils::GetFileName(doc.m_path);
    doc.m_language = CLexStyles::Instance().GetLanguageForDocument(doc, m_scratchEditor);
    m_DocManager.SetDocument(docID, doc);

    m_editor.SetupLexerForLang(doc.m_language);
    if (sFileName.empty())
        m_TabBar.SetCurrentTitle(GetNewTabName().c_str());
    else
        m_TabBar.SetCurrentTitle(sFileName.c_str());

    UpdateTab(docID);
    UpdateCaptionBar();
    UpdateStatusBar(true);

    GoToLine(line);

    m_fileTree.SetPath(CPathUtils::GetParentDirectory(savepath));
    ResizeChildWindows();

    DeleteFile(filepath.c_str());
}

bool CMainWindow::OnMouseMove(UINT nFlags, POINT point)
{
    if (m_bDragging)
    {
        if ((nFlags & MK_LBUTTON) != 0 && (point.x != m_oldPt.x))
        {
            m_treeWidth = point.x;
            m_treeWidth = max(50, m_treeWidth);
            RECT rc;
            GetClientRect(*this, &rc);
            m_treeWidth = min(m_treeWidth, rc.right - rc.left - 200);
            ResizeChildWindows();
        }
    }

    return true;
}

bool CMainWindow::OnLButtonDown(UINT nFlags, POINT point)
{
    UNREFERENCED_PARAMETER(nFlags);

    if (!m_bDragging)
    {
        RECT rctree;
        GetWindowRect(m_fileTree, &rctree);
        MapWindowPoints(NULL, *this, (LPPOINT)&rctree, 2);
        if (point.y < rctree.top)
            return true;
        if (point.y > rctree.bottom)
            return true;
        if (point.x < rctree.right - 20)
            return true;
        if (point.x > rctree.right + 20)
            return true;

        SetCapture(*this);
        m_bDragging = true;
    }

    return true;
}

bool CMainWindow::OnLButtonUp(UINT nFlags, POINT point)
{
    UNREFERENCED_PARAMETER(nFlags);

    if (!m_bDragging)
        return false;

    m_treeWidth = point.x;
    m_treeWidth = max(50, m_treeWidth);
    RECT rc;
    GetClientRect(*this, &rc);
    m_treeWidth = min(m_treeWidth, rc.right - rc.left - 200);

    CIniSettings::Instance().SetInt64(L"View", L"FileTreeWidth", m_treeWidth);

    ReleaseCapture();
    m_bDragging = false;
    
    ResizeChildWindows();

    return true;
}

void CMainWindow::ShowFileTree(bool bShow)
{
    m_fileTreeVisible = bShow;
    ShowWindow(m_fileTree, m_fileTreeVisible ? SW_SHOW : SW_HIDE);
    ResizeChildWindows();
    CIniSettings::Instance().SetInt64(L"View", L"FileTree", m_fileTreeVisible ? 1 : 0);
}

