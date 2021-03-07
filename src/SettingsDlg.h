// This file is part of BowPad.
//
// Copyright (C) 2021 - Stefan Kueng
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
#include "ICommand.h"
#include "BaseDialog.h"
#include "DlgResizer.h"

enum class SettingType
{
    Boolean,
    Number,
    String,
    None,
};

union SettingDefaultValue
{
    bool    b;
    LPCTSTR s;
    DWORD   l;
};

struct SettingData
{
    std::wstring        section;
    std::wstring        key;
    std::wstring        name;
    std::wstring        description;
    SettingType         type;
    SettingDefaultValue def;
};

class CSettingsDlg : public CDialog
{
public:
    CSettingsDlg();
    ~CSettingsDlg() = default;

protected:
    LRESULT CALLBACK DlgFunc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) override;
    LRESULT          DoCommand(int id, int msg);
    LRESULT          DoListNotify(LPNMITEMACTIVATE lpNMItemActivate);
    void             InitSettingsList();

private:
    CDlgResizer              m_resizer;
    std::vector<SettingData> m_settings;
};
