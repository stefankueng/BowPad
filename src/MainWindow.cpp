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

// TODO: Need to check the encoding logic on reload has been put back
// together properly after merging somewhat duplicate open/reload code paths.

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

#include <memory>
#include <cassert>
#include <type_traits>
#include <future>
#include <Shobjidl.h>

IUIFramework *g_pFramework = nullptr;  // Reference to the Ribbon framework.

const int STATUSBAR_DOC_TYPE      = 0;
const int STATUSBAR_DOC_SIZE      = 1;
const int STATUSBAR_CUR_POS       = 2;
const int STATUSBAR_EOF_FORMAT    = 3;
const int STATUSBAR_UNICODE_TYPE  = 4;
const int STATUSBAR_TYPING_MODE   = 5;
const int STATUSBAR_CAPS          = 6;
const int STATUSBAR_TABS          = 7;

static const char URL_REG_EXPR[] = { "\\b[A-Za-z+]{3,9}://[A-Za-z0-9_\\-+~.:?&@=/%#,;{}()[\\]|*!\\\\]+\\b" };


CMainWindow::CMainWindow(HINSTANCE hInst, const WNDCLASSEX* wcx /* = NULL*/)
    : CWindow(hInst, wcx)
    , m_StatusBar(hInst)
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
    , m_handlingOutsideChanges(false)
    , m_pRibbon(nullptr)
    , m_RibbonHeight(0)
{
    m_hShieldIcon = (HICON)::LoadImage(hResource, MAKEINTRESOURCE(IDI_ELEVATED), IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR);
}

CMainWindow::~CMainWindow(void)
{
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
            {
                hr = pView->QueryInterface(IID_PPV_ARGS(&m_pRibbon));
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
            }
            break;
            // The view has been resized.  For the Ribbon view, the application should
            // call GetHeight to determine the height of the ribbon.
        case UI_VIEWVERB_SIZE:
            {
                if (m_pRibbon)
                {
                    // Call to the framework to determine the desired height of the Ribbon.
                    hr = m_pRibbon->GetHeight(&m_RibbonHeight);
                    if (!CAppUtils::FailedShowMessage((hr)))
                        ResizeChildWindows();
                }
            }
            break;
            // The view was destroyed.
        case UI_VIEWVERB_DESTROY:
            {
                hr = S_OK;
                IStreamPtr pStrm;
                std::wstring ribbonsettingspath = CAppUtils::GetDataPath() + L"\\ribbonsettings";
                hr = SHCreateStreamOnFileEx(ribbonsettingspath.c_str(), STGM_WRITE|STGM_CREATE, FILE_ATTRIBUTE_NORMAL, TRUE, nullptr, &pStrm);
                if (!CAppUtils::FailedShowMessage(hr))
                {
                    LARGE_INTEGER liPos;
                    ULARGE_INTEGER uliSize;

                    liPos.QuadPart = 0;
                    uliSize.QuadPart = 0;

                    pStrm->Seek(liPos, STREAM_SEEK_SET, nullptr);
                    pStrm->SetSize(uliSize);

                    m_pRibbon->SaveSettingsToStream(pStrm);
                }
                m_pRibbon->Release();
                m_pRibbon = nullptr;
            }
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
   ResString sTitle(hRes, IDS_APP_TITLE);
   return (LPCWSTR)sTitle;
}

