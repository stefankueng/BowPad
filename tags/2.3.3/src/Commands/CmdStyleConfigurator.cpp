// This file is part of BowPad.
//
// Copyright (C) 2013-2017 - Stefan Kueng
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

// OVERVIEW:
// This component allows the user to change BowPad's styles.
// The user can choose the style they want to edit by selecting it from the syle combo box.
// Alternatively the user can click an item in the active editor window and the
// dialog will update itself to show the details of the style that was clicked on.
// Clicking either color button makes a modal color dialog appear so the user can change
// the styles colors. The modal color dialog that opens is standard but hacked to allow the user
// to click any number of colors and view the result of each one immediately without having
// to click ok for each color first.
// The user can click on other styles while the color modal dialog box is open and the
// modeless configuration dialog underneath will move on to track the new style.
// After changing N styles the user can click OK to accept all their N
// style changes and close the dialog or click Cancel to reject them all.

// Some things to be aware of:
// * Due to how the Windows Color Control appears to work, running this component
// in the debugger may not reflect how things will work when in release mode.
// This may be because the windows control capture the focus or cursor or
// something else but be aware of that and test in release mode if you
// observe strange behaviour before trying to fix it.
// * Activating the configuration editor causes a "surprising" change from the dark to light theme.
// * Changing theme while changing styles can leave the user usure about the state of their changes.

#include "stdafx.h"
#include "CmdStyleConfigurator.h"
#include "BowPad.h"
#include "MainWindow.h"
#include "UnicodeUtils.h"
#include "StringUtils.h"
#include "LexStyles.h"
#include "Theme.h"
#include "OnOutOfScope.h"

#include <memory>

namespace
{
    const UINT WM_CURRENTSTYLECHANGED = (WM_APP + 1);
    const UINT WM_CURRENTDOCCHANGED = (WM_APP + 2);

    std::unique_ptr<CStyleConfiguratorDlg> g_pStyleConfiguratorDlg;
    int g_lastStyle = -1;
}

int CALLBACK CStyleConfiguratorDlg::EnumFontFamExProc(const LOGFONT *lpelfe, const TEXTMETRIC * /*lpntme*/, DWORD /*FontType*/, LPARAM lParam)
{
    // only show the default and ansi fonts
    if (lpelfe->lfCharSet == DEFAULT_CHARSET || lpelfe->lfCharSet == ANSI_CHARSET)
    {
        // filter out fonts that start with a '@'
        if (lpelfe->lfFaceName[0] != '@')
        {
            auto& tempFonts = *reinterpret_cast<std::set<std::wstring>*>(lParam);
            tempFonts.insert(lpelfe->lfFaceName);
        }
    }
    return TRUE;
}


CStyleConfiguratorDlg::CStyleConfiguratorDlg(void* obj)
    : ICommand(obj)
{
}

