// This file is part of BowPad.
//
// Copyright (C) 2016-2018, 2020-2021 - Stefan Kueng
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
#include "GDIHelpers.h"
#include "DPIAware.h"

bool CTabBtn::SetText(const wchar_t* str)
{
    return SetWindowText(*this, str) != 0;
}

bool CTabBtn::Init(HINSTANCE /*hInst*/, HWND hParent, HMENU id)
{
    InitCommonControls();
    CreateEx(0, WS_CHILD | WS_VISIBLE, hParent, nullptr, WC_BUTTON, id);
    if (!*this)
        return false;

    m_hFont = reinterpret_cast<HFONT>(::SendMessage(*this, WM_GETFONT, 0, 0));

    if (m_hFont == nullptr)
    {
        NONCLIENTMETRICS ncm{};
        ncm.cbSize = sizeof(NONCLIENTMETRICS);
        SystemParametersInfo(SPI_GETNONCLIENTMETRICS, sizeof(NONCLIENTMETRICS), &ncm, 0U);
        m_hFont = CreateFontIndirect(&ncm.lfMessageFont);
        ::SendMessage(*this, WM_SETFONT, reinterpret_cast<WPARAM>(m_hFont), 0);
    }

    return true;
}

void CTabBtn::SetFont(const wchar_t* fontName, int fontSize)
{
    if (m_hFont)
        ::DeleteObject(m_hFont);

    m_hFont = ::CreateFont(fontSize, 0,
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
        switch (uMsg)
        {
            case WM_PAINT:
            {
                // only do custom drawing when in dark theme
                PAINTSTRUCT ps;
                HDC         hdc = BeginPaint(hwnd, &ps);

                auto state = Button_GetState(*this);

                auto clr1 = CTheme::Instance().GetThemeColor(::GetSysColor(COLOR_BTNSHADOW));
                auto clr2 = CTheme::Instance().GetThemeColor(::GetSysColor(COLOR_BTNFACE));
                clr1      = GDIHelpers::Darker(clr1, 0.5f);
                if (m_colorset && !CTheme::Instance().IsHighContrastMode())
                {
                    clr1 = CTheme::Instance().GetThemeColor(m_color, true);
                    clr2 = GDIHelpers::Darker(clr1, 0.7f);
                }
                auto r1 = GetRValue(clr1);
                auto g1 = GetGValue(clr1);
                auto b1 = GetBValue(clr1);
                auto r2 = GetRValue(clr2);
                auto g2 = GetGValue(clr2);
                auto b2 = GetBValue(clr2);
                // m_AnimVarHot changes from 0.0 (not hot) to 1.0 (hot)
                auto fraction = Animator::GetValue(m_animVarHot);
                clr1          = RGB((r1 - r2) * fraction + r1, (g1 - g2) * fraction + g1, (b1 - b2) * fraction + b1);
                clr2          = RGB((r1 - r2) * fraction + r2, (g1 - g2) * fraction + g2, (b1 - b2) * fraction + b2);

                ::SetBkColor(hdc, (state & BST_HOT) != 0 ? clr2 : clr1);

                // first draw the whole area
                RECT rect;
                ::GetClientRect(*this, &rect);
                ::ExtTextOut(hdc, 0, 0, ETO_OPAQUE, &rect, nullptr, 0, nullptr);
                // now draw a slightly smaller area in a different color, which
                // makes the outer area drawn before look like a border
                const auto onedpi = CDPIAware::Instance().Scale(*this, 1);
                InflateRect(&rect, -onedpi, -onedpi);
                auto halfpos = ((rect.bottom - rect.top) / 2) + rect.top;
                GDIHelpers::FillSolidRect(hdc, rect.left, rect.top, rect.right, halfpos, (state & BST_HOT) != 0 ? clr1 : clr2);
                GDIHelpers::FillSolidRect(hdc, rect.left, halfpos, rect.right, rect.bottom, GDIHelpers::Darker((state & BST_HOT) != 0 ? clr1 : clr2, 0.9f));

                ::SetBkMode(hdc, TRANSPARENT);
                if (m_textcolorset && !CTheme::Instance().IsHighContrastMode())
                    ::SetTextColor(hdc, CTheme::Instance().GetThemeColor(m_textcolor));
                else
                    ::SetTextColor(hdc, CTheme::Instance().GetThemeColor(::GetSysColor(COLOR_BTNTEXT)));
                wchar_t buf[20]; // we don't handle big texts in these buttons because they're meant to be very small
                ::GetWindowText(hwnd, buf, _countof(buf));

                HGDIOBJ hOldFont = SelectObject(hdc, m_hFont);
                ::DrawText(hdc, buf, -1, &rect, DT_CENTER | DT_SINGLELINE | DT_VCENTER);
                SelectObject(hdc, hOldFont);
                EndPaint(hwnd, &ps);
                return 0;
            }
            case WM_ERASEBKGND:
                return TRUE;
            case WM_MOUSEMOVE:
            {
                if ((Button_GetState(*this) & BST_HOT) == 0)
                {
                    auto transHot   = Animator::Instance().CreateLinearTransition(m_animVarHot, 0.3, 1.0);
                    auto storyBoard = Animator::Instance().CreateStoryBoard();
                    if (storyBoard && transHot)
                    {
                        storyBoard->AddTransition(m_animVarHot.m_animVar, transHot);
                        Animator::Instance().RunStoryBoard(storyBoard, [this]() {
                            InvalidateRect(*this, nullptr, false);
                        });
                    }
                    else
                        InvalidateRect(*this, nullptr, false);
                    TRACKMOUSEEVENT tme = {sizeof(tme)};
                    tme.dwFlags         = TME_LEAVE;
                    tme.hwndTrack       = hwnd;
                    TrackMouseEvent(&tme);
                }
            }
            break;
            case WM_MOUSELEAVE:
            {
                TRACKMOUSEEVENT tme = {0};
                tme.cbSize          = sizeof(TRACKMOUSEEVENT);
                tme.dwFlags         = TME_LEAVE | TME_CANCEL;
                tme.hwndTrack       = *this;
                TrackMouseEvent(&tme);
                if ((Button_GetState(*this) & BST_HOT) != 0)
                {
                    auto transHot   = Animator::Instance().CreateLinearTransition(m_animVarHot, 0.5, 0.0);
                    auto storyBoard = Animator::Instance().CreateStoryBoard();
                    if (storyBoard && transHot)
                    {
                        storyBoard->AddTransition(m_animVarHot.m_animVar, transHot);
                        Animator::Instance().RunStoryBoard(storyBoard, [this]() {
                            InvalidateRect(*this, nullptr, false);
                        });
                    }
                    else
                        InvalidateRect(*this, nullptr, false);
                }
            }
        }
    }
    if (prevWndProc)
        return prevWndProc(hwnd, uMsg, wParam, lParam);
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}
