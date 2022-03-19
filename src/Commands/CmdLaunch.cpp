// This file is part of BowPad.
//
// Copyright (C) 2013-2017, 2019-2022 - Stefan Kueng
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
#include "CmdLaunch.h"
#include "BowPad.h"
#include "StringUtils.h"
#include "PathUtils.h"
#include "UnicodeUtils.h"
#include "EscapeUtils.h"
#include "Theme.h"
#include "ResString.h"

bool LaunchBase::Launch(const std::wstring& cmdline) const
{
    if (cmdline.empty() || !HasActiveDocument())
        return false;
    std::wstring cmd = cmdline;
    // remove EOLs
    SearchRemoveAll(cmd, L"\n");
    SearchReplace(cmd, L"\r", L" ");
    // replace the macros in the command line
    const auto&  doc     = GetActiveDocument();
    std::wstring tabPath = doc.m_path;
    if ((cmd.find(L"$(TAB_PATH)") != std::wstring::npos) ||
        (cmd.find(L"$(TAB_NAME)") != std::wstring::npos) ||
        (cmd.find(L"$(TAB_EXT)") != std::wstring::npos))
    {
        // save the file first
        SaveCurrentTab();
    }

    auto tabDirPath = CPathUtils::GetParentDirectory(tabPath);
    if (PathFileExists(tabPath.c_str()))
    {
        SearchReplace(cmd, L"$(TAB_PATH)", tabPath);
        SearchReplace(cmd, L"$(TAB_DIR)", tabDirPath);
        SearchReplace(cmd, L"$(TAB_NAME)", CPathUtils::GetFileNameWithoutExtension(tabPath));
        SearchReplace(cmd, L"$(TAB_EXT)", CPathUtils::GetFileExtension(tabPath));
    }
    else
    {
        SearchRemoveAll(cmd, L"$(TAB_PATH)");
        SearchRemoveAll(cmd, L"$(TAB_DIR)");
    }

    SearchReplace(cmd, L"$(LINE)", std::to_wstring(GetCurrentLineNumber()));
    SearchReplace(cmd, L"$(POS)", std::to_wstring(static_cast<int>(Scintilla().CurrentPos())));
    // find selected text or current word
    std::string  sSelText = GetSelectedText(SelectionHandling::CurrentWordIfSelectionIsEmpty);
    std::wstring selText = CUnicodeUtils::StdGetUnicode(sSelText);
    SearchReplace(cmd, L"$(SEL_TEXT)", selText);
    SearchReplace(cmd, L"$(SEL_TEXT_ESCAPED)", CEscapeUtils::EscapeString(selText));

    // split the command line into the command and the parameters
    std::wstring params;
    if (cmd[0] == '"')
    {
        cmd             = cmd.substr(1);
        size_t quotePos = cmd.find_first_of('"');
        if (quotePos == std::wstring::npos)
            return false;
        params = cmd.substr(quotePos + 1);
        cmd    = cmd.substr(0, quotePos);
    }
    else
    {
        size_t spacePos = cmd.find_first_of(' ');
        if (spacePos != std::wstring::npos)
        {
            params = cmd.substr(spacePos + 1);
            cmd    = cmd.substr(0, spacePos);
        }
    }

    SHELLEXECUTEINFO shi = {0};
    shi.cbSize           = sizeof(SHELLEXECUTEINFO);
    shi.fMask            = SEE_MASK_DOENVSUBST | SEE_MASK_UNICODE;
    shi.hwnd             = GetHwnd();
    shi.lpVerb           = L"open";
    shi.lpFile           = cmd.c_str();
    shi.lpParameters     = params.empty() ? nullptr : params.c_str();
    shi.lpDirectory      = tabDirPath.c_str();
    shi.nShow            = SW_SHOW;

    return !!ShellExecuteEx(&shi);
}

bool CCmdLaunchSearch::Execute()
{
    std::wstring sLaunch = CIniSettings::Instance().GetString(L"CustomLaunch", L"websearch", L"http://www.bing.com/search?q=$(SEL_TEXT_ESCAPED)");
    if (sLaunch.empty())
        sLaunch = L"http://www.bing.com/search?q=$(SEL_TEXT_ESCAPED)";
    return Launch(sLaunch);
}

