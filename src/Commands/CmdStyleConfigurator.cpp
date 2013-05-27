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
#include "stdafx.h"
#include "CmdStyleConfigurator.h"
#include "BowPad.h"
#include "MainWindow.h"
#include "UnicodeUtils.h"
#include "StringUtils.h"
#include "LexStyles.h"

static std::string sFindString;
static int         nSearchFlags;

#define WM_CURRENTSTYLECHANGED (WM_APP + 1)
#define WM_CURRENTDOCCHANGED   (WM_APP + 2)

int CALLBACK CStyleConfiguratorDlg::EnumFontFamExProc(const LOGFONT *lpelfe, const TEXTMETRIC * /*lpntme*/, DWORD /*FontType*/, LPARAM lParam)
{
    // only show the default and ansi fonts
    if ((lpelfe->lfCharSet == DEFAULT_CHARSET) || (lpelfe->lfCharSet == ANSI_CHARSET))
    {
        // filter out fonts that start with a '@'
        if (lpelfe->lfFaceName[0] != '@')
        {
            CStyleConfiguratorDlg * pThis = reinterpret_cast<CStyleConfiguratorDlg*>(lParam);
            pThis->m_tempFonts.insert(lpelfe->lfFaceName);
        }
    }
    return TRUE;
}


CStyleConfiguratorDlg::CStyleConfiguratorDlg(void * obj)
    : ICommand(obj)
{
}

CStyleConfiguratorDlg::~CStyleConfiguratorDlg(void)
{
}

LRESULT CStyleConfiguratorDlg::DlgFunc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (uMsg)
    {
    case WM_INITDIALOG:
        {
            InitDialog(hwndDlg, IDI_BOWPAD);
            auto languages = CLexStyles::Instance().GetLanguages();
            for (auto l:languages)
                SendDlgItemMessage(*this, IDC_LANGCOMBO, CB_ADDSTRING, 0, (LPARAM)l.c_str());

            HDC dc = GetWindowDC(*this);
            LOGFONT lf = {0};
            lf.lfCharSet = DEFAULT_CHARSET;
            lf.lfFaceName[0] = 0;
            m_fonts.push_back(L"");
            EnumFontFamiliesEx(dc, &lf, EnumFontFamExProc, (LPARAM)this, 0);
            ReleaseDC(*this, dc);
            for (auto it:m_tempFonts)
                m_fonts.push_back(it);
            for (auto it:m_fonts)
                SendDlgItemMessage(*this, IDC_FONTCOMBO, CB_ADDSTRING, 0, (LPARAM)it.c_str());

            int index = (int)SendDlgItemMessage(*this, IDC_FONTSIZECOMBO, CB_ADDSTRING, 0, (LPARAM)L"");
            SendDlgItemMessage(*this, IDC_FONTSIZECOMBO, CB_SETITEMDATA, 0, 0);
            int fontsizes[] = {5,6,7,8,9,10,11,12,14,16,18,20,22,24,26,28};
            for (auto i:fontsizes)
            {
                std::wstring s = CStringUtils::Format(L"%d", i);
                index = (int)SendDlgItemMessage(*this, IDC_FONTSIZECOMBO, CB_ADDSTRING, 0, (LPARAM)s.c_str());
                SendDlgItemMessage(*this, IDC_FONTSIZECOMBO, CB_SETITEMDATA, index, i);
            }
            m_fgColor.ConvertToColorButton(hwndDlg, IDC_FG_BTN);
            m_bkColor.ConvertToColorButton(hwndDlg, IDC_BK_BTN);

            // select the current language
            CDocument doc = GetDocument(GetCurrentTabIndex());
            SendDlgItemMessage(*this, IDC_LANGCOMBO, CB_SELECTSTRING, (WPARAM)-1, (LPARAM)doc.m_language.c_str());
            DoCommand(IDC_LANGCOMBO, CBN_SELCHANGE);

            int style = (int)ScintillaCall(SCI_GETSTYLEAT, ScintillaCall(SCI_GETCURRENTPOS));
            SelectStyle(style);
        }
        return FALSE;
    case WM_ACTIVATE:
            SetTransparency(wParam ? 255 : 150);
        break;
    case WM_COMMAND:
        return DoCommand(LOWORD(wParam), HIWORD(wParam));
    case WM_CURRENTSTYLECHANGED:
        {
            int index = (int)SendDlgItemMessage(*this, IDC_LANGCOMBO, CB_GETCURSEL, 0, 0);
            auto languages = CLexStyles::Instance().GetLanguages();
            if ((index >= 0) && (index < languages.size()))
            {
                std::wstring currentLang = languages[index];
                CDocument doc = GetDocument(GetCurrentTabIndex());
                if (doc.m_language.compare(currentLang) == 0)
                {
                    SelectStyle((int)wParam);
                }
            }
        }
        break;
    case WM_CURRENTDOCCHANGED:
        {
            CDocument doc = GetDocument(GetCurrentTabIndex());
            SendDlgItemMessage(*this, IDC_LANGCOMBO, CB_SELECTSTRING, (WPARAM)-1, (LPARAM)doc.m_language.c_str());
            DoCommand(IDC_LANGCOMBO, CBN_SELCHANGE);

            int style = (int)ScintillaCall(SCI_GETSTYLEAT, ScintillaCall(SCI_GETCURRENTPOS));
            SelectStyle(style);
        }
        break;
    default:
        return FALSE;
    }
    return FALSE;
}

