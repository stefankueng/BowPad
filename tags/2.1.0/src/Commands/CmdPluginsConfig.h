// This file is part of BowPad.
//
// Copyright (C) 2014 - Stefan Kueng
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
#include "DlgResizer.h"
#include "ScintillaWnd.h"

class PluginInfo
{
public:
    PluginInfo()
        : version(0)
        , minversion(0200)
        , installedversion(0)
    {}

    std::wstring        name;
    int                 version;
    int                 minversion;
    int                 installedversion;
    std::wstring        description;
};

class CPluginsConfigDlg : public CDialog, public ICommand
{
public:
    CPluginsConfigDlg(void * obj);
    ~CPluginsConfigDlg(void);

protected:
    LRESULT CALLBACK        DlgFunc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
    LRESULT                 DoCommand(int id, int msg);
    LRESULT                 DoListNotify(LPNMITEMACTIVATE lpNMItemActivate);
    void                    InitPluginsList();

    virtual bool Execute() override { return true; }
    virtual UINT GetCmdId() override { return 0; }
private:
    CDlgResizer                         m_resizer;
    std::vector<PluginInfo>             m_plugins;
};


class CCmdPluginsConfig : public ICommand
{
public:

    CCmdPluginsConfig(void * obj)
        : ICommand(obj)
    {}

    ~CCmdPluginsConfig(void)
    {}

    virtual bool Execute() override;

    virtual UINT GetCmdId() override { return cmdPluginConfig; }

};
