#include "stdafx.h"
#include "MainWindow.h"
#include "RibbonNotepad.h"
#include "RibbonNotepadUI.h"
#include "StringUtils.h"
#include "MRU.h"

#include <memory>
#include <Shobjidl.h>

IUIFramework *g_pFramework = NULL;  // Reference to the Ribbon framework.

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
    UNREFERENCED_PARAMETER(ppropvarNewValue);

    HRESULT hr = E_NOTIMPL;
    //if(UI_PKEY_Enabled == key)
    //{
    //    return UIInitPropertyFromBoolean(UI_PKEY_Enabled, GetStatus(nCmdID), newValue);
    //}

    switch(nCmdID)
    {
    case cmdMRUList:
        if (UI_PKEY_RecentItems == key)
        {
            hr = CMRU::Instance().PopulateRibbonRecentItems(ppropvarNewValue);
        }
        break;
    default:
        //if (UI_PKEY_BooleanValue == key)
        //{
        //    hr = UIInitPropertyFromBoolean(UI_PKEY_BooleanValue, GetStatus(nCmdID) & OLECMDF_LATCHED, newValue);
        //}
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
    ResString clsName(hResource, IDC_RIBBONNOTEPAD);
    wcx.lpszClassName = clsName;
    wcx.hIcon = LoadIcon(hResource, MAKEINTRESOURCE(IDI_SMALL));
    wcx.hbrBackground = (HBRUSH)(COLOR_3DFACE+1);
    wcx.lpszMenuName = NULL;
    wcx.hIconSm = LoadIcon(wcx.hInstance, MAKEINTRESOURCE(IDI_SMALL));
    if (RegisterWindow(&wcx))
    {
        if (Create(WS_CAPTION | WS_MAXIMIZEBOX | WS_MINIMIZEBOX | WS_SIZEBOX | WS_SYSMENU | WS_CLIPCHILDREN, NULL))
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
                        return RGB(0, 255, 0);
                    break; 
                case TCN_SELCHANGE:
                    {
                        // document got activated
                        int tab = m_TabBar.GetCurrentTabIndex();
                        if (tab < m_DocManager.GetCount())
                        {
                            CDocument doc = m_DocManager.GetDocument(tab);
                            m_scintilla.Call(SCI_SETDOCPOINTER, 0, doc.m_document);
                            m_scintilla.RestoreCurrentPos(doc.m_position);
                            SetFocus(m_scintilla);
                            m_scintilla.Call(SCI_GRABFOCUS);
                        }
                    }
                    break;
                case TCN_SELCHANGING:
                    {
                        // document is about to be deactivated
                        int tab = m_TabBar.GetCurrentTabIndex();
                        if (tab < m_DocManager.GetCount())
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
                        if (tab == ptbhdr->tabOrigin)
                        {
                            if (tab > 0)
                                m_TabBar.ActivateAt(tab-1);
                        }
                        m_DocManager.RemoveDocument(ptbhdr->tabOrigin);
                        m_TabBar.DeletItemAt(ptbhdr->tabOrigin);
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
                    pfd->SetTitle(L"RibbonNotepad");
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
        {
            int tab = m_TabBar.GetCurrentTabIndex();
            if (tab < m_DocManager.GetCount())
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
                            pfd->SetTitle(L"RibbonNotepad");
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
                                }
                            }
                        }
                    }
                }
                m_DocManager.SaveFile(*this, doc);
            }
        }
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
    hr = g_pFramework->LoadUI(GetModuleHandle(NULL), L"RIBBONNOTEPAD_RIBBON");
    if (FAILED(hr))
    {
        return false;
    }

    return true;
}

void CMainWindow::ResizeChildWindows()
{
    RECT rect;
    GetClientRect(*this, &rect);
    HDWP hDwp = BeginDeferWindowPos(3);
    DeferWindowPos(hDwp, m_StatusBar, NULL, rect.left, rect.bottom-m_StatusBar.GetHeight(), rect.right-rect.left, m_StatusBar.GetHeight(), SWP_NOACTIVATE|SWP_NOOWNERZORDER|SWP_NOZORDER|SWP_SHOWWINDOW);
    DeferWindowPos(hDwp, m_TabBar, NULL, rect.left, rect.top+m_RibbonHeight, rect.right-rect.left, rect.bottom-rect.top, SWP_NOACTIVATE|SWP_NOOWNERZORDER|SWP_NOZORDER|SWP_SHOWWINDOW);
    RECT tabrc;
    TabCtrl_GetItemRect(m_TabBar, 0, &tabrc);
    MapWindowPoints(m_TabBar, *this, (LPPOINT)&tabrc, 2);
    DeferWindowPos(hDwp, m_scintilla, NULL, rect.left, tabrc.bottom, rect.right-rect.left, rect.bottom-rect.top-tabrc.bottom-m_StatusBar.GetHeight(), SWP_NOACTIVATE|SWP_NOOWNERZORDER|SWP_NOZORDER|SWP_SHOWWINDOW);
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

