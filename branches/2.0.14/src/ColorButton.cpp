// This file is part of BowPad.
//
// Copyright (C) 2013, 2015 - Stefan Kueng
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
#include "ColorButton.h"
#include <Commdlg.h>

CColorButton::CColorButton(void)
    : m_pfnOrigCtlProc(nullptr)
    , m_color(0)
    , m_hwnd(0)
    , m_ctlId(0)
{
}

CColorButton::~CColorButton(void)
{
}

BOOL CColorButton::ConvertToColorButton(HWND hwndCtl)
{
    // Subclass the existing control.
    m_pfnOrigCtlProc = (WNDPROC) GetWindowLongPtr(hwndCtl, GWLP_WNDPROC);
    SetWindowLongPtr(hwndCtl, GWLP_WNDPROC, (LONG_PTR)_ColorButtonProc);
    SetWindowLongPtr(hwndCtl, GWLP_USERDATA, (LPARAM)this);
    m_hwnd = hwndCtl;
    return TRUE;
}

BOOL CColorButton::ConvertToColorButton(HWND hwndParent, UINT uiCtlId)
{
    m_ctlId = uiCtlId;
    return ConvertToColorButton(GetDlgItem(hwndParent, uiCtlId));
}


LRESULT CALLBACK CColorButton::_ColorButtonProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    CColorButton * pColorButton = GetObjectFromWindow(hwnd);
    switch (message)
    {
    case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc;
            hdc = BeginPaint(hwnd, &ps);
            RECT rc;
            GetClientRect(hwnd, &rc);
            SetBkColor(hdc, pColorButton->m_color);
            ExtTextOut(hdc, rc.left, rc.top, ETO_CLIPPED | ETO_OPAQUE, &rc, L"", 0, NULL);
            EndPaint(hwnd, &ps);
            return 0L;
        }
        break;
    case WM_ERASEBKGND:
        return TRUE;
    case WM_KEYUP:
        {
            if( wParam != VK_SPACE )
            {
                break;
            }
        }
        // Fall through
    case WM_LBUTTONUP:
        {
            CHOOSECOLOR cc = {0};
            static COLORREF acrCustClr[16];
            cc.lStructSize = sizeof(CHOOSECOLOR);
            cc.hwndOwner = hwnd;
            cc.rgbResult = pColorButton->m_color;
            cc.lpCustColors = (LPDWORD) acrCustClr;
            cc.Flags = CC_ANYCOLOR | CC_RGBINIT;

            if (ChooseColor(&cc)==TRUE)
            {
                pColorButton->m_color = cc.rgbResult;
                InvalidateRect(hwnd, NULL, FALSE);
                SendMessage(GetParent(hwnd), WM_COMMAND, pColorButton->m_ctlId, (LPARAM)hwnd);
            }
            return 0;
        }
        break;
    case WM_DESTROY:
        SetWindowLongPtr(hwnd, GWLP_WNDPROC, (LONG_PTR) pColorButton->m_pfnOrigCtlProc);
        break;
    }

    return CallWindowProc(pColorButton->m_pfnOrigCtlProc, hwnd, message, wParam, lParam);
}

void CColorButton::SetColor( COLORREF clr )
{
    m_color = clr;
    InvalidateRect(m_hwnd, NULL, FALSE);
}

