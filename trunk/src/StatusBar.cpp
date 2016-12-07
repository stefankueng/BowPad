// This file is part of BowPad.
//
// Copyright (C) 2013-2014, 2016 - Stefan Kueng
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
#include "StatusBar.h"
#include "Theme.h"

static void DrawSizeGrip(HDC hdc, LPCRECT lpRect)
{
    HPEN hPenFace, hPenShadow, hPenHighlight, hOldPen;
    POINT pt;
    INT i;

    pt.x = lpRect->right - 1;
    pt.y = lpRect->bottom - 1;

    hPenFace = CreatePen( PS_SOLID, 1, CTheme::Instance().GetThemeColor(GetSysColor(COLOR_3DFACE)));
    hOldPen = (HPEN)SelectObject( hdc, hPenFace );
    MoveToEx(hdc, pt.x - 12, pt.y, nullptr);
    LineTo(hdc, pt.x, pt.y);
    LineTo(hdc, pt.x, pt.y - 13);

    pt.x--;
    pt.y--;

    hPenShadow = CreatePen( PS_SOLID, 1, CTheme::Instance().GetThemeColor(GetSysColor(COLOR_3DSHADOW)));
    SelectObject( hdc, hPenShadow );
    for (i = 1; i < 11; i += 4)
    {
        MoveToEx (hdc, pt.x - i, pt.y, nullptr);
        LineTo (hdc, pt.x + 1, pt.y - i - 1);

        MoveToEx (hdc, pt.x - i - 1, pt.y, nullptr);
        LineTo (hdc, pt.x + 1, pt.y - i - 2);
    }

    hPenHighlight = CreatePen( PS_SOLID, 1, CTheme::Instance().GetThemeColor(GetSysColor(COLOR_3DHIGHLIGHT)));
    SelectObject( hdc, hPenHighlight );
    for (i = 3; i < 13; i += 4)
    {
        MoveToEx (hdc, pt.x - i, pt.y, nullptr);
        LineTo (hdc, pt.x + 1, pt.y - i - 1);
    }

    SelectObject(hdc, hOldPen);
    DeleteObject( hPenFace );
    DeleteObject( hPenShadow );
    DeleteObject( hPenHighlight );
}

static LRESULT Refresh(HWND hWnd, HDC hdc)
{
    if (!IsWindowVisible(hWnd))
        return 0;

    RECT rect;
    GetClientRect(hWnd, &rect);

    const auto faceClr = CTheme::Instance().GetThemeColor(GetSysColor(COLOR_3DFACE));
    HBRUSH hbrBk = CreateSolidBrush(faceClr);
    FillRect(hdc, &rect, hbrBk);

    auto textFgc = CTheme::Instance().GetThemeColor(GetSysColor(COLOR_WINDOWTEXT));
    auto textBgc = faceClr;

    HFONT hFont = (HFONT)SendMessage(hWnd, WM_GETFONT, 0, 0);
    HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);

    const int numparts = (int)SendMessage(hWnd, SB_GETPARTS, 0, 0);
    std::wstring textbuf;
    for (int itemID = 0; itemID < numparts; itemID++)
    {
        RECT rcPart;
        SendMessage(hWnd, SB_GETRECT, itemID, (LPARAM)&rcPart);
        if (rcPart.right < rcPart.left)
            continue;

        if (!RectVisible(hdc, &rcPart))
            continue;

        DrawEdge(hdc, &rcPart, BDR_SUNKENOUTER, BF_RECT | BF_ADJUST);

        int x = 0;
        HICON hIcon = (HICON)SendMessage(hWnd, SB_GETICON, itemID, 0);
        if (hIcon)
        {
            INT cy = rcPart.bottom - rcPart.top;
            DrawIconEx(hdc, rcPart.left + 2, rcPart.top, hIcon, 0, 0, 0, 0, DI_NORMAL);
            x = 2 + cy;
        }

        rcPart.left += x;
        const int textlen = (int)SendMessage(hWnd, SB_GETTEXTLENGTH, itemID, 0);
        textbuf.resize(textlen + 1);
        SendMessage(hWnd, SB_GETTEXT, itemID, (LPARAM)&textbuf[0]);
        textbuf.resize(textlen);
        SetTextColor(hdc, textFgc);
        SetBkColor(hdc, textBgc);
        InflateRect(&rcPart, -2, 0);
        DrawText(hdc, textbuf.c_str(), -1, &rcPart, DT_LEFT | DT_SINGLELINE | DT_VCENTER);
    }
    SelectObject(hdc, hOldFont);
    DeleteObject(hbrBk);

    if (GetWindowLongW(hWnd, GWL_STYLE) & SBARS_SIZEGRIP)
        DrawSizeGrip(hdc, &rect);

    return 0;
}

