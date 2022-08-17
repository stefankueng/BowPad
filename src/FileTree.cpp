// This file is part of BowPad.
//
// Copyright (C) 2014-2022 - Stefan Kueng
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
#include "DarkModeHelper.h"

#include <thread>
#include <vector>
#include <algorithm>
#include <VersionHelpers.h>
#include <Uxtheme.h>
#include <format>

#pragma comment(lib, "Uxtheme.lib")

constexpr int         SCRATCH_QCM_FIRST    = 1;
constexpr int         SCRATCH_QCM_LAST     = 0x6FFF;

static IContextMenu2* g_pcm2               = nullptr;
static IContextMenu3* g_pcm3               = nullptr;

constexpr wchar_t     thisOsPathSeparator  = L'\\';
constexpr wchar_t     otherOsPathSeparator = L'/';

inline bool           IsFolderSeparator(wchar_t c)
{
    return (c == thisOsPathSeparator || c == otherOsPathSeparator);
}

static HRESULT GetUIObjectOfFile(HWND hwnd, LPCWSTR pszPath, REFIID riid, void** ppv)
{
    *ppv              = nullptr;
    HRESULT      hr   = E_FAIL;
    LPITEMIDLIST pidl = nullptr;
    SFGAOF       sfgao{};
    if (SUCCEEDED(hr = SHParseDisplayName(pszPath, nullptr, &pidl, 0, &sfgao)))
    {
        IShellFolder* psf = nullptr;
        LPCITEMIDLIST pidlChild;
        if (SUCCEEDED(hr = SHBindToParent(pidl, IID_IShellFolder,
                                          reinterpret_cast<void**>(&psf), &pidlChild)))
        {
            hr = psf->GetUIObjectOf(hwnd, 1, &pidlChild, riid, nullptr, ppv);
            psf->Release();
        }
        CoTaskMemFree(pidl);
    }
    return hr;
}

static FileTreeItem* GetFileTreeItem(HWND hWnd, HTREEITEM hItem)
{
    TVITEM item = {};
    item.mask   = TVIF_PARAM;
    item.hItem  = hItem;
    if (TreeView_GetItem(hWnd, &item) != FALSE)
        return reinterpret_cast<FileTreeItem*>(item.lParam);
    return nullptr;
}

CFileTree::CFileTree(HINSTANCE hInst, void* obj)
    : CWindow(hInst)
    , ICommand(obj)
    , m_nBlockRefresh(0)
    , m_threadsRunning(0)
    , m_bStop(false)
    , m_bRootBusy(false)
    , m_activeItem(nullptr)
    , m_bBlockExpansion(false)
{
}

CFileTree::~CFileTree()
{
    Clear();
}

void CFileTree::Clear()
{
    m_activeItem = nullptr;
    m_pathWatcher.ClearPaths(true);
    for (auto& [handle, data] : m_data)
    {
        delete data;
    }
    SendMessage(*this, WM_SETREDRAW, FALSE, 0);
    OnOutOfScope(SendMessage(*this, WM_SETREDRAW, TRUE, 0));
    TreeView_DeleteAllItems(*this);
}

