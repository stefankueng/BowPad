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
#include "stdafx.h"
#include "CmdLaunch.h"
#include "BowPad.h"
#include <Shellapi.h>

bool LaunchBase::Launch( const std::wstring& cmdline )
{
    if (cmdline.empty() || !HasActiveDocument())
        return false;
    std::wstring cmd = cmdline;
    // replace the macros in the command line
    CDocument doc = GetActiveDocument();
    std::wstring tabpath = doc.m_path;
    if (PathFileExists(tabpath.c_str()))
        SearchReplace(cmd, L"$(TAB_PATH)", tabpath);
    else
        SearchReplace(cmd, L"$(TAB_PATH)", L"");
    tabpath = CPathUtils::GetParentDirectory(tabpath);
    if (PathFileExists(tabpath.c_str()))
        SearchReplace(cmd, L"$(TAB_DIR)", tabpath);
    else
        SearchReplace(cmd, L"$(TAB_DIR)", L"");

    SearchReplace(cmd, L"$(LINE)", CStringUtils::Format(L"%lld", ScintillaCall(SCI_LINEFROMPOSITION, ScintillaCall(SCI_GETCURRENTPOS))));
    SearchReplace(cmd, L"$(POS)", CStringUtils::Format(L"%lld", ScintillaCall(SCI_GETCURRENTPOS)));
    // find selected text or current word
    std::string sSelText;
    int selTextLen = (int)ScintillaCall(SCI_GETSELTEXT);
    std::unique_ptr<char[]> seltextbuffer(new char[selTextLen + 1]);
    ScintillaCall(SCI_GETSELTEXT, 0, (LPARAM)(char*)seltextbuffer.get());
    sSelText = seltextbuffer.get();
    if (sSelText.empty())
    {
        // get the current word instead
        long currentPos = (long)ScintillaCall(SCI_GETCURRENTPOS);
        long startPos   = (long)ScintillaCall(SCI_WORDSTARTPOSITION, currentPos);
        long endPos     = (long)ScintillaCall(SCI_WORDENDPOSITION, currentPos);
        std::unique_ptr<char[]> textbuf(new char[endPos - startPos + 1]);
        Scintilla::Sci_TextRange range;
        range.chrg.cpMin = startPos;
        range.chrg.cpMax = endPos;
        range.lpstrText = textbuf.get();
        ScintillaCall(SCI_GETTEXTRANGE, 0, (sptr_t)&range);
        sSelText = textbuf.get();
    }
    std::wstring selText = CUnicodeUtils::StdGetUnicode(sSelText);
    SearchReplace(cmd, L"$(SEL_TEXT)", selText);
    SearchReplace(cmd, L"$(SEL_TEXT_ESCAPED)", CEscapeUtils::EscapeString(selText));

    // split the command line into the command and the parameters
    std::wstring params;
    if (cmd[0] == '"')
    {
        cmd = cmd.substr(1);
        size_t quotepos = cmd.find_first_of('"');
        if (quotepos == std::wstring::npos)
            return false;
        params = cmd.substr(quotepos+1);
        cmd = cmd.substr(0, quotepos);
    }
    else
    {
        size_t spacepos = cmd.find_first_of(' ');
        if (spacepos != std::wstring::npos)
        {
            params = cmd.substr(spacepos+1);
            cmd = cmd.substr(0, spacepos);
        }
    }

    SHELLEXECUTEINFO shi = {0};
    shi.cbSize = sizeof(SHELLEXECUTEINFO);
    shi.fMask = SEE_MASK_DOENVSUBST|SEE_MASK_UNICODE;
    shi.hwnd = GetHwnd();
    shi.lpVerb = L"open";
    shi.lpFile = cmd.c_str();
    shi.lpParameters = params.empty() ? NULL : params.c_str();
    shi.lpDirectory = CPathUtils::GetParentDirectory(tabpath).c_str();
    shi.nShow = SW_SHOW;

    return !!ShellExecuteEx(&shi);
}

