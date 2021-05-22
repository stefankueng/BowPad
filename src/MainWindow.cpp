// This file is part of BowPad.
//
// Copyright (C) 2013-2021 - Stefan Kueng
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
#include "SettingsDlg.h"
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
#include "CustomTooltip.h"
#include "GDIHelpers.h"
#include "Windows10Colors.h"
#include "DarkModeHelper.h"
#include "DPIAware.h"
#include "Monitor.h"
#include "ResString.h"
#include "../ext/tinyexpr/tinyexpr.h"

#include <memory>
#include <cassert>
#include <type_traits>
#include <future>
#include <Shobjidl.h>

IUIFramework* g_pFramework = nullptr; // Reference to the Ribbon framework.

namespace
{
constexpr COLORREF folderColors[] = {
    RGB(177, 199, 253),
    RGB(221, 253, 177),
    RGB(253, 177, 243),
    RGB(177, 253, 240),
    RGB(253, 218, 177),
    RGB(196, 177, 253),
    RGB(180, 253, 177),
    RGB(253, 177, 202),
    RGB(177, 225, 253),
    RGB(247, 253, 177),
};
constexpr int MAX_FOLDERCOLORS = static_cast<int>(std::size(folderColors));

const int STATUSBAR_DOC_TYPE     = 0;
const int STATUSBAR_CUR_POS      = 1;
const int STATUSBAR_SEL          = 2;
const int STATUSBAR_EDITORCONFIG = 3;
const int STATUSBAR_EOL_FORMAT   = 4;
const int STATUSBAR_TABSPACE     = 5;
// ReSharper disable once CppInconsistentNaming
const int STATUSBAR_R2L          = 6;
const int STATUSBAR_UNICODE_TYPE = 7;
const int STATUSBAR_TYPING_MODE  = 8;
const int STATUSBAR_CAPS         = 9;
const int STATUSBAR_TABS         = 10;
const int STATUSBAR_ZOOM         = 11;

constexpr char   URL_REG_EXPR[]      = {"\\b[A-Za-z+]{3,9}://[A-Za-z0-9_\\-+~.:?&@=/%#,;{}()[\\]|*!\\\\]+\\b"};
constexpr size_t URL_REG_EXPR_LENGTH = _countof(URL_REG_EXPR) - 1;

const int TIMER_UPDATECHECK = 101;

ResponseToOutsideModifiedFile responseToOutsideModifiedFile      = ResponseToOutsideModifiedFile::Reload;
BOOL                          responseToOutsideModifiedFileDoAll = FALSE;
bool                          doModifiedAll                      = FALSE;

bool               doCloseAll         = false;
BOOL               closeAllDoAll      = FALSE;
ResponseToCloseTab responseToCloseTab = ResponseToCloseTab::CloseWithoutSaving;
} // namespace

inline bool IsHexDigitString(const char* str)
{
    for (int i = 0; str[i]; ++i)
    {
        if (!isxdigit(str[i]))
            return false;
    }
    return true;
}

static bool ShowFileSaveDialog(HWND hParentWnd, const std::wstring& title, const std::wstring fileExt, UINT extIndex, std::wstring& path)
{
    PreserveChdir      keepCwd;
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

    hr = pfd->SetTitle(title.c_str());
    if (CAppUtils::FailedShowMessage(hr))
        return false;

    hr = pfd->SetFileTypes(static_cast<UINT>(CLexStyles::Instance().GetFilterSpecCount()), CLexStyles::Instance().GetFilterSpecData());
    if (CAppUtils::FailedShowMessage(hr))
        return false;

    if (extIndex > 0)
    {
        hr = pfd->SetFileTypeIndex(extIndex + 1);
        if (CAppUtils::FailedShowMessage(hr))
            return false;
    }
    if (!fileExt.empty())
    {
        hr = pfd->SetDefaultExtension(fileExt.c_str());
        if (CAppUtils::FailedShowMessage(hr))
            return false;
    }

    // set the default folder to the folder of the current tab
    if (!path.empty())
    {
        std::wstring folder = CPathUtils::GetParentDirectory(path);
        if (folder.empty())
            folder = CPathUtils::GetCWD();
        auto modDir = CPathUtils::GetLongPathname(CPathUtils::GetModuleDir());
        if (_wcsicmp(folder.c_str(), modDir.c_str()) != 0)
        {
            std::wstring  filename     = CPathUtils::GetFileName(path);
            IShellItemPtr psiDefFolder = nullptr;
            hr                         = SHCreateItemFromParsingName(folder.c_str(), nullptr, IID_PPV_ARGS(&psiDefFolder));
            if (!CAppUtils::FailedShowMessage(hr))
            {
                hr = pfd->SetFolder(psiDefFolder);
                if (CAppUtils::FailedShowMessage(hr))
                    return false;
                if (!filename.empty())
                {
                    hr = pfd->SetFileName(filename.c_str());
                    if (CAppUtils::FailedShowMessage(hr))
                        return false;
                }
            }
        }
    }

    // Show the save file dialog
    hr = pfd->Show(hParentWnd);
    if (CAppUtils::FailedShowMessage(hr))
        return false;
    IShellItemPtr psiResult = nullptr;
    hr                      = pfd->GetResult(&psiResult);
    if (CAppUtils::FailedShowMessage(hr))
        return false;
    PWSTR pszPath = nullptr;
    hr            = psiResult->GetDisplayName(SIGDN_FILESYSPATH, &pszPath);
    if (CAppUtils::FailedShowMessage(hr))
        return false;
    path = (pszPath ? pszPath : L"");
    CoTaskMemFree(pszPath);
    return true;
}

CMainWindow::CMainWindow(HINSTANCE hInst, const WNDCLASSEX* wcx /* = nullptr*/)
    : CWindow(hInst, wcx)
    , m_statusBar(hInst)
    , m_tabBar(hInst)
    , m_editor(hInst)
    , m_fileTree(hInst, this)
    , m_tablistBtn(hInst)
    , m_newTabBtn(hInst)
    , m_closeTabBtn(hInst)
    , m_progressBar(hInst)
    , m_custToolTip(hResource)
    , m_treeWidth(0)
    , m_bDragging(false)
    , m_oldPt{0, 0}
    , m_fileTreeVisible(true)
    , m_bPathsToOpenMRU(true)
    , m_tabMoveMod(false)
    , m_initLine(0)
    , m_insertionIndex(-1)
    , m_windowRestored(false)
    , m_inMenuLoop(false)
    , m_scratchEditor(hResource)
    , m_lastFolderColorIndex(0)
    , m_blockCount(0)
    , m_normalThemeText(0)
    , m_normalThemeBack(0)
    , m_normalThemeHigh(0)
    , m_autoCompleter(this, &m_editor)
    , m_dwellStartPos(-1)
    , m_bBlockAutoIndent(false)
    , m_hShieldIcon(nullptr)
    , m_hCapsLockIcon(nullptr)
    , m_hLexerIcon(nullptr)
    , m_hZoomIcon(nullptr)
    , m_hZoomDarkIcon(nullptr)
    , m_hEmptyIcon(nullptr)
    , hEditorconfigActive(nullptr)
    , hEditorconfigInactive(nullptr)
    , m_cRef(1)
    , m_newCount(0)
    , m_pRibbon(nullptr)
    , m_ribbonHeight(0)
{
    auto cxIcon = GetSystemMetrics(SM_CXSMICON);
    auto cyIcon = GetSystemMetrics(SM_CYSMICON);

    m_hShieldIcon         = static_cast<HICON>(::LoadImage(hResource, MAKEINTRESOURCE(IDI_ELEVATED), IMAGE_ICON, cxIcon, cyIcon, LR_DEFAULTCOLOR));
    m_hCapsLockIcon       = static_cast<HICON>(::LoadImage(hResource, MAKEINTRESOURCE(IDI_CAPSLOCK), IMAGE_ICON, cxIcon, cyIcon, LR_DEFAULTCOLOR));
    m_hLexerIcon          = static_cast<HICON>(::LoadImage(hResource, MAKEINTRESOURCE(IDI_LEXER), IMAGE_ICON, cxIcon, cyIcon, LR_DEFAULTCOLOR));
    m_hZoomIcon           = static_cast<HICON>(::LoadImage(hResource, MAKEINTRESOURCE(IDI_ZOOM), IMAGE_ICON, cxIcon, cyIcon, LR_DEFAULTCOLOR));
    m_hZoomDarkIcon       = static_cast<HICON>(::LoadImage(hResource, MAKEINTRESOURCE(IDI_ZOOMDARK), IMAGE_ICON, cxIcon, cyIcon, LR_DEFAULTCOLOR));
    m_hEmptyIcon          = static_cast<HICON>(::LoadImage(hResource, MAKEINTRESOURCE(IDI_EMPTY), IMAGE_ICON, cxIcon, cyIcon, LR_DEFAULTCOLOR));
    hEditorconfigActive   = static_cast<HICON>(::LoadImage(hResource, MAKEINTRESOURCE(IDI_EDITORCONFIGACTIVE), IMAGE_ICON, cxIcon, cyIcon, LR_DEFAULTCOLOR));
    hEditorconfigInactive = static_cast<HICON>(::LoadImage(hResource, MAKEINTRESOURCE(IDI_EDITORCONFIGINACTIVE), IMAGE_ICON, cxIcon, cyIcon, LR_DEFAULTCOLOR));
    m_fileTreeVisible     = CIniSettings::Instance().GetInt64(L"View", L"FileTree", 1) != 0;
    m_scratchEditor.InitScratch(g_hRes);
}

extern void findReplaceFinish();
extern void regexCaptureFinish();

CMainWindow::~CMainWindow()
{
    DestroyIcon(m_hShieldIcon);
    DestroyIcon(m_hCapsLockIcon);
    DestroyIcon(m_hLexerIcon);
    DestroyIcon(m_hZoomIcon);
    DestroyIcon(m_hZoomDarkIcon);
    DestroyIcon(m_hEmptyIcon);
    DestroyIcon(hEditorconfigActive);
    DestroyIcon(hEditorconfigInactive);
}

// IUnknown method implementations.
ULONG STDMETHODCALLTYPE CMainWindow::AddRef()
{
    return InterlockedIncrement(&m_cRef);
}

ULONG STDMETHODCALLTYPE CMainWindow::Release()
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
        IStreamPtr   pStrm;
        std::wstring ribbonSettingsPath = CAppUtils::GetDataPath() + L"\\ribbonsettings";
        hr                              = SHCreateStreamOnFileEx(ribbonSettingsPath.c_str(), STGM_READ, 0, FALSE, nullptr, &pStrm);
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
    IStreamPtr   pStrm;
    std::wstring ribbonSettingsPath = CAppUtils::GetDataPath() + L"\\ribbonsettings";
    HRESULT      hr                 = SHCreateStreamOnFileEx(ribbonSettingsPath.c_str(),
                                        STGM_WRITE | STGM_CREATE, FILE_ATTRIBUTE_NORMAL, TRUE, nullptr, &pStrm);
    if (!CAppUtils::FailedShowMessage(hr))
    {
        LARGE_INTEGER  liPos{};
        ULARGE_INTEGER uliSize{};

        liPos.QuadPart   = 0;
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
        hr = m_pRibbon->GetHeight(&m_ribbonHeight);
        if (!CAppUtils::FailedShowMessage((hr)))
            ResizeChildWindows();
    }
    return hr;
}

//  PURPOSE: Called by the Ribbon framework for each command specified in markup, to allow
//           the host application to bind a command handler to that command.
STDMETHODIMP CMainWindow::OnCreateUICommand(
    UINT                nCmdID,
    UI_COMMANDTYPE      typeID,
    IUICommandHandler** ppCommandHandler)
{
    UNREFERENCED_PARAMETER(typeID);
    UNREFERENCED_PARAMETER(nCmdID);

    return QueryInterface(IID_PPV_ARGS(ppCommandHandler));
}

//  PURPOSE: Called when the state of a View (Ribbon is a view) changes, for example, created, destroyed, or resized.
STDMETHODIMP CMainWindow::OnViewChanged(
    UINT        viewId,
    UI_VIEWTYPE typeId,
    IUnknown*   pView,
    UI_VIEWVERB verb,
    INT         uReasonCode)
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
            default:
                break;
        }
    }

    return hr;
}

STDMETHODIMP CMainWindow::OnDestroyUICommand(
    UINT32             nCmdID,
    UI_COMMANDTYPE     typeID,
    IUICommandHandler* commandHandler)
{
    UNREFERENCED_PARAMETER(commandHandler);
    UNREFERENCED_PARAMETER(typeID);
    UNREFERENCED_PARAMETER(nCmdID);

    return E_NOTIMPL;
}

STDMETHODIMP CMainWindow::UpdateProperty(
    UINT               nCmdID,
    REFPROPERTYKEY     key,
    const PROPVARIANT* pPropVarCurrentValue,
    PROPVARIANT*       pPropVarNewValue)
{
    UNREFERENCED_PARAMETER(pPropVarCurrentValue);

    HRESULT hr    = E_NOTIMPL;
    HRESULT hrImg = E_NOTIMPL;
    if ((key == UI_PKEY_LargeImage) ||
        (key == UI_PKEY_SmallImage))
    {
        if (!IsWindows8OrGreater())
        {
            IUIImagePtr image = nullptr;
            const auto& imgIt = m_win7PNGWorkaroundData.find(nCmdID);
            if (imgIt != m_win7PNGWorkaroundData.end())
            {
                image = imgIt->second;
            }
            else
            {
                const auto& resourceData = CKeyboardShortcutHandler::Instance().GetResourceData();
                auto        whereAt      = std::find_if(resourceData.begin(), resourceData.end(),
                                            [&](const auto& item) { return (static_cast<UINT>(item.second) == nCmdID); });
                while (whereAt != resourceData.end())
                {
                    auto sID = whereAt->first;
                    sID += L"_LargeImages_RESID";

                    auto ttIDit = resourceData.find(sID);
                    if (ttIDit == resourceData.end())
                    {
                        sID = whereAt->first;
                        sID += L"_SmallImages_RESID";
                        ttIDit = resourceData.find(sID);
                    }
                    if (ttIDit != resourceData.end())
                    {
                        if (SUCCEEDED(CAppUtils::CreateImage(MAKEINTRESOURCE(ttIDit->second), image)))
                        {
                            m_win7PNGWorkaroundData[nCmdID] = image;
                            break;
                        }
                    }
                    whereAt = std::find_if(std::next(whereAt), resourceData.end(),
                                           [&](const auto& item) { return (static_cast<UINT>(item.second) == nCmdID); });
                }
            }
            if (image)
            {
                hrImg = UIInitPropertyFromImage(key, image, pPropVarNewValue);
            }
        }
    }
    ICommand* pCmd = CCommandHandler::Instance().GetCommand(nCmdID);
    if (pCmd)
    {
        hr                    = pCmd->IUICommandHandlerUpdateProperty(key, pPropVarCurrentValue, pPropVarNewValue);
        std::wstring shortKey = CKeyboardShortcutHandler::Instance().GetTooltipTitleForCommand(static_cast<WORD>(nCmdID));
        if (!shortKey.empty())
            g_pFramework->InvalidateUICommand(nCmdID, UI_INVALIDATIONS_PROPERTY, &UI_PKEY_TooltipTitle);
        if (key == UI_PKEY_TooltipTitle)
        {
            if (!shortKey.empty())
            {
                hr = UIInitPropertyFromString(UI_PKEY_TooltipTitle, shortKey.c_str(), pPropVarNewValue);
            }
        }
    }
    if ((hrImg != E_NOTIMPL) && FAILED(hr))
        return hrImg;
    return hr;
}

STDMETHODIMP CMainWindow::Execute(
    UINT                  nCmdID,
    UI_EXECUTIONVERB      verb,
    const PROPERTYKEY*    key,
    const PROPVARIANT*    pPropVarValue,
    IUISimplePropertySet* pCommandExecutionProperties)
{
    HRESULT hr = S_OK;

    ICommand* pCmd = CCommandHandler::Instance().GetCommand(nCmdID);
    if (pCmd)
    {
        hr = pCmd->IUICommandHandlerExecute(verb, key, pPropVarValue, pCommandExecutionProperties);
        if (hr == E_NOTIMPL)
        {
            hr = S_OK;
            if (verb == UI_EXECUTIONVERB_EXECUTE)
                DoCommand(nCmdID, 0);
        }
    }
    else
        DoCommand(nCmdID, 0);
    return hr;
}

void CMainWindow::About() const
{
    CAboutDlg dlg(*this);
    dlg.DoModal(g_hRes, IDD_ABOUTBOX, *this);
}

void CMainWindow::ShowCommandPalette()
{
    if (!m_commandPaletteDlg)
        m_commandPaletteDlg = std::make_unique<CCommandPaletteDlg>(*this);

    RECT rect{};
    GetClientRect(*this, &rect);
    RECT tabrc{};
    TabCtrl_GetItemRect(m_tabBar, 0, &tabrc);
    MapWindowPoints(m_tabBar, *this, reinterpret_cast<LPPOINT>(&tabrc), 2);
    const int  treeWidth = m_fileTreeVisible ? m_treeWidth : 0;
    const int  marginY   = CDPIAware::Instance().Scale(*this, 10);
    const int  marginX   = CDPIAware::Instance().Scale(*this, 30);
    const UINT flags     = SWP_NOOWNERZORDER | SWP_NOZORDER | SWP_SHOWWINDOW | SWP_NOCOPYBITS;

    m_commandPaletteDlg->ShowModeless(g_hRes, IDD_COMMANDPALETTE, *this, false);
    RECT thisRc{};
    GetClientRect(*m_commandPaletteDlg, &thisRc);
    RECT pos{};
    pos.left   = treeWidth + marginX + rect.left;
    pos.top    = tabrc.bottom + marginY + rect.top;
    pos.right  = rect.right - marginX;
    pos.bottom = pos.top + thisRc.bottom - thisRc.top;
    MapWindowPoints(*this, nullptr, reinterpret_cast<LPPOINT>(&pos), 2);

    SetWindowPos(*m_commandPaletteDlg, nullptr,
                 pos.left, pos.top, pos.right - pos.left, pos.bottom - pos.top,
                 flags);
    m_commandPaletteDlg->ClearFilterText();
}