LRESULT CStyleConfiguratorDlg::DlgFunc(HWND /*hwndDlg*/, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (uMsg)
    {
    case WM_NCDESTROY:
        g_pStyleConfiguratorDlg.reset();
        g_lastStyle = -1;
        break;
    case WM_INITDIALOG:
        {
            InitDialog(*this, IDI_BOWPAD);
            auto languages = CLexStyles::Instance().GetLanguages();
            auto hLangCombo = GetDlgItem(*this, IDC_LANGCOMBO);
            for (const auto& langName : languages)
                ComboBox_AddString(hLangCombo, langName.c_str());

            std::set<std::wstring> tempFonts;
            // Populates tempFonts.
            {
                HDC dc = GetWindowDC(*this);
                OnOutOfScope(
                    ReleaseDC(*this, dc);
                );
                LOGFONT lf = { 0 };
                lf.lfCharSet = DEFAULT_CHARSET;
                lf.lfFaceName[0] = 0;
                EnumFontFamiliesEx(dc, &lf, EnumFontFamExProc, (LPARAM)& tempFonts, 0);
            }
            m_fonts.push_back(L"");
            for (const auto& fontName : tempFonts)
                m_fonts.push_back(fontName);
            tempFonts.clear(); // We don't need them any more.
            auto hFontCombo = GetDlgItem(*this, IDC_FONTCOMBO);
            for (const auto& fontName : m_fonts)
                ComboBox_AddString(hFontCombo, fontName.c_str());

            auto hFontSizeCombo = GetDlgItem(*this, IDC_FONTSIZECOMBO);
            int index = ComboBox_AddString(hFontSizeCombo, L"");
            ComboBox_SetItemData(hFontSizeCombo, 0, 0);
            static constexpr int fontsizes[] = { 5,6,7,8,9,10,11,12,14,16,18,20,22,24,26,28 };
            for (auto fontSize : fontsizes)
            {
                std::wstring s = std::to_wstring(fontSize);
                index = (int)ComboBox_AddString(hFontSizeCombo, s.c_str());
                ComboBox_SetItemData(hFontSizeCombo, index, fontSize);
            }
            m_fgColor.ConvertToColorButton(*this, IDC_FG_BTN);
            m_bkColor.ConvertToColorButton(*this, IDC_BK_BTN);

            ResString sExtTooltip(hRes, IDS_EXTENSIONTOOLTIP);

            AddToolTip(IDC_EXTENSIONS, sExtTooltip);

            // Select the current language.
            if (HasActiveDocument())
            {
                const auto& doc = GetActiveDocument();
                ComboBox_SelectString(hLangCombo, -1, CUnicodeUtils::StdGetUnicode(doc.GetLanguage()).c_str());
            }
            DoCommand(IDC_LANGCOMBO, CBN_SELCHANGE);

            int style = (int)ScintillaCall(SCI_GETSTYLEAT, ScintillaCall(SCI_GETCURRENTPOS));
            SelectStyle(style);
        }
        return FALSE;
    case WM_ACTIVATE:
            //SetTransparency((wParam == WA_INACTIVE) ? 200 : 255);
        break;

    case WM_COMMAND:
        return DoCommand(LOWORD(wParam), HIWORD(wParam));

    case WM_CURRENTSTYLECHANGED:
        {
            // If you change one style then move to another style, the cancel
            // ability must be reset so it doesn't apply to the new style.
            m_fgColor.Reset();
            m_bkColor.Reset();
            int langSel = (int)ComboBox_GetCurSel(GetDlgItem(*this, IDC_LANGCOMBO));
            auto languages = CLexStyles::Instance().GetLanguages();
            if (langSel >= 0 && langSel < (int)languages.size())
            {
                auto currentLang = CUnicodeUtils::StdGetUTF8(languages[langSel]);
                if (HasActiveDocument())
                {
                    const auto& doc = GetActiveDocument();
                    if (doc.GetLanguage().compare(currentLang) == 0)
                    {
                        SelectStyle((int)wParam);
                    }
                }
            }
        }
        break;
    case WM_CURRENTDOCCHANGED:
        {
            if (HasActiveDocument())
            {
                const auto& doc = GetActiveDocument();
                ComboBox_SelectString(GetDlgItem(*this, IDC_LANGCOMBO), -1, CUnicodeUtils::StdGetUnicode(doc.GetLanguage()).c_str());
            }
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
        if (msg == BN_CLICKED)
        {
            CLexStyles::Instance().SaveUserData();
            if (HasActiveDocument())
            {
                const auto& doc = GetActiveDocument();
                SetupLexerForLang(doc.GetLanguage());
            }
            DestroyWindow(*this);
        }
        break;
    case IDCANCEL:
        if (msg == BN_CLICKED)
        {
            CLexStyles::Instance().ResetUserData();
            if (HasActiveDocument())
            {
                const auto& doc = GetActiveDocument();
                SetupLexerForLang(doc.GetLanguage());
            }
            DestroyWindow(*this);
        }
        break;
    case IDC_LANGCOMBO:
        {
            if (msg == CBN_SELCHANGE)
            {
                auto hLangCombo = GetDlgItem(*this, IDC_LANGCOMBO);
                int langSel = ComboBox_GetCurSel(hLangCombo);
                auto languages = CLexStyles::Instance().GetLanguages();
                if (langSel >= 0 && langSel < (int)languages.size())
                {
                    std::wstring currentLang = languages[langSel];
                    const auto& lexData = CLexStyles::Instance().GetLexerDataForLang(CUnicodeUtils::StdGetUTF8(currentLang));
                    auto hStyleCombo = GetDlgItem(*this, IDC_STYLECOMBO);
                    ComboBox_ResetContent(hStyleCombo);
                    for (const auto& style:lexData.Styles)
                    {
                        int styleSel = ComboBox_AddString(hStyleCombo, style.second.Name.c_str());
                        ComboBox_SetItemData(hStyleCombo, styleSel, style.first);
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
                auto hLangCombo = GetDlgItem(*this, IDC_LANGCOMBO);
                int langSel = ComboBox_GetCurSel(hLangCombo);
                auto languages = CLexStyles::Instance().GetLanguages();
                if (langSel >= 0 && langSel < (int)languages.size())
                {
                    auto currentLang = CUnicodeUtils::StdGetUTF8(languages[langSel]);
                    auto hStyleCombo = GetDlgItem(*this, IDC_STYLECOMBO);
                    int styleSel = ComboBox_GetCurSel(hStyleCombo);
                    int styleKey = (int)ComboBox_GetItemData(hStyleCombo, styleSel);
                    const auto& lexData = CLexStyles::Instance().GetLexerDataForLang(currentLang);

                    auto foundStyle = lexData.Styles.find(styleKey);
                    if (foundStyle != lexData.Styles.end())
                    {
                        ComboBox_SelectString(GetDlgItem(*this, IDC_FONTCOMBO), -1, foundStyle->second.FontName.c_str());
                        std::wstring fsize = std::to_wstring(foundStyle->second.FontSize);
                        if (foundStyle->second.FontSize == 0)
                            fsize.clear();
                        ComboBox_SelectString(GetDlgItem(*this, IDC_FONTSIZECOMBO), -1, fsize.c_str());
                        Button_SetCheck(GetDlgItem(*this, IDC_BOLDCHECK),
                            (foundStyle->second.FontStyle & FONTSTYLE_BOLD) ? BST_CHECKED : BST_UNCHECKED);
                        Button_SetCheck(GetDlgItem(*this, IDC_ITALICCHECK),
                            (foundStyle->second.FontStyle & FONTSTYLE_ITALIC) ? BST_CHECKED : BST_UNCHECKED);
                        Button_SetCheck(GetDlgItem(*this, IDC_UNDERLINECHECK),
                            (foundStyle->second.FontStyle & FONTSTYLE_UNDERLINED) ? BST_CHECKED : BST_UNCHECKED);

                        // REVIEW:
                        // If the user changes a style from color A to color B,
                        // they would see B instantly in the editor if a document of that
                        // language/style is open.
                        // However without the code below if they leave this current style
                        // and return to it, the user will see the original uncommited
                        // color A again and not B. This is confusing to the user.
                        // So we try to use the active style data not the original style data,
                        // if possible.
                        COLORREF fgc = foundStyle->second.ForegroundColor;
                        COLORREF bgc = foundStyle->second.BackgroundColor;
                        m_fgColor.SetColor(fgc);
                        m_bkColor.SetColor(bgc);
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
            auto hLangCombo = GetDlgItem(*this, IDC_LANGCOMBO);
            int langSel = ComboBox_GetCurSel(hLangCombo);
            auto languages = CLexStyles::Instance().GetLanguages();
            if (langSel >= 0 && langSel < (int)languages.size())
            {
                auto currentLang = CUnicodeUtils::StdGetUTF8(languages[langSel]);
                const auto& lexData = CLexStyles::Instance().GetLexerDataForLang(currentLang);
                const auto lexID = lexData.ID;
                const auto hStyleCombo = GetDlgItem(*this, IDC_STYLECOMBO);
                const int styleSel = ComboBox_GetCurSel(hStyleCombo);
                const int styleKey = (int)ComboBox_GetItemData(hStyleCombo, styleSel);
                bool updateView = false;
                if (HasActiveDocument())
                {
                    const auto& doc = GetActiveDocument();
                    if (doc.GetLanguage().compare(currentLang) == 0)
                        updateView = true;
                }
                switch (id)
                {
                case IDC_FG_BTN:
                {
                    auto fgcolor = m_fgColor.GetColor();
                    // When colours are applied by SetupLexer GetThemeColor is applied,
                    // so don't do it again here when storing the color.
                    CLexStyles::Instance().SetUserForeground(lexID, styleKey, fgcolor);
                    if (updateView)
                        ScintillaCall(SCI_STYLESETFORE, styleKey, CTheme::Instance().GetThemeColor(fgcolor));
                    break;
                }
                case IDC_BK_BTN:
                {
                    auto bgcolor = m_bkColor.GetColor();
                    CLexStyles::Instance().SetUserBackground(lexID, styleKey, bgcolor);
                    if (updateView)
                        ScintillaCall(SCI_STYLESETBACK, styleKey, CTheme::Instance().GetThemeColor(bgcolor));
                    break;
                }
                case IDC_FONTCOMBO:
                    {
                        if (msg == CBN_SELCHANGE)
                        {
                            int fontSel = ComboBox_GetCurSel(GetDlgItem(*this, IDC_FONTCOMBO));
                            if (fontSel >= 0 && fontSel < (int)m_fonts.size())
                            {
                                std::wstring font = m_fonts[fontSel];
                                CLexStyles::Instance().SetUserFont(lexID, styleKey, font);
                                if (updateView)
                                {
                                    if (font.empty())
                                    {
                                        HFONT hFont = CreateFont(0, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, FIXED_PITCH, L"Consolas");
                                        if (hFont)
                                        {
                                            DeleteObject(hFont);
                                            ScintillaCall(SCI_STYLESETFONT, styleKey, (LPARAM)"Consolas");
                                        }
                                        else
                                            ScintillaCall(SCI_STYLESETFONT, styleKey, (LPARAM)"Courier New");
                                    }
                                    else
                                    {
                                        ScintillaCall(SCI_STYLESETFONT, styleKey, (sptr_t)CUnicodeUtils::StdGetUTF8(font).c_str());
                                    }
                                }
                            }
                        }
                    }
                    break;
                case IDC_FONTSIZECOMBO:
                    {
                        if (msg == CBN_SELCHANGE)
                        {
                            auto hFontSizeCombo = GetDlgItem(*this, IDC_FONTSIZECOMBO);
                            int fontSizeSel = ComboBox_GetCurSel(hFontSizeCombo);
                            if (fontSizeSel >= 0)
                            {
                                int fontSize = (int)ComboBox_GetItemData(hFontSizeCombo, fontSizeSel);
                                CLexStyles::Instance().SetUserFontSize(lexID, styleKey, fontSize);
                                if (updateView)
                                {
                                    if (fontSize > 0)
                                        ScintillaCall(SCI_STYLESETSIZE, styleKey, fontSize);
                                    else
                                        ScintillaCall(SCI_STYLESETSIZE, styleKey, 10);
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
                        if (Button_GetCheck(GetDlgItem(*this, IDC_BOLDCHECK)) == BST_CHECKED)
                            fstyle = (FontStyle)(fstyle | FONTSTYLE_BOLD);
                        if (Button_GetCheck(GetDlgItem(*this, IDC_ITALICCHECK)) == BST_CHECKED)
                            fstyle = (FontStyle)(fstyle | FONTSTYLE_ITALIC);
                        if (Button_GetCheck(GetDlgItem(*this, IDC_UNDERLINECHECK)) == BST_CHECKED)
                            fstyle = (FontStyle)(fstyle | FONTSTYLE_UNDERLINED);
                        CLexStyles::Instance().SetUserFontStyle(lexData.ID, styleKey, fstyle);
                        if (updateView)
                        {
                            ScintillaCall(SCI_STYLESETBOLD, styleKey, (fstyle & FONTSTYLE_BOLD) ? 1 : 0);
                            ScintillaCall(SCI_STYLESETITALIC, styleKey, (fstyle & FONTSTYLE_ITALIC) ? 1 : 0);
                            ScintillaCall(SCI_STYLESETUNDERLINE, styleKey, (fstyle & FONTSTYLE_UNDERLINED) ? 1 : 0);
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
    auto hStyleCombo = GetDlgItem(*this, IDC_STYLECOMBO);
    int styleCount = ComboBox_GetCount(hStyleCombo);
    for (int i = 0; i < styleCount; ++i)
    {
        int stylec = (int)ComboBox_GetItemData(hStyleCombo, i);
        if (style == stylec)
        {
            ComboBox_SetCurSel(hStyleCombo, i);
            DoCommand(IDC_STYLECOMBO, CBN_SELCHANGE);
        }
    }
}

CCmdStyleConfigurator::CCmdStyleConfigurator(void* obj)
    : ICommand(obj)
{
}

CCmdStyleConfigurator::~CCmdStyleConfigurator()
{
    if (g_pStyleConfiguratorDlg != nullptr)
    {
        PostMessage(*g_pStyleConfiguratorDlg, WM_CLOSE, WPARAM(0), LPARAM(0));
    }
}

bool CCmdStyleConfigurator::Execute()
{
    if (g_pStyleConfiguratorDlg == nullptr)
        g_pStyleConfiguratorDlg = std::make_unique<CStyleConfiguratorDlg>(m_pMainWindow);

    g_pStyleConfiguratorDlg->ShowModeless(hRes, IDD_STYLECONFIGURATOR, GetHwnd());

    return true;
}

void CCmdStyleConfigurator::ScintillaNotify( SCNotification* pScn )
{
    if (pScn->nmhdr.code == SCN_UPDATEUI)
    {
        if (g_pStyleConfiguratorDlg)
        {
            if (IsWindowVisible(*g_pStyleConfiguratorDlg))
            {
                int style = (int)ScintillaCall(SCI_GETSTYLEAT, ScintillaCall(SCI_GETCURRENTPOS));
                if (style != g_lastStyle)
                {
                    SendMessage(*g_pStyleConfiguratorDlg, WM_CURRENTSTYLECHANGED, style, 0);
                    g_lastStyle = style;
                }
            }
        }
    }
}

void CCmdStyleConfigurator::TabNotify( TBHDR* ptbhdr )
{
    if (g_pStyleConfiguratorDlg != nullptr && ptbhdr->hdr.code == TCN_SELCHANGE)
    {
        SendMessage(*g_pStyleConfiguratorDlg, WM_CURRENTDOCCHANGED, 0, 0);
    }
}