LRESULT CStyleConfiguratorDlg::DoCommand(int id, int msg)
{
    switch (id)
    {
    case IDOK:
        {
            CLexStyles::Instance().SaveUserData();
            CDocument doc = GetDocument(GetCurrentTabIndex());
            SetupLexerForLang(doc.m_language);
            ShowWindow(*this, SW_HIDE);
        }
        break;
    case IDCANCEL:
        {
            CLexStyles::Instance().ResetUserData();
            CDocument doc = GetDocument(GetCurrentTabIndex());
            SetupLexerForLang(doc.m_language);
            ShowWindow(*this, SW_HIDE);
        }
        break;
    case IDC_LANGCOMBO:
        {
            if (msg == CBN_SELCHANGE)
            {
                int index = (int)SendDlgItemMessage(*this, IDC_LANGCOMBO, CB_GETCURSEL, 0, 0);
                auto languages = CLexStyles::Instance().GetLanguages();
                if ((index >= 0) && (index < languages.size()))
                {
                    std::wstring currentLang = languages[index];
                    auto lexData = CLexStyles::Instance().GetLexerDataForLang(CUnicodeUtils::StdGetUTF8(currentLang));
                    SendDlgItemMessage(*this, IDC_STYLECOMBO, CB_RESETCONTENT, 0, 0);
                    for (auto style:lexData.Styles)
                    {
                        index = (int)SendDlgItemMessage(*this, IDC_STYLECOMBO, CB_ADDSTRING, 0, (LPARAM)style.second.Name.c_str());
                        SendDlgItemMessage(*this, IDC_STYLECOMBO, CB_SETITEMDATA, index, style.first);
                    }
                    std::wstring exts = CLexStyles::Instance().GetUserExtensionsForLanguage(currentLang);
                    SetDlgItemText(*this, IDC_EXTENSIONS, exts.c_str());
                    DialogEnableWindow(IDC_EXTENSIONS, true);
                }
                else
                {
                    SetDlgItemText(*this, IDC_EXTENSIONS, L"");
                    DialogEnableWindow(IDC_EXTENSIONS, false);
                }
            }
        }
        break;
    case IDC_STYLECOMBO:
        {
            if (msg == CBN_SELCHANGE)
            {
                int index = (int)SendDlgItemMessage(*this, IDC_LANGCOMBO, CB_GETCURSEL, 0, 0);
                auto languages = CLexStyles::Instance().GetLanguages();
                if ((index >= 0) && (index < languages.size()))
                {
                    auto lexData = CLexStyles::Instance().GetLexerDataForLang(CUnicodeUtils::StdGetUTF8(languages[index]));
                    index = (int)SendDlgItemMessage(*this, IDC_STYLECOMBO, CB_GETCURSEL, 0, 0);
                    int styleIndex = (int)SendDlgItemMessage(*this, IDC_STYLECOMBO, CB_GETITEMDATA, index, 0);

                    auto foundStyle = lexData.Styles.find(styleIndex);
                    if (foundStyle != lexData.Styles.end())
                    {
                        SendDlgItemMessage(*this, IDC_FONTCOMBO, CB_SELECTSTRING, (WPARAM)-1, (LPARAM)foundStyle->second.FontName.c_str());
                        std::wstring fsize = CStringUtils::Format(L"%d", foundStyle->second.FontSize);
                        if (foundStyle->second.FontSize == 0)
                            fsize.clear();
                        SendDlgItemMessage(*this, IDC_FONTSIZECOMBO, CB_SELECTSTRING, (WPARAM)-1, (LPARAM)fsize.c_str());

                        SendDlgItemMessage(*this, IDC_BOLDCHECK,      BM_SETCHECK, foundStyle->second.FontStyle & FONTSTYLE_BOLD       ? BST_CHECKED : BST_UNCHECKED, NULL);
                        SendDlgItemMessage(*this, IDC_ITALICCHECK,    BM_SETCHECK, foundStyle->second.FontStyle & FONTSTYLE_ITALIC     ? BST_CHECKED : BST_UNCHECKED, NULL);
                        SendDlgItemMessage(*this, IDC_UNDERLINECHECK, BM_SETCHECK, foundStyle->second.FontStyle & FONTSTYLE_UNDERLINED ? BST_CHECKED : BST_UNCHECKED, NULL);

                        m_fgColor.SetColor(foundStyle->second.ForegroundColor);
                        m_bkColor.SetColor(foundStyle->second.BackgroundColor);
                    }
                }
            }
        }
        break;
    case IDC_FG_BTN:
    case IDC_BK_BTN:
    case IDC_FONTCOMBO:
    case IDC_FONTSIZECOMBO:
    case IDC_BOLDCHECK:
    case IDC_ITALICCHECK:
    case IDC_UNDERLINECHECK:
    case IDC_EXTENSIONS:
        {
            int index = (int)SendDlgItemMessage(*this, IDC_LANGCOMBO, CB_GETCURSEL, 0, 0);
            auto languages = CLexStyles::Instance().GetLanguages();
            if ((index >= 0) && (index < languages.size()))
            {
                std::wstring currentLang = languages[index];
                auto lexData = CLexStyles::Instance().GetLexerDataForLang(CUnicodeUtils::StdGetUTF8(currentLang));
                index = (int)SendDlgItemMessage(*this, IDC_STYLECOMBO, CB_GETCURSEL, 0, 0);
                int styleIndex = (int)SendDlgItemMessage(*this, IDC_STYLECOMBO, CB_GETITEMDATA, index, 0);
                bool updateView = false;
                CDocument doc = GetDocument(GetCurrentTabIndex());
                if (doc.m_language.compare(currentLang) == 0)
                    updateView = true;
                switch (id)
                {
                case IDC_FG_BTN:
                    CLexStyles::Instance().SetUserForeground(lexData.ID, styleIndex, m_fgColor.GetColor());
                    if (updateView)
                        ScintillaCall(SCI_STYLESETFORE, styleIndex, m_fgColor.GetColor());
                    break;
                case IDC_BK_BTN:
                    CLexStyles::Instance().SetUserBackground(lexData.ID, styleIndex, m_bkColor.GetColor());
                    if (updateView)
                        ScintillaCall(SCI_STYLESETBACK, styleIndex, m_bkColor.GetColor());
                    break;
                case IDC_FONTCOMBO:
                    {
                        if (msg == CBN_SELCHANGE)
                        {
                            int selInd = (int)SendDlgItemMessage(*this, IDC_FONTCOMBO, CB_GETCURSEL, 0, 0);
                            if ((selInd >= 0) && (selInd < m_fonts.size()))
                            {
                                std::wstring font = m_fonts[selInd];
                                CLexStyles::Instance().SetUserFont(lexData.ID, styleIndex, font);
                                if (updateView)
                                {
                                    if (font.empty())
                                    {
                                        HFONT hFont = CreateFont(0, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, FIXED_PITCH, L"Consolas");
                                        if (hFont)
                                        {
                                            DeleteObject(hFont);
                                            ScintillaCall(SCI_STYLESETFONT, styleIndex, (LPARAM)"Consolas");
                                        }
                                        else
                                            ScintillaCall(SCI_STYLESETFONT, styleIndex, (LPARAM)"Courier New");
                                    }
                                    else
                                        ScintillaCall(SCI_STYLESETFONT, styleIndex, (sptr_t)CUnicodeUtils::StdGetUTF8(font).c_str());
                                }
                            }
                        }
                    }
                    break;
                case IDC_FONTSIZECOMBO:
                    {
                        if (msg == CBN_SELCHANGE)
                        {
                            int selInd = (int)SendDlgItemMessage(*this, IDC_FONTSIZECOMBO, CB_GETCURSEL, 0, 0);
                            if (selInd >= 0)
                            {
                                int size = (int)SendDlgItemMessage(*this, IDC_FONTSIZECOMBO, CB_GETITEMDATA, selInd, 0);
                                CLexStyles::Instance().SetUserFontSize(lexData.ID, styleIndex, size);
                                if (updateView)
                                {
                                    if (size)
                                        ScintillaCall(SCI_STYLESETSIZE, styleIndex, size);
                                    else
                                        ScintillaCall(SCI_STYLESETSIZE, styleIndex, 10);
                                }
                            }
                        }
                    }
                    break;
                case IDC_BOLDCHECK:
                case IDC_ITALICCHECK:
                case IDC_UNDERLINECHECK:
                    {
                        FontStyle fstyle = FONTSTYLE_NORMAL;
                        if (SendDlgItemMessage(*this, IDC_BOLDCHECK, BM_GETCHECK, 0, NULL) == BST_CHECKED)
                            fstyle = (FontStyle)(fstyle | FONTSTYLE_BOLD);
                        if (SendDlgItemMessage(*this, IDC_ITALICCHECK, BM_GETCHECK, 0, NULL) == BST_CHECKED)
                            fstyle = (FontStyle)(fstyle | FONTSTYLE_ITALIC);
                        if (SendDlgItemMessage(*this, IDC_UNDERLINECHECK, BM_GETCHECK, 0, NULL) == BST_CHECKED)
                            fstyle = (FontStyle)(fstyle | FONTSTYLE_UNDERLINED);
                        CLexStyles::Instance().SetUserFontStyle(lexData.ID, styleIndex, fstyle);
                        if (updateView)
                        {
                            ScintillaCall(SCI_STYLESETBOLD, styleIndex, fstyle & FONTSTYLE_BOLD ? 1 : 0);
                            ScintillaCall(SCI_STYLESETITALIC, styleIndex, fstyle & FONTSTYLE_ITALIC ? 1 : 0);
                            ScintillaCall(SCI_STYLESETUNDERLINE, styleIndex, fstyle & FONTSTYLE_UNDERLINED ? 1 : 0);
                        }
                    }
                    break;
                case IDC_EXTENSIONS:
                    {
                        if (msg == EN_KILLFOCUS)
                        {
                            auto extText = GetDlgItemText(IDC_EXTENSIONS);
                            CLexStyles::Instance().SetUserExt(extText.get(), currentLang);
                        }
                    }
                    break;
                }
            }
        }
        break;
    }
    return 1;
}