HRESULT CCmdLaunchCustom::IUICommandHandlerUpdateProperty( REFPROPERTYKEY key, const PROPVARIANT* /*ppropvarCurrentValue*/, PROPVARIANT* ppropvarNewValue )
{
    if (m_customId < 9)
    {
        // ugly, ugly workaround:
        // invalidating anything on a command only seems to work for the first command in a MenuGroup, at
        // least before the control has been shown. Since all these buttons are not shown until the dropdown-button
        // is clicked, all but the first button gets updated properly with the custom name text.
        // By invalidating the next command here we can work around that problem.
        InvalidateUICommand(m_customCmdId+1, UI_INVALIDATIONS_PROPERTY, &UI_PKEY_Label);
        InvalidateUICommand(m_customCmdId+1, UI_INVALIDATIONS_STATE, &UI_PKEY_Enabled);
    }
    if (UI_PKEY_Enabled == key)
    {
        size_t len = wcslen(CIniSettings::Instance().GetString(L"CustomLaunch", m_settingsID.c_str(), L""));
        return UIInitPropertyFromBoolean(UI_PKEY_Enabled, len > 0, ppropvarNewValue);
    }
    if (UI_PKEY_Label == key)
    {
        std::wstring settingsIDName = m_settingsID + L"Label";
        std::wstring commandLabel = CIniSettings::Instance().GetString(L"CustomLaunch", settingsIDName.c_str(), L"");
        if (!commandLabel.empty())
            return UIInitPropertyFromString(UI_PKEY_Label, commandLabel.c_str(), ppropvarNewValue);
        else
        {
            ResString label(hRes, cmdLaunchCustom0_LabelTitle_RESID + (m_customId * 4));
            return UIInitPropertyFromString(UI_PKEY_Label, label, ppropvarNewValue);
        }
    }
    return E_FAIL;
}

CCustomCommandsDlg::CCustomCommandsDlg()
{

}

CCustomCommandsDlg::~CCustomCommandsDlg( void )
{

}

