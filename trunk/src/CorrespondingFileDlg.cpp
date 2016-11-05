// This file is part of BowPad.
//
// Copyright (C) 2013 - 2016 Stefan Kueng
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

// OVERVIEW:
// Provides a simple means for a user to create two tabs at once with
// similar names / locations, though the user can change the locations.
// But things will not then work as smoothly if the location changes.
// It's primary value is in creating .cpp and .h files.

#include "stdafx.h"
#include "BowPad.h"
#include "CorrespondingFileDlg.h"
#include "AppUtils.h"
#include "PathUtils.h"
#include "StringUtils.h"
#include "OnOutOfScope.h"
#include "version.h"
#include <string>
#include <Commdlg.h>


CCorrespondingFileDlg::CCorrespondingFileDlg(void* obj)
    : ICommand(obj)
{
}

CCorrespondingFileDlg::~CCorrespondingFileDlg()
{
}

void CCorrespondingFileDlg::Show(HWND parent, const std::wstring& initialPath)
{
    m_initialPath = initialPath;
    ShowModeless(hRes, IDD_CORRESPONDINGFILEDLG, parent);
}

LRESULT CCorrespondingFileDlg::DlgFunc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_NCDESTROY:
        delete this;
        break;
    case WM_DESTROY:
        break;
    case WM_INITDIALOG:
        return DoInitDialog(hwndDlg);
        break;
    case WM_ACTIVATE:
        if (wParam == WA_INACTIVE)
            SetTransparency(150);
        else
            SetTransparency(255);
        break;
    case WM_SIZE:
    {
        int newWidth = LOWORD(lParam);
        int newHeight = HIWORD(lParam);
        m_resizer.DoResize(newWidth, newHeight);
        break;
    }
    case WM_GETMINMAXINFO:
        if (!m_freeresize)
        {
            MINMAXINFO* mmi = (MINMAXINFO*)lParam;
            mmi->ptMinTrackSize.x = m_resizer.GetDlgRectScreen()->right;
            mmi->ptMinTrackSize.y = m_resizer.GetDlgRectScreen()->bottom;
            return 0;
        }
        break;
    case WM_COMMAND:
        return DoCommand(LOWORD(wParam), HIWORD(wParam));
    case WM_NOTIFY:
        break;
    }
    return FALSE;
}

void CCorrespondingFileDlg::SyncFileNameParts()
{
    if (m_initialisingDialog)
        return;
    std::wstring f1 = GetDlgItemText(IDC_CFNAME1).get();
    std::wstring f2 = CPathUtils::GetFileNameWithoutExtension(f1);
    f2 += L".cpp";
    SetDlgItemText(*this, IDC_CFNAME2, f2.c_str());
}

void CCorrespondingFileDlg::SetStatus(const std::wstring& status)
{
    SetDlgItemText(*this, IDC_STATUS, status.c_str());
}

CFFileInfo CCorrespondingFileDlg::GetCFInfo()
{
    // We currently allow the user to avoid creating the files
    // or even specifiying a folder.
    // But the header / source switch doesn't work so well when
    // the user doesn't create the files or specifiy a folder,
    // so may dissalow this flexibility in later patches. TBD.
    // But allowing the user to defer creation or folder identification
    // until later offers the user some flexibility to make
    // decisions later in the process about these things.

    CFFileInfo info;
    info.ok = false;
    info.f1Filename = GetDlgItemText(IDC_CFNAME1).get();
    info.f1Folder = GetDlgItemText(IDC_CFFOLDER1).get();
    info.f1Path = CPathUtils::Append(info.f1Folder, info.f1Filename);
    info.f1Exists = PathFileExists(info.f1Path.c_str()) != FALSE;

    info.f2Filename = GetDlgItemText(IDC_CFNAME2).get();
    info.f2Folder = GetDlgItemText(IDC_CFFOLDER2).get();
    info.f2Path = CPathUtils::Append(info.f2Folder,info.f2Filename);
    info.f2Exists = PathFileExists(info.f2Path.c_str()) != FALSE;
    info.create = (Button_GetCheck(GetDlgItem(*this, IDC_CFCREATE)) == BST_CHECKED);

    // TODO! Use resource strings for error messages etc.

    if (info.f1Filename.empty())
    {
        info.status = L"Filename required for first file.";
        return info;
    }
    if (info.f2Filename.empty())
    {
        info.status = L"Filename required for second file.";
        return info;
    }

    if (CPathUtils::PathCompare(info.f1Path, info.f2Path) == 0)
    {
        info.status = L"The files must be different.";
        return info;
    }

    if (!info.f1Folder.empty() && !PathFileExists(info.f1Folder.c_str()))
    {
        info.status = CStringUtils::Format(L"Folder \"%s\" does not exist.",
            info.f1Folder.c_str());
        return info;
    }
    if (!info.f2Folder.empty() && !PathFileExists(info.f2Folder.c_str()))
    {
        info.status = CStringUtils::Format(L"Folder \"%s\" does not exist.",
            info.f2Folder.c_str());
        return info;
    }

    if (info.create)
    {
        if (info.f1Folder.empty())
        {
            info.status = L"Folder required for first file.";
            return info;
        }
        if (info.f2Folder.empty())
        {
            info.status = L"Folder required for second file.";
            return info;
        }

        if (info.f1Exists && info.f2Exists)
        {
            info.status = CStringUtils::Format(L"Files \"%s\" and \"%s\" already exist.",
                info.f1Path.c_str(), info.f2Path.c_str());
            return info;
        }
        if (info.f1Exists)
        {
            info.status = CStringUtils::Format(L"File \"%s\" already exists.", info.f1Path.c_str());
            return info;
        }
        if (info.f2Exists)
        {
            info.status = CStringUtils::Format(L"File \"%s\" already exists.", info.f2Path.c_str());
            return info;
        }
    }

    info.ok = true;
    return info;
}

