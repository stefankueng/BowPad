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
#include "CmdFindReplace.h"
#include "BowPad.h"
#include "ScintillaWnd.h"
#include "UnicodeUtils.h"
#include "StringUtils.h"

#include <regex>

std::string         sFindString;
std::string         sHighlightString;
static int          nSearchFlags;

#define TIMER_INFOSTRING    100

CFindReplaceDlg::CFindReplaceDlg(void * obj)
    : ICommand(obj)
    , m_freeresize(false)
    , m_searchWnd(hRes)
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
            InitDialog(hwndDlg, IDI_BOWPAD, false);

            // Position the find dialog in the top right corner.
            // Make sure we don't obsecure the scroll bar though.
            RECT rcScintilla, rcDlg;
            GetWindowRect(GetScintillaWnd(), &rcScintilla);
            GetWindowRect(hwndDlg, &rcDlg);

            int sbVertWidth = GetSystemMetrics(SM_CXVSCROLL);
            // Make a small gap between the dialog and the SB, or edge if
            // there isn't an sb. The gap needs to be a little bit
            // big to account for the shadow dialogs sometimes get.
            // We don't want the shadow obscuring the sb either.
            LONG adjustX = 15;
            if (sbVertWidth >= 0)
                adjustX += sbVertWidth;
            LONG x = rcScintilla.right - ((rcDlg.right - rcDlg.left) + adjustX);
            // Try (unscientifically) to not get to close to the tab bar either.
            LONG y = rcScintilla.top + 15;

            SetWindowPos(hwndDlg, HWND_TOP, x, y, 0, 0, SWP_NOSIZE);

            AdjustControlSize(IDC_MATCHWORD);
            AdjustControlSize(IDC_MATCHCASE);
            AdjustControlSize(IDC_MATCHREGEX);
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
            m_resizer.AddControl(hwndDlg, IDC_FINDRESULTS, RESIZER_TOPLEFTBOTTOMRIGHT);
            m_resizer.AddControl(hwndDlg, IDC_SEARCHINFO, RESIZER_TOPLEFTRIGHT);
            m_freeresize = true;
            ShowResults(false);
            m_freeresize = false;
            m_resizer.AdjustMinMaxSize();

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

            m_searchWnd.Init(hRes, *this);
            ShowWindow(*this, SW_SHOW);
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
            if (!m_freeresize)
            {
                MINMAXINFO * mmi = (MINMAXINFO*)lParam;
                mmi->ptMinTrackSize.x = m_resizer.GetDlgRectScreen()->right;
                mmi->ptMinTrackSize.y = m_resizer.GetDlgRectScreen()->bottom;
                return 0;
            }
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
    case WM_NOTIFY:
        switch (wParam)
        {
            case IDC_FINDRESULTS:
                return DoListNotify((LPNMITEMACTIVATE)lParam);
            case IDC_FINDBTN:
            case IDC_REPLACEALLBTN:
                switch (((LPNMHDR)lParam)->code)
                {
                    case BCN_DROPDOWN:
                    {
                        NMBCDROPDOWN* pDropDown = (NMBCDROPDOWN*)lParam;
                        // Get screen coordinates of the button.
                        POINT pt;
                        pt.x = pDropDown->rcButton.left;
                        pt.y = pDropDown->rcButton.bottom;
                        ClientToScreen(pDropDown->hdr.hwndFrom, &pt);

                        // Create a menu and add items.
                        HMENU hSplitMenu = CreatePopupMenu();
                        if (pDropDown->hdr.hwndFrom == GetDlgItem(*this, IDC_FINDBTN))
                        {
                            ResString sFindAll(hRes, IDS_FINDALL);
                            ResString sFindAllInTabs(hRes, IDS_FINDALLINTABS);
                            AppendMenu(hSplitMenu, MF_BYPOSITION, IDC_FINDALL, sFindAll);
                            AppendMenu(hSplitMenu, MF_BYPOSITION, IDC_FINDALLINTABS, sFindAllInTabs);
                        }
                        else if (pDropDown->hdr.hwndFrom == GetDlgItem(*this, IDC_REPLACEALLBTN))
                        {
                            size_t selStart = ScintillaCall(SCI_GETSELECTIONSTART);
                            size_t selEnd = ScintillaCall(SCI_GETSELECTIONEND);
                            size_t linestart = ScintillaCall(SCI_LINEFROMPOSITION, selStart);
                            size_t lineend = ScintillaCall(SCI_LINEFROMPOSITION, selEnd);

                            int wrapcount = 1;
                            if (linestart == lineend)
                                wrapcount = (int)ScintillaCall(SCI_WRAPCOUNT, linestart);
                            bool bReplaceOnlyInSelection = (linestart != lineend) || ((wrapcount > 1) && (selEnd - selStart > 20));

                            ResString sReplaceAll(hRes, IDS_REPLACEALL);
                            ResString sReplaceAllInSelection(hRes, IDS_REPLACEALLINSELECTION);
                            AppendMenu(hSplitMenu, MF_BYPOSITION, IDC_REPLACEALLBTN, bReplaceOnlyInSelection ? sReplaceAllInSelection : sReplaceAll);

                            ResString sReplaceAllInTabs(hRes, IDS_REPLACEALLINTABS);
                            AppendMenu(hSplitMenu, MF_BYPOSITION, IDC_REPLACEALLINTABSBTN, sReplaceAllInTabs);
                        }
                        // Display the menu.
                        TrackPopupMenu(hSplitMenu, TPM_LEFTALIGN | TPM_TOPALIGN, pt.x, pt.y, 0, *this, NULL);
                        return TRUE;
                    }
                    break;
                }
                break;
            }
            break;
        default:
            return FALSE;
    }
    return FALSE;
}