CCmdLaunchCustom::CCmdLaunchCustom(UINT customId, void* obj)
    : LaunchBase(obj)
    , m_customId(customId)
{
    m_customCmdId = cmdLaunchCustom0 + customId;
    m_settingsID  = CStringUtils::Format(L"Command%u", customId);
}

void CCmdLaunchCustom::AfterInit()
{
    InvalidateUICommand(m_customCmdId, UI_INVALIDATIONS_PROPERTY, &UI_PKEY_Label);
    InvalidateUICommand(m_customCmdId, UI_INVALIDATIONS_STATE, &UI_PKEY_Enabled);
}

HRESULT CCmdLaunchCustom::IUICommandHandlerUpdateProperty(REFPROPERTYKEY key, const PROPVARIANT* /*pPropVarCurrentValue*/, PROPVARIANT* pPropVarNewValue)
{
    if (m_customId < 9)
    {
        // ugly, ugly workaround:
        // invalidating anything on a command only seems to work for the first command in a MenuGroup, at
        // least before the control has been shown. Since all these buttons are not shown until the dropdown-button
        // is clicked, all but the first button gets updated properly with the custom name text.
        // By invalidating the next command here we can work around that problem.
        InvalidateUICommand(m_customCmdId + 1, UI_INVALIDATIONS_PROPERTY, &UI_PKEY_Label);
        InvalidateUICommand(m_customCmdId + 1, UI_INVALIDATIONS_STATE, &UI_PKEY_Enabled);
    }
    if (UI_PKEY_Enabled == key)
    {
        size_t len = wcslen(CIniSettings::Instance().GetString(L"CustomLaunch", m_settingsID.c_str(), L""));
        return UIInitPropertyFromBoolean(UI_PKEY_Enabled, len > 0, pPropVarNewValue);
    }
    if (UI_PKEY_Label == key)
    {
        std::wstring settingsIDName = m_settingsID + L"Label";
        std::wstring commandLabel   = CIniSettings::Instance().GetString(L"CustomLaunch", settingsIDName.c_str(), L"");
        if (!commandLabel.empty())
            return UIInitPropertyFromString(UI_PKEY_Label, commandLabel.c_str(), pPropVarNewValue);
        else
        {
            ResString label(g_hRes, IDS_CUSTOMCOMMANDTITLE);
            auto      sLabel = CStringUtils::Format(label, m_customId);
            return UIInitPropertyFromString(UI_PKEY_Label, sLabel.c_str(), pPropVarNewValue);
        }
    }
    return E_FAIL;
}

CCustomCommandsDlg::CCustomCommandsDlg()
{
}

