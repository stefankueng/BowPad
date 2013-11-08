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
#include "CmdFindReplace.h"
#include "BowPad.h"
#include "ScintillaWnd.h"
#include "UnicodeUtils.h"
#include "StringUtils.h"

std::string         sFindString;
static int          nSearchFlags;

#define TIMER_INFOSTRING    100

CFindReplaceDlg::CFindReplaceDlg(void * obj)
    : ICommand(obj)
{
}

CFindReplaceDlg::~CFindReplaceDlg(void)
{
}

LRESULT CFindReplaceDlg::DlgFunc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (uMsg)
    {
    case WM_INITDIALOG:
        {
            InitDialog(hwndDlg, IDI_BOWPAD);
            m_resizer.Init(hwndDlg);
            m_resizer.AddControl(hwndDlg, IDC_LABEL1, RESIZER_TOPLEFT);
            m_resizer.AddControl(hwndDlg, IDC_LABEL2, RESIZER_TOPLEFT);
            m_resizer.AddControl(hwndDlg, IDC_SEARCHCOMBO, RESIZER_TOPLEFTRIGHT);
            m_resizer.AddControl(hwndDlg, IDC_REPLACECOMBO, RESIZER_TOPLEFTRIGHT);
            m_resizer.AddControl(hwndDlg, IDC_MATCHWORD, RESIZER_TOPLEFT);
            m_resizer.AddControl(hwndDlg, IDC_MATCHCASE, RESIZER_TOPLEFT);
            m_resizer.AddControl(hwndDlg, IDC_MATCHREGEX, RESIZER_TOPLEFT);
            m_resizer.AddControl(hwndDlg, IDC_FINDBTN, RESIZER_TOPRIGHT);
            m_resizer.AddControl(hwndDlg, IDC_REPLACEBTN, RESIZER_TOPRIGHT);
            m_resizer.AddControl(hwndDlg, IDC_REPLACEALLBTN, RESIZER_TOPRIGHT);
            m_resizer.AddControl(hwndDlg, IDC_SEARCHINFO, RESIZER_TOPLEFTRIGHT);
            m_resizer.AddControl(hwndDlg, IDCANCEL, RESIZER_TOPRIGHT);

            HWND hSearchCombo = GetDlgItem(*this, IDC_SEARCHCOMBO);
            int maxSearch = (int)CIniSettings::Instance().GetInt64(L"searchreplace", L"maxsearch", 20);
            m_searchStrings.clear();
            for (int i = 0; i < maxSearch; ++i)
            {
                std::wstring sKey = CStringUtils::Format(L"search%d", i);
                std::wstring sSearchWord = CIniSettings::Instance().GetString(L"searchreplace", sKey.c_str(), L"");
                if (!sSearchWord.empty())
                {
                    m_searchStrings.push_back(sSearchWord);
                    SendMessage(hSearchCombo, CB_INSERTSTRING, (WPARAM)-1, (LPARAM)sSearchWord.c_str());
                }
            }

            HWND hReplaceCombo = GetDlgItem(*this, IDC_REPLACECOMBO);
            int maxReplace = (int)CIniSettings::Instance().GetInt64(L"searchreplace", L"maxReplace", 20);
            m_replaceStrings.clear();
            for (int i = 0; i < maxReplace; ++i)
            {
                std::wstring sKey = CStringUtils::Format(L"replace%d", i);
                std::wstring sReplaceWord = CIniSettings::Instance().GetString(L"searchreplace", sKey.c_str(), L"");
                if (!sReplaceWord.empty())
                {
                    m_replaceStrings.push_back(sReplaceWord);
                    SendMessage(hReplaceCombo, CB_INSERTSTRING, (WPARAM)-1, (LPARAM)sReplaceWord.c_str());
                }
            }

            SetFocus(hSearchCombo);
        }
        return FALSE;
    case WM_ACTIVATE:
            SetTransparency(wParam ? 255 : 150);
        break;
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
        return DoCommand(LOWORD(wParam), HIWORD(wParam));
    case WM_TIMER:
        {
            if (wParam == TIMER_INFOSTRING)
                SetDlgItemText(*this, IDC_SEARCHINFO, L"");
            KillTimer(*this, TIMER_INFOSTRING);
        }
        break;
    default:
        return FALSE;
    }
    return FALSE;
}

