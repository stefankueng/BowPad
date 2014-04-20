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
#include "stdafx.h"
#include "MainWindow.h"
#include "BowPad.h"
#include "BowPadUI.h"
#include "AboutDlg.h"
#include "StringUtils.h"
#include "UnicodeUtils.h"
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
#include <future>
#include <Shobjidl.h>
#include <Shellapi.h>
#include <Shlobj.h>

IUIFramework *g_pFramework = NULL;  // Reference to the Ribbon framework.

#define STATUSBAR_DOC_TYPE      0
#define STATUSBAR_DOC_SIZE      1
#define STATUSBAR_CUR_POS       2
#define STATUSBAR_EOF_FORMAT    3
#define STATUSBAR_UNICODE_TYPE  4
#define STATUSBAR_TYPING_MODE   5
#define STATUSBAR_CAPS          6
#define STATUSBAR_TABS          7

#define URL_REG_EXPR "\\b[A-Za-z+]{3,9}://[A-Za-z0-9_\\-+~.:?&@=/%#,;{}()[\\]|*!\\\\]+\\b"

static bool bWindowRestored = false;

CMainWindow::CMainWindow(HINSTANCE hInst, const WNDCLASSEX* wcx /* = NULL*/)
    : CWindow(hInst, wcx)
    , m_StatusBar(hInst)
    , m_TabBar(hInst)
    , m_scintilla(hInst)
    , m_cRef(1)
    , m_hShieldIcon(NULL)
    , m_tabmovemod(false)
    , m_initLine(0)
    , m_bPathsToOpenMRU(true)
    , m_insertionIndex(-1)
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
        *ppv = NULL;
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
                IStreamPtr pStrm;
                std::wstring ribbonsettingspath = CAppUtils::GetDataPath() + L"\\ribbonsettings";
                hr = SHCreateStreamOnFileEx(ribbonsettingspath.c_str(), STGM_READ, 0, FALSE, NULL, &pStrm);

                if (SUCCEEDED(hr))
                    m_pRibbon->LoadSettingsFromStream(pStrm);
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
                    ResizeChildWindows();
                }
            }
            break;
            // The view was destroyed.
        case UI_VIEWVERB_DESTROY:
            {
                hr = S_OK;
                _COM_SMARTPTR_TYPEDEF(IStream, __uuidof(IStream));
                IStreamPtr pStrm;
                std::wstring ribbonsettingspath = CAppUtils::GetDataPath() + L"\\ribbonsettings";
                hr = SHCreateStreamOnFileEx(ribbonsettingspath.c_str(), STGM_WRITE|STGM_CREATE, FILE_ATTRIBUTE_NORMAL, TRUE, NULL, &pStrm);
                if (SUCCEEDED(hr))
                {
                    LARGE_INTEGER liPos;
                    ULARGE_INTEGER uliSize;

                    liPos.QuadPart = 0;
                    uliSize.QuadPart = 0;

                    pStrm->Seek(liPos, STREAM_SEEK_SET, NULL);
                    pStrm->SetSize(uliSize);

                    m_pRibbon->SaveSettingsToStream(pStrm);
                }
                m_pRibbon->Release();
                m_pRibbon = NULL;
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

bool CMainWindow::RegisterAndCreateWindow()
{
    WNDCLASSEX wcx;

    // Fill in the window class structure with default parameters
    wcx.cbSize = sizeof(WNDCLASSEX);
    wcx.style = 0; // Don't use CS_HREDRAW or CS_VREDRAW with a Ribbon
    wcx.lpfnWndProc = CWindow::stWinMsgHandler;
    wcx.cbClsExtra = 0;
    wcx.cbWndExtra = 0;
    wcx.hInstance = hResource;
    wcx.hCursor = NULL;
    ResString clsResName(hResource, IDC_BOWPAD);
    std::wstring clsName = (LPCWSTR)clsResName + CAppUtils::GetSessionID();
    wcx.lpszClassName = clsName.c_str();
    wcx.hIcon = LoadIcon(hResource, MAKEINTRESOURCE(IDI_BOWPAD));
    wcx.hbrBackground = (HBRUSH)(COLOR_3DFACE+1);
    wcx.lpszMenuName = NULL;
    wcx.hIconSm = LoadIcon(wcx.hInstance, MAKEINTRESOURCE(IDI_BOWPAD));
    if (RegisterWindow(&wcx))
    {
        if (CreateEx(WS_EX_ACCEPTFILES, WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, NULL))
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
        }
        break;
    case WM_COMMAND:
        {
            return DoCommand(LOWORD(wParam));
        }
        break;
    case WM_SIZE:
        {
            ResizeChildWindows();
        }
        break;
    case WM_GETMINMAXINFO:
        {
            MINMAXINFO * mmi = (MINMAXINFO*)lParam;
            mmi->ptMinTrackSize.x = 400;
            mmi->ptMinTrackSize.y = 100;
            return 0;
        }
        break;
    case WM_DRAWITEM :
        {
            DRAWITEMSTRUCT *dis = (DRAWITEMSTRUCT *)lParam;
            if (dis->CtlType == ODT_TAB)
            {
                return ::SendMessage(dis->hwndItem, WM_DRAWITEM, wParam, lParam);
            }
        }
        break;
    case WM_DROPFILES:
        {
            HDROP hDrop = reinterpret_cast<HDROP>(wParam);
            if (hDrop)
            {
                int filesDropped = DragQueryFile(hDrop, 0xffffffff, NULL, 0);
                std::vector<std::wstring> files;
                for (int i = 0 ; i < filesDropped ; ++i)
                {
                    UINT len = DragQueryFile(hDrop, i, NULL, 0);
                    std::unique_ptr<wchar_t[]> pathBuf(new wchar_t[len+1]);
                    DragQueryFile(hDrop, i, pathBuf.get(), len+1);
                    files.push_back(pathBuf.get());
                }
                DragFinish(hDrop);
                for (auto it:files)
                    OpenFile(it, true);
            }
        }
        break;
    case WM_COPYDATA:
        {
            COPYDATASTRUCT *cds;
            cds = (COPYDATASTRUCT *) lParam;
            switch (cds->dwData)
            {
                case CD_COMMAND_LINE:
                {
                    CCmdLineParser parser((LPCWSTR)cds->lpData);
                    if (parser.HasVal(L"path"))
                    {
                        if (AskToCreateNonExistingFile(parser.GetVal(L"path")))
                        {
                            OpenFile(parser.GetVal(L"path"), true);
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

                        LPWSTR * szArglist = CommandLineToArgvW((LPCWSTR)cds->lpData, &nArgs);
                        if (szArglist)
                        {
                            for (int i = 1; i < nArgs; i++)
                            {
                                if (szArglist[i][0] != '/')
                                {
                                    if (!AskToCreateNonExistingFile(szArglist[i]))
                                        continue;
                                    OpenFile(szArglist[i], true);
                                }
                            }
                            if (parser.HasVal(L"line"))
                            {
                                GoToLine(parser.GetLongVal(L"line") - 1);
                            }

                            // Free memory allocated for CommandLineToArgvW arguments.
                            LocalFree(szArglist);
                        }
                    }
                }
                    break;
                case CD_COMMAND_MOVETAB:
                {
                    std::wstring paths = std::wstring((wchar_t*)cds->lpData, cds->cbData / sizeof(wchar_t));
                    std::vector<std::wstring> datavec;
                    stringtok(datavec, paths, false, L"*");
                    std::wstring realpath = datavec[0];
                    std::wstring temppath = datavec[1];
                    bool bMod = _wtoi(datavec[2].c_str()) != 0;
                    int line = _wtoi(datavec[3].c_str());

                    int id = m_DocManager.GetIdForPath(realpath.c_str());
                    if (id != -1)
                    {
                        m_TabBar.ActivateAt(m_TabBar.GetIndexFromID(id));
                    }
                    else
                    {
                        OpenFile(temppath, false);
                        id = m_DocManager.GetIdForPath(temppath);
                        m_TabBar.ActivateAt(m_TabBar.GetIndexFromID(id));
                        CDocument doc = m_DocManager.GetDocumentFromID(id);
                        doc.m_path = CPathUtils::GetLongPathname(realpath);
                        doc.m_bIsDirty = bMod;
                        doc.m_bNeedsSaving = bMod;
                        m_DocManager.UpdateFileTime(doc);
                        doc.m_language = CLexStyles::Instance().GetLanguageForExt(doc.m_path.substr(doc.m_path.find_last_of('.') + 1));
                        m_DocManager.SetDocument(id, doc);
                        m_scintilla.SetupLexerForExt(doc.m_path.substr(doc.m_path.find_last_of('.') + 1).c_str());
                        std::wstring sFileName = doc.m_path.substr(doc.m_path.find_last_of('\\') + 1);
                        m_TabBar.SetCurrentTitle(sFileName.c_str());
                        wchar_t sTabTitle[100] = { 0 };
                        m_TabBar.GetCurrentTitle(sTabTitle, _countof(sTabTitle));
                        std::wstring sWindowTitle = CStringUtils::Format(L"%s - BowPad", doc.m_path.empty() ? sTabTitle : doc.m_path.c_str());
                        SetWindowText(*this, sWindowTitle.c_str());
                        UpdateStatusBar(true);
                        TCITEM tie;
                        tie.lParam = -1;
                        tie.mask = TCIF_IMAGE;
                        tie.iImage = doc.m_bIsDirty || doc.m_bNeedsSaving ? UNSAVED_IMG_INDEX : SAVED_IMG_INDEX;
                        if (doc.m_bIsReadonly)
                            tie.iImage = REDONLY_IMG_INDEX;
                        ::SendMessage(m_TabBar, TCM_SETITEM, m_TabBar.GetIndexFromID(id), reinterpret_cast<LPARAM>(&tie));

                        GoToLine(line);
                    }
                    // delete the temp file
                    DeleteFile(temppath.c_str());
                }
                break;
            }
        }
        break;
    case WM_UPDATEAVAILABLE:
        CAppUtils::ShowUpdateAvailableDialog(*this);
        break;
    case WM_AFTERINIT:
        CCommandHandler::Instance().AfterInit();
        for (const auto& path : m_pathsToOpen)
        {
            if (!AskToCreateNonExistingFile(path.first))
                continue;
            OpenFile(path.first, m_bPathsToOpenMRU);
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
        break;
    case WM_NOTIFY:
        {
            LPNMHDR pNMHDR = reinterpret_cast<LPNMHDR>(lParam);

            switch (pNMHDR->code)
            {
            case TTN_GETDISPINFO:
                {
                    POINT p;
                    ::GetCursorPos(&p);
                    ::ScreenToClient(*this, &p);
                    HWND hWin = ::RealChildWindowFromPoint(*this, p);
                    if (hWin == m_TabBar)
                    {
                        LPNMTTDISPINFO lpnmtdi = (LPNMTTDISPINFO)lParam;
                        int id = m_TabBar.GetIDFromIndex((int)pNMHDR->idFrom);
                        if (id >= 0)
                        {
                            if (m_DocManager.HasDocumentID(id))
                            {
                                CDocument doc = m_DocManager.GetDocumentFromID(id);
                                m_tooltipbuffer = std::unique_ptr<wchar_t[]>(new wchar_t[doc.m_path.size()+1]);
                                wcscpy_s(m_tooltipbuffer.get(), doc.m_path.size()+1, doc.m_path.c_str());
                                lpnmtdi->lpszText = m_tooltipbuffer.get();
                                lpnmtdi->hinst = NULL;
                                return 0;
                            }
                        }
                    }
                }
                break;
            default:
                break;
            }

            if ((pNMHDR->idFrom == (UINT_PTR)&m_TabBar) ||
                (pNMHDR->hwndFrom == m_TabBar))
            {
                TBHDR * ptbhdr = reinterpret_cast<TBHDR*>(lParam);
                CCommandHandler::Instance().TabNotify(ptbhdr);

                switch (pNMHDR->code)
                {
                case TCN_GETCOLOR:
                    if ((ptbhdr->tabOrigin >= 0) && (ptbhdr->tabOrigin < m_DocManager.GetCount()))
                    {
                        int id = m_TabBar.GetIDFromIndex(ptbhdr->tabOrigin);
                        if (m_DocManager.HasDocumentID(id))
                            return CTheme::Instance().GetThemeColor(m_DocManager.GetColorForDocument(id));
                    }
                    break;
                case TCN_SELCHANGE:
                    {
                        // document got activated
                        int id = m_TabBar.GetCurrentTabId();
                        if ((id >= 0) && m_DocManager.HasDocumentID(id))
                        {
                            CDocument doc = m_DocManager.GetDocumentFromID(id);
                            m_scintilla.Call(SCI_SETDOCPOINTER, 0, doc.m_document);
                            m_scintilla.SetupLexerForLang(doc.m_language);
                            m_scintilla.RestoreCurrentPos(doc.m_position);
                            m_scintilla.SetTabSettings();
                            CEditorConfigHandler::Instance().ApplySettingsForPath(doc.m_path, &m_scintilla, doc);
                            m_DocManager.SetDocument(id, doc);
                            m_scintilla.MarkSelectedWord(true);
                            m_scintilla.MarkBookmarksInScrollbar();
                            wchar_t sTabTitle[100] = {0};
                            m_TabBar.GetCurrentTitle(sTabTitle, _countof(sTabTitle));
                            std::wstring sWindowTitle = CStringUtils::Format(L"%s - BowPad", doc.m_path.empty() ? sTabTitle : doc.m_path.c_str());
                            SetWindowText(*this, sWindowTitle.c_str());
                            HandleOutsideModifications(id);
                            SetFocus(m_scintilla);
                            m_scintilla.Call(SCI_GRABFOCUS);
                            UpdateStatusBar(true);
                        }
                    }
                    break;
                case TCN_SELCHANGING:
                    {
                        // document is about to be deactivated
                        int id = m_TabBar.GetCurrentTabId();
                        if ((id >= 0) && m_DocManager.HasDocumentID(id))
                        {
                            CDocument doc = m_DocManager.GetDocumentFromID(id);
                            m_scintilla.SaveCurrentPos(&doc.m_position);
                            m_DocManager.SetDocument(id, doc);
                        }
                    }
                    break;
                case TCN_TABDELETE:
                    {
                        int id = m_TabBar.GetCurrentTabId();
                        if (CloseTab(ptbhdr->tabOrigin))
                        {
                            if (id == m_TabBar.GetIDFromIndex(ptbhdr->tabOrigin))
                            {
                                if (ptbhdr->tabOrigin > 0)
                                {
                                    m_TabBar.ActivateAt(ptbhdr->tabOrigin-1);
                                }
                            }
                        }
                    }
                    break;
                case TCN_TABDROPPEDOUTSIDE:
                    {
                        // start a new instance of BowPad with this dropped tab, or add this tab to
                        // the BowPad window the drop was done on. Then close this tab.

                        // first save the file to a temp location to ensure all unsaved mods are saved
                        std::wstring temppath = CPathUtils::GetTempFilePath();
                        CDocument doc = m_DocManager.GetDocumentFromID(m_TabBar.GetIDFromIndex(ptbhdr->tabOrigin));
                        CDocument tempdoc = doc;
                        tempdoc.m_path = temppath;
                        m_DocManager.SaveFile(*this, tempdoc);
                        DWORD pos = GetMessagePos();
                        POINT pt;
                        pt.x = GET_X_LPARAM(pos);
                        pt.y = GET_Y_LPARAM(pos);
                        HWND hDroppedWnd = WindowFromPoint(pt);
                        if (hDroppedWnd)
                        {
                            ResString clsResName(hRes, IDC_BOWPAD);
                            std::wstring clsName = (LPCWSTR)clsResName + CAppUtils::GetSessionID();
                            while (hDroppedWnd)
                            {
                                wchar_t classname[MAX_PATH] = { 0 };
                                GetClassName(hDroppedWnd, classname, _countof(classname));
                                if (clsName.compare(classname) == 0)
                                    break;
                                hDroppedWnd = GetParent(hDroppedWnd);
                            }
                            // dropping on our own window shall create an new BowPad instance
                            if (hDroppedWnd == *this)
                                hDroppedWnd = NULL;
                            if (hDroppedWnd)
                            {
                                // dropped on another BowPad Window, 'move' this tab to that BowPad Window
                                COPYDATASTRUCT cpd = { 0 };
                                cpd.dwData = CD_COMMAND_MOVETAB;
                                std::wstring cpdata = doc.m_path + L"*" + temppath + L"*";
                                cpdata += (doc.m_bIsDirty || doc.m_bNeedsSaving) ? L"1*" : L"0*";
                                cpdata += CStringUtils::Format(L"%ld", (long)(m_scintilla.Call(SCI_LINEFROMPOSITION, m_scintilla.Call(SCI_GETCURRENTPOS)) + 1));
                                cpd.lpData = (PVOID)cpdata.c_str();
                                cpd.cbData = DWORD(cpdata.size()*sizeof(wchar_t));
                                SendMessage(hDroppedWnd, WM_COPYDATA, (WPARAM)m_hwnd, (LPARAM)&cpd);
                                // remove the tab
                                CloseTab(ptbhdr->tabOrigin, true);
                                break;
                            }
                        }
                        // no BowPad Window at the drop location: start a new instance and open the tab there
                        // but only if there are more than one tab in the current window
                        if (m_TabBar.GetItemCount() < 2)
                            break;

                        std::wstring modpath = CPathUtils::GetModulePath();
                        std::wstring cmdline = CStringUtils::Format(L"/multiple /tabmove /savepath:\"%s\" /path:\"%s\" /line:%ld",
                                                                    doc.m_path.c_str(), temppath.c_str(),
                                                                    (long)(m_scintilla.Call(SCI_LINEFROMPOSITION, m_scintilla.Call(SCI_GETCURRENTPOS)) + 1));
                        if (doc.m_bIsDirty || doc.m_bNeedsSaving)
                            cmdline += L" /modified";
                        SHELLEXECUTEINFO shExecInfo;
                        shExecInfo.cbSize = sizeof(SHELLEXECUTEINFO);

                        shExecInfo.fMask = NULL;
                        shExecInfo.hwnd = *this;
                        shExecInfo.lpVerb = L"open";
                        shExecInfo.lpFile = modpath.c_str();
                        shExecInfo.lpParameters = cmdline.c_str();
                        shExecInfo.lpDirectory = NULL;
                        shExecInfo.nShow = SW_NORMAL;
                        shExecInfo.hInstApp = NULL;

                        if (ShellExecuteEx(&shExecInfo))
                        {
                            // remove the tab
                            CloseTab(ptbhdr->tabOrigin, true);
                        }
                    }
                    break;
                }
            }
            else if ((pNMHDR->idFrom == (UINT_PTR)&m_scintilla) ||
                (pNMHDR->hwndFrom == m_scintilla))
            {
                if (pNMHDR->code == NM_COOLSB_CUSTOMDRAW)
                {
                    return m_scintilla.HandleScrollbarCustomDraw(wParam, (NMCSBCUSTOMDRAW *)lParam);
                }

                Scintilla::SCNotification * pScn = reinterpret_cast<Scintilla::SCNotification *>(lParam);
                CCommandHandler::Instance().ScintillaNotify(pScn);

                switch (pScn->nmhdr.code)
                {
                case SCN_PAINTED:
                    {
                        m_scintilla.UpdateLineNumberWidth();
                    }
                    break;
                case SCN_SAVEPOINTREACHED:
                case SCN_SAVEPOINTLEFT:
                    {
                        int id = m_TabBar.GetCurrentTabId();
                        if ((id >= 0) && m_DocManager.HasDocumentID(id))
                        {
                            CDocument doc = m_DocManager.GetDocumentFromID(id);
                            doc.m_bIsDirty = pScn->nmhdr.code == SCN_SAVEPOINTLEFT;
                            m_DocManager.SetDocument(id, doc);
                            TCITEM tie;
                            tie.lParam = -1;
                            tie.mask = TCIF_IMAGE;
                            tie.iImage = doc.m_bIsDirty||doc.m_bNeedsSaving?UNSAVED_IMG_INDEX:SAVED_IMG_INDEX;
                            if (doc.m_bIsReadonly)
                                tie.iImage = REDONLY_IMG_INDEX;
                            ::SendMessage(m_TabBar, TCM_SETITEM, m_TabBar.GetIndexFromID(id), reinterpret_cast<LPARAM>(&tie));
                        }
                    }
                    break;
                case SCN_MARGINCLICK:
                    {
                        m_scintilla.MarginClick(pScn);
                    }
                    break;
                case SCN_UPDATEUI:
                    {
                        if ((pScn->updated & SC_UPDATE_SELECTION) ||
                            (pScn->updated & SC_UPDATE_H_SCROLL)  ||
                            (pScn->updated & SC_UPDATE_V_SCROLL))
                            m_scintilla.MarkSelectedWord(false);

                        m_scintilla.MatchBraces();
                        m_scintilla.MatchTags();
                        AddHotSpots();
                        UpdateStatusBar(false);
                    }
                    break;
                case SCN_CHARADDED:
                    {
                        AutoIndent(pScn);
                    }
                    break;
                case SCN_HOTSPOTCLICK:
                    {
                        if (pScn->modifiers & SCMOD_CTRL)
                        {
                            m_scintilla.Call(SCI_SETWORDCHARS, 0, (LPARAM)"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-+.,:?&@=/%#()");

                            long pos = pScn->position;
                            long startPos = static_cast<long>(m_scintilla.Call(SCI_WORDSTARTPOSITION, pos, false));
                            long endPos = static_cast<long>(m_scintilla.Call(SCI_WORDENDPOSITION, pos, false));

                            m_scintilla.Call(SCI_SETTARGETSTART, startPos);
                            m_scintilla.Call(SCI_SETTARGETEND, endPos);

                            long posFound = (long)m_scintilla.Call(SCI_SEARCHINTARGET, strlen(URL_REG_EXPR), (LPARAM)URL_REG_EXPR);
                            if (posFound != -1)
                            {
                                startPos = int(m_scintilla.Call(SCI_GETTARGETSTART));
                                endPos = int(m_scintilla.Call(SCI_GETTARGETEND));
                            }

                            std::unique_ptr<char[]> urltext(new char[endPos - startPos + 2]);
                            Scintilla::TextRange tr;
                            tr.chrg.cpMin = startPos;
                            tr.chrg.cpMax = endPos;
                            tr.lpstrText = urltext.get();
                            m_scintilla.Call(SCI_GETTEXTRANGE, 0, reinterpret_cast<LPARAM>(&tr));


                            // This treatment would fail on some valid URLs where there's actually supposed to be a comma or parenthesis at the end.
                            size_t lastCharIndex = strlen(urltext.get())-1;
                            while (lastCharIndex > 0 && (urltext[lastCharIndex] == ',' || urltext[lastCharIndex] == ')' || urltext[lastCharIndex] == '('))
                            {
                                urltext[lastCharIndex] = '\0';
                                --lastCharIndex;
                            }

                            std::wstring url = CUnicodeUtils::StdGetUnicode(urltext.get());
                            while ((*url.begin() == '(') || (*url.begin() == ')') || (*url.begin() == ','))
                                url.erase(url.begin());

                            ::ShellExecute(*this, L"open", url.c_str(), NULL, NULL, SW_SHOW);
                            m_scintilla.Call(SCI_SETCHARSDEFAULT);
                        }
                    }
                    break;
                case SCN_DWELLSTART:
                    {
                        int style = (int)m_scintilla.Call(SCI_GETSTYLEAT, pScn->position);
                        if (style & INDIC2_MASK)
                        {
                            // an url hotspot
                            ResString str(hRes, IDS_CTRLCLICKTOOPEN);
                            std::string strA = CUnicodeUtils::StdGetUTF8((LPCWSTR)str);
                            m_scintilla.Call(SCI_CALLTIPSHOW, pScn->position, (sptr_t)strA.c_str());
                        }
                        else
                        {
                            m_scintilla.Call(SCI_SETWORDCHARS, 0, (LPARAM)"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_.,#");
                            Scintilla::Sci_TextRange tr = {0};
                            tr.chrg.cpMin = static_cast<long>(m_scintilla.Call(SCI_WORDSTARTPOSITION, pScn->position, false));
                            tr.chrg.cpMax = static_cast<long>(m_scintilla.Call(SCI_WORDENDPOSITION, pScn->position, false));
                            std::unique_ptr<char[]> word(new char[tr.chrg.cpMax - tr.chrg.cpMin + 2]);
                            tr.lpstrText = word.get();

                            m_scintilla.Call(SCI_GETTEXTRANGE, 0, (sptr_t)&tr);

                            std::string sWord = tr.lpstrText;
                            m_scintilla.Call(SCI_SETCHARSDEFAULT);
                            if (!sWord.empty())
                            {
                                if ((sWord[0] == '#') &&
                                    ((sWord.size() == 4) || (sWord.size() == 7)))
                                {
                                    // html color
                                    COLORREF color = 0;
                                    DWORD hexval = 0;
                                    if (sWord.size() == 4)
                                    {
                                        // shorthand form
                                        char * endptr = nullptr;
                                        char dig[2] = {0};
                                        dig[0] = sWord[1];
                                        int red = strtol(dig, &endptr, 16);
                                        if (endptr != &dig[1])
                                            break;
                                        red = red * 16 + red;
                                        dig[0] = sWord[2];
                                        int green = strtol(dig, &endptr, 16);
                                        if (endptr != &dig[1])
                                            break;
                                        green = green * 16 + green;
                                        dig[0] = sWord[3];
                                        int blue = strtol(dig, &endptr, 16);
                                        if (endptr != &dig[1])
                                            break;
                                        blue = blue * 16 + blue;
                                        color = RGB(red, green, blue);
                                        hexval = (RGB((color >> 16) & 0xFF, (color >> 8) & 0xFF, color & 0xFF)) | (color & 0xFF000000);
                                    }
                                    else if (sWord.size() == 7)
                                    {
                                        // normal/long form
                                        char * endptr = nullptr;
                                        hexval = strtol(&sWord[1], &endptr, 16);
                                        if (endptr != &sWord[7])
                                            break;
                                        color = (RGB((hexval >> 16) & 0xFF, (hexval >> 8) & 0xFF, hexval & 0xFF)) | (hexval & 0xFF000000);
                                    }
                                    std::string sCallTip = CStringUtils::Format("RGB(%d,%d,%d)\nHex: #%06lX\n####################\n####################\n####################", GetRValue(color), GetGValue(color), GetBValue(color), hexval);
                                    m_scintilla.Call(SCI_CALLTIPSETFOREHLT, color);
                                    m_scintilla.Call(SCI_CALLTIPSHOW, pScn->position, (sptr_t)sCallTip.c_str());
                                    size_t pos = sCallTip.find_first_of('\n');
                                    pos = sCallTip.find_first_of('\n', pos+1);
                                    m_scintilla.Call(SCI_CALLTIPSETHLT, pos, pos+63);
                                }
                                else
                                {
                                    char * endptr = nullptr;
                                    long number = strtol(sWord.c_str(), &endptr, 0);
                                    if (number && (endptr != &sWord[sWord.size()-1]))
                                    {
                                        // show number calltip
                                        std::string sCallTip = CStringUtils::Format("Dec: %ld - Hex: %#lX - Oct:%#lo", number, number, number);
                                        m_scintilla.Call(SCI_CALLTIPSHOW, pScn->position, (sptr_t)sCallTip.c_str());
                                    }
                                }
                            }
                        }
                    }
                    break;
                case SCN_DWELLEND:
                    {
                        m_scintilla.Call(SCI_CALLTIPCANCEL);
                    }
                    break;
                }
            }
        }
        break;
    case WM_ACTIVATEAPP:
        if (!bWindowRestored)
        {
            CIniSettings::Instance().RestoreWindowPos(L"MainWindow", *this, 0);
            bWindowRestored = true;
        }
        // intentional fall through!
    case WM_SETFOCUS:
    case WM_ACTIVATE:
        {
            if ((wParam == WA_ACTIVE)||(wParam == WA_CLICKACTIVE))
            {
                HWND hFocus = GetFocus();
                if (hFocus)
                {
                    hFocus = GetAncestor(hFocus, GA_ROOT);
                    if (hFocus == *this)
                        HandleOutsideModifications();
                }
                SetFocus(m_scintilla);
                m_scintilla.Call(SCI_SETFOCUS, true);
            }
        }
        break;
    case WM_CLIPBOARDUPDATE:
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
                        if (lptstr != NULL)
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
        CIniSettings::Instance().SaveWindowPos(L"MainWindow", *this);
        if (CloseAllTabs())
            ::DestroyWindow(m_hwnd);
        break;
    default:
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }

    return 0;
};

LRESULT CMainWindow::DoCommand(int id)
{
    switch (id)
    {
    case cmdExit:
        PostMessage(m_hwnd, WM_CLOSE, 0, 0);
        break;
    case cmdNew:
        {
            static int newCount = 0;
            newCount++;
            CDocument doc;
            doc.m_document = m_scintilla.Call(SCI_CREATEDOCUMENT);
            doc.m_bHasBOM = CIniSettings::Instance().GetInt64(L"Defaults", L"encodingnewbom", 0) != 0;
            doc.m_encoding = (UINT)CIniSettings::Instance().GetInt64(L"Defaults", L"encodingnew", GetACP());
            ResString newRes(hRes, IDS_NEW_TABTITLE);
            std::wstring s = CStringUtils::Format(newRes, newCount);
            int index = -1;
            if (m_insertionIndex >= 0)
                index = m_TabBar.InsertAfter(m_insertionIndex, s.c_str());
            else
                index = m_TabBar.InsertAtEnd(s.c_str());
            int id = m_TabBar.GetIDFromIndex(index);
            m_DocManager.AddDocumentAtEnd(doc, id);
            m_TabBar.ActivateAt(index);
            m_scintilla.SetupLexerForLang(L"Text");
            m_insertionIndex = -1;
        }
        break;
    case cmdClose:
        CloseTab(m_TabBar.GetCurrentTabIndex());
        break;
    case cmdCloseAll:
        CloseAllTabs();
        break;
    case cmdCloseAllButThis:
        {
            int count = m_TabBar.GetItemCount();
            int current = m_TabBar.GetCurrentTabIndex();
            for (int i = count-1; i >= 0; --i)
            {
                if (i != current)
                    CloseTab(i);
            }
        }
        break;
    case cmdCopyPath:
        {
            int id = m_TabBar.GetCurrentTabId();
            if ((id >= 0) && m_DocManager.HasDocumentID(id))
            {
                CDocument doc = m_DocManager.GetDocumentFromID(id);
                WriteAsciiStringToClipboard(doc.m_path.c_str(), *this);
            }
        }
        break;
    case cmdCopyName:
        {
            int id = m_TabBar.GetCurrentTabId();
            if ((id >= 0) && m_DocManager.HasDocumentID(id))
            {
                CDocument doc = m_DocManager.GetDocumentFromID(id);
                WriteAsciiStringToClipboard(doc.m_path.substr(doc.m_path.find_last_of(L"\\/")+1).c_str(), *this);
            }
        }
        break;
    case cmdCopyDir:
        {
            int id = m_TabBar.GetCurrentTabId();
            if ((id >= 0) && m_DocManager.HasDocumentID(id))
            {
                CDocument doc = m_DocManager.GetDocumentFromID(id);
                WriteAsciiStringToClipboard(doc.m_path.substr(0, doc.m_path.find_last_of(L"\\/")).c_str(), *this);
            }
        }
        break;
    case cmdExplore:
        {
            int id = m_TabBar.GetCurrentTabId();
            if ((id >= 0) && m_DocManager.HasDocumentID(id))
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
        break;
    case cmdExploreProperties:
        {
            int id = m_TabBar.GetCurrentTabId();
            if ((id >= 0) && m_DocManager.HasDocumentID(id))
            {
                CDocument doc = m_DocManager.GetDocumentFromID(id);
                SHELLEXECUTEINFO info = {sizeof(SHELLEXECUTEINFO)};
                info.lpVerb = L"properties";
                info.lpFile = doc.m_path.c_str();
                info.nShow = SW_SHOW;
                info.fMask = SEE_MASK_INVOKEIDLIST;
                info.hwnd = *this;
                ShellExecuteEx(&info);
            }
        }
        break;
    case cmdPasteHistory:
        {
            if (!m_ClipboardHistory.empty())
            {
                // create a context menu with all the items in the clipboard history
                HMENU hMenu = CreatePopupMenu();
                if (hMenu)
                {
                    size_t pos = m_scintilla.Call(SCI_GETCURRENTPOS);
                    POINT pt;
                    pt.x = (LONG)m_scintilla.Call(SCI_POINTXFROMPOSITION, 0, pos);
                    pt.y = (LONG)m_scintilla.Call(SCI_POINTYFROMPOSITION, 0, pos);
                    ClientToScreen(m_scintilla, &pt);
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
                    int selIndex = TrackPopupMenu(hMenu, TPM_LEFTALIGN|TPM_RETURNCMD, pt.x, pt.y, 0, m_scintilla, NULL);
                    DestroyMenu (hMenu);
                    if (selIndex > 0)
                    {
                        index = 1;
                        for (const auto& s : m_ClipboardHistory)
                        {
                            if (index == selIndex)
                            {
                                WriteAsciiStringToClipboard(s.c_str(), *this);
                                m_scintilla.Call(SCI_PASTE);
                                break;
                            }
                            ++index;
                        }
                    }
                }
            }
        }
        break;
    case cmdAbout:
        {
            CAboutDlg dlg(*this);
            dlg.DoModal(hRes, IDD_ABOUTBOX, *this);
        }
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
            func(*this,       WM_COPYDATA, MSGFLT_ALLOW, NULL );
            func(m_scintilla, WM_COPYDATA, MSGFLT_ALLOW, NULL );
            func(m_TabBar,    WM_COPYDATA, MSGFLT_ALLOW, NULL );
            func(m_StatusBar, WM_COPYDATA, MSGFLT_ALLOW, NULL );
        }
        else
        {
            typedef BOOL (WINAPI *MESSAGEFILTERFUNC)(UINT message,DWORD dwFlag);
            MESSAGEFILTERFUNC func = (MESSAGEFILTERFUNC)::GetProcAddress( hDll, "ChangeWindowMessageFilter" );

            if (func)
                func(WM_COPYDATA, MSGFLT_ADD);
        }
    }

    m_scintilla.Init(hResource, *this);
    int barParts[8] = {100, 300, 550, 650, 750, 780, 820, 880};
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
    // Here we instantiate the Ribbon framework object.
    HRESULT hr = CoCreateInstance(CLSID_UIRibbonFramework, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&g_pFramework));
    if (FAILED(hr))
    {
        return false;
    }

    hr = g_pFramework->Initialize(*this, this);
    if (FAILED(hr))
    {
        return false;
    }

    // Finally, we load the binary markup.  This will initiate callbacks to the IUIApplication object
    // that was provided to the framework earlier, allowing command handlers to be bound to individual
    // commands.
    hr = g_pFramework->LoadUI(hRes, L"BOWPAD_RIBBON");
    if (FAILED(hr))
    {
        return false;
    }

    CCommandHandler::Instance().Init(this);
    CKeyboardShortcutHandler::Instance().UpdateTooltips(true);
    AddClipboardFormatListener(*this);
    return true;
}

void CMainWindow::ResizeChildWindows()
{
    RECT rect;
    GetClientRect(*this, &rect);
    if (!IsRectEmpty(&rect))
    {
        HDWP hDwp = BeginDeferWindowPos(3);
        DeferWindowPos(hDwp, m_StatusBar, NULL, rect.left, rect.bottom - m_StatusBar.GetHeight(), rect.right - rect.left, m_StatusBar.GetHeight(), SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_NOZORDER | SWP_SHOWWINDOW | SWP_NOCOPYBITS);
        DeferWindowPos(hDwp, m_TabBar, NULL, rect.left, rect.top + m_RibbonHeight, rect.right - rect.left, rect.bottom - rect.top, SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_NOZORDER | SWP_SHOWWINDOW | SWP_NOCOPYBITS);
        RECT tabrc;
        TabCtrl_GetItemRect(m_TabBar, 0, &tabrc);
        MapWindowPoints(m_TabBar, *this, (LPPOINT)&tabrc, 2);
        DeferWindowPos(hDwp, m_scintilla, NULL, rect.left, rect.top + m_RibbonHeight + tabrc.bottom - tabrc.top, rect.right - rect.left, rect.bottom - (m_RibbonHeight + tabrc.bottom - tabrc.top) - m_StatusBar.GetHeight(), SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_NOZORDER | SWP_SHOWWINDOW | SWP_NOCOPYBITS);
        EndDeferWindowPos(hDwp);
        m_StatusBar.Resize();
    }
}

bool CMainWindow::SaveCurrentTab(bool bSaveAs /* = false */)
{
    bool bRet = false;
    int id = m_TabBar.GetCurrentTabId();
    if ((id >= 0) && m_DocManager.HasDocumentID(id))
    {
        CDocument doc = m_DocManager.GetDocumentFromID(id);
        if (doc.m_path.empty() || bSaveAs || doc.m_bDoSaveAs)
        {
            bSaveAs = true;
            PreserveChdir keepCWD;

            IFileSaveDialogPtr pfd = NULL;

            HRESULT hr = pfd.CreateInstance(CLSID_FileSaveDialog, NULL, CLSCTX_INPROC_SERVER);
            if (SUCCEEDED(hr))
            {
                // Set the dialog options
                DWORD dwOptions;
                if (SUCCEEDED(hr = pfd->GetOptions(&dwOptions)))
                {
                    hr = pfd->SetOptions(dwOptions | FOS_FORCEFILESYSTEM | FOS_OVERWRITEPROMPT);
                }

                // Set a title
                if (SUCCEEDED(hr))
                {
                    ResString sTitle(hRes, IDS_APP_TITLE);
                    std::wstring s = (const wchar_t*)sTitle;
                    s += L" - ";
                    wchar_t buf[100] = {0};
                    m_TabBar.GetCurrentTitle(buf, _countof(buf));
                    s += buf;
                    pfd->SetTitle(s.c_str());
                }

                // set the default folder to the folder of the current tab
                if (!doc.m_path.empty())
                {
                    std::wstring folder = CPathUtils::GetParentDirectory(doc.m_path);
                    IShellItemPtr psiDefFolder = NULL;
                    hr = SHCreateItemFromParsingName(folder.c_str(), NULL, IID_PPV_ARGS(&psiDefFolder));

                    if (SUCCEEDED(hr))
                    {
                        pfd->SetFolder(psiDefFolder);
                    }
                }

                // Show the save file dialog
                if (SUCCEEDED(hr) && SUCCEEDED(hr = pfd->Show(*this)))
                {
                    IShellItemPtr psiResult = NULL;
                    hr = pfd->GetResult(&psiResult);
                    if (SUCCEEDED(hr))
                    {
                        PWSTR pszPath = NULL;
                        hr = psiResult->GetDisplayName(SIGDN_FILESYSPATH, &pszPath);
                        if (SUCCEEDED(hr))
                        {
                            doc.m_path = pszPath;
                            CMRU::Instance().AddPath(doc.m_path);
                        }
                        else
                            return bRet;
                    }
                    else
                        return bRet;
                }
                else
                    return bRet;
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
            {
                size_t endpos = m_scintilla.Call(SCI_GETLENGTH);
                char c = (char)m_scintilla.Call(SCI_GETCHARAT, endpos - 1);
                if ((c != '\r') && (c != '\n'))
                {
                    switch (doc.m_format)
                    {
                        case WIN_FORMAT:
                            m_scintilla.Call(SCI_APPENDTEXT, 2, (sptr_t)"\r\n");
                            break;
                        case MAC_FORMAT:
                            m_scintilla.Call(SCI_APPENDTEXT, 1, (sptr_t)"\r");
                            break;
                        case UNIX_FORMAT:
                        default:
                            m_scintilla.Call(SCI_APPENDTEXT, 1, (sptr_t)"\n");
                            break;
                    }
                }
            }
            bRet = m_DocManager.SaveFile(*this, doc);
            if (bRet)
            {
                if (_wcsicmp(CIniSettings::Instance().GetIniPath().c_str(), doc.m_path.c_str()) == 0)
                {
                    CIniSettings::Instance().Reload();
                }

                doc.m_bIsDirty = false;
                doc.m_bNeedsSaving = false;
                m_DocManager.UpdateFileTime(doc);
                if (bSaveAs)
                {
                    doc.m_language = CLexStyles::Instance().GetLanguageForExt(doc.m_path.substr(doc.m_path.find_last_of('.')+1));
                    m_scintilla.SetupLexerForExt(doc.m_path.substr(doc.m_path.find_last_of('.')+1).c_str());
                }
                std::wstring sFileName = doc.m_path.substr(doc.m_path.find_last_of('\\')+1);
                m_TabBar.SetCurrentTitle(sFileName.c_str());
                m_DocManager.SetDocument(id, doc);
                UpdateStatusBar(true);
                m_scintilla.Call(SCI_SETSAVEPOINT);
                CCommandHandler::Instance().OnDocumentSave(m_TabBar.GetCurrentTabIndex(), bSaveAs);
            }
        }
    }
    return bRet;
}

void CMainWindow::EnsureAtLeastOneTab()
{
    if (m_TabBar.GetItemCount() == 0)
        DoCommand(cmdNew);
}

void CMainWindow::GoToLine( size_t line )
{
    m_scintilla.Call(SCI_GOTOLINE, line);
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

    if (m_scintilla.GetSelectedCount(selByte, selLine))
        swprintf_s(strSel, L"Sel : %Iu | %Iu", selByte, selLine);
    else
        swprintf_s(strSel, L"Sel : %s", L"N/A");
    long line = (long)m_scintilla.Call(SCI_LINEFROMPOSITION, m_scintilla.Call(SCI_GETCURRENTPOS)) + 1;
    long column = (long)m_scintilla.Call(SCI_GETCOLUMN, m_scintilla.Call(SCI_GETCURRENTPOS)) + 1;
    swprintf_s(strLnCol, L"Ln : %ld    Col : %ld    %s",
                       line, column,
                       strSel);
    std::wstring ttcurpos = CStringUtils::Format(rsStatusTTCurPos, line, column, selByte, selLine);
    m_StatusBar.SetText(strLnCol, ttcurpos.c_str(), STATUSBAR_CUR_POS);

    TCHAR strDocLen[256];
    swprintf_s(strDocLen, L"length : %d    lines : %d", m_scintilla.Call(SCI_GETLENGTH), m_scintilla.Call(SCI_GETLINECOUNT));
    std::wstring ttdocsize = CStringUtils::Format(rsStatusTTDocSize, m_scintilla.Call(SCI_GETLENGTH), m_scintilla.Call(SCI_GETLINECOUNT));
    m_StatusBar.SetText(strDocLen, ttdocsize.c_str(), STATUSBAR_DOC_SIZE);

    std::wstring tttyping = CStringUtils::Format(rsStatusTTTyping, m_scintilla.Call(SCI_GETOVERTYPE) ? (LPCWSTR)rsStatusTTTypingOvl : (LPCWSTR)rsStatusTTTypingIns);
    m_StatusBar.SetText(m_scintilla.Call(SCI_GETOVERTYPE) ? L"OVR" : L"INS", tttyping.c_str(), STATUSBAR_TYPING_MODE);
    bool bCapsLockOn = (GetKeyState(VK_CAPITAL)&0x01)!=0;
    m_StatusBar.SetText(bCapsLockOn ? L"CAPS" : L"", NULL, STATUSBAR_CAPS);
    if (bEverything)
    {
        CDocument doc = m_DocManager.GetDocumentFromID(m_TabBar.GetCurrentTabId());
        m_StatusBar.SetText(doc.m_language.c_str(), NULL, STATUSBAR_DOC_TYPE);
        std::wstring tteof = CStringUtils::Format(rsStatusTTEOF, FormatTypeToString(doc.m_format).c_str());
        m_StatusBar.SetText(FormatTypeToString(doc.m_format).c_str(), tteof.c_str(), STATUSBAR_EOF_FORMAT);
        m_StatusBar.SetText(doc.GetEncodingString().c_str(), NULL, STATUSBAR_UNICODE_TYPE);

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
        ResString rTitle(hRes, IDS_HASMODIFICATIONS);
        ResString rQuestion(hRes, IDS_DOYOUWANTOSAVE);
        ResString rSave(hRes, IDS_SAVE);
        ResString rDontSave(hRes, IDS_DONTSAVE);
        wchar_t buf[100] = {0};
        m_TabBar.GetCurrentTitle(buf, _countof(buf));
        std::wstring sQuestion = CStringUtils::Format(rQuestion, buf);

        TASKDIALOGCONFIG tdc = { sizeof(TASKDIALOGCONFIG) };
        TASKDIALOG_BUTTON aCustomButtons[2];
        aCustomButtons[0].nButtonID = 100;
        aCustomButtons[0].pszButtonText = rSave;
        aCustomButtons[1].nButtonID = 101;
        aCustomButtons[1].pszButtonText = rDontSave;

        tdc.hwndParent = *this;
        tdc.hInstance = hRes;
        tdc.dwCommonButtons = TDCBF_CANCEL_BUTTON;
        tdc.pButtons = aCustomButtons;
        tdc.cButtons = _countof(aCustomButtons);
        tdc.pszWindowTitle = MAKEINTRESOURCE(IDS_APP_TITLE);
        tdc.pszMainIcon = TD_INFORMATION_ICON;
        tdc.pszMainInstruction = rTitle;
        tdc.pszContent = sQuestion.c_str();
        tdc.nDefaultButton = 100;
        int nClickedBtn = 0;
        HRESULT hr = TaskDialogIndirect ( &tdc, &nClickedBtn, NULL, NULL );

        if (SUCCEEDED(hr))
        {
            if (nClickedBtn == 100)
                SaveCurrentTab();
            else if (nClickedBtn != 101)
            {
                m_TabBar.ActivateAt(tab);
                return false;  // don't close!
            }
        }

        m_TabBar.ActivateAt(tab);
    }

    if ((m_TabBar.GetItemCount() == 1)&&(m_scintilla.Call(SCI_GETTEXTLENGTH)==0)&&(m_scintilla.Call(SCI_GETMODIFY)==0)&&doc.m_path.empty())
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
        if ((m_TabBar.GetItemCount() == 1)&&(m_scintilla.Call(SCI_GETTEXTLENGTH)==0)&&(m_scintilla.Call(SCI_GETMODIFY)==0)&&m_DocManager.GetDocumentFromID(m_TabBar.GetIDFromIndex(0)).m_path.empty())
            return true;
    } while (m_TabBar.GetItemCount() > 0);
    return true;
}

bool CMainWindow::OpenFile(const std::wstring& file, bool bAddToMRU)
{
    bool bRet = true;
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
        CDocument doc = m_DocManager.LoadFile(*this, filepath.c_str(), encoding);
        if (doc.m_document)
        {
            if (m_TabBar.GetItemCount() == 1)
            {
                // check if the only tab is empty and if it is, remove it
                CDocument existDoc = m_DocManager.GetDocumentFromID(m_TabBar.GetIDFromIndex(0));
                if (existDoc.m_path.empty() && (m_scintilla.Call(SCI_GETLENGTH)==0) && (m_scintilla.Call(SCI_CANUNDO)==0))
                {
                    m_DocManager.RemoveDocument(m_TabBar.GetIDFromIndex(0));
                    m_TabBar.DeletItemAt(0);
                }
            }
            if (bAddToMRU)
                CMRU::Instance().AddPath(filepath);
            std::wstring sFileName = filepath.substr(filepath.find_last_of('\\')+1);
            auto dotpos = filepath.find_last_of('.');
            std::wstring sExt = filepath.substr(dotpos + 1);
            int index = -1;
            if (m_insertionIndex >= 0)
                index = m_TabBar.InsertAfter(m_insertionIndex, sFileName.c_str());
            else
                index = m_TabBar.InsertAtEnd(sFileName.c_str());
            int id = m_TabBar.GetIDFromIndex(index);
            if (dotpos == std::wstring::npos)
                doc.m_language = CLexStyles::Instance().GetLanguageForPath(filepath);
            else
                doc.m_language = CLexStyles::Instance().GetLanguageForExt(sExt);
            m_DocManager.AddDocumentAtEnd(doc, id);
            m_TabBar.ActivateAt(index);
            m_scintilla.Call(SCI_SETDOCPOINTER, 0, doc.m_document);
            if (dotpos == std::wstring::npos)
                m_scintilla.SetupLexerForLang(doc.m_language);
            else
                m_scintilla.SetupLexerForExt(sExt.c_str());
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

bool CMainWindow::ReloadTab( int tab, int encoding )
{
    if ((tab < m_TabBar.GetItemCount())&&(tab < m_DocManager.GetCount()))
    {
        // first check if the document is modified and needs saving
        CDocument doc = m_DocManager.GetDocumentFromID(m_TabBar.GetIDFromIndex(tab));
        if (doc.m_bIsDirty||doc.m_bNeedsSaving)
        {
            // doc has modifications, ask the user what to do:
            // * reload without saving
            // * save first, then reload
            // * cancel
            m_TabBar.ActivateAt(tab);
            ResString rTitle(hRes, IDS_HASMODIFICATIONS);
            ResString rQuestion(hRes, IDS_RELOADREALLY);
            ResString rReload(hRes, IDS_DORELOAD);
            ResString rCancel(hRes, IDS_DONTRELOAD);
            wchar_t buf[100] = {0};
            m_TabBar.GetCurrentTitle(buf, _countof(buf));
            std::wstring sQuestion = CStringUtils::Format(rQuestion, buf);

            TASKDIALOGCONFIG tdc = { sizeof(TASKDIALOGCONFIG) };
            TASKDIALOG_BUTTON aCustomButtons[2];
            aCustomButtons[0].nButtonID = 100;
            aCustomButtons[0].pszButtonText = rReload;
            aCustomButtons[1].nButtonID = 101;
            aCustomButtons[1].pszButtonText = rCancel;

            tdc.hwndParent = *this;
            tdc.hInstance = hRes;
            tdc.dwFlags = TDF_USE_COMMAND_LINKS | TDF_ENABLE_HYPERLINKS | TDF_POSITION_RELATIVE_TO_WINDOW | TDF_SIZE_TO_CONTENT | TDF_ALLOW_DIALOG_CANCELLATION;
            tdc.pButtons = aCustomButtons;
            tdc.cButtons = _countof(aCustomButtons);
            tdc.pszWindowTitle = MAKEINTRESOURCE(IDS_APP_TITLE);
            tdc.pszMainIcon = TD_INFORMATION_ICON;
            tdc.pszMainInstruction = rTitle;
            tdc.pszContent = sQuestion.c_str();
            tdc.nDefaultButton = 101;
            int nClickedBtn = 0;
            HRESULT hr = TaskDialogIndirect ( &tdc, &nClickedBtn, NULL, NULL );

            if (SUCCEEDED(hr) && (nClickedBtn != 100))
            {
                m_TabBar.ActivateAt(tab);
                return false;  // don't close!
            }

            m_TabBar.ActivateAt(tab);
        }

        CDocument docreload = m_DocManager.LoadFile(*this, doc.m_path.c_str(), encoding);
        if (docreload.m_document)
        {
            if (tab == m_TabBar.GetCurrentTabIndex())
                m_scintilla.SaveCurrentPos(&doc.m_position);

            m_scintilla.Call(SCI_SETDOCPOINTER, 0, docreload.m_document);
            docreload.m_language = doc.m_language;
            docreload.m_position = doc.m_position;
            std::wstring sFileName = doc.m_path.substr(doc.m_path.find_last_of('\\') + 1);
            m_DocManager.SetDocument(m_TabBar.GetIDFromIndex(tab), docreload);
            if (tab == m_TabBar.GetCurrentTabIndex())
            {
                m_scintilla.SetupLexerForLang(docreload.m_language);
                m_scintilla.RestoreCurrentPos(docreload.m_position);
            }
            UpdateStatusBar(true);
            m_scintilla.Call(SCI_SETSAVEPOINT);
            return true;
        }
    }
    return false;
}

void CMainWindow::AddHotSpots()
{
    long startPos = 0;
    long endPos = -1;
    long endStyle = (long)m_scintilla.Call(SCI_GETENDSTYLED);


    long firstVisibleLine = (long)m_scintilla.Call(SCI_GETFIRSTVISIBLELINE);
    startPos = (long)m_scintilla.Call(SCI_POSITIONFROMLINE, m_scintilla.Call(SCI_DOCLINEFROMVISIBLE, firstVisibleLine));
    long linesOnScreen = (long)m_scintilla.Call(SCI_LINESONSCREEN);
    long lineCount = (long)m_scintilla.Call(SCI_GETLINECOUNT);
    endPos = (long)m_scintilla.Call(SCI_POSITIONFROMLINE, m_scintilla.Call(SCI_DOCLINEFROMVISIBLE, firstVisibleLine + min(linesOnScreen, lineCount)));

    std::vector<unsigned char> hotspotPairs;

    unsigned char style_hotspot = 0;
    unsigned char mask = INDIC2_MASK;

    // to speed up the search, first search for "://" without using the regex engine
    long fStartPos = startPos;
    long fEndPos = endPos;
    m_scintilla.Call(SCI_SETSEARCHFLAGS, 0);
    m_scintilla.Call(SCI_SETTARGETSTART, fStartPos);
    m_scintilla.Call(SCI_SETTARGETEND, fEndPos);
    LRESULT posFoundColonSlash = m_scintilla.Call(SCI_SEARCHINTARGET, 3, (LPARAM)"://");
    while (posFoundColonSlash != -1)
    {
        // found a "://"
        long lineFoundcolonSlash = (long)m_scintilla.Call(SCI_LINEFROMPOSITION, posFoundColonSlash);
        startPos = (long)m_scintilla.Call(SCI_POSITIONFROMLINE, lineFoundcolonSlash);
        endPos = (long)m_scintilla.Call(SCI_GETLINEENDPOSITION, lineFoundcolonSlash);
        fStartPos = (long)posFoundColonSlash + 1;

        m_scintilla.Call(SCI_SETSEARCHFLAGS, SCFIND_REGEXP|SCFIND_POSIX);

        m_scintilla.Call(SCI_SETTARGETSTART, startPos);
        m_scintilla.Call(SCI_SETTARGETEND, endPos);

        LRESULT posFound = m_scintilla.Call(SCI_SEARCHINTARGET, strlen(URL_REG_EXPR), (LPARAM)URL_REG_EXPR);

        while (posFound != -1)
        {
            long start = long(m_scintilla.Call(SCI_GETTARGETSTART));
            long end = long(m_scintilla.Call(SCI_GETTARGETEND));
            long foundTextLen = end - start;
            unsigned char idStyle = static_cast<unsigned char>(m_scintilla.Call(SCI_GETSTYLEAT, posFound));

            // Search the style
            long fs = -1;
            for (size_t i = 0 ; i < hotspotPairs.size() ; i++)
            {
                // make sure to ignore "hotspot bit" when comparing document style with archived hotspot style
                if ((hotspotPairs[i] & ~mask) == (idStyle & ~mask))
                {
                    fs = hotspotPairs[i];
                    m_scintilla.Call(SCI_STYLEGETFORE, fs);
                    break;
                }
            }

            // if we found it then use it to colorize
            if (fs != -1)
            {
                m_scintilla.Call(SCI_STARTSTYLING, start, 0xFF);
                m_scintilla.Call(SCI_SETSTYLING, foundTextLen, fs);
            }
            else // generate a new style and add it into a array
            {
                style_hotspot = idStyle | mask; // set "hotspot bit"
                hotspotPairs.push_back(style_hotspot);
                int activeFG = 0xFF0000;
                unsigned char idStyleMSBunset = idStyle & ~mask;
                char fontNameA[128];

                // copy the style data
                m_scintilla.Call(SCI_STYLEGETFONT, idStyleMSBunset, (LPARAM)fontNameA);
                m_scintilla.Call(SCI_STYLESETFONT, style_hotspot, (LPARAM)fontNameA);

                m_scintilla.Call(SCI_STYLESETSIZE, style_hotspot, m_scintilla.Call(SCI_STYLEGETSIZE, idStyleMSBunset));
                m_scintilla.Call(SCI_STYLESETBOLD, style_hotspot, m_scintilla.Call(SCI_STYLEGETBOLD, idStyleMSBunset));
                m_scintilla.Call(SCI_STYLESETWEIGHT, style_hotspot, m_scintilla.Call(SCI_STYLEGETWEIGHT, idStyleMSBunset));
                m_scintilla.Call(SCI_STYLESETITALIC, style_hotspot, m_scintilla.Call(SCI_STYLEGETITALIC, idStyleMSBunset));
                m_scintilla.Call(SCI_STYLESETUNDERLINE, style_hotspot, m_scintilla.Call(SCI_STYLEGETUNDERLINE, idStyleMSBunset));
                m_scintilla.Call(SCI_STYLESETFORE, style_hotspot, m_scintilla.Call(SCI_STYLEGETFORE, idStyleMSBunset));
                m_scintilla.Call(SCI_STYLESETBACK, style_hotspot, m_scintilla.Call(SCI_STYLEGETBACK, idStyleMSBunset));
                m_scintilla.Call(SCI_STYLESETEOLFILLED, style_hotspot, m_scintilla.Call(SCI_STYLEGETEOLFILLED, idStyleMSBunset));
                m_scintilla.Call(SCI_STYLESETCASE, style_hotspot, m_scintilla.Call(SCI_STYLEGETCASE, idStyleMSBunset));


                m_scintilla.Call(SCI_STYLESETHOTSPOT, style_hotspot, TRUE);
                m_scintilla.Call(SCI_SETHOTSPOTACTIVEUNDERLINE, style_hotspot, TRUE);
                m_scintilla.Call(SCI_SETHOTSPOTACTIVEFORE, TRUE, activeFG);
                m_scintilla.Call(SCI_SETHOTSPOTSINGLELINE, style_hotspot, 0);

                // colorize it!
                m_scintilla.Call(SCI_STARTSTYLING, start, 0xFF);
                m_scintilla.Call(SCI_SETSTYLING, foundTextLen, style_hotspot);
            }

            m_scintilla.Call(SCI_SETTARGETSTART, posFound + max(1, foundTextLen));
            m_scintilla.Call(SCI_SETTARGETEND, endPos);

            posFound = (int)m_scintilla.Call(SCI_SEARCHINTARGET, strlen(URL_REG_EXPR), (LPARAM)URL_REG_EXPR);
        }
        m_scintilla.Call(SCI_SETTARGETSTART, fStartPos);
        m_scintilla.Call(SCI_SETTARGETEND, fEndPos);
        m_scintilla.Call(SCI_SETSEARCHFLAGS, 0);
        posFoundColonSlash = (int)m_scintilla.Call(SCI_SEARCHINTARGET, 3, (LPARAM)"://");
    }

    m_scintilla.Call(SCI_STARTSTYLING, endStyle, 0xFF);
    m_scintilla.Call(SCI_SETSTYLING, 0, 0);
}

void CMainWindow::AutoIndent( Scintilla::SCNotification * pScn )
{
    int eolMode = int(m_scintilla.Call(SCI_GETEOLMODE));
    size_t curLine = m_scintilla.Call(SCI_LINEFROMPOSITION, m_scintilla.Call(SCI_GETCURRENTPOS));
    size_t lastLine = curLine - 1;
    int indentAmount = 0;

    if (((eolMode == SC_EOL_CRLF || eolMode == SC_EOL_LF) && pScn->ch == '\n') ||
        (eolMode == SC_EOL_CR && pScn->ch == '\r'))
    {
        // use the same indentation as the last line
        while (lastLine > 0 && (m_scintilla.Call(SCI_GETLINEENDPOSITION, lastLine) - m_scintilla.Call(SCI_POSITIONFROMLINE, lastLine)) == 0)
            lastLine--;

        indentAmount = (int)m_scintilla.Call(SCI_GETLINEINDENTATION, lastLine);

        if (indentAmount > 0)
        {
            Scintilla::CharacterRange crange;
            crange.cpMin = long(m_scintilla.Call(SCI_GETSELECTIONSTART));
            crange.cpMax = long(m_scintilla.Call(SCI_GETSELECTIONEND));
            int posBefore = (int)m_scintilla.Call(SCI_GETLINEINDENTPOSITION, curLine);
            m_scintilla.Call(SCI_SETLINEINDENTATION, curLine, indentAmount);
            int posAfter = (int)m_scintilla.Call(SCI_GETLINEINDENTPOSITION, curLine);
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
            m_scintilla.Call(SCI_SETSEL, crange.cpMin, crange.cpMax);
        }
    }
}

bool CMainWindow::HandleOutsideModifications( int id /*= -1*/ )
{
    static bool bInHandleOutsideModifications = false;
    // recurse protection
    if (bInHandleOutsideModifications)
        return false;

    bInHandleOutsideModifications = true;
    bool bRet = true;
    for (int i = 0; i < m_TabBar.GetItemCount(); ++i)
    {
        int docID = id == -1 ? m_TabBar.GetIDFromIndex(i) : id;
        auto ds = m_DocManager.HasFileChanged(docID);
        if (ds == DM_Modified)
        {
            CDocument doc = m_DocManager.GetDocumentFromID(docID);
            m_TabBar.ActivateAt(i);
            ResString rTitle(hRes, IDS_OUTSIDEMODIFICATIONS);
            ResString rQuestion(hRes, doc.m_bNeedsSaving || doc.m_bIsDirty ? IDS_DOYOUWANTRELOADBUTDIRTY : IDS_DOYOUWANTTORELOAD);
            ResString rSave(hRes, IDS_SAVELOSTOUTSIDEMODS);
            ResString rReload(hRes, doc.m_bNeedsSaving || doc.m_bIsDirty ? IDS_RELOADLOSTMODS : IDS_RELOAD);
            ResString rCancel(hRes, IDS_NORELOAD);
            wchar_t buf[100] = {0};
            m_TabBar.GetCurrentTitle(buf, _countof(buf));
            std::wstring sQuestion = CStringUtils::Format(rQuestion, buf);

            TASKDIALOGCONFIG tdc = { sizeof(TASKDIALOGCONFIG) };
            TASKDIALOG_BUTTON aCustomButtons[3];
            int bi = 0;
            aCustomButtons[bi].nButtonID = bi+100;
            aCustomButtons[bi++].pszButtonText = rReload;
            if (doc.m_bNeedsSaving || doc.m_bIsDirty)
            {
                aCustomButtons[bi].nButtonID = bi+100;
                aCustomButtons[bi++].pszButtonText = rSave;
            }
            aCustomButtons[bi].nButtonID = bi+100;
            aCustomButtons[bi].pszButtonText = rCancel;

            tdc.hwndParent = *this;
            tdc.hInstance = hRes;
            tdc.dwFlags = TDF_USE_COMMAND_LINKS | TDF_POSITION_RELATIVE_TO_WINDOW | TDF_SIZE_TO_CONTENT | TDF_ALLOW_DIALOG_CANCELLATION;
            tdc.pButtons = aCustomButtons;
            tdc.cButtons = doc.m_bNeedsSaving || doc.m_bIsDirty ? 3 : 2;
            tdc.pszWindowTitle = MAKEINTRESOURCE(IDS_APP_TITLE);
            tdc.pszMainIcon = doc.m_bNeedsSaving || doc.m_bIsDirty ? TD_WARNING_ICON : TD_INFORMATION_ICON;
            tdc.pszMainInstruction = rTitle;
            tdc.pszContent = sQuestion.c_str();
            tdc.nDefaultButton = doc.m_bNeedsSaving || doc.m_bIsDirty ? 102 : 100;  // default to "Cancel" in case the file is modified
            int nClickedBtn = 0;
            HRESULT hr = TaskDialogIndirect ( &tdc, &nClickedBtn, NULL, NULL );

            if (SUCCEEDED(hr))
            {
                if (nClickedBtn == 100)
                {
                    // reload the document
                    CDocument docreload = m_DocManager.LoadFile(*this, doc.m_path.c_str(), doc.m_encoding);
                    if (docreload.m_document)
                    {
                        m_scintilla.SaveCurrentPos(&doc.m_position);

                        m_scintilla.Call(SCI_SETDOCPOINTER, 0, docreload.m_document);
                        docreload.m_language = doc.m_language;
                        docreload.m_position = doc.m_position;
                        m_DocManager.SetDocument(docID, docreload);
                        m_scintilla.SetupLexerForLang(docreload.m_language);
                        m_scintilla.RestoreCurrentPos(docreload.m_position);
                    }
                }
                else if ((nClickedBtn == 101) && (doc.m_bNeedsSaving || doc.m_bIsDirty))
                {
                    // save the document
                    SaveCurrentTab();
                    bRet = false;
                }
                else
                {
                    // update the filetime of the document to avoid this warning
                    m_DocManager.UpdateFileTime(doc);
                    m_DocManager.SetDocument(docID, doc);
                    bRet = false;
                }
            }
            else
            {
                // update the filetime of the document to avoid this warning
                m_DocManager.UpdateFileTime(doc);
                m_DocManager.SetDocument(docID, doc);
            }
        }
        else if (ds == DM_Removed)
        {
            // file was removed. Options are:
            // * keep the file open
            // * close the file
            CDocument doc = m_DocManager.GetDocumentFromID(docID);
            m_TabBar.ActivateAt(i);
            ResString rTitle(hRes, IDS_OUTSIDEREMOVEDHEAD);
            ResString rQuestion(hRes, IDS_OUTSIDEREMOVED);
            ResString rKeep(hRes, IDS_OUTSIDEREMOVEDKEEP);
            ResString rClose(hRes, IDS_OUTSIDEREMOVEDCLOSE);
            wchar_t buf[100] = {0};
            m_TabBar.GetCurrentTitle(buf, _countof(buf));
            std::wstring sQuestion = CStringUtils::Format(rQuestion, buf);

            TASKDIALOGCONFIG tdc = { sizeof(TASKDIALOGCONFIG) };
            TASKDIALOG_BUTTON aCustomButtons[2];
            int bi = 0;
            aCustomButtons[bi].nButtonID = bi+100;
            aCustomButtons[bi++].pszButtonText = rKeep;
            aCustomButtons[bi].nButtonID = bi+100;
            aCustomButtons[bi].pszButtonText = rClose;

            tdc.hwndParent = *this;
            tdc.hInstance = hRes;
            tdc.dwFlags = TDF_USE_COMMAND_LINKS | TDF_POSITION_RELATIVE_TO_WINDOW | TDF_SIZE_TO_CONTENT | TDF_ALLOW_DIALOG_CANCELLATION;
            tdc.pButtons = aCustomButtons;
            tdc.cButtons = _countof(aCustomButtons);
            tdc.pszWindowTitle = MAKEINTRESOURCE(IDS_APP_TITLE);
            tdc.pszMainIcon = TD_INFORMATION_ICON;
            tdc.pszMainInstruction = rTitle;
            tdc.pszContent = sQuestion.c_str();
            tdc.nDefaultButton = 100;
            int nClickedBtn = 0;
            HRESULT hr = TaskDialogIndirect ( &tdc, &nClickedBtn, NULL, NULL );

            if (SUCCEEDED(hr) && (nClickedBtn == 101))
            {
                // close the tab
                CloseTab(i);
                if (id == -1)
                    --i;    // the tab was removed, so continue with the next one
            }
            else
            {
                // keep the file: mark the file as modified
                doc.m_bNeedsSaving = true;
                // update the filetime of the document to avoid this warning
                m_DocManager.UpdateFileTime(doc);
                m_DocManager.SetDocument(docID, doc);
                // the next to calls are only here to trigger SCN_SAVEPOINTLEFT/SCN_SAVEPOINTREACHED messages
                m_scintilla.Call(SCI_ADDUNDOACTION, 0, 0);
                m_scintilla.Call(SCI_UNDO);
                bRet = false;
            }
        }

        if (id != -1)
            break;
    }
    bInHandleOutsideModifications = false;
    return bRet;
}

void CMainWindow::ElevatedSave( const std::wstring& path, const std::wstring& savepath, long line )
{
    std::wstring filepath = CPathUtils::GetLongPathname(path);
    int id = m_DocManager.GetIdForPath(filepath.c_str());
    if (id != -1)
    {
        m_TabBar.ActivateAt(m_TabBar.GetIndexFromID(id));
        CDocument doc = m_DocManager.GetDocumentFromID(id);
        doc.m_path = CPathUtils::GetLongPathname(savepath);
        m_DocManager.SetDocument(id, doc);
        SaveCurrentTab();
        wchar_t sTabTitle[100] = {0};
        m_TabBar.GetCurrentTitle(sTabTitle, _countof(sTabTitle));
        std::wstring sWindowTitle = CStringUtils::Format(L"%s - BowPad", doc.m_path.empty() ? sTabTitle : doc.m_path.c_str());
        SetWindowText(*this, sWindowTitle.c_str());
        UpdateStatusBar(true);
        GoToLine(line);

        // delete the temp file used for the elevated save
        DeleteFile(path.c_str());
    }
}

void CMainWindow::TabMove(const std::wstring& path, const std::wstring& savepath, bool bMod, long line)
{
    std::wstring filepath = CPathUtils::GetLongPathname(path);
    int id = m_DocManager.GetIdForPath(filepath.c_str());
    if (id != -1)
    {
        m_TabBar.ActivateAt(m_TabBar.GetIndexFromID(id));
        CDocument doc = m_DocManager.GetDocumentFromID(id);
        doc.m_path = CPathUtils::GetLongPathname(savepath);
        doc.m_bIsDirty = bMod;
        doc.m_bNeedsSaving = bMod;
        m_DocManager.UpdateFileTime(doc);
        doc.m_language = CLexStyles::Instance().GetLanguageForExt(doc.m_path.substr(doc.m_path.find_last_of('.') + 1));
        m_DocManager.SetDocument(id, doc);

        m_scintilla.SetupLexerForExt(doc.m_path.substr(doc.m_path.find_last_of('.') + 1).c_str());
        std::wstring sFileName = doc.m_path.substr(doc.m_path.find_last_of('\\') + 1);
        m_TabBar.SetCurrentTitle(sFileName.c_str());

        wchar_t sTabTitle[100] = { 0 };
        m_TabBar.GetCurrentTitle(sTabTitle, _countof(sTabTitle));
        std::wstring sWindowTitle = CStringUtils::Format(L"%s - BowPad", doc.m_path.empty() ? sTabTitle : doc.m_path.c_str());
        SetWindowText(*this, sWindowTitle.c_str());
        UpdateStatusBar(true);

        TCITEM tie;
        tie.lParam = -1;
        tie.mask = TCIF_IMAGE;
        tie.iImage = doc.m_bIsDirty || doc.m_bNeedsSaving ? UNSAVED_IMG_INDEX : SAVED_IMG_INDEX;
        if (doc.m_bIsReadonly)
            tie.iImage = REDONLY_IMG_INDEX;
        ::SendMessage(m_TabBar, TCM_SETITEM, m_TabBar.GetIndexFromID(id), reinterpret_cast<LPARAM>(&tie));

        GoToLine(line);

        // delete the temp file
        DeleteFile(path.c_str());
    }
}

bool CMainWindow::AskToCreateNonExistingFile(const std::wstring& path)
{
    if (!PathFileExists(path.c_str()))
    {
        ResString rTitle(hRes, IDS_FILE_DOESNT_EXIST);
        ResString rQuestion(hRes, IDS_FILE_ASK_TO_CREATE);
        ResString rCreate(hRes, IDS_FILE_CREATE);
        ResString rCancel(hRes, IDS_FILE_CREATE_CANCEL);
        std::wstring sQuestion = CStringUtils::Format(rQuestion, path.substr(path.find_last_of('\\') + 1).c_str());

        TASKDIALOGCONFIG tdc = { sizeof(TASKDIALOGCONFIG) };
        TASKDIALOG_BUTTON aCustomButtons[2];
        int bi = 0;
        aCustomButtons[bi].nButtonID = bi + 100;
        aCustomButtons[bi++].pszButtonText = rCreate;
        aCustomButtons[bi].nButtonID = bi + 100;
        aCustomButtons[bi++].pszButtonText = rCancel;

        tdc.hwndParent = *this;
        tdc.hInstance = hRes;
        tdc.dwFlags = TDF_USE_COMMAND_LINKS | TDF_POSITION_RELATIVE_TO_WINDOW | TDF_SIZE_TO_CONTENT | TDF_ALLOW_DIALOG_CANCELLATION;
        tdc.pButtons = aCustomButtons;
        tdc.cButtons = _countof(aCustomButtons);
        tdc.pszWindowTitle = MAKEINTRESOURCE(IDS_APP_TITLE);
        tdc.pszMainIcon = TD_INFORMATION_ICON;
        tdc.pszMainInstruction = rTitle;
        tdc.pszContent = sQuestion.c_str();
        tdc.nDefaultButton = 100;
        int nClickedBtn = 0;
        HRESULT hr = TaskDialogIndirect(&tdc, &nClickedBtn, NULL, NULL);

        if (SUCCEEDED(hr) && (nClickedBtn == 100))
        {
            HANDLE hFile = CreateFile(path.c_str(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_DELETE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
            if (hFile != INVALID_HANDLE_VALUE)
            {
                CloseHandle(hFile);
            }
        }
        else
            return false;
    }
    return true;
}
