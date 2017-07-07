// This file is part of BowPad.
//
// Copyright (C) 2016-2017 - Stefan Kueng
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
#include "StdAfx.H"
#include "CustomTooltip.h"
#include "GDIHelpers.h"
#include "Theme.h"

#define COLORBOX_SIZE int(20*m_dpiScaleY)
#define BORDER int(5*m_dpiScaleX)
#define RECTBORDER int(2*m_dpiScaleX)

void CCustomToolTip::Init(HWND hParent)
{
#define POPUPCLASSNAME "BASEPOPUPWNDCLASS"
    // Register the window class if it has not already been registered.
    WNDCLASSEX wndcls = { sizeof(WNDCLASSEX) };
    if (!(::GetClassInfoEx(hResource, TEXT(POPUPCLASSNAME), &wndcls)))
    {
        // otherwise we need to register a new class
        wndcls.style = CS_SAVEBITS;
        wndcls.lpfnWndProc = ::DefWindowProc;
        wndcls.cbClsExtra = wndcls.cbWndExtra = 0;
        wndcls.hInstance = hResource;
        wndcls.hIcon = nullptr;
        wndcls.hCursor = LoadCursor(hResource, IDC_ARROW);
        wndcls.hbrBackground = nullptr;
        wndcls.lpszMenuName = nullptr;
        wndcls.lpszClassName = TEXT(POPUPCLASSNAME);

        if (RegisterClassEx(&wndcls) == 0)
        {
            if (GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
                throw std::exception("failed to register " POPUPCLASSNAME);
        }
    }

    DWORD dwStyle = WS_POPUP | WS_BORDER;
    DWORD dwExStyle = 0;

    m_hParent = hParent;

    if (!CreateEx(dwExStyle, dwStyle, hParent, 0, TEXT(POPUPCLASSNAME)))
        return;
}

void CCustomToolTip::ShowTip(POINT screenPt, const std::wstring & text, COLORREF * color)
{
    m_infoText = text;
    if (color)
        m_color = (*color)&0xFFFFFF;
    m_bShowColorBox = color != nullptr;
    auto dc = GetDC(*this);
    auto textbuf = std::make_unique<wchar_t[]>(m_infoText.size() + 4);
    wcscpy_s(textbuf.get(), m_infoText.size() + 4, m_infoText.c_str());
    RECT rc;
    rc.left = 0; rc.right = 800; rc.top = 0; rc.bottom = 800;

    NONCLIENTMETRICS ncm;
    ncm.cbSize = sizeof(NONCLIENTMETRICS);
    SystemParametersInfo(SPI_GETNONCLIENTMETRICS, sizeof(NONCLIENTMETRICS), &ncm, 0U);
    m_hFont = CreateFontIndirect(&ncm.lfStatusFont);

    auto oldfont = SelectObject(dc, m_hFont);
    DrawText(dc, textbuf.get(), (int)m_infoText.size(), &rc, DT_LEFT | DT_TOP | DT_CALCRECT | DT_NOPREFIX | DT_EXPANDTABS | DT_NOCLIP);
    SelectObject(dc, oldfont);

    if (m_bShowColorBox)
    {
        rc.bottom += COLORBOX_SIZE + BORDER;   // space for the color box
        rc.right = max(rc.right, COLORBOX_SIZE);
    }
    SetTransparency(0);
    SetWindowPos(*this, nullptr,
                 screenPt.x - rc.right / 2, screenPt.y - rc.bottom - 20,
                 rc.right + BORDER + BORDER, rc.bottom + BORDER + BORDER,
                 SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_NOZORDER | SWP_SHOWWINDOW);

    auto transAlpha = Animator::Instance().CreateLinearTransition(0.3, 255);
    auto storyBoard = Animator::Instance().CreateStoryBoard();
    storyBoard->AddTransition(m_AnimVarAlpha, transAlpha);
    Animator::Instance().RunStoryBoard(storyBoard, [this]()
    {
        SetTransparency((BYTE)Animator::GetIntegerValue(m_AnimVarAlpha));
    });
}

void CCustomToolTip::HideTip()
{
    auto transAlpha = Animator::Instance().CreateLinearTransition(0.5, 0);
    auto storyBoard = Animator::Instance().CreateStoryBoard();
    storyBoard->AddTransition(m_AnimVarAlpha, transAlpha);
    Animator::Instance().RunStoryBoard(storyBoard, [this]()
    {
        auto alpha = Animator::GetIntegerValue(m_AnimVarAlpha);
        SetTransparency((BYTE)alpha);
        if (alpha == 0)
            ShowWindow(*this, SW_HIDE);
    });
}

void CCustomToolTip::OnPaint(HDC hdc, RECT * pRc)
{
    GDIHelpers::FillSolidRect(hdc, pRc, CTheme::Instance().GetThemeColor(GetSysColor(COLOR_INFOBK)));
    pRc->left += BORDER;
    pRc->top += BORDER;
    pRc->right -= BORDER;
    pRc->bottom -= BORDER;
    ::SetTextColor(hdc, CTheme::Instance().GetThemeColor(GetSysColor(COLOR_INFOTEXT)));
    SetBkMode(hdc, TRANSPARENT);
    auto oldfont = SelectObject(hdc, m_hFont);
    auto textbuf = std::make_unique<wchar_t[]>(m_infoText.size() + 4);
    wcscpy_s(textbuf.get(), m_infoText.size() + 4, m_infoText.c_str());
    DrawText(hdc, textbuf.get(), -1, pRc, DT_LEFT | DT_TOP | DT_NOPREFIX | DT_EXPANDTABS | DT_NOCLIP);
    if (m_bShowColorBox)
    {
        SelectObject(hdc, GetStockObject(GRAY_BRUSH));
        Rectangle(hdc, pRc->left, pRc->bottom - COLORBOX_SIZE, pRc->right, pRc->bottom);
        GDIHelpers::FillSolidRect(hdc, pRc->left + RECTBORDER, pRc->bottom - COLORBOX_SIZE + RECTBORDER, pRc->right - RECTBORDER, pRc->bottom - RECTBORDER, m_color);
    }
    SelectObject(hdc, oldfont);
}

LRESULT CCustomToolTip::WinMsgHandler(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
        case WM_KEYDOWN:
        if (wParam == VK_ESCAPE)
        {
            HideTip();
            return TRUE;
        }
        break;
        case WM_ERASEBKGND:
        return TRUE;
        case WM_LBUTTONDOWN:
        {
            // send the click to the parent window
            POINT pt;
            pt.x = GET_X_LPARAM(lParam);
            pt.y = GET_Y_LPARAM(lParam);
            MapWindowPoints(m_hwnd, m_hParent, &pt, 1);
            SendMessage(m_hParent, WM_LBUTTONDOWN, 0, MAKELPARAM(pt.x, pt.y));
            return 0;
        }
        case WM_RBUTTONDOWN:
            HideTip();
            return 0;
        case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc;
            hdc = BeginPaint(hwnd, &ps);
            RECT rc;
            GetClientRect(hwnd, &rc);
            OnPaint(hdc, &rc);
            EndPaint(hwnd, &ps);
            return 0L;
        }
    }

    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}
