// This file is part of BowPad.
//
// Copyright (C) 2020-2022 - Stefan Kueng
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

class CmdPalData
{
public:
    UINT         cmdId           = 0;
    int          collectionIndex = -1;
    std::wstring command;
    std::wstring description;
    std::wstring shortcut;
};

/**
 * about dialog.
 */
class CCommandPaletteDlg : public CDialog
{
public:
    CCommandPaletteDlg(HWND hParent);
    virtual ~CCommandPaletteDlg();

    void ClearFilterText();

protected:
    LRESULT CALLBACK        DlgFunc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) override;
    LRESULT                 DoCommand(int id, int code);

    void                    InitResultsList() const;
    void                    FillResults(bool force);
    LRESULT                 DoListNotify(LPNMITEMACTIVATE lpNMItemActivate);
    LRESULT                 GetListItemDispInfo(NMLVDISPINFO* pDispInfo) const;
    LRESULT                 DrawListItem(NMLVCUSTOMDRAW* pLVCD) const;

    static LRESULT CALLBACK EditSubClassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData);

private:
    CDlgResizer             m_resizer;
    HWND                    m_hParent;
    HWND                    m_hFilter;
    HWND                    m_hResults;

    std::vector<CmdPalData> m_results;
    std::vector<CmdPalData> m_allResults;
    std::vector<CmdPalData> m_collectionResults;
    ICommand*               m_pCmd;
};
