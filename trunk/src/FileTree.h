// This file is part of BowPad.
//
// Copyright (C) 2014 - Stefan Kueng
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
    {};
    virtual ~CFileTree();

    bool Init(HINSTANCE hInst, HWND hParent);
    void SetPath(const std::wstring& path) { m_path = path; Refresh(TVI_ROOT); }
    std::wstring GetPath()const { return m_path; }
    HTREEITEM GetHitItem();
    void Refresh(HTREEITEM refreshRoot);
    std::wstring GetFilePathForHitItem();
    std::wstring GetFilePathForSelItem();
    
    virtual void OnThemeChanged(bool bDark) override;
    virtual bool Execute() override;
    virtual UINT GetCmdId() override;

protected:
    virtual LRESULT CALLBACK WinMsgHandler(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) override;

    HTREEITEM RecurseTree(HTREEITEM hItem, ItemHandler handler);

private:
    std::wstring        m_path;
};
