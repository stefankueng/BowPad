// This file is part of BowPad.
//
// Copyright (C) 2013, 2016, 2021, 2024 - Stefan Kueng
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
#include "BaseDialog.h"
#include "hyperlink.h"

/**
 * about dialog.
 */
class CAboutDlg : public CDialog
{
public:
    CAboutDlg(HWND hParent);
    ~CAboutDlg();

    void SetHiddenWnd(HWND hWnd) { m_hHiddenWnd = hWnd; }

protected:
    LRESULT CALLBACK DlgFunc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) override;
    LRESULT          DoCommand(int id);
    bool             PreTranslateMessage(MSG* pMsg) override;

private:
    HWND         m_hParent;
    HWND         m_hHiddenWnd;
    CHyperLink   m_link;
    std::wstring m_version;
};
