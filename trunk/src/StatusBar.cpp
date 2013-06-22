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
#include "StatusBar.h"
#include "AppUtils.h"
#include "Theme.h"

#include <Commctrl.h>

void DrawSizeGrip(HDC hdc, LPRECT lpRect)
{
    HPEN hPenFace, hPenShadow, hPenHighlight, hOldPen;
    POINT pt;
    INT i;

    pt.x = lpRect->right - 1;
    pt.y = lpRect->bottom - 1;

    hPenFace = CreatePen( PS_SOLID, 1, CTheme::Instance().GetThemeColor(GetSysColor(COLOR_3DFACE)));
    hOldPen = (HPEN)SelectObject( hdc, hPenFace );
    MoveToEx (hdc, pt.x - 12, pt.y, NULL);
    LineTo (hdc, pt.x, pt.y);
    LineTo (hdc, pt.x, pt.y - 13);

    pt.x--;
    pt.y--;

    hPenShadow = CreatePen( PS_SOLID, 1, CTheme::Instance().GetThemeColor(GetSysColor(COLOR_3DSHADOW)));
    SelectObject( hdc, hPenShadow );
    for (i = 1; i < 11; i += 4)
    {
        MoveToEx (hdc, pt.x - i, pt.y, NULL);
        LineTo (hdc, pt.x + 1, pt.y - i - 1);

        MoveToEx (hdc, pt.x - i - 1, pt.y, NULL);
        LineTo (hdc, pt.x + 1, pt.y - i - 2);
    }

    hPenHighlight = CreatePen( PS_SOLID, 1, CTheme::Instance().GetThemeColor(GetSysColor(COLOR_3DHIGHLIGHT)));
    SelectObject( hdc, hPenHighlight );
    for (i = 3; i < 13; i += 4)
    {
        MoveToEx (hdc, pt.x - i, pt.y, NULL);
        LineTo (hdc, pt.x + 1, pt.y - i - 1);
    }

    SelectObject (hdc, hOldPen);
    DeleteObject( hPenFace );
    DeleteObject( hPenShadow );
    DeleteObject( hPenHighlight );
}

void DrawPart (HWND hWnd, HDC hdc, int itemID)
{
    RECT rcPart;
    SendMessage(hWnd, SB_GETRECT, itemID, (LPARAM)&rcPart);

    UINT border = BDR_SUNKENOUTER;
    int x = 0;

    DrawEdge(hdc, &rcPart, border, BF_RECT|BF_ADJUST);

    HICON hIcon = (HICON)SendMessage(hWnd, SB_GETICON, itemID, 0);
    if (hIcon)
    {
        INT cy = rcPart.bottom - rcPart.top;
        DrawIconEx (hdc, rcPart.left + 2, rcPart.top, hIcon, cy, cy, 0, 0, DI_NORMAL);
        x = 2 + cy;
    }

    rcPart.left += x;
    int textlen = (int)SendMessage(hWnd, SB_GETTEXTLENGTH, itemID, 0);
    std::unique_ptr<wchar_t[]> textbuf(new wchar_t[textlen + 1]);
    SendMessage(hWnd, SB_GETTEXT, itemID, (LPARAM)textbuf.get());
    SetTextColor(hdc, CTheme::Instance().GetThemeColor(GetSysColor(COLOR_WINDOWTEXT)));
    SetBkColor(hdc, CTheme::Instance().GetThemeColor(GetSysColor(COLOR_3DFACE)));
    InflateRect(&rcPart, -2, 0);
    DrawText(hdc, textbuf.get(), -1, &rcPart, DT_LEFT|DT_SINGLELINE|DT_VCENTER);
}

void RefreshPart (HWND hWnd, HDC hdc, int itemID)
{
    HBRUSH hbrBk;

    RECT rcPart;
    SendMessage(hWnd, SB_GETRECT, itemID, (LPARAM)&rcPart);
    if (rcPart.right < rcPart.left) return;

    if (!RectVisible(hdc, &rcPart))
        return;

    hbrBk = CreateSolidBrush(CTheme::Instance().GetThemeColor(GetSysColor(COLOR_3DFACE)));
    FillRect(hdc, &rcPart, hbrBk);
    DeleteObject (hbrBk);

    DrawPart (hWnd, hdc, itemID);
}

LRESULT Refresh (HWND hWnd, HDC hdc)
{
    RECT   rect;
    HBRUSH hbrBk;
    HFONT  hOldFont;

    if (!IsWindowVisible(hWnd))
        return 0;

    GetClientRect (hWnd, &rect);

    hbrBk = CreateSolidBrush(CTheme::Instance().GetThemeColor(GetSysColor(COLOR_3DFACE)));
    FillRect(hdc, &rect, hbrBk);
    DeleteObject (hbrBk);

    HFONT hFont = (HFONT)SendMessage(hWnd, WM_GETFONT, 0, 0);

    hOldFont = (HFONT)SelectObject (hdc, hFont);
    int numparts = (int)SendMessage(hWnd, SB_GETPARTS, 0, 0);
    for (int i = 0; i < numparts; i++)
    {
        RefreshPart (hWnd, hdc, i);
    }

    SelectObject (hdc, hOldFont);

    if (GetWindowLongW (hWnd, GWL_STYLE) & SBARS_SIZEGRIP)
        DrawSizeGrip (hdc, &rect);

    return 0;
}

bool CStatusBar::Init(HINSTANCE /*hInst*/, HWND hParent, int nbParts, int * nsParts)
{
    InitCommonControls();

    CreateEx(0, WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP, hParent, 0, STATUSCLASSNAME);

    if (!*this)
    {
        return false;
    }

    SendMessage(*this, SB_SETPARTS, (WPARAM) nbParts, (LPARAM)nsParts);

    RECT rcClient;
    GetClientRect(*this, &rcClient);
    m_height = rcClient.bottom-rcClient.top;
    return true;
}

LRESULT CALLBACK CStatusBar::WinMsgHandler( HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam )
{
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
