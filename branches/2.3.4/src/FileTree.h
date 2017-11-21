// This file is part of BowPad.
//
// Copyright (C) 2014-2016 - Stefan Kueng
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
#include "ICommand.h"
#include <functional>

typedef std::function<bool(HTREEITEM)> ItemHandler;

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
    HTREEITEM                   refreshRoot = nullptr;
    std::vector<FileTreeItem*>  data;
};

class CFileTree : public CWindow, public ICommand
{
public:
    CFileTree(HINSTANCE hInst, void * obj);
    virtual ~CFileTree();

    bool Init(HINSTANCE hInst, HWND hParent);
    void Clear();
    void SetPath(const std::wstring& path) { m_path = path; Refresh(TVI_ROOT); }
    std::wstring GetPath()const { return m_path; }
    HTREEITEM GetHitItem() const;
    void Refresh(HTREEITEM refreshRoot, bool force = false);
    std::wstring GetFilePathForHitItem() const;
    std::wstring GetFilePathForSelItem() const;
    std::wstring GetDirPathForHitItem() const;
    
    void OnThemeChanged(bool bDark) override;
    bool Execute() override;
    UINT GetCmdId() override;

    void BlockRefresh(bool bBlock);

protected:
    LRESULT CALLBACK WinMsgHandler(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) override;

    HTREEITEM RecurseTree(HTREEITEM hItem, ItemHandler handler);
    HTREEITEM GetItemForPath(const std::wstring& expandpath);
    void RefreshThread(HTREEITEM refreshRoot, const std::wstring& refreshPath);
    void MarkActiveDocument();

    void TabNotify(TBHDR * ptbhdr) override;

    void OnClose() override;

private:
    std::wstring        m_path;
    int                 m_nBlockRefresh;
    volatile LONG       m_ThreadsRunning;
    bool                m_bStop;
    bool                m_bRootBusy;
};
