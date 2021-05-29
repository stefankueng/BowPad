// This file is part of BowPad.
//
// Copyright (C) 2014-2016, 2018-2021 - Stefan Kueng
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
#include "PathWatcher.h"
#include "ICommand.h"
#include <functional>

using ItemHandler = std::function<bool(HTREEITEM)>;

class FileTreeItem
{
public:
    FileTreeItem() {}
    std::wstring path;
    bool         isDir = false;
    bool         isDot = false;
    bool         busy  = false;
};

class FileTreeData
{
public:
    FileTreeData() {}

    std::wstring                               refreshPath;
    HTREEITEM                                  refreshRoot = nullptr;
    std::vector<std::unique_ptr<FileTreeItem>> data;
};

class CFileTree
    : public CWindow
    , public ICommand
{
public:
    CFileTree(HINSTANCE hInst, void* obj);
    ~CFileTree() override;

    bool         Init(HINSTANCE hInst, HWND hParent);
    void         Clear();
    void         SetPath(const std::wstring& path, bool forceRefresh = true);
    std::wstring GetPath() const { return m_path; }
    HTREEITEM    GetHitItem() const;
    void         ExpandItem(HTREEITEM hItem, bool expand);
    void         Refresh(HTREEITEM refreshRoot, bool force = false, bool expanding = false);
    std::wstring GetPathForHitItem(bool* isDir, bool* isDot) const;
    std::wstring GetPathForSelItem(bool* isDir, bool* isDot) const;
    std::wstring GetDirPathForHitItem() const;

    void OnThemeChanged(bool bDark) override;
    bool Execute() override;
    UINT GetCmdId() override;

    void BlockRefresh(bool bBlock);

protected:
    LRESULT CALLBACK WinMsgHandler(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) override;

    HTREEITEM   RecurseTree(HTREEITEM hItem, ItemHandler handler);
    HTREEITEM   GetItemForPath(const std::wstring& expandpath);
    void        RefreshThread(HTREEITEM refreshRoot, const std::wstring& refreshPath, bool expanding);
    void        MarkActiveDocument(bool ensureVisible);
    void        SetActiveItem(HTREEITEM hItem);
    static bool PathIsChild(const std::wstring& parent, const std::wstring& child);
    void        HandleChangeNotifications();
    bool        InsertItem(const std::unique_ptr<FileTreeItem>& item, HTREEITEM parent, HTREEITEM insertAfter, const std::wstring& activePath);

    void TabNotify(TBHDR* ptbhdr) override;

    void OnClose() override;

private:
    std::wstring                       m_path;
    int                                m_nBlockRefresh;
    volatile LONG                      m_threadsRunning;
    volatile LONG                      m_bStop;
    bool                               m_bRootBusy;
    HTREEITEM                          m_activeItem;
    bool                               m_bBlockExpansion;
    std::map<HTREEITEM, FileTreeData*> m_data;
    CPathWatcher                       m_pathWatcher;
};