bool CCorrespondingFileDlg::CheckStatus()
{
    bool ok = false;
    auto info = GetCFInfo();
    SetStatus(info.status.c_str());
    EnableWindow(GetDlgItem(*this, IDOK), info.ok);
    return ok;
}

LRESULT CCorrespondingFileDlg::DoCommand(int id, int msg)
{
    switch (id)
    {
    case IDCANCEL:
        if (msg == BN_CLICKED)
            DestroyWindow(*this);
        break;
    case IDOK:
    {
        if (msg == BN_CLICKED)
        {
            auto info = GetCFInfo();
            SetStatus(info.status);

            if (info.ok)
            {
                ShowWindow(*this, SW_HIDE);
                unsigned int openFlags;
                if (info.create)
                    openFlags = OpenFlags::AddToMRU | OpenFlags::CreateIfMissing;
                else
                    openFlags = OpenFlags::CreateTabOnly;
                OpenFile(info.f1Path.c_str(), openFlags);
                OpenFile(info.f2Path.c_str(), openFlags);
                ShowWindow(*this, SW_SHOW);
                DestroyWindow(*this);
            }
        }
        break;
    }
    case IDC_CFCREATE:
    {
        if (msg == BN_CLICKED)
        {
            CheckStatus();
        }
        break;
    }
    case IDC_CFNAME1:
    {
        if (msg == EN_CHANGE)
        {
            SyncFileNameParts();
            CheckStatus();
        }
        break;
    }
    case IDC_CFFOLDER1:
    {
        if (msg == EN_CHANGE)
        {
            SetDlgItemText(*this, IDC_CFFOLDER2, GetDlgItemText(IDC_CFFOLDER1).get());
            CheckStatus();
        }
        break;
    }
    case IDC_CFNAME2:
    {
        if (msg == EN_CHANGE)
            CheckStatus();
        break;
    }
    case IDC_CFFOLDER2:
    {
        if (msg == EN_CHANGE)
            CheckStatus();
        break;
    }
    case IDC_CFSETFOLDER1TOPARENT:
    case IDC_CFSETFOLDER2TOPARENT:
    {
        if (msg == BN_CLICKED)
        {
            int folderCtlId = (id == IDC_CFSETFOLDER1TOPARENT) ? IDC_CFFOLDER1 : IDC_CFFOLDER2;
            std::wstring searchFolder = GetDlgItemText(folderCtlId).get();
            std::wstring parentFolder = CPathUtils::GetParentDirectory(searchFolder);
            // If parent folder is empty assume we reached the root and do nothing.
            if (!parentFolder.empty())
                SetDlgItemText(*this, folderCtlId, parentFolder.c_str());
        }
        break;
    }
    } // switch.

    return 1;
}

void CCorrespondingFileDlg::InitSizing()
{
    HWND hwndDlg = *this;
    AdjustControlSize(IDC_CFCREATE);
    m_resizer.Init(hwndDlg);
    m_resizer.AddControl(hwndDlg, IDC_CFNAME1, RESIZER_TOPLEFT);
    m_resizer.AddControl(hwndDlg, IDC_CFFOLDER1, RESIZER_TOPLEFTRIGHT);
    m_resizer.AddControl(hwndDlg, IDC_CFSETFOLDER1TOPARENT, RESIZER_TOPRIGHT);
    m_resizer.AddControl(hwndDlg, IDC_CFNAME2, RESIZER_TOPLEFT);
    m_resizer.AddControl(hwndDlg, IDC_CFFOLDER2, RESIZER_TOPLEFTRIGHT);
    m_resizer.AddControl(hwndDlg, IDC_CFSETFOLDER2TOPARENT, RESIZER_TOPRIGHT);
    m_resizer.AddControl(hwndDlg, IDC_STATUS, RESIZER_BOTTOMLEFTRIGHT);
    m_resizer.AddControl(hwndDlg, IDOK, RESIZER_BOTTOMRIGHT);
    m_resizer.AddControl(hwndDlg, IDCANCEL, RESIZER_BOTTOMRIGHT);
    m_resizer.AdjustMinMaxSize();
}

LRESULT CCorrespondingFileDlg::DoInitDialog(HWND hwndDlg)
{
    OnOutOfScope(
        m_initialisingDialog = false;
    );
    InitDialog(hwndDlg, IDI_BOWPAD, true);
    InitSizing();
    SetDlgItemText(*this, IDC_CFNAME1, L".h");
    SetDlgItemText(*this, IDC_CFNAME2, L".cpp");
    auto hF1Folder = GetDlgItem(*this, IDC_CFFOLDER1);
    auto hF2Folder = GetDlgItem(*this, IDC_CFFOLDER2);
    SetDlgItemText(*this, IDC_CFFOLDER1, m_initialPath.c_str());
    SetDlgItemText(*this, IDC_CFFOLDER2, m_initialPath.c_str());
    // Create the files by default because the header switch
    // logic work better when they exist on disk.
    // We might change this default if we improve things in later patches.
    Button_SetCheck(GetDlgItem(*this, IDC_CFCREATE),BST_CHECKED);
    SHAutoComplete(hF1Folder, SHACF_FILESYS_DIRS);
    SHAutoComplete(hF2Folder, SHACF_FILESYS_DIRS);
    CheckStatus();
    auto hF1Filename = GetDlgItem(*this, IDC_CFNAME1);
    Edit_SetSel(hF1Filename, 0, 0);
    return FALSE;
}