std::wstring CMainWindow::GetWindowClassName() const
{
    ResString clsResName(hResource, IDC_BOWPAD);
    std::wstring clsName = (LPCWSTR)clsResName + CAppUtils::GetSessionID();
    return clsName;
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
    std::wstring clsName = GetWindowClassName();
    while (hStartWnd)
    {
        wchar_t classname[MAX_PATH];
        GetClassName(hStartWnd, classname, _countof(classname));
        if (clsName.compare(classname) == 0)
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
    if (RegisterWindow(&wcx))
    {
        if (CreateEx(WS_EX_ACCEPTFILES, WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, nullptr))
        {
            UpdateWindow(*this);
            return true;
        }
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
    case WM_DROPFILES:
        {
            HDROP hDrop = reinterpret_cast<HDROP>(wParam);
            if (hDrop)
                HandleDropFiles(hDrop);
        }
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
            default:
                break;
            }

            if ((nmhdr.idFrom == (UINT_PTR)&m_TabBar) ||
                (nmhdr.hwndFrom == m_TabBar))
            {
                TBHDR* ptbhdr = reinterpret_cast<TBHDR*>(lParam);
                const TBHDR& tbhdr = *ptbhdr;
                assert(tbhdr.hdr.code == nmhdr.code);

                CCommandHandler::Instance().TabNotify(ptbhdr);

                switch (nmhdr.code)
                {
                case TCN_GETCOLOR:
                    if ((tbhdr.tabOrigin >= 0) && (tbhdr.tabOrigin < m_DocManager.GetCount()))
                    {
                        int id = m_TabBar.GetIDFromIndex(tbhdr.tabOrigin);
                        if (m_DocManager.HasDocumentID(id))
                            return CTheme::Instance().GetThemeColor(m_DocManager.GetColorForDocument(id));
                    }
                    break;
                case TCN_SELCHANGE:
                    HandleTabChange(tbhdr);
                    break;
                case TCN_SELCHANGING:
                    HandleTabChanging(tbhdr);
                    break;
                case TCN_TABDELETE:
                    HandleTabDelete(tbhdr);
                    break;
                case TCN_TABDROPPEDOUTSIDE:
                    {
                        DWORD pos = GetMessagePos();
                        POINT pt;
                        pt.x = GET_X_LPARAM(pos);
                        pt.y = GET_Y_LPARAM(pos);
                        HandleTabDroppedOutside(ptbhdr->tabOrigin, pt);
                    }
                    break;
                }
            }
            else if ((nmhdr.idFrom == (UINT_PTR)&m_editor) ||
                (nmhdr.hwndFrom == m_editor))
            {
                if (nmhdr.code == NM_COOLSB_CUSTOMDRAW)
                {
                    return m_editor.HandleScrollbarCustomDraw(wParam, (NMCSBCUSTOMDRAW *)lParam);
                }

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
                case SCN_HOTSPOTCLICK:
                    HandleHotSpotClick(scn);
                    break;
                case SCN_DWELLSTART:
                    HandleDwellStart(scn);
                    break;
                case SCN_DWELLEND:
                    m_editor.Call(SCI_CALLTIPCANCEL);
                    break;
                }
            }
        }
        break;

    case WM_MOUSEACTIVATE:
        // OutputDebugStringA("WM_MOUSEACTIVATE\n");
        // If outside changes have been made, then we will later
        // likely popup a dialog to respond to those changes like
        // ask to reload the current document. If this
        // happens, any click will be meaningless or dangerous
        // in the context of the document that's been reloaded.
        // So eat the click in that case.
        // If no outside changes have been made pass it on for
        // business as usual as otherwise we'll fail to
        // recognize clicks, like to tab headers or whatever.

        if (HasOutsideChangesOccurred())
            return MA_ACTIVATEANDEAT;
        else
            return DefWindowProc(hwnd,uMsg,wParam,lParam);
        break;

    case WM_ACTIVATEAPP:
    {
        //bool activating = (wParam != 0);
// Handy for debugging until we're sure problems related here are definitely fixed.
#if 0
        DWORD threadIdOfWindowOwner = reinterpret_cast<DWORD>(lpParam);
        OutputDebugStringA("WM_ACTIVATEAPP ");
        OutputDebugStringA(activating ? "activating\n" : "deactivating\n");
#endif
        // Only restore the window position on activation as that's when we'll
        // be showing any window and needing to use it.
        if (!m_windowRestored)
        {
            CIniSettings::Instance().RestoreWindowPos(L"MainWindow", *this, 0);
            m_windowRestored = true;
        }
    }
        break;
    case WM_ACTIVATE:
    {
// Handy for debugging until we're sure problems related here are definitely fixed.
#if 0
        // Useful debugging details.
        // Note WA_ activation codes are packed in the wParam parameter and not raw.
        WORD activationType = LOWORD(wParam);
        bool minimized = HIWORD(wParam) != 0;
        HWND windowChangingActivation = reinterpret_cast<HWND>(lParam);
        OutputDebugStringA("WM_ACTIVATE ");
        switch (activationType)
        {
        case WA_CLICKACTIVE:
            OutputDebugStringA("WA_CLICKACTIVE ");
            break;
        case WA_ACTIVE:
            OutputDebugStringA("WA_ACTIVE ");
            break;
        case WA_INACTIVE:
            OutputDebugStringA("WA_INACTIVE ");
            break;
        default:
            break;
        }
        OutputDebugStringA(minimized? "minimized\n" : "\n");
#endif
        // Ensure proper focus handling occurs such as
        // making sure WM_SETFOCUS is generated in all situations.
        return DefWindowProc(hwnd,uMsg,wParam,lParam);
    }
        break;
    case WM_SETFOCUS: // lParam HWND that is losing focus.
    {
        //OutputDebugStringA("WM_SETFOCUS\n");
        SetFocus(m_editor);
        m_editor.Call(SCI_SETFOCUS, true);
        // Dialog Box's popup as part of handling outside changes,
        // when they close, focus returns here
        if (!m_handlingOutsideChanges)
        {
            m_handlingOutsideChanges = true;
            //OutputDebugStringA("CheckForOutsideChanges()\n");
            CheckForOutsideChanges();
            m_handlingOutsideChanges = false;
        }
        //OutputDebugStringA("CheckForOutsideChanges() compelete\n");
    }
        break;
    case WM_CLIPBOARDUPDATE:
        HandleClipboardUpdate();
        break;
    case WM_TIMER:
        CCommandHandler::Instance().OnTimer((UINT)wParam);
        break;
    case WM_DESTROY:
        g_pFramework->Destroy();
        PostQuitMessage(0);
        break;
    case WM_CLOSE:
        CCommandHandler::Instance().OnClose();
        if (CloseAllTabs())
        {
            CIniSettings::Instance().SaveWindowPos(L"MainWindow", *this);
            ::DestroyWindow(m_hwnd);
        }
        break;
    default:
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }

    return 0;
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
            ICommand * pCmd = CCommandHandler::Instance().GetCommand(id);
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
            func(m_editor, WM_COPYDATA, MSGFLT_ALLOW, nullptr );
            func(m_TabBar,    WM_COPYDATA, MSGFLT_ALLOW, nullptr );
            func(m_StatusBar, WM_COPYDATA, MSGFLT_ALLOW, nullptr );
        }
        else
        {
            typedef BOOL (WINAPI *MESSAGEFILTERFUNC)(UINT message,DWORD dwFlag);
            MESSAGEFILTERFUNC func = (MESSAGEFILTERFUNC)::GetProcAddress( hDll, "ChangeWindowMessageFilter" );

            if (func)
                func(WM_COPYDATA, MSGFLT_ADD);
        }
    }

    m_editor.Init(hResource, *this);
    const int barParts[8] = {100, 300, 550, 650, 750, 780, 820, 880};
    m_StatusBar.Init(hResource, *this, _countof(barParts), barParts);
    m_TabBar.Init(hResource, *this);
    HIMAGELIST hImgList = ImageList_Create(13, 13, ILC_COLOR32 | ILC_MASK, 0, 3);
    HICON hIcon = ::LoadIcon(hResource, MAKEINTRESOURCE(IDI_SAVED_ICON));
    ImageList_AddIcon(hImgList, hIcon);
    ::DestroyIcon(hIcon);
    hIcon = ::LoadIcon(hResource, MAKEINTRESOURCE(IDI_UNSAVED_ICON));
    ImageList_AddIcon(hImgList, hIcon);
    ::DestroyIcon(hIcon);
    hIcon = ::LoadIcon(hResource, MAKEINTRESOURCE(IDI_READONLY_ICON));
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

    std::async([=]
    {
        bool bNewer = CAppUtils::CheckForUpdate(false);
        if (bNewer)
            PostMessage(m_hwnd, WM_UPDATEAVAILABLE, 0, 0);
    });
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
        HRESULT hr = CoCreateInstance(CLSID_TaskbarList, NULL, CLSCTX_ALL, IID_PPV_ARGS(&pTaskbarInterface));
        if (SUCCEEDED(hr))
        {
            pTaskbarInterface->SetOverlayIcon(m_hwnd, m_hShieldIcon, L"elevated");
        }
    }
}

