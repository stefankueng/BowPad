// This file is part of BowPad.
//
// Copyright (C) 2013-2014 - Stefan Kueng
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
#include "StringUtils.h"
#include "PathUtils.h"
#include "UnicodeUtils.h"
#include "EscapeUtils.h"
#include "BaseDialog.h"
#include "DlgResizer.h"

class LaunchBase : public ICommand
{
public:
    LaunchBase(void * obj) : ICommand(obj){}
    virtual ~LaunchBase(void){}

protected:
    bool Launch(const std::wstring& cmdline);
};

class CCmdLaunchIE : public LaunchBase
{
public:
    CCmdLaunchIE(void * obj) : LaunchBase(obj){}
    ~CCmdLaunchIE(void){}

    virtual bool Execute() { return Launch(L"iexplore \"$(TAB_PATH)\""); }
    virtual UINT GetCmdId() { return cmdLaunchIE; }
};

class CCmdLaunchFirefox : public LaunchBase
{
public:
    CCmdLaunchFirefox(void * obj) : LaunchBase(obj){}
    ~CCmdLaunchFirefox(void){}

    virtual bool Execute() { return Launch(L"firefox \"$(TAB_PATH)\""); }
    virtual UINT GetCmdId() { return cmdLaunchFirefox; }
};

class CCmdLaunchChrome : public LaunchBase
{
public:
    CCmdLaunchChrome(void * obj) : LaunchBase(obj){}
    ~CCmdLaunchChrome(void){}

    virtual bool Execute() { return Launch(L"chrome \"$(TAB_PATH)\""); }
    virtual UINT GetCmdId() { return cmdLaunchChrome; }
};

class CCmdLaunchSafari : public LaunchBase
{
public:
    CCmdLaunchSafari(void * obj) : LaunchBase(obj){}
    ~CCmdLaunchSafari(void){}

    virtual bool Execute() { return Launch(L"safari \"$(TAB_PATH)\""); }
    virtual UINT GetCmdId() { return cmdLaunchSafari; }
};

class CCmdLaunchOpera : public LaunchBase
{
public:
    CCmdLaunchOpera(void * obj) : LaunchBase(obj){}
    ~CCmdLaunchOpera(void){}

    virtual bool Execute() { return Launch(L"opera \"$(TAB_PATH)\""); }
    virtual UINT GetCmdId() { return cmdLaunchOpera; }
};

class CCmdLaunchSearch : public LaunchBase
{
public:
    CCmdLaunchSearch(void * obj) : LaunchBase(obj){}
    ~CCmdLaunchSearch(void){}

    virtual bool Execute()
    {
        std::wstring sLaunch = CIniSettings::Instance().GetString(L"CustomLaunch", L"websearch", L"http://www.google.com/search?q=$(SEL_TEXT_ESCAPED)");
        if (sLaunch.empty())
            sLaunch = L"http://www.google.com/search?q=$(SEL_TEXT_ESCAPED)";
        return Launch(sLaunch);
    }
    virtual UINT GetCmdId() { return cmdLaunchSearch; }
};

class CCmdLaunchWikipedia : public LaunchBase
{
public:
    CCmdLaunchWikipedia(void * obj) : LaunchBase(obj){}
    ~CCmdLaunchWikipedia(void){}

    virtual bool Execute() { return Launch(L"http://en.wikipedia.org/wiki/Special:Search?search=$(SEL_TEXT_ESCAPED)"); }
    virtual UINT GetCmdId() { return cmdLaunchWikipedia; }
};

class CCmdLaunchConsole : public LaunchBase
{
public:
    CCmdLaunchConsole(void * obj) : LaunchBase(obj){}
    ~CCmdLaunchConsole(void){}

    virtual bool Execute() { return Launch(L"cmd /K cd /d \"$(TAB_DIR)\""); }
    virtual UINT GetCmdId() { return cmdLaunchConsole; }
};

class CCmdLaunchExplorer : public LaunchBase
{
public:
    CCmdLaunchExplorer(void * obj) : LaunchBase(obj){}
    ~CCmdLaunchExplorer(void){}

    virtual bool Execute() { return Launch(L"explorer \"$(TAB_DIR)\""); }
    virtual UINT GetCmdId() { return cmdLaunchExplorer; }
};

class CCmdLaunchCustom : public LaunchBase
{
public:
    CCmdLaunchCustom(UINT customId, void * obj) : m_customId(customId), LaunchBase(obj)
    {
        m_customCmdId = cmdLaunchCustom0 + customId;
        m_settingsID = CStringUtils::Format(L"Command%d", customId);
        InvalidateUICommand(m_customCmdId, UI_INVALIDATIONS_PROPERTY, &UI_PKEY_Label);
        InvalidateUICommand(m_customCmdId, UI_INVALIDATIONS_STATE, &UI_PKEY_Enabled);
    }
    ~CCmdLaunchCustom(void){}

    virtual bool Execute() { return Launch(CIniSettings::Instance().GetString(L"CustomLaunch", m_settingsID.c_str(), L"")); }
    virtual UINT GetCmdId() { return m_customCmdId; }

    virtual HRESULT IUICommandHandlerUpdateProperty( REFPROPERTYKEY key, const PROPVARIANT* ppropvarCurrentValue, PROPVARIANT* ppropvarNewValue );

private:
    UINT            m_customId;
    UINT            m_customCmdId;
    std::wstring    m_settingsID;
};

class CCustomCommandsDlg : public CDialog
{
public:
    CCustomCommandsDlg();
    ~CCustomCommandsDlg(void);

protected:
    LRESULT CALLBACK        DlgFunc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
    LRESULT                 DoCommand(int id);

    virtual bool Execute() { return true; }
    virtual UINT GetCmdId() { return 0; }

private:
    CDlgResizer             m_resizer;
};

class CCmdCustomCommands : public ICommand
{
public:
    CCmdCustomCommands(void * obj) : ICommand(obj) {}
    ~CCmdCustomCommands(void){}

    virtual bool Execute();
    virtual UINT GetCmdId() { return cmdCustomCommands; }
};

