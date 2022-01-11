// This file is part of BowPad.
//
// Copyright (C) 2016-2018, 2020-2022 - Stefan Kueng
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
#include "DPIAware.h"

#define COLORBOX_SIZE      CDPIAware::Instance().Scale(*this, 20)
#define COLORBOX_TEXTWIDTH CDPIAware::Instance().Scale(*this, 60)
#define BORDER             CDPIAware::Instance().Scale(*this, 5)
#define RECTBORDER         CDPIAware::Instance().Scale(*this, 2)

void CCustomToolTip::Init(HWND hParent)
{
#define POPUPCLASSNAME "BASEPOPUPWNDCLASS"
    // Register the window class if it has not already been registered.
    WNDCLASSEX wndCls = {sizeof(WNDCLASSEX)};
    if (!(::GetClassInfoEx(hResource, TEXT(POPUPCLASSNAME), &wndCls)))
    {
        // otherwise we need to register a new class
        wndCls.style       = CS_SAVEBITS;
        wndCls.lpfnWndProc = ::DefWindowProc;
        wndCls.cbClsExtra = wndCls.cbWndExtra = 0;
        wndCls.hInstance                      = hResource;
        wndCls.hIcon                          = nullptr;
        wndCls.hCursor                        = LoadCursor(hResource, IDC_ARROW);
        wndCls.hbrBackground                  = nullptr;
        wndCls.lpszMenuName                   = nullptr;
        wndCls.lpszClassName                  = TEXT(POPUPCLASSNAME);

        if (RegisterClassEx(&wndCls) == 0)
        {
            if (GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
                throw std::exception("failed to register " POPUPCLASSNAME);
        }
    }

    DWORD dwStyle   = WS_POPUP | WS_BORDER;
    DWORD dwExStyle = 0;

    m_hParent       = hParent;

    if (!CreateEx(dwExStyle, dwStyle, hParent, nullptr, TEXT(POPUPCLASSNAME)))
        return;
}

void CCustomToolTip::ShowTip(POINT screenPt, const std::wstring& text, COLORREF* color)
{
    m_infoText = text;
    if (color)
        m_color = (*color) & 0xFFFFFF;
    m_bShowColorBox = color != nullptr;
    auto dc         = GetDC(*this);
    auto textBuf    = std::make_unique<wchar_t[]>(m_infoText.size() + 4);
    wcscpy_s(textBuf.get(), m_infoText.size() + 4, m_infoText.c_str());
    RECT rc{};
    rc.left   = 0;
    rc.right  = CDPIAware::Instance().Scale(*this, 800);
    rc.top    = 0;
    rc.bottom = CDPIAware::Instance().Scale(*this, 800);

    NONCLIENTMETRICS ncm{};
    ncm.cbSize = sizeof(NONCLIENTMETRICS);
    SystemParametersInfo(SPI_GETNONCLIENTMETRICS, sizeof(NONCLIENTMETRICS), &ncm, 0U);
    m_hFont      = CreateFontIndirect(&ncm.lfStatusFont);

    auto oldFont = SelectObject(dc, m_hFont);
    DrawText(dc, textBuf.get(), static_cast<int>(m_infoText.size()), &rc, DT_LEFT | DT_TOP | DT_CALCRECT | DT_NOPREFIX | DT_EXPANDTABS | DT_NOCLIP);
    SelectObject(dc, oldFont);

    if (m_bShowColorBox)
    {
        rc.bottom += COLORBOX_SIZE + BORDER; // space for the color box
        rc.right = max(rc.right, COLORBOX_SIZE);
    }
    SetTransparency(0);
    SetWindowPos(*this, nullptr,
                 screenPt.x - rc.right / 2, screenPt.y - rc.bottom - CDPIAware::Instance().Scale(*this, 20),
                 rc.right + BORDER + BORDER, rc.bottom + BORDER + BORDER,
                 SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_NOZORDER | SWP_SHOWWINDOW);

    auto transAlpha = Animator::Instance().CreateSmoothStopTransition(m_animVarAlpha, 0.3, 255);
    auto storyBoard = Animator::Instance().CreateStoryBoard();
    if (storyBoard && transAlpha)
    {
        storyBoard->AddTransition(m_animVarAlpha.m_animVar, transAlpha);
        Animator::Instance().RunStoryBoard(storyBoard, [this]() {
            SetTransparency(static_cast<BYTE>(Animator::GetIntegerValue(m_animVarAlpha)));
        });
    }
    else
        SetTransparency(static_cast<BYTE>(Animator::GetIntegerValue(m_animVarAlpha)));
}

void CCustomToolTip::HideTip()
{
    auto transAlpha = Animator::Instance().CreateSmoothStopTransition(m_animVarAlpha, 0.5, 0);
    auto storyBoard = Animator::Instance().CreateStoryBoard();
    if (storyBoard && transAlpha)
    {
        storyBoard->AddTransition(m_animVarAlpha.m_animVar, transAlpha);
        Animator::Instance().RunStoryBoard(storyBoard, [this]() {
            auto alpha = Animator::GetIntegerValue(m_animVarAlpha);
            SetTransparency(static_cast<BYTE>(alpha));
            if (alpha == 0)
                ShowWindow(*this, SW_HIDE);
        });
    }
    else
        SetTransparency(static_cast<BYTE>(Animator::GetIntegerValue(m_animVarAlpha)));
}

void CCustomToolTip::OnPaint(HDC hdc, RECT* pRc)
{
    GDIHelpers::FillSolidRect(hdc, pRc, CTheme::Instance().GetThemeColor(GetSysColor(COLOR_INFOBK)));
    pRc->left += BORDER;
    pRc->top += BORDER;
    pRc->right -= BORDER;
    pRc->bottom -= BORDER;
    ::SetTextColor(hdc, CTheme::Instance().GetThemeColor(GetSysColor(COLOR_INFOTEXT)));
    SetBkMode(hdc, TRANSPARENT);
    auto oldFont = SelectObject(hdc, m_hFont);
    auto textBuf = std::make_unique<wchar_t[]>(m_infoText.size() + 4);
    wcscpy_s(textBuf.get(), m_infoText.size() + 4, m_infoText.c_str());
    DrawText(hdc, textBuf.get(), -1, pRc, DT_LEFT | DT_TOP | DT_NOPREFIX | DT_EXPANDTABS | DT_NOCLIP);
    if (m_bShowColorBox)
    {
        RECT clrTextRc = *pRc;
        clrTextRc.top  = pRc->bottom - COLORBOX_SIZE;
        DrawText(hdc, L"color:", -1, &clrTextRc, DT_LEFT | DT_TOP | DT_NOPREFIX | DT_EXPANDTABS | DT_NOCLIP);
        SelectObject(hdc, GetStockObject(GRAY_BRUSH));
        Rectangle(hdc, pRc->left + COLORBOX_TEXTWIDTH, pRc->bottom - COLORBOX_SIZE, pRc->right, pRc->bottom);
        GDIHelpers::FillSolidRect(hdc, pRc->left + RECTBORDER + COLORBOX_TEXTWIDTH, pRc->bottom - COLORBOX_SIZE + RECTBORDER, pRc->right - RECTBORDER, pRc->bottom - RECTBORDER, m_color);
    }
    SelectObject(hdc, oldFont);
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
            POINT pt{};
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
            HDC         hdc = BeginPaint(hwnd, &ps);
            RECT        rc;
            GetClientRect(hwnd, &rc);
            OnPaint(hdc, &rc);
            EndPaint(hwnd, &ps);
            return 0L;
        }
        default:
            break;
    }

    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}