void CMainWindow::HandleAfterInit()
{
    CCommandHandler::Instance().AfterInit();
    for (const auto& path : m_pathsToOpen)
    {
        unsigned int openFlags = OpenFlags::AskToCreateIfMissing;
        if (m_bPathsToOpenMRU)
            openFlags |= OpenFlags::AddToMRU;
        OpenFileEx(path.first, openFlags);
        if (path.second != (size_t)-1)
            GoToLine(path.second);
    }
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
}

void CMainWindow::ResizeChildWindows()
{
    RECT rect;
    GetClientRect(*this, &rect);
    if (!IsRectEmpty(&rect))
    {
        const UINT flags = SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_NOZORDER | SWP_SHOWWINDOW | SWP_NOCOPYBITS;
        HDWP hDwp = BeginDeferWindowPos(3);
        DeferWindowPos(hDwp, m_StatusBar, nullptr, rect.left, rect.bottom - m_StatusBar.GetHeight(), rect.right - rect.left, m_StatusBar.GetHeight(), flags);
        DeferWindowPos(hDwp, m_TabBar, nullptr, rect.left, rect.top + m_RibbonHeight, rect.right - rect.left, rect.bottom - rect.top, flags);
        RECT tabrc;
        TabCtrl_GetItemRect(m_TabBar, 0, &tabrc);
        MapWindowPoints(m_TabBar, *this, (LPPOINT)&tabrc, 2);
        DeferWindowPos(hDwp, m_editor, nullptr, rect.left, rect.top + m_RibbonHeight + tabrc.bottom - tabrc.top, rect.right - rect.left, rect.bottom - (m_RibbonHeight + tabrc.bottom - tabrc.top) - m_StatusBar.GetHeight(), flags);
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
            IShellItemPtr psiDefFolder = nullptr;
            hr = SHCreateItemFromParsingName(folder.c_str(), nullptr, IID_PPV_ARGS(&psiDefFolder));
            if (CAppUtils::FailedShowMessage(hr))
                return false;
            hr = pfd->SetFolder(psiDefFolder);
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
        doc.m_path = pszPath;
        CMRU::Instance().AddPath(doc.m_path);
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
        {
            EnsureNewLineAtEnd(doc);
        }
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
            std::wstring ext = CPathUtils::GetFileExtension(doc.m_path);
            doc.m_language = CLexStyles::Instance().GetLanguageForExt(ext);
            m_editor.SetupLexerForExt(ext);
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
    m_editor.Call(SCI_GOTOLINE, line);
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

    TCHAR strLnCol[128] = { 0 };
    TCHAR strSel[64] = {0};
    size_t selByte = 0;
    size_t selLine = 0;

    if (m_editor.GetSelectedCount(selByte, selLine))
        swprintf_s(strSel, L"Sel : %Iu | %Iu", selByte, selLine);
    else
        swprintf_s(strSel, L"Sel : %s", L"N/A");
    long line = (long)m_editor.Call(SCI_LINEFROMPOSITION, m_editor.Call(SCI_GETCURRENTPOS)) + 1;
    long column = (long)m_editor.Call(SCI_GETCOLUMN, m_editor.Call(SCI_GETCURRENTPOS)) + 1;
    swprintf_s(strLnCol, L"Ln : %ld    Col : %ld    %s",
                       line, column,
                       strSel);
    std::wstring ttcurpos = CStringUtils::Format(rsStatusTTCurPos, line, column, selByte, selLine);
    m_StatusBar.SetText(strLnCol, ttcurpos.c_str(), STATUSBAR_CUR_POS);

    TCHAR strDocLen[256];
    swprintf_s(strDocLen, L"length : %d    lines : %d", (int)m_editor.Call(SCI_GETLENGTH), (int)m_editor.Call(SCI_GETLINECOUNT));
    std::wstring ttdocsize = CStringUtils::Format(rsStatusTTDocSize, m_editor.Call(SCI_GETLENGTH), m_editor.Call(SCI_GETLINECOUNT));
    m_StatusBar.SetText(strDocLen, ttdocsize.c_str(), STATUSBAR_DOC_SIZE);

    std::wstring tttyping = CStringUtils::Format(rsStatusTTTyping, m_editor.Call(SCI_GETOVERTYPE) ? (LPCWSTR)rsStatusTTTypingOvl : (LPCWSTR)rsStatusTTTypingIns);
    m_StatusBar.SetText(m_editor.Call(SCI_GETOVERTYPE) ? L"OVR" : L"INS", tttyping.c_str(), STATUSBAR_TYPING_MODE);
    bool bCapsLockOn = (GetKeyState(VK_CAPITAL)&0x01)!=0;
    m_StatusBar.SetText(bCapsLockOn ? L"CAPS" : L"", nullptr, STATUSBAR_CAPS);
    if (bEverything)
    {
        CDocument doc = m_DocManager.GetDocumentFromID(m_TabBar.GetCurrentTabId());
        m_StatusBar.SetText(doc.m_language.c_str(), nullptr, STATUSBAR_DOC_TYPE);
        std::wstring tteof = CStringUtils::Format(rsStatusTTEOF, FormatTypeToString(doc.m_format).c_str());
        m_StatusBar.SetText(FormatTypeToString(doc.m_format).c_str(), tteof.c_str(), STATUSBAR_EOF_FORMAT);
        m_StatusBar.SetText(doc.GetEncodingString().c_str(), nullptr, STATUSBAR_UNICODE_TYPE);

        std::wstring tttabs = CStringUtils::Format(rsStatusTTTabs, m_TabBar.GetItemCount());
        m_StatusBar.SetText(CStringUtils::Format(L"tabs: %d", m_TabBar.GetItemCount()).c_str(), tttabs.c_str(), STATUSBAR_TABS);

        if (SysInfo::Instance().IsUACEnabled() && SysInfo::Instance().IsElevated())
        {
            // in case we're running elevated, use a BowPad icon with a shield
            SendMessage(m_StatusBar, SB_SETICON, 0, (LPARAM)m_hShieldIcon);
        }
    }
}

bool CMainWindow::CloseTab( int tab, bool force /* = false */ )
{
    if ((tab < 0) || (tab >= m_DocManager.GetCount()))
        return false;
    CDocument doc = m_DocManager.GetDocumentFromID(m_TabBar.GetIDFromIndex(tab));
    if (!force && (doc.m_bIsDirty||doc.m_bNeedsSaving))
    {
        m_TabBar.ActivateAt(tab);
        ResponseToCloseTab response = AskToCloseTab();
        if (response == ResponseToCloseTab::SaveAndClose)
            SaveCurrentTab(); // Save And (fall through to) Close

        else if (response == ResponseToCloseTab::CloseWithoutSaving)
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

    if ((m_TabBar.GetItemCount() == 1)&&(m_editor.Call(SCI_GETTEXTLENGTH)==0)&&(m_editor.Call(SCI_GETMODIFY)==0)&&doc.m_path.empty())
        return true;  // leave the empty, new document as is

    CCommandHandler::Instance().OnDocumentClose(tab);

    m_DocManager.RemoveDocument(m_TabBar.GetIDFromIndex(tab));
    m_TabBar.DeletItemAt(tab);
    EnsureAtLeastOneTab();
    m_TabBar.ActivateAt(tab < m_TabBar.GetItemCount() ? tab : m_TabBar.GetItemCount()-1);
    return true;
}

bool CMainWindow::CloseAllTabs()
{
    do
    {
        if (CloseTab(m_TabBar.GetItemCount()-1) == false)
            return false;
        if ((m_TabBar.GetItemCount() == 1)&&(m_editor.Call(SCI_GETTEXTLENGTH)==0)&&(m_editor.Call(SCI_GETMODIFY)==0)&&m_DocManager.GetDocumentFromID(m_TabBar.GetIDFromIndex(0)).m_path.empty())
            return true;
    } while (m_TabBar.GetItemCount() > 0);
    return true;
}

void CMainWindow::CloseAllButCurrentTab()
{
    int count = m_TabBar.GetItemCount();
    int current = m_TabBar.GetCurrentTabIndex();
    for (int i = count-1; i >= 0; --i)
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

        std::wstring sWindowTitle = elev;
        if (!elev.empty())
            sWindowTitle += L" : ";
        sWindowTitle += doc.m_path.empty() ? m_TabBar.GetCurrentTitle() : doc.m_path;
        sWindowTitle += L" - ";
        sWindowTitle += appName;
        SetWindowText(*this, sWindowTitle.c_str());
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
    if (doc.m_bIsReadonly)
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
    int nClickedBtn = 0;
    HRESULT hr = TaskDialogIndirect( &tdc, &nClickedBtn, nullptr, nullptr );
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
    bool changed = doc.m_bNeedsSaving || doc.m_bIsDirty;
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
    int nClickedBtn = 0;
    HRESULT hr = TaskDialogIndirect ( &tdc, &nClickedBtn, nullptr, nullptr );
    if (CAppUtils::FailedShowMessage(hr))
        nClickedBtn = 0;
    ResponseToOutsideModifiedFile response = ResponseToOutsideModifiedFile::Cancel;
    switch (nClickedBtn)
    {
    case 101:
        response = ResponseToOutsideModifiedFile::Reload;
        break;
    case 102:
        response = ResponseToOutsideModifiedFile::KeepOurChanges;
        break;
    }
    return response;
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
    ResString rEditFile(hRes, IDS_EDITFILE);
    ResString rCancel(hRes, IDS_CANCEL);

    // TODO! remove the short cut accelerator &e &c
    // options from these buttons because this dialog is auomatically
    // triggered by typing and it's too easy to accidentally
    // acknowledge the button by type say hello int o the editor
    // and getting interrupted by the dialog and matching the
    // the edit button with the e of hello.
    // Need mew resource entries for this.
    TASKDIALOGCONFIG tdc = { sizeof(TASKDIALOGCONFIG) };
    TASKDIALOG_BUTTON aCustomButtons[2];
    aCustomButtons[0].nButtonID = 101;
    aCustomButtons[0].pszButtonText = rEditFile;
    aCustomButtons[1].nButtonID = 100;
    aCustomButtons[1].pszButtonText = rCancel;
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
        // FIXME TODO! Create/use an appropriate CPathUtils function for this.
        WriteAsciiStringToClipboard(doc.m_path.substr(0, doc.m_path.find_last_of(L"\\/")).c_str(), *this);
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
                if (lptstr != nullptr)
                {
                    s = lptstr;
                    GlobalUnlock(hData);
                }
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
            m_ClipboardHistory.push_front(s);
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
                std::wstring::iterator new_end = std::unique(sf.begin(), sf.end(), [&] (wchar_t lhs, wchar_t rhs) -> bool
                {
                    return (lhs == rhs) && (lhs == ' ');
                });
                sf.erase(new_end, sf.end());

                AppendMenu(hMenu, MF_ENABLED, index, sf.substr(0, maxsize).c_str());
                ++index;
            }
            int selIndex = TrackPopupMenu(hMenu, TPM_LEFTALIGN|TPM_RETURNCMD, pt.x, pt.y, 0, m_editor, nullptr);
            DestroyMenu (hMenu);
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
    int style = (int)m_editor.Call(SCI_GETSTYLEAT, scn.position);
    // Note style will be zero if no style or past end of the document.
    if (style & INDIC2_MASK)
    {
        // an url hotspot
        ResString str(hRes, IDS_CTRLCLICKTOOPEN);
        std::string strA = CUnicodeUtils::StdGetUTF8((LPCWSTR)str);
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
        bool failed = true;
        COLORREF color = 0;
        DWORD hexval = 0;

        // Note: could use std::stoi family of functions but they throw
        // range exceptions etc. and VC reports those in the debugger output
        // window. That's distracting and gives the impression something
        // is drastically wrong when it isn't, so we don't use those.

        std::string strNum = sWord.substr(1); // Drop #
        if (strNum.size() == 3) // short form .e.g. F0F
        {
            BYTE rgb[3]; // [0]=red, [1]=green, [2]=blue
            failed = false;
            char dig[2];
            dig[1] = '\0';
            for (int i = 0; i < 3; ++i)
            {
                dig[0] = strNum[i];
                BYTE& v = rgb[i];
                char* ep = nullptr;
                errno = 0;
                // Must convert all digits of string.
                v = (BYTE) strtoul(dig, &ep, 16);
                if (errno == 0 && ep == &dig[1])
                    v = v * 16 + v;
                else
                {
                    v = 0;
                    failed = true;
                }
            }
            if (!failed)
            {
                color = RGB(rgb[0], rgb[1], rgb[2]);
                hexval = (RGB((color >> 16) & 0xFF, (color >> 8) & 0xFF, color & 0xFF)) | (color & 0xFF000000);
            }
        }
        else if (strNum.size() == 6) // normal/long form, e.g. FF00FF
        {
            char* ep = nullptr;
            failed = false;
            errno = 0;
            hexval = strtoul(strNum.c_str(), &ep, 16);
            // Must convert all digits of string.
            if (errno == 0 && ep == &strNum[6] )
                color = (RGB((hexval >> 16) & 0xFF, (hexval >> 8) & 0xFF, hexval & 0xFF)) | (hexval & 0xFF000000);
            else
                failed = true;
        }
        if (!failed)
        {
            std::string sCallTip = CStringUtils::Format("RGB(%d,%d,%d)\nHex: #%06lX\n####################\n####################\n####################", GetRValue(color), GetGValue(color), GetBValue(color), hexval);
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
        long long number = 0;
        char* ep = nullptr;
        // 0 base means determine base from any format in the string.
        errno = 0;
        number = strtoll(sWord.c_str(), &ep, 0);
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
        int id = m_TabBar.GetIDFromIndex(tab);
        if (id >= 0)
        {
            if (m_DocManager.HasDocumentID(id))
            {
                CDocument doc = m_DocManager.GetDocumentFromID(id);
                m_tooltipbuffer = std::unique_ptr<wchar_t[]>(new wchar_t[doc.m_path.size()+1]);
                wcscpy_s(m_tooltipbuffer.get(), doc.m_path.size()+1, doc.m_path.c_str());
                lpnmtdi->lpszText = m_tooltipbuffer.get();
                lpnmtdi->hinst = nullptr;
            }
        }
    }
}

void CMainWindow::HandleHotSpotClick(const Scintilla::SCNotification& scn)
{
   if (!(scn.modifiers & SCMOD_CTRL))
       return;

    m_editor.Call(SCI_SETWORDCHARS, 0, (LPARAM)"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-+.,:;?&@=/%#()");

    long pos = scn.position;
    long startPos = static_cast<long>(m_editor.Call(SCI_WORDSTARTPOSITION, pos, false));
    long endPos = static_cast<long>(m_editor.Call(SCI_WORDENDPOSITION, pos, false));

    m_editor.Call(SCI_SETTARGETSTART, startPos);
    m_editor.Call(SCI_SETTARGETEND, endPos);

    long posFound = (long)m_editor.Call(SCI_SEARCHINTARGET, strlen(URL_REG_EXPR), (LPARAM)URL_REG_EXPR);
    if (posFound != -1)
    {
        startPos = int(m_editor.Call(SCI_GETTARGETSTART));
        endPos = int(m_editor.Call(SCI_GETTARGETEND));
    }

    std::string urltext = m_editor.GetTextRange(startPos, endPos);

    // This treatment would fail on some valid URLs where there's actually supposed to be a comma or parenthesis at the end.
    size_t lastCharIndex = urltext.size() - 1;
    while (lastCharIndex > 0 && (urltext[lastCharIndex] == ',' || urltext[lastCharIndex] == ')' || urltext[lastCharIndex] == '('))
    {
        urltext[lastCharIndex] = '\0';
        --lastCharIndex;
    }

    std::wstring url = CUnicodeUtils::StdGetUnicode(urltext);
    while ((*url.begin() == '(') || (*url.begin() == ')') || (*url.begin() == ','))
        url.erase(url.begin());

    SearchReplace(url, L"&amp;", L"&");

    ::ShellExecute(*this, L"open", url.c_str(), nullptr, nullptr, SW_SHOW);
    m_editor.Call(SCI_SETCHARSDEFAULT);
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
        if (doc.m_bIsReadonly)
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
    long endPos = -1;
    long endStyle = (long)m_editor.Call(SCI_GETENDSTYLED);

    long firstVisibleLine = (long)m_editor.Call(SCI_GETFIRSTVISIBLELINE);
    startPos = (long)m_editor.Call(SCI_POSITIONFROMLINE, m_editor.Call(SCI_DOCLINEFROMVISIBLE, firstVisibleLine));
    long linesOnScreen = (long)m_editor.Call(SCI_LINESONSCREEN);
    long lineCount = (long)m_editor.Call(SCI_GETLINECOUNT);
    endPos = (long)m_editor.Call(SCI_POSITIONFROMLINE, m_editor.Call(SCI_DOCLINEFROMVISIBLE, firstVisibleLine + min(linesOnScreen, lineCount)));

    std::vector<unsigned char> hotspotPairs;

    unsigned char style_hotspot = 0;
    unsigned char mask = INDIC2_MASK;

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

        m_editor.Call(SCI_SETTARGETSTART, startPos);
        m_editor.Call(SCI_SETTARGETEND, endPos);

        LRESULT posFound = m_editor.Call(SCI_SEARCHINTARGET, strlen(URL_REG_EXPR), (LPARAM)URL_REG_EXPR);

        if (posFound != -1)
        {
            long start = long(m_editor.Call(SCI_GETTARGETSTART));
            long end = long(m_editor.Call(SCI_GETTARGETEND));
            long foundTextLen = end - start;
            unsigned char idStyle = static_cast<unsigned char>(m_editor.Call(SCI_GETSTYLEAT, posFound));

            // Search the style
            long fs = -1;
            for (size_t i = 0 ; i < hotspotPairs.size() ; i++)
            {
                // make sure to ignore "hotspot bit" when comparing document style with archived hotspot style
                if ((hotspotPairs[i] & ~mask) == (idStyle & ~mask))
                {
                    fs = hotspotPairs[i];
                    m_editor.Call(SCI_STYLEGETFORE, fs);
                    break;
                }
            }

            // if we found it then use it to colorize
            if (fs != -1)
            {
                m_editor.Call(SCI_STARTSTYLING, start, 0xFF);
                m_editor.Call(SCI_SETSTYLING, foundTextLen, fs);
            }
            else // generate a new style and add it into a array
            {
                style_hotspot = idStyle | mask; // set "hotspot bit"
                hotspotPairs.push_back(style_hotspot);
                int activeFG = 0xFF0000;
                unsigned char idStyleMSBunset = idStyle & ~mask;
                char fontNameA[128];

                // copy the style data
                m_editor.Call(SCI_STYLEGETFONT, idStyleMSBunset, (LPARAM)fontNameA);
                m_editor.Call(SCI_STYLESETFONT, style_hotspot, (LPARAM)fontNameA);

                m_editor.Call(SCI_STYLESETSIZE, style_hotspot, m_editor.Call(SCI_STYLEGETSIZE, idStyleMSBunset));
                m_editor.Call(SCI_STYLESETBOLD, style_hotspot, m_editor.Call(SCI_STYLEGETBOLD, idStyleMSBunset));
                m_editor.Call(SCI_STYLESETWEIGHT, style_hotspot, m_editor.Call(SCI_STYLEGETWEIGHT, idStyleMSBunset));
                m_editor.Call(SCI_STYLESETITALIC, style_hotspot, m_editor.Call(SCI_STYLEGETITALIC, idStyleMSBunset));
                m_editor.Call(SCI_STYLESETUNDERLINE, style_hotspot, m_editor.Call(SCI_STYLEGETUNDERLINE, idStyleMSBunset));
                m_editor.Call(SCI_STYLESETFORE, style_hotspot, m_editor.Call(SCI_STYLEGETFORE, idStyleMSBunset));
                m_editor.Call(SCI_STYLESETBACK, style_hotspot, m_editor.Call(SCI_STYLEGETBACK, idStyleMSBunset));
                m_editor.Call(SCI_STYLESETEOLFILLED, style_hotspot, m_editor.Call(SCI_STYLEGETEOLFILLED, idStyleMSBunset));
                m_editor.Call(SCI_STYLESETCASE, style_hotspot, m_editor.Call(SCI_STYLEGETCASE, idStyleMSBunset));


                m_editor.Call(SCI_STYLESETHOTSPOT, style_hotspot, TRUE);
                m_editor.Call(SCI_SETHOTSPOTACTIVEUNDERLINE, style_hotspot, TRUE);
                m_editor.Call(SCI_SETHOTSPOTACTIVEFORE, TRUE, activeFG);
                m_editor.Call(SCI_SETHOTSPOTSINGLELINE, style_hotspot, 0);

                // colorize it!
                m_editor.Call(SCI_STARTSTYLING, start, 0xFF);
                m_editor.Call(SCI_SETSTYLING, foundTextLen, style_hotspot);
            }
        }
        m_editor.Call(SCI_SETTARGETSTART, fStartPos);
        m_editor.Call(SCI_SETTARGETEND, fEndPos);
        m_editor.Call(SCI_SETSEARCHFLAGS, 0);
        posFoundColonSlash = (int)m_editor.Call(SCI_SEARCHINTARGET, 3, (LPARAM)"://");
    }

    m_editor.Call(SCI_STARTSTYLING, endStyle, 0xFF);
    m_editor.Call(SCI_SETSTYLING, 0, 0);
}

void CMainWindow::HandleUpdateUI(const Scintilla::SCNotification& scn)
{
    const unsigned int uiflags = SC_UPDATE_SELECTION |
        SC_UPDATE_H_SCROLL | SC_UPDATE_V_SCROLL;
    if ((scn.updated & uiflags) != 0)
        m_editor.MarkSelectedWord(false);

    m_editor.MatchBraces();
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
}

void CMainWindow::HandleTabChanging(const TBHDR& /*tbhdr*/)
{
    // document is about to be deactivated
    int docID = m_TabBar.GetCurrentTabId();
    if (m_DocManager.HasDocumentID(docID))
    {
        CDocument doc = m_DocManager.GetDocumentFromID(docID);
        m_editor.SaveCurrentPos(&doc.m_position);
        m_DocManager.SetDocument(docID, doc);
    }
}

void CMainWindow::HandleTabChange(const TBHDR& ptbhdr)
{
    int curTab = m_TabBar.GetCurrentTabIndex();
    APPVERIFY(ptbhdr.tabOrigin == curTab);
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
        // Shouldn't be modified after we've handled it.
        auto ds = m_DocManager.HasFileChanged(docID);
        APPVERIFY(ds != DM_Modified);
    }
    else if (ds == DM_Removed)
    {
        HandleOutsideDeletedFile(curTab);
    }
    CheckForOutsideChanges();
}

