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
#pragma once
#include "BaseWindow.h"
#include "ICommand.h"
#include <functional>

typedef std::function<bool(HTREEITEM)> ItemHandler;

class CFileTree : public CWindow, public ICommand
{
public:
    CFileTree(HINSTANCE hInst, void * obj)
        : CWindow(hInst)
        , ICommand(obj)
        , m_bBlockRefresh(false)
        , m_bBusyRoot(false)
        , m_ThreadsRunning(0)
        , m_bStop(false)
    {};
    virtual ~CFileTree();

    bool Init(HINSTANCE hInst, HWND hParent);
    void SetPath(const std::wstring& path) { m_path = path; Refresh(TVI_ROOT); }
    std::wstring GetPath()const { return m_path; }
    HTREEITEM GetHitItem();
    void Refresh(HTREEITEM refreshRoot, bool force = false);
    std::wstring GetFilePathForHitItem();
    std::wstring GetFilePathForSelItem();
    
    virtual void OnThemeChanged(bool bDark) override;
    virtual bool Execute() override;
    virtual UINT GetCmdId() override;

    void BlockRefresh(bool bBlock);

protected:
    virtual LRESULT CALLBACK WinMsgHandler(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) override;

    HTREEITEM RecurseTree(HTREEITEM hItem, ItemHandler handler);
    void RefreshThread(HTREEITEM refreshRoot, const std::wstring& activepath);

    virtual void TabNotify(TBHDR * ptbhdr);

    virtual void OnClose() override;

private:
    std::wstring        m_path;
    bool                m_bBlockRefresh;
    bool                m_bBusyRoot;
    volatile LONG       m_ThreadsRunning;
    bool                m_bStop;
};