std::wstring CMainWindow::GetAppName()
{
    auto title = LoadResourceWString(g_hRes, IDS_APP_TITLE);
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
    int          newCount = ++m_newCount;
    ResString    newRes(g_hRes, IDS_NEW_TABTITLE);
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
    WNDCLASSEX wcx = {sizeof(WNDCLASSEX)}; // Set size and zero out rest.

    //wcx.style = 0; - Don't use CS_HREDRAW or CS_VREDRAW with a Ribbon
    wcx.style                  = CS_DBLCLKS;
    wcx.lpfnWndProc            = stWinMsgHandler;
    wcx.hInstance              = hResource;
    const std::wstring clsName = GetWindowClassName();
    wcx.lpszClassName          = clsName.c_str();
    wcx.hIcon                  = LoadIcon(hResource, MAKEINTRESOURCE(IDI_BOWPAD));
    wcx.hbrBackground          = reinterpret_cast<HBRUSH>((COLOR_3DFACE + 1));
    wcx.hIconSm                = LoadIcon(wcx.hInstance, MAKEINTRESOURCE(IDI_BOWPAD));
    wcx.hCursor                = LoadCursor(nullptr, static_cast<LPTSTR>(IDC_SIZEWE)); // for resizing the tree control
    if (RegisterWindow(&wcx))
    {
        // create the window hidden, then after the window is created use the RestoreWindowPos
        // methods of the CIniSettings to show the window and move it to the saved position.
        // RestoreWindowPos uses the API SetWindowPlacement() which ensures the window is automatically
        // shown on a monitor and not outside (e.g. if the window position was saved on an external
        // monitor but that monitor is not connected now).
        if (CreateEx(WS_EX_ACCEPTFILES | WS_EX_NOINHERITLAYOUT, WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, nullptr))
        {
            SetFileTreeWidth(static_cast<int>(CIniSettings::Instance().GetInt64(L"View", L"FileTreeWidth", 200)));
            // hide the tab and status bar so they won't show right away when
            // restoring the window: those two show a white background until properly painted.
            // After restoring and showing the main window, ResizeChildControls() is called
            // which will show those controls again.
            ShowWindow(m_tabBar, SW_HIDE);
            ShowWindow(m_statusBar, SW_HIDE);
            std::wstring winPosKey = L"MainWindow_" + GetMonitorSetupHash();
            CIniSettings::Instance().RestoreWindowPos(winPosKey.c_str(), *this, 0);
            UpdateWindow(*this);
            m_editor.StartupDone();
            PostMessage(m_hwnd, WM_AFTERINIT, 0, 0);
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
            return DoCommand(wParam, lParam);
        case WM_SIZE:
            ResizeChildWindows();
            break;
        case WM_GETMINMAXINFO:
        {
            MINMAXINFO& mmi      = *reinterpret_cast<MINMAXINFO*>(lParam);
            mmi.ptMinTrackSize.x = 400;
            mmi.ptMinTrackSize.y = 100;
            return 0;
        }
        case WM_DRAWITEM:
        {
            const DRAWITEMSTRUCT& dis = *reinterpret_cast<DRAWITEMSTRUCT*>(lParam);
            if (dis.CtlType == ODT_TAB)
                return ::SendMessage(dis.hwndItem, WM_DRAWITEM, wParam, lParam);
        }
        break;
        case WM_MOUSEMOVE:
            return OnMouseMove(static_cast<UINT>(wParam), {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)});

        case WM_LBUTTONDOWN:
            return OnLButtonDown(static_cast<UINT>(wParam), {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)});

        case WM_LBUTTONUP:
            return OnLButtonUp(static_cast<UINT>(wParam), {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)});

        case WM_DROPFILES:
        {
            auto hDrop = reinterpret_cast<HDROP>(wParam);
            OnOutOfScope(
                DragFinish(hDrop););
            HandleDropFiles(hDrop);
        }
        break;
        case WM_COPYDATA:
        {
            if (lParam == 0)
                return 0;
            const COPYDATASTRUCT& cds = *reinterpret_cast<const COPYDATASTRUCT*>(lParam);
            switch (cds.dwData)
            {
                case CD_COMMAND_LINE:
                    HandleCopyDataCommandLine(cds);
                    break;
                case CD_COMMAND_MOVETAB:
                    return static_cast<LRESULT>(HandleCopyDataMoveTab(cds));
            }
        }
        break;
        case WM_MOVETODESKTOP:
            PostMessage(*this, WM_MOVETODESKTOP2, wParam, lParam);
            break;
        case WM_MOVETODESKTOP2:
        {
            IVirtualDesktopManager* pvdm = nullptr;
            if (SUCCEEDED(CoCreateInstance(CLSID_VirtualDesktopManager, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&pvdm))))
            {
                if (pvdm)
                {
                    GUID guid{};
                    if (SUCCEEDED(pvdm->GetWindowDesktopId(reinterpret_cast<HWND>(lParam), &guid)))
                        pvdm->MoveWindowToDesktop(*this, guid);
                    SetForegroundWindow(*this);
                }
                pvdm->Release();
            }
            return TRUE;
        }
        case WM_UPDATEAVAILABLE:
            CAppUtils::ShowUpdateAvailableDialog(*this);
            break;
        case WM_AFTERINIT:
            HandleAfterInit();
            break;
        case WM_NOTIFY:
        {
            LPNMHDR pnmHdr = reinterpret_cast<LPNMHDR>(lParam);
            APPVERIFY(pnmHdr != nullptr);
            if (pnmHdr == nullptr)
                return 0;
            const NMHDR& nmHdr = *pnmHdr;

            switch (nmHdr.code)
            {
                case TTN_GETDISPINFO:
                {
                    LPNMTTDISPINFO lpNmtdi = reinterpret_cast<LPNMTTDISPINFO>(lParam);
                    HandleGetDispInfo(static_cast<int>(nmHdr.idFrom), lpNmtdi);
                }
                break;
            }

            if (nmHdr.idFrom == reinterpret_cast<UINT_PTR>(&m_tabBar) || nmHdr.hwndFrom == m_tabBar)
            {
                return HandleTabBarEvents(nmHdr, wParam, lParam);
            }
            if (nmHdr.idFrom == reinterpret_cast<UINT_PTR>(&m_editor) || nmHdr.hwndFrom == m_editor)
            {
                return HandleEditorEvents(nmHdr, wParam, lParam);
            }
            if (nmHdr.idFrom == reinterpret_cast<UINT_PTR>(&m_fileTree) || nmHdr.hwndFrom == m_fileTree)
            {
                return HandleFileTreeEvents(nmHdr, wParam, lParam);
            }
        }
        break;

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
            SetTimer(*this, TIMER_UPDATECHECK, 200, nullptr);
        }
        break;
        case WM_CLIPBOARDUPDATE:
            HandleClipboardUpdate();
            break;
        case WM_TIMER:
            if (wParam >= COMMAND_TIMER_ID_START)
                CCommandHandler::Instance().OnTimer(static_cast<UINT>(wParam));
            if (wParam == TIMER_UPDATECHECK)
            {
                KillTimer(*this, TIMER_UPDATECHECK);
                CheckForOutsideChanges();
            }
            break;
        case WM_DESTROY:
            findReplaceFinish();
            regexCaptureFinish();
            g_pFramework->Destroy();
            PostQuitMessage(0);
            break;
        case WM_CLOSE:
            CCommandHandler::Instance().OnClose();
            // Close all tabs, don't leave any open even a blank one.
            if (CloseAllTabs(true))
            {
                std::wstring winPosKey = L"MainWindow_" + GetMonitorSetupHash();
                CIniSettings::Instance().SaveWindowPos(winPosKey.c_str(), *this);
                DestroyWindow(m_hwnd);
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
        case WM_SETCURSOR:
        {
            // Because the tab bar does not set the cursor if the mouse pointer
            // is over its 'extended' area (the area where no tab buttons are shown
            // on the right side up to the point where the scroll buttons would appear),
            // we have to set the cursor for that area here to an arrow.
            // The tab control resizes itself to the width of the area of the buttons!
            DWORD pos = GetMessagePos();
            POINT pt  = {GET_X_LPARAM(pos), GET_Y_LPARAM(pos)};
            RECT  tabRc{};
            TabCtrl_GetItemRect(m_tabBar, 0, &tabRc);
            MapWindowPoints(m_tabBar, nullptr, reinterpret_cast<LPPOINT>(&tabRc), 2);
            tabRc.top -= 3; // adjust for margin
            if ((pt.y <= tabRc.bottom) && (pt.y >= tabRc.top))
            {
                SetCursor(LoadCursor(nullptr, static_cast<LPTSTR>(IDC_ARROW)));
                return TRUE;
            }
            // Pass the message onto the system so the cursor adapts
            // such as changing to the appropriate sizing cursor when
            // over the window border.
            return DefWindowProc(hwnd, uMsg, wParam, lParam);
        }
        case WM_CANHIDECURSOR:
        {
            BOOL* result = reinterpret_cast<BOOL*>(lParam);
            *result      = m_inMenuLoop ? FALSE : TRUE;
        }
        break;
        case WM_MOUSEWHEEL:
        {
            POINT pt = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            RECT  rc;
            GetWindowRect(m_tabBar, &rc);
            RECT tabRc{};
            TabCtrl_GetItemRect(m_tabBar, 0, &tabRc);
            MapWindowPoints(m_tabBar, nullptr, reinterpret_cast<LPPOINT>(&tabRc), 2);
            rc.bottom = tabRc.bottom;
            if (PtInRect(&rc, pt))
            {
                if (SendMessage(m_tabBar, uMsg, wParam, lParam))
                    return 0;
            }
            GetWindowRect(m_fileTree, &rc);
            if (PtInRect(&rc, pt))
            {
                if (SendMessage(m_fileTree, uMsg, wParam, lParam))
                    return 0;
            }
            return DefWindowProc(hwnd, uMsg, wParam, lParam);
        }
        case WM_SETTINGCHANGE:
        case WM_SYSCOLORCHANGE:
        case WM_DPICHANGED:
            SendMessage(m_editor, uMsg, wParam, lParam);
            CDPIAware::Instance().Invalidate();
            CTheme::Instance().OnSysColorChanged();
            SetTheme(CTheme::Instance().IsDarkTheme());
            if (uMsg == WM_DPICHANGED)
            {
                RECT rc;
                GetWindowRect(*this, &rc);
                const RECT* rect = reinterpret_cast<RECT*>(lParam);
                SetWindowPos(*this, nullptr, rect->left, rect->top, rect->right - rect->left, rect->bottom - rect->top, SWP_NOZORDER | SWP_NOACTIVATE);
            }
            break;
        case WM_LBUTTONDBLCLK:
        {
            RECT rc{}, tabRc{};
            GetWindowRect(m_tabBar, &rc);
            TabCtrl_GetItemRect(m_tabBar, m_tabBar.GetItemCount() - 1LL, &tabRc);
            MapWindowPoints(m_tabBar, nullptr, reinterpret_cast<LPPOINT>(&tabRc), 2);
            if (tabRc.right > rc.right)
                break;
            rc.bottom = tabRc.bottom;
            rc.left   = tabRc.right;
            POINT pt{};
            pt.x = GET_X_LPARAM(lParam);
            pt.y = GET_Y_LPARAM(lParam);
            MapWindowPoints(*this, nullptr, &pt, 1);

            if (PtInRect(&rc, pt))
                OpenNewTab();
        }
        break;
        case WM_SCICHAR:
        {
            auto ret = m_autoCompleter.HandleChar(wParam, lParam);
            return ret ? 1 : 0;
        }
        default:
            return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }

    return 0;
}

LRESULT CMainWindow::HandleTabBarEvents(const NMHDR& nmHdr, WPARAM /*wParam*/, LPARAM lParam)
{
    TBHDR tbh = {};
    if (nmHdr.idFrom != reinterpret_cast<UINT_PTR>(&m_tabBar))
    {
        // Events that are not from CTabBar might be
        // lower level and of type HMHDR, not TBHDR
        // and therefore missing tabOrigin.
        // In case they are NMHDR, map them to TBHDR and set
        // an obviously bogus value for tabOrigin so it isn't
        // used by mistake.
        // We need a TBHDR type to notify commands with.
        tbh.hdr       = nmHdr;
        tbh.tabOrigin = ~0; // Obviously bogus value.
        lParam        = reinterpret_cast<LPARAM>(&tbh);
    }

    TBHDR*       ptbHdr = reinterpret_cast<TBHDR*>(lParam);
    const TBHDR& tbHdr  = *ptbHdr;
    assert(tbHdr.hdr.code == nmHdr.code);

    OnOutOfScope(CCommandHandler::Instance().TabNotify(ptbHdr));

    switch (nmHdr.code)
    {
        case TCN_GETCOLOR:
        {
            if (tbHdr.tabOrigin >= 0 && tbHdr.tabOrigin < m_tabBar.GetItemCount())
            {
                auto docId = m_tabBar.GetIDFromIndex(tbHdr.tabOrigin);
                APPVERIFY(docId.IsValid());
                if (m_docManager.HasDocumentID(docId))
                {
                    auto clr = GetColorForDocument(docId);
                    return CTheme::Instance().GetThemeColor(clr, true);
                }
            }
            else
                APPVERIFY(false);
            break;
        }
        case TCN_SELCHANGE:
            HandleTabChange(nmHdr);
            InvalidateRect(m_fileTree, nullptr, TRUE);
            break;
        case TCN_SELCHANGING:
            HandleTabChanging(nmHdr);
            break;
        case TCN_TABDELETE:
            HandleTabDelete(tbHdr);
            break;
        case TCN_TABDROPPEDOUTSIDE:
        {
            DWORD pos = GetMessagePos();
            POINT pt{GET_X_LPARAM(pos), GET_Y_LPARAM(pos)};
            HandleTabDroppedOutside(ptbHdr->tabOrigin, pt);
        }
        break;
        case TCN_GETDROPICON:
        {
            DWORD pos = GetMessagePos();
            POINT pt{GET_X_LPARAM(pos), GET_Y_LPARAM(pos)};
            auto  hPtWnd = WindowFromPoint(pt);
            if (hPtWnd == m_fileTree)
            {
                auto        docID = m_tabBar.GetIDFromIndex(ptbHdr->tabOrigin);
                const auto& doc   = m_docManager.GetDocumentFromID(docID);
                if (!doc.m_path.empty())
                {
                    if (GetKeyState(VK_CONTROL) & 0x8000)
                        return reinterpret_cast<LRESULT>(::LoadCursor(hResource, MAKEINTRESOURCE(IDC_DRAG_COPYFILE)));
                    return reinterpret_cast<LRESULT>(::LoadCursor(hResource, MAKEINTRESOURCE(IDC_DRAG_MOVEFILE)));
                }
            }
        }
        break;
    }
    return 0;
}

void CMainWindow::ShowTablistDropdown(HWND hWnd, int offsetX, int offsetY)
{
    if (hWnd)
    {
        RECT rc{};
        GetWindowRect(hWnd, &rc);
        auto hMenu = CreatePopupMenu();
        if (hMenu)
        {
            OnOutOfScope(DestroyMenu(hMenu));
            auto                                       currentIndex = m_tabBar.GetCurrentTabIndex();
            std::multimap<std::wstring, int, ci_lessW> prepList;
            int                                        tabCount = m_tabBar.GetItemCount();
            for (int i = 0; i < tabCount; ++i)
            {
                prepList.insert(std::make_pair(m_tabBar.GetTitle(i), i));
            }
            std::multimap<std::wstring, int, ci_lessW> tablist;
            for (auto& [tabTitle, tabIndex] : prepList)
            {
                auto count = std::count_if(prepList.begin(), prepList.end(), [&](const auto& item) -> bool {
                    return item.first == tabTitle;
                });
                if (count > 1)
                {
                    wchar_t     pathBuf[30] = {0};
                    const auto& doc         = m_docManager.GetDocumentFromID(m_tabBar.GetIDFromIndex(tabIndex));
                    PathCompactPathEx(pathBuf, CPathUtils::GetParentDirectory(doc.m_path).c_str(), _countof(pathBuf), 0);
                    auto text = CStringUtils::Format(L"%s (%s)",
                                                     tabTitle.c_str(),
                                                     pathBuf);
                    tablist.insert(std::make_pair(text, tabIndex + 1));
                }
                else
                    tablist.insert(std::make_pair(tabTitle, tabIndex + 1));
            }

            for (auto& [tabTitle, tabIndex] : tablist)
            {
                if (tabIndex == (currentIndex + 1))
                    AppendMenu(hMenu, MF_STRING | MF_CHECKED, tabIndex, tabTitle.c_str());
                else
                    AppendMenu(hMenu, MF_STRING, tabIndex, tabTitle.c_str());
            }
            TPMPARAMS tpm{};
            tpm.cbSize    = sizeof(TPMPARAMS);
            tpm.rcExclude = rc;
            auto tab      = TrackPopupMenuEx(hMenu, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_LEFTBUTTON | TPM_VERTICAL, rc.left + offsetX, rc.bottom + offsetY, *this, &tpm);
            if (tab > 0)
            {
                --tab;
                if (tab != currentIndex)
                    m_tabBar.ActivateAt(tab);
            }
        }
    }
}

LRESULT CMainWindow::HandleEditorEvents(const NMHDR& nmHdr, WPARAM wParam, LPARAM lParam)
{
    if (nmHdr.code == NM_COOLSB_CUSTOMDRAW)
        return m_editor.HandleScrollbarCustomDraw(wParam, reinterpret_cast<NMCSBCUSTOMDRAW*>(lParam));
    if (nmHdr.code == NM_COOLSB_CLICK)
    {
        auto pNmcb = reinterpret_cast<NMCOOLBUTMSG*>(lParam);
        return DoCommand(pNmcb->uCmdId, 0);
    }
    SCNotification*       pScn = reinterpret_cast<SCNotification*>(lParam);
    const SCNotification& scn  = *pScn;

    m_editor.ReflectEvents(pScn);
    m_autoCompleter.HandleScintillaEvents(pScn);

    CCommandHandler::Instance().ScintillaNotify(pScn);
    switch (scn.nmhdr.code)
    {
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
            m_dwellStartPos = scn.position;
            HandleDwellStart(scn, true);
            break;
        case SCN_DWELLEND:
            if ((scn.position >= 0) && m_editor.Call(SCI_CALLTIPACTIVE))
                HandleDwellStart(scn, false);
            else
            {
                m_editor.Call(SCI_CALLTIPCANCEL);
                m_custToolTip.HideTip();
                m_dwellStartPos = -1;
            }
            break;
        case SCN_CALLTIPCLICK:
            OpenUrlAtPos(m_dwellStartPos);
            break;
        case SCN_ZOOM:
            m_editor.UpdateLineNumberWidth();
            UpdateStatusBar(false);
            break;
    }
    return 0;
}