LRESULT CFindReplaceDlg::DoListNotify(LPNMITEMACTIVATE lpNMItemActivate)
{
    std::wstring sTemp;
    switch (lpNMItemActivate->hdr.code)
    {
        case LVN_GETDISPINFO:
        {
            NMLVDISPINFO * pDispInfo = (NMLVDISPINFO*)lpNMItemActivate;
            if (pDispInfo->item.mask & LVIF_TEXT)
            {
                if (pDispInfo->item.pszText == nullptr)
                    return 0;
                pDispInfo->item.pszText[0] = 0;
                if (pDispInfo->item.iItem >= (int)m_searchResults.size())
                    return 0;
                switch (pDispInfo->item.iSubItem)
                {
                    case 0:     // file
                        sTemp = GetTitleForDocID(m_searchResults[pDispInfo->item.iItem].docID);
                        lstrcpyn(pDispInfo->item.pszText, sTemp.c_str(), pDispInfo->item.cchTextMax);
                        break;

                    case 1:     // line
                        sTemp = CStringUtils::Format(L"%ld", m_searchResults[pDispInfo->item.iItem].line+1);
                        lstrcpyn(pDispInfo->item.pszText, sTemp.c_str(), pDispInfo->item.cchTextMax);
                        break;

                    case 2:     // line text
                        lstrcpyn(pDispInfo->item.pszText, m_searchResults[pDispInfo->item.iItem].lineText.c_str(), pDispInfo->item.cchTextMax);
                        break;

                    default:
                        break;
                }
            }
        }
            break;
        case NM_DBLCLK:
        {
            if (lpNMItemActivate->iItem >= (int)m_searchResults.size())
                return 0;
            int tab = GetActiveTabIndex();
            int docID = GetDocIDFromTabIndex(tab);
            if (docID != m_searchResults[lpNMItemActivate->iItem].docID)
            {
                int tabtoopen = GetTabIndexFromDocID(m_searchResults[lpNMItemActivate->iItem].docID);
                if (tabtoopen >= 0)
                    TabActivateAt(tabtoopen);
                else
                    return 0;
            }
            ScintillaCall(SCI_GOTOLINE, m_searchResults[lpNMItemActivate->iItem].line);
            Center((long)m_searchResults[lpNMItemActivate->iItem].pos, (long)m_searchResults[lpNMItemActivate->iItem].pos);
        }
            break;
        case NM_CUSTOMDRAW:
        {
            NMLVCUSTOMDRAW* pLVCD = reinterpret_cast<NMLVCUSTOMDRAW*>(lpNMItemActivate);

            switch (pLVCD->nmcd.dwDrawStage)
            {
                case CDDS_PREPAINT:
                {
                    return CDRF_NOTIFYITEMDRAW;
                }
                    break;
                case CDDS_ITEMPREPAINT:
                {
                    // Tell Windows to send draw notifications for each subitem.
                    return CDRF_NOTIFYSUBITEMDRAW;
                }
                    break;
                case CDDS_ITEMPREPAINT | CDDS_ITEM | CDDS_SUBITEM:
                {
                    //return DrawListItemWithMatches(pLVCD);
                    switch (pLVCD->iSubItem)
                    {
                        case 0:     // file
                        case 1:     // line
                            return CDRF_DODEFAULT;
                            break;

                        case 2:     // line text
                            return DrawListItemWithMatches(pLVCD);
                    }
                }
                    break;
            }
            return CDRF_DODEFAULT;
        }
            break;
        default:
            break;
    }
    return 0;
}

