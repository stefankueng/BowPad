// This file is part of BowPad.
//
// Copyright (C) 2014-2015 - Stefan Kueng
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
#include "BowPadUI.h"
#include "FileTree.h"
#include "Theme.h"
#include "SysImageList.h"
#include "DirFileEnum.h"
#include "PathUtils.h"
#include "OnOutOfScope.h"

#include <thread>
#include <vector>
#include <VersionHelpers.h>
#include <Uxtheme.h>

#pragma comment(lib, "Uxtheme.lib")

#define SCRATCH_QCM_FIRST 1
#define SCRATCH_QCM_LAST  0x6FFF

IContextMenu2 *g_pcm2 = nullptr;
IContextMenu3 *g_pcm3 = nullptr;

HRESULT GetUIObjectOfFile(HWND hwnd, LPCWSTR pszPath, REFIID riid, void **ppv)
{
    *ppv = NULL;
    HRESULT hr = E_FAIL;
    LPITEMIDLIST pidl;
    SFGAOF sfgao;
    if (SUCCEEDED(hr = SHParseDisplayName(pszPath, NULL, &pidl, 0, &sfgao)))
    {
        IShellFolder *psf;
        LPCITEMIDLIST pidlChild;
        if (SUCCEEDED(hr = SHBindToParent(pidl, IID_IShellFolder,
            (void**)&psf, &pidlChild)))
        {
            hr = psf->GetUIObjectOf(hwnd, 1, &pidlChild, riid, NULL, ppv);
            psf->Release();
        }
        CoTaskMemFree(pidl);
    }
    return hr;
}

class FileTreeItem
{
public:
    FileTreeItem()
        : isDir(false)
        , busy(false)
    {}
    std::wstring    path;
    bool            isDir;
    bool            busy;
};

class FileTreeData
{
public:
    FileTreeData() {}

    std::wstring                refreshpath;
    HTREEITEM                   refreshRoot;
    std::vector<FileTreeItem*>  data;
};

int CALLBACK TreeCompareFunc(LPARAM lParam1, LPARAM lParam2, LPARAM /*lParamSort*/)
{
    FileTreeItem * pTreeItem1 = reinterpret_cast<FileTreeItem*>(lParam1);
    FileTreeItem * pTreeItem2 = reinterpret_cast<FileTreeItem*>(lParam2);

    if (pTreeItem1->isDir != pTreeItem2->isDir)
        return pTreeItem1->isDir ? -1 : 1;

    auto res = CompareStringEx(nullptr, LINGUISTIC_IGNORECASE | SORT_DIGITSASNUMBERS | SORT_STRINGSORT,
                               pTreeItem1->path.c_str(), (int)pTreeItem1->path.length(),
                               pTreeItem2->path.c_str(), (int)pTreeItem2->path.length(),
                               nullptr, nullptr, 0);
    return res - 2;
}

CFileTree::~CFileTree()
{
    RecurseTree(TVI_ROOT, [&](HTREEITEM hItem)->bool
    {
        TVITEM item = { 0 };
        item.mask = TVIF_PARAM;
        item.hItem = hItem;
        TreeView_GetItem(*this, &item);
        FileTreeItem * pTreeItem = reinterpret_cast<FileTreeItem*>(item.lParam);
        if (pTreeItem)
        {
            delete pTreeItem;
        }
        return false;
    });
    TreeView_DeleteAllItems(*this);
}

bool CFileTree::Init(HINSTANCE /*hInst*/, HWND hParent)
{
    INITCOMMONCONTROLSEX icce;
    icce.dwSize = sizeof(icce);
    icce.dwICC = ICC_TREEVIEW_CLASSES;
    InitCommonControlsEx(&icce);
    CreateEx(0, WS_VISIBLE | WS_CHILD | WS_TABSTOP | TVS_HASBUTTONS | TVS_HASLINES | TVS_LINESATROOT | TVS_DISABLEDRAGDROP | TVS_SHOWSELALWAYS | TVS_NOHSCROLL, hParent, 0, WC_TREEVIEW);
    if (!*this)
        return false;
    TreeView_SetExtendedStyle(*this, TVS_EX_AUTOHSCROLL | TVS_EX_DOUBLEBUFFER, TVS_EX_AUTOHSCROLL | TVS_EX_DOUBLEBUFFER);
    TreeView_SetImageList(*this, CSysImageList::GetInstance(), TVSIL_NORMAL);

    OnThemeChanged(CTheme::Instance().IsDarkTheme());

    return true;
}

