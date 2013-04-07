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
#include "stdafx.h"
#include "MainWindow.h"
#include "BowPad.h"
#include "BowPadUI.h"
#include "StringUtils.h"
#include "MRU.h"

#include <memory>
#include <Shobjidl.h>
#include <Shellapi.h>

IUIFramework *g_pFramework = NULL;  // Reference to the Ribbon framework.

#define STATUSBAR_DOC_TYPE 0
#define STATUSBAR_DOC_SIZE 1
#define STATUSBAR_CUR_POS 2
#define STATUSBAR_EOF_FORMAT 3
#define STATUSBAR_UNICODE_TYPE 4
#define STATUSBAR_TYPING_MODE 5


CMainWindow::CMainWindow(HINSTANCE hInst, const WNDCLASSEX* wcx /* = NULL*/)
    : CWindow(hInst, wcx)
    , m_StatusBar(hInst)
    , m_TabBar(hInst)
    , m_scintilla(hInst)
    , m_cRef(1)
{
}

CMainWindow::~CMainWindow(void)
{

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

//
//  FUNCTION: OnCreateUICommand(UINT, UI_COMMANDTYPE, IUICommandHandler)
//
//  PURPOSE: Called by the Ribbon framework for each command specified in markup, to allow
//           the host application to bind a command handler to that command.
//
//    To view the OnCreateUICommand callbacks, uncomment the _cwprintf call.
//
//
STDMETHODIMP CMainWindow::OnCreateUICommand(
    UINT nCmdID,
    UI_COMMANDTYPE typeID,
    IUICommandHandler** ppCommandHandler)
{
    UNREFERENCED_PARAMETER(typeID);
    UNREFERENCED_PARAMETER(nCmdID);

    return QueryInterface(IID_PPV_ARGS(ppCommandHandler));
}

//
//  FUNCTION: OnViewChanged(UINT, UI_VIEWTYPE, IUnknown*, UI_VIEWVERB, INT)
//
//  PURPOSE: Called when the state of a View (Ribbon is a view) changes, for example, created, destroyed, or resized.
//
//
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
            hr = pView->QueryInterface(IID_PPV_ARGS(&m_pRibbon));
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
            hr = S_OK;
            m_pRibbon->Release();
            m_pRibbon = NULL;
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
    if(UI_PKEY_Enabled == key)
    {
        return UIInitPropertyFromBoolean(UI_PKEY_Enabled, GetStatus(nCmdID), ppropvarNewValue);
    }
    if (UI_PKEY_BooleanValue == key)
    {
        hr = UIInitPropertyFromBoolean(UI_PKEY_BooleanValue, GetState(nCmdID), ppropvarNewValue);
    }

    switch(nCmdID)
    {
    case cmdMRUList:
        if (UI_PKEY_RecentItems == key)
        {
            hr = CMRU::Instance().PopulateRibbonRecentItems(ppropvarNewValue);
        }
        break;
    default:
        break;
    }
    return hr;
}