LRESULT CALLBACK CCustomCommandsDlg::DlgFunc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (uMsg)
    {
        case WM_INITDIALOG:
        {
            InitDialog(hwndDlg, IDI_BOWPAD);
            CTheme::Instance().SetThemeForDialog(*this, CTheme::Instance().IsDarkTheme());
            ResString sInfo(g_hRes, IDS_CUSTOMCOMMANDSINFO);
            SetDlgItemText(hwndDlg, IDC_INFO, sInfo);

            m_resizer.Init(hwndDlg);
            m_resizer.AddControl(hwndDlg, IDC_STATICWEB, RESIZER_TOPLEFT);
            m_resizer.AddControl(hwndDlg, IDC_WEBSEARCHCMD, RESIZER_TOPLEFTRIGHT);

            m_resizer.AddControl(hwndDlg, IDC_STATICNAME, RESIZER_TOPLEFT);
            m_resizer.AddControl(hwndDlg, IDC_STATICCMDLINE, RESIZER_TOPLEFTRIGHT);

            m_resizer.AddControl(hwndDlg, IDC_STATIC0, RESIZER_TOPLEFT);
            m_resizer.AddControl(hwndDlg, IDC_CUSTNAME0, RESIZER_TOPLEFT);
            m_resizer.AddControl(hwndDlg, IDC_CUSTCMD0, RESIZER_TOPLEFTRIGHT);
            m_resizer.AddControl(hwndDlg, IDC_STATIC1, RESIZER_TOPLEFT);
            m_resizer.AddControl(hwndDlg, IDC_CUSTNAME1, RESIZER_TOPLEFT);
            m_resizer.AddControl(hwndDlg, IDC_CUSTCMD1, RESIZER_TOPLEFTRIGHT);
            m_resizer.AddControl(hwndDlg, IDC_STATIC2, RESIZER_TOPLEFT);
            m_resizer.AddControl(hwndDlg, IDC_CUSTNAME2, RESIZER_TOPLEFT);
            m_resizer.AddControl(hwndDlg, IDC_CUSTCMD2, RESIZER_TOPLEFTRIGHT);
            m_resizer.AddControl(hwndDlg, IDC_STATIC3, RESIZER_TOPLEFT);
            m_resizer.AddControl(hwndDlg, IDC_CUSTNAME3, RESIZER_TOPLEFT);
            m_resizer.AddControl(hwndDlg, IDC_CUSTCMD3, RESIZER_TOPLEFTRIGHT);
            m_resizer.AddControl(hwndDlg, IDC_STATIC4, RESIZER_TOPLEFT);
            m_resizer.AddControl(hwndDlg, IDC_CUSTNAME4, RESIZER_TOPLEFT);
            m_resizer.AddControl(hwndDlg, IDC_CUSTCMD4, RESIZER_TOPLEFTRIGHT);
            m_resizer.AddControl(hwndDlg, IDC_STATIC5, RESIZER_TOPLEFT);
            m_resizer.AddControl(hwndDlg, IDC_CUSTNAME5, RESIZER_TOPLEFT);
            m_resizer.AddControl(hwndDlg, IDC_CUSTCMD5, RESIZER_TOPLEFTRIGHT);
            m_resizer.AddControl(hwndDlg, IDC_STATIC6, RESIZER_TOPLEFT);
            m_resizer.AddControl(hwndDlg, IDC_CUSTNAME6, RESIZER_TOPLEFT);
            m_resizer.AddControl(hwndDlg, IDC_CUSTCMD6, RESIZER_TOPLEFTRIGHT);
            m_resizer.AddControl(hwndDlg, IDC_STATIC7, RESIZER_TOPLEFT);
            m_resizer.AddControl(hwndDlg, IDC_CUSTNAME7, RESIZER_TOPLEFT);
            m_resizer.AddControl(hwndDlg, IDC_CUSTCMD7, RESIZER_TOPLEFTRIGHT);
            m_resizer.AddControl(hwndDlg, IDC_STATIC8, RESIZER_TOPLEFT);
            m_resizer.AddControl(hwndDlg, IDC_CUSTNAME8, RESIZER_TOPLEFT);
            m_resizer.AddControl(hwndDlg, IDC_CUSTCMD8, RESIZER_TOPLEFTRIGHT);
            m_resizer.AddControl(hwndDlg, IDC_STATIC9, RESIZER_TOPLEFT);
            m_resizer.AddControl(hwndDlg, IDC_CUSTNAME9, RESIZER_TOPLEFT);
            m_resizer.AddControl(hwndDlg, IDC_CUSTCMD9, RESIZER_TOPLEFTRIGHT);
            m_resizer.AddControl(hwndDlg, IDC_INFO, RESIZER_TOPLEFTBOTTOMRIGHT);

            m_resizer.AddControl(hwndDlg, IDOK, RESIZER_BOTTOMRIGHT);
            m_resizer.AddControl(hwndDlg, IDCANCEL, RESIZER_BOTTOMRIGHT);

            std::wstring sWebSearch = CIniSettings::Instance().GetString(L"CustomLaunch", L"websearch", L"http://www.google.com/search?q=$(SEL_TEXT_ESCAPED)");
            if (sWebSearch.empty())
                sWebSearch = L"http://www.google.com/search?q=$(SEL_TEXT_ESCAPED)";
            SetDlgItemText(*this, IDC_WEBSEARCHCMD, sWebSearch.c_str());

            std::wstring sName = CIniSettings::Instance().GetString(L"CustomLaunch", L"Command0Label", L"");
            SetDlgItemText(*this, IDC_CUSTNAME0, sName.c_str());
            std::wstring sCmd = CIniSettings::Instance().GetString(L"CustomLaunch", L"Command0", L"");
            SetDlgItemText(*this, IDC_CUSTCMD0, sCmd.c_str());
            sName = CIniSettings::Instance().GetString(L"CustomLaunch", L"Command1Label", L"");
            SetDlgItemText(*this, IDC_CUSTNAME1, sName.c_str());
            sCmd = CIniSettings::Instance().GetString(L"CustomLaunch", L"Command1", L"");
            SetDlgItemText(*this, IDC_CUSTCMD1, sCmd.c_str());
            sName = CIniSettings::Instance().GetString(L"CustomLaunch", L"Command2Label", L"");
            SetDlgItemText(*this, IDC_CUSTNAME2, sName.c_str());
            sCmd = CIniSettings::Instance().GetString(L"CustomLaunch", L"Command2", L"");
            SetDlgItemText(*this, IDC_CUSTCMD2, sCmd.c_str());
            sName = CIniSettings::Instance().GetString(L"CustomLaunch", L"Command3Label", L"");
            SetDlgItemText(*this, IDC_CUSTNAME3, sName.c_str());
            sCmd = CIniSettings::Instance().GetString(L"CustomLaunch", L"Command3", L"");
            SetDlgItemText(*this, IDC_CUSTCMD3, sCmd.c_str());
            sName = CIniSettings::Instance().GetString(L"CustomLaunch", L"Command4Label", L"");
            SetDlgItemText(*this, IDC_CUSTNAME4, sName.c_str());
            sCmd = CIniSettings::Instance().GetString(L"CustomLaunch", L"Command4", L"");
            SetDlgItemText(*this, IDC_CUSTCMD4, sCmd.c_str());
            sName = CIniSettings::Instance().GetString(L"CustomLaunch", L"Command5Label", L"");
            SetDlgItemText(*this, IDC_CUSTNAME5, sName.c_str());
            sCmd = CIniSettings::Instance().GetString(L"CustomLaunch", L"Command5", L"");
            SetDlgItemText(*this, IDC_CUSTCMD5, sCmd.c_str());
            sName = CIniSettings::Instance().GetString(L"CustomLaunch", L"Command6Label", L"");
            SetDlgItemText(*this, IDC_CUSTNAME6, sName.c_str());
            sCmd = CIniSettings::Instance().GetString(L"CustomLaunch", L"Command6", L"");
            SetDlgItemText(*this, IDC_CUSTCMD6, sCmd.c_str());
            sName = CIniSettings::Instance().GetString(L"CustomLaunch", L"Command7Label", L"");
            SetDlgItemText(*this, IDC_CUSTNAME7, sName.c_str());
            sCmd = CIniSettings::Instance().GetString(L"CustomLaunch", L"Command7", L"");
            SetDlgItemText(*this, IDC_CUSTCMD7, sCmd.c_str());
            sName = CIniSettings::Instance().GetString(L"CustomLaunch", L"Command8Label", L"");
            SetDlgItemText(*this, IDC_CUSTNAME8, sName.c_str());
            sCmd = CIniSettings::Instance().GetString(L"CustomLaunch", L"Command8", L"");
            SetDlgItemText(*this, IDC_CUSTCMD8, sCmd.c_str());
            sName = CIniSettings::Instance().GetString(L"CustomLaunch", L"Command9Label", L"");
            SetDlgItemText(*this, IDC_CUSTNAME9, sName.c_str());
            sCmd = CIniSettings::Instance().GetString(L"CustomLaunch", L"Command9", L"");
            SetDlgItemText(*this, IDC_CUSTCMD9, sCmd.c_str());
        }
            return FALSE;
        case WM_SIZE:
            m_resizer.DoResize(LOWORD(lParam), HIWORD(lParam));
            break;
        case WM_GETMINMAXINFO:
        {
            MINMAXINFO* mmi       = reinterpret_cast<MINMAXINFO*>(lParam);
            mmi->ptMinTrackSize.x = m_resizer.GetDlgRectScreen()->right;
            mmi->ptMinTrackSize.y = m_resizer.GetDlgRectScreen()->bottom;
            return 0;
        }
        case WM_COMMAND:
            return DoCommand(LOWORD(wParam));
        default:
            return FALSE;
    }
    return FALSE;
}