void CMainWindow::HandleTabDelete(const TBHDR& tbhdr)
{
    int tabToDelete = tbhdr.tabOrigin;
    // Close tab will take care of activating any next tab.
    CloseTab(tabToDelete);
}

bool CMainWindow::OpenFileEx(const std::wstring& file, unsigned int openFlags)
{
    bool bRet = true;
    bool bAddToMRU = (openFlags & OpenFlags::AddToMRU) != 0;
    bool bAskToCreateIfMissing = (openFlags & OpenFlags::AskToCreateIfMissing) != 0;

    int encoding = -1;
    std::wstring filepath = CPathUtils::GetLongPathname(file);
    if (_wcsicmp(CIniSettings::Instance().GetIniPath().c_str(), filepath.c_str()) == 0)
    {
        CIniSettings::Instance().Save();
    }
    int id = m_DocManager.GetIdForPath(filepath.c_str());
    if (id != -1)
    {
        // document already open
        m_TabBar.ActivateAt(m_TabBar.GetIndexFromID(id));
    }
    else
    {
        bool createIfMissing = false;
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
        }

        CDocument doc = m_DocManager.LoadFile(*this, filepath, encoding, createIfMissing);
        if (doc.m_document)
        {
            if (m_TabBar.GetItemCount() == 1)
            {
                // check if the only tab is empty and if it is, remove it
                auto docID = m_TabBar.GetIDFromIndex(0);
                CDocument existDoc = m_DocManager.GetDocumentFromID(docID);
                if (existDoc.m_path.empty() && (m_editor.Call(SCI_GETLENGTH)==0) && (m_editor.Call(SCI_CANUNDO)==0))
                {
                    m_DocManager.RemoveDocument(docID);
                    m_TabBar.DeletItemAt(0);
                }
            }
            if (bAddToMRU)
                CMRU::Instance().AddPath(filepath);
            std::wstring sFileName = CPathUtils::GetFileName(filepath);
            std::wstring ext = CPathUtils::GetFileExtension(filepath);
            int index = -1;
            if (m_insertionIndex >= 0)
                index = m_TabBar.InsertAfter(m_insertionIndex, sFileName.c_str());
            else
                index = m_TabBar.InsertAtEnd(sFileName.c_str());
            int id = m_TabBar.GetIDFromIndex(index);
            if (ext.empty())
                doc.m_language = CLexStyles::Instance().GetLanguageForPath(filepath);
            else
                doc.m_language = CLexStyles::Instance().GetLanguageForExt(ext);
            m_DocManager.AddDocumentAtEnd(doc, id);
            m_editor.Call(SCI_SETDOCPOINTER, 0, doc.m_document);
            m_TabBar.ActivateAt(index);
            if (ext.empty())
                m_editor.SetupLexerForLang(doc.m_language);
            else
                m_editor.SetupLexerForExt(ext);
            SHAddToRecentDocs(SHARD_PATHW, filepath.c_str());
            CCommandHandler::Instance().OnDocumentOpen(index);
        }
        else
        {
            CMRU::Instance().RemovePath(filepath, false);
            bRet = false;
        }
    }
    m_insertionIndex = -1;
    return bRet;
}