void CFileTree::SetPath(const std::wstring& path, bool forceRefresh)
{
    if (m_bRootBusy && (m_path != path))
    {
        m_bStop     = true;
        int timeout = 100;
        while (m_bRootBusy && timeout)
        {
            // if the root is busy but the thread has already finished,
            // we need to handle the WM_THREADRESULTREADY message to get
            // the busy flag cleared.
            MSG msg;
            while (PeekMessage(&msg, *this, WM_THREADRESULTREADY, WM_THREADRESULTREADY, PM_REMOVE))
            {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
            Sleep(20);
            --timeout;
        }
        m_bStop = false;
        m_path  = path;
        Refresh(TVI_ROOT);
    }
    else
    {
        if (forceRefresh || (m_path != path))
        {
            m_bBlockExpansion = false;
            m_path            = path;
            Refresh(TVI_ROOT);
        }
    }
}

bool CFileTree::Init(HINSTANCE /*hInst*/, HWND hParent)
{
    INITCOMMONCONTROLSEX icce = {};
    icce.dwSize               = sizeof(icce);
    icce.dwICC                = ICC_TREEVIEW_CLASSES;
    InitCommonControlsEx(&icce);
    CreateEx(0, WS_VISIBLE | WS_CHILD | WS_TABSTOP | TVS_HASBUTTONS | TVS_HASLINES | TVS_LINESATROOT | TVS_DISABLEDRAGDROP | TVS_SHOWSELALWAYS | TVS_TRACKSELECT, hParent, nullptr, WC_TREEVIEW);
    if (!*this)
        return false;
    TreeView_SetExtendedStyle(*this, TVS_EX_AUTOHSCROLL | TVS_EX_DOUBLEBUFFER, TVS_EX_AUTOHSCROLL | TVS_EX_DOUBLEBUFFER);
    TreeView_SetImageList(*this, CSysImageList::GetInstance(), TVSIL_NORMAL);

    OnThemeChanged(CTheme::Instance().IsDarkTheme());

    SetTimer(*this, 100, 1000, nullptr);

    return true;
}

LRESULT CALLBACK CFileTree::WinMsgHandler(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    if (g_pcm3)
    {
        LRESULT lRes;
        if (SUCCEEDED(g_pcm3->HandleMenuMsg2(uMsg, wParam, lParam, &lRes)))
        {
            return lRes;
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
        case WM_CONTEXTMENU:
        {
            HTREEITEM hSelItem = TreeView_GetSelection(*this);
            POINT     pt{};
            POINTSTOPOINT(pt, lParam);

            if (pt.x == -1 && pt.y == -1)
            {
                hSelItem = TreeView_GetSelection(*this);
                RECT rc{};
                TreeView_GetItemRect(*this, hSelItem, &rc, TRUE);
                pt.x = rc.left;
                pt.y = rc.top;
                ClientToScreen(*this, &pt);
            }
            else
            {
                ScreenToClient(*this, &pt);

                TV_HITTESTINFO tvHti{};
                tvHti.pt        = pt;
                HTREEITEM hItem = TreeView_HitTest(*this, &tvHti);
                if ((tvHti.flags & TVHT_ONITEM) != 0)
                {
                    hSelItem = hItem;
                }
                ClientToScreen(*this, &pt);
            }
            if (hSelItem == nullptr)
                break;
            const FileTreeItem* pTreeItem = GetFileTreeItem(*this, hSelItem);
            if (!pTreeItem)
            {
                APPVERIFY(false);
                break; // switch.
            }

            IContextMenuPtr pcm      = nullptr;
            HTREEITEM       hRefresh = nullptr;
            if (SUCCEEDED(GetUIObjectOfFile(hwnd, pTreeItem->path.c_str(),
                                            IID_IContextMenu, reinterpret_cast<void**>(&pcm))))
            {
                HMENU hmenu = CreatePopupMenu();
                if (hmenu)
                {
                    OnOutOfScope(
                        DestroyMenu(hmenu););
                    if (SUCCEEDED(pcm->QueryContextMenu(hmenu, 1,
                                                        SCRATCH_QCM_FIRST, SCRATCH_QCM_LAST,
                                                        CMF_NORMAL)))
                    {
                        pcm->QueryInterface(IID_IContextMenu2, reinterpret_cast<void**>(&g_pcm2));
                        pcm->QueryInterface(IID_IContextMenu3, reinterpret_cast<void**>(&g_pcm3));
                        int iCmd = TrackPopupMenuEx(hmenu, TPM_RETURNCMD,
                                                    pt.x, pt.y, hwnd, nullptr);
                        if (g_pcm2)
                        {
                            g_pcm2->Release();
                            g_pcm2 = nullptr;
                        }
                        if (g_pcm3)
                        {
                            g_pcm3->Release();
                            g_pcm3 = nullptr;
                        }
                        if (iCmd > 0)
                        {
                            CMINVOKECOMMANDINFOEX info = {0};
                            info.cbSize                = sizeof(info);
                            info.fMask                 = CMIC_MASK_UNICODE | CMIC_MASK_PTINVOKE;
                            if (GetKeyState(VK_CONTROL) < 0)
                            {
                                info.fMask |= CMIC_MASK_CONTROL_DOWN;
                            }
                            if (GetKeyState(VK_SHIFT) < 0)
                            {
                                info.fMask |= CMIC_MASK_SHIFT_DOWN;
                            }
                            info.hwnd     = hwnd;
                            info.lpVerb   = MAKEINTRESOURCEA(iCmd - SCRATCH_QCM_FIRST);
                            info.lpVerbW  = MAKEINTRESOURCEW(iCmd - SCRATCH_QCM_FIRST);
                            info.nShow    = SW_SHOWNORMAL;
                            info.ptInvoke = pt;
                            pcm->InvokeCommand(reinterpret_cast<LPCMINVOKECOMMANDINFO>(&info));
                            hRefresh = TreeView_GetParent(*this, hSelItem);
                            if (hRefresh == nullptr)
                                hRefresh = TVI_ROOT;
                        }
                    }
                }
                pcm.Release();
                if (hRefresh)
                    Refresh(hRefresh);
            }
        }
        break;
        case WM_THREADRESULTREADY:
        {
            FileTreeData* pData = reinterpret_cast<FileTreeData*>(lParam); // Will delete but not change.
            assert(pData != nullptr);                                      // not possible.

            if (m_bStop)
            {
                m_bRootBusy = false;
                delete pData;
                break; // switch
            }

            SendMessage(*this, WM_SETREDRAW, FALSE, 0);
            OnOutOfScope(SendMessage(*this, WM_SETREDRAW, TRUE, 0));

            // check if the refresh root is still there
            FileTreeItem* fi = nullptr;
            if (pData->refreshRoot != TVI_ROOT)
                fi = GetFileTreeItem(*this, pData->refreshRoot);

            if (((pData->refreshRoot == TVI_ROOT) && (CPathUtils::PathCompare(pData->refreshPath, m_path) == 0)) ||
                (fi && (CPathUtils::PathCompare(fi->path, pData->refreshPath) == 0)))
            {
                std::wstring activePath;
                if (HasActiveDocument())
                {
                    const auto& doc = GetActiveDocument();
                    if (!doc.m_path.empty() && (m_path.size() < doc.m_path.size()))
                    {
                        if (CPathUtils::PathCompare(m_path, doc.m_path.substr(0, m_path.size())) == 0)
                        {
                            activePath = doc.m_path;
                        }
                    }
                }
                bool activepathmarked = false;
                if (pData->data.size() < static_cast<size_t>(CIniSettings::Instance().GetInt64(L"View", L"maxTreeChildren", 2000)))
                {
                    for (const auto& item : pData->data)
                    {
                        activepathmarked = InsertItem(item.get(), pData->refreshRoot, TVI_FIRST, activePath) || activepathmarked;
                    }
                }
                else
                {
                    auto dummyPath          = std::format(L"{0} files...", pData->data.size());
                    auto pFi       = std::make_unique<FileTreeItem>();
                    pFi->path = dummyPath;
                    pFi->maxDummy = true;
                    pData->data.push_back(std::move(pFi));
                    activepathmarked = InsertItem(pData->data.back().get(), pData->refreshRoot, TVI_FIRST, L"");
                }

                TreeView_Expand(*this, pData->refreshRoot, TVE_EXPAND);
                TreeView_SetItemState(*this, pData->refreshRoot, 0, TVIS_CUT);

                if (fi)
                    fi->busy = false;
                m_bRootBusy = false;

                m_data[pData->refreshRoot] = std::move(pData);

                if (!activePath.empty() && !activepathmarked && !m_path.empty())
                {
                    // the current path does not appear to be visible, so
                    // refresh all sub-paths down to the active path
                    std::wstring expandPath = CPathUtils::GetParentDirectory(activePath);
                    HTREEITEM    hItem      = GetItemForPath(expandPath);
                    if (hItem == nullptr)
                    {
                        while ((hItem == nullptr) && !expandPath.empty() && (expandPath.size() > m_path.size()))
                        {
                            expandPath = CPathUtils::GetParentDirectory(expandPath);
                            hItem      = GetItemForPath(expandPath);
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
                delete pData;
                pData = nullptr;
            }
        }
            MarkActiveDocument(wParam == 0);
            break;
        case WM_TIMER:
            HandleChangeNotifications();
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
    return nullptr;
}

void CFileTree::ExpandItem(HTREEITEM hItem, bool expand)
{
    if (expand)
        Refresh(hItem, false, true);
    else
    {
        auto fi = GetFileTreeItem(*this, hItem);
        m_pathWatcher.RemovePath(fi->path);
    }
}

void CFileTree::Refresh(HTREEITEM refreshRoot, bool force /*= false*/, bool expanding /*= false*/)
{
    InvalidateUICommand(cmdFileTree, UI_INVALIDATIONS_PROPERTY, &UI_PKEY_Enabled);
    InvalidateUICommand(cmdFileTree, UI_INVALIDATIONS_PROPERTY, &UI_PKEY_BooleanValue);

    if (!force && !IsWindowVisible(*this))
        return;
    if (m_nBlockRefresh)
        return;

    if (m_bRootBusy)
        return;

    FileTreeItem* fi = nullptr;
    if (refreshRoot != TVI_ROOT)
        fi = GetFileTreeItem(*this, refreshRoot);
    if (fi)
    {
        if (fi->busy)
            return;
        fi->busy = true;
    }
    else
        m_bRootBusy = true;
    if (refreshRoot == TVI_ROOT)
    {
        m_pathWatcher.ClearPaths(true);
        m_pathWatcher.AddPath(m_path, false);
    }
    else
    {
        if (fi)
            m_pathWatcher.AddPath(fi->path, false);
    }

    TreeView_SetItemState(*this, refreshRoot, TVIS_CUT, TVIS_CUT);

    std::wstring refreshPath = m_path;
    {
        SendMessage(*this, WM_SETREDRAW, FALSE, 0);
        OnOutOfScope(SendMessage(*this, WM_SETREDRAW, TRUE, 0));

        if (!expanding)
            SetActiveItem(nullptr);

        if (refreshRoot != TVI_ROOT)
        {
            const FileTreeItem* pTreeItem = GetFileTreeItem(*this, refreshRoot);
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

        if (refreshRoot == TVI_ROOT)
            TreeView_DeleteAllItems(*this);
        else
        {
            HWND hTree = *this;
            RecurseTree(TreeView_GetChild(*this, refreshRoot), [hTree](HTREEITEM hItem) -> bool {
                TreeView_DeleteItem(hTree, hItem);
                return false;
            });
        }

        for (auto it = m_data.cbegin(); it != m_data.cend(); /* no increment */)
        {
            if (PathIsChild(refreshPath, it->second->refreshPath))
            {
                delete it->second;
                it = m_data.erase(it);
            }
            else
                ++it;
        }
        auto oldDataIt = m_data.find(refreshRoot);
        if (oldDataIt != m_data.end())
        {
            delete oldDataIt->second;
            m_data.erase(oldDataIt);
        }

        if (refreshPath.empty())
        {
            m_bRootBusy = false;
            return;
        }
        if (!PathFileExists(refreshPath.c_str()))
        {
            m_bRootBusy = false;
            return;
        }
    }

    m_threadsRunning = true;

    std::thread(&CFileTree::RefreshThread, this, refreshRoot, refreshPath, expanding).detach();
}

void CFileTree::RefreshThread(HTREEITEM refreshRoot, const std::wstring& refreshPath, bool expanding)
{
    OnOutOfScope(m_threadsRunning = false;);

    auto data         = new FileTreeData();
    data->refreshPath = refreshPath;
    data->refreshRoot = refreshRoot;
    data->data.reserve(50000);

    if (refreshRoot == TVI_ROOT)
    {
        // add an entry ".." which is used to go to the
        // parent folder.
        if (refreshPath.size() > 3)
        {
            auto parentDir = CPathUtils::GetParentDirectory(refreshPath);
            auto fi        = std::make_unique<FileTreeItem>();
            fi->path       = std::move(parentDir);
            fi->isDir      = true;
            fi->isDot      = true;

            data->data.push_back(std::move(fi));
        }
    }
    CDirFileEnum enumerator(refreshPath);
    enumerator.SetAttributesToIgnore(FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_DEVICE | FILE_ATTRIBUTE_REPARSE_POINT | FILE_ATTRIBUTE_VIRTUAL);
    bool         bIsDir = false;
    std::wstring path;
    while (enumerator.NextFile(path, &bIsDir, false) && !m_bStop)
    {
        auto fi   = std::make_unique<FileTreeItem>();
        fi->path  = std::move(path);
        fi->isDir = bIsDir;

        data->data.push_back(std::move(fi));
    }
    if (m_bStop)
    {
        delete data;
        m_bRootBusy = false;
        return;
    }
    std::sort(data->data.begin(), data->data.end(), [](const auto& lhs, const auto& rhs) {
        if (lhs->isDot)
            return false;
        if (rhs->isDot)
            return true;

        if (lhs->isDir != rhs->isDir)
            return !lhs->isDir;

        auto res = CompareStringEx(nullptr, LINGUISTIC_IGNORECASE | SORT_DIGITSASNUMBERS | SORT_STRINGSORT,
                                   lhs->path.c_str(), static_cast<int>(lhs->path.length()),
                                   rhs->path.c_str(), static_cast<int>(rhs->path.length()),
                                   nullptr, nullptr, 0);
        return res == CSTR_GREATER_THAN;
    });

    if (m_bStop)
    {
        delete data;
        m_bRootBusy = false;
        return;
    }
    PostMessage(*this, WM_THREADRESULTREADY, static_cast<WPARAM>(expanding), reinterpret_cast<LPARAM>(data));
}

HTREEITEM CFileTree::GetHitItem() const
{
    DWORD mPos = GetMessagePos();
    POINT pt{};
    pt.x = GET_X_LPARAM(mPos);
    pt.y = GET_Y_LPARAM(mPos);
    ScreenToClient(*this, &pt);
    TV_HITTESTINFO tvHti{};
    tvHti.pt        = pt;
    HTREEITEM hItem = TreeView_HitTest(*this, &tvHti);
    if ((tvHti.flags & TVHT_ONITEM) == 0)
        return nullptr;
    return hItem;
}

std::wstring CFileTree::GetPathForHitItem(bool* isDir, bool* isDot) const
{
    if (isDir)
        (*isDir) = false;
    if (isDot)
        (*isDot) = false;
    HTREEITEM hItem = GetHitItem();
    if (hItem)
    {
        const FileTreeItem* pTreeItem = GetFileTreeItem(*this, hItem);
        if (pTreeItem)
        {
            if (isDir)
                (*isDir) = pTreeItem->isDir;
            if (isDot)
                (*isDot) = pTreeItem->isDot;
            if (pTreeItem->maxDummy)
                return {};
            return pTreeItem->path;
        }
    }
    return {};
}

std::wstring CFileTree::GetPathForSelItem(bool* isDir, bool* isDot) const
{
    if (isDir)
        (*isDir) = false;
    if (isDot)
        (*isDot) = false;
    HTREEITEM hItem = TreeView_GetSelection(*this);
    if (hItem)
    {
        const FileTreeItem* pTreeItem = GetFileTreeItem(*this, hItem);
        if (pTreeItem)
        {
            if (isDir)
                (*isDir) = pTreeItem->isDir;
            if (isDot)
                (*isDot) = pTreeItem->isDot;
            if (pTreeItem->maxDummy)
                return {};
            return pTreeItem->path;
        }
    }
    return std::wstring();
}

std::wstring CFileTree::GetDirPathForHitItem() const
{
    HTREEITEM hItem = GetHitItem();
    if (hItem)
    {
        const FileTreeItem* pTreeItem = GetFileTreeItem(*this, hItem);
        if (pTreeItem)
        {
            if (pTreeItem->isDir)
                return pTreeItem->path;
            if (pTreeItem->maxDummy)
                return {};
            return pTreeItem->path.substr(0, pTreeItem->path.find_last_of('\\'));
        }
    }
    return GetPath();
}

void CFileTree::OnThemeChanged(bool bDark)
{
    if (!IsWindows8OrGreater())
    {
        // At least on Windows 7:
        // In dark mode the "Explorer" style icons for 'expand folder' aren't clear.
        // The highlight mode for selected tree items that do not
        // have the focus are too bright and blocky too such that they take attention
        // away from the main editor window.
        // THe main window has owner draw code to undo this effect
        // but windows themes override that code so we turn them of here.
        if (bDark)
        {
            SetWindowTheme(*this, nullptr, nullptr);
            TreeView_SetBkColor(*this, CTheme::Instance().GetThemeColor(RGB(255, 255, 255), true));
            TreeView_SetTextColor(*this, CTheme::Instance().GetThemeColor(RGB(0, 0, 0), true));
        }
        else
        {
            SetWindowTheme(*this, L"Explorer", nullptr);
            TreeView_SetTextColor(*this, CLR_INVALID);
            TreeView_SetBkColor(*this, CLR_INVALID);
        }
    }
    else
    {
        if (bDark)
        {
            DarkModeHelper::Instance().AllowDarkModeForWindow(*this, TRUE);
            if FAILED (SetWindowTheme(*this, L"DarkMode_Explorer", nullptr))
                SetWindowTheme(*this, L"Explorer", nullptr);
        }
        else
            SetWindowTheme(*this, L"Explorer", nullptr);
        if (bDark)
        {
            TreeView_SetBkColor(*this, CTheme::Instance().GetThemeColor(RGB(255, 255, 255), true));
        }
        else
        {
            TreeView_SetBkColor(*this, CLR_INVALID);
        }
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

void CFileTree::TabNotify(TBHDR* ptbhdr)
{
    if ((ptbhdr->hdr.code == TCN_SELCHANGE))
    {
        if (m_nBlockRefresh)
            return;
        m_bBlockExpansion = false;
        MarkActiveDocument(true);
    }
}

void CFileTree::MarkActiveDocument(bool ensureVisible)
{
    InvalidateUICommand(cmdFileTree, UI_INVALIDATIONS_PROPERTY, &UI_PKEY_Enabled);
    InvalidateUICommand(cmdFileTree, UI_INVALIDATIONS_PROPERTY, &UI_PKEY_BooleanValue);
    bool bRecurseDone = false;
    if (HasActiveDocument())
    {
        const auto& doc = GetActiveDocument();
        if (!doc.m_path.empty())
        {
            if (!PathIsChild(m_path, doc.m_path))
            {
                SetPath(CPathUtils::GetParentDirectory(doc.m_path));
            }
        }
        if (!doc.m_path.empty() && (m_path.size() < doc.m_path.size()))
        {
            if (CPathUtils::PathCompare(m_path, doc.m_path.substr(0, m_path.size())) == 0)
            {
                // find the path
                RecurseTree(TreeView_GetChild(*this, TVI_ROOT), [&](HTREEITEM hItem) -> bool {
                    const FileTreeItem* pTreeItem = GetFileTreeItem(*this, hItem);
                    if (pTreeItem)
                    {
                        if (!pTreeItem->isDir)
                        {
                            if (CPathUtils::PathCompare(doc.m_path, pTreeItem->path) == 0)
                            {
                                if (ensureVisible)
                                    TreeView_EnsureVisible(*this, hItem);
                                TreeView_SetItemState(*this, hItem, TVIS_BOLD, TVIS_BOLD);
                                SetActiveItem(hItem);
                                m_bBlockExpansion = true;
                                return true;
                            }
                        }
                        else
                        {
                            if (!m_bBlockExpansion && PathIsChild(pTreeItem->path, doc.m_path))
                                TreeView_Expand(*this, hItem, TVE_EXPAND);
                        }
                    }
                    return false;
                });
                bRecurseDone = true;
            }
        }
    }
    if (!bRecurseDone)
    {
        SetActiveItem(nullptr);
    }
}

void CFileTree::SetActiveItem(HTREEITEM hItem)
{
    if (m_activeItem)
        TreeView_SetItemState(*this, m_activeItem, 0, TVIS_BOLD);
    if (hItem)
        TreeView_SetItemState(*this, hItem, TVIS_BOLD, TVIS_BOLD);
    m_activeItem = hItem;
}

bool CFileTree::PathIsChild(const std::wstring& parent, const std::wstring& child)
{
    std::wstring sChildAsParent = child.substr(0, parent.size());
    if (sChildAsParent.empty())
        return false;
    if (CPathUtils::PathCompare(parent, sChildAsParent) != 0)
        return false;

    if (IsFolderSeparator(*parent.rbegin()))
    {
        if (!IsFolderSeparator(*sChildAsParent.rbegin()))
            return false;
    }
    else
    {
        if (!IsFolderSeparator(child[parent.size()]))
            return false;
    }
    return true;
}

void CFileTree::HandleChangeNotifications()
{
    auto changedPaths = m_pathWatcher.GetChangedPaths();
    if (changedPaths.empty())
        return;
    if (changedPaths.size() > 20)
    {
        // more than twenty new and/or deleted files: just
        // refresh the whole tree
        Refresh(TVI_ROOT);
    }
    else
    {
        bool refreshRoot = false;
        for (const auto& [action, path] : changedPaths)
        {
            if (refreshRoot)
                break;
            switch (action)
            {
                case FILE_ACTION_ADDED:
                case FILE_ACTION_RENAMED_NEW_NAME:
                {
                    HTREEITEM hDir = nullptr;
                    if (CPathUtils::PathCompare(CPathUtils::GetParentDirectory(path), m_path) == 0)
                        hDir = TVI_ROOT;
                    else
                        hDir = GetItemForPath(CPathUtils::GetParentDirectory(path));
                    auto dirIt = m_data.find(hDir);
                    if (hDir && dirIt != m_data.end())
                    {
                        if (auto& data = dirIt->second->data; data.size() < static_cast<size_t>(CIniSettings::Instance().GetInt64(L"View", L"maxTreeChildren", 2000)))
                        {
                            auto fi = std::make_unique<FileTreeItem>();
                            fi->isDir  = PathIsDirectory(path.c_str()) != 0;
                            fi->path   = path;
                            data.push_back(std::move(fi));
                            std::sort(data.begin(), data.end(), [](const auto& lhs, const auto& rhs) {
                                if (lhs->isDot)
                                    return false;
                                if (rhs->isDot)
                                    return true;

                                if (lhs->isDir != rhs->isDir)
                                    return !lhs->isDir;

                                auto res = CompareStringEx(nullptr, LINGUISTIC_IGNORECASE | SORT_DIGITSASNUMBERS | SORT_STRINGSORT,
                                                           lhs->path.c_str(), static_cast<int>(lhs->path.length()),
                                                           rhs->path.c_str(), static_cast<int>(rhs->path.length()),
                                                           nullptr, nullptr, 0);
                                return res == CSTR_GREATER_THAN;
                            });
                            HTREEITEM insertAfter = TVI_FIRST;
                            auto      foundIt     = std::find_if(data.begin(), data.end(), [&](const auto& fti) -> bool { return CPathUtils::PathCompare(fti->path, path) == 0; });
                            auto      nextIt      = foundIt;
                            ++nextIt;
                            if (nextIt != data.cend())
                            {
                                insertAfter = GetItemForPath((*nextIt)->path);
                            }
                            InsertItem(foundIt->get(), hDir, insertAfter, {});
                        }
                    }
                    else
                        refreshRoot = true;
                }
                break;
                case FILE_ACTION_REMOVED:
                case FILE_ACTION_RENAMED_OLD_NAME:
                {
                    auto hItem = GetItemForPath(path);
                    if (hItem)
                    {
                        HTREEITEM hDir = nullptr;
                        if (CPathUtils::PathCompare(path, m_path) == 0)
                            hDir = TVI_ROOT;
                        else
                            hDir = GetItemForPath(CPathUtils::GetParentDirectory(path));
                        if (hDir)
                        {
                            auto& data    = m_data[hDir]->data;
                            auto  foundIt = std::find_if(data.begin(), data.end(), [&](const auto& fi) -> bool { return CPathUtils::PathCompare(fi->path, path) == 0; });
                            data.erase(foundIt);
                        }
                        TreeView_DeleteItem(*this, hItem);
                    }
                }
                break;
                default:
                    break;
            }
        }
        if (refreshRoot)
            Refresh(TVI_ROOT);
    }
}

bool CFileTree::InsertItem(const FileTreeItem* item, HTREEITEM parent, HTREEITEM insertAfter, const std::wstring& activePath)
{
    TVITEMEX       tvi    = {0};
    TVINSERTSTRUCT tvIns  = {nullptr};
    wchar_t        dots[] = L"..";

    tvi.mask              = TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_PARAM | TVIF_CHILDREN;

    const auto filename   = CPathUtils::GetFileName(item->path);
    tvi.pszText           = const_cast<LPWSTR>(filename.c_str());
    tvi.cchTextMax        = static_cast<int>(filename.length());
    tvi.cChildren         = 0;

    if (item->isDir)
    {
        auto dirIconIndex     = CSysImageList::GetInstance().GetDirIconIndex();
        auto dirOpenIconIndex = CSysImageList::GetInstance().GetDirOpenIconIndex();
        tvi.mask |= TVIF_EXPANDEDIMAGE;
        tvi.iImage         = dirIconIndex;
        tvi.iSelectedImage = dirIconIndex;
        tvi.iExpandedImage = dirOpenIconIndex;
        tvi.cChildren      = 1;
        if (item->isDot)
        {
            tvi.iImage         = 0;
            tvi.iSelectedImage = 0;
            tvi.iExpandedImage = 0;
            tvi.pszText        = dots;
            tvi.cchTextMax     = 3;
            tvi.cChildren      = 0;
        }
    }
    else
    {
        int fileIconIndex  = CSysImageList::GetInstance().GetFileIconIndex(item->path);
        tvi.iImage         = fileIconIndex;
        tvi.iSelectedImage = fileIconIndex;
    }
    if (item->maxDummy)
    {
        tvi.mask |= TVIF_STATE;
        tvi.state     = TVIS_CUT;
        tvi.stateMask = TVIS_CUT;
    }
    bool activePathMarked = false;
    if (!activePath.empty())
    {
        if (CPathUtils::PathCompare(item->path, activePath) == 0)
        {
            tvi.mask |= TVIF_STATE;
            tvi.state        = TVIS_BOLD;
            tvi.stateMask    = TVIS_BOLD;
            activePathMarked = true;
        }
    }

    tvi.lParam         = reinterpret_cast<LPARAM>(item);
    tvIns.itemex       = tvi;
    tvIns.hInsertAfter = insertAfter;
    tvIns.hParent      = parent;

    auto hInsertedItem = TreeView_InsertItem(*this, &tvIns);
    if (tvi.state == TVIS_BOLD)
        SetActiveItem(hInsertedItem);
    return activePathMarked;
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
    m_bStop    = true;
    auto start = GetTickCount64();
    while (m_threadsRunning && (GetTickCount64() - start < 5000))
        Sleep(10);
    m_bStop = true;
}

HTREEITEM CFileTree::GetItemForPath(const std::wstring& expandpath)
{
    HTREEITEM hResult = nullptr;
    RecurseTree(TreeView_GetChild(*this, TVI_ROOT), [&](HTREEITEM hItem) -> bool {
        const FileTreeItem* pTreeItem = GetFileTreeItem(*this, hItem);
        if (pTreeItem)
        {
            if (CPathUtils::PathCompare(pTreeItem->path, expandpath) == 0)
            {
                hResult = hItem;
                return true;
            }
        }
        return false;
    });
    return hResult;
}