STDMETHODIMP CMainWindow::Execute(
    UINT nCmdID,
    UI_EXECUTIONVERB verb,
    const PROPERTYKEY* key,
    const PROPVARIANT* ppropvarValue,
    IUISimplePropertySet* pCommandExecutionProperties)
{
    UNREFERENCED_PARAMETER(pCommandExecutionProperties);
    UNREFERENCED_PARAMETER(ppropvarValue);
    UNREFERENCED_PARAMETER(key);
    UNREFERENCED_PARAMETER(verb);

    HRESULT hr = S_OK;
    if (nCmdID == cmdMRUList)
    {
        if (*key == UI_PKEY_RecentItems)
        {
            if (ppropvarValue)
            {
                SAFEARRAY * psa = V_ARRAY(ppropvarValue);
                LONG lstart, lend;
                hr = SafeArrayGetLBound( psa, 1, &lstart );
                if (FAILED(hr))
                    return hr;
                hr = SafeArrayGetUBound( psa, 1, &lend );
                if (FAILED(hr))
                    return hr;
                IUISimplePropertySet ** data;
                hr = SafeArrayAccessData(psa,(void **)&data);
                for (LONG idx = lstart; idx <= lend; ++idx)
                {
                    IUISimplePropertySet * ppset = (IUISimplePropertySet *)data[idx];
                    if (ppset)
                    {
                        PROPVARIANT var;
                        ppset->GetValue(UI_PKEY_LabelDescription, &var);
                        std::wstring path = var.bstrVal;
                        PropVariantClear(&var);
                        ppset->GetValue(UI_PKEY_Pinned, &var);
                        bool bPinned = VARIANT_TRUE==var.boolVal;
                        PropVariantClear(&var);
                        CMRU::Instance().PinPath(path, bPinned);
                    }
                }
                hr = SafeArrayUnaccessData(psa);
            }
        }
        if (*key == UI_PKEY_SelectedItem)
        {
            if (pCommandExecutionProperties)
            {
                PROPVARIANT var;
                pCommandExecutionProperties->GetValue(UI_PKEY_LabelDescription, &var);
                std::wstring path = var.bstrVal;
                PropVariantClear(&var);
                std::vector<std::wstring> files;
                files.push_back(path);
                OpenFiles(files);
            }
        }
    }

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
    ResString clsName(hResource, IDC_BOWPAD);
    wcx.lpszClassName = clsName;
    wcx.hIcon = LoadIcon(hResource, MAKEINTRESOURCE(IDI_BOWPAD));
    wcx.hbrBackground = (HBRUSH)(COLOR_3DFACE+1);
    wcx.lpszMenuName = NULL;
    wcx.hIconSm = LoadIcon(wcx.hInstance, MAKEINTRESOURCE(IDI_BOWPAD));
    if (RegisterWindow(&wcx))
    {
        if (CreateEx(WS_EX_ACCEPTFILES, WS_CAPTION | WS_MAXIMIZEBOX | WS_MINIMIZEBOX | WS_SIZEBOX | WS_SYSMENU | WS_CLIPCHILDREN, NULL))
        {
            ShowWindow(*this, SW_SHOW);
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
                OpenFiles(files);
            }
        }
        break;
    case WM_NOTIFY:
        {
            LPNMHDR pNMHDR = reinterpret_cast<LPNMHDR>(lParam);
            if ((pNMHDR->idFrom == (UINT_PTR)&m_TabBar) ||
                (pNMHDR->hwndFrom == m_TabBar))
            {
                TBHDR * ptbhdr = reinterpret_cast<TBHDR*>(lParam);

                switch (((LPNMHDR)lParam)->code)
                {
                case TCN_GETCOLOR:
                    if ((ptbhdr->tabOrigin >= 0) && (ptbhdr->tabOrigin < m_DocManager.GetCount()))
                    {
                        return m_DocManager.GetColorForDocument(ptbhdr->tabOrigin);
                    }
                    break;
                case TCN_SELCHANGE:
                    {
                        // document got activated
                        int tab = m_TabBar.GetCurrentTabIndex();
                        if ((tab >= 0) && (tab < m_DocManager.GetCount()))
                        {
                            CDocument doc = m_DocManager.GetDocument(tab);
                            m_scintilla.Call(SCI_SETDOCPOINTER, 0, doc.m_document);
                            m_scintilla.RestoreCurrentPos(doc.m_position);
                            SetFocus(m_scintilla);
                            m_scintilla.Call(SCI_GRABFOCUS);
                            m_StatusBar.SetText(doc.m_language.c_str(), STATUSBAR_DOC_TYPE);
                            m_StatusBar.SetText(FormatTypeToString(doc.m_format).c_str(), STATUSBAR_EOF_FORMAT);
                            m_StatusBar.SetText(doc.GetEncodingString().c_str(), STATUSBAR_UNICODE_TYPE);

                        }
                    }
                    break;
                case TCN_SELCHANGING:
                    {
                        // document is about to be deactivated
                        int tab = m_TabBar.GetCurrentTabIndex();
                        if ((tab >= 0) && (tab < m_DocManager.GetCount()))
                        {
                            CDocument doc = m_DocManager.GetDocument(tab);
                            m_scintilla.SaveCurrentPos(&doc.m_position);
                            m_DocManager.SetDocument(tab, doc);
                        }
                    }
                    break;
                case TCN_TABDROPPED:
                    {
                        int src = m_TabBar.GetSrcTab();
                        int dst = m_TabBar.GetDstTab();
                        m_DocManager.ExchangeDocs(src, dst);
                    }
                    break;
                case TCN_TABDELETE:
                    {
                        int tab = m_TabBar.GetCurrentTabIndex();
                        CDocument doc = m_DocManager.GetDocument(ptbhdr->tabOrigin);
                        if (doc.m_bIsDirty)
                        {
                            m_TabBar.ActivateAt(ptbhdr->tabOrigin);
                            ResString rTitle(hInst, IDS_HASMODIFICATIONS);
                            ResString rQuestion(hInst, IDS_DOYOUWANTOSAVE);
                            ResString rSave(hInst, IDS_SAVE);
                            ResString rDontSave(hInst, IDS_DONTSAVE);
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
                            tdc.hInstance = hInst;
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
                                    break;  // don't close!
                                }
                            }

                            m_TabBar.ActivateAt(tab);
                        }

                        if ((m_TabBar.GetItemCount() == 1)&&(m_scintilla.Call(SCI_GETTEXTLENGTH)==0)&&(m_scintilla.Call(SCI_GETMODIFY)==0)&&doc.m_path.empty())
                            break;  // leave the empty, new document as is

                        if (tab == ptbhdr->tabOrigin)
                        {
                            if (tab > 0)
                                m_TabBar.ActivateAt(tab-1);
                        }
                        m_DocManager.RemoveDocument(ptbhdr->tabOrigin);
                        m_TabBar.DeletItemAt(ptbhdr->tabOrigin);
                        EnsureAtLeastOneTab();
                    }
                    break;
                }
            }
            else if ((pNMHDR->idFrom == (UINT_PTR)&m_scintilla) ||
                (pNMHDR->hwndFrom == m_scintilla))
            {
                Scintilla::SCNotification * pScn = reinterpret_cast<Scintilla::SCNotification *>(lParam);
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
                        int tab = m_TabBar.GetCurrentTabIndex();
                        if ((tab >= 0) && (tab < m_DocManager.GetCount()))
                        {
                            CDocument doc = m_DocManager.GetDocument(tab);
                            doc.m_bIsDirty = pScn->nmhdr.code == SCN_SAVEPOINTLEFT;
                            m_DocManager.SetDocument(tab, doc);
                            TCITEM tie;
                            tie.lParam = -1;
                            tie.mask = TCIF_IMAGE;
                            tie.iImage = doc.m_bIsDirty?UNSAVED_IMG_INDEX:SAVED_IMG_INDEX;
                            if (doc.m_bIsReadonly)
                                tie.iImage = REDONLY_IMG_INDEX;
                            ::SendMessage(m_TabBar, TCM_SETITEM, tab, reinterpret_cast<LPARAM>(&tie));
                        }
                        g_pFramework->InvalidateUICommand(cmdSave, UI_INVALIDATIONS_STATE, NULL);
                        g_pFramework->InvalidateUICommand(cmdSaveAll, UI_INVALIDATIONS_STATE, NULL);
                    }
                    break;
                case SCN_MARGINCLICK:
                    {
                        m_scintilla.MarginClick(pScn);
                    }
                    break;
                case SCN_UPDATEUI:
                    {
                        m_scintilla.MarkSelectedWord();
                        UpdateStatusBar();
                        g_pFramework->InvalidateUICommand(cmdCut, UI_INVALIDATIONS_STATE, NULL);
                        g_pFramework->InvalidateUICommand(cmdPaste, UI_INVALIDATIONS_STATE, NULL);
                    }
                    break;
                case SCN_MODIFIED:
                    {
                        g_pFramework->InvalidateUICommand(cmdUndo, UI_INVALIDATIONS_STATE, NULL);
                        g_pFramework->InvalidateUICommand(cmdRedo, UI_INVALIDATIONS_STATE, NULL);
                    }
                    break;
                }

            }
        }
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    case WM_CLOSE:
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
        {
            ::PostQuitMessage(0);
        }
        break;
    case cmdNew:
        {
            static int newCount = 0;
            newCount++;
            CDocument doc;
            doc.m_document = m_scintilla.Call(SCI_CREATEDOCUMENT);
            m_DocManager.AddDocumentAtEnd(doc);
            ResString newRes(hInst, IDS_NEW_TABTITLE);
            std::wstring s = CStringUtils::Format(newRes, newCount);
            int index = m_TabBar.InsertAtEnd(s.c_str());
            m_TabBar.ActivateAt(index);
        }
        break;
    case cmdOpen:
        {
            std::vector<std::wstring> paths;
            CComPtr<IFileOpenDialog> pfd = NULL;

            HRESULT hr = pfd.CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_INPROC_SERVER);
            if (SUCCEEDED(hr))
            {
                // Set the dialog options
                DWORD dwOptions;
                if (SUCCEEDED(hr = pfd->GetOptions(&dwOptions)))
                {
                    hr = pfd->SetOptions(dwOptions | FOS_FILEMUSTEXIST | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST | FOS_ALLOWMULTISELECT);
                }

                // Set a title
                if (SUCCEEDED(hr))
                {
                    pfd->SetTitle(L"BowPad");
                }

                // Show the save/open file dialog
                if (SUCCEEDED(hr) && SUCCEEDED(hr = pfd->Show(*this)))
                {
                    CComPtr<IShellItemArray> psiaResults;
                    hr = pfd->GetResults(&psiaResults);
                    if (SUCCEEDED(hr))
                    {
                        DWORD count = 0;
                        hr = psiaResults->GetCount(&count);
                        for (DWORD i = 0; i < count; ++i)
                        {
                            CComPtr<IShellItem> psiResult = NULL;
                            hr = psiaResults->GetItemAt(i, &psiResult);
                            PWSTR pszPath = NULL;
                            hr = psiResult->GetDisplayName(SIGDN_FILESYSPATH, &pszPath);
                            if (SUCCEEDED(hr))
                            {
                                paths.push_back(pszPath);
                            }
                        }
                    }
                }
            }
            if (SUCCEEDED(hr))
                OpenFiles(paths);
        }
        break;
    case cmdSave:
        SaveCurrentTab();
        g_pFramework->InvalidateUICommand(cmdSave, UI_INVALIDATIONS_STATE, NULL);
        g_pFramework->InvalidateUICommand(cmdSaveAll, UI_INVALIDATIONS_STATE, NULL);
        break;
    case cmdSaveAll:
        {
            for (int i = 0; i < (int)m_DocManager.GetCount(); ++i)
            {
                if (m_DocManager.GetDocument(i).m_bIsDirty)
                {
                    m_TabBar.ActivateAt(i);
                    SaveCurrentTab();
                }
            }
            g_pFramework->InvalidateUICommand(cmdSave, UI_INVALIDATIONS_STATE, NULL);
            g_pFramework->InvalidateUICommand(cmdSaveAll, UI_INVALIDATIONS_STATE, NULL);
        }
        break;
    case cmdCopy:
        m_scintilla.Call(SCI_COPYALLOWLINE);
        break;
    case cmdCut:
        m_scintilla.Call(SCI_CUT);
        break;
    case cmdPaste:
        m_scintilla.Call(SCI_PASTE);
        break;
    case cmdUndo:
        m_scintilla.Call(SCI_UNDO);
        break;
    case cmdRedo:
        m_scintilla.Call(SCI_REDO);
    case cmdLineWrap:
        m_scintilla.Call(SCI_SETWRAPMODE, m_scintilla.Call(SCI_GETWRAPMODE) ? 0 : SC_WRAP_WORD);
        g_pFramework->InvalidateUICommand(cmdLineWrap, UI_INVALIDATIONS_STATE, NULL);
        break;
    default:
        break;
    }
    return 1;
}