LRESULT CFindReplaceDlg::DrawListItemWithMatches(NMLVCUSTOMDRAW * pLVCD)
{
    HWND hListControl = pLVCD->nmcd.hdr.hwndFrom;
    if (pLVCD->nmcd.dwItemSpec >= m_searchResults.size())
        return CDRF_DODEFAULT;

    //wchar_t textbuf[1024] = { 0 };
    //ListView_GetItemText(hListControl, (int)pLVCD->nmcd.dwItemSpec, pLVCD->iSubItem, textbuf, _countof(textbuf));
    //if (textbuf[0] == 0)
    //    return CDRF_DODEFAULT;
    //std::wstring text = textbuf;

    const std::wstring& text = m_searchResults[pLVCD->nmcd.dwItemSpec].lineText;

    size_t start = m_searchResults[pLVCD->nmcd.dwItemSpec].posInLineStart;
    size_t end   = m_searchResults[pLVCD->nmcd.dwItemSpec].posInLineEnd;

    if (start != end)
    {
        int drawPos = 0;

        // even though we initialize the 'rect' here with nmcd.rc,
        // we must not use it but use the rects from GetItemRect()
        // and GetSubItemRect(). Because on XP, the nmcd.rc has
        // bogus data in it.
        RECT rect = pLVCD->nmcd.rc;

        // find the margin where the text label starts
        RECT labelRC, boundsRC, iconRC;

        ListView_GetItemRect(hListControl, (int)pLVCD->nmcd.dwItemSpec, &labelRC, LVIR_LABEL);
        ListView_GetItemRect(hListControl, (int)pLVCD->nmcd.dwItemSpec, &iconRC, LVIR_ICON);
        ListView_GetItemRect(hListControl, (int)pLVCD->nmcd.dwItemSpec, &boundsRC, LVIR_BOUNDS);

        DrawListColumnBackground(pLVCD);
        int leftmargin = labelRC.left - boundsRC.left;
        if (pLVCD->iSubItem)
        {
            leftmargin -= (iconRC.right - iconRC.left);
        }
        if (pLVCD->iSubItem != 0)
            ListView_GetSubItemRect(hListControl, (int)pLVCD->nmcd.dwItemSpec, pLVCD->iSubItem, LVIR_BOUNDS, &rect);

        int borderWidth = 0;
        if (IsAppThemed() /* && SysInfo::Instance().IsVistaOrLater() */)
        {
            HTHEME hTheme = OpenThemeData(*this, L"LISTVIEW");
            GetThemeMetric(hTheme, pLVCD->nmcd.hdc, LVP_LISTITEM, LISS_NORMAL, TMT_BORDERSIZE, &borderWidth);
            CloseThemeData(hTheme);
        }
        else
        {
            borderWidth = GetSystemMetrics(SM_CXBORDER);
        }

        if (ListView_GetExtendedListViewStyle(hListControl) & LVS_EX_CHECKBOXES)
        {
            // I'm not very happy about this fixed margin here
            // but I haven't found a way to ask the system what
            // the margin really is.
            // At least it works on XP/Vista/win7, and even with
            // increased font sizes
            leftmargin = 4;
        }

        LVITEM item = { 0 };
        item.iItem = (int)pLVCD->nmcd.dwItemSpec;
        item.iSubItem = 0;
        item.mask = LVIF_IMAGE | LVIF_STATE;
        item.stateMask = (UINT)-1;
        ListView_GetItem(hListControl, &item);

        // draw the icon for the first column
        if (pLVCD->iSubItem == 0)
        {
            rect = boundsRC;
            rect.right = rect.left + ListView_GetColumnWidth(hListControl, 0) - 2 * borderWidth;
            rect.left = iconRC.left;

            if (item.iImage >= 0)
            {
                POINT pt;
                pt.x = rect.left;
                pt.y = rect.top;
                HIMAGELIST hImgList = ListView_GetImageList(hListControl, LVSIL_SMALL);
                ImageList_Draw(hImgList, item.iImage, pLVCD->nmcd.hdc, pt.x, pt.y, ILD_TRANSPARENT);
                leftmargin -= iconRC.left;
            }
            else
            {
                RECT irc = boundsRC;
                irc.left += borderWidth;
                irc.right = iconRC.left;

                int state = 0;
                if (item.state & LVIS_SELECTED)
                {
                    if (ListView_GetHotItem(hListControl) == item.iItem)
                        state = CBS_CHECKEDHOT;
                    else
                        state = CBS_CHECKEDNORMAL;
                }
                else
                {
                    if (ListView_GetHotItem(hListControl) == item.iItem)
                        state = CBS_UNCHECKEDHOT;
                }
                if ((state) && (ListView_GetExtendedListViewStyle(hListControl) & LVS_EX_CHECKBOXES))
                {
                    HTHEME hTheme = OpenThemeData(*this, L"BUTTON");
                    DrawThemeBackground(hTheme, pLVCD->nmcd.hdc, BP_CHECKBOX, state, &irc, NULL);
                    CloseThemeData(hTheme);
                }
            }
        }
        InflateRect(&rect, -(2 * borderWidth), 0);

        rect.left += leftmargin;
        RECT rc = rect;

        HTHEME hTheme = OpenThemeData(hListControl, L"Explorer");
        // is the column left- or right-aligned? (we don't handle centered (yet))
        LVCOLUMN Column;
        Column.mask = LVCF_FMT;
        ListView_GetColumn(hListControl, pLVCD->iSubItem, &Column);
        if (Column.fmt & LVCFMT_RIGHT)
        {
            DrawText(pLVCD->nmcd.hdc, text.c_str(), -1, &rc, DT_CALCRECT | DT_SINGLELINE |
                     DT_VCENTER | DT_NOPREFIX | DT_END_ELLIPSIS);
            rect.left = rect.right - (rc.right - rc.left);
            if (!IsAppThemed() /* || SysInfo::Instance().IsXP() */)
            {
                rect.left += 2 * borderWidth;
                rect.right += 2 * borderWidth;
            }
        }
        COLORREF textColor = pLVCD->clrText;

        Scintilla::Sci_CharacterRange r;
        r.cpMin = (long)start;
        r.cpMax = (long)min(end, text.size());  // regex search can match over several lines, but here we only draw one line

        SetTextColor(pLVCD->nmcd.hdc, textColor);
        SetBkMode(pLVCD->nmcd.hdc, TRANSPARENT);
        rc = rect;
        if (r.cpMin - drawPos)
        {
            DrawText(pLVCD->nmcd.hdc, &text[drawPos], r.cpMin - drawPos, &rc,
                     DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX | DT_END_ELLIPSIS);
            DrawText(pLVCD->nmcd.hdc, &text[drawPos], r.cpMin - drawPos, &rc,
                     DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX | DT_END_ELLIPSIS | DT_CALCRECT);
            rect.left = rc.right;
        }
        rc = rect;
        drawPos = r.cpMin;
        if (r.cpMax - drawPos)
        {
            SetTextColor(pLVCD->nmcd.hdc, RGB(255, 0, 0));
            DrawText(pLVCD->nmcd.hdc, &text[drawPos], r.cpMax - drawPos, &rc,
                     DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX | DT_END_ELLIPSIS);
            DrawText(pLVCD->nmcd.hdc, &text[drawPos], r.cpMax - drawPos, &rc,
                     DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX | DT_END_ELLIPSIS | DT_CALCRECT);
            rect.left = rc.right;
            SetTextColor(pLVCD->nmcd.hdc, textColor);
        }
        rc = rect;
        drawPos = r.cpMax;
        if ((int)text.size() > drawPos)
            DrawText(pLVCD->nmcd.hdc, text.substr(drawPos).c_str(), -1, &rc,
                     DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX | DT_END_ELLIPSIS);
        CloseThemeData(hTheme);
        return CDRF_SKIPDEFAULT;
    }
    return CDRF_DODEFAULT;
}