bool CStatusBar::SetText(const TCHAR *str, const TCHAR *tooltip, int whichPart)
{
    if (tooltip)
        m_PartsTooltips[whichPart] = tooltip;
    else
        m_PartsTooltips[whichPart] = str;
    return (::SendMessage(*this, SB_SETTEXT, whichPart, (LPARAM)str) == TRUE);
}

bool CStatusBar::Init(HINSTANCE hInst, HWND hParent, std::initializer_list<int> parts)
{
    return Init(hInst, hParent, (int)parts.size(), std::cbegin(parts));
}

bool CStatusBar::Init(HINSTANCE /*hInst*/, HWND hParent, int numParts, const int parts[])
{
    InitCommonControls();

    CreateEx(WS_EX_COMPOSITED, WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP | SBARS_TOOLTIPS, hParent, 0, STATUSCLASSNAME);
    if (!*this)
        return false;

    assert(numParts > 0); // Must have 1 part even if -1.
    SendMessage(*this, SB_SETPARTS, (LPARAM) numParts, (WPARAM) parts);

    m_Parts = std::vector<int>(parts, parts + numParts);
    m_PartsTooltips = std::vector<std::wstring>(m_Parts.size());
    m_bHasOnlyFixedWidth = std::find(
        std::begin(m_Parts), std::end(m_Parts), -1) == std::end(m_Parts);

    RECT rcClient;
    GetClientRect(*this, &rcClient);
    m_height = rcClient.bottom - rcClient.top;

    return true;
}

LRESULT CALLBACK CStatusBar::WinMsgHandler( HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam )
{
    switch (uMsg)
    {
        case WM_NOTIFY:
        {
            LPNMTTDISPINFO lpnmtdi = (LPNMTTDISPINFO)lParam;
            if (lpnmtdi->hdr.code == TTN_GETDISPINFO)
            {
                DWORD mpos = GetMessagePos();
                POINT pt;
                pt.x = GET_X_LPARAM(mpos);
                pt.y = GET_Y_LPARAM(mpos);
                ScreenToClient(*this, &pt);
                for (size_t i = 0; i < m_Parts.size(); ++i)
                {
                    RECT rc;
                    SendMessage(*this, SB_GETRECT, (WPARAM)i, (LPARAM)&rc);
                    if (PtInRect(&rc, pt))
                    {
                        lpnmtdi->lpszText = const_cast<LPWSTR>(m_PartsTooltips[i].c_str());
                        SendMessage(lpnmtdi->hdr.hwndFrom, TTM_SETMAXTIPWIDTH, 0, 600);
                        break;
                    }
                }
            }
        }
            break;
        case WM_LBUTTONDBLCLK:
        case WM_LBUTTONUP:
        case WM_LBUTTONDOWN:
        case WM_CONTEXTMENU:
        {
            POINT pt;
            pt.x = GET_X_LPARAM(lParam);
            pt.y = GET_Y_LPARAM(lParam);
            if (uMsg == WM_CONTEXTMENU)
                ScreenToClient(*this, &pt);
            for (size_t i = 0; i < m_Parts.size(); ++i)
            {
                RECT rc;
                SendMessage(*this, SB_GETRECT, (WPARAM)i, (LPARAM)&rc);
                if (PtInRect(&rc, pt))
                {
                    SendMessage(::GetParent(*this), WM_STATUSBAR_MSG, uMsg, i);
                    break;
                }
            }
        }
            break;
    }
    if (CTheme::Instance().IsDarkTheme())
    {
        // only do custom drawing when in dark theme
        switch (uMsg)
        {
        case WM_PAINT:
            {
                PAINTSTRUCT ps;
                HDC hdc = BeginPaint(hwnd, &ps);
                Refresh(hwnd, hdc);
                EndPaint(hwnd, &ps);
                return 0;
            }
            break;
        case WM_ERASEBKGND:
            return TRUE;
        }
    }
    if (prevWndProc)
        return prevWndProc(hwnd, uMsg, wParam, lParam);
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

void CStatusBar::Resize()
{
    size_t nParts = m_Parts.size();
    assert(nParts > 0); // Must have 1 part, set in Init.
    if (m_bHasOnlyFixedWidth)
    {
        RECT rc;
        GetClientRect(*this, &rc);
        const int available_width = rc.right - rc.left;

        // Calculate new edges to match the new size reality.
        // Dynamic change does not alter our original design units in m_Parts.
        std::vector<int> sbParts;
        sbParts.reserve(nParts);
        int lastx = m_Parts.back();
        for (int right_edge : m_Parts)
            sbParts.push_back(right_edge * available_width / lastx);
        assert(sbParts.size() == m_Parts.size());
        SendMessage(*this, SB_SETPARTS, (WPARAM) sbParts.size(), (LPARAM)&sbParts[0]);
    }
}