LRESULT CALLBACK CFileTree::WinMsgHandler(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    if (g_pcm3)
    {
        LRESULT lres;
        if (SUCCEEDED(g_pcm3->HandleMenuMsg2(uMsg, wParam, lParam, &lres)))
        {
            return lres;
        }
    }
    else if (g_pcm2)
    {
        if (SUCCEEDED(g_pcm2->HandleMenuMsg(uMsg, wParam, lParam)))
        {
            return 0;
        }
    }

    switch (uMsg)
    {
        case WM_SHOWWINDOW:
            if (wParam)
                Refresh(TVI_ROOT, true);
            break;
        case WM_ERASEBKGND:
        {
            if (CTheme::Instance().IsDarkTheme())
            {
                HDC hDC = (HDC)wParam;
                ::SetBkColor(hDC, CTheme::Instance().GetThemeColor(RGB(255, 255, 255)));
                RECT rect;
                GetClientRect(*this, &rect);
                ::ExtTextOut(hDC, 0, 0, ETO_OPAQUE, &rect, NULL, 0, NULL);
                return TRUE;
            }
        }
        break;
        case WM_CONTEXTMENU:
        {
            HTREEITEM hSelItem = TreeView_GetSelection(*this);
            POINT pt;
            POINTSTOPOINT(pt, lParam);

            if (pt.x == -1 && pt.y == -1)
            {
                hSelItem = TreeView_GetSelection(*this);
                RECT rc;
                TreeView_GetItemRect(*this, hSelItem, &rc, TRUE);
                pt.x = rc.left;
                pt.y = rc.top;
                ClientToScreen(*this, &pt);
            }
            else
            {
                ScreenToClient(*this, &pt);

                TV_HITTESTINFO tvhti;
                tvhti.pt = pt;
                HTREEITEM hItem = TreeView_HitTest(*this, &tvhti);
                if ((tvhti.flags & TVHT_ONITEM) != 0)
                {
                    hSelItem = hItem;
                }
                ClientToScreen(*this, &pt);
            }
            if (hSelItem == 0)
                break;
            TVITEM item = { 0 };
            item.mask = TVIF_PARAM;
            item.hItem = hSelItem;
            TreeView_GetItem(*this, &item);
            FileTreeItem * pTreeItem = reinterpret_cast<FileTreeItem*>(item.lParam);

            IContextMenu *pcm;
            HTREEITEM hRefresh = NULL;
            if (SUCCEEDED(GetUIObjectOfFile(hwnd, pTreeItem->path.c_str(),
                IID_IContextMenu, (void**)&pcm)))
            {
                HMENU hmenu = CreatePopupMenu();
                if (hmenu)
                {
                    if (SUCCEEDED(pcm->QueryContextMenu(hmenu, 1,
                        SCRATCH_QCM_FIRST, SCRATCH_QCM_LAST,
                        CMF_NORMAL)))
                    {
                        pcm->QueryInterface(IID_IContextMenu2, (void**)&g_pcm2);
                        pcm->QueryInterface(IID_IContextMenu3, (void**)&g_pcm3);
                        int iCmd = TrackPopupMenuEx(hmenu, TPM_RETURNCMD,
                                                    pt.x, pt.y, hwnd, NULL);
                        if (g_pcm2)
                        {
                            g_pcm2->Release();
                            g_pcm2 = NULL;
                        }
                        if (g_pcm3)
                        {
                            g_pcm3->Release();
                            g_pcm3 = NULL;
                        }
                        if (iCmd > 0)
                        {
                            CMINVOKECOMMANDINFOEX info = { 0 };
                            info.cbSize = sizeof(info);
                            info.fMask = CMIC_MASK_UNICODE | CMIC_MASK_PTINVOKE;
                            if (GetKeyState(VK_CONTROL) < 0)
                            {
                                info.fMask |= CMIC_MASK_CONTROL_DOWN;
                            }
                            if (GetKeyState(VK_SHIFT) < 0)
                            {
                                info.fMask |= CMIC_MASK_SHIFT_DOWN;
                            }
                            info.hwnd = hwnd;
                            info.lpVerb = MAKEINTRESOURCEA(iCmd - SCRATCH_QCM_FIRST);
                            info.lpVerbW = MAKEINTRESOURCEW(iCmd - SCRATCH_QCM_FIRST);
                            info.nShow = SW_SHOWNORMAL;
                            info.ptInvoke = pt;
                            pcm->InvokeCommand((LPCMINVOKECOMMANDINFO)&info);
                            hRefresh = TreeView_GetParent(*this, hSelItem);
                            if (hRefresh == NULL)
                                hRefresh = TVI_ROOT;
                        }
                    }
                    DestroyMenu(hmenu);
                }
                pcm->Release();
                if (hRefresh)
                    Refresh(hRefresh);
            }
        }
        break;
        case WM_THREADRESULTREADY:
        {
            if (m_bStop)
            {
                m_bRootBusy = false;
                break;
            }

            std::wstring activepath;
            if (HasActiveDocument())
            {
                auto doc = GetActiveDocument();
                if (!doc.m_path.empty() && (m_path.size() < doc.m_path.size()))
                {
                    if (CPathUtils::PathCompare(m_path, doc.m_path.substr(0, m_path.size())) == 0)
                    {
                        activepath = doc.m_path;
                    }
                }
            }

            FileTreeData * pData = (FileTreeData*)lParam;
            if (pData)
            {
                SendMessage(*this, WM_SETREDRAW, FALSE, 0);
                OnOutOfScope(SendMessage(*this, WM_SETREDRAW, TRUE, 0));

                // check if the refresh root is still there
                FileTreeItem * fi = nullptr;
                if (pData->refreshRoot != TVI_ROOT)
                {
                    TVITEMEX tvex = { 0 };
                    tvex.mask = TVIF_PARAM;
                    tvex.hItem = pData->refreshRoot;
                    TreeView_GetItem(*this, &tvex);
                    fi = (FileTreeItem*)tvex.lParam;
                }
                bool activepathmarked = false;
                if (((pData->refreshRoot == TVI_ROOT) && (pData->refreshpath.compare(m_path) == 0)) ||
                    (fi && (fi->path.compare(pData->refreshpath) == 0)))
                {
                    wchar_t textbuf[MAX_PATH];
                    for (const auto& item : pData->data)
                    {
                        TVITEMEX tvi = { 0 };
                        TVINSERTSTRUCT tvins = { 0 };

                        tvi.mask = TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_PARAM | TVIF_CHILDREN;

                        wcscpy_s(textbuf, CPathUtils::GetFileName(item->path).c_str());
                        tvi.pszText = textbuf;
                        tvi.cchTextMax = _countof(textbuf);
                        tvi.cChildren = 0;

                        if (item->isDir)
                        {
                            tvi.mask |= TVIF_EXPANDEDIMAGE;
                            tvi.iImage = CSysImageList::GetInstance().GetDirIconIndex();
                            tvi.iSelectedImage = CSysImageList::GetInstance().GetDirIconIndex();
                            tvi.iExpandedImage = CSysImageList::GetInstance().GetDirOpenIconIndex();
                            tvi.cChildren = 1;
                        }
                        else
                        {
                            tvi.iImage = CSysImageList::GetInstance().GetFileIconIndex(item->path);
                            tvi.iSelectedImage = CSysImageList::GetInstance().GetFileIconIndex(item->path);
                        }
                        if (!activepath.empty())
                        {
                            if (CPathUtils::PathCompare(item->path, activepath) == 0)
                            {
                                tvi.mask |= TVIF_STATE;
                                tvi.state = TVIS_BOLD;
                                tvi.stateMask = TVIS_BOLD;
                                activepathmarked = true;
                            }
                        }
                        tvi.lParam = (LPARAM)item;
                        tvins.itemex = tvi;
                        tvins.hInsertAfter = TVI_LAST;
                        tvins.hParent = pData->refreshRoot;

                        TreeView_InsertItem(*this, &tvins);
                    }

                    TVSORTCB sortcb = { 0 };
                    sortcb.hParent = pData->refreshRoot;
                    sortcb.lParam = (LPARAM)this;
                    sortcb.lpfnCompare = TreeCompareFunc;
                    TreeView_SortChildrenCB(*this, &sortcb, false);
                    TreeView_Expand(*this, pData->refreshRoot, TVE_EXPAND);
                    TreeView_SetItemState(*this, pData->refreshRoot, 0, TVIS_CUT);

                    if (fi)
                        fi->busy = false;
                    m_bRootBusy = false;


                    delete pData;

                    if (!activepath.empty() && !activepathmarked && !m_path.empty())
                    {
                        // the current path does not appear to be visible, so
                        // refresh all sub-paths down to the active path
                        std::wstring expandpath = CPathUtils::GetParentDirectory(activepath);
                        HTREEITEM hItem = GetItemForPath(expandpath);
                        if (hItem == NULL)
                        {
                            while ((hItem == NULL) && (expandpath.size() > m_path.size()))
                            {
                                expandpath = CPathUtils::GetParentDirectory(expandpath);
                                hItem = GetItemForPath(expandpath);
                            }
                            if (hItem)
                                Refresh(hItem);
                        }
                    }
                }
                else
                {
                    m_bRootBusy = false;
                    // the refresh root is not valid anymore (e.g., the tab
                    // was changed while the thread was running), so delete
                    // the data the thread provided
                    for (const auto& item : pData->data)
                    {
                        delete item;
                    }
                    delete pData;
                }
            }
        }
        break;
        default:
            break;
    }
    if (prevWndProc)
        return prevWndProc(hwnd, uMsg, wParam, lParam);
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

HTREEITEM CFileTree::RecurseTree(HTREEITEM hItem, ItemHandler handler)
{
    HTREEITEM hFound = hItem;
    while (hFound)
    {
        if (hFound == TVI_ROOT)
            hFound = TreeView_GetNextItem(*this, TVI_ROOT, TVGN_ROOT);
        HTREEITEM hNext = TreeView_GetNextItem(*this, hFound, TVGN_NEXT);
        if (hFound && handler(hFound))
            return hFound;
        HTREEITEM hChild = TreeView_GetChild(*this, hFound);
        if (hChild)
        {
            hChild = RecurseTree(hChild, handler);
            if (hChild)
                return hChild;
        }
        hFound = hNext;
    }
    return NULL;
}

void CFileTree::Refresh(HTREEITEM refreshRoot, bool force /*= false*/)
{
    InvalidateUICommand(cmdFileTree, UI_INVALIDATIONS_PROPERTY, &UI_PKEY_Enabled);
    InvalidateUICommand(cmdFileTree, UI_INVALIDATIONS_PROPERTY, &UI_PKEY_BooleanValue);

    if (!force && !IsWindowVisible(*this))
        return;
    if (m_nBlockRefresh)
        return;

    if (m_bRootBusy)
        return;

    FileTreeItem * fi = nullptr;
    if (refreshRoot != TVI_ROOT)
    {
        TVITEMEX tvex = { 0 };
        tvex.mask = TVIF_PARAM;
        tvex.hItem = refreshRoot;
        TreeView_GetItem(*this, &tvex);
        fi = (FileTreeItem*)tvex.lParam;
    }
    if (fi)
    {
        if (fi->busy)
            return;
        fi->busy = true;
    }
    else
        m_bRootBusy = true;

    TreeView_SetItemState(*this, refreshRoot, TVIS_CUT, TVIS_CUT);

    SendMessage(*this, WM_SETREDRAW, FALSE, 0);
    OnOutOfScope(SendMessage(*this, WM_SETREDRAW, TRUE, 0));

    RecurseTree(TreeView_GetChild(*this, refreshRoot), [&](HTREEITEM hItem)->bool
    {
        TVITEM item = { 0 };
        item.mask = TVIF_PARAM;
        item.hItem = hItem;
        TreeView_GetItem(*this, &item);
        FileTreeItem * pTreeItem = reinterpret_cast<FileTreeItem*>(item.lParam);
        if (pTreeItem)
        {
            delete pTreeItem;
            TreeView_DeleteItem(*this, hItem);
        }
        return false;
    });

    std::wstring refreshPath = m_path;
    if (refreshRoot != TVI_ROOT)
    {
        TVITEM item = { 0 };
        item.mask = TVIF_PARAM;
        item.hItem = refreshRoot;
        TreeView_GetItem(*this, &item);
        FileTreeItem * pTreeItem = reinterpret_cast<FileTreeItem*>(item.lParam);
        if (pTreeItem && pTreeItem->isDir)
        {
            refreshPath = pTreeItem->path;
        }
        else
        {
            m_bRootBusy = false;
            return;
        }
    }
    if (refreshPath.empty())
    {
        m_bRootBusy = false;
        return;
    }


    InterlockedIncrement(&m_ThreadsRunning);

    std::thread t(&CFileTree::RefreshThread, this, refreshRoot, refreshPath);
    t.detach();
}

void CFileTree::RefreshThread(HTREEITEM refreshRoot, const std::wstring& refreshPath)
{
    OnOutOfScope(InterlockedDecrement(&m_ThreadsRunning););

    auto data = new FileTreeData();
    data->refreshpath = refreshPath;
    data->refreshRoot = refreshRoot;
    CDirFileEnum enumerator(refreshPath);
    bool bIsDir = false;
    std::wstring path;
    while (enumerator.NextFile(path, &bIsDir, false) && !m_bStop)
    {
        FileTreeItem * fi = new FileTreeItem();
        fi->path = path;
        fi->isDir = bIsDir;

        data->data.push_back(fi);
    }
    if (m_bStop)
    {
        for (const auto& i : data->data)
            delete i;
        delete data;
        m_bRootBusy = false;
        return;
    }
    PostMessage(*this, WM_THREADRESULTREADY, 0, (LPARAM)data);
}

HTREEITEM CFileTree::GetHitItem()
{
    DWORD mpos = GetMessagePos();
    POINT pt;
    pt.x = GET_X_LPARAM(mpos);
    pt.y = GET_Y_LPARAM(mpos);
    ScreenToClient(*this, &pt);
    TV_HITTESTINFO tvhti;
    tvhti.pt = pt;
    HTREEITEM hItem = TreeView_HitTest(*this, &tvhti);
    if ((tvhti.flags & TVHT_ONITEM) == 0)
        return NULL;
    return hItem;
}

std::wstring CFileTree::GetFilePathForHitItem()
{
    HTREEITEM hItem = GetHitItem();
    if (hItem)
    {
        TVITEM item = { 0 };
        item.mask = TVIF_PARAM;
        item.hItem = hItem;
        TreeView_GetItem(*this, &item);
        FileTreeItem * pTreeItem = reinterpret_cast<FileTreeItem*>(item.lParam);
        if (pTreeItem && !pTreeItem->isDir)
        {
            return pTreeItem->path;
        }
    }
    return std::wstring();
}

std::wstring CFileTree::GetFilePathForSelItem()
{
    HTREEITEM hItem = TreeView_GetSelection(*this);
    if (hItem)
    {
        TVITEM item = { 0 };
        item.mask = TVIF_PARAM;
        item.hItem = hItem;
        TreeView_GetItem(*this, &item);
        FileTreeItem * pTreeItem = reinterpret_cast<FileTreeItem*>(item.lParam);
        if (pTreeItem && !pTreeItem->isDir)
        {
            return pTreeItem->path;
        }
    }
    return std::wstring();
}

void CFileTree::OnThemeChanged(bool bDark)
{
    if (bDark)
    {
        // When in dark theme mode, we have to disable double buffering:
        // if double buffering is enabled, the background of the tree
        // control is always shown in white, even though we draw it black
        // in the WM_ERASEBACKGROUND handler. Haven't found a way to work around this
        TreeView_SetExtendedStyle(*this, 0, TVS_EX_DOUBLEBUFFER);
        if (!IsWindows8OrGreater())
        {
            // disable the explorer theme in dark mode on Win7:
            // the color of the selection is too bright and causes
            // the text to be unreadable
            // works much better on Win8 so we leave the explorer theme there
            SetWindowTheme(*this, NULL, NULL);
        }
        else
            SetWindowTheme(*this, L"Explorer", NULL);
    }
    else
    {
        TreeView_SetExtendedStyle(*this, TVS_EX_DOUBLEBUFFER, TVS_EX_DOUBLEBUFFER);
        SetWindowTheme(*this, L"Explorer", NULL);
    }
}

bool CFileTree::Execute()
{
    return false;
}

UINT CFileTree::GetCmdId()
{
    return cmdFileTreeControl;
}

void CFileTree::TabNotify(TBHDR * ptbhdr)
{
    if ((ptbhdr->hdr.code == TCN_SELCHANGE))
    {
        InvalidateUICommand(cmdFileTree, UI_INVALIDATIONS_PROPERTY, &UI_PKEY_Enabled);
        InvalidateUICommand(cmdFileTree, UI_INVALIDATIONS_PROPERTY, &UI_PKEY_BooleanValue);
        // remove the bold status from all items
        RecurseTree(TreeView_GetChild(*this, TVI_ROOT), [&](HTREEITEM hItem)->bool
        {
            TreeView_SetItemState(*this, hItem, 0, TVIS_BOLD);
            return false;
        });
        if (HasActiveDocument())
        {
            auto doc = GetActiveDocument();
            if (!doc.m_path.empty())
            {
                if (!CPathUtils::PathIsChild(m_path, doc.m_path))
                {
                    SetPath(CPathUtils::GetParentDirectory(doc.m_path));
                }
            }
            if (!doc.m_path.empty() && (m_path.size() < doc.m_path.size()))
            {
                if (CPathUtils::PathCompare(m_path, doc.m_path.substr(0, m_path.size())) == 0)
                {
                    // find the path
                    RecurseTree(TreeView_GetChild(*this, TVI_ROOT), [&](HTREEITEM hItem)->bool
                    {
                        TVITEM item = { 0 };
                        item.mask = TVIF_PARAM;
                        item.hItem = hItem;
                        TreeView_GetItem(*this, &item);
                        FileTreeItem * pTreeItem = reinterpret_cast<FileTreeItem*>(item.lParam);
                        if (pTreeItem)
                        {
                            if (!pTreeItem->isDir)
                            {
                                if (CPathUtils::PathCompare(doc.m_path, pTreeItem->path) == 0)
                                {
                                    TreeView_EnsureVisible(*this, hItem);
                                    TreeView_SetItemState(*this, hItem, TVIS_BOLD, TVIS_BOLD);
                                    return true;
                                }
                            }
                            else
                            {
                                if (CPathUtils::PathIsChild(pTreeItem->path, doc.m_path))
                                    TreeView_Expand(*this, hItem, TVE_EXPAND);
                            }
                        }
                        return false;
                    });
                }
            }
        }
    }
}

void CFileTree::BlockRefresh(bool bBlock)
{
    if (bBlock)
        ++m_nBlockRefresh;
    else
        --m_nBlockRefresh;
    if (m_nBlockRefresh < 0)
    {
        // this should not happen!
        assert(false);
        m_nBlockRefresh = 0;
    }
    if (m_nBlockRefresh == 0)
    {
        Refresh(TVI_ROOT, false);
    }
}

void CFileTree::OnClose()
{
    m_bStop = true;
    auto start = GetTickCount64();
    while (m_ThreadsRunning && (GetTickCount64() - start < 5000))
        Sleep(10);
}

HTREEITEM CFileTree::GetItemForPath(const std::wstring& expandpath)
{
    HTREEITEM hResult = NULL;
    RecurseTree(TreeView_GetChild(*this, TVI_ROOT), [&](HTREEITEM hItem)->bool
    {
        TVITEM item = { 0 };
        item.mask = TVIF_PARAM;
        item.hItem = hItem;
        TreeView_GetItem(*this, &item);
        FileTreeItem * pTreeItem = reinterpret_cast<FileTreeItem*>(item.lParam);
        if (pTreeItem)
        {
            if (pTreeItem->path.compare(expandpath) == 0)
            {
                hResult = hItem;
                return true;
            }
        }
        return false;
    });
    return hResult;
}


