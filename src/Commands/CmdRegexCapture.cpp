// This file is part of BowPad.
//
// Copyright (C) 2013-2022 - Stefan Kueng
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
#include "CmdRegexCapture.h"

#include "BowPad.h"
#include "ScintillaWnd.h"
#include "UnicodeUtils.h"
#include "StringUtils.h"
#include "ResString.h"
#include "Theme.h"

#include <regex>
#include <memory>
#include <sstream>

#include <dwmapi.h>
#pragma comment(lib, "dwmapi.lib")

constexpr auto                    DEFAULT_MAX_SEARCH_STRINGS = 20;
constexpr auto                    TIMER_INFOSTRING           = 100;

std::unique_ptr<CRegexCaptureDlg> g_pRegexCaptureDlg;

void                              regexCaptureFinish()
{
    g_pRegexCaptureDlg.reset();
}

CRegexCaptureDlg::CRegexCaptureDlg(void* obj)
    : ICommand(obj)
    , m_captureWnd(g_hRes)
    , m_themeCallbackId(-1)
    , m_maxRegexStrings(DEFAULT_MAX_SEARCH_STRINGS)
    , m_maxCaptureStrings(DEFAULT_MAX_SEARCH_STRINGS)
{
}

void CRegexCaptureDlg::Show()
{
    this->ShowModeless(g_hRes, IDD_REGEXCAPTUREDLG, GetHwnd());
    SetFocus(GetDlgItem(*this, IDC_REGEXCOMBO));
    UpdateWindow(*this);
}

LRESULT CRegexCaptureDlg::DlgFunc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
        case WM_SHOWWINDOW:
            // m_open = wParam != FALSE;
            break;
        case WM_DESTROY:
            break;
        case WM_INITDIALOG:
            DoInitDialog(hwndDlg);
            break;
        case WM_SIZE:
        {
            int newWidth  = LOWORD(lParam);
            int newHeight = HIWORD(lParam);
            m_resizer.DoResize(newWidth, newHeight);
            break;
        }
        case WM_GETMINMAXINFO:
        {
            MINMAXINFO* mmi       = reinterpret_cast<MINMAXINFO*>(lParam);
            mmi->ptMinTrackSize.x = m_resizer.GetDlgRectScreen()->right;
            mmi->ptMinTrackSize.y = m_resizer.GetDlgRectScreen()->bottom;
            return 0;
        }
        case WM_COMMAND:
            return DoCommand(LOWORD(wParam), HIWORD(wParam));
        case WM_NOTIFY:
        {
            LPNMHDR pnmHdr = reinterpret_cast<LPNMHDR>(lParam);
            APPVERIFY(pnmHdr != nullptr);
            if (pnmHdr == nullptr)
                return 0;
            const NMHDR& nmHdr = *pnmHdr;

            if (nmHdr.idFrom == reinterpret_cast<UINT_PTR>(&m_captureWnd) || nmHdr.hwndFrom == m_captureWnd)
            {
                if (nmHdr.code == NM_COOLSB_CUSTOMDRAW)
                    return m_captureWnd.HandleScrollbarCustomDraw(wParam, reinterpret_cast<NMCSBCUSTOMDRAW*>(lParam));
            }
        }
        break;
        case WM_TIMER:
            if (wParam == TIMER_INFOSTRING)
            {
                KillTimer(*this, TIMER_INFOSTRING);
                SetDlgItemText(*this, IDC_INFOLABEL, L"");
#ifdef DWMWA_COLOR_DEFAULT
#    if NTDDI_VERSION >= 0x0A00000B
                COLORREF brdClr = DWMWA_COLOR_DEFAULT;
                DwmSetWindowAttribute(*this, DWMWA_BORDER_COLOR, &brdClr, sizeof(brdClr));
#    endif
#endif
            }
            break;
        default:
            break;
    }
    return FALSE;
}

LRESULT CRegexCaptureDlg::DoCommand(int id, int /*msg*/)
{
    switch (id)
    {
        case IDCANCEL:
        {
            m_captureWnd.Scintilla().ClearAll();
            size_t lengthDoc = Scintilla().Length();
            for (int i = INDIC_REGEXCAPTURE; i < INDIC_REGEXCAPTURE_END; ++i)
            {
                Scintilla().SetIndicatorCurrent(i);
                Scintilla().IndicatorClearRange(0, lengthDoc);
            }

            ShowWindow(*this, SW_HIDE);
        }
        break;
        case IDOK:
            DoCapture();
            break;
        default:
            break;
    }
    return 1;
}

