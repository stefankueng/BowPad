// This file is part of BowPad.
//
// Copyright (C) 2013, 2017, 2020-2021 - Stefan Kueng
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
#include "BowPad.h"
#include "ChoseDlg.h"
#include "Theme.h"
#include <string>
#include <Commdlg.h>

CChoseDlg::CChoseDlg(HWND hParent)
    : m_hParent(hParent)
{
}

CChoseDlg::~CChoseDlg(void)
{
}

LRESULT CChoseDlg::DlgFunc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (uMsg)
    {
        case WM_INITDIALOG:
        {
            InitDialog(hwndDlg, IDI_BOWPAD);
            CTheme::Instance().SetThemeForDialog(*this, CTheme::Instance().IsDarkTheme());
            m_resizer.Init(hwndDlg);
            m_resizer.AddControl(hwndDlg, IDC_LIST, RESIZER_TOPLEFTBOTTOMRIGHT);
            m_resizer.UseSizeGrip(false);
            // initialize the controls
            int maxlen = 0;
            for (const auto& item : m_list)
            {
                maxlen = max(maxlen, (int)item.size());
                SendDlgItemMessage(hwndDlg, IDC_LIST, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(item.c_str()));
            }

            // resize the dialog to match the item list
            int height = static_cast<int>(SendDlgItemMessage(hwndDlg, IDC_LIST, LB_GETITEMHEIGHT, 0, 0));
            int width  = maxlen * height * 2 / 3;
            width += 20;
            height = height * min((int)m_list.size(), 5);
            height += 20;
            SetWindowPos(hwndDlg, nullptr, 0, 0, width, height, SWP_NOMOVE | SWP_NOREPOSITION);
        }
            return TRUE;
        case WM_SIZE:
            m_resizer.DoResize(LOWORD(lParam), HIWORD(lParam));
            break;
        case WM_COMMAND:
            return DoCommand(LOWORD(wParam), HIWORD(wParam));
        default:
            return FALSE;
    }
    return 0;
}

LRESULT CChoseDlg::DoCommand(int id, int notify)
{
    switch (id)
    {
        case IDOK:
            EndDialog(*this, SendDlgItemMessage(*this, IDC_LIST, LB_GETCURSEL, 0, 0));
            break;
        case IDCANCEL:
            EndDialog(*this, -1);
            break;
        case IDC_LIST:
        {
            if (notify == LBN_DBLCLK)
                EndDialog(*this, SendDlgItemMessage(*this, IDC_LIST, LB_GETCURSEL, 0, 0));
        }
        break;
    }
    return 1;
}