RECT CFindReplaceDlg::DrawListColumnBackground(NMLVCUSTOMDRAW * pLVCD)
{
    HWND hListControl = pLVCD->nmcd.hdr.hwndFrom;

    // Get the selected state of the
    // item being drawn.
    LVITEM   rItem;
    SecureZeroMemory(&rItem, sizeof(LVITEM));
    rItem.mask = LVIF_STATE;
    rItem.iItem = (int)pLVCD->nmcd.dwItemSpec;
    rItem.stateMask = LVIS_SELECTED | LVIS_FOCUSED;
    ListView_GetItem(hListControl, &rItem);


    RECT rect;
    ListView_GetSubItemRect(hListControl, (int)pLVCD->nmcd.dwItemSpec, pLVCD->iSubItem, LVIR_BOUNDS, &rect);

    // the rect we get for column 0 always extends over the whole row instead of just
    // the column itself. Since we must not redraw the background for the whole row (other columns
    // might not be asked to redraw), we have to find the right border of the column
    // another way.
    if (pLVCD->iSubItem == 0)
        rect.right = ListView_GetColumnWidth(hListControl, 0);

    // Fill the background
    if (IsAppThemed() /* && SysInfo::Instance().IsVistaOrLater() */)
    {
        HTHEME hTheme = OpenThemeData(*this, L"Explorer");
        int state = LISS_NORMAL;
        if (rItem.state & LVIS_SELECTED)
        {
            if (::GetFocus() == hListControl)
                state = LISS_SELECTED;
            else
                state = LISS_SELECTEDNOTFOCUS;
        }

        if (IsThemeBackgroundPartiallyTransparent(hTheme, LVP_LISTDETAIL, state))
            DrawThemeParentBackground(*this, pLVCD->nmcd.hdc, &rect);

        DrawThemeBackground(hTheme, pLVCD->nmcd.hdc, LVP_LISTDETAIL, state, &rect, NULL);
        CloseThemeData(hTheme);
    }
    else
    {
        HBRUSH brush;
        if (rItem.state & LVIS_SELECTED)
        {
            if (::GetFocus() == hListControl)
                brush = ::CreateSolidBrush(::GetSysColor(COLOR_HIGHLIGHT));
            else
                brush = ::CreateSolidBrush(::GetSysColor(COLOR_BTNFACE));
        }
        else
        {
            brush = ::CreateSolidBrush(::GetSysColor(COLOR_WINDOW));
        }
        if (brush == NULL)
            return rect;

        ::FillRect(pLVCD->nmcd.hdc, &rect, brush);
        ::DeleteObject(brush);
    }

    return rect;
}

void CFindReplaceDlg::ShowResults(bool bShow)
{
    RECT windowRect, infoRect;
    GetWindowRect(*this, &windowRect);
    GetWindowRect(GetDlgItem(*this, IDC_SEARCHINFO), &infoRect);
    int height = infoRect.bottom - windowRect.top;
    if (bShow)
    {
        MoveWindow(*this, windowRect.left, windowRect.top, windowRect.right - windowRect.left, height + 300, TRUE);
        ShowWindow(GetDlgItem(*this, IDC_FINDRESULTS), SW_SHOW);
    }
    else
    {
        MoveWindow(*this, windowRect.left, windowRect.top, windowRect.right - windowRect.left, height + 15, TRUE);
        ShowWindow(GetDlgItem(*this, IDC_FINDRESULTS), SW_HIDE);
    }
}

