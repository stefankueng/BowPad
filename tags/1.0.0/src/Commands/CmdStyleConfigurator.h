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
#include "ICommand.h"
#include "BowPadUI.h"
#include "BaseDialog.h"
#include "ColorButton.h"

#include <vector>
#include <set>

class CStyleConfiguratorDlg : public CDialog,  public ICommand
{
public:
    CStyleConfiguratorDlg(void * obj);
    ~CStyleConfiguratorDlg(void);

protected:
    LRESULT CALLBACK        DlgFunc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
    LRESULT                 DoCommand(int id, int msg);

    virtual bool            Execute() { return true; }
    virtual UINT            GetCmdId() { return 0; }

    static int CALLBACK     EnumFontFamExProc(const LOGFONT *lpelfe, const TEXTMETRIC *lpntme, DWORD FontType, LPARAM lParam);
    void                    SelectStyle( int style );
private:
    std::vector<std::wstring>   m_fonts;
    std::set<std::wstring>      m_tempFonts;
    CColorButton                m_fgColor;
    CColorButton                m_bkColor;
};


class CCmdStyleConfigurator : public ICommand
{
public:

    CCmdStyleConfigurator(void * obj)
        : ICommand(obj)
        , m_pStyleConfiguratorDlg(nullptr)
    {
    }

    ~CCmdStyleConfigurator(void)
    {
        delete m_pStyleConfiguratorDlg;
    }

    virtual bool Execute();

    virtual UINT GetCmdId() { return cmdStyleConfigurator; }

    virtual void ScintillaNotify( Scintilla::SCNotification * pScn );

    virtual void TabNotify( TBHDR * ptbhdr );

private:
    CStyleConfiguratorDlg *           m_pStyleConfiguratorDlg;
};