void CStyleConfiguratorDlg::SelectStyle( int style )
{
    int styleCount = (int)SendDlgItemMessage(*this, IDC_STYLECOMBO, CB_GETCOUNT, 0, 0);
    for (int i = 0; i < styleCount; ++i)
    {
        int stylec = (int)SendDlgItemMessage(*this, IDC_STYLECOMBO, CB_GETITEMDATA, i, 0);
        if (style == stylec)
        {
            SendDlgItemMessage(*this, IDC_STYLECOMBO, CB_SETCURSEL, i, 0);
            DoCommand(IDC_STYLECOMBO, CBN_SELCHANGE);
        }
    }
}

bool CCmdStyleConfigurator::Execute()
{
    if (m_pStyleConfiguratorDlg == nullptr)
        m_pStyleConfiguratorDlg = new CStyleConfiguratorDlg(m_Obj);

    m_pStyleConfiguratorDlg->ShowModeless(hInst, IDD_STYLECONFIGURATOR, GetHwnd());

    return true;
}

void CCmdStyleConfigurator::ScintillaNotify( Scintilla::SCNotification * pScn )
{
    if (pScn->nmhdr.code == SCN_UPDATEUI)
    {
        if (m_pStyleConfiguratorDlg)
        {
            if (IsWindowVisible(*m_pStyleConfiguratorDlg))
            {
                static int lastStyle = -1;
                int style = (int)ScintillaCall(SCI_GETSTYLEAT, ScintillaCall(SCI_GETCURRENTPOS));
                if (style != lastStyle)
                {
                    SendMessage(*m_pStyleConfiguratorDlg, WM_CURRENTSTYLECHANGED, style, 0);
                    lastStyle = style;
                }
            }
        }
    }
}

void CCmdStyleConfigurator::TabNotify( TBHDR * ptbhdr )
{
    if ((m_pStyleConfiguratorDlg) && (ptbhdr->hdr.code == TCN_SELCHANGE))
    {
        SendMessage(*m_pStyleConfiguratorDlg, WM_CURRENTDOCCHANGED, 0, 0);
    }
}

