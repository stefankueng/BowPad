// This file is part of BowPad.
//
// Copyright (C) 2013, 2015-2017, 2020 - Stefan Kueng
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
#include "AboutDlg.h"
#include "AppUtils.h"
#include "version.h"
#include "Theme.h"
#include <string>
#include <Commdlg.h>

CAboutDlg::CAboutDlg(HWND hParent)
    : m_hParent(hParent)
    , m_hHiddenWnd(nullptr)
{
}

CAboutDlg::~CAboutDlg()
{
}

LRESULT CAboutDlg::DlgFunc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (uMsg)
    {
        case WM_INITDIALOG:
        {
            InitDialog(hwndDlg, IDI_BOWPAD);
            CTheme::Instance().SetThemeForDialog(*this, CTheme::Instance().IsDarkTheme());
            // initialize the controls
            m_link.ConvertStaticToHyperlink(hwndDlg, IDC_WEBLINK, L"http://tools.stefankueng.com");
            wchar_t verbuf[1024] = {0};
#ifdef _WIN64
            swprintf_s(verbuf, _countof(verbuf), L"BowPad version %d.%d.%d.%d (64-bit)", BP_VERMAJOR, BP_VERMINOR, BP_VERMICRO, BP_VERBUILD);
#else
            swprintf_s(verbuf, _countof(verbuf), L"BowPad version %d.%d.%d.%d", BP_VERMAJOR, BP_VERMINOR, BP_VERMICRO, BP_VERBUILD);
#endif
            SetDlgItemText(hwndDlg, IDC_VERSIONLABEL, verbuf);
        }
            return TRUE;
        case WM_COMMAND:
            return DoCommand(LOWORD(wParam));
        default:
            return FALSE;
    }
}

LRESULT CAboutDlg::DoCommand(int id)
{
    switch (id)
    {
        case IDOK:
            // fall through
        case IDCANCEL:
            EndDialog(*this, id);
            break;
        case IDC_UPDATECHECK:
        {
            bool bNewer = CAppUtils::CheckForUpdate(true);
            if (bNewer)
            {
                CAppUtils::ShowUpdateAvailableDialog(*this);
            }
            else
            {
                ResString sNoUpdate(hRes, IDS_NOUPDATES);
                MessageBox(*this, sNoUpdate, L"BowPad", MB_ICONINFORMATION);
            }
        }
        break;
    }
    return 1;
}