LRESULT CFindReplaceDlg::DoCommand(int id, int msg)
{
    switch (id)
    {
    case IDCANCEL:
        ShowResults(false);
        ShowWindow(*this, SW_HIDE);
        sHighlightString.clear();
        DocScrollUpdate();
        break;
    case IDC_FINDBTN:
        {
            if (GetFocus() == GetDlgItem(*this, IDC_FINDRESULTS))
            {
                int selIndex = ListView_GetSelectionMark(GetDlgItem(*this, IDC_FINDRESULTS));
                if (!m_searchResults.empty() && (selIndex >= 0) && (selIndex < (int)m_searchResults.size()))
                {
                    int tab = GetActiveTabIndex();
                    int docID = GetDocIDFromTabIndex(tab);
                    if (docID != m_searchResults[selIndex].docID)
                    {
                        int tabtoopen = GetTabIndexFromDocID(m_searchResults[selIndex].docID);
                        if (tabtoopen >= 0)
                            TabActivateAt(tabtoopen);
                        else
                            break;
                    }
                    ScintillaCall(SCI_GOTOLINE, m_searchResults[selIndex].line);
                    Center((long)m_searchResults[selIndex].pos, (long)m_searchResults[selIndex].pos);
                }
            }
            DoSearch();
        }
        break;
    case IDC_FINDALL:
    case IDC_FINDALLINTABS:
        DoSearchAll(id);
        ShowResults(true);
        break;
    case IDC_REPLACEALLBTN:
    case IDC_REPLACEBTN:
    case IDC_REPLACEALLINTABSBTN:
    {
        DoReplace(id);
    }
        break;
    case IDC_MATCHREGEX:
        {
            ResString sInfo(hRes, IDS_REGEXTOOLTIP);
            bool bChecked = !!IsDlgButtonChecked(*this, IDC_MATCHREGEX);
            // get the edit control of the replace combo
            COMBOBOXINFO cinfo = { 0 };
            cinfo.cbSize = sizeof(COMBOBOXINFO);
            GetComboBoxInfo(GetDlgItem(*this, IDC_REPLACECOMBO), &cinfo);
            AddToolTip(cinfo.hwndCombo, bChecked ? (LPCWSTR)sInfo : L"");
            AddToolTip(cinfo.hwndItem, bChecked ? (LPCWSTR)sInfo : L"");
            AddToolTip(cinfo.hwndList, bChecked ? (LPCWSTR)sInfo : L"");
            AddToolTip(IDC_LABEL2, bChecked ? (LPCWSTR)sInfo : L"");
        }
        break;
    case IDC_SEARCHCOMBO:
        {
            if ((msg == CBN_EDITCHANGE) && (IsDlgButtonChecked(*this, IDC_MATCHREGEX)))
                CheckRegex();
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

    size_t len = ScintillaCall(SCI_GETLENGTH);
    if ((len == 0) && (sFindString.empty()))
        return; // empty document and empty search string, nothing to replace

    size_t selStart = ScintillaCall(SCI_GETSELECTIONSTART);
    size_t selEnd = ScintillaCall(SCI_GETSELECTIONEND);
    size_t linestart = ScintillaCall(SCI_LINEFROMPOSITION, selStart);
    size_t lineend = ScintillaCall(SCI_LINEFROMPOSITION, selEnd);
    int wrapcount = 1;
    if (linestart == lineend)
        wrapcount = (int)ScintillaCall(SCI_WRAPCOUNT, linestart);
    bool bReplaceOnlyInSelection = ((linestart != lineend) || ((wrapcount > 1) && (selEnd - selStart > 20))) && (id == IDC_REPLACEALLBTN);

    if (bReplaceOnlyInSelection)
    {
        ScintillaCall(SCI_SETTARGETSTART, selStart);
        ScintillaCall(SCI_SETTARGETEND, selEnd);
    }
    else
    {
        ScintillaCall(SCI_SETTARGETSTART, id == IDC_REPLACEBTN ? selStart : 0);
        ScintillaCall(SCI_SETTARGETEND, ScintillaCall(SCI_GETLENGTH));
    }

    auto findText = GetDlgItemText(IDC_SEARCHCOMBO);
    sFindString = CUnicodeUtils::StdGetUTF8(findText.get());
    sHighlightString = sFindString;
    auto replaceText = GetDlgItemText(IDC_REPLACECOMBO);
    std::string sReplaceString = CUnicodeUtils::StdGetUTF8(replaceText.get());

    nSearchFlags = 0;
    nSearchFlags |= IsDlgButtonChecked(*this, IDC_MATCHCASE)  ? SCFIND_MATCHCASE : 0;
    nSearchFlags |= IsDlgButtonChecked(*this, IDC_MATCHWORD)  ? SCFIND_WHOLEWORD : 0;
    nSearchFlags |= IsDlgButtonChecked(*this, IDC_MATCHREGEX) ? SCFIND_REGEXP    : 0;

    if (nSearchFlags & SCFIND_REGEXP)
    {
        sReplaceString = UnEscape(sReplaceString);
    }

    int replaceCount = 0;
    if (id == IDC_REPLACEALLINTABSBTN)
    {
        int tabcount = GetTabCount();
        for (int i = 0; i < tabcount; ++i)
        {
            int docID = GetDocIDFromTabIndex(i);
            CDocument doc = GetDocumentFromID(docID);
            int rcount = ReplaceDocument(doc, sFindString, sReplaceString, nSearchFlags);
            if (rcount)
            {
                replaceCount += rcount;
                SetDocument(docID, doc);
                UpdateTab(i);
            }
        }
    }
    else
    {
        ScintillaCall(SCI_SETSEARCHFLAGS, nSearchFlags);
        sptr_t findRet = -1;
        ScintillaCall(SCI_BEGINUNDOACTION);
        do
        {
            findRet = ScintillaCall(SCI_SEARCHINTARGET, sFindString.length(), (sptr_t)sFindString.c_str());
            if ((findRet == -1) && (id == IDC_REPLACEBTN))
            {
                SetInfoText(IDS_FINDRETRYWRAP);
                // retry from the start of the doc
                ScintillaCall(SCI_SETTARGETSTART, 0);
                ScintillaCall(SCI_SETTARGETEND, ScintillaCall(SCI_GETCURRENTPOS));
                findRet = ScintillaCall(SCI_SEARCHINTARGET, sFindString.length(), (sptr_t)sFindString.c_str());
            }
            if (findRet >= 0)
            {
                ScintillaCall((nSearchFlags & SCFIND_REGEXP) != 0 ? SCI_REPLACETARGETRE : SCI_REPLACETARGET, sReplaceString.length(), (sptr_t)sReplaceString.c_str());
                ++replaceCount;
                long tstart = (long)ScintillaCall(SCI_GETTARGETSTART);
                long tend = (long)ScintillaCall(SCI_GETTARGETEND);
                if (id == IDC_REPLACEBTN)
                    Center(tstart, tend);
                if ((tend > tstart) || (sReplaceString.empty()))
                    ScintillaCall(SCI_SETTARGETSTART, tend);
                else
                    ScintillaCall(SCI_SETTARGETSTART, tend + 1);
                if (bReplaceOnlyInSelection)
                    ScintillaCall(SCI_SETTARGETEND, selEnd);
                else
                    ScintillaCall(SCI_SETTARGETEND, ScintillaCall(SCI_GETLENGTH));
            }
        } while ((id == IDC_REPLACEALLBTN) && (findRet != -1));
        ScintillaCall(SCI_ENDUNDOACTION);
    }
    if ((id == IDC_REPLACEALLBTN) || (id == IDC_REPLACEALLINTABSBTN))
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
    sHighlightString = sFindString;
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

void CFindReplaceDlg::DoSearchAll(int id)
{
    // to prevent having each document activate the tab while searching,
    // we use a dummy scintilla control and do the search in there
    // instead of using the active control.
    SetDlgItemText(*this, IDC_SEARCHINFO, L"");
    m_searchResults.clear();
    InitResultList();

    auto findText = GetDlgItemText(IDC_SEARCHCOMBO);
    std::string searchfor = CUnicodeUtils::StdGetUTF8(findText.get());
    int searchflags = 0;
    searchflags |= IsDlgButtonChecked(*this, IDC_MATCHCASE) ? SCFIND_MATCHCASE : 0;
    searchflags |= IsDlgButtonChecked(*this, IDC_MATCHWORD) ? SCFIND_WHOLEWORD : 0;
    searchflags |= IsDlgButtonChecked(*this, IDC_MATCHREGEX) ? SCFIND_REGEXP : 0;

    if (id == IDC_FINDALL && HasActiveDocument())
    {
        int docId = GetDocIDFromTabIndex(GetActiveTabIndex());
        CDocument doc = GetActiveDocument();
        SearchDocument(docId, doc, searchfor, searchflags);
    }
    else if (id == IDC_FINDALLINTABS)
    {
        int tabcount = GetTabCount();
        for (int i = 0; i < tabcount; ++i)
        {
            int docID = GetDocIDFromTabIndex(i);
            CDocument doc = GetDocumentFromID(docID);
            SearchDocument(docID, doc, searchfor, searchflags);
        }
    }
    InitResultList();
    int maxSearch = (int)CIniSettings::Instance().GetInt64(L"searchreplace", L"maxsearch", 20);
    std::wstring sFindStringW = CUnicodeUtils::StdGetUnicode(searchfor);
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
    // go to the first search result
    if (!m_searchResults.empty())
    {
        int tab = GetActiveTabIndex();
        int docID = GetDocIDFromTabIndex(tab);
        if (docID == m_searchResults[0].docID)
        {
            ScintillaCall(SCI_GOTOLINE, m_searchResults[0].line);
            Center((long)m_searchResults[0].pos, (long)m_searchResults[0].pos);
        }
    }
    std::wstring sInfo;
    if (id == IDC_FINDALLINTABS)
    {
        ResString rInfo(hRes, IDS_FINDRESULT_COUNTALL);
        sInfo = CStringUtils::Format(rInfo, (int)m_searchResults.size(), GetTabCount());
    }
    else
    {
        ResString rInfo(hRes, IDS_FINDRESULT_COUNT);
        sInfo = CStringUtils::Format(rInfo, (int)m_searchResults.size());
    }
    SetDlgItemText(*this, IDC_SEARCHINFO, sInfo.c_str());

    SendMessage(*this, WM_NEXTDLGCTL, (WPARAM)GetDlgItem(*this, IDC_FINDRESULTS), TRUE);
}

void CFindReplaceDlg::SearchDocument(int docID, const CDocument& doc, const std::string& searchfor, int searchflags)
{
    m_searchWnd.Call(SCI_SETSTATUS, SC_STATUS_OK);   // reset error status
    m_searchWnd.Call(SCI_CLEARALL);
    m_searchWnd.Call(SCI_SETDOCPOINTER, 0, doc.m_document);
    m_searchWnd.Call(SCI_SETREADONLY, true);
    m_searchWnd.Call(SCI_SETCODEPAGE, CP_UTF8);

    Scintilla::Sci_TextToFind ttf = { 0 };
    ttf.chrg.cpMin = 0;
    ttf.chrg.cpMax = (long)m_searchWnd.Call(SCI_GETLENGTH);
    auto findText = GetDlgItemText(IDC_SEARCHCOMBO);
    ttf.lpstrText = const_cast<char*>(searchfor.c_str());

    sptr_t findRet = -1;
    do
    {
        findRet = m_searchWnd.Call(SCI_FINDTEXT, searchflags, (sptr_t)&ttf);
        if (findRet >= 0)
        {
            CSearchResult result;
            result.pos = ttf.chrgText.cpMin;
            result.line = m_searchWnd.Call(SCI_LINEFROMPOSITION, ttf.chrgText.cpMin);
            auto linepos = m_searchWnd.Call(SCI_POSITIONFROMLINE, result.line);

            result.posInLineStart = ttf.chrgText.cpMin - linepos;
            result.posInLineEnd = ttf.chrgText.cpMax - linepos;
            result.docID = docID;

            size_t linesize = m_searchWnd.Call(SCI_GETLINE, result.line, 0);
            auto pLine = std::make_unique<char[]>(linesize + 1);
            m_searchWnd.Call(SCI_GETLINE, result.line, (sptr_t)pLine.get());
            pLine[linesize] = 0;
            // remove EOLs
            while ((pLine[linesize-1] == '\n') || (pLine[linesize-1] == '\r'))
                --linesize;
            pLine[linesize] = 0;

            result.lineText = CUnicodeUtils::StdGetUnicode(pLine.get());
            m_searchResults.push_back(result);

            if (ttf.chrg.cpMin >= ttf.chrgText.cpMax)
                break;
            ttf.chrg.cpMin = ttf.chrgText.cpMax + 1;
        }
    } while (findRet >= 0);
    m_searchWnd.Call(SCI_SETREADONLY, false);
    m_searchWnd.Call(SCI_SETSAVEPOINT);
    m_searchWnd.Call(SCI_SETDOCPOINTER, 0, 0);
}

int CFindReplaceDlg::ReplaceDocument(CDocument& doc, const std::string& sFindString, const std::string& sReplaceString, int searchflags)
{
    m_searchWnd.Call(SCI_SETSTATUS, SC_STATUS_OK);   // reset error status
    m_searchWnd.Call(SCI_CLEARALL);
    m_searchWnd.Call(SCI_SETDOCPOINTER, 0, doc.m_document);
    m_searchWnd.Call(SCI_SETCODEPAGE, CP_UTF8);

    m_searchWnd.Call(SCI_SETTARGETSTART, 0);
    m_searchWnd.Call(SCI_SETTARGETEND, m_searchWnd.Call(SCI_GETLENGTH));

    m_searchWnd.Call(SCI_SETSEARCHFLAGS, searchflags);
    sptr_t findRet = -1;
    int replaceCount = 0;
    m_searchWnd.Call(SCI_BEGINUNDOACTION);
    do
    {
        findRet = m_searchWnd.Call(SCI_SEARCHINTARGET, sFindString.length(), (sptr_t)sFindString.c_str());
        if (findRet >= 0)
        {
            m_searchWnd.Call((searchflags & SCFIND_REGEXP) != 0 ? SCI_REPLACETARGETRE : SCI_REPLACETARGET, sReplaceString.length(), (sptr_t)sReplaceString.c_str());
            ++replaceCount;
            long tstart = (long)m_searchWnd.Call(SCI_GETTARGETSTART);
            long tend = (long)m_searchWnd.Call(SCI_GETTARGETEND);
            if ((tend > tstart) || (sReplaceString.empty()))
                m_searchWnd.Call(SCI_SETTARGETSTART, tend);
            else
                m_searchWnd.Call(SCI_SETTARGETSTART, tend + 1);

            m_searchWnd.Call(SCI_SETTARGETEND, m_searchWnd.Call(SCI_GETLENGTH));
            doc.m_bIsDirty = true;
        }
    } while (findRet != -1);
    m_searchWnd.Call(SCI_ENDUNDOACTION);

    m_searchWnd.Call(SCI_SETDOCPOINTER, 0, 0);
    return replaceCount;
}

void CFindReplaceDlg::InitResultList()
{
    HWND hListControl = GetDlgItem(*this, IDC_FINDRESULTS);
    DWORD exStyle = LVS_EX_DOUBLEBUFFER | LVS_EX_INFOTIP | LVS_EX_FULLROWSELECT;
    SetWindowTheme(hListControl, L"Explorer", NULL);
    ListView_SetItemCount(hListControl, 0);

    int c = Header_GetItemCount(ListView_GetHeader(hListControl)) - 1;
    while (c >= 0)
        ListView_DeleteColumn(hListControl, c--);

    ListView_SetExtendedListViewStyle(hListControl, exStyle);

    ResString sFile(hRes, IDS_FINDRESULT_HEADERFILE);
    ResString sLine(hRes, IDS_FINDRESULT_HEADERLINE);
    ResString sLineText(hRes, IDS_FINDRESULT_HEADERLINETEXT);

    LVCOLUMN lvc = { 0 };
    lvc.mask = LVCF_TEXT | LVCF_FMT;
    lvc.fmt = LVCFMT_LEFT;
    lvc.cx = -1;
    lvc.pszText = const_cast<LPWSTR>((LPCWSTR)sFile);
    ListView_InsertColumn(hListControl, 0, &lvc);
    lvc.pszText = const_cast<LPWSTR>((LPCWSTR)sLine);
    lvc.fmt = LVCFMT_RIGHT;
    ListView_InsertColumn(hListControl, 1, &lvc);
    lvc.fmt = LVCFMT_LEFT;
    lvc.pszText = const_cast<LPWSTR>((LPCWSTR)sLineText);
    ListView_InsertColumn(hListControl, 2, &lvc);

    ListView_SetItemCountEx(hListControl, m_searchResults.size(), LVSICF_NOSCROLL);

    ListView_SetColumnWidth(hListControl, 0, 100);
    ListView_SetColumnWidth(hListControl, 1, 40);
    ListView_SetColumnWidth(hListControl, 2, LVSCW_AUTOSIZE_USEHEADER);
}

void CFindReplaceDlg::CheckRegex()
{
    try
    {
        auto findText = GetDlgItemText(IDC_SEARCHCOMBO);
        const std::wregex ignex(findText.get(), std::regex_constants::icase | std::regex_constants::ECMAScript);
        SetInfoText(IDS_REGEX_OK);
    }
    catch (std::exception)
    {
        SetInfoText(IDS_REGEX_NOTOK);
    }
}

std::string CFindReplaceDlg::UnEscape(const std::string& str)
{
    int i = 0, j = 0;
    int charLeft = (int)str.length();
    char current;
    auto result = std::make_unique<char[]>(str.length() + 1);
    while (i < (int)str.length())
    {
        current = str[i];
        --charLeft;
        if (current == '\\' && charLeft)
        {
            // possible escape sequence
            ++i;
            --charLeft;
            current = str[i];
            switch (current)
            {
                case 'r':
                    result[j] = '\r';
                    break;
                case 'n':
                    result[j] = '\n';
                    break;
                case '0':
                    result[j] = '\0';
                    break;
                case 't':
                    result[j] = '\t';
                    break;
                case '\\':
                    result[j] = '\\';
                    break;
                case 'b':
                case 'd':
                case 'o':
                case 'x':
                case 'u':
                {
                    int size = 0, base = 0;
                    if (current == 'b')
                    {
                        // 11111111
                        size = 8, base = 2;
                    }
                    else if (current == 'o')
                    {
                        // 377
                        size = 3, base = 8;
                    }
                    else if (current == 'd')
                    {
                        // 255
                        size = 3, base = 10;
                    }
                    else if (current == 'x')
                    {
                        // 0xFF
                        size = 2, base = 16;
                    }
                    else if (current == 'u')
                    {
                        // 0xCDCD
                        size = 4, base = 16;
                    }
                    if (charLeft >= size)
                    {
                        int res = 0;
                        if (ReadBase(str.c_str() + (i + 1), &res, base, size))
                        {
                            result[j] = (char)res;
                            i += size;
                            break;
                        }
                    }
                    // not enough chars to make parameter, use default method as fallback
                }
                default:
                {
                    // unknown sequence, treat as regular text
                    result[j] = '\\';
                    ++j;
                    result[j] = current;
                    break;
                }
            }
        }
        else
        {
            result[j] = str[i];
        }
        ++i;
        ++j;
    }
    result[j] = 0;
    return result.get();
}

bool CFindReplaceDlg::ReadBase(const char * str, int * value, int base, int size)
{
    int i = 0, temp = 0;
    *value = 0;
    char max = '0' + (char)base - 1;
    char current;
    while (i < size)
    {
        current = str[i];
        if (current >= 'A')
        {
            current &= 0xdf;
            current -= ('A' - '0' - 10);
        }
        else if (current > '9')
            return false;

        if (current >= '0' && current <= max)
        {
            temp *= base;
            temp += (current - '0');
        }
        else
        {
            return false;
        }
        ++i;
    }
    *value = temp;
    return true;
}


bool CCmdFindReplace::Execute()
{
    if (m_pFindReplaceDlg == nullptr)
        m_pFindReplaceDlg = new CFindReplaceDlg(m_Obj);

    size_t selStart = ScintillaCall(SCI_GETSELECTIONSTART);
    size_t selEnd = ScintillaCall(SCI_GETSELECTIONEND);
    size_t linestart = ScintillaCall(SCI_LINEFROMPOSITION, selStart);
    size_t lineend = ScintillaCall(SCI_LINEFROMPOSITION, selEnd);

    if (linestart == lineend)
    {
        std::string sSelText = GetSelectedText();

        if (sSelText.empty())
        {
            // get the current word instead
            long currentPos = (long)ScintillaCall(SCI_GETCURRENTPOS);
            long startPos = (long)ScintillaCall(SCI_WORDSTARTPOSITION, currentPos, true);
            long endPos = (long)ScintillaCall(SCI_WORDENDPOSITION, currentPos, true);
            auto textbuf = std::make_unique<char[]>(endPos - startPos + 1);
            Scintilla::Sci_TextRange range;
            range.chrg.cpMin = startPos;
            range.chrg.cpMax = endPos;
            range.lpstrText = textbuf.get();
            ScintillaCall(SCI_GETTEXTRANGE, 0, (sptr_t)&range);
            sSelText = textbuf.get();
        }
        m_pFindReplaceDlg->ShowModeless(hRes, IDD_FINDREPLACEDLG, GetHwnd());
        if (!sSelText.empty() && (sSelText.size() < 50))
        {
            SetDlgItemText(*m_pFindReplaceDlg, IDC_SEARCHCOMBO, CUnicodeUtils::StdGetUnicode(sSelText).c_str());
            SetFocus(GetDlgItem(*m_pFindReplaceDlg, IDC_SEARCHCOMBO));
        }
    }
    else
        m_pFindReplaceDlg->ShowModeless(hRes, IDD_FINDREPLACEDLG, GetHwnd());

    return true;
}

void CCmdFindReplace::ScintillaNotify( Scintilla::SCNotification * pScn )
{
    switch (pScn->nmhdr.code)
    {
    case SCN_UPDATEUI:
        {
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
            ScintillaCall(SCI_INDICATORCLEARRANGE, startstylepos, len - 1);

            if (sHighlightString.empty())
            {
                m_lastSelText.clear();
                DocScrollClear(DOCSCROLLTYPE_SEARCHTEXT);
                return;
            }

            Scintilla::Sci_TextToFind FindText;
            FindText.chrg.cpMin = startstylepos;
            FindText.chrg.cpMax = endstylepos;
            FindText.lpstrText = const_cast<char*>(sHighlightString.c_str());
            while (ScintillaCall(SCI_FINDTEXT, nSearchFlags, (LPARAM)&FindText) >= 0)
            {
                ScintillaCall(SCI_INDICATORFILLRANGE, FindText.chrgText.cpMin, FindText.chrgText.cpMax-FindText.chrgText.cpMin);
                if (FindText.chrg.cpMin >= FindText.chrgText.cpMax)
                    break;
                FindText.chrg.cpMin = FindText.chrgText.cpMax;
            }

            if (m_lastSelText.empty() || m_lastSelText.compare(sHighlightString) || (nSearchFlags != lastSearchFlags))
            {
                DocScrollClear(DOCSCROLLTYPE_SEARCHTEXT);
                Scintilla::Sci_TextToFind FindText;
                FindText.chrg.cpMin = 0;
                FindText.chrg.cpMax = (long)ScintillaCall(SCI_GETLENGTH);
                FindText.lpstrText = const_cast<char*>(sHighlightString.c_str());
                while (ScintillaCall(SCI_FINDTEXT, nSearchFlags, (LPARAM)&FindText) >= 0)
                {
                    size_t line = ScintillaCall(SCI_LINEFROMPOSITION, FindText.chrgText.cpMin);
                    DocScrollAddLineColor(DOCSCROLLTYPE_SEARCHTEXT, line, RGB(200,200,0));
                    if (FindText.chrg.cpMin >= FindText.chrgText.cpMax)
                        break;
                    FindText.chrg.cpMin = FindText.chrgText.cpMax;
                }
                DocScrollUpdate();
            }
            m_lastSelText = sHighlightString.c_str();
            lastSearchFlags = nSearchFlags;
        }
        break;
    default:
        break;
    }
}

void CCmdFindReplace::TabNotify(TBHDR * ptbhdr)
{
    if (ptbhdr->hdr.code == TCN_SELCHANGE)
    {
        m_lastSelText.clear();
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
    sHighlightString.clear();
    int selTextLen = (int)ScintillaCall(SCI_GETSELTEXT);
    if (selTextLen == 0)
    {
        DocScrollUpdate();
        return false;
    }
    std::string seltext = GetSelectedText();
    if (seltext.empty())
    {
        DocScrollUpdate();
        return false;
    }
    sFindString = seltext;
    sHighlightString = sFindString;

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
    DocScrollUpdate();
    return true;
}

bool CCmdFindSelectedPrev::Execute()
{
    sHighlightString.clear();
    int selTextLen = (int)ScintillaCall(SCI_GETSELTEXT);
    if (selTextLen == 0)
    {
        DocScrollUpdate();
        return false;
    }

    std::string seltext = GetSelectedText();
    if (seltext.empty())
    {
        DocScrollUpdate();
        return false;
    }
    sFindString = seltext;
    sHighlightString = sFindString;

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
    DocScrollUpdate();
    return true;
}
