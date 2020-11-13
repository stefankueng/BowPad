// This file is part of BowPad.
//
// Copyright (C) 2013 - 2016, 2020 Stefan Kueng
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
#include "ICommand.h"

#include <vector>

struct CFFileInfo
{
    std::wstring status;
    bool         ok       = false;
    bool         create   = false;
    bool         f1Exists = false;
    bool         f2Exists = false;
    std::wstring f1Filename;
    std::wstring f1Folder;
    std::wstring f1Path;
    std::wstring f2Filename;
    std::wstring f2Folder;
    std::wstring f2Path;
};

class CCorrespondingFileDlg : public CDialog
    , public ICommand
{
public:
    CCorrespondingFileDlg(void* obj);
    ~CCorrespondingFileDlg();
    void Show(HWND hParent, const std::wstring& initialPath);

protected:
    bool Execute() override { return true; }
    UINT GetCmdId() override { return 0; }
    //void OnClose() override;

protected:
    LRESULT CALLBACK DlgFunc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) override;
    LRESULT          DoCommand(int id, int msg);
    void             InitSizing();
    LRESULT          DoInitDialog(HWND hwndDlg);
    void             SyncFileNameParts();
    void             SetStatus(const std::wstring& status);
    bool             CheckStatus();
    CFFileInfo       GetCFInfo();

protected:
    CDlgResizer  m_resizer;
    std::wstring m_initialPath;
};
