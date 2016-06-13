// This file is part of BowPad.
//
// Copyright (C) 2016 - Stefan Kueng
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
#include "TabBtn.h"
#include "Theme.h"

bool CTabBtn::SetText(const TCHAR *str)
{
    return SetWindowText(*this, str) != 0;
}

bool CTabBtn::Init(HINSTANCE /*hInst*/, HWND hParent, HMENU id)
{
    InitCommonControls();
    CreateEx(0, WS_CHILD | WS_VISIBLE, hParent, 0, WC_BUTTON, id);
    if (!*this)
        return false;

    m_hFont = (HFONT)::SendMessage(*this, WM_GETFONT, 0, 0);

    if (m_hFont == nullptr)
    {
        m_hFont = (HFONT)::GetStockObject(DEFAULT_GUI_FONT);
        ::SendMessage(*this, WM_SETFONT, reinterpret_cast<WPARAM>(m_hFont), 0);
    }

    return true;
}

void CTabBtn::SetFont(const TCHAR *fontName, int fontSize)
{
    if (m_hFont)
        ::DeleteObject(m_hFont);

    m_hFont = ::CreateFont((int)fontSize, 0,
                           0,
                           0,
                           FW_NORMAL,
                           0, 0, 0, 0,
                           0, 0, 0, 0,
                           fontName);
    if (m_hFont)
        ::SendMessage(*this, WM_SETFONT, reinterpret_cast<WPARAM>(m_hFont), 0);
}

LRESULT CALLBACK CTabBtn::WinMsgHandler(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
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

                auto state = Button_GetState(*this);

                auto clr1 = CTheme::Instance().GetThemeColor(::GetSysColor(COLOR_BTNSHADOW));
                auto clr2 = CTheme::Instance().GetThemeColor(::GetSysColor(COLOR_BTNFACE));

                ::SetBkColor(hdc, (state & BST_HOT) != 0 ? clr2 : clr1);

                // first draw the whole area
                RECT rect;
                ::GetClientRect(*this, &rect);
                ::ExtTextOut(hdc, 0, 0, ETO_OPAQUE, &rect, nullptr, 0, nullptr);
                // now draw a slightly smaller area in a different color, which
                // makes the outer area drawn before look like a border
                InflateRect(&rect, -1, -1);
                ::SetBkColor(hdc, (state & BST_HOT) != 0 ? clr1 : clr2);
                ::ExtTextOut(hdc, 0, 0, ETO_OPAQUE, &rect, nullptr, 0, nullptr);

                ::SetTextColor(hdc, CTheme::Instance().GetThemeColor(::GetSysColor(COLOR_BTNTEXT)));
                wchar_t buf[20]; // we don't handle big texts in these buttons because they're meant to be very small
                ::GetWindowText(hwnd, buf, _countof(buf));

                HGDIOBJ hOldFont = SelectObject(hdc, m_hFont);
                ::DrawText(hdc, buf, -1, &rect, DT_CENTER | DT_SINGLELINE | DT_VCENTER);
                SelectObject(hdc, hOldFont);
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