LRESULT CFindReplaceDlg::DoCommand(int id, int /*msg*/)
{
    switch (id)
    {
    case IDCANCEL:
        ShowWindow(*this, SW_HIDE);
        break;
    case IDC_FINDBTN:
        {
            DoSearch();
        }
        break;
    case IDC_REPLACEALLBTN:
    case IDC_REPLACEBTN:
        {
            DoReplace(id);
        }
        break;
    }
    return 1;
}

void CFindReplaceDlg::SetInfoText( UINT resid )
{
    ResString str(hRes, resid);
    SetDlgItemText(*this, IDC_SEARCHINFO, str);
    SetTimer(*this, TIMER_INFOSTRING, 5000, NULL);
}

void CFindReplaceDlg::DoReplace( int id )
{
    SetDlgItemText(*this, IDC_SEARCHINFO, L"");
    ScintillaCall(SCI_SETTARGETSTART, id==IDC_REPLACEBTN ? ScintillaCall(SCI_GETSELECTIONSTART): 0);
    ScintillaCall(SCI_SETTARGETEND, ScintillaCall(SCI_GETLENGTH));

    auto findText = GetDlgItemText(IDC_SEARCHCOMBO);
    sFindString = CUnicodeUtils::StdGetUTF8(findText.get());
    auto replaceText = GetDlgItemText(IDC_REPLACECOMBO);
    std::string sReplaceString = CUnicodeUtils::StdGetUTF8(replaceText.get());

    nSearchFlags = 0;
    nSearchFlags |= IsDlgButtonChecked(*this, IDC_MATCHCASE)  ? SCFIND_MATCHCASE : 0;
    nSearchFlags |= IsDlgButtonChecked(*this, IDC_MATCHWORD)  ? SCFIND_WHOLEWORD : 0;
    nSearchFlags |= IsDlgButtonChecked(*this, IDC_MATCHREGEX) ? SCFIND_REGEXP    : 0;

    ScintillaCall(SCI_SETSEARCHFLAGS, nSearchFlags);
    sptr_t findRet = -1;
    int replaceCount = 0;
    ScintillaCall(SCI_BEGINUNDOACTION);
    do
    {
        findRet = ScintillaCall(SCI_SEARCHINTARGET, sFindString.length(), (sptr_t)sFindString.c_str());
        if ((findRet == -1) && (id==IDC_REPLACEBTN))
        {
            SetInfoText(IDS_FINDRETRYWRAP);
            // retry from the start of the doc
            ScintillaCall(SCI_SETTARGETSTART, 0);
            ScintillaCall(SCI_SETTARGETEND, ScintillaCall(SCI_GETCURRENTPOS));
            findRet = ScintillaCall(SCI_SEARCHINTARGET, sFindString.length(), (sptr_t)sFindString.c_str());
        }
        if (findRet >= 0)
        {
            ScintillaCall((nSearchFlags & SCFIND_REGEXP)!=0 ? SCI_REPLACETARGETRE : SCI_REPLACETARGET, sReplaceString.length(), (sptr_t)sReplaceString.c_str());
            ++replaceCount;
            if (id==IDC_REPLACEBTN)
                Center((long)ScintillaCall(SCI_GETTARGETSTART), (long)ScintillaCall(SCI_GETTARGETEND));

            ScintillaCall(SCI_SETTARGETSTART, ScintillaCall(SCI_GETTARGETEND));
            ScintillaCall(SCI_SETTARGETEND, ScintillaCall(SCI_GETLENGTH));
        }
    } while ((id==IDC_REPLACEALLBTN) && (findRet != -1));
    ScintillaCall(SCI_ENDUNDOACTION);
    if (id==IDC_REPLACEALLBTN)
    {
        // TODO: maybe use a better way to inform the user than an interrupting dialog box
        std::wstring sInfo = CStringUtils::Format(L"%d occurrences were replaced", replaceCount);
        ::MessageBox(*this, sInfo.c_str(), L"BowPad", MB_ICONINFORMATION);
    }
    else
        DoSearch();

    int maxSearch = (int)CIniSettings::Instance().GetInt64(L"searchreplace", L"maxsearch", 20);
    std::wstring sFindStringW = CUnicodeUtils::StdGetUnicode(sFindString);
    auto foundIt = std::find(m_searchStrings.begin(), m_searchStrings.end(), sFindStringW);
    if (foundIt != m_searchStrings.end())
        m_searchStrings.erase(foundIt);
    else
        SendDlgItemMessage(*this, IDC_SEARCHCOMBO, CB_INSERTSTRING, 0, (LPARAM)sFindStringW.c_str());
    m_searchStrings.push_front(sFindStringW);
    while (m_searchStrings.size() >= (size_t)maxSearch)
        m_searchStrings.pop_back();

    int i = 0;
    for (const auto& it : m_searchStrings)
    {
        std::wstring sKey = CStringUtils::Format(L"search%d", i++);
        CIniSettings::Instance().SetString(L"searchreplace", sKey.c_str(), it.c_str());
    }


    int maxReplace = (int)CIniSettings::Instance().GetInt64(L"searchreplace", L"maxreplace", 20);
    std::wstring sReplaceStringW = CUnicodeUtils::StdGetUnicode(sReplaceString);
    foundIt = std::find(m_replaceStrings.begin(), m_replaceStrings.end(), sReplaceStringW);
    if (foundIt != m_replaceStrings.end())
        m_replaceStrings.erase(foundIt);
    else
        SendDlgItemMessage(*this, IDC_REPLACECOMBO, CB_INSERTSTRING, 0, (LPARAM)sReplaceStringW.c_str());
    m_replaceStrings.push_front(sReplaceStringW);
    while (m_replaceStrings.size() >= (size_t)maxReplace)
        m_replaceStrings.pop_back();

    i = 0;
    for (const auto& it : m_replaceStrings)
    {
        std::wstring sKey = CStringUtils::Format(L"replace%d", i++);
        CIniSettings::Instance().SetString(L"searchreplace", sKey.c_str(), it.c_str());
    }
}