// TODO: consider continuing merging process,
// how to merge with OpenFileAs with OpenFile
bool CMainWindow::OpenFileAs( const std::wstring& temppath, const std::wstring& realpath, bool bModified )
{
    // If we can't open it, not much we can do.
    if (!OpenFile(temppath, 0))
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
    std::wstring ext = CPathUtils::GetFileExtension(doc.m_path);
    std::wstring sFileName = CPathUtils::GetFileName(doc.m_path);
    doc.m_language = CLexStyles::Instance().GetLanguageForExt(ext);
    m_DocManager.SetDocument(docID, doc);
    if (doc.m_bIsReadonly)
        m_editor.Call(SCI_SETREADONLY, true);
    m_editor.SetupLexerForExt(ext);
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

// TODO:
// CmdClipboard.h's CmdPaste::execute() method should probably use this.

void CMainWindow::HandleDropFiles(HDROP hDrop)
{
    int filesDropped = DragQueryFile(hDrop, 0xffffffff, nullptr, 0);
    std::vector<std::wstring> files;
    for (int i = 0 ; i < filesDropped ; ++i)
    {
        UINT len = DragQueryFile(hDrop, i, nullptr, 0);
        std::unique_ptr<wchar_t[]> pathBuf(new wchar_t[len+1]);
        DragQueryFile(hDrop, i, pathBuf.get(), len+1);
        files.push_back(pathBuf.get());
    }
    DragFinish(hDrop);
    for (const auto& filename:files)
    {
        OpenFileEx(filename, OpenFlags::AddToMRU);
    }
}

void CMainWindow::HandleCopyDataCommandLine(const COPYDATASTRUCT& cds)
{
    CCmdLineParser parser((LPCWSTR)cds.lpData);
    LPCTSTR path = parser.GetVal(L"path");
    if (path)
    {
        if (OpenFileEx(path, OpenFlags::AddToMRU | OpenFlags::AskToCreateIfMissing))
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

        LPWSTR * szArglist = CommandLineToArgvW((LPCWSTR)cds.lpData, &nArgs);
        if (!szArglist)
            return;

        int filesOpened = 0;
        for (int i = 1; i < nArgs; i++)
        {
            if (szArglist[i][0] != '/')
            {
                if (OpenFileEx(szArglist[i], OpenFlags::AddToMRU | OpenFlags::AskToCreateIfMissing))
                    ++filesOpened;
            }
        }

        // Free memory allocated for CommandLineToArgvW arguments.
        LocalFree(szArglist);

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

void CMainWindow::HandleTabDroppedOutside(int tab, POINT pt)
{
    // Start a new instance of BowPad with this dropped tab, or add this tab to
    // the BowPad window the drop was done on. Then close this tab.
    // First save the file to a temp location to ensure all unsaved mods are saved.
    std::wstring temppath = CTempFiles::Instance().GetTempFilePath(true);
    CDocument doc = m_DocManager.GetDocumentFromID(m_TabBar.GetIDFromIndex(tab));
    CDocument tempdoc = doc;
    tempdoc.m_path = temppath;
    bool bDummy = false;
    m_DocManager.SaveFile(*this, tempdoc, bDummy);
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
            COPYDATASTRUCT cpd = { 0 };
            cpd.dwData = CD_COMMAND_MOVETAB;
            std::wstring cpdata = doc.m_path + L"*" + temppath + L"*";
            cpdata += (doc.m_bIsDirty || doc.m_bNeedsSaving) ? L"1*" : L"0*";
            cpdata += CStringUtils::Format(L"%ld", (long)(m_editor.Call(SCI_LINEFROMPOSITION, m_editor.Call(SCI_GETCURRENTPOS)) + 1));
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
                // TODO. Work in progress.
                // On error, in theory the sender or receiver or both can display an error.
                // Until it's clear what set of errors and handling is required just throw
                // up a simple dialog box this side. In theory the receiver may even crash
                // with no error, so probably some minimal message might be useful.
                ::MessageBox(*this, L"Failed to move Tab.", GetAppName().c_str(), MB_OK | MB_ICONERROR);
            }
            return;
        }
    }

    // Start a new instance and open the tab there.
    std::wstring modpath = CPathUtils::GetModulePath();
    std::wstring cmdline = CStringUtils::Format(L"/multiple /tabmove /savepath:\"%s\" /path:\"%s\" /line:%ld",
                                                doc.m_path.c_str(), temppath.c_str(),
                                                (long)(m_editor.Call(SCI_LINEFROMPOSITION, m_editor.Call(SCI_GETCURRENTPOS)) + 1));
    if (doc.m_bIsDirty || doc.m_bNeedsSaving)
        cmdline += L" /modified";
    SHELLEXECUTEINFO shExecInfo = { };
    shExecInfo.cbSize = sizeof(SHELLEXECUTEINFO);

    //shExecInfo.fMask = NULL;
    shExecInfo.hwnd = *this;
    shExecInfo.lpVerb = L"open";
    shExecInfo.lpFile = modpath.c_str();
    shExecInfo.lpParameters = cmdline.c_str();
    //shExecInfo.lpDirectory = NULL;
    shExecInfo.nShow = SW_NORMAL;
    //shExecInfo.hInstApp = NULL;

    if (ShellExecuteEx(&shExecInfo))
    {
        // remove the tab
        CloseTab(tab, true);
    }
}