void CRegexCaptureDlg::DoInitDialog(HWND hwndDlg)
{
    m_themeCallbackId = CTheme::Instance().RegisterThemeChangeCallback(
        [this]() {
            SetTheme(CTheme::Instance().IsDarkTheme());
        });
    m_captureWnd.Init(g_hRes, *this, GetDlgItem(*this, IDC_SCINTILLA));
    m_captureWnd.SetupLexerForLang("Text");
    SetTheme(CTheme::Instance().IsDarkTheme());
    InitDialog(hwndDlg, IDI_BOWPAD, false);
    m_captureWnd.SetupLexerForLang("Text");
    m_captureWnd.UpdateLineNumberWidth();

    // Position the dialog in the top right corner.
    // Make sure we don't obscure the scroll bar though.
    RECT rcScintilla, rcDlg;
    GetWindowRect(GetScintillaWnd(), &rcScintilla);
    GetWindowRect(hwndDlg, &rcDlg);

    int  sbVertWidth = GetSystemMetrics(SM_CXVSCROLL);

    LONG adjustX     = 15;
    if (sbVertWidth >= 0)
        adjustX += sbVertWidth;
    LONG x = rcScintilla.right - ((rcDlg.right - rcDlg.left) + adjustX);
    // Try (unscientifically) to not get to close to the tab bar either.
    LONG y = rcScintilla.top + 15;

    SetWindowPos(hwndDlg, nullptr, x, y, 0, 0, SWP_NOSIZE | SWP_NOACTIVATE);

    AdjustControlSize(IDC_ICASE);
    AdjustControlSize(IDC_DOTNEWLINE);
    AdjustControlSize(IDC_ADDNEWLINE);

    m_resizer.Init(hwndDlg);
    m_resizer.UseSizeGrip(!CTheme::Instance().IsDarkTheme());
    m_resizer.AddControl(hwndDlg, IDC_REGEXLABEL, RESIZER_TOPLEFT);
    m_resizer.AddControl(hwndDlg, IDC_CAPTURELABEL, RESIZER_TOPLEFT);
    m_resizer.AddControl(hwndDlg, IDC_REGEXCOMBO, RESIZER_TOPLEFTRIGHT);
    m_resizer.AddControl(hwndDlg, IDC_ICASE, RESIZER_TOPLEFT);
    m_resizer.AddControl(hwndDlg, IDC_DOTNEWLINE, RESIZER_TOPLEFT);
    m_resizer.AddControl(hwndDlg, IDC_ADDNEWLINE, RESIZER_TOPLEFT);
    m_resizer.AddControl(hwndDlg, IDC_CAPTURECOMBO, RESIZER_TOPLEFTRIGHT);
    m_resizer.AddControl(hwndDlg, IDOK, RESIZER_TOPRIGHT);
    m_resizer.AddControl(hwndDlg, IDC_SCINTILLA, RESIZER_TOPLEFTBOTTOMRIGHT);
    m_resizer.AdjustMinMaxSize();

    GetWindowRect(hwndDlg, &rcDlg);

    std::vector<std::wstring> regexStrings;
    m_maxRegexStrings = LoadData(regexStrings, DEFAULT_MAX_SEARCH_STRINGS, L"regexcapture", L"maxsearch", L"regex%d");
    LoadCombo(IDC_REGEXCOMBO, regexStrings);

    std::vector<std::wstring> captureStrings;
    m_maxCaptureStrings = LoadData(captureStrings, DEFAULT_MAX_SEARCH_STRINGS, L"regexcapture", L"maxsearch", L"capture%d");
    LoadCombo(IDC_CAPTURECOMBO, captureStrings);

    EnableComboBoxDeleteEvents(IDC_REGEXCOMBO, true);
    EnableComboBoxDeleteEvents(IDC_CAPTURECOMBO, true);
}

void CRegexCaptureDlg::SetTheme(bool bDark)
{
    CTheme::Instance().SetThemeForDialog(*this, bDark);
}