void CFindReplaceDlg::DoSearch()
{
    SetDlgItemText(*this, IDC_SEARCHINFO, L"");
    Scintilla::Sci_TextToFind ttf = {0};
    ttf.chrg.cpMin = (long)ScintillaCall(SCI_GETCURRENTPOS);
    ttf.chrg.cpMax = (long)ScintillaCall(SCI_GETLENGTH);
    auto findText = GetDlgItemText(IDC_SEARCHCOMBO);
    sFindString = CUnicodeUtils::StdGetUTF8(findText.get());
    ttf.lpstrText = const_cast<char*>(sFindString.c_str());
    nSearchFlags = 0;
    nSearchFlags |= IsDlgButtonChecked(*this, IDC_MATCHCASE)  ? SCFIND_MATCHCASE : 0;
    nSearchFlags |= IsDlgButtonChecked(*this, IDC_MATCHWORD)  ? SCFIND_WHOLEWORD : 0;
    nSearchFlags |= IsDlgButtonChecked(*this, IDC_MATCHREGEX) ? SCFIND_REGEXP    : 0;

    sptr_t findRet = ScintillaCall(SCI_FINDTEXT, nSearchFlags, (sptr_t)&ttf);
    if (findRet == -1)
    {
        SetInfoText(IDS_FINDRETRYWRAP);
        // retry from the start of the doc
        ttf.chrg.cpMax = ttf.chrg.cpMin;
        ttf.chrg.cpMin = 0;
        findRet = ScintillaCall(SCI_FINDTEXT, nSearchFlags, (sptr_t)&ttf);
    }
    if (findRet >= 0)
        Center(ttf.chrgText.cpMin, ttf.chrgText.cpMax);
    else
    {
        SetInfoText(IDS_FINDNOTFOUND);
        FLASHWINFO fi = {sizeof(FLASHWINFO)};
        fi.hwnd = *this;
        fi.dwFlags = FLASHW_CAPTION;
        fi.uCount = 3;
        fi.dwTimeout = 20;
        FlashWindowEx(&fi);
    }

    int maxSearch = (int)CIniSettings::Instance().GetInt64(L"searchreplace", L"maxsearch", 20);
    std::wstring sFindStringW = CUnicodeUtils::StdGetUnicode(sFindString);
    auto foundIt = std::find(m_searchStrings.begin(), m_searchStrings.end(), sFindStringW);
    if (foundIt != m_searchStrings.end())
        m_searchStrings.erase(foundIt);
    else
        SendDlgItemMessage(*this, IDC_SEARCHCOMBO, CB_INSERTSTRING, 0, (LPARAM)CUnicodeUtils::StdGetUnicode(sFindString).c_str());
    m_searchStrings.push_front(sFindStringW);
    while (m_searchStrings.size() >= (size_t)maxSearch)
        m_searchStrings.pop_back();

    int i = 0;
    for (const auto& it : m_searchStrings)
    {
        std::wstring sKey = CStringUtils::Format(L"search%d", i++);
        CIniSettings::Instance().SetString(L"searchreplace", sKey.c_str(), it.c_str());
    }
}