bool CMainWindow::Initialize()
{
    m_scintilla.Init(hResource, *this);
    m_StatusBar.Init(hResource, *this, 5);
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
    hr = g_pFramework->LoadUI(GetModuleHandle(NULL), L"BOWPAD_RIBBON");
    if (FAILED(hr))
    {
        return false;
    }


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

    return true;
}

void CMainWindow::ResizeChildWindows()
{
    RECT rect;
    GetClientRect(*this, &rect);
    HDWP hDwp = BeginDeferWindowPos(3);
    DeferWindowPos(hDwp, m_StatusBar, NULL, rect.left, rect.bottom-m_StatusBar.GetHeight(), rect.right-rect.left, m_StatusBar.GetHeight(), SWP_NOACTIVATE|SWP_NOOWNERZORDER|SWP_NOZORDER|SWP_SHOWWINDOW|SWP_NOCOPYBITS);
    DeferWindowPos(hDwp, m_TabBar, NULL, rect.left, rect.top+m_RibbonHeight, rect.right-rect.left, rect.bottom-rect.top, SWP_NOACTIVATE|SWP_NOOWNERZORDER|SWP_NOZORDER|SWP_SHOWWINDOW|SWP_NOCOPYBITS);
    RECT tabrc;
    TabCtrl_GetItemRect(m_TabBar, 0, &tabrc);
    MapWindowPoints(m_TabBar, *this, (LPPOINT)&tabrc, 2);
    DeferWindowPos(hDwp, m_scintilla, NULL, rect.left, rect.top+m_RibbonHeight+tabrc.bottom-tabrc.top, rect.right-rect.left, rect.bottom-(rect.top-rect.top+m_RibbonHeight+tabrc.bottom-tabrc.top)-m_StatusBar.GetHeight(), SWP_NOACTIVATE|SWP_NOOWNERZORDER|SWP_NOZORDER|SWP_SHOWWINDOW|SWP_NOCOPYBITS);
    EndDeferWindowPos(hDwp);
}

