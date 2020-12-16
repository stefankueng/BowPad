// This file is part of BowPad.
//
// Copyright (C) 2013-2014, 2016-2017, 2020 - Stefan Kueng
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
#include "CmdGotoLine.h"
#include "BowPad.h"
#include "ScintillaWnd.h"
#include "UnicodeUtils.h"
#include "StringUtils.h"
#include "Theme.h"

CGotoLineDlg::CGotoLineDlg()
    : line(0)
{
}

LRESULT CGotoLineDlg::DlgFunc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (uMsg)
    {
    case WM_INITDIALOG:
        {
            InitDialog(hwndDlg, IDI_BOWPAD);
            CTheme::Instance().SetThemeForDialog(*this, CTheme::Instance().IsDarkTheme());

            SetDlgItemText(hwndDlg, IDC_LINEINFO, lineinfo.c_str());
            std::wstring sLine = CStringUtils::Format(L"%lld", line);
            SetDlgItemText(hwndDlg, IDC_LINE, sLine.c_str());
            SendDlgItemMessage(hwndDlg, IDC_LINE, EM_SETSEL, 0, -1);
        }
        return FALSE;
    case WM_COMMAND:
        return DoCommand(LOWORD(wParam), HIWORD(wParam));
    default:
        return FALSE;
    }
    return FALSE;
}

LRESULT CGotoLineDlg::DoCommand(int id, int /*msg*/)
{
    switch (id)
    {
    case IDCANCEL:
        EndDialog(*this, id);
        break;
    case IDOK:
        {
            auto sLine = GetDlgItemText(IDC_LINE);
            line = _wtol(sLine.get());
            EndDialog(*this, id);
        }
    }
    return 1;
}

bool CCmdGotoLine::Execute()
{
    CGotoLineDlg dlg;
    dlg.line   = GetCurrentLineNumber() + 1;
    auto first = ScintillaCall(SCI_LINEFROMPOSITION, 0)+1;
    auto last  = ScintillaCall(SCI_LINEFROMPOSITION, ScintillaCall(SCI_GETLENGTH))+1;
    ResString lineformat(g_hRes, IDS_GOTOLINEINFO);
    dlg.lineinfo = CStringUtils::Format(lineformat, first, last);
    if (dlg.DoModal(g_hRes, IDD_GOTOLINE, GetHwnd())==IDOK)
    {
        GotoLine(dlg.line - 1);
    }

    return true;
}