bool CCmdFindReplace::Execute()
{
    if (m_pFindReplaceDlg == nullptr)
        m_pFindReplaceDlg = new CFindReplaceDlg(m_Obj);

    int selTextLen = (int)ScintillaCall(SCI_GETSELTEXT);
    std::unique_ptr<char[]> seltextbuffer(new char[selTextLen + 1]);
    ScintillaCall(SCI_GETSELTEXT, 0, (LPARAM)(char*)seltextbuffer.get());
    std::string sSelText = seltextbuffer.get();

    m_pFindReplaceDlg->ShowModeless(hRes, IDD_FINDREPLACEDLG, GetHwnd());
    if (!sSelText.empty())
    {
        SetDlgItemText(*m_pFindReplaceDlg, IDC_SEARCHCOMBO, CUnicodeUtils::StdGetUnicode(sSelText).c_str());
        SetFocus(GetDlgItem(*m_pFindReplaceDlg, IDC_SEARCHCOMBO));
    }

    return true;
}

void CCmdFindReplace::ScintillaNotify( Scintilla::SCNotification * pScn )
{
    switch (pScn->nmhdr.code)
    {
    case SCN_UPDATEUI:
        {
            static std::string lastSelText;
            static int         lastSearchFlags;

            LRESULT firstline = ScintillaCall(SCI_GETFIRSTVISIBLELINE);
            LRESULT lastline = firstline + ScintillaCall(SCI_LINESONSCREEN);
            long startstylepos = (long)ScintillaCall(SCI_POSITIONFROMLINE, firstline);
            long endstylepos = (long)ScintillaCall(SCI_POSITIONFROMLINE, lastline) + (long)ScintillaCall(SCI_LINELENGTH, lastline);
            if (endstylepos < 0)
                endstylepos = (long)ScintillaCall(SCI_GETLENGTH)-startstylepos;

            int len = endstylepos - startstylepos + 1;
            // reset indicators
            ScintillaCall(SCI_SETINDICATORCURRENT, INDIC_FINDTEXT_MARK);
            ScintillaCall(SCI_INDICATORCLEARRANGE, startstylepos, len);

            if (sFindString.empty())
            {
                DocScrollClear(DOCSCROLLTYPE_SEARCHTEXT);
                return;
            }

            Scintilla::Sci_TextToFind FindText;
            FindText.chrg.cpMin = startstylepos;
            FindText.chrg.cpMax = endstylepos;
            FindText.lpstrText = const_cast<char*>(sFindString.c_str());
            while (ScintillaCall(SCI_FINDTEXT, nSearchFlags, (LPARAM)&FindText) >= 0)
            {
                ScintillaCall(SCI_INDICATORFILLRANGE, FindText.chrgText.cpMin, FindText.chrgText.cpMax-FindText.chrgText.cpMin);
                if (FindText.chrg.cpMin >= FindText.chrgText.cpMax)
                    break;
                FindText.chrg.cpMin = FindText.chrgText.cpMax;
            }

            if (lastSelText.compare(sFindString) || (nSearchFlags != lastSearchFlags))
            {
                DocScrollClear(DOCSCROLLTYPE_SEARCHTEXT);
                Scintilla::Sci_TextToFind FindText;
                FindText.chrg.cpMin = 0;
                FindText.chrg.cpMax = (long)ScintillaCall(SCI_GETLENGTH);
                FindText.lpstrText = const_cast<char*>(sFindString.c_str());
                while (ScintillaCall(SCI_FINDTEXT, nSearchFlags, (LPARAM)&FindText) >= 0)
                {
                    size_t line = ScintillaCall(SCI_LINEFROMPOSITION, FindText.chrgText.cpMin);
                    DocScrollAddLineColor(DOCSCROLLTYPE_SEARCHTEXT, line, RGB(200,200,0));
                    if (FindText.chrg.cpMin >= FindText.chrgText.cpMax)
                        break;
                    FindText.chrg.cpMin = FindText.chrgText.cpMax;
                }
            }
            lastSelText = sFindString.c_str();
            lastSearchFlags = nSearchFlags;
        }
        break;
    default:
        break;
    }
}

