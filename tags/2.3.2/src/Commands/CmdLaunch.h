// This file is part of BowPad.
//
// Copyright (C) 2013-2014, 2016-2017 - Stefan Kueng
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

class LaunchBase : public ICommand
{
public:
    LaunchBase(void * obj) : ICommand(obj) {}
    virtual ~LaunchBase() = default;

protected:
    bool Launch(const std::wstring& cmdline);
};

class CCmdLaunchIE : public LaunchBase
{
public:
    CCmdLaunchIE(void * obj) : LaunchBase(obj) {}
    ~CCmdLaunchIE() = default;

    bool Execute() override { return Launch(L"iexplore \"$(TAB_PATH)\""); }
    UINT GetCmdId() override { return cmdLaunchIE; }
};

class CCmdLaunchFirefox : public LaunchBase
{
public:
    CCmdLaunchFirefox(void * obj) : LaunchBase(obj) {}
    ~CCmdLaunchFirefox() = default;

    bool Execute() override { return Launch(L"firefox \"$(TAB_PATH)\""); }
    UINT GetCmdId() override { return cmdLaunchFirefox; }
};

class CCmdLaunchChrome : public LaunchBase
{
public:
    CCmdLaunchChrome(void * obj) : LaunchBase(obj) {}
    ~CCmdLaunchChrome() = default;

    bool Execute() override { return Launch(L"chrome \"$(TAB_PATH)\""); }
    UINT GetCmdId() override { return cmdLaunchChrome; }
};

class CCmdLaunchSafari : public LaunchBase
{
public:
    CCmdLaunchSafari(void * obj) : LaunchBase(obj) {}
    ~CCmdLaunchSafari() = default;

    bool Execute() override { return Launch(L"safari \"$(TAB_PATH)\""); }
    UINT GetCmdId() override { return cmdLaunchSafari; }
};

class CCmdLaunchOpera : public LaunchBase
{
public:
    CCmdLaunchOpera(void * obj) : LaunchBase(obj) {}
    ~CCmdLaunchOpera() = default;

    bool Execute() override { return Launch(L"opera \"$(TAB_PATH)\""); }
    UINT GetCmdId() override { return cmdLaunchOpera; }
};

class CCmdLaunchSearch : public LaunchBase
{
public:
    CCmdLaunchSearch(void * obj) : LaunchBase(obj) {}
    ~CCmdLaunchSearch() = default;

    bool Execute() override;
    UINT GetCmdId() override { return cmdLaunchSearch; }
};

class CCmdLaunchWikipedia : public LaunchBase
{
public:
    CCmdLaunchWikipedia(void * obj) : LaunchBase(obj) {}
    ~CCmdLaunchWikipedia() = default;

    bool Execute() override { return Launch(L"http://en.wikipedia.org/wiki/Special:Search?search=$(SEL_TEXT_ESCAPED)"); }
    UINT GetCmdId() override { return cmdLaunchWikipedia; }
};

class CCmdLaunchConsole : public LaunchBase
{
public:
    CCmdLaunchConsole(void * obj) : LaunchBase(obj) {}
    ~CCmdLaunchConsole() = default;

    bool Execute() override
    {
        if (HasActiveDocument() && !GetActiveDocument().m_path.empty())
            return Launch(L"cmd /K cd /d \"$(TAB_DIR)\"");
        else
            return Launch(L"cmd /K");
    }
    UINT GetCmdId() override { return cmdLaunchConsole; }
};

class CCmdLaunchExplorer : public LaunchBase
{
public:
    CCmdLaunchExplorer(void * obj) : LaunchBase(obj) {}
    ~CCmdLaunchExplorer() = default;

    bool Execute() override { return Launch(L"explorer \"$(TAB_DIR)\""); }
    UINT GetCmdId() override { return cmdLaunchExplorer; }
};

class CCmdLaunchCustom : public LaunchBase
{
public:
    CCmdLaunchCustom(UINT customId, void * obj);
    ~CCmdLaunchCustom() = default;

    bool Execute() override { return Launch(CIniSettings::Instance().GetString(L"CustomLaunch", m_settingsID.c_str(), L"")); }
    UINT GetCmdId() override { return m_customCmdId; }
    void AfterInit() override;

    HRESULT IUICommandHandlerUpdateProperty(REFPROPERTYKEY key, const PROPVARIANT* ppropvarCurrentValue, PROPVARIANT* ppropvarNewValue) override;

private:
    UINT            m_customId;
    UINT            m_customCmdId;
    std::wstring    m_settingsID;
};

class CCustomCommandsDlg : public CDialog
{
public:
    CCustomCommandsDlg();
    ~CCustomCommandsDlg() = default;

protected:
    LRESULT CALLBACK        DlgFunc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
    LRESULT                 DoCommand(int id);

private:
    CDlgResizer             m_resizer;
};

class CCmdCustomCommands : public ICommand
{
public:
    CCmdCustomCommands(void * obj) : ICommand(obj) {}
    ~CCmdCustomCommands() = default;

    virtual bool Execute() override;
    virtual UINT GetCmdId() override { return cmdCustomCommands; }
};