bool CMainWindow::OpenFiles( const std::vector<std::wstring>& files )
{
    bool bRet = true;
    for (auto file: files)
    {
        int encoding = -1;
        size_t index = m_DocManager.GetIndexForPath(file);
        if (index != -1)
        {
            // document already open
            m_TabBar.ActivateAt((int)index);
        }
        else
        {
            CDocument doc = m_DocManager.LoadFile(*this, file, encoding);
            if (doc.m_document)
            {
                CMRU::Instance().AddPath(file);
                m_scintilla.Call(SCI_SETDOCPOINTER, 0, doc.m_document);
                doc.m_language = CLexStyles::Instance().GetLanguageForExt(file.substr(file.find_last_of('.')+1));
                m_scintilla.SetupLexerForExt(file.substr(file.find_last_of('.')+1));
                m_DocManager.AddDocumentAtEnd(doc);
                std::wstring sFileName = file.substr(file.find_last_of('\\')+1);
                int index = m_TabBar.InsertAtEnd(sFileName.c_str());
                m_TabBar.ActivateAt(index);
            }
            else
            {
                CMRU::Instance().RemovePath(file, false);
                bRet = false;
            }
        }
    }
    return bRet;
}

BOOL CMainWindow::GetStatus( int cmdId )
{
    switch (cmdId)
    {
    case cmdSave:
        {
            int tab = m_TabBar.GetCurrentTabIndex();
            if ((tab >= 0) && (tab < m_DocManager.GetCount()))
            {
                CDocument doc = m_DocManager.GetDocument(tab);
                return doc.m_bIsDirty;
            }
        }
        break;
    case cmdSaveAll:
        {
            int dirtycount = 0;
            for (int i = 0; i < m_DocManager.GetCount(); ++i)
            {
                CDocument doc = m_DocManager.GetDocument(i);
                if (doc.m_bIsDirty)
                    dirtycount++;
            }
            return dirtycount > 1;
        }
        break;
    case cmdCut:
        return (m_scintilla.Call(SCI_GETSELTEXT)>1);
        break;
    case cmdPaste:
        return (m_scintilla.Call(SCI_CANPASTE) != 0);
        break;
    case cmdUndo:
        return (m_scintilla.Call(SCI_CANUNDO) != 0);
        break;
    case cmdRedo:
        return (m_scintilla.Call(SCI_CANREDO) != 0);
        break;
    }
    return TRUE;
}