LRESULT CMainWindow::HandleFileTreeEvents(const NMHDR& nmHdr, WPARAM /*wParam*/, LPARAM lParam)
{
    LPNMTREEVIEW pNmTv = reinterpret_cast<LPNMTREEVIEWW>(lParam);
    switch (nmHdr.code)
    {
        case NM_RETURN:
        {
            bool isDir = false;
            bool isDot = false;
            auto path  = m_fileTree.GetPathForSelItem(&isDir, &isDot);
            if (!path.empty())
            {
                HandleTreePath(path, isDir, isDot);
                return TRUE;
            }
        }
        break;
        case NM_DBLCLK:
        {
            bool isDir = false;
            bool isDot = false;
            auto path  = m_fileTree.GetPathForHitItem(&isDir, &isDot);
            if (!path.empty())
            {
                HandleTreePath(path, isDir, isDot);
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
            SendMessage(m_fileTree, WM_CONTEXTMENU, reinterpret_cast<WPARAM>(m_hwnd), GetMessagePos());
        }
        break;
        case TVN_ITEMEXPANDING:
        {
            if ((pNmTv->action & TVE_EXPAND) != 0)
                m_fileTree.ExpandItem(pNmTv->itemNew.hItem);
        }
        break;
        case NM_CUSTOMDRAW:
        {
            if (CTheme::Instance().IsDarkTheme())
            {
                LPNMTVCUSTOMDRAW lpNMCustomDraw = reinterpret_cast<LPNMTVCUSTOMDRAW>(lParam);
                // only do custom drawing when in dark theme
                switch (lpNMCustomDraw->nmcd.dwDrawStage)
                {
                    case CDDS_PREPAINT:
                        return CDRF_NOTIFYITEMDRAW;
                    case CDDS_ITEMPREPAINT:
                    {
                        if (IsWindows8OrGreater())
                        {
                            lpNMCustomDraw->clrText   = CTheme::Instance().GetThemeColor(RGB(0, 0, 0), true);
                            lpNMCustomDraw->clrTextBk = CTheme::Instance().GetThemeColor(RGB(255, 255, 255), true);
                        }
                        else
                        {
                            if ((lpNMCustomDraw->nmcd.uItemState & CDIS_SELECTED) != 0 &&
                                (lpNMCustomDraw->nmcd.uItemState & CDIS_FOCUS) == 0)
                            {
                                lpNMCustomDraw->clrTextBk = CTheme::Instance().GetThemeColor(RGB(255, 255, 255), true);
                                lpNMCustomDraw->clrText   = RGB(128, 128, 128);
                            }
                        }

                        return CDRF_DODEFAULT;
                    }
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

void CMainWindow::HandleTreePath(const std::wstring& path, bool isDir, bool isDot)
{
    if (isDir)
    {
        if (isDot)
        {
            m_fileTree.SetPath(path);
        }
    }
    else
    {
        bool         control   = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
        unsigned int openFlags = OpenFlags::AddToMRU;
        if (control)
            openFlags |= OpenFlags::OpenIntoActiveTab;
        OpenFile(path, openFlags);
    }
}

std::vector<std::wstring> CMainWindow::GetFileListFromGlobPath(const std::wstring& path)
{
    std::vector<std::wstring> results;
    if (path.find_first_of(L"*?") == std::string::npos)
    {
        results.push_back(path);
        return results;
    }
    CDirFileEnum enumerator(path);
    bool         bIsDir = false;
    std::wstring enumPath;
    while (enumerator.NextFile(enumPath, &bIsDir, false))
    {
        if (!bIsDir)
        {
            results.push_back(enumPath);
        }
    }

    return results;
}

void CMainWindow::HandleStatusBar(WPARAM wParam, LPARAM lParam)
{
    switch (wParam)
    {
        case WM_LBUTTONUP:
        {
            switch (lParam)
            {
                case STATUSBAR_TABS:
                {
                    auto  pos = GetMessagePos();
                    POINT pt  = {GET_X_LPARAM(pos), GET_Y_LPARAM(pos)};
                    ScreenToClient(m_statusBar, &pt);
                    ShowTablistDropdown(m_statusBar, pt.x, pt.y);
                }
                break;
            }
        }
        break;
        case WM_CONTEXTMENU:
        {
            switch (lParam)
            {
                case STATUSBAR_ZOOM:
                    HandleStatusBarZoom();
                    break;
                case STATUSBAR_EOL_FORMAT:
                    HandleStatusBarEOLFormat();
                    break;
            }
            break;
        }
        case WM_LBUTTONDBLCLK:
        {
            switch (lParam)
            {
                case STATUSBAR_TABSPACE:
                    DoCommand(cmdUseTabs, 0);
                    break;
                case STATUSBAR_R2L:
                {
                    auto biDi = m_editor.Call(SCI_GETBIDIRECTIONAL);
                    m_editor.SetReadDirection(biDi == SC_BIDIRECTIONAL_R2L ? ReadDirection::Disabled : ReadDirection::R2L);
                    auto& doc     = m_docManager.GetModDocumentFromID(m_tabBar.GetCurrentTabId());
                    doc.m_readDir = static_cast<ReadDirection>(m_editor.Call(SCI_GETBIDIRECTIONAL));
                }
                break;
                case STATUSBAR_TYPING_MODE:
                    m_editor.Call(SCI_EDITTOGGLEOVERTYPE);
                    break;
                case STATUSBAR_ZOOM:
                    m_editor.Call(SCI_SETZOOM, 0);
                    break;
                case STATUSBAR_EDITORCONFIG:
                {
                    auto& doc                 = m_docManager.GetModDocumentFromID(m_tabBar.GetCurrentTabId());
                    auto  editorConfigEnabled = CEditorConfigHandler::Instance().IsEnabled(doc.m_path);
                    CEditorConfigHandler::Instance().EnableForPath(doc.m_path, !editorConfigEnabled);
                    if (!editorConfigEnabled)
                        CEditorConfigHandler::Instance().ApplySettingsForPath(doc.m_path, &m_editor, doc, true);
                }
                break;
            }
            UpdateStatusBar(true);
        }
        break;
    }
}

void CMainWindow::HandleStatusBarEOLFormat()
{
    DWORD msgPos = GetMessagePos();
    int   xPos   = GET_X_LPARAM(msgPos);
    int   yPos   = GET_Y_LPARAM(msgPos);

    HMENU hPopup = CreatePopupMenu();
    if (!hPopup)
        return;
    OnOutOfScope(
        DestroyMenu(hPopup););
    int                    currentEolMode   = static_cast<int>(m_editor.Call(SCI_GETEOLMODE));
    EOLFormat              currentEolFormat = toEolFormat(currentEolMode);
    static const EOLFormat options[]        = {EOLFormat::Win_Format, EOLFormat::Mac_Format, EOLFormat::Unix_Format};
    const size_t           numOptions       = std::size(options);
    for (size_t i = 0; i < numOptions; ++i)
    {
        std::wstring eolName       = getEolFormatDescription(options[i]);
        UINT         menuItemFlags = MF_STRING;
        if (options[i] == currentEolFormat)
            menuItemFlags |= MF_CHECKED | MF_DISABLED;
        AppendMenu(hPopup, menuItemFlags, i + 1, eolName.c_str());
    }
    auto result = TrackPopupMenu(hPopup, TPM_RIGHTALIGN | TPM_TOPALIGN | TPM_RETURNCMD, xPos, yPos, 0, *this, nullptr);
    if (result != FALSE)
    {
        size_t optionIndex       = static_cast<size_t>(result) - 1;
        auto   selectedEolFormat = options[optionIndex];
        if (selectedEolFormat != currentEolFormat)
        {
            auto selectedEolMode = toEolMode(selectedEolFormat);
            m_editor.Call(SCI_SETEOLMODE, selectedEolMode);
            m_editor.Call(SCI_CONVERTEOLS, selectedEolMode);
            auto id = m_tabBar.GetCurrentTabId();
            if (m_docManager.HasDocumentID(id))
            {
                auto& doc    = m_docManager.GetModDocumentFromID(id);
                doc.m_format = selectedEolFormat;
                UpdateStatusBar(true);
            }
        }
    }
}

void CMainWindow::HandleStatusBarZoom()
{
    DWORD msgPos = GetMessagePos();
    int   xPos   = GET_X_LPARAM(msgPos);
    int   yPos   = GET_Y_LPARAM(msgPos);

    HMENU hPopup = CreatePopupMenu();
    if (hPopup)
    {
        OnOutOfScope(
            DestroyMenu(hPopup););
        AppendMenu(hPopup, MF_STRING, 25, L"25%");
        AppendMenu(hPopup, MF_STRING, 50, L"50%");
        AppendMenu(hPopup, MF_STRING, 75, L"75%");
        AppendMenu(hPopup, MF_STRING, 100, L"100%");
        AppendMenu(hPopup, MF_STRING, 125, L"125%");
        AppendMenu(hPopup, MF_STRING, 150, L"150%");
        AppendMenu(hPopup, MF_STRING, 175, L"175%");
        AppendMenu(hPopup, MF_STRING, 200, L"200%");

        // Note the font point size is a factor in what determines the zoom percentage the user actually gets.
        // So the values above are estimates. However, the status bar shows the true/exact setting that resulted.
        // This might be necessary as the user can adjust the zoom through ctrl + mouse wheel to get finer/other settings
        // than we offer here. We could round and lie to make the status bar and estimates match (meh) or
        // we could show real size in the menu to be consistent (arguably better), but we don't take either approach yet,
        // and opt to show the user nice instantly relate-able percentages that we then don't quite honor precisely in practice.
        auto cmd = TrackPopupMenu(hPopup, TPM_RIGHTALIGN | TPM_TOPALIGN | TPM_RETURNCMD, xPos, yPos, 0, *this, nullptr);
        if (cmd != 0)
            SetZoomPC(cmd);
    }
}

LRESULT CMainWindow::DoCommand(WPARAM wParam, LPARAM lParam)
{
    auto id = LOWORD(wParam);
    switch (id)
    {
        case cmdExit:
            PostMessage(m_hwnd, WM_CLOSE, 0, 0);
            break;
        case cmdNew:
            OpenNewTab();
            break;
        case cmdClose:
            CloseTab(m_tabBar.GetCurrentTabIndex());
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
        case cmdTabListDropdownMenu:
            ShowTablistDropdown(reinterpret_cast<HWND>(lParam), 0, 0);
            break;
        case cmdAbout:
            About();
            break;
        case cmdCommandPalette:
            ShowCommandPalette();
            break;
        case cmdSettings:
        {
            CSettingsDlg dlg;
            dlg.DoModal(g_hRes, IDD_SETTINGS, *this);
        }
        break;
        case cmdAutocompleteConfig:
        {
            CAutoCompleteConfigDlg dlg(this);
            dlg.DoModal(g_hRes, IDD_CODESNIPPETS, *this);
        }
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
    m_fileTree.Init(hResource, *this);
    CCommandHandler::Instance().AddCommand(&m_fileTree);
    CCommandHandler::Instance().AddCommand(cmdNew);
    CCommandHandler::Instance().AddCommand(cmdClose);
    CCommandHandler::Instance().AddCommand(cmdCloseAll);
    CCommandHandler::Instance().AddCommand(cmdCloseAllButThis);
    CCommandHandler::Instance().AddCommand(cmdCopyPath);
    CCommandHandler::Instance().AddCommand(cmdCopyName);
    CCommandHandler::Instance().AddCommand(cmdCopyDir);
    CCommandHandler::Instance().AddCommand(cmdExplore);
    CCommandHandler::Instance().AddCommand(cmdExploreProperties);
    CCommandHandler::Instance().AddCommand(cmdPasteHistory);
    CCommandHandler::Instance().AddCommand(cmdTabListDropdownMenu);
    CCommandHandler::Instance().AddCommand(cmdAbout);

    m_editor.Init(hResource, *this);
    m_autoCompleter.Init();
    m_statusBar.Init(*this, true);
    m_statusBar.SetHandlerFunc([](COLORREF clr) -> COLORREF {
        return CTheme::Instance().GetThemeColor(clr);
    });
    m_tabBar.Init(hResource, *this);
    m_tablistBtn.Init(hResource, *this, reinterpret_cast<HMENU>(cmdTabListDropdownMenu));
    m_tablistBtn.SetText(L"\u25BC");
    m_newTabBtn.Init(hResource, *this, reinterpret_cast<HMENU>(cmdNew));
    if (IsWindows10OrGreater())
        m_newTabBtn.SetText(L"\u2795");
    else
        m_newTabBtn.SetText(L"+");
    m_closeTabBtn.Init(hResource, *this, reinterpret_cast<HMENU>(cmdClose));
    if (IsWindows10OrGreater())
        m_closeTabBtn.SetText(L"\u274C");
    else
        m_closeTabBtn.SetText(L"X");
    m_closeTabBtn.SetTextColor(RGB(255, 0, 0));
    m_progressBar.Init(hResource, *this);
    m_custToolTip.Init(m_editor);
    // Note DestroyIcon not technically needed here but we may as well leave in
    // in case someone changes things to load a non static resource.
    HIMAGELIST hImgList = ImageList_Create(13, 13, ILC_COLOR32 | ILC_MASK, 0, 3);
    m_tabBarImageList.reset(hImgList);
    HICON hIcon = ::LoadIcon(hResource, MAKEINTRESOURCE(IDI_SAVED_ICON));
    assert(hIcon != nullptr);
    ImageList_AddIcon(hImgList, hIcon);
    DestroyIcon(hIcon);
    hIcon = ::LoadIcon(hResource, MAKEINTRESOURCE(IDI_UNSAVED_ICON));
    assert(hIcon != nullptr);
    ImageList_AddIcon(hImgList, hIcon);
    DestroyIcon(hIcon);
    hIcon = ::LoadIcon(hResource, MAKEINTRESOURCE(IDI_READONLY_ICON));
    assert(hIcon != nullptr);
    ImageList_AddIcon(hImgList, hIcon);
    DestroyIcon(hIcon);
    m_tabBar.SetImageList(hImgList);

    if (!CreateRibbon())
        return false;

    // Tell UAC that lower integrity processes are allowed to send WM_COPYDATA messages to this process (or window)
    HMODULE hDll = GetModuleHandle(TEXT("user32.dll"));
    if (hDll)
    {
        // first try ChangeWindowMessageFilterEx, if it's not available (i.e., running on Vista), then
        // try ChangeWindowMessageFilter.
        using MESSAGEFILTERFUNCEX = BOOL(WINAPI*)(HWND hWnd, UINT message, DWORD action, VOID * pChangeFilterStruct);
        MESSAGEFILTERFUNCEX func  = reinterpret_cast<MESSAGEFILTERFUNCEX>(GetProcAddress(hDll, "ChangeWindowMessageFilterEx"));

        constexpr UINT WM_COPYGLOBALDATA = 0x0049;
        if (func)
        {
            // note:
            // this enabled dropping files from e.g. explorer, but only
            // on the main window, status bar, tab bar and the file tree.
            // it does NOT work on the scintilla window because scintilla
            // calls RegisterDragDrop() - and OLE drag'n'drop does not work
            // between different elevation levels! Unfortunately, once
            // a window calls RegisterDragDrop() the WM_DROPFILES message also
            // won't work anymore...
            func(*this, WM_COPYDATA, MSGFLT_ALLOW, nullptr);
            func(*this, WM_COPYGLOBALDATA, MSGFLT_ALLOW, nullptr);
            func(*this, WM_DROPFILES, MSGFLT_ALLOW, nullptr);
        }
        else
        {
            ChangeWindowMessageFilter(WM_COPYDATA, MSGFLT_ADD);
            ChangeWindowMessageFilter(WM_COPYGLOBALDATA, MSGFLT_ADD);
            ChangeWindowMessageFilter(WM_DROPFILES, MSGFLT_ADD);
        }
    }

    GetRibbonColors(m_normalThemeText, m_normalThemeBack, m_normalThemeHigh);
    SetTheme(CTheme::Instance().IsDarkTheme());
    CTheme::Instance().RegisterThemeChangeCallback(
        [this]() {
            SetTheme(CTheme::Instance().IsDarkTheme());
        });

    CCommandHandler::Instance().Init(this);
    CKeyboardShortcutHandler::Instance().UpdateTooltips();
    g_pFramework->FlushPendingInvalidations();
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
    hr = g_pFramework->LoadUI(g_hRes, L"BOWPAD_RIBBON");
    if (CAppUtils::FailedShowMessage(hr))
        return false;
    return true;
}

void CMainWindow::HandleCreate(HWND hwnd)
{
    m_hwnd = hwnd;
    Initialize();

    if (SysInfo::Instance().IsUACEnabled() && SysInfo::Instance().IsElevated())
    {
        // in case we're running elevated, use a BowPad icon with a shield
        HICON hIcon = static_cast<HICON>(::LoadImage(hResource, MAKEINTRESOURCE(IDI_BOWPAD_ELEVATED), IMAGE_ICON, 0, 0, LR_DEFAULTSIZE | LR_SHARED));
        ::SendMessage(m_hwnd, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(hIcon));
        ::SendMessage(m_hwnd, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(hIcon));
    }

    if (SysInfo::Instance().IsUACEnabled() && SysInfo::Instance().IsElevated())
    {
        ITaskbarList3Ptr pTaskBarInterface;
        HRESULT          hr = CoCreateInstance(CLSID_TaskbarList, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&pTaskBarInterface));
        if (SUCCEEDED(hr))
        {
            pTaskBarInterface->SetOverlayIcon(m_hwnd, m_hShieldIcon, L"elevated");
        }
    }
}

void CMainWindow::HandleAfterInit()
{
    UpdateWindow(*this);

    CCommandHandler::Instance().BeforeLoad();

    if ((m_pathsToOpen.size() == 1) && (PathIsDirectory(m_pathsToOpen.begin()->first.c_str())))
    {
        if (!m_fileTree.GetPath().empty()) // File tree not empty: create a new empty tab first.
            OpenNewTab();
        m_fileTree.SetPath(m_pathsToOpen.begin()->first);
        ShowFileTree(true);
    }
    else if (m_pathsToOpen.size() > 0)
    {
        unsigned int openFlags = OpenFlags::AskToCreateIfMissing;
        if (m_bPathsToOpenMRU)
            openFlags |= OpenFlags::AddToMRU;
        BlockAllUIUpdates(true);
        OnOutOfScope(BlockAllUIUpdates(false););

        ShowProgressCtrl(static_cast<UINT>(CIniSettings::Instance().GetInt64(L"View", L"progressdelay", 1000)));
        OnOutOfScope(HideProgressCtrl());

        int fileCounter = 0;
        for (const auto& [path, line] : m_pathsToOpen)
        {
            ++fileCounter;
            SetProgress(fileCounter, static_cast<DWORD>(m_pathsToOpen.size()));
            if (OpenFile(path, openFlags) >= 0)
            {
                if (line != static_cast<size_t>(-1))
                    GoToLine(line);
            }
        }
    }
    m_pathsToOpen.clear();
    m_bPathsToOpenMRU = true;
    if (!m_elevatePath.empty())
    {
        ElevatedSave(m_elevatePath, m_elevateSavePath, m_initLine);
        m_elevatePath.clear();
        m_elevateSavePath.clear();
    }

    if (!m_tabMovePath.empty())
    {
        TabMove(m_tabMovePath, m_tabMoveSavePath, m_tabMoveMod, m_initLine, m_tabMoveTitle);
        m_tabMovePath.clear();
        m_tabMoveSavePath.clear();
        m_tabMoveTitle.clear();
    }
    EnsureAtLeastOneTab();

    std::thread([=] {
        bool bNewer = CAppUtils::CheckForUpdate(false);
        if (bNewer)
            PostMessage(m_hwnd, WM_UPDATEAVAILABLE, 0, 0);
    }).detach();

    CCommandHandler::Instance().AfterInit();
}

void CMainWindow::ResizeChildWindows()
{
    RECT rect;
    GetClientRect(*this, &rect);
    // if the main window is not visible (yet) or the UI is blocked,
    // then don't resize the child controls.
    // as soon as the UI is unblocked, ResizeChildWindows() is called
    // again.
    if (!IsRectEmpty(&rect) && IsWindowVisible(*this))
    {
        const UINT flags       = SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_NOZORDER | SWP_SHOWWINDOW | SWP_NOCOPYBITS;
        const UINT noShowFlags = SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_NOZORDER | SWP_NOCOPYBITS;

        // note: if there are no tab items in the tab bar, the call to
        // TabCtrl_GetItemRect will return FALSE but the tabrc rect still has
        // the height data filled in.
        // And we only use the height, so it makes no difference.
        RECT tabrc{};
        TabCtrl_GetItemRect(m_tabBar, 0, &tabrc);
        MapWindowPoints(m_tabBar, *this, reinterpret_cast<LPPOINT>(&tabrc), 2);
        const int tbHeight    = tabrc.bottom - tabrc.top;
        const int tabBtnWidth = tbHeight + CDPIAware::Instance().Scale(*this, 2);
        const int treeWidth   = m_fileTreeVisible ? m_treeWidth : 0;
        const int mainWidth   = rect.right - rect.left;
        const int btnMargin   = CDPIAware::Instance().Scale(*this, 2);

        HDWP hDwp = BeginDeferWindowPos(7);
        DeferWindowPos(hDwp, m_statusBar, nullptr, rect.left, rect.bottom - m_statusBar.GetHeight(), mainWidth, m_statusBar.GetHeight(), flags);
        DeferWindowPos(hDwp, m_tabBar, nullptr, treeWidth + rect.left, rect.top + m_ribbonHeight, mainWidth - treeWidth - (3 * (tabBtnWidth + btnMargin)), rect.bottom - rect.top, flags);
        DeferWindowPos(hDwp, m_tablistBtn, nullptr, mainWidth - (3 * (tabBtnWidth + btnMargin)), rect.top + m_ribbonHeight, tabBtnWidth, tbHeight, flags);
        DeferWindowPos(hDwp, m_newTabBtn, nullptr, mainWidth - (2 * (tabBtnWidth + btnMargin)), rect.top + m_ribbonHeight, tabBtnWidth, tbHeight, flags);
        DeferWindowPos(hDwp, m_closeTabBtn, nullptr, mainWidth - tabBtnWidth - btnMargin, rect.top + m_ribbonHeight, tabBtnWidth, tbHeight, flags);
        DeferWindowPos(hDwp, m_editor, nullptr, rect.left + treeWidth, rect.top + m_ribbonHeight + tbHeight, mainWidth - treeWidth, rect.bottom - (m_ribbonHeight + tbHeight) - m_statusBar.GetHeight(), flags);
        DeferWindowPos(hDwp, m_fileTree, nullptr, rect.left, rect.top + m_ribbonHeight, treeWidth ? treeWidth - CDPIAware::Instance().Scale(*this, 5) : 0, rect.bottom - (m_ribbonHeight)-m_statusBar.GetHeight(), m_fileTreeVisible ? flags : noShowFlags);
        EndDeferWindowPos(hDwp);
    }
}

void CMainWindow::EnsureNewLineAtEnd(const CDocument& doc) const
{
    size_t endPos = m_editor.Call(SCI_GETLENGTH);
    char   c      = static_cast<char>(m_editor.Call(SCI_GETCHARAT, endPos - 1));
    if ((c != '\r') && (c != '\n'))
    {
        switch (doc.m_format)
        {
            case EOLFormat::Win_Format:
                m_editor.AppendText(2, "\r\n");
                break;
            case EOLFormat::Mac_Format:
                m_editor.AppendText(1, "\r");
                break;
            case EOLFormat::Unix_Format:
            default:
                m_editor.AppendText(1, "\n");
                break;
        }
    }
}

bool CMainWindow::SaveCurrentTab(bool bSaveAs /* = false */)
{
    auto docID = m_tabBar.GetCurrentTabId();
    return SaveDoc(docID, bSaveAs);
}

bool CMainWindow::SaveDoc(DocID docID, bool bSaveAs)
{
    if (!docID.IsValid())
        return false;
    if (!m_docManager.HasDocumentID(docID))
        return false;

    auto& doc = m_docManager.GetModDocumentFromID(docID);
    if (!bSaveAs && !doc.m_bIsDirty && !doc.m_bNeedsSaving)
        return false;

    auto isActiveTab    = docID == m_tabBar.GetCurrentTabId();
    bool updateFileTree = false;
    if (doc.m_path.empty() || bSaveAs || doc.m_bDoSaveAs)
    {
        bSaveAs = true;
        std::wstring filePath;

        std::wstring title    = GetAppName();
        std::wstring fileName = m_tabBar.GetTitle(m_tabBar.GetIndexFromID(docID));
        title += L" - ";
        title += fileName;
        // Do not change doc.m_path until the user determines to save
        if (doc.m_path.empty())
        {
            if (m_fileTree.GetPath().empty())
                filePath = fileName;
            else
                filePath = m_fileTree.GetPath() + L"\\" + fileName;
        }
        else
            filePath = doc.m_path;

        UINT         extIndex = 0;
        std::wstring ext;
        // if there's a lexer active, get the default extension and default filet type index in filter types
        CLexStyles::Instance().GetDefaultExtensionForLanguage(doc.GetLanguage(), ext, extIndex);

        if (!ShowFileSaveDialog(*this, title, ext, extIndex, filePath))
            return false;
        doc.m_path = filePath;
        CMRU::Instance().AddPath(doc.m_path);
        if ((isActiveTab && m_fileTree.GetPath().empty()) || bSaveAs)
        {
            updateFileTree = true;
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
        if (!m_docManager.SaveFile(*this, doc, bTabMoved))
        {
            return false;
        }
        if (doc.m_saveCallback)
            doc.m_saveCallback();

        if (CPathUtils::PathCompare(CIniSettings::Instance().GetIniPath(), doc.m_path))
            CIniSettings::Instance().Reload();

        doc.m_bIsDirty     = false;
        doc.m_bNeedsSaving = false;
        m_docManager.UpdateFileTime(doc, false);
        if (bSaveAs)
        {
            const auto& lang = CLexStyles::Instance().GetLanguageForDocument(doc, m_scratchEditor);
            if (isActiveTab)
                m_editor.SetupLexerForLang(lang);
            doc.SetLanguage(lang);
        }
        // Update tab so the various states are updated (readonly, modified, ...)
        UpdateTab(docID);
        if (isActiveTab)
        {
            std::wstring sFileName = CPathUtils::GetFileName(doc.m_path);
            m_tabBar.SetCurrentTitle(sFileName.c_str());
            UpdateCaptionBar();
            UpdateStatusBar(true);
            m_editor.Call(SCI_SETSAVEPOINT);
        }
        if (updateFileTree)
        {
            m_fileTree.SetPath(CPathUtils::GetParentDirectory(doc.m_path), bSaveAs);
            ResizeChildWindows();
        }
        CCommandHandler::Instance().OnDocumentSave(docID, bSaveAs);
    }
    return true;
}

bool CMainWindow::SaveDoc(DocID docID, const std::wstring& path)
{
    if (!docID.IsValid())
        return false;
    if (!m_docManager.HasDocumentID(docID))
        return false;
    if (path.empty())
        return false;

    auto& doc = m_docManager.GetModDocumentFromID(docID);

    if (!m_docManager.SaveFile(*this, doc, path))
    {
        return false;
    }
    if (doc.m_saveCallback)
        doc.m_saveCallback();
    return true;
}

// TODO! Get rid of TabMove, make callers use OpenFileAs

// Happens when the user drags a tab out and drops it over a BowPad window.
// path is the temporary file that contains the latest document.
// savePath is the file we want to save the temporary file over and then use.
void CMainWindow::TabMove(const std::wstring& path, const std::wstring& savePath, bool bMod, long line, const std::wstring& title)
{
    std::wstring filepath = CPathUtils::GetLongPathname(path);

    auto docID = m_docManager.GetIdForPath(filepath);
    if (!docID.IsValid())
        return;

    int tab = m_tabBar.GetIndexFromID(docID);
    m_tabBar.ActivateAt(tab);
    auto& doc          = m_docManager.GetModDocumentFromID(docID);
    doc.m_path         = CPathUtils::GetLongPathname(savePath);
    doc.m_bIsDirty     = bMod;
    doc.m_bNeedsSaving = bMod;
    m_docManager.UpdateFileTime(doc, true);
    m_editor.Call(SCI_SETREADONLY, doc.m_bIsReadonly || doc.m_bIsWriteProtected);

    std::wstring sFileName = CPathUtils::GetFileName(doc.m_path);
    const auto&  lang      = CLexStyles::Instance().GetLanguageForDocument(doc, m_scratchEditor);
    m_editor.SetupLexerForLang(lang);
    doc.SetLanguage(lang);

    if (!title.empty())
        m_tabBar.SetCurrentTitle(title.c_str());
    else if (sFileName.empty())
        m_tabBar.SetCurrentTitle(GetNewTabName().c_str());
    else
        m_tabBar.SetCurrentTitle(sFileName.c_str());

    UpdateTab(docID);
    UpdateCaptionBar();
    UpdateStatusBar(true);

    GoToLine(line);

    m_fileTree.SetPath(CPathUtils::GetParentDirectory(savePath), false);
    ResizeChildWindows();

    DeleteFile(filepath.c_str());
}

void CMainWindow::SetTabMove(const std::wstring& path, const std::wstring& savePath, bool bMod, long line, const std::wstring& title)
{
    m_tabMovePath     = path;
    m_tabMoveSavePath = savePath;
    m_tabMoveMod      = bMod;
    m_initLine        = line;
    m_tabMoveTitle    = title;
}

void CMainWindow::SetElevatedSave(const std::wstring& path, const std::wstring& savePath, long line)
{
    m_elevatePath     = path;
    m_elevateSavePath = savePath;
    m_initLine        = line;
}

// path is the temporary file that contains the latest document.
// savePath is the file we want to save the temporary file over and then use.
void CMainWindow::ElevatedSave(const std::wstring& path, const std::wstring& savePath, long line)
{
    std::wstring filepath = CPathUtils::GetLongPathname(path);
    auto         docID    = m_docManager.GetIdForPath(filepath);
    if (docID.IsValid())
    {
        auto tab = m_tabBar.GetIndexFromID(docID);
        m_tabBar.SetTitle(tab, CPathUtils::GetFileName(savePath).c_str());
        m_tabBar.ActivateAt(tab);
        auto& doc          = m_docManager.GetModDocumentFromID(docID);
        doc.m_bNeedsSaving = true;
        doc.m_path         = CPathUtils::GetLongPathname(savePath);
        SaveCurrentTab();
        UpdateCaptionBar();
        UpdateStatusBar(true);
        GoToLine(line);
        m_fileTree.SetPath(CPathUtils::GetParentDirectory(savePath), false);
        ResizeChildWindows();
        // delete the temp file used for the elevated save
        DeleteFile(path.c_str());
    }
}

void CMainWindow::EnsureAtLeastOneTab()
{
    if (m_tabBar.GetItemCount() == 0)
        OpenNewTab();
}

void CMainWindow::GoToLine(size_t line)
{
    m_editor.GotoLine(static_cast<long>(line));
}

int CMainWindow::GetZoomPC() const
{
    int fontSize   = static_cast<int>(m_editor.ConstCall(SCI_STYLEGETSIZE, STYLE_DEFAULT));
    int zoom       = static_cast<int>(m_editor.ConstCall(SCI_GETZOOM));
    int zoomFactor = (fontSize + zoom) * 100 / fontSize;
    if (zoomFactor == 0)
        zoomFactor = 1;
    return zoomFactor;
}

void CMainWindow::SetZoomPC(int zoomPC) const
{
    int fontSize = static_cast<int>(m_editor.Call(SCI_STYLEGETSIZE, STYLE_DEFAULT));
    int zoom     = (fontSize * zoomPC / 100) - fontSize;
    m_editor.Call(SCI_SETZOOM, zoom);
}

void CMainWindow::UpdateStatusBar(bool bEverything)
{
    static ResString rsStatusTTDocSize(g_hRes, IDS_STATUSTTDOCSIZE);         // Length in bytes: %ld\r\nLines: %ld
    static ResString rsStatusTTCurPos(g_hRes, IDS_STATUSTTCURPOS);           // Line : %ld\r\nColumn : %ld\r\nSelection : %Iu | %Iu\r\nMatches: %ld
    static ResString rsStatusTTEOL(g_hRes, IDS_STATUSTTEOL);                 // Line endings: %s
    static ResString rsStatusTTTyping(g_hRes, IDS_STATUSTTTYPING);           // Typing mode: %s
    static ResString rsStatusTTTypingOvl(g_hRes, IDS_STATUSTTTYPINGOVL);     // Overtype
    static ResString rsStatusTTTypingIns(g_hRes, IDS_STATUSTTTYPINGINS);     // Insert
    static ResString rsStatusTTTabs(g_hRes, IDS_STATUSTTTABS);               // Open files: %d
    static ResString rsStatusSelection(g_hRes, IDS_STATUSSELECTION);         // Sel: %Iu chars | %Iu lines | %ld matches.
    static ResString rsStatusSelectionLong(g_hRes, IDS_STATUSSELECTIONLONG); // Selection: %Iu chars | %Iu lines | %ld matches.
    static ResString rsStatusSelectionNone(g_hRes, IDS_STATUSSELECTIONNONE); // no selection
    static ResString rsStatusTTTabSpaces(g_hRes, IDS_STATUSTTTABSPACES);     // Insert Tabs or Spaces
    static ResString rsStatusTTR2L(g_hRes, IDS_STATUSTTR2L);                 // Reading order (left-to-right or right-to-left)
    static ResString rsStatusTTEncoding(g_hRes, IDS_STATUSTTENCODING);       // Encoding: %s
    static ResString rsStatusZoom(g_hRes, IDS_STATUSZOOM);                   // Zoom: %d%%
    static ResString rsStatusCurposLong(g_hRes, IDS_STATUS_CURPOSLONG);      // Line: %ld / %ld   Column: %ld
    static ResString rsStatusCurpos(g_hRes, IDS_STATUS_CURPOS);              // Ln: %ld / %ld    Col: %ld
    static ResString rsStatusTabsOpenLong(g_hRes, IDS_STATUS_TABSOPENLONG);  // Ln: %ld / %ld    Col: %ld
    static ResString rsStatusTabsOpen(g_hRes, IDS_STATUS_TABSOPEN);          // Ln: %ld / %ld    Col: %ld

    auto lineCount = static_cast<long>(m_editor.Call(SCI_GETLINECOUNT));

    sptr_t selByte = 0;
    sptr_t selLine = 0;
    m_editor.GetSelectedCount(selByte, selLine);
    long selTextMarkerCount = m_editor.GetSelTextMarkerCount();
    auto curPos             = m_editor.Call(SCI_GETCURRENTPOS);
    long line               = static_cast<long>(m_editor.Call(SCI_LINEFROMPOSITION, curPos)) + 1;
    long column             = static_cast<long>(m_editor.Call(SCI_GETCOLUMN, curPos)) + 1;
    auto lengthInBytes      = m_editor.Call(SCI_GETLENGTH);
    auto bidi               = m_editor.Call(SCI_GETBIDIRECTIONAL);

    wchar_t readableLength[100] = {0};
    StrFormatByteSize(lengthInBytes, readableLength, _countof(readableLength));

    auto numberColor = 0x600000;
    if (CTheme::Instance().IsHighContrastModeDark())
        numberColor = CTheme::Instance().GetThemeColor(GetSysColor(COLOR_WINDOWTEXT));
    else if (CTheme::Instance().IsHighContrastMode())
        numberColor = CTheme::Instance().GetThemeColor(GetSysColor(COLOR_WINDOWTEXT));

    m_statusBar.SetPart(STATUSBAR_CUR_POS,
                        CStringUtils::Format(rsStatusCurposLong, numberColor, line, numberColor, lineCount, numberColor, column),
                        CStringUtils::Format(rsStatusCurpos, numberColor, line, numberColor, lineCount, numberColor, column),
                        CStringUtils::Format(rsStatusTTDocSize, lengthInBytes, readableLength, lineCount),
                        200,
                        130,
                        0,
                        true);
    auto sNoSel = CStringUtils::Format(rsStatusSelectionNone, GetSysColor(COLOR_GRAYTEXT));
    m_statusBar.SetPart(STATUSBAR_SEL,
                        selByte ? CStringUtils::Format(rsStatusSelectionLong, numberColor, selByte, numberColor, selLine, (selTextMarkerCount ? 0x008000 : numberColor), selTextMarkerCount) : sNoSel,
                        selByte ? CStringUtils::Format(rsStatusSelection, numberColor, selByte, numberColor, selLine, (selTextMarkerCount ? 0x008000 : numberColor), selTextMarkerCount) : sNoSel,
                        CStringUtils::Format(rsStatusTTCurPos, line, column, selByte, selLine, selTextMarkerCount),
                        250,
                        200,
                        0,
                        true);

    auto overType = m_editor.Call(SCI_GETOVERTYPE);
    m_statusBar.SetPart(STATUSBAR_TYPING_MODE,
                        overType ? L"OVR" : L"INS",
                        L"",
                        CStringUtils::Format(rsStatusTTTyping, overType ? rsStatusTTTypingOvl.c_str() : rsStatusTTTypingIns.c_str()),
                        0,
                        0,
                        1, // center
                        true);

    bool bCapsLockOn = (GetKeyState(VK_CAPITAL) & 0x01) != 0;
    m_statusBar.SetPart(STATUSBAR_CAPS,
                        bCapsLockOn ? L"CAPS" : L"",
                        L"",
                        bCapsLockOn ? L"CAPS" : L"",
                        35,
                        35,
                        1, // center
                        true,
                        false,
                        nullptr,
                        bCapsLockOn ? m_hCapsLockIcon : m_hEmptyIcon);

    bool usingTabs = m_editor.Call(SCI_GETUSETABS) ? true : false;
    int  tabSize   = static_cast<int>(m_editor.Call(SCI_GETTABWIDTH));
    m_statusBar.SetPart(STATUSBAR_TABSPACE,
                        usingTabs ? CStringUtils::Format(L"Tabs: %%c%06X%d", numberColor, tabSize) : L"Spaces",
                        L"",
                        rsStatusTTTabSpaces,
                        0,
                        0,
                        1, // center
                        true);
    m_statusBar.SetPart(STATUSBAR_R2L,
                        bidi == SC_BIDIRECTIONAL_R2L ? L"R2L" : L"L2R",
                        L"",
                        rsStatusTTR2L,
                        0,
                        0,
                        1, // center
                        true);

    int  zoomFactor = GetZoomPC();
    auto sZoom      = CStringUtils::Format(rsStatusZoom, numberColor, zoomFactor);
    auto sZoomTT    = CRichStatusBar::GetPlainString(sZoom);
    m_statusBar.SetPart(STATUSBAR_ZOOM,
                        sZoom,
                        L"",
                        sZoomTT,
                        85,
                        85,
                        0,
                        false,
                        true,
                        nullptr,
                        CTheme::Instance().IsDarkTheme() ? m_hZoomDarkIcon : m_hZoomIcon);

    if (bEverything)
    {
        const auto& doc   = m_docManager.GetDocumentFromID(m_tabBar.GetCurrentTabId());
        auto        sLang = CUnicodeUtils::StdGetUnicode(doc.GetLanguage());
        m_statusBar.SetPart(STATUSBAR_DOC_TYPE,
                            L"%b" + sLang,
                            L"",
                            sLang,
                            0,
                            0,
                            1, // center
                            true,
                            false,
                            SysInfo::Instance().IsUACEnabled() && SysInfo::Instance().IsElevated() ? m_hShieldIcon : nullptr,
                            m_hLexerIcon);

        auto editorConfigEnabled = CEditorConfigHandler::Instance().IsEnabled(doc.m_path);
        m_statusBar.SetPart(STATUSBAR_EDITORCONFIG,
                            L"",
                            L"",
                            editorConfigEnabled ? L"Editorconfig active" : L"Editorconfig inactive",
                            0,
                            0,
                            1, // center
                            true,
                            false,
                            editorConfigEnabled ? hEditorconfigActive : hEditorconfigInactive);

        int eolMode = static_cast<int>(m_editor.Call(SCI_GETEOLMODE));
        APPVERIFY(toEolMode(doc.m_format) == eolMode);
        auto eolDesc = getEolFormatDescription(doc.m_format);
        m_statusBar.SetPart(STATUSBAR_EOL_FORMAT,
                            eolDesc,
                            L"",
                            CStringUtils::Format(rsStatusTTEOL, eolDesc.c_str()),
                            0,
                            0,
                            1, // center
                            true,
                            true);

        m_statusBar.SetPart(STATUSBAR_UNICODE_TYPE,
                            doc.GetEncodingString(),
                            L"",
                            CStringUtils::Format(rsStatusTTEncoding, doc.GetEncodingString().c_str()),
                            0,
                            0,
                            1 // center
        );

        auto tabCount = m_tabBar.GetItemCount();
        m_statusBar.SetPart(STATUSBAR_TABS,
                            CStringUtils::Format(rsStatusTabsOpenLong, numberColor, tabCount),
                            CStringUtils::Format(rsStatusTabsOpen, numberColor, tabCount),
                            CStringUtils::Format(rsStatusTTTabs, tabCount),
                            65,
                            55);
    }
    m_statusBar.CalcWidths();
}

bool CMainWindow::CloseTab(int closingTabIndex, bool force /* = false */, bool quitting)
{
    if (closingTabIndex < 0 || closingTabIndex >= m_tabBar.GetItemCount())
    {
        APPVERIFY(false);
        return false;
    }
    auto             closingTabId = m_tabBar.GetIDFromIndex(closingTabIndex);
    auto             currentTabId = m_tabBar.GetCurrentTabId();
    const CDocument& closingDoc   = m_docManager.GetDocumentFromID(closingTabId);
    if (!force && (closingDoc.m_bIsDirty || closingDoc.m_bNeedsSaving))
    {
        if (closingTabId != currentTabId)
            m_tabBar.ActivateAt(closingTabIndex);
        if (!doCloseAll || !closeAllDoAll)
        {
            auto bc            = UnblockUI();
            responseToCloseTab = AskToCloseTab();
            ReBlockUI(bc);
        }

        if (responseToCloseTab == ResponseToCloseTab::SaveAndClose)
        {
            if (!SaveCurrentTab()) // Save And (fall through to) Close
                return false;
        }
        else if (responseToCloseTab != ResponseToCloseTab::CloseWithoutSaving)
        {
            // Cancel And Stay Open
            // activate the tab: user might have clicked another than
            // the active tab to close: user clicked on that tab so activate that tab now
            //m_TabBar.ActivateAt(closingTabIndex);
            return false;
        }
        // If the save successful or closed without saveing, the tab will be closed.
    }
    CCommandHandler::Instance().OnDocumentClose(closingTabId);
    // Prefer to remove the document after the tab has gone as it supports it
    // and deletion causes events that may expect it to be there.
    m_tabBar.DeleteItemAt(closingTabIndex);
    // SCI_SETDOCPOINTER is necessary so the reference count of the document
    // is decreased and the memory can be released.
    m_editor.Call(SCI_SETDOCPOINTER, 0, 0);
    m_docManager.RemoveDocument(closingTabId);

    int tabCount     = m_tabBar.GetItemCount();
    int nextTabIndex = (closingTabIndex < tabCount) ? closingTabIndex : tabCount - 1;
    if (closingTabId != currentTabId)
    {
        auto nxtIndex = m_tabBar.GetIndexFromID(currentTabId);
        if (nxtIndex >= 0)
            nextTabIndex = nxtIndex;
    }
    else if (tabCount == 0 && !quitting)
    {
        EnsureAtLeastOneTab();
        return true;
    }
    if (nextTabIndex >= 0)
        m_tabBar.SelectChange(nextTabIndex);
    return true;
}

bool CMainWindow::CloseAllTabs(bool quitting)
{
    BlockAllUIUpdates(true);
    OnOutOfScope(BlockAllUIUpdates(false));

    ShowProgressCtrl(static_cast<UINT>(CIniSettings::Instance().GetInt64(L"View", L"progressdelay", 1000)));
    OnOutOfScope(HideProgressCtrl());

    closeAllDoAll = FALSE;
    doCloseAll    = true;
    OnOutOfScope(
        closeAllDoAll = FALSE;
        doCloseAll    = false;);
    auto total = m_tabBar.GetItemCount();
    int  count = 0;
    for (;;)
    {
        SetProgress(count++, total);
        if (m_tabBar.GetItemCount() == 0)
            break;
        if (!CloseTab(m_tabBar.GetCurrentTabIndex(), false, quitting))
            return false;
        if (!quitting && m_tabBar.GetItemCount() == 1 &&
            m_editor.Call(SCI_GETTEXTLENGTH) == 0 &&
            m_editor.Call(SCI_GETMODIFY) == 0 &&
            m_docManager.GetDocumentFromID(m_tabBar.GetIDFromIndex(0)).m_path.empty())
            return false;
    }
    if (quitting)
        m_fileTree.BlockRefresh(true); // when we're quitting, don't let the file tree do a refresh

    return m_tabBar.GetItemCount() == 0;
}

void CMainWindow::SetFileToOpen(const std::wstring& path, size_t line)
{
    auto list = GetFileListFromGlobPath(path);
    for (const auto p : list)
    {
        m_pathsToOpen[p] = line;
    }
}

void CMainWindow::CloseAllButCurrentTab()
{
    BlockAllUIUpdates(true);
    OnOutOfScope(BlockAllUIUpdates(false));
    int count   = m_tabBar.GetItemCount();
    int current = m_tabBar.GetCurrentTabIndex();

    ShowProgressCtrl(static_cast<UINT>(CIniSettings::Instance().GetInt64(L"View", L"progressdelay", 1000)));
    OnOutOfScope(HideProgressCtrl());

    closeAllDoAll = FALSE;
    doCloseAll    = true;
    OnOutOfScope(
        closeAllDoAll = FALSE;
        doCloseAll    = false;);
    for (int i = count - 1; i >= 0; --i)
    {
        if (i != current)
        {
            CloseTab(i);
            SetProgress(count - i, count - 1);
        }
    }
}

void CMainWindow::UpdateCaptionBar()
{
    auto         docID   = m_tabBar.GetCurrentTabId();
    std::wstring appName = GetAppName();
    std::wstring elev;
    ResString    rElev(g_hRes, IDS_ELEVATED);
    if (SysInfo::Instance().IsUACEnabled() && SysInfo::Instance().IsElevated())
        elev = static_cast<LPCWSTR>(rElev);

    if (m_docManager.HasDocumentID(docID))
    {
        const auto& doc = m_docManager.GetDocumentFromID(docID);

        std::wstring sTitle = elev;
        if (!elev.empty())
            sTitle += L" : ";
        sTitle += doc.m_path.empty() ? m_tabBar.GetCurrentTitle() : doc.m_path;
        if (doc.m_bNeedsSaving || doc.m_bIsDirty)
            sTitle += L" * ";
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

void CMainWindow::UpdateTab(DocID docID)
{
    const auto& doc = m_docManager.GetDocumentFromID(docID);
    TCITEM      tie{};
    tie.lParam = -1;
    tie.mask   = TCIF_IMAGE;
    if (doc.m_bIsReadonly || doc.m_bIsWriteProtected || (m_editor.Call(SCI_GETREADONLY) != 0))
        tie.iImage = REDONLY_IMG_INDEX;
    else
        tie.iImage = doc.m_bIsDirty || doc.m_bNeedsSaving ? UNSAVED_IMG_INDEX : SAVED_IMG_INDEX;
    TabCtrl_SetItem(m_tabBar, m_tabBar.GetIndexFromID(docID), &tie);
    UpdateCaptionBar();
}

ResponseToCloseTab CMainWindow::AskToCloseTab() const
{
    ResString    rTitle(g_hRes, IDS_HASMODIFICATIONS);
    ResString    rQuestion(g_hRes, IDS_DOYOUWANTOSAVE);
    ResString    rSave(g_hRes, IDS_SAVE);
    ResString    rDontSave(g_hRes, IDS_DONTSAVE);
    std::wstring sQuestion = CStringUtils::Format(rQuestion, m_tabBar.GetCurrentTitle().c_str());

    TASKDIALOGCONFIG  tdc               = {sizeof(TASKDIALOGCONFIG)};
    TASKDIALOG_BUTTON aCustomButtons[2] = {};
    aCustomButtons[0].nButtonID         = 100;
    aCustomButtons[0].pszButtonText     = rSave;
    aCustomButtons[1].nButtonID         = 101;
    aCustomButtons[1].pszButtonText     = rDontSave;
    tdc.pButtons                        = aCustomButtons;
    tdc.cButtons                        = _countof(aCustomButtons);
    assert(tdc.cButtons <= _countof(aCustomButtons));
    tdc.nDefaultButton = 100;

    tdc.hwndParent         = *this;
    tdc.hInstance          = g_hRes;
    tdc.dwCommonButtons    = TDCBF_CANCEL_BUTTON;
    tdc.pszWindowTitle     = MAKEINTRESOURCE(IDS_APP_TITLE);
    tdc.pszMainIcon        = TD_INFORMATION_ICON;
    tdc.pszMainInstruction = rTitle;
    tdc.pszContent         = sQuestion.c_str();
    if (doCloseAll)
        tdc.pszVerificationText = MAKEINTRESOURCE(IDS_DOITFORALLFILES);
    int     nClickedBtn = 0;
    HRESULT hr          = TaskDialogIndirect(&tdc, &nClickedBtn, nullptr, &closeAllDoAll);
    if (CAppUtils::FailedShowMessage(hr))
        nClickedBtn = 0;
    ResponseToCloseTab response = ResponseToCloseTab::StayOpen;
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
    if (!responseToOutsideModifiedFileDoAll || !doModifiedAll)
    {
        bool changed = doc.m_bNeedsSaving || doc.m_bIsDirty;
        if (!changed && CIniSettings::Instance().GetInt64(L"View", L"autorefreshifnotmodified", 1))
            return ResponseToOutsideModifiedFile::Reload;
        ResString rTitle(g_hRes, IDS_OUTSIDEMODIFICATIONS);
        ResString rQuestion(g_hRes, changed ? IDS_DOYOUWANTRELOADBUTDIRTY : IDS_DOYOUWANTTORELOAD);
        ResString rSave(g_hRes, IDS_SAVELOSTOUTSIDEMODS);
        ResString rReload(g_hRes, changed ? IDS_RELOADLOSTMODS : IDS_RELOAD);
        ResString rCancel(g_hRes, IDS_NORELOAD);
        // Be specific about what they are re-loading.
        std::wstring sQuestion = CStringUtils::Format(rQuestion, doc.m_path.c_str());

        TASKDIALOGCONFIG  tdc               = {sizeof(TASKDIALOGCONFIG)};
        TASKDIALOG_BUTTON aCustomButtons[3] = {};
        int               bi                = 0;
        aCustomButtons[bi].nButtonID        = 101;
        aCustomButtons[bi++].pszButtonText  = rReload;
        if (changed)
        {
            aCustomButtons[bi].nButtonID       = 102;
            aCustomButtons[bi++].pszButtonText = rSave;
        }
        aCustomButtons[bi].nButtonID       = 100;
        aCustomButtons[bi++].pszButtonText = rCancel;
        tdc.pButtons                       = aCustomButtons;
        tdc.cButtons                       = bi;
        assert(tdc.cButtons <= _countof(aCustomButtons));
        tdc.nDefaultButton = 100; // default to "Cancel" in case the file is modified

        tdc.hwndParent          = *this;
        tdc.hInstance           = g_hRes;
        tdc.dwFlags             = TDF_USE_COMMAND_LINKS | TDF_POSITION_RELATIVE_TO_WINDOW | TDF_SIZE_TO_CONTENT | TDF_ALLOW_DIALOG_CANCELLATION;
        tdc.pszWindowTitle      = MAKEINTRESOURCE(IDS_APP_TITLE);
        tdc.pszMainIcon         = changed ? TD_WARNING_ICON : TD_INFORMATION_ICON;
        tdc.pszMainInstruction  = rTitle;
        tdc.pszContent          = sQuestion.c_str();
        tdc.pszVerificationText = MAKEINTRESOURCE(IDS_DOITFORALLFILES);
        int     nClickedBtn     = 0;
        HRESULT hr              = TaskDialogIndirect(&tdc, &nClickedBtn, nullptr, &responseToOutsideModifiedFileDoAll);
        if (CAppUtils::FailedShowMessage(hr))
            nClickedBtn = 0;
        responseToOutsideModifiedFile = ResponseToOutsideModifiedFile::Cancel;
        switch (nClickedBtn)
        {
            case 101:
                responseToOutsideModifiedFile = ResponseToOutsideModifiedFile::Reload;
                break;
            case 102:
                responseToOutsideModifiedFile = ResponseToOutsideModifiedFile::KeepOurChanges;
                break;
        }
    }
    return responseToOutsideModifiedFile;
}

bool CMainWindow::AskToReload(const CDocument& doc) const
{
    ResString rTitle(g_hRes, IDS_HASMODIFICATIONS);
    ResString rQuestion(g_hRes, IDS_RELOADREALLY);
    ResString rReload(g_hRes, IDS_DORELOAD);
    ResString rCancel(g_hRes, IDS_DONTRELOAD);
    // Be specific about what they are re-loading.
    std::wstring sQuestion = CStringUtils::Format(rQuestion, doc.m_path.c_str());

    TASKDIALOGCONFIG  tdc               = {sizeof(TASKDIALOGCONFIG)};
    TASKDIALOG_BUTTON aCustomButtons[2] = {};
    aCustomButtons[0].nButtonID         = 101;
    aCustomButtons[0].pszButtonText     = rReload;
    aCustomButtons[1].nButtonID         = 100;
    aCustomButtons[1].pszButtonText     = rCancel;
    tdc.pButtons                        = aCustomButtons;
    tdc.cButtons                        = _countof(aCustomButtons);
    assert(tdc.cButtons <= _countof(aCustomButtons));
    tdc.nDefaultButton = 100; // Default to cancel

    tdc.hwndParent         = *this;
    tdc.hInstance          = g_hRes;
    tdc.dwFlags            = TDF_USE_COMMAND_LINKS | TDF_ENABLE_HYPERLINKS | TDF_POSITION_RELATIVE_TO_WINDOW | TDF_SIZE_TO_CONTENT | TDF_ALLOW_DIALOG_CANCELLATION;
    tdc.pszWindowTitle     = MAKEINTRESOURCE(IDS_APP_TITLE);
    tdc.pszMainIcon        = TD_WARNING_ICON; // Warn because we are going to throw away the document.
    tdc.pszMainInstruction = rTitle;
    tdc.pszContent         = sQuestion.c_str();
    int     nClickedBtn    = 0;
    HRESULT hr             = TaskDialogIndirect(&tdc, &nClickedBtn, nullptr, nullptr);
    if (CAppUtils::FailedShowMessage(hr))
        nClickedBtn = 0;
    bool reload = (nClickedBtn == 101);
    return reload;
}

bool CMainWindow::AskAboutOutsideDeletedFile(const CDocument& doc) const
{
    ResString rTitle(g_hRes, IDS_OUTSIDEREMOVEDHEAD);
    ResString rQuestion(g_hRes, IDS_OUTSIDEREMOVED);
    ResString rKeep(g_hRes, IDS_OUTSIDEREMOVEDKEEP);
    ResString rClose(g_hRes, IDS_OUTSIDEREMOVEDCLOSE);
    // Be specific about what they are removing.
    std::wstring sQuestion = CStringUtils::Format(rQuestion, doc.m_path.c_str());

    TASKDIALOGCONFIG  tdc               = {sizeof(TASKDIALOGCONFIG)};
    TASKDIALOG_BUTTON aCustomButtons[2] = {};
    int               bi                = 0;
    aCustomButtons[bi].nButtonID        = 100;
    aCustomButtons[bi++].pszButtonText  = rKeep;
    aCustomButtons[bi].nButtonID        = 101;
    aCustomButtons[bi].pszButtonText    = rClose;
    tdc.pButtons                        = aCustomButtons;
    tdc.cButtons                        = _countof(aCustomButtons);
    tdc.nDefaultButton                  = 100;

    tdc.hwndParent         = *this;
    tdc.hInstance          = g_hRes;
    tdc.dwFlags            = TDF_USE_COMMAND_LINKS | TDF_POSITION_RELATIVE_TO_WINDOW | TDF_SIZE_TO_CONTENT | TDF_ALLOW_DIALOG_CANCELLATION;
    tdc.pszWindowTitle     = MAKEINTRESOURCE(IDS_APP_TITLE);
    tdc.pszMainIcon        = TD_INFORMATION_ICON;
    tdc.pszMainInstruction = rTitle;
    tdc.pszContent         = sQuestion.c_str();
    int     nClickedBtn    = 0;
    HRESULT hr             = TaskDialogIndirect(&tdc, &nClickedBtn, nullptr, nullptr);
    if (CAppUtils::FailedShowMessage(hr))
        nClickedBtn = 0;
    bool bKeep = true;
    if (nClickedBtn == 101)
        bKeep = false;
    return bKeep;
}

bool CMainWindow::AskToRemoveReadOnlyAttribute() const
{
    ResString rTitle(g_hRes, IDS_FILEISREADONLY);
    ResString rQuestion(g_hRes, IDS_FILEMAKEWRITABLEASK);
    auto      rEditFile = LoadResourceWString(g_hRes, IDS_EDITFILE);
    auto      rCancel   = LoadResourceWString(g_hRes, IDS_CANCEL);
    // We remove the short cut accelerators from these buttons because this
    // dialog pops up automatically and it's too easy to be typing into the editor
    // when that happens and accidentally acknowledge a button.
    SearchRemoveAll(rEditFile, L"&");
    SearchRemoveAll(rCancel, L"&");

    TASKDIALOGCONFIG  tdc               = {sizeof(TASKDIALOGCONFIG)};
    TASKDIALOG_BUTTON aCustomButtons[2] = {};
    aCustomButtons[0].nButtonID         = 101;
    aCustomButtons[0].pszButtonText     = rEditFile.c_str();
    aCustomButtons[1].nButtonID         = 100;
    aCustomButtons[1].pszButtonText     = rCancel.c_str();
    tdc.pButtons                        = aCustomButtons;
    tdc.cButtons                        = _countof(aCustomButtons);
    assert(tdc.cButtons <= _countof(aCustomButtons));
    tdc.nDefaultButton = 100; // Default to cancel

    tdc.hwndParent         = *this;
    tdc.hInstance          = g_hRes;
    tdc.dwFlags            = TDF_POSITION_RELATIVE_TO_WINDOW | TDF_SIZE_TO_CONTENT | TDF_ALLOW_DIALOG_CANCELLATION;
    tdc.pszWindowTitle     = MAKEINTRESOURCE(IDS_APP_TITLE);
    tdc.pszMainIcon        = TD_WARNING_ICON;
    tdc.pszMainInstruction = rTitle;
    tdc.pszContent         = rQuestion;
    int     nClickedBtn    = 0;
    HRESULT hr             = TaskDialogIndirect(&tdc, &nClickedBtn, nullptr, nullptr);
    if (CAppUtils::FailedShowMessage(hr))
        nClickedBtn = 0;
    bool bRemoveReadOnlyAttribute = (nClickedBtn == 101);
    return bRemoveReadOnlyAttribute;
}

// Returns true if file exists or was created.
bool CMainWindow::AskToCreateNonExistingFile(const std::wstring& path) const
{
    ResString rTitle(g_hRes, IDS_FILE_DOESNT_EXIST);
    ResString rQuestion(g_hRes, IDS_FILE_ASK_TO_CREATE);
    ResString rCreate(g_hRes, IDS_FILE_CREATE);
    ResString rCancel(g_hRes, IDS_FILE_CREATE_CANCEL);
    // Show exactly what we are creating.
    std::wstring sQuestion = CStringUtils::Format(rQuestion, path.c_str());

    TASKDIALOGCONFIG  tdc               = {sizeof(TASKDIALOGCONFIG)};
    TASKDIALOG_BUTTON aCustomButtons[2] = {};
    int               bi                = 0;
    aCustomButtons[bi].nButtonID        = 101;
    aCustomButtons[bi++].pszButtonText  = rCreate;
    aCustomButtons[bi].nButtonID        = 100;
    aCustomButtons[bi++].pszButtonText  = rCancel;
    tdc.pButtons                        = aCustomButtons;
    tdc.cButtons                        = bi;
    assert(tdc.cButtons <= _countof(aCustomButtons));
    tdc.nDefaultButton = 101;

    tdc.hwndParent         = *this;
    tdc.hInstance          = g_hRes;
    tdc.dwFlags            = TDF_USE_COMMAND_LINKS | TDF_POSITION_RELATIVE_TO_WINDOW | TDF_SIZE_TO_CONTENT | TDF_ALLOW_DIALOG_CANCELLATION;
    tdc.pszWindowTitle     = MAKEINTRESOURCE(IDS_APP_TITLE);
    tdc.pszMainIcon        = TD_INFORMATION_ICON;
    tdc.pszMainInstruction = rTitle;
    tdc.pszContent         = sQuestion.c_str();
    int     nClickedBtn    = 0;
    HRESULT hr             = TaskDialogIndirect(&tdc, &nClickedBtn, nullptr, nullptr);
    if (CAppUtils::FailedShowMessage(hr))
        nClickedBtn = 0;
    bool bCreate = (nClickedBtn == 101);
    return bCreate;
}

void CMainWindow::CopyCurDocPathToClipboard() const
{
    auto id = m_tabBar.GetCurrentTabId();
    if (m_docManager.HasDocumentID(id))
    {
        const auto& doc = m_docManager.GetDocumentFromID(id);
        WriteAsciiStringToClipboard(doc.m_path.c_str(), *this);
    }
}

void CMainWindow::CopyCurDocNameToClipboard() const
{
    auto id = m_tabBar.GetCurrentTabId();
    if (m_docManager.HasDocumentID(id))
    {
        const auto& doc = m_docManager.GetDocumentFromID(id);
        WriteAsciiStringToClipboard(CPathUtils::GetFileName(doc.m_path).c_str(), *this);
    }
}

void CMainWindow::CopyCurDocDirToClipboard() const
{
    auto id = m_tabBar.GetCurrentTabId();
    if (m_docManager.HasDocumentID(id))
    {
        const auto& doc = m_docManager.GetDocumentFromID(id);
        WriteAsciiStringToClipboard(CPathUtils::GetParentDirectory(doc.m_path).c_str(), *this);
    }
}

void CMainWindow::ShowCurDocInExplorer() const
{
    auto id = m_tabBar.GetCurrentTabId();
    if (m_docManager.HasDocumentID(id))
    {
        const auto& doc                    = m_docManager.GetDocumentFromID(id);
        PCIDLIST_ABSOLUTE __unaligned pidl = ILCreateFromPath(doc.m_path.c_str());
        if (pidl)
        {
            SHOpenFolderAndSelectItems(pidl, 0, nullptr, 0);
            CoTaskMemFree(static_cast<LPVOID>(const_cast<PIDLIST_ABSOLUTE>(pidl)));
        }
    }
}

void CMainWindow::ShowCurDocExplorerProperties() const
{
    auto id = m_tabBar.GetCurrentTabId();
    if (m_docManager.HasDocumentID(id))
    {
        // This creates an ugly exception dialog on my machine in debug mode
        // but it seems to work anyway.
        const auto&      doc  = m_docManager.GetDocumentFromID(id);
        SHELLEXECUTEINFO info = {sizeof(SHELLEXECUTEINFO)};
        info.lpVerb           = L"properties";
        info.lpFile           = doc.m_path.c_str();
        info.nShow            = SW_NORMAL;
        info.fMask            = SEE_MASK_INVOKEIDLIST;
        info.hwnd             = *this;
        ShellExecuteEx(&info);
    }
}

void CMainWindow::HandleClipboardUpdate()
{
    CCommandHandler::Instance().OnClipboardChanged();
    std::wstring s;
    if (IsClipboardFormatAvailable(CF_UNICODETEXT))
    {
        CClipboardHelper clipboard;
        if (clipboard.Open(*this))
        {
            HANDLE hData = GetClipboardData(CF_UNICODETEXT);
            if (hData)
            {
                LPCWSTR lptStr = static_cast<LPCWSTR>(GlobalLock(hData));
                OnOutOfScope(
                    GlobalUnlock(hData););
                if (lptStr != nullptr)
                    s = lptStr;
            }
        }
    }
    if (!s.empty())
    {
        std::wstring sTrimmed = s;
        CStringUtils::trim(sTrimmed);
        if (!sTrimmed.empty())
        {
            for (auto it = m_clipboardHistory.cbegin(); it != m_clipboardHistory.cend(); ++it)
            {
                if (it->compare(s) == 0)
                {
                    m_clipboardHistory.erase(it);
                    break;
                }
            }
            m_clipboardHistory.push_front(std::move(s));
        }
    }

    size_t maxsize = static_cast<size_t>(CIniSettings::Instance().GetInt64(L"clipboard", L"maxhistory", 20));
    if (m_clipboardHistory.size() > maxsize)
        m_clipboardHistory.pop_back();
}

void CMainWindow::PasteHistory()
{
    if (!m_clipboardHistory.empty())
    {
        // create a context menu with all the items in the clipboard history
        HMENU hMenu = CreatePopupMenu();
        if (hMenu)
        {
            OnOutOfScope(
                DestroyMenu(hMenu););
            size_t pos = m_editor.Call(SCI_GETCURRENTPOS);
            POINT  pt{};
            pt.x = static_cast<LONG>(m_editor.Call(SCI_POINTXFROMPOSITION, 0, pos));
            pt.y = static_cast<LONG>(m_editor.Call(SCI_POINTYFROMPOSITION, 0, pos));
            ClientToScreen(m_editor, &pt);
            int    index   = 1;
            size_t maxsize = static_cast<size_t>(CIniSettings::Instance().GetInt64(L"clipboard", L"maxuilength", 40));
            for (const auto& s : m_clipboardHistory)
            {
                std::wstring sf = s;
                SearchReplace(sf, L"\t", L" ");
                SearchReplace(sf, L"\n", L" ");
                SearchReplace(sf, L"\r", L" ");
                CStringUtils::trim(sf);
                // remove unnecessary whitespace inside the string
                std::wstring::iterator newEnd = std::unique(sf.begin(), sf.end(), [](wchar_t lhs, wchar_t rhs) -> bool {
                    return (lhs == ' ' && rhs == ' ');
                });
                sf.erase(newEnd, sf.end());

                AppendMenu(hMenu, MF_ENABLED, index, sf.substr(0, maxsize).c_str());
                ++index;
            }
            int selIndex = TrackPopupMenu(hMenu, TPM_LEFTALIGN | TPM_RETURNCMD, pt.x, pt.y, 0, m_editor, nullptr);
            if (selIndex > 0)
            {
                index = 1;
                for (const auto& s : m_clipboardHistory)
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
void CMainWindow::HandleDwellStart(const SCNotification& scn, bool start)
{
    // Note style will be zero if no style or past end of the document.
    if ((scn.position >= 0) && (!start || m_editor.Call(SCI_INDICATORVALUEAT, INDIC_URLHOTSPOT, scn.position)))
    {
        const sptr_t pixelMargin = CDPIAware::Instance().Scale(*this, 4);
        // an url hotspot
        // find start of url
        auto startPos     = scn.position;
        auto endPos       = scn.position;
        auto lineStartPos = m_editor.Call(SCI_POSITIONFROMPOINT, 0, scn.y);
        if (!start && !m_editor.Call(SCI_INDICATORVALUEAT, INDIC_URLHOTSPOT, startPos))
        {
            startPos     = m_editor.Call(SCI_POSITIONFROMPOINT, scn.x, scn.y + pixelMargin);
            endPos       = startPos;
            lineStartPos = m_editor.Call(SCI_POSITIONFROMPOINT, 0, scn.y + pixelMargin);
            if (!m_editor.Call(SCI_INDICATORVALUEAT, INDIC_URLHOTSPOT, startPos))
            {
                m_editor.Call(SCI_CALLTIPCANCEL);
                return;
            }
        }
        while (startPos && m_editor.Call(SCI_INDICATORVALUEAT, INDIC_URLHOTSPOT, startPos))
            --startPos;
        ++startPos;
        // find end of url
        auto docEnd = m_editor.Call(SCI_GETLENGTH);
        while (endPos < docEnd && m_editor.Call(SCI_INDICATORVALUEAT, INDIC_URLHOTSPOT, endPos))
            ++endPos;
        --endPos;

        ResString          str(g_hRes, IDS_HOWTOOPENURL);
        std::string        strA = CUnicodeUtils::StdGetUTF8(str);
        std::istringstream is(strA);
        std::string        part;
        size_t             lineLength = 0;
        while (getline(is, part, '\n'))
            lineLength = max(lineLength, part.size());
        auto tipPos = startPos + ((endPos - startPos) / 2) - static_cast<sptr_t>(lineLength) / 2;
        if (m_editor.Call(SCI_CALLTIPACTIVE))
        {
            auto linePos = m_editor.Call(SCI_LINEFROMPOSITION, scn.position);
            auto upPos   = m_editor.Call(SCI_LINEFROMPOSITION, m_editor.Call(SCI_POSITIONFROMPOINT, 0, scn.y - pixelMargin));
            if (upPos != linePos)
            {
                m_editor.Call(SCI_CALLTIPCANCEL);
                return;
            }
        }
        tipPos = max(lineStartPos, tipPos);
        if (m_editor.Call(SCI_CALLTIPACTIVE) && m_editor.Call(SCI_CALLTIPPOSSTART) == tipPos)
            return;
        if (!start && !m_editor.Call(SCI_CALLTIPACTIVE))
            return;

        m_editor.Call(SCI_CALLTIPSHOW, tipPos, reinterpret_cast<sptr_t>(strA.c_str()));
        return;
    }

    // try the users real selection first
    std::string sWord    = m_editor.GetSelectedText();
    auto        selStart = m_editor.Call(SCI_GETSELECTIONSTART);
    auto        selEnd   = m_editor.Call(SCI_GETSELECTIONEND);

    if (sWord.empty() ||
        (scn.position > selEnd) || (scn.position < selStart))
    {
        auto wordCharsBuffer = m_editor.GetWordChars();
        OnOutOfScope(m_editor.Call(SCI_SETWORDCHARS, 0, reinterpret_cast<LPARAM>(wordCharsBuffer.c_str())));

        m_editor.Call(SCI_SETWORDCHARS, 0, reinterpret_cast<LPARAM>("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_.,#"));
        selStart = m_editor.Call(SCI_WORDSTARTPOSITION, scn.position, false);
        selEnd   = m_editor.Call(SCI_WORDENDPOSITION, scn.position, false);
        sWord    = m_editor.GetTextRange(static_cast<long>(selStart), static_cast<long>(selEnd));
    }
    if (sWord.empty())
        return;

    // Short form or long form html color e.g. #F0F or #FF00FF
    // Make sure the string is hexadecimal
    if (sWord[0] == '#' && (sWord.size() == 4 || sWord.size() == 7 || sWord.size() == 9) && IsHexDigitString(sWord.c_str() + 1))
    {
        bool     ok    = false;
        COLORREF color = 0;

        // Note: could use std::stoi family of functions but they throw
        // range exceptions etc. and VC reports those in the debugger output
        // window. That's distracting and gives the impression something
        // is drastically wrong when it isn't, so we don't use those.

        std::string strNum = sWord.substr(1); // Drop #
        if (strNum.size() == 3)               // short form .e.g. F0F
        {
            ok = GDIHelpers::ShortHexStringToCOLORREF(strNum, &color);
        }
        else if (strNum.size() == 6) // normal/long form, e.g. FF00FF
        {
            ok = GDIHelpers::HexStringToCOLORREF(strNum, &color);
        }
        else if (strNum.size() == 8) // normal/long form with alpha, e.g. 00FF00FF
        {
            ok = GDIHelpers::HexStringToCOLORREF(strNum, &color);
        }
        if (ok)
        {
            // Don't display hex with # as that means web color hex triplet
            // Show as hex with 0x and show what it was parsed to.
            auto sCallTip = CStringUtils::Format(
                L"RGB(%d, %d, %d)\nHex: 0x%06lX",
                GetRValue(color), GetGValue(color), GetBValue(color), static_cast<DWORD>(color));
            auto  msgPos = GetMessagePos();
            POINT pt     = {GET_X_LPARAM(msgPos), GET_Y_LPARAM(msgPos)};
            m_custToolTip.ShowTip(pt, sCallTip, &color);
            return;
        }
    }

    // See if the data looks like a pattern matching RGB(r,g,b) where each element
    // can be decimal, hex with leading 0x, or octal with leading 0, like C/C++.
    auto          wWord  = CUnicodeUtils::StdGetUnicode(sWord);
    const wchar_t rgb[]  = {L"RGB"};
    const size_t  rgbLen = wcslen(rgb);
    if (_wcsnicmp(wWord.c_str(), rgb, rgbLen) == 0)
    {
        // get the word up to the closing bracket
        int maxlength = 20;
        while ((static_cast<char>(m_editor.Call(SCI_GETCHARAT, ++selEnd)) != ')') && --maxlength)
        {
        }
        if (maxlength == 0)
            return;
        sWord = m_editor.GetTextRange(static_cast<long>(selStart), static_cast<long>(selEnd));
        wWord = CUnicodeUtils::StdGetUnicode(sWord);

        // Grab the data the between brackets that follows the word RGB,
        // if there looks to be 3 elements to it, try to parse each r,g,b element
        // as a number in decimal, hex or octal.
        wWord = wWord.substr(rgbLen, (wWord.length() - rgbLen));
        CStringUtils::TrimLeadingAndTrailing(wWord, std::wstring(L"()"));
        int                       r, g, b;
        std::vector<std::wstring> vec;
        stringtok(vec, wWord, true, L",");
        if (vec.size() == 3 &&
            CAppUtils::TryParse(vec[0].c_str(), r, false, 0, 0) &&
            CAppUtils::TryParse(vec[1].c_str(), g, false, 0, 0) &&
            CAppUtils::TryParse(vec[2].c_str(), b, false, 0, 0))
        {
            // Display the item back as RGB(r,g,b) where each is decimal
            // (since they can already see any other element type that might have used,
            // so all decimal might mean more info)
            // and as a hex color triplet which is useful using # to indicate that.
            auto color    = RGB(r, g, b);
            auto sCallTip = CStringUtils::Format(
                L"RGB(%d, %d, %d)\n#%02X%02X%02X\nHex: 0x%06lX",
                r, g, b, GetRValue(color), GetGValue(color), GetBValue(color), color);

            auto  msgPos = GetMessagePos();
            POINT pt     = {GET_X_LPARAM(msgPos), GET_Y_LPARAM(msgPos)};
            m_custToolTip.ShowTip(pt, sCallTip, &color);
            return;
        }
    }

    // Assume a number format determined by the string itself
    // and as recognized as a valid format according to strtoll:
    // e.g. 0xF0F hex == 3855 decimal == 07417 octal.
    // Use long long to yield more conversion range than say just long.
    char* ep = nullptr;
    // 0 base means determine base from any format in the string.
    errno            = 0;
    long long number = strtoll(CStringUtils::trim(CStringUtils::trim(CStringUtils::trim(sWord, L'('), L','), L')').c_str(), &ep, 0);
    // Be forgiving if given 100xyz, show 100, but
    // don't accept xyz100, show nothing.
    // Must convert some digits of string.
    if (errno == 0 && (*ep == 0))
    {
        auto      bs       = to_bit_wstring(number, true);
        auto      sCallTip = CStringUtils::Format(L"Dec: %lld\nHex: 0x%llX\nOct: %#llo\nBin: %s (%d digits)",
                                             number, number, number, bs.c_str(), static_cast<int>(bs.size()));
        COLORREF  color    = 0;
        COLORREF* pColor   = nullptr;
        if (sWord.size() > 7)
        {
            // may be a color: 0xFF205090 or just 0x205090
            if (sWord[0] == '0' && (sWord[1] == 'x' || sWord[1] == 'X'))
            {
                BYTE r = (number >> 16) & 0xFF;
                BYTE g = (number >> 8) & 0xFF;
                BYTE b = number & 0xFF;
                color  = RGB(r, g, b) | (number & 0xFF000000);
                pColor = &color;
            }
        }
        auto  msgPos = GetMessagePos();
        POINT pt     = {GET_X_LPARAM(msgPos), GET_Y_LPARAM(msgPos)};
        m_custToolTip.ShowTip(pt, sCallTip, pColor);
        return;
    }
    int  err       = 0;
    auto exprValue = te_interp(sWord.c_str(), &err);
    if (err)
    {
        auto wordCharsBuffer = m_editor.GetWordChars();
        OnOutOfScope(m_editor.Call(SCI_SETWORDCHARS, 0, reinterpret_cast<LPARAM>(wordCharsBuffer.c_str())));

        m_editor.Call(SCI_SETWORDCHARS, 0, reinterpret_cast<LPARAM>("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-+*/!%~()_.,"));
        selStart = m_editor.Call(SCI_WORDSTARTPOSITION, scn.position, false);
        selEnd   = m_editor.Call(SCI_WORDENDPOSITION, scn.position, false);
        sWord    = m_editor.GetTextRange(static_cast<long>(selStart), static_cast<long>(selEnd));
    }
    exprValue = te_interp(sWord.c_str(), &err);
    if (err == 0)
    {
        long long ulongVal = static_cast<long long>(exprValue);
        auto      sCallTip = CStringUtils::Format(L"Expr: %S\n-->\nVal: %f\nDec: %lld\nHex: 0x%llX\nOct: %#llo",
                                             sWord.c_str(), exprValue, ulongVal, ulongVal, ulongVal);
        auto      msgPos   = GetMessagePos();
        POINT     pt       = {GET_X_LPARAM(msgPos), GET_Y_LPARAM(msgPos)};
        m_custToolTip.ShowTip(pt, sCallTip, nullptr);
    }
}

void CMainWindow::HandleGetDispInfo(int tab, LPNMTTDISPINFO lpNmtdi)
{
    POINT p;
    GetCursorPos(&p);
    ScreenToClient(*this, &p);
    HWND hWin = RealChildWindowFromPoint(*this, p);
    if (hWin == m_tabBar)
    {
        auto docId = m_tabBar.GetIDFromIndex(tab);
        if (docId.IsValid())
        {
            if (m_docManager.HasDocumentID(docId))
            {
                const auto& doc = m_docManager.GetDocumentFromID(docId);
                m_tooltipBuffer = std::make_unique<wchar_t[]>(doc.m_path.size() + 1);
                wcscpy_s(m_tooltipBuffer.get(), doc.m_path.size() + 1, doc.m_path.c_str());
                lpNmtdi->lpszText = m_tooltipBuffer.get();
                lpNmtdi->hinst    = nullptr;
            }
        }
    }
}

bool CMainWindow::HandleDoubleClick(const SCNotification& scn)
{
    if (!(scn.modifiers & SCMOD_CTRL))
        return false;
    return OpenUrlAtPos(scn.position);
}

bool CMainWindow::OpenUrlAtPos(Sci_Position pos)
{
    auto wordCharsBuffer = m_editor.GetWordChars();
    OnOutOfScope(m_editor.Call(SCI_SETWORDCHARS, 0, reinterpret_cast<LPARAM>(wordCharsBuffer.c_str())));

    m_editor.Call(SCI_SETWORDCHARS, 0, reinterpret_cast<LPARAM>("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-+.,:;?&@=/%#()"));

    Sci_Position startPos = static_cast<Sci_Position>(m_editor.Call(SCI_WORDSTARTPOSITION, pos, false));
    Sci_Position endPos   = static_cast<Sci_Position>(m_editor.Call(SCI_WORDENDPOSITION, pos, false));

    m_editor.Call(SCI_SETTARGETSTART, startPos);
    m_editor.Call(SCI_SETTARGETEND, endPos);
    auto originalSearchFlags = m_editor.Call(SCI_GETSEARCHFLAGS);
    OnOutOfScope(m_editor.Call(SCI_SETSEARCHFLAGS, originalSearchFlags));

    m_editor.Call(SCI_SETSEARCHFLAGS, SCFIND_REGEXP);

    Sci_Position posFound = static_cast<Sci_Position>(m_editor.Call(SCI_SEARCHINTARGET, URL_REG_EXPR_LENGTH, reinterpret_cast<LPARAM>(URL_REG_EXPR)));
    if (posFound != -1)
    {
        startPos = static_cast<Sci_Position>(m_editor.Call(SCI_GETTARGETSTART));
        endPos   = static_cast<Sci_Position>(m_editor.Call(SCI_GETTARGETEND));
    }
    else
        return false;
    std::string urlText = m_editor.GetTextRange(startPos, endPos);
    // This treatment would fail on some valid URLs where there's actually supposed to be a comma or parenthesis at the end.
    CStringUtils::TrimLeadingAndTrailing(urlText, std::vector<char>{',', ')', '('});
    std::wstring url = CUnicodeUtils::StdGetUnicode(urlText);
    SearchReplace(url, L"&amp;", L"&");

    ::ShellExecute(*this, L"open", url.c_str(), nullptr, nullptr, SW_SHOW);

    return true;
}

void CMainWindow::HandleSavePoint(const SCNotification& scn)
{
    auto docID = m_tabBar.GetCurrentTabId();
    if (m_docManager.HasDocumentID(docID))
    {
        auto& doc      = m_docManager.GetModDocumentFromID(docID);
        doc.m_bIsDirty = scn.nmhdr.code == SCN_SAVEPOINTLEFT;
        UpdateTab(docID);
    }
}

void CMainWindow::HandleWriteProtectedEdit()
{
    // user tried to edit a readonly file: ask whether
    // to make the file writable
    auto docID = m_tabBar.GetCurrentTabId();
    if (m_docManager.HasDocumentID(docID))
    {
        auto& doc = m_docManager.GetModDocumentFromID(docID);
        if (!doc.m_bIsWriteProtected && (doc.m_bIsReadonly || (m_editor.Call(SCI_GETREADONLY) != 0)))
        {
            // If the user really wants to edit despite it being read only, let them.
            if (AskToRemoveReadOnlyAttribute())
            {
                doc.m_bIsReadonly = false;
                UpdateTab(docID);
                m_editor.Call(SCI_SETREADONLY, false);
                m_editor.Call(SCI_SETSAVEPOINT);
            }
        }
    }
}

void CMainWindow::AddHotSpots() const
{
    auto firstVisibleLine = m_editor.Call(SCI_GETFIRSTVISIBLELINE);
    auto startPos         = m_editor.Call(SCI_POSITIONFROMLINE, m_editor.Call(SCI_DOCLINEFROMVISIBLE, firstVisibleLine));
    auto linesOnScreen    = m_editor.Call(SCI_LINESONSCREEN);
    auto lineCount        = m_editor.Call(SCI_GETLINECOUNT);
    auto endPos           = m_editor.Call(SCI_POSITIONFROMLINE, m_editor.Call(SCI_DOCLINEFROMVISIBLE, firstVisibleLine + min(linesOnScreen, lineCount)));

    // to speed up the search, first search for "://" without using the regex engine
    auto fStartPos = startPos;
    auto fEndPos   = endPos;
    m_editor.Call(SCI_SETSEARCHFLAGS, 0);
    m_editor.Call(SCI_SETTARGETSTART, fStartPos);
    m_editor.Call(SCI_SETTARGETEND, fEndPos);
    LRESULT posFoundColonSlash = m_editor.Call(SCI_SEARCHINTARGET, 3, reinterpret_cast<sptr_t>("://"));
    while (posFoundColonSlash != -1)
    {
        // found a "://"
        auto lineFoundColonSlash = m_editor.Call(SCI_LINEFROMPOSITION, posFoundColonSlash);
        startPos                 = m_editor.Call(SCI_POSITIONFROMLINE, lineFoundColonSlash);
        endPos                   = m_editor.Call(SCI_GETLINEENDPOSITION, lineFoundColonSlash);
        fStartPos                = posFoundColonSlash + 1LL;

        m_editor.Call(SCI_SETSEARCHFLAGS, SCFIND_REGEXP | SCFIND_CXX11REGEX);

        // 20 chars for the url protocol should be enough
        m_editor.Call(SCI_SETTARGETSTART, max(startPos, posFoundColonSlash - 20));
        // urls longer than 2048 are not handled by browsers
        m_editor.Call(SCI_SETTARGETEND, min(endPos, posFoundColonSlash + 2048));

        LRESULT posFound = m_editor.Call(SCI_SEARCHINTARGET, URL_REG_EXPR_LENGTH, reinterpret_cast<sptr_t>(URL_REG_EXPR));

        if (posFound != -1)
        {
            auto start        = m_editor.Call(SCI_GETTARGETSTART);
            auto end          = m_editor.Call(SCI_GETTARGETEND);
            auto foundTextLen = end - start;

            // reset indicators
            m_editor.Call(SCI_SETINDICATORCURRENT, INDIC_URLHOTSPOT);
            m_editor.Call(SCI_INDICATORCLEARRANGE, start, foundTextLen);
            m_editor.Call(SCI_INDICATORCLEARRANGE, start, foundTextLen - 1LL);

            m_editor.Call(SCI_INDICATORFILLRANGE, start, foundTextLen);
        }
        m_editor.Call(SCI_SETTARGETSTART, fStartPos);
        m_editor.Call(SCI_SETTARGETEND, fEndPos);
        m_editor.Call(SCI_SETSEARCHFLAGS, 0);
        posFoundColonSlash = static_cast<int>(m_editor.Call(SCI_SEARCHINTARGET, 3, reinterpret_cast<sptr_t>("://")));
    }
}

void CMainWindow::HandleUpdateUI(const SCNotification& scn)
{
    if (scn.updated & SC_UPDATE_V_SCROLL)
        m_editor.UpdateLineNumberWidth();
    if (scn.updated & SC_UPDATE_SELECTION)
        m_editor.SelectionUpdated();
    const unsigned int uiFlags = SC_UPDATE_SELECTION |
                                 SC_UPDATE_H_SCROLL | SC_UPDATE_V_SCROLL;
    if ((scn.updated & uiFlags) != 0)
        m_editor.MarkSelectedWord(false, false);

    m_editor.MatchBraces(BraceMatch::Braces);
    m_editor.MatchTags();
    AddHotSpots();
    UpdateStatusBar(false);
}

void CMainWindow::IndentToLastLine() const
{
    auto curLine      = m_editor.GetCurrentLineNumber();
    auto lastLine     = curLine - 1;
    int  indentAmount = 0;
    // use the same indentation as the last line
    while (lastLine > 0 && (m_editor.Call(SCI_GETLINEENDPOSITION, lastLine) - m_editor.Call(SCI_POSITIONFROMLINE, lastLine)) == 0)
        lastLine--;

    indentAmount = static_cast<int>(m_editor.Call(SCI_GETLINEINDENTATION, lastLine));

    if (indentAmount > 0)
    {
        Sci_CharacterRange cRange{};
        cRange.cpMin   = static_cast<Sci_PositionCR>(m_editor.Call(SCI_GETSELECTIONSTART));
        cRange.cpMax   = static_cast<Sci_PositionCR>(m_editor.Call(SCI_GETSELECTIONEND));
        auto posBefore = m_editor.Call(SCI_GETLINEINDENTPOSITION, curLine);
        m_editor.Call(SCI_SETLINEINDENTATION, curLine, indentAmount);
        auto posAfter      = m_editor.Call(SCI_GETLINEINDENTPOSITION, curLine);
        auto posDifference = posAfter - posBefore;
        if (posAfter > posBefore)
        {
            // Move selection on
            if (cRange.cpMin >= posBefore)
                cRange.cpMin += static_cast<Sci_PositionCR>(posDifference);
            if (cRange.cpMax >= posBefore)
                cRange.cpMax += static_cast<Sci_PositionCR>(posDifference);
        }
        else if (posAfter < posBefore)
        {
            // Move selection back
            if (cRange.cpMin >= posAfter)
            {
                if (cRange.cpMin >= posBefore)
                    cRange.cpMin += static_cast<Sci_PositionCR>(posDifference);
                else
                    cRange.cpMin = static_cast<Sci_PositionCR>(posAfter);
            }
            if (cRange.cpMax >= posAfter)
            {
                if (cRange.cpMax >= posBefore)
                    cRange.cpMax += static_cast<Sci_PositionCR>(posDifference);
                else
                    cRange.cpMax = static_cast<Sci_PositionCR>(posAfter);
            }
        }
        m_editor.Call(SCI_SETSEL, cRange.cpMin, cRange.cpMax);
    }
}

void CMainWindow::HandleAutoIndent(const SCNotification& scn) const
{
    if (m_bBlockAutoIndent)
        return;
    int eolMode = static_cast<int>(m_editor.Call(SCI_GETEOLMODE));

    if (((eolMode == SC_EOL_CRLF || eolMode == SC_EOL_LF) && scn.ch == '\n') ||
        (eolMode == SC_EOL_CR && scn.ch == '\r'))
    {
        IndentToLastLine();
    }
}

void CMainWindow::OpenNewTab()
{
    OnOutOfScope(
        m_insertionIndex = -1;);

    m_tabBar.SelectChanging(-1);

    CDocument doc;
    doc.m_document = m_editor.Call(SCI_CREATEDOCUMENT);
    doc.m_bHasBOM  = CIniSettings::Instance().GetInt64(L"Defaults", L"encodingnewbom", 0) != 0;
    doc.m_encoding = static_cast<UINT>(CIniSettings::Instance().GetInt64(L"Defaults", L"encodingnew", GetACP()));
    doc.m_format   = static_cast<EOLFormat>(CIniSettings::Instance().GetInt64(L"Defaults", L"lineendingnew", static_cast<int>(EOLFormat::Win_Format)));
    auto eolMode   = toEolMode(doc.m_format);
    m_editor.SetEOLType(eolMode);

    doc.SetLanguage("Text");
    std::wstring tabName = GetNewTabName();
    int          index   = -1;
    if (m_insertionIndex >= 0)
    {
        index            = m_tabBar.InsertAfter(m_insertionIndex, tabName.c_str());
        m_insertionIndex = -1;
    }
    else
        index = m_tabBar.InsertAtEnd(tabName.c_str());
    auto docID = m_tabBar.GetIDFromIndex(index);
    m_docManager.AddDocumentAtEnd(doc, docID);
    CCommandHandler::Instance().OnDocumentOpen(docID);

    m_tabBar.SelectChange(index);
    //m_editor.GotoLine(0);
}

void CMainWindow::HandleTabChanging(const NMHDR& /*nmhdr*/)
{
    // document is about to be deactivated
    auto docID = m_tabBar.GetCurrentTabId();
    if (m_docManager.HasDocumentID(docID))
    {
        m_editor.MatchBraces(BraceMatch::Clear);
        auto& doc = m_docManager.GetModDocumentFromID(docID);
        m_editor.SaveCurrentPos(doc.m_position);
    }
}

void CMainWindow::HandleTabChange(const NMHDR& /*nmhdr*/)
{
    int curTab = m_tabBar.GetCurrentTabIndex();
    // document got activated
    auto docID = m_tabBar.GetCurrentTabId();
    // Can't do much if no document for this tab.
    if (!m_docManager.HasDocumentID(docID))
        return;

    auto& doc = m_docManager.GetModDocumentFromID(docID);
    m_editor.Call(SCI_SETDOCPOINTER, 0, doc.m_document);
    m_editor.SetEOLType(toEolMode(doc.m_format));
    m_editor.SetupLexerForLang(doc.GetLanguage());
    m_editor.RestoreCurrentPos(doc.m_position);
    m_editor.SetTabSettings(doc.m_tabSpace);
    m_editor.SetReadDirection(doc.m_readDir);
    CEditorConfigHandler::Instance().ApplySettingsForPath(doc.m_path, &m_editor, doc, true);
    CCommandHandler::Instance().OnStylesSet();
    g_pFramework->InvalidateUICommand(cmdUseTabs, UI_INVALIDATIONS_PROPERTY, &UI_PKEY_BooleanValue);
    m_editor.MarkSelectedWord(true, false);
    m_editor.MarkBookmarksInScrollbar();
    UpdateCaptionBar();
    SetFocus(m_editor);
    m_editor.Call(SCI_GRABFOCUS);
    UpdateStatusBar(true);
    auto ds = m_docManager.HasFileChanged(docID);
    if (ds == DocModifiedState::Modified)
    {
        ReloadTab(curTab, -1, true);
    }
    else if (ds == DocModifiedState::Removed)
    {
        HandleOutsideDeletedFile(curTab);
    }
}

void CMainWindow::HandleTabDelete(const TBHDR& tbHdr)
{
    int tabToDelete = tbHdr.tabOrigin;
    // Close tab will take care of activating any next tab.
    CloseTab(tabToDelete);
}

int CMainWindow::OpenFile(const std::wstring& file, unsigned int openFlags)
{
    int  index                 = -1;
    bool bAddToMRU             = (openFlags & OpenFlags::AddToMRU) != 0;
    bool bAskToCreateIfMissing = (openFlags & OpenFlags::AskToCreateIfMissing) != 0;
    bool bCreateIfMissing      = (openFlags & OpenFlags::CreateIfMissing) != 0;
    bool bIgnoreIfMissing      = (openFlags & OpenFlags::IgnoreIfMissing) != 0;
    bool bOpenIntoActiveTab    = (openFlags & OpenFlags::OpenIntoActiveTab) != 0;
    // Ignore no activate flag for now. It causes too many issues.
    bool bActivate      = true; //(openFlags & OpenFlags::NoActivate) == 0;
    bool bCreateTabOnly = (openFlags & OpenFlags::CreateTabOnly) != 0;

    if (bCreateTabOnly)
    {
        auto      fileName = CPathUtils::GetFileName(file);
        CDocument doc;
        doc.m_document     = m_editor.Call(SCI_CREATEDOCUMENT);
        doc.m_bHasBOM      = CIniSettings::Instance().GetInt64(L"Defaults", L"encodingnewbom", 0) != 0;
        doc.m_encoding     = static_cast<UINT>(CIniSettings::Instance().GetInt64(L"Defaults", L"encodingnew", GetACP()));
        doc.m_bNeedsSaving = true;
        doc.m_bDoSaveAs    = true;
        doc.m_path         = file;
        auto lang          = CLexStyles::Instance().GetLanguageForPath(fileName);
        if (lang.empty())
            lang = "Text";
        doc.SetLanguage(lang);
        index      = m_tabBar.InsertAtEnd(fileName.c_str());
        auto docID = m_tabBar.GetIDFromIndex(index);
        m_docManager.AddDocumentAtEnd(doc, docID);
        UpdateTab(docID);
        UpdateStatusBar(true);
        m_tabBar.ActivateAt(index);
        CCommandHandler::Instance().OnDocumentOpen(docID);

        return index;
    }

    // if we're opening the first file, we have to activate it
    // to ensure all the activation stuff is handled for that first
    // file.
    if (m_tabBar.GetItemCount() == 0)
        bActivate = true;

    int          encoding = -1;
    std::wstring filepath = CPathUtils::GetLongPathname(file);
    if (_wcsicmp(CIniSettings::Instance().GetIniPath().c_str(), filepath.c_str()) == 0)
    {
        CIniSettings::Instance().Save();
    }
    auto id = m_docManager.GetIdForPath(filepath);
    if (id.IsValid())
    {
        index = m_tabBar.GetIndexFromID(id);
        // document already open.
        if (IsWindowEnabled(*this) && m_tabBar.GetCurrentTabIndex() != index)
        {
            // only activate the new doc tab if the main window is enabled:
            // if it's disabled, a modal dialog is shown
            // (e.g., the handle-outside-modifications confirmation dialog)
            // and we then must not change the active tab.
            m_tabBar.ActivateAt(index);
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

        CDocument doc = m_docManager.LoadFile(*this, filepath, encoding, createIfMissing);
        if (doc.m_document)
        {
            DocID activeTabId;
            if (bOpenIntoActiveTab)
            {
                activeTabId           = m_tabBar.GetCurrentTabId();
                const auto& activeDoc = m_docManager.GetDocumentFromID(activeTabId);
                if (!activeDoc.m_bIsDirty && !activeDoc.m_bNeedsSaving)
                    m_docManager.RemoveDocument(activeTabId);
                else
                    activeTabId = DocID();
            }
            if (!activeTabId.IsValid())
            {
                // check if the only tab is empty and if it is, remove it
                auto docID = m_tabBar.GetCurrentTabId();
                if (docID.IsValid())
                {
                    const auto& existDoc = m_docManager.GetDocumentFromID(docID);
                    if (existDoc.m_path.empty() && (m_editor.Call(SCI_GETLENGTH) == 0) && (m_editor.Call(SCI_CANUNDO) == 0))
                    {
                        auto curTabIndex = m_tabBar.GetCurrentTabIndex();
                        CCommandHandler::Instance().OnDocumentClose(docID);
                        m_insertionIndex = curTabIndex;
                        m_tabBar.DeleteItemAt(m_insertionIndex);
                        if (m_insertionIndex)
                            --m_insertionIndex;
                        // Prefer to remove the document after the tab has gone as it supports it
                        // and deletion causes events that may expect it to be there.
                        m_docManager.RemoveDocument(docID);
                    }
                }
            }
            if (bAddToMRU)
                CMRU::Instance().AddPath(filepath);
            std::wstring sFileName = CPathUtils::GetFileName(filepath);
            if (!activeTabId.IsValid())
            {
                if (m_insertionIndex >= 0)
                    index = m_tabBar.InsertAfter(m_insertionIndex, sFileName.c_str());
                else
                    index = m_tabBar.InsertAtEnd(sFileName.c_str());
                id = m_tabBar.GetIDFromIndex(index);
            }
            else
            {
                index = m_tabBar.GetCurrentTabIndex();
                m_tabBar.SetCurrentTabId(activeTabId);
                id = activeTabId;
                m_tabBar.SetCurrentTitle(sFileName.c_str());
            }
            doc.SetLanguage(CLexStyles::Instance().GetLanguageForDocument(doc, m_scratchEditor));

            if (CPathUtils::PathCompare(filepath, m_tabMovePath) == 0)
            {
                filepath = m_tabMoveSavePath;
            }
            // check if the file is special, i.e. if we need to do something
            // when the file is saved
            auto sExt = CStringUtils::to_lower(CPathUtils::GetFileExtension(doc.m_path));
            if (sExt == L"bpj" || sExt == L"bpv")
            {
                doc.m_saveCallback = [this]() { CCommandHandler::Instance().InsertPlugins(this); };
            }
            else if (sExt == L"ini")
            {
                if (doc.m_path == CIniSettings::Instance().GetIniPath())
                {
                    doc.m_saveCallback = []() { CIniSettings::Instance().Reload(); };
                }
            }
            else if (sExt == L"bplex")
            {
                doc.m_saveCallback = []() { CLexStyles::Instance().Reload(); };
            }

            m_docManager.AddDocumentAtEnd(doc, id);
            doc = m_docManager.GetDocumentFromID(id);

            // only activate the new doc tab if the main window is enabled:
            // if it's disabled, a modal dialog is shown
            // (e.g., the handle-outside-modifications confirmation dialog)
            // and we then must not change the active tab.
            if (IsWindowEnabled(*this))
            {
                bool bResize = m_fileTree.GetPath().empty() && !doc.m_path.empty();
                if (bActivate)
                {
                    m_tabBar.ActivateAt(index);
                }
                else
                    // SCI_SETDOCPOINTER is necessary so the reference count of the document
                    // is decreased and the memory can be released.
                    m_editor.Call(SCI_SETDOCPOINTER, 0, doc.m_document);
                if (bResize)
                    ResizeChildWindows();
            }
            else
                m_editor.Call(SCI_SETDOCPOINTER, 0, doc.m_document);

            CEditorConfigHandler::Instance().ApplySettingsForPath(doc.m_path, &m_editor, doc, false);
            InvalidateRect(m_tabBar, nullptr, FALSE);

            if (bAddToMRU)
                SHAddToRecentDocs(SHARD_PATHW, filepath.c_str());
            if (m_fileTree.GetPath().empty())
            {
                m_fileTree.SetPath(CPathUtils::GetParentDirectory(filepath), false);
                ResizeChildWindows();
            }
            UpdateTab(id);
            CCommandHandler::Instance().OnDocumentOpen(id);
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
bool CMainWindow::OpenFileAs(const std::wstring& tempPath, const std::wstring& realpath, bool bModified)
{
    // If we can't open it, not much we can do.
    if (OpenFile(tempPath, 0) < 0)
    {
        DeleteFile(tempPath.c_str());
        return false;
    }

    // Get the id for the document we just loaded,
    // it'll currently have a temporary name.
    auto  docID        = m_docManager.GetIdForPath(tempPath);
    auto& doc          = m_docManager.GetModDocumentFromID(docID);
    doc.m_path         = CPathUtils::GetLongPathname(realpath);
    doc.m_bIsDirty     = bModified;
    doc.m_bNeedsSaving = bModified;
    m_docManager.UpdateFileTime(doc, true);
    std::wstring sFileName = CPathUtils::GetFileName(doc.m_path);
    const auto&  lang      = CLexStyles::Instance().GetLanguageForDocument(doc, m_scratchEditor);
    m_editor.Call(SCI_SETREADONLY, doc.m_bIsReadonly);
    m_editor.SetupLexerForLang(lang);
    doc.SetLanguage(lang);
    UpdateTab(docID);
    if (sFileName.empty())
        m_tabBar.SetCurrentTitle(GetNewTabName().c_str());
    else
        m_tabBar.SetCurrentTitle(sFileName.c_str());
    UpdateCaptionBar();
    UpdateStatusBar(true);
    DeleteFile(tempPath.c_str());

    return true;
}

// Called when the user drops a selection of files (or a folder!) onto
// BowPad's main window. The response is to try to load all those files.
// Note: this function is also called from clipboard functions, so we must
// not call DragFinish() on the hDrop object here!
void CMainWindow::HandleDropFiles(HDROP hDrop)
{
    if (!hDrop)
        return;
    int                       filesDropped = DragQueryFile(hDrop, 0xffffffff, nullptr, 0);
    std::vector<std::wstring> files;
    for (int i = 0; i < filesDropped; ++i)
    {
        UINT len     = DragQueryFile(hDrop, i, nullptr, 0);
        auto pathBuf = std::make_unique<wchar_t[]>(len + 2);
        DragQueryFile(hDrop, i, pathBuf.get(), len + 1);
        files.push_back(pathBuf.get());
    }

    if (files.size() == 1)
    {
        if (CPathUtils::GetFileExtension(files[0]) == L"bplex")
        {
            // ask whether to install or open the file
            ResString    rTitle(g_hRes, IDS_IMPORTBPLEX_TITLE);
            ResString    rQuestion(g_hRes, IDS_IMPORTBPLEX_QUESTION);
            ResString    rImport(g_hRes, IDS_IMPORTBPLEX_IMPORT);
            ResString    rOpen(g_hRes, IDS_IMPORTBPLEX_OPEN);
            std::wstring sQuestion = CStringUtils::Format(rQuestion, CPathUtils::GetFileName(files[0]).c_str());

            TASKDIALOGCONFIG tdc                = {sizeof(TASKDIALOGCONFIG)};
            tdc.dwFlags                         = TDF_USE_COMMAND_LINKS | TDF_POSITION_RELATIVE_TO_WINDOW | TDF_SIZE_TO_CONTENT | TDF_ALLOW_DIALOG_CANCELLATION;
            TASKDIALOG_BUTTON aCustomButtons[2] = {};
            aCustomButtons[0].nButtonID         = 100;
            aCustomButtons[0].pszButtonText     = rImport;
            aCustomButtons[1].nButtonID         = 101;
            aCustomButtons[1].pszButtonText     = rOpen;
            tdc.pButtons                        = aCustomButtons;
            tdc.cButtons                        = _countof(aCustomButtons);
            assert(tdc.cButtons <= _countof(aCustomButtons));
            tdc.nDefaultButton = 100;

            tdc.hwndParent         = *this;
            tdc.hInstance          = g_hRes;
            tdc.dwCommonButtons    = TDCBF_CANCEL_BUTTON;
            tdc.pszWindowTitle     = MAKEINTRESOURCE(IDS_APP_TITLE);
            tdc.pszMainIcon        = TD_INFORMATION_ICON;
            tdc.pszMainInstruction = rTitle;
            tdc.pszContent         = sQuestion.c_str();
            int     nClickedBtn    = 0;
            HRESULT hr             = TaskDialogIndirect(&tdc, &nClickedBtn, nullptr, &closeAllDoAll);
            if (CAppUtils::FailedShowMessage(hr))
                nClickedBtn = 0;
            if (nClickedBtn == 100)
            {
                // import the file
                CopyFile(files[0].c_str(), (CAppUtils::GetDataPath() + L"\\" + CPathUtils::GetFileName(files[0])).c_str(), FALSE);
                return;
            }
        }
    }

    BlockAllUIUpdates(true);
    OnOutOfScope(BlockAllUIUpdates(false););

    ShowProgressCtrl(static_cast<UINT>(CIniSettings::Instance().GetInt64(L"View", L"progressdelay", 1000)));
    OnOutOfScope(HideProgressCtrl());

    const size_t maxFiles    = 100;
    int          fileCounter = 0;
    for (const auto& filename : files)
    {
        ++fileCounter;
        SetProgress(fileCounter, static_cast<DWORD>(files.size()));
        if (PathIsDirectory(filename.c_str()))
        {
            std::vector<std::wstring> recurseFiles;
            CDirFileEnum              enumerator(filename);
            bool                      bIsDir = false;
            std::wstring              path;
            // Collect no more than maxFiles + 1 files. + 1 so we know we have too many.
            while (enumerator.NextFile(path, &bIsDir, false))
            {
                if (!bIsDir)
                {
                    recurseFiles.push_back(std::move(path));
                    if (recurseFiles.size() > maxFiles)
                        break;
                }
            }
            if (recurseFiles.size() <= maxFiles)
            {
                std::sort(recurseFiles.begin(), recurseFiles.end(),
                          [](const std::wstring& lhs, const std::wstring& rhs) {
                              return CPathUtils::PathCompare(lhs, rhs) < 0;
                          });

                for (const auto& f : recurseFiles)
                    OpenFile(f, 0);
            }
        }
        else
            OpenFile(filename, OpenFlags::AddToMRU);
    }
}

void CMainWindow::HandleCopyDataCommandLine(const COPYDATASTRUCT& cds)
{
    CCmdLineParser parser(static_cast<LPCWSTR>(cds.lpData));
    LPCTSTR        path = parser.GetVal(L"path");
    if (path)
    {
        if (PathIsDirectory(path))
        {
            if (!m_fileTree.GetPath().empty()) // File tree not empty: create a new empty tab first.
                OpenNewTab();
            m_fileTree.SetPath(path);
            ShowFileTree(true);
        }
        else
        {
            auto list = GetFileListFromGlobPath(path);
            for (const auto p : list)
            {
                if (OpenFile(p, OpenFlags::AddToMRU | OpenFlags::AskToCreateIfMissing) >= 0)
                {
                    if (parser.HasVal(L"line"))
                    {
                        GoToLine(static_cast<size_t>(parser.GetLongLongVal(L"line") - 1));
                    }
                }
            }
        }
    }
    else
    {
        // find out if there are paths specified without the key/value pair syntax
        int nArgs = 0;

        LPCWSTR* szArgList = const_cast<LPCWSTR*>(CommandLineToArgvW(static_cast<LPCWSTR>(cds.lpData), &nArgs));
        OnOutOfScope(LocalFree(szArgList););
        if (!szArgList)
            return;

        BlockAllUIUpdates(true);
        OnOutOfScope(BlockAllUIUpdates(false););
        int filesOpened = 0;
        // no need for a progress dialog here:
        // the command line is limited in size, so there can't be too many
        // filePaths passed here
        for (int i = 1; i < nArgs; i++)
        {
            if (szArgList[i][0] != '/')
            {
                if (PathIsDirectory(szArgList[i]))
                {
                    if (!m_fileTree.GetPath().empty()) // File tree not empty: create a new empty tab first.
                        OpenNewTab();
                    m_fileTree.SetPath(szArgList[i]);
                    ShowFileTree(true);
                }
                else
                {
                    auto list = GetFileListFromGlobPath(szArgList[i]);
                    for (const auto p : list)
                    {
                        if (OpenFile(p, OpenFlags::AddToMRU | OpenFlags::AskToCreateIfMissing) >= 0)
                            ++filesOpened;
                    }
                }
            }
        }

        if ((filesOpened == 1) && parser.HasVal(L"line"))
        {
            GoToLine(static_cast<size_t>(parser.GetLongLongVal(L"line") - 1));
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
// have it open themselves and was just instructing the receiver to load it;
// if the receiver refuses, nothing will happen and that would be also wrong.
//
// The receiver could load the senders document but the receiver might have
// modified their version, in which case we don't know if it's
// safe to replace it and if we silently do they may lose something.
//
// Even if the receivers document isn't modified, there is no
// guarantee that the senders document is newer.
//
// The design probably needs to change so that the receiver asks
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
// Callers using SendMessage can check if the receiver
// loaded the tab they sent before they close their tab
// it was sent from.

// TODO!: Moving a tab to another instance means losing
// undo history.
// Consider warning about that or if the undo history
// could be saved and restored.

// Called when a Tab is dropped over another instance of BowPad.
bool CMainWindow::HandleCopyDataMoveTab(const COPYDATASTRUCT& cds)
{
    std::wstring              paths = std::wstring(static_cast<const wchar_t*>(cds.lpData), cds.cbData / sizeof(wchar_t));
    std::vector<std::wstring> dataVec;
    stringtok(dataVec, paths, false, L"*");
    // Be a little untrusting of the clipboard data and if it's
    // a mistake by the caller let them know so they
    // don't throw away something we can't open.
    if (dataVec.size() != 4)
    {
        APPVERIFY(false); // Assume a bug if testing.
        return false;
    }
    std::wstring realPath = dataVec[0];
    std::wstring tempPath = dataVec[1];
    bool         bMod     = _wtoi(dataVec[2].c_str()) != 0;
    int          line     = _wtoi(dataVec[3].c_str());

    // Unsaved files / New tabs have an empty real path.

    if (!realPath.empty()) // If this is a saved file
    {
        auto docID = m_docManager.GetIdForPath(realPath);
        if (docID.IsValid()) // If it already exists switch to it.
        {
            // TODO: we can lose work here, see notes above.
            // The document we switch to may be the same.
            // better to reject the move and return false or something.

            int tab = m_tabBar.GetIndexFromID(docID);
            m_tabBar.ActivateAt(tab);
            DeleteFile(tempPath.c_str());

            return true;
        }
        // If it doesn't exist, fall through to load it.
    }
    bool opened = OpenFileAs(tempPath, realPath, bMod);
    if (opened)
        GoToLine(line);
    return opened;
}

static bool AskToCopyOrMoveFile(HWND hWnd, const std::wstring& filename, const std::wstring& hitpath, bool bCopy)
{
    ResString rTitle(g_hRes, IDS_FILE_DROP);
    ResString rQuestion(g_hRes, bCopy ? IDS_FILE_DROP_COPY : IDS_FILE_DROP_MOVE);
    ResString rDoIt(g_hRes, bCopy ? IDS_FILE_DROP_DOCOPY : IDS_FILE_DROP_DOMOVE);
    ResString rCancel(g_hRes, IDS_FILE_DROP_CANCEL);
    // Show exactly what we are creating.
    std::wstring sQuestion = CStringUtils::Format(rQuestion, filename.c_str(), hitpath.c_str());

    TASKDIALOGCONFIG  tdc               = {sizeof(TASKDIALOGCONFIG)};
    TASKDIALOG_BUTTON aCustomButtons[2] = {};
    int               bi                = 0;
    aCustomButtons[bi].nButtonID        = 101;
    aCustomButtons[bi++].pszButtonText  = rDoIt;
    aCustomButtons[bi].nButtonID        = 100;
    aCustomButtons[bi++].pszButtonText  = rCancel;
    tdc.pButtons                        = aCustomButtons;
    tdc.cButtons                        = bi;
    assert(tdc.cButtons <= _countof(aCustomButtons));
    tdc.nDefaultButton = 101;

    tdc.hwndParent         = hWnd;
    tdc.hInstance          = g_hRes;
    tdc.dwFlags            = TDF_USE_COMMAND_LINKS | TDF_POSITION_RELATIVE_TO_WINDOW | TDF_SIZE_TO_CONTENT | TDF_ALLOW_DIALOG_CANCELLATION;
    tdc.pszWindowTitle     = MAKEINTRESOURCE(IDS_APP_TITLE);
    tdc.pszMainIcon        = TD_INFORMATION_ICON;
    tdc.pszMainInstruction = rTitle;
    tdc.pszContent         = sQuestion.c_str();
    int     nClickedBtn    = 0;
    HRESULT hr             = TaskDialogIndirect(&tdc, &nClickedBtn, nullptr, nullptr);
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
    std::wstring tempPath    = CTempFiles::Instance().GetTempFilePath(true);
    auto         docID       = m_tabBar.GetIDFromIndex(tab);
    auto&        doc         = m_docManager.GetModDocumentFromID(docID);
    HWND         hDroppedWnd = WindowFromPoint(pt);
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
        if (hMainWnd && !isThisInstance)
        {
            // dropped on another BowPad Window, 'move' this tab to that BowPad Window
            CDocument tempDoc = doc;
            tempDoc.m_path    = tempPath;
            bool bDummy       = false;
            bool bModified    = doc.m_bIsDirty || doc.m_bNeedsSaving;
            m_docManager.SaveFile(*this, tempDoc, bDummy);

            COPYDATASTRUCT cpd  = {0};
            cpd.dwData          = CD_COMMAND_MOVETAB;
            std::wstring cpData = doc.m_path + L"*" + tempPath + L"*";
            cpData += bModified ? L"1*" : L"0*";
            cpData += std::to_wstring(m_editor.GetCurrentLineNumber() + 1);
            cpd.lpData = static_cast<PVOID>(const_cast<LPWSTR>(cpData.c_str()));
            cpd.cbData = static_cast<DWORD>(cpData.size() * sizeof(wchar_t));

            // It's an important concept that the receiver can reject/be unable
            // to load / handle the file/tab we are trying to move to them.
            // We don't want the user to lose their work if that happens.
            // So only close this tab if the move was successful.
            bool moved = SendMessage(hMainWnd, WM_COPYDATA, reinterpret_cast<WPARAM>(m_hwnd), reinterpret_cast<LPARAM>(&cpd)) ? true : false;
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
        if ((hDroppedWnd == m_fileTree) && (!doc.m_path.empty()))
        {
            // drop over file tree
            auto hitPath = m_fileTree.GetDirPathForHitItem();
            if (!hitPath.empty())
            {
                auto fileName = CPathUtils::GetFileName(doc.m_path);
                auto destPath = CPathUtils::Append(hitPath, fileName);

                if (CPathUtils::PathCompare(doc.m_path, destPath))
                {
                    bool bCopy = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
                    if (AskToCopyOrMoveFile(*this, fileName, destPath, bCopy))
                    {
                        if (bCopy)
                        {
                            CDocument tempDoc = doc;
                            tempDoc.m_path    = destPath;
                            bool bDummy       = false;
                            m_docManager.SaveFile(*this, tempDoc, bDummy);
                            OpenFile(destPath, OpenFlags::AddToMRU);
                        }
                        else
                        {
                            if (MoveFile(doc.m_path.c_str(), destPath.c_str()))
                            {
                                doc.m_path = destPath;
                                m_tabBar.ActivateAt(tab);
                            }
                        }
                    }
                    return;
                }
            }
        }
    }
    CDocument tempDoc = doc;
    tempDoc.m_path    = tempPath;
    bool bDummy       = false;
    bool bModified    = doc.m_bIsDirty || doc.m_bNeedsSaving;
    m_docManager.SaveFile(*this, tempDoc, bDummy);

    // have the plugins save any information for this document
    // before we start the new process!
    CCommandHandler::Instance().OnDocumentClose(docID);

    // Start a new instance and open the tab there.
    std::wstring modPath = CPathUtils::GetModulePath();
    std::wstring cmdLine = CStringUtils::Format(L"/multiple /tabmove /savepath:\"%s\" /path:\"%s\" /line:%lld /title:\"%s\"",
                                                doc.m_path.c_str(), tempPath.c_str(),
                                                m_editor.GetCurrentLineNumber() + 1,
                                                m_tabBar.GetTitle(tab).c_str());
    if (bModified)
        cmdLine += L" /modified";
    SHELLEXECUTEINFO shExecInfo = {};
    shExecInfo.cbSize           = sizeof(SHELLEXECUTEINFO);

    shExecInfo.hwnd         = *this;
    shExecInfo.lpVerb       = L"open";
    shExecInfo.lpFile       = modPath.c_str();
    shExecInfo.lpParameters = cmdLine.c_str();
    shExecInfo.nShow        = SW_NORMAL;

    if (ShellExecuteEx(&shExecInfo))
        CloseTab(tab, true);
}

bool CMainWindow::ReloadTab(int tab, int encoding, bool dueToOutsideChanges)
{
    auto docID = m_tabBar.GetIDFromIndex(tab);
    if (!docID.IsValid()) // No such tab.
        return false;
    if (!m_docManager.HasDocumentID(docID)) // No such document
        return false;
    bool  bReloadCurrentTab = (tab == m_tabBar.GetCurrentTabIndex());
    auto& doc               = m_docManager.GetModDocumentFromID(docID);
    // if encoding is set to default, use the current encoding
    if (encoding == -1)
        encoding = doc.m_encoding;

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
            SaveDoc(docID);
        }
        else // Cancel or failed to ask
        {
            // update the fileTime of the document to avoid this warning
            m_docManager.UpdateFileTime(doc, false);
            // the current content of the tab is possibly different
            // than the content on disk: mark the content as dirty
            // so the user knows he can save the changes.
            doc.m_bIsDirty     = true;
            doc.m_bNeedsSaving = true;
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
    CDocument docReload = m_docManager.LoadFile(*this, doc.m_path, encoding, false);
    if (!docReload.m_document)
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
        editor->Call(SCI_SETDOCPOINTER, 0, docReload.m_document);
    }

    docReload.m_position          = doc.m_position;
    docReload.m_bIsWriteProtected = doc.m_bIsWriteProtected;
    docReload.m_saveCallback      = doc.m_saveCallback;
    auto lang                     = doc.GetLanguage();
    doc                           = docReload;
    editor->SetupLexerForLang(lang);
    doc.SetLanguage(lang);
    editor->RestoreCurrentPos(docReload.m_position);
    editor->Call(SCI_SETREADONLY, docReload.m_bIsWriteProtected);
    CEditorConfigHandler::Instance().ApplySettingsForPath(doc.m_path, editor, doc, false);

    TBHDR tbHdr        = {};
    tbHdr.hdr.hwndFrom = *this;
    tbHdr.hdr.code     = TCN_RELOAD;
    tbHdr.hdr.idFrom   = tab;
    tbHdr.tabOrigin    = tab;
    CCommandHandler::Instance().TabNotify(&tbHdr);
    CCommandHandler::Instance().OnStylesSet();

    if (bReloadCurrentTab)
        UpdateStatusBar(true);
    UpdateTab(docID);
    if (bReloadCurrentTab)
        editor->Call(SCI_SETSAVEPOINT);

    // refresh the file tree
    m_fileTree.SetPath(m_fileTree.GetPath(), !dueToOutsideChanges);

    return true;
}

// Return true if requested to removed document.
bool CMainWindow::HandleOutsideDeletedFile(int tab)
{
    assert(tab == m_tabBar.GetCurrentTabIndex());
    auto  docID = m_tabBar.GetIDFromIndex(tab);
    auto& doc   = m_docManager.GetModDocumentFromID(docID);
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
    // update the fileTime of the document to avoid this warning
    m_docManager.UpdateFileTime(doc, false);
    // the next to calls are only here to trigger SCN_SAVEPOINTLEFT/SCN_SAVEPOINTREACHED messages
    m_editor.Call(SCI_ADDUNDOACTION, 0, 0);
    m_editor.Call(SCI_UNDO);
    return false;
}

bool CMainWindow::HasOutsideChangesOccurred() const
{
    int tabCount = m_tabBar.GetItemCount();
    for (int i = 0; i < tabCount; ++i)
    {
        auto docID = m_tabBar.GetIDFromIndex(i);
        auto ds    = m_docManager.HasFileChanged(docID);
        if (ds == DocModifiedState::Modified || ds == DocModifiedState::Removed)
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
    bool bChangedTab = false;
    int  activeTab   = m_tabBar.GetCurrentTabIndex();
    {
        responseToOutsideModifiedFileDoAll = FALSE;
        doModifiedAll                      = true;
        OnOutOfScope(
            responseToOutsideModifiedFileDoAll = FALSE;
            doModifiedAll                      = false;);
        for (int i = 0; i < m_tabBar.GetItemCount(); ++i)
        {
            auto docID = m_tabBar.GetIDFromIndex(i);
            auto ds    = m_docManager.HasFileChanged(docID);
            if (ds == DocModifiedState::Modified || ds == DocModifiedState::Removed)
            {
                const auto& doc = m_docManager.GetDocumentFromID(docID);
                if ((ds != DocModifiedState::Removed) && !doc.m_bIsDirty && !doc.m_bNeedsSaving && CIniSettings::Instance().GetInt64(L"View", L"autorefreshifnotmodified", 1))
                {
                    ReloadTab(i, -1, true);
                }
                else
                {
                    m_tabBar.ActivateAt(i);
                    if (i != activeTab)
                        bChangedTab = true;
                }
            }
        }
    }

    if (bChangedTab)
        m_tabBar.ActivateAt(activeTab);
}

bool CMainWindow::OnMouseMove(UINT nFlags, POINT point)
{
    if (m_bDragging)
    {
        if ((nFlags & MK_LBUTTON) != 0 && (point.x != m_oldPt.x))
        {
            SetFileTreeWidth(point.x);
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
        RECT rcTree;
        GetWindowRect(m_fileTree, &rcTree);
        MapWindowPoints(nullptr, *this, reinterpret_cast<LPPOINT>(&rcTree), 2);
        if (point.y < rcTree.top)
            return true;
        if (point.y > rcTree.bottom)
            return true;
        if (point.x < rcTree.right - 20)
            return true;
        if (point.x > rcTree.right + 20)
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

    SetFileTreeWidth(point.x);

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

COLORREF CMainWindow::GetColorForDocument(DocID id)
{
    const CDocument& doc        = m_docManager.GetDocumentFromID(id);
    std::wstring     folderPath = CPathUtils::GetParentDirectory(doc.m_path);
    // note: if the folder path is different in case but points to
    // the same folder on disk, the color will be different!
    auto foundIt = m_folderColorIndexes.find(folderPath);
    if (foundIt != m_folderColorIndexes.end())
        return folderColors[foundIt->second % MAX_FOLDERCOLORS];

    m_folderColorIndexes[folderPath] = m_lastFolderColorIndex;
    return folderColors[m_lastFolderColorIndex++ % MAX_FOLDERCOLORS];
}

void CMainWindow::OpenFiles(const std::vector<std::wstring>& paths)
{
    if (paths.size() == 1)
    {
        if (!paths[0].empty())
        {
            unsigned int openFlags = OpenFlags::AddToMRU;
            if ((GetKeyState(VK_CONTROL) & 0x8000) != 0)
                openFlags |= OpenFlags::OpenIntoActiveTab;
            OpenFile(paths[0], openFlags);
        }
    }
    else if (paths.size() > 0)
    {
        BlockAllUIUpdates(true);
        OnOutOfScope(BlockAllUIUpdates(false));
        ShowProgressCtrl(static_cast<UINT>(CIniSettings::Instance().GetInt64(L"View", L"progressdelay", 1000)));
        OnOutOfScope(HideProgressCtrl());

        // Open all that was selected or at least returned.
        DocID docToActivate;
        int   fileCounter = 0;
        for (const auto& file : paths)
        {
            ++fileCounter;
            SetProgress(fileCounter, static_cast<DWORD32>(paths.size()));
            // Remember whatever we first successfully open in order to return to it.
            if (OpenFile(file, OpenFlags::AddToMRU) >= 0 && !docToActivate.IsValid())
                docToActivate = m_docManager.GetIdForPath(file);
        }

        if (docToActivate.IsValid())
        {
            auto tabToActivate = m_tabBar.GetIndexFromID(docToActivate);
            m_tabBar.ActivateAt(tabToActivate);
        }
    }
}

void CMainWindow::BlockAllUIUpdates(bool block)
{
    if (block)
    {
        if (m_blockCount == 0)
            SendMessage(*this, WM_SETREDRAW, FALSE, 0);
        FileTreeBlockRefresh(block);
        ++m_blockCount;
    }
    else
    {
        --m_blockCount;
        APPVERIFY(m_blockCount >= 0);
        if (m_blockCount == 0)
        {
            // unblock
            SendMessage(*this, WM_SETREDRAW, TRUE, 0);
            // force a redraw
            RedrawWindow(*this, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_INTERNALPAINT | RDW_ALLCHILDREN | RDW_UPDATENOW);
        }
        // FileTreeBlockRefresh maintains it's own count.
        FileTreeBlockRefresh(block);
        if (m_blockCount == 0)
            ResizeChildWindows();
    }
}

int CMainWindow::UnblockUI()
{
    auto blockCount = m_blockCount;
    // Only unblock if it was blocked.
    if (m_blockCount > 0)
    {
        m_blockCount = 0;
        SendMessage(*this, WM_SETREDRAW, TRUE, 0);
        RedrawWindow(*this, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_INTERNALPAINT | RDW_ALLCHILDREN | RDW_UPDATENOW);
    }
    return blockCount;
}

void CMainWindow::ReBlockUI(int blockCount)
{
    if (blockCount)
    {
        m_blockCount = blockCount;
        SendMessage(*this, WM_SETREDRAW, FALSE, 0);
    }
}

void CMainWindow::ShowProgressCtrl(UINT delay)
{
    APPVERIFY(m_blockCount > 0);

    m_progressBar.SetDarkMode(CTheme::Instance().IsDarkTheme(), CTheme::Instance().GetThemeColor(GetSysColor(COLOR_WINDOW)));
    RECT rect;
    GetClientRect(*this, &rect);
    MapWindowPoints(*this, nullptr, reinterpret_cast<LPPOINT>(&rect), 2);
    SetWindowPos(m_progressBar, HWND_TOP, rect.left, rect.bottom - m_statusBar.GetHeight(), rect.right - rect.left, m_statusBar.GetHeight(), SWP_NOACTIVATE | SWP_NOCOPYBITS);

    m_progressBar.ShowWindow(delay);
}

void CMainWindow::HideProgressCtrl()
{
    ShowWindow(m_progressBar, SW_HIDE);
}

void CMainWindow::SetProgress(DWORD32 pos, DWORD32 end)
{
    m_progressBar.SetRange(0, end);
    m_progressBar.SetPos(pos);
    UpdateWindow(m_progressBar);
}

void CMainWindow::SetFileTreeWidth(int width)
{
    m_treeWidth = width;
    m_treeWidth = max(50, m_treeWidth);
    RECT rc;
    GetClientRect(*this, &rc);
    m_treeWidth = min(m_treeWidth, rc.right - rc.left - 200);
}

void CMainWindow::AddAutoCompleteWords(const std::string& lang, std::map<std::string, AutoCompleteType>&& words)
{
    m_autoCompleter.AddWords(lang, words);
}

void CMainWindow::AddAutoCompleteWords(const std::string& lang, const std::map<std::string, AutoCompleteType>& words)
{
    m_autoCompleter.AddWords(lang, words);
}

void CMainWindow::AddAutoCompleteWords(const DocID& docID, std::map<std::string, AutoCompleteType>&& words)
{
    m_autoCompleter.AddWords(docID, words);
}

void CMainWindow::AddAutoCompleteWords(const DocID& docID, const std::map<std::string, AutoCompleteType>& words)
{
    m_autoCompleter.AddWords(docID, words);
}

void CMainWindow::SetRibbonColors(COLORREF text, COLORREF background, COLORREF highlight)
{
    IPropertyStorePtr spPropertyStore;

    // g_pFramework is a pointer to the IUIFramework interface that is assigned
    // when the Ribbon is initialized.
    if (SUCCEEDED(g_pFramework->QueryInterface(&spPropertyStore)))
    {
        PROPVARIANT propVarBackground;
        PROPVARIANT propVarHighlight;
        PROPVARIANT propVarText;

        // UI_HSBCOLOR is a type defined in UIRibbon.h that is composed of
        // three component values: hue, saturation and brightness, respectively.
        BYTE h, s, b;
        GDIHelpers::RGBToHSB(text, h, s, b);
        UI_HSBCOLOR textColor = UI_HSB(h, s, b);
        GDIHelpers::RGBToHSB(background, h, s, b);
        UI_HSBCOLOR backgroundColor = UI_HSB(h, s, b);
        GDIHelpers::RGBToHSB(highlight, h, s, b);
        UI_HSBCOLOR highlightColor = UI_HSB(h, s, b);

        InitPropVariantFromUInt32(backgroundColor, &propVarBackground);
        InitPropVariantFromUInt32(highlightColor, &propVarHighlight);
        InitPropVariantFromUInt32(textColor, &propVarText);

        spPropertyStore->SetValue(UI_PKEY_GlobalBackgroundColor, propVarBackground);
        spPropertyStore->SetValue(UI_PKEY_GlobalHighlightColor, propVarHighlight);
        spPropertyStore->SetValue(UI_PKEY_GlobalTextColor, propVarText);

        spPropertyStore->Commit();
    }
}

void CMainWindow::SetRibbonColorsHSB(UI_HSBCOLOR text, UI_HSBCOLOR background, UI_HSBCOLOR highlight)
{
    IPropertyStorePtr spPropertyStore;

    APPVERIFY(g_pFramework != nullptr);
    // g_pFramework is a pointer to the IUIFramework interface that is assigned
    // when the Ribbon is initialized.
    HRESULT hr = g_pFramework->QueryInterface(&spPropertyStore);
    if (SUCCEEDED(hr))
    {
        PROPVARIANT propVarBackground;
        PROPVARIANT propVarHighlight;
        PROPVARIANT propVarText;

        InitPropVariantFromUInt32(background, &propVarBackground);
        InitPropVariantFromUInt32(highlight, &propVarHighlight);
        InitPropVariantFromUInt32(text, &propVarText);

        spPropertyStore->SetValue(UI_PKEY_GlobalBackgroundColor, propVarBackground);
        spPropertyStore->SetValue(UI_PKEY_GlobalHighlightColor, propVarHighlight);
        spPropertyStore->SetValue(UI_PKEY_GlobalTextColor, propVarText);

        spPropertyStore->Commit();
    }
}

void CMainWindow::GetRibbonColors(UI_HSBCOLOR& text, UI_HSBCOLOR& background, UI_HSBCOLOR& highlight)
{
    IPropertyStorePtr spPropertyStore;

    // g_pFramework is a pointer to the IUIFramework interface that is assigned
    // when the Ribbon is initialized.
    HRESULT hr = g_pFramework->QueryInterface(&spPropertyStore);
    if (SUCCEEDED(hr))
    {
        PROPVARIANT propVarBackground;
        PROPVARIANT propVarHighlight;
        PROPVARIANT propVarText;

        spPropertyStore->GetValue(UI_PKEY_GlobalBackgroundColor, &propVarBackground);
        spPropertyStore->GetValue(UI_PKEY_GlobalHighlightColor, &propVarHighlight);
        spPropertyStore->GetValue(UI_PKEY_GlobalTextColor, &propVarText);

        text       = propVarText.uintVal;
        background = propVarBackground.uintVal;
        highlight  = propVarHighlight.uintVal;
    }
}

// Implementation helper only,
// use CTheme::Instance::SetDarkTheme to actually set the theme.
#ifndef UI_PKEY_DarkModeRibbon
// ReSharper disable once CppInconsistentNaming
DEFINE_UIPROPERTYKEY(UI_PKEY_DarkModeRibbon, VT_BOOL, 2004);
// ReSharper disable once CppInconsistentNaming
DEFINE_UIPROPERTYKEY(UI_PKEY_ApplicationButtonColor, VT_UI4, 2003);
#endif
void CMainWindow::SetTheme(bool dark)
{
    // as of the windows 10 update 1809, the background color
    // of the ribbon does not change anymore!
    // But, through reverse engineering I found the UI_PKEY_DarkModeRibbon
    // property, which we can use instead.
    bool                      bCanChangeBackground = true;
    auto                      version              = CPathUtils::GetVersionFromFile(L"uiribbon.dll");
    std::vector<std::wstring> tokens;
    stringtok(tokens, version, false, L".");
    if (tokens.size() == 4)
    {
        auto major = std::stol(tokens[0]);
        //auto minor = std::stol(tokens[1]);
        auto micro = std::stol(tokens[2]);
        //auto build = std::stol(tokens[3]);

        // the windows 10 update 1809 has the version
        // number as 10.0.17763.10000
        if (major == 10 && micro > 17762)
            bCanChangeBackground = false;
    }

    Win10Colors::AccentColor accentColor;
    if (SUCCEEDED(Win10Colors::GetAccentColor(accentColor)))
    {
        BYTE h, s, b;
        GDIHelpers::RGBToHSB(dark ? accentColor.accent : accentColor.accent, h, s, b);
        UI_HSBCOLOR aColor = UI_HSB(h, s, b);

        IPropertyStorePtr spPropertyStore;
        HRESULT           hr = g_pFramework->QueryInterface(&spPropertyStore);
        if (SUCCEEDED(hr))
        {
            PROPVARIANT propVarAccentColor;
            InitPropVariantFromUInt32(aColor, &propVarAccentColor);
            spPropertyStore->SetValue(UI_PKEY_ApplicationButtonColor, propVarAccentColor);
            spPropertyStore->Commit();
        }
    }

    if (dark)
    {
        if (bCanChangeBackground)
            SetRibbonColorsHSB(UI_HSB(0, 0, 255), UI_HSB(160, 0, 0), UI_HSB(160, 44, 0));
        else
        {
            IPropertyStorePtr spPropertyStore;
            HRESULT           hr = g_pFramework->QueryInterface(&spPropertyStore);
            if (SUCCEEDED(hr))
            {
                DarkModeHelper::Instance().AllowDarkModeForWindow(*this, TRUE);
                PROPVARIANT propVarDarkMode;
                InitPropVariantFromBoolean(1, &propVarDarkMode);
                spPropertyStore->SetValue(UI_PKEY_DarkModeRibbon, propVarDarkMode);
                spPropertyStore->Commit();
            }
        }
        DarkModeHelper::Instance().AllowDarkModeForApp(TRUE);

        auto darkModeForWindow = [](HWND hWnd) {
            DarkModeHelper::Instance().AllowDarkModeForWindow(hWnd, TRUE);
            SetWindowTheme(hWnd, L"Explorer", nullptr);
        };

        SetClassLongPtr(m_hwnd, GCLP_HBRBACKGROUND, reinterpret_cast<LONG_PTR>(GetStockObject(BLACK_BRUSH)));
        SetClassLongPtr(m_tabBar, GCLP_HBRBACKGROUND, reinterpret_cast<LONG_PTR>(GetStockObject(BLACK_BRUSH)));
        darkModeForWindow(m_hwnd);
        darkModeForWindow(m_statusBar);
        darkModeForWindow(m_tabBar);
        darkModeForWindow(m_fileTree);
        darkModeForWindow(m_tablistBtn);
        darkModeForWindow(m_newTabBtn);
        darkModeForWindow(m_closeTabBtn);
        darkModeForWindow(m_editor);

        BOOL                                        darkFlag = TRUE;
        DarkModeHelper::WINDOWCOMPOSITIONATTRIBDATA data     = {DarkModeHelper::WCA_USEDARKMODECOLORS, &darkFlag, sizeof(darkFlag)};
        DarkModeHelper::Instance().SetWindowCompositionAttribute(*this, &data);
        DarkModeHelper::Instance().FlushMenuThemes();
        DarkModeHelper::Instance().RefreshImmersiveColorPolicyState();
    }
    else
    {
        SetRibbonColorsHSB(m_normalThemeText, m_normalThemeBack, m_normalThemeHigh);
        if (!bCanChangeBackground)
        {
            DarkModeHelper::Instance().AllowDarkModeForWindow(*this, FALSE);
            IPropertyStorePtr spPropertyStore;
            HRESULT           hr = g_pFramework->QueryInterface(&spPropertyStore);
            if (SUCCEEDED(hr))
            {
                PROPVARIANT propVarDarkMode;
                InitPropVariantFromBoolean(false, &propVarDarkMode);
                spPropertyStore->SetValue(UI_PKEY_DarkModeRibbon, propVarDarkMode);
                spPropertyStore->Commit();
            }
        }

        auto normalModeForWindow = [](HWND hWnd) {
            DarkModeHelper::Instance().AllowDarkModeForWindow(hWnd, FALSE);
            SetWindowTheme(hWnd, L"Explorer", nullptr);
        };

        SetClassLongPtr(m_hwnd, GCLP_HBRBACKGROUND, reinterpret_cast<LONG_PTR>(GetSysColorBrush(COLOR_3DFACE)));
        SetClassLongPtr(m_tabBar, GCLP_HBRBACKGROUND, reinterpret_cast<LONG_PTR>(GetSysColorBrush(COLOR_3DFACE)));
        normalModeForWindow(m_hwnd);
        normalModeForWindow(m_statusBar);
        normalModeForWindow(m_tabBar);
        normalModeForWindow(m_fileTree);
        normalModeForWindow(m_tablistBtn);
        normalModeForWindow(m_newTabBtn);
        normalModeForWindow(m_closeTabBtn);
        normalModeForWindow(m_editor);

        BOOL                                        darkFlag = FALSE;
        DarkModeHelper::WINDOWCOMPOSITIONATTRIBDATA data     = {DarkModeHelper::WCA_USEDARKMODECOLORS, &darkFlag, sizeof(darkFlag)};
        DarkModeHelper::Instance().SetWindowCompositionAttribute(*this, &data);
        DarkModeHelper::Instance().FlushMenuThemes();
        DarkModeHelper::Instance().AllowDarkModeForApp(FALSE);
        DarkModeHelper::Instance().RefreshImmersiveColorPolicyState();
    }
    auto activeTabId = m_tabBar.GetCurrentTabId();
    if (activeTabId.IsValid())
    {
        m_editor.Call(SCI_CLEARDOCUMENTSTYLE);
        m_editor.Call(SCI_COLOURISE, 0, m_editor.Call(SCI_POSITIONFROMLINE, m_editor.Call(SCI_LINESONSCREEN) + 1));
        const auto& doc = m_docManager.GetDocumentFromID(activeTabId);
        m_editor.SetupLexerForLang(doc.GetLanguage());
    }
    DarkModeHelper::Instance().RefreshTitleBarThemeColor(*this, dark);
    CCommandHandler::Instance().OnThemeChanged(dark);
    RedrawWindow(*this, nullptr, nullptr, RDW_FRAME | RDW_INVALIDATE | RDW_ERASE | RDW_INTERNALPAINT | RDW_ALLCHILDREN | RDW_UPDATENOW | RDW_ERASENOW);
    RECT rc{0};
    GetWindowRect(*this, &rc);
    SetWindowPos(*this, nullptr, 0, 0, rc.right - rc.left, rc.bottom - rc.top - 1, SWP_DRAWFRAME | SWP_FRAMECHANGED | SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOOWNERZORDER | SWP_NOZORDER);
    SetWindowPos(*this, nullptr, 0, 0, rc.right - rc.left, rc.bottom - rc.top, SWP_DRAWFRAME | SWP_FRAMECHANGED | SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOOWNERZORDER | SWP_NOZORDER);
}