bool CMainWindow::ReloadTab( int tab, int encoding, bool dueToOutsideChanges )
{
    APPVERIFY(tab == m_TabBar.GetCurrentTabIndex());
    int docID = m_TabBar.GetIDFromIndex(tab);
    if (docID < 0) // No such tab.
        return false;
    if (!m_DocManager.HasDocumentID(docID)) // No such document
        return false;
    CDocument doc = m_DocManager.GetDocumentFromID(docID);

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
            m_DocManager.SetDocument(docID, doc);
            return false;
        }
    }
    else if (doc.m_bIsDirty || doc.m_bNeedsSaving)
    {
        if (!AskToReload(doc)) // User doesn't want to reload.
            return false;
    }

    // TODO! Review: I merged two similar reload functions with slightly
    // different enconding handling. One seemed to try to keep the encoding,
    // another seemed to want to reload it to revert it.
    // Check I changed the handling and what should happen.
    // I think it should revert or you should be asked.

    // LoadFile increases the reference count, so decrease it here first
    m_editor.Call(SCI_RELEASEDOCUMENT, 0, doc.m_document);
    CDocument docreload = m_DocManager.LoadFile(*this, doc.m_path.c_str(), encoding, false);
    if (! docreload.m_document)
        return false;

    if (tab == m_TabBar.GetCurrentTabIndex())
        m_editor.SaveCurrentPos(&doc.m_position);

    // Apply the new one.
    m_editor.Call(SCI_SETDOCPOINTER, 0, docreload.m_document);

    docreload.m_language = doc.m_language;
    docreload.m_position = doc.m_position;
    m_DocManager.SetDocument(docID, docreload);
    if (tab == m_TabBar.GetCurrentTabIndex())
    {
        m_editor.SetupLexerForLang(docreload.m_language);
        m_editor.RestoreCurrentPos(docreload.m_position);
    }
    UpdateStatusBar(true);
    UpdateTab(docID);
    m_editor.Call(SCI_SETSAVEPOINT);
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
    // TODO! This logic is a bit restrictured from before but
    // has the same design and flaw. We shouldn't activate each
    // tab forcing the user to make reload choices for each one.
    // just the first. Then let the user find the others when they
    // change tabs. We could draw their attention to it by
    // marking the tab header "stale" via a colour change for example.

    // See if any doc has been changed externally.
    for (int i = 0; i < m_TabBar.GetItemCount(); ++i)
    {
        int docID = m_TabBar.GetIDFromIndex(i);
        auto ds = m_DocManager.HasFileChanged(docID);
        if (ds == DM_Modified || ds == DM_Removed)
        {
            m_TabBar.ActivateAt(i);
            break;
        }
    }
    // TODO! Test if All tabs might get removed here?
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
    if (doc.m_bIsReadonly)
        m_editor.Call(SCI_SETREADONLY, true);

    std::wstring ext = CPathUtils::GetFileExtension(doc.m_path);
    std::wstring sFileName = CPathUtils::GetFileName(doc.m_path);
    doc.m_language = CLexStyles::Instance().GetLanguageForExt(ext);
    m_DocManager.SetDocument(docID, doc);

    m_editor.SetupLexerForExt(ext);
    if (sFileName.empty())
        m_TabBar.SetCurrentTitle(GetNewTabName().c_str());
    else
        m_TabBar.SetCurrentTitle(sFileName.c_str());

    UpdateTab(docID);
    UpdateCaptionBar();
    UpdateStatusBar(true);

    GoToLine(line);

    DeleteFile(filepath.c_str());
}