BOOL CMainWindow::GetState( int cmdId )
{
    switch (cmdId)
    {
    case cmdLineWrap:
        return (m_scintilla.Call(SCI_GETWRAPMODE) > 0);
        break;
    }
    return TRUE;
}

bool CMainWindow::SaveCurrentTab()
{
    bool bRet = false;
    int tab = m_TabBar.GetCurrentTabIndex();
    if ((tab >= 0) && (tab < m_DocManager.GetCount()))
    {
        CDocument doc = m_DocManager.GetDocument(tab);
        if (doc.m_path.empty())
        {
            CComPtr<IFileSaveDialog> pfd = NULL;

            HRESULT hr = pfd.CoCreateInstance(CLSID_FileSaveDialog, NULL, CLSCTX_INPROC_SERVER);
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
                    ResString sTitle(hInst, IDS_APP_TITLE);
                    std::wstring s = (const wchar_t*)sTitle;
                    s += L" - ";
                    wchar_t buf[100] = {0};
                    m_TabBar.GetCurrentTitle(buf, _countof(buf));
                    s += buf;
                    pfd->SetTitle(s.c_str());
                }

                // Show the save/open file dialog
                if (SUCCEEDED(hr) && SUCCEEDED(hr = pfd->Show(*this)))
                {
                    CComPtr<IShellItem> psiResult = NULL;
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
                    }
                }
            }
        }
        if (!doc.m_path.empty())
            bRet = m_DocManager.SaveFile(*this, doc);
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