bool CCmdFindNext::Execute()
{
    Scintilla::Sci_TextToFind ttf = {0};
    ttf.chrg.cpMin = (long)ScintillaCall(SCI_GETCURRENTPOS);
    ttf.chrg.cpMax = (long)ScintillaCall(SCI_GETLENGTH);
    ttf.lpstrText = const_cast<char*>(sFindString.c_str());
    sptr_t findRet = ScintillaCall(SCI_FINDTEXT, nSearchFlags, (sptr_t)&ttf);
    if (findRet == -1)
    {
        // retry from the start of the doc
        ttf.chrg.cpMax = ttf.chrg.cpMin;
        ttf.chrg.cpMin = 0;
        findRet = ScintillaCall(SCI_FINDTEXT, nSearchFlags, (sptr_t)&ttf);
    }
    if (findRet >= 0)
        Center(ttf.chrgText.cpMin, ttf.chrgText.cpMax);
    else
    {
        FLASHWINFO fi = {sizeof(FLASHWINFO)};
        fi.hwnd = GetHwnd();
        fi.dwFlags = FLASHW_CAPTION;
        fi.uCount = 3;
        fi.dwTimeout = 20;
        FlashWindowEx(&fi);
    }
    return true;
}

bool CCmdFindPrev::Execute()
{
    Scintilla::Sci_TextToFind ttf = {0};
    ttf.chrg.cpMin = (long)ScintillaCall(SCI_GETCURRENTPOS);
    if (ttf.chrg.cpMin > 0)
        ttf.chrg.cpMin--;
    ttf.chrg.cpMax = 0;
    ttf.lpstrText = const_cast<char*>(sFindString.c_str());
    sptr_t findRet = ScintillaCall(SCI_FINDTEXT, nSearchFlags, (sptr_t)&ttf);
    if (findRet == -1)
    {
        // retry from the end of the doc
        ttf.chrg.cpMax = ttf.chrg.cpMin+1;
        ttf.chrg.cpMin = (long)ScintillaCall(SCI_GETLENGTH);
        findRet = ScintillaCall(SCI_FINDTEXT, nSearchFlags, (sptr_t)&ttf);
    }
    if (findRet >= 0)
        Center(ttf.chrgText.cpMin, ttf.chrgText.cpMax);
    else
    {
        FLASHWINFO fi = {sizeof(FLASHWINFO)};
        fi.hwnd = GetHwnd();
        fi.dwFlags = FLASHW_CAPTION;
        fi.uCount = 3;
        fi.dwTimeout = 20;
        FlashWindowEx(&fi);
    }
    return true;
}