LRESULT CALLBACK CCustomCommandsDlg::DlgFunc( HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam )
{
    UNREFERENCED_PARAMETER(lParam);
    switch (uMsg)
    {
    case WM_INITDIALOG:
        {
            InitDialog(hwndDlg, IDI_BOWPAD);
            ResString sInfo(hRes, IDS_CUSTOMCOMMANDSINFO);
            SetDlgItemText(hwndDlg, IDC_INFO, sInfo);

            m_resizer.Init(hwndDlg);
            m_resizer.AddControl(hwndDlg, IDC_STATICWEB, RESIZER_TOPLEFT);
            m_resizer.AddControl(hwndDlg, IDC_WEBSEARCHCMD, RESIZER_TOPLEFTRIGHT);

            m_resizer.AddControl(hwndDlg, IDC_STATICNAME, RESIZER_TOPLEFT);
            m_resizer.AddControl(hwndDlg, IDC_STATICCMDLINE, RESIZER_TOPLEFTRIGHT);

            m_resizer.AddControl(hwndDlg, IDC_STATIC0,      RESIZER_TOPLEFT);
            m_resizer.AddControl(hwndDlg, IDC_CUSTNAME0,    RESIZER_TOPLEFT);
            m_resizer.AddControl(hwndDlg, IDC_CUSTCMD0,     RESIZER_TOPLEFTRIGHT);
            m_resizer.AddControl(hwndDlg, IDC_STATIC1,      RESIZER_TOPLEFT);
            m_resizer.AddControl(hwndDlg, IDC_CUSTNAME1,    RESIZER_TOPLEFT);
            m_resizer.AddControl(hwndDlg, IDC_CUSTCMD1,     RESIZER_TOPLEFTRIGHT);
            m_resizer.AddControl(hwndDlg, IDC_STATIC2,      RESIZER_TOPLEFT);
            m_resizer.AddControl(hwndDlg, IDC_CUSTNAME2,    RESIZER_TOPLEFT);
            m_resizer.AddControl(hwndDlg, IDC_CUSTCMD2,     RESIZER_TOPLEFTRIGHT);
            m_resizer.AddControl(hwndDlg, IDC_STATIC3,      RESIZER_TOPLEFT);
            m_resizer.AddControl(hwndDlg, IDC_CUSTNAME3,    RESIZER_TOPLEFT);
            m_resizer.AddControl(hwndDlg, IDC_CUSTCMD3,     RESIZER_TOPLEFTRIGHT);
            m_resizer.AddControl(hwndDlg, IDC_STATIC4,      RESIZER_TOPLEFT);
            m_resizer.AddControl(hwndDlg, IDC_CUSTNAME4,    RESIZER_TOPLEFT);
            m_resizer.AddControl(hwndDlg, IDC_CUSTCMD4,     RESIZER_TOPLEFTRIGHT);
            m_resizer.AddControl(hwndDlg, IDC_STATIC5,      RESIZER_TOPLEFT);
            m_resizer.AddControl(hwndDlg, IDC_CUSTNAME5,    RESIZER_TOPLEFT);
            m_resizer.AddControl(hwndDlg, IDC_CUSTCMD5,     RESIZER_TOPLEFTRIGHT);
            m_resizer.AddControl(hwndDlg, IDC_STATIC6,      RESIZER_TOPLEFT);
            m_resizer.AddControl(hwndDlg, IDC_CUSTNAME6,    RESIZER_TOPLEFT);
            m_resizer.AddControl(hwndDlg, IDC_CUSTCMD6,     RESIZER_TOPLEFTRIGHT);
            m_resizer.AddControl(hwndDlg, IDC_STATIC7,      RESIZER_TOPLEFT);
            m_resizer.AddControl(hwndDlg, IDC_CUSTNAME7,    RESIZER_TOPLEFT);
            m_resizer.AddControl(hwndDlg, IDC_CUSTCMD7,     RESIZER_TOPLEFTRIGHT);
            m_resizer.AddControl(hwndDlg, IDC_STATIC8,      RESIZER_TOPLEFT);
            m_resizer.AddControl(hwndDlg, IDC_CUSTNAME8,    RESIZER_TOPLEFT);
            m_resizer.AddControl(hwndDlg, IDC_CUSTCMD8,     RESIZER_TOPLEFTRIGHT);
            m_resizer.AddControl(hwndDlg, IDC_STATIC9,      RESIZER_TOPLEFT);
            m_resizer.AddControl(hwndDlg, IDC_CUSTNAME9,    RESIZER_TOPLEFT);
            m_resizer.AddControl(hwndDlg, IDC_CUSTCMD9,     RESIZER_TOPLEFTRIGHT);
            m_resizer.AddControl(hwndDlg, IDC_INFO,         RESIZER_TOPLEFTBOTTOMRIGHT);

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
            MINMAXINFO * mmi = (MINMAXINFO*)lParam;
            mmi->ptMinTrackSize.x = m_resizer.GetDlgRectScreen()->right;
            mmi->ptMinTrackSize.y = m_resizer.GetDlgRectScreen()->bottom;
            return 0;
        }
        break;
    case WM_COMMAND:
        return DoCommand(LOWORD(wParam));
    default:
        return FALSE;
    }
    return FALSE;
}

LRESULT CCustomCommandsDlg::DoCommand( int id )
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
    }
    return 1;
}

bool CCmdCustomCommands::Execute()
{
    CCustomCommandsDlg dlg;
    if (dlg.DoModal(hRes, IDD_CUSTOMCOMMANDSDLG, GetHwnd())==IDOK)
    {
        for (int i = 0; i < 10; ++i)
        {
            InvalidateUICommand(cmdLaunchCustom0+i, UI_INVALIDATIONS_PROPERTY, &UI_PKEY_Label);
            InvalidateUICommand(cmdLaunchCustom0+i, UI_INVALIDATIONS_STATE, &UI_PKEY_Enabled);
        }
    }
    return true;
}