void CMainWindow::UpdateStatusBar()
{
    TCHAR strLnCol[128] = {0};
    TCHAR strSel[64] = {0};
    size_t selByte = 0;
    size_t selLine = 0;

    if (m_scintilla.GetSelectedCount(selByte, selLine))
        wsprintf(strSel, L"Sel : %d | %d", selByte, selLine);
    else
        wsprintf(strSel, L"Sel : %s", L"N/A");

    wsprintf(strLnCol, L"Ln : %d    Col : %d    %s",
                       (m_scintilla.Call(SCI_LINEFROMPOSITION, m_scintilla.Call(SCI_GETCURRENTPOS)) + 1),
                       (m_scintilla.Call(SCI_GETCOLUMN, m_scintilla.Call(SCI_GETCURRENTPOS)) + 1),
                       strSel);

    m_StatusBar.SetText(strLnCol, STATUSBAR_CUR_POS);

    TCHAR strDocLen[256];
    wsprintf(strDocLen, L"length : %d    lines : %d", m_scintilla.Call(SCI_GETLENGTH), m_scintilla.Call(SCI_GETLINECOUNT));
    m_StatusBar.SetText(strDocLen, STATUSBAR_DOC_SIZE);
    m_StatusBar.SetText(m_scintilla.Call(SCI_GETOVERTYPE) ? L"OVR" : L"INS", STATUSBAR_TYPING_MODE);
}