LRESULT CCustomCommandsDlg::DoCommand(int id)
{
    switch (id)
    {
        case IDOK:
        {
            auto name = GetDlgItemText(IDC_CUSTNAME0);
            CIniSettings::Instance().SetString(L"CustomLaunch", L"Command0Label", name.get());
            auto cmd = GetDlgItemText(IDC_CUSTCMD0);
            CIniSettings::Instance().SetString(L"CustomLaunch", L"Command0", cmd.get());
            name = GetDlgItemText(IDC_CUSTNAME1);
            CIniSettings::Instance().SetString(L"CustomLaunch", L"Command1Label", name.get());
            cmd = GetDlgItemText(IDC_CUSTCMD1);
            CIniSettings::Instance().SetString(L"CustomLaunch", L"Command1", cmd.get());
            name = GetDlgItemText(IDC_CUSTNAME2);
            CIniSettings::Instance().SetString(L"CustomLaunch", L"Command2Label", name.get());
            cmd = GetDlgItemText(IDC_CUSTCMD2);
            CIniSettings::Instance().SetString(L"CustomLaunch", L"Command2", cmd.get());
            name = GetDlgItemText(IDC_CUSTNAME3);
            CIniSettings::Instance().SetString(L"CustomLaunch", L"Command3Label", name.get());
            cmd = GetDlgItemText(IDC_CUSTCMD3);
            CIniSettings::Instance().SetString(L"CustomLaunch", L"Command3", cmd.get());
            name = GetDlgItemText(IDC_CUSTNAME4);
            CIniSettings::Instance().SetString(L"CustomLaunch", L"Command4Label", name.get());
            cmd = GetDlgItemText(IDC_CUSTCMD4);
            CIniSettings::Instance().SetString(L"CustomLaunch", L"Command4", cmd.get());
            name = GetDlgItemText(IDC_CUSTNAME5);
            CIniSettings::Instance().SetString(L"CustomLaunch", L"Command5Label", name.get());
            cmd = GetDlgItemText(IDC_CUSTCMD5);
            CIniSettings::Instance().SetString(L"CustomLaunch", L"Command5", cmd.get());
            name = GetDlgItemText(IDC_CUSTNAME6);
            CIniSettings::Instance().SetString(L"CustomLaunch", L"Command6Label", name.get());
            cmd = GetDlgItemText(IDC_CUSTCMD6);
            CIniSettings::Instance().SetString(L"CustomLaunch", L"Command6", cmd.get());
            name = GetDlgItemText(IDC_CUSTNAME7);
            CIniSettings::Instance().SetString(L"CustomLaunch", L"Command7Label", name.get());
            cmd = GetDlgItemText(IDC_CUSTCMD7);
            CIniSettings::Instance().SetString(L"CustomLaunch", L"Command7", cmd.get());
            name = GetDlgItemText(IDC_CUSTNAME8);
            CIniSettings::Instance().SetString(L"CustomLaunch", L"Command8Label", name.get());
            cmd = GetDlgItemText(IDC_CUSTCMD8);
            CIniSettings::Instance().SetString(L"CustomLaunch", L"Command8", cmd.get());
            name = GetDlgItemText(IDC_CUSTNAME9);
            CIniSettings::Instance().SetString(L"CustomLaunch", L"Command9Label", name.get());
            cmd = GetDlgItemText(IDC_CUSTCMD9);
            CIniSettings::Instance().SetString(L"CustomLaunch", L"Command9", cmd.get());

            cmd = GetDlgItemText(IDC_WEBSEARCHCMD);
            CIniSettings::Instance().SetString(L"CustomLaunch", L"websearch", cmd.get());

            EndDialog(*this, id);
        }
        break;
        case IDCANCEL:
            EndDialog(*this, id);
            break;
        default:
            break;
    }
    return 1;
}

bool CCmdCustomCommands::Execute()
{
    CCustomCommandsDlg dlg;
    if (dlg.DoModal(g_hRes, IDD_CUSTOMCOMMANDSDLG, GetHwnd()) == IDOK)
    {
        for (int i = 0; i < 10; ++i)
        {
            InvalidateUICommand(cmdLaunchCustom0 + i, UI_INVALIDATIONS_ALLPROPERTIES, &UI_PKEY_Label);
        }
    }
    return true;
}