void CRegexCaptureDlg::DoCapture()
{
    SetDlgItemText(*this, IDC_INFOLABEL, L"");
#ifdef DWMWA_COLOR_DEFAULT
#    if NTDDI_VERSION >= 0x0A00000B
    COLORREF brdClr = DWMWA_COLOR_DEFAULT;
    DwmSetWindowAttribute(*this, DWMWA_BORDER_COLOR, &brdClr, sizeof(brdClr));
#    endif
#endif

    std::wstring sRegexW = GetDlgItemText(IDC_REGEXCOMBO).get();
    UpdateCombo(IDC_REGEXCOMBO, sRegexW, m_maxRegexStrings);
    std::wstring sCaptureW = GetDlgItemText(IDC_CAPTURECOMBO).get();
    UpdateCombo(IDC_CAPTURECOMBO, sCaptureW, m_maxCaptureStrings);

    auto sRegex   = CUnicodeUtils::StdGetUTF8(sRegexW);
    auto sCapture = UnEscape(CUnicodeUtils::StdGetUTF8(sCaptureW));
    if (sCapture.empty())
        sCapture = "$&";
    if (IsDlgButtonChecked(*this, IDC_ADDNEWLINE))
    {
        switch (Scintilla().EOLMode())
        {
            case Scintilla::EndOfLine::CrLf:
                sCapture += "\r\n";
                break;
            case Scintilla::EndOfLine::Lf:
                sCapture += "\n";
                break;
            case Scintilla::EndOfLine::Cr:
                sCapture += "\r";
                break;
            default:
                sCapture += "\r\n";
                APPVERIFY(false); // Shouldn't happen.
        }
    }
    try
    {
        auto                  findText = GetDlgItemText(IDC_SEARCHCOMBO);
        std::regex::flag_type rxFlags  = std::regex_constants::ECMAScript;
        if (IsDlgButtonChecked(*this, IDC_ICASE))
            rxFlags |= std::regex_constants::icase;
        // replace all "\n" chars with "(?:\n|\r\n|\n\r)"
        if ((sRegex.size() > 1) && (sRegex.find("\\r") == std::wstring::npos))
        {
            SearchReplace(sRegex, "\\n", "(!:\\n|\\r\\n|\\n\\r)");
        }

        const std::regex rx(sRegex, rxFlags);

        m_captureWnd.Scintilla().ClearAll();

        size_t lengthDoc = Scintilla().Length();
        for (int i = INDIC_REGEXCAPTURE; i < INDIC_REGEXCAPTURE_END; ++i)
        {
            Scintilla().SetIndicatorCurrent(i);
            Scintilla().IndicatorClearRange(0, lengthDoc);
        }

        const char*                                          pText = static_cast<const char*>(Scintilla().CharacterPointer());
        std::string_view                                     searchText(pText, lengthDoc);
        std::match_results<std::string_view::const_iterator> whatC;
        std::regex_constants::match_flag_type                flags = std::regex_constants::match_flag_type::match_default | std::regex_constants::match_flag_type::match_not_null;
        if (IsDlgButtonChecked(*this, IDC_DOTNEWLINE))
            flags |= std::regex_constants::match_flag_type::match_not_eol;
        auto                                         start = searchText.cbegin();
        auto                                         end   = searchText.cend();
        std::vector<std::tuple<int, size_t, size_t>> capturePositions;
        std::ostringstream                           outStream;
        while (std::regex_search(start, end, whatC, rx, flags))
        {
            if (whatC[0].matched)
            {
                auto out = whatC.format(sCapture, flags);
                outStream << out;
                if (outStream.tellp() > 5 * 1024 * 1024)
                {
                    const auto& sOut = outStream.str();
                    m_captureWnd.Scintilla().AppendText(sOut.size(), sOut.c_str());
                    outStream.str("");
                    outStream.clear();
                }

                int captureCount = 0;
                for (const auto& w : whatC)
                {
                    capturePositions.push_back(std::make_tuple(captureCount, w.first - searchText.cbegin(), w.length()));
                    ++captureCount;
                }
            }
            // update search position:
            if (start == whatC[0].second)
            {
                if (start == end)
                    break;
                ++start;
            }
            else
                start = whatC[0].second;
            // update flags for continuation
            flags |= std::regex_constants::match_flag_type::match_prev_avail;
        }
        const auto& resultString = outStream.str();
        m_captureWnd.Scintilla().AppendText(resultString.size(), resultString.c_str());
        m_captureWnd.UpdateLineNumberWidth();

        for (const auto& [num, begin, length] : capturePositions)
        {
            Scintilla().SetIndicatorCurrent(INDIC_REGEXCAPTURE + num);
            Scintilla().IndicatorFillRange(begin, length);
        }

        std::vector<std::wstring> regexStrings;
        SaveCombo(IDC_REGEXCOMBO, regexStrings);
        SaveData(regexStrings, L"regexcapture", L"maxsearch", L"regex%d");
        std::vector<std::wstring> captureStrings;
        SaveCombo(IDC_CAPTURECOMBO, captureStrings);
        SaveData(captureStrings, L"regexcapture", L"maxsearch", L"capture%d");
    }
    catch (const std::exception&)
    {
        SetInfoText(IDS_REGEX_NOTOK, AlertMode::Flash);
    }
}

void CRegexCaptureDlg::SetInfoText(UINT resid, AlertMode alertMode)
{
    ResString str(g_hRes, resid);
    SetDlgItemText(*this, IDC_INFOLABEL, str);
    if (alertMode == AlertMode::Flash)
    {
        FlashWindow(*this);
#ifdef DWMWA_COLOR_DEFAULT
#    if NTDDI_VERSION >= 0x0A00000B
        COLORREF brdClr = RGB(255, 0, 0);
        DwmSetWindowAttribute(*this, DWMWA_BORDER_COLOR, &brdClr, sizeof(brdClr));
#    endif
#endif
    }
    else
    {
#ifdef DWMWA_COLOR_DEFAULT
#    if NTDDI_VERSION >= 0x0A00000B
        COLORREF brdClr = DWMWA_COLOR_DEFAULT;
        DwmSetWindowAttribute(*this, DWMWA_BORDER_COLOR, &brdClr, sizeof(brdClr));
#    endif
#endif
    }
    SetTimer(*this, TIMER_INFOSTRING, 5000, nullptr);
}

CCmdRegexCapture::CCmdRegexCapture(void* obj)
    : ICommand(obj)
{
}

bool CCmdRegexCapture::Execute()
{
    if (!g_pRegexCaptureDlg)
        g_pRegexCaptureDlg = std::make_unique<CRegexCaptureDlg>(m_pMainWindow);

    g_pRegexCaptureDlg->Show();
    return true;
}
