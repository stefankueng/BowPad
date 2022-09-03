// This file is part of BowPad.
//
// Copyright (C) 2013-2018, 2020-2022 - Stefan Kueng
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
// observe strange behavior before trying to fix it.
// * Activating the configuration editor causes a "surprising" change from the dark to light theme.
// * Changing theme while changing styles can leave the user unsure about the state of their changes.

#include "stdafx.h"
#include "CmdStyleConfigurator.h"
#include "BowPad.h"
#include "MainWindow.h"
#include "UnicodeUtils.h"
#include "LexStyles.h"
#include "Theme.h"
#include "OnOutOfScope.h"
#include "ResString.h"

#include <memory>

namespace
{
constexpr UINT WM_CURRENTSTYLECHANGED = (WM_APP + 1);
constexpr UINT WM_CURRENTDOCCHANGED   = (WM_APP + 2);

std::unique_ptr<CStyleConfiguratorDlg> g_pStyleConfiguratorDlg;
int                                    g_lastStyle = -1;
} // namespace

int CALLBACK CStyleConfiguratorDlg::EnumFontFamExProc(const LOGFONT* lpelfe, const TEXTMETRIC* /*lpntme*/, DWORD /*FontType*/, LPARAM lParam)
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
            CTheme::Instance().SetThemeForDialog(*this, CTheme::Instance().IsDarkTheme());
            auto languages  = CLexStyles::Instance().GetLanguages();
            auto hLangCombo = GetDlgItem(*this, IDC_LANGCOMBO);
            for (const auto& langName : languages)
                ComboBox_AddString(hLangCombo, langName.c_str());

            std::set<std::wstring> tempFonts;
            // Populates tempFonts.
            {
                HDC dc = GetWindowDC(*this);
                OnOutOfScope(
                    ReleaseDC(*this, dc););
                LOGFONT lf       = {0};
                lf.lfCharSet     = DEFAULT_CHARSET;
                lf.lfFaceName[0] = 0;
                EnumFontFamiliesEx(dc, &lf, EnumFontFamExProc, reinterpret_cast<LPARAM>(&tempFonts), 0);
            }
            m_fonts.push_back(L"");
            for (const auto& fontName : tempFonts)
                m_fonts.push_back(fontName);
            tempFonts.clear(); // We don't need them any more.
            auto hFontCombo = GetDlgItem(*this, IDC_FONTCOMBO);
            for (const auto& fontName : m_fonts)
                ComboBox_AddString(hFontCombo, fontName.c_str());

            auto hFontSizeCombo = GetDlgItem(*this, IDC_FONTSIZECOMBO);
            int  index          = ComboBox_AddString(hFontSizeCombo, L"");
            ComboBox_SetItemData(hFontSizeCombo, 0, 0);
            static constexpr int fontSizes[] = {5, 6, 7, 8, 9, 10, 11, 12, 14, 16, 18, 20, 22, 24, 26, 28};
            for (auto fontSize : fontSizes)
            {
                std::wstring s = std::to_wstring(fontSize);
                index          = static_cast<int>(ComboBox_AddString(hFontSizeCombo, s.c_str()));
                ComboBox_SetItemData(hFontSizeCombo, index, fontSize);
            }
            m_fgColor.ConvertToColorButton(*this, IDC_FG_BTN);
            m_bkColor.ConvertToColorButton(*this, IDC_BK_BTN);

            ResString sExtTooltip(g_hRes, IDS_EXTENSIONTOOLTIP);

            AddToolTip(IDC_EXTENSIONS, sExtTooltip);

            // Select the current language.
            if (HasActiveDocument())
            {
                const auto& doc = GetActiveDocument();
                ComboBox_SelectString(hLangCombo, -1, CUnicodeUtils::StdGetUnicode(doc.GetLanguage()).c_str());
            }
            DoCommand(IDC_LANGCOMBO, CBN_SELCHANGE);

            int style = static_cast<int>(Scintilla().StyleAt(Scintilla().CurrentPos()));
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
            int  langSel   = static_cast<int>(ComboBox_GetCurSel(GetDlgItem(*this, IDC_LANGCOMBO)));
            auto languages = CLexStyles::Instance().GetLanguages();
            if (langSel >= 0 && langSel < static_cast<int>(languages.size()))
            {
                auto currentLang = CUnicodeUtils::StdGetUTF8(languages[langSel]);
                if (HasActiveDocument())
                {
                    const auto& doc = GetActiveDocument();
                    if (doc.GetLanguage().compare(currentLang) == 0)
                    {
                        SelectStyle(static_cast<int>(wParam));
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

            int style = static_cast<int>(Scintilla().StyleAt(Scintilla().CurrentPos()));
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
                int  langSel    = ComboBox_GetCurSel(hLangCombo);
                auto languages  = CLexStyles::Instance().GetLanguages();
                if (langSel >= 0 && langSel < static_cast<int>(languages.size()))
                {
                    std::wstring currentLang = languages[langSel];
                    const auto&  lexData     = CLexStyles::Instance().GetLexerDataForLang(CUnicodeUtils::StdGetUTF8(currentLang));
                    auto         hStyleCombo = GetDlgItem(*this, IDC_STYLECOMBO);
                    ComboBox_ResetContent(hStyleCombo);
                    for (const auto& [lexId, styleData] : lexData.styles)
                    {
                        int styleSel = ComboBox_AddString(hStyleCombo, styleData.name.c_str());
                        ComboBox_SetItemData(hStyleCombo, styleSel, lexId);
                    }
                    int style = static_cast<int>(Scintilla().StyleAt(Scintilla().CurrentPos()));
                    SelectStyle(style);
                    std::wstring exts = CLexStyles::Instance().GetUserExtensionsForLanguage(currentLang);
                    SetDlgItemText(*this, IDC_EXTENSIONS, exts.c_str());
                    DialogEnableWindow(IDC_EXTENSIONS, true);
                    CheckDlgButton(m_hwnd, IDC_HIDE, CLexStyles::Instance().IsLanguageHidden(currentLang) ? BST_CHECKED : BST_UNCHECKED);
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
                int  langSel    = ComboBox_GetCurSel(hLangCombo);
                auto languages  = CLexStyles::Instance().GetLanguages();
                if (langSel >= 0 && langSel < static_cast<int>(languages.size()))
                {
                    auto        currentLang = CUnicodeUtils::StdGetUTF8(languages[langSel]);
                    auto        hStyleCombo = GetDlgItem(*this, IDC_STYLECOMBO);
                    int         styleSel    = ComboBox_GetCurSel(hStyleCombo);
                    int         styleKey    = static_cast<int>(ComboBox_GetItemData(hStyleCombo, styleSel));
                    const auto& lexData     = CLexStyles::Instance().GetLexerDataForLang(currentLang);

                    auto foundStyle = lexData.styles.find(styleKey);
                    if (foundStyle != lexData.styles.end())
                    {
                        ComboBox_SelectString(GetDlgItem(*this, IDC_FONTCOMBO), -1, foundStyle->second.fontName.c_str());
                        std::wstring fsize = std::to_wstring(foundStyle->second.fontSize);
                        if (foundStyle->second.fontSize == 0)
                            fsize.clear();
                        ComboBox_SelectString(GetDlgItem(*this, IDC_FONTSIZECOMBO), -1, fsize.c_str());
                        Button_SetCheck(GetDlgItem(*this, IDC_BOLDCHECK),
                                        (foundStyle->second.fontStyle & Fontstyle_Bold) ? BST_CHECKED : BST_UNCHECKED);
                        Button_SetCheck(GetDlgItem(*this, IDC_ITALICCHECK),
                                        (foundStyle->second.fontStyle & Fontstyle_Italic) ? BST_CHECKED : BST_UNCHECKED);
                        Button_SetCheck(GetDlgItem(*this, IDC_UNDERLINECHECK),
                                        (foundStyle->second.fontStyle & Fontstyle_Underlined) ? BST_CHECKED : BST_UNCHECKED);

                        // REVIEW:
                        // If the user changes a style from color A to color B,
                        // they would see B instantly in the editor if a document of that
                        // language/style is open.
                        // However without the code below if they leave this current style
                        // and return to it, the user will see the original uncommited
                        // color A again and not B. This is confusing to the user.
                        // So we try to use the active style data not the original style data,
                        // if possible.
                        COLORREF fgc = foundStyle->second.foregroundColor;
                        COLORREF bgc = foundStyle->second.backgroundColor;
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
        case IDC_HIDE:
        {
            auto hLangCombo = GetDlgItem(*this, IDC_LANGCOMBO);
            int  langSel    = ComboBox_GetCurSel(hLangCombo);
            auto languages  = CLexStyles::Instance().GetLanguages();
            if (langSel >= 0 && langSel < static_cast<int>(languages.size()))
            {
                auto        currentLang = CUnicodeUtils::StdGetUTF8(languages[langSel]);
                const auto& lexData     = CLexStyles::Instance().GetLexerDataForLang(currentLang);
                const auto  lexID       = lexData.id;
                const auto  hStyleCombo = GetDlgItem(*this, IDC_STYLECOMBO);
                const int   styleSel    = ComboBox_GetCurSel(hStyleCombo);
                const int   styleKey    = static_cast<int>(ComboBox_GetItemData(hStyleCombo, styleSel));
                bool        updateView  = false;
                if (HasActiveDocument())
                {
                    const auto& doc = GetActiveDocument();
                    if (doc.GetLanguage().compare(currentLang) == 0)
                        updateView = true;
                }
                switch (id)
                {
                    case IDC_HIDE:
                    {
                        CLexStyles::Instance().SetLanguageHidden(languages[langSel], IsDlgButtonChecked(m_hwnd, IDC_HIDE) != 0);
                        NotifyPlugins(L"cmdStyleConfigurator", 1);
                        break;
                    }
                    case IDC_FG_BTN:
                    {
                        auto fgcolor = m_fgColor.GetColor();
                        // When colors are applied by SetupLexer GetThemeColor is applied,
                        // so don't do it again here when storing the color.
                        CLexStyles::Instance().SetUserForeground(lexID, styleKey, fgcolor);
                        if (updateView)
                            Scintilla().StyleSetFore(styleKey, CTheme::Instance().GetThemeColor(fgcolor));
                        break;
                    }
                    case IDC_BK_BTN:
                    {
                        auto bgcolor = m_bkColor.GetColor();
                        CLexStyles::Instance().SetUserBackground(lexID, styleKey, bgcolor);
                        if (updateView)
                            Scintilla().StyleSetBack(styleKey, CTheme::Instance().GetThemeColor(bgcolor));
                        break;
                    }
                    case IDC_FONTCOMBO:
                    {
                        if (msg == CBN_SELCHANGE)
                        {
                            int fontSel = ComboBox_GetCurSel(GetDlgItem(*this, IDC_FONTCOMBO));
                            if (fontSel >= 0 && fontSel < static_cast<int>(m_fonts.size()))
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
                                            Scintilla().StyleSetFont(styleKey, "Consolas");
                                        }
                                        else
                                            Scintilla().StyleSetFont(styleKey, "Courier New");
                                    }
                                    else
                                    {
                                        Scintilla().StyleSetFont(styleKey, CUnicodeUtils::StdGetUTF8(font).c_str());
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
                            int  fontSizeSel    = ComboBox_GetCurSel(hFontSizeCombo);
                            if (fontSizeSel >= 0)
                            {
                                int fontSize = static_cast<int>(ComboBox_GetItemData(hFontSizeCombo, fontSizeSel));
                                CLexStyles::Instance().SetUserFontSize(lexID, styleKey, fontSize);
                                if (updateView)
                                {
                                    if (fontSize > 0)
                                        Scintilla().StyleSetSize(styleKey, fontSize);
                                    else
                                        Scintilla().StyleSetSize(styleKey, 10);
                                }
                            }
                        }
                    }
                    break;
                    case IDC_BOLDCHECK:
                    case IDC_ITALICCHECK:
                    case IDC_UNDERLINECHECK:
                    {
                        FontStyle fontStyle = Fontstyle_Normal;
                        if (Button_GetCheck(GetDlgItem(*this, IDC_BOLDCHECK)) == BST_CHECKED)
                            fontStyle = static_cast<FontStyle>(fontStyle | Fontstyle_Bold);
                        if (Button_GetCheck(GetDlgItem(*this, IDC_ITALICCHECK)) == BST_CHECKED)
                            fontStyle = static_cast<FontStyle>(fontStyle | Fontstyle_Italic);
                        if (Button_GetCheck(GetDlgItem(*this, IDC_UNDERLINECHECK)) == BST_CHECKED)
                            fontStyle = static_cast<FontStyle>(fontStyle | Fontstyle_Underlined);
                        CLexStyles::Instance().SetUserFontStyle(lexData.id, styleKey, fontStyle);
                        if (updateView)
                        {
                            Scintilla().StyleSetBold(styleKey, (fontStyle & Fontstyle_Bold) ? 1 : 0);
                            Scintilla().StyleSetItalic(styleKey, (fontStyle & Fontstyle_Italic) ? 1 : 0);
                            Scintilla().StyleSetUnderline(styleKey, (fontStyle & Fontstyle_Underlined) ? 1 : 0);
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

void CStyleConfiguratorDlg::SelectStyle(int style)
{
    auto hStyleCombo = GetDlgItem(*this, IDC_STYLECOMBO);
    int  styleCount  = ComboBox_GetCount(hStyleCombo);
    bool selected    = false;
    for (int i = 0; i < styleCount; ++i)
    {
        int styleCombo = static_cast<int>(ComboBox_GetItemData(hStyleCombo, i));
        if (style == styleCombo)
        {
            ComboBox_SetCurSel(hStyleCombo, i);
            DoCommand(IDC_STYLECOMBO, CBN_SELCHANGE);
            selected = true;
            break;
        }
    }
    if (!selected)
        ComboBox_SetCurSel(hStyleCombo, 0);
}

CCmdStyleConfigurator::CCmdStyleConfigurator(void* obj)
    : ICommand(obj)
{
}

CCmdStyleConfigurator::~CCmdStyleConfigurator()
{
    if (g_pStyleConfiguratorDlg != nullptr)
    {
        PostMessage(*g_pStyleConfiguratorDlg, WM_CLOSE, 0, 0);
    }
}

bool CCmdStyleConfigurator::Execute()
{
    if (g_pStyleConfiguratorDlg == nullptr)
        g_pStyleConfiguratorDlg = std::make_unique<CStyleConfiguratorDlg>(m_pMainWindow);

    g_pStyleConfiguratorDlg->ShowModeless(g_hRes, IDD_STYLECONFIGURATOR, GetHwnd());

    return true;
}

void CCmdStyleConfigurator::ScintillaNotify(SCNotification* pScn)
{
    if (pScn->nmhdr.code == SCN_UPDATEUI)
    {
        if (g_pStyleConfiguratorDlg)
        {
            if (IsWindowVisible(*g_pStyleConfiguratorDlg))
            {
                int style = static_cast<int>(Scintilla().StyleAt(Scintilla().CurrentPos()));
                if (style != g_lastStyle)
                {
                    SendMessage(*g_pStyleConfiguratorDlg, WM_CURRENTSTYLECHANGED, style, 0);
                    g_lastStyle = style;
                }
            }
        }
    }
}

void CCmdStyleConfigurator::TabNotify(TBHDR* ptbHdr)
{
    if (g_pStyleConfiguratorDlg != nullptr && ptbHdr->hdr.code == TCN_SELCHANGE)
    {
        SendMessage(*g_pStyleConfiguratorDlg, WM_CURRENTDOCCHANGED, 0, 0);
    }
}
