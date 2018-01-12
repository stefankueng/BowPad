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
#pragma once
#include "BaseDialog.h"
#include "DlgResizer.h"

#include <vector>
/**
 * dialog to chose an item from a list of items
 */
class CChoseDlg : public CDialog
{
public:
    CChoseDlg(HWND hParent);
    ~CChoseDlg(void);

    void                    SetList(const std::vector<std::wstring>& list) { m_list = list; }

protected:
    LRESULT CALLBACK        DlgFunc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
    LRESULT DoCommand(int id, int notify);

private:
    HWND                    m_hParent;
    CDlgResizer             m_resizer;
    std::vector<std::wstring> m_list;
};
