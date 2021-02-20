// This file is part of BowPad.
//
// Copyright (C) 2013-2017, 2020-2021 - Stefan Kueng
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
#include "BowPadUI.h"
#include "BaseDialog.h"
#include "ColorButton.h"

#include <vector>

class CStyleConfiguratorDlg : public CDialog
    , public ICommand
{
public:
    CStyleConfiguratorDlg(void* obj);
    ~CStyleConfiguratorDlg() = default;

protected:
    LRESULT CALLBACK DlgFunc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) override;
    LRESULT          DoCommand(int id, int msg);

    bool Execute() override { return true; }
    UINT GetCmdId() override { return cmdStyleConfigurator; }

    static int CALLBACK EnumFontFamExProc(const LOGFONT* lpelfe, const TEXTMETRIC* lpntme, DWORD fontType, LPARAM lParam);
    void                SelectStyle(int style);

private:
    std::vector<std::wstring> m_fonts;
    CColorButton              m_fgColor;
    CColorButton              m_bkColor;
};

class CCmdStyleConfigurator : public ICommand
{
public:
    CCmdStyleConfigurator(void* obj);
    ~CCmdStyleConfigurator();

    bool Execute() override;

    UINT GetCmdId() override { return cmdStyleConfigurator; }

    void ScintillaNotify(SCNotification* pScn) override;

    void TabNotify(TBHDR* ptbHdr) override;
};