bool CCmdFindSelectedNext::Execute()
{
    int selTextLen = (int)ScintillaCall(SCI_GETSELTEXT);
    if (selTextLen == 0)
        return false;
    std::unique_ptr<char[]> seltextbuffer(new char[selTextLen + 1]);
    ScintillaCall(SCI_GETSELTEXT, 0, (LPARAM)(char*)seltextbuffer.get());
    if (seltextbuffer[0] == 0)
        return false;
    sFindString = seltextbuffer.get();

    Scintilla::Sci_TextToFind ttf = {0};
    ttf.chrg.cpMin = (long)ScintillaCall(SCI_GETCURRENTPOS);
    ttf.chrg.cpMax = (long)ScintillaCall(SCI_GETLENGTH);
    ttf.lpstrText = const_cast<char*>(sFindString.c_str());
    sptr_t findRet = ScintillaCall(SCI_FINDTEXT, nSearchFlags, (sptr_t)&ttf);
    if (findRet == -1)
    {
        // retry from the start of the doc
        ttf.chrg.cpMax = ttf.chrg.cpMin;
        ttf.chrg.cpMin = 0;
        findRet = ScintillaCall(SCI_FINDTEXT, nSearchFlags, (sptr_t)&ttf);
    }
    if (findRet >= 0)
        Center(ttf.chrgText.cpMin, ttf.chrgText.cpMax);
    else
    {
        FLASHWINFO fi = {sizeof(FLASHWINFO)};
        fi.hwnd = GetHwnd();
        fi.dwFlags = FLASHW_CAPTION;
        fi.uCount = 3;
        fi.dwTimeout = 20;
        FlashWindowEx(&fi);
    }
    return true;
}

bool CCmdFindSelectedPrev::Execute()
{
    int selTextLen = (int)ScintillaCall(SCI_GETSELTEXT);
    if (selTextLen == 0)
        return false;
    std::unique_ptr<char[]> seltextbuffer(new char[selTextLen + 1]);
    ScintillaCall(SCI_GETSELTEXT, 0, (LPARAM)(char*)seltextbuffer.get());
    if (seltextbuffer[0] == 0)
        return false;
    sFindString = seltextbuffer.get();

    Scintilla::Sci_TextToFind ttf = {0};
    ttf.chrg.cpMin = (long)ScintillaCall(SCI_GETCURRENTPOS);
    if (ttf.chrg.cpMin > 0)
        ttf.chrg.cpMin--;
    ttf.chrg.cpMax = 0;
    ttf.lpstrText = const_cast<char*>(sFindString.c_str());
    sptr_t findRet = ScintillaCall(SCI_FINDTEXT, nSearchFlags, (sptr_t)&ttf);
    if (findRet == -1)
    {
        // retry from the end of the doc
        ttf.chrg.cpMax = ttf.chrg.cpMin+1;
        ttf.chrg.cpMin = (long)ScintillaCall(SCI_GETLENGTH);
        findRet = ScintillaCall(SCI_FINDTEXT, nSearchFlags, (sptr_t)&ttf);
    }
    if (findRet >= 0)
        Center(ttf.chrgText.cpMin, ttf.chrgText.cpMax);
    else
    {
        FLASHWINFO fi = {sizeof(FLASHWINFO)};
        fi.hwnd = GetHwnd();
        fi.dwFlags = FLASHW_CAPTION;
        fi.uCount = 3;
        fi.dwTimeout = 20;
        FlashWindowEx(&fi);
    }
    return true;
}
