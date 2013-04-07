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
#include "DocScroll.h"

#include <Commctrl.h>


static void FillSolidRect(HDC hDC, int left, int top, int right, int bottom, COLORREF clr)
{
    ::SetBkColor(hDC, clr);
    RECT rect;
    rect.left = left;
    rect.top = top;
    rect.right = right;
    rect.bottom = bottom;
    ::ExtTextOut(hDC, 0, 0, ETO_OPAQUE, &rect, NULL, 0, NULL);
}


CDocScroll::~CDocScroll()
{
}

CDocScroll::CDocScroll()
    : m_line(0)
    , m_lines(0)
{

}

void CDocScroll::InitScintilla( HWND hWnd )
{
    InitializeCoolSB(hWnd);
    CoolSB_SetStyle(hWnd, SB_HORZ, CSBS_HOTTRACKED);
    CoolSB_SetStyle(hWnd, SB_VERT, CSBS_HOTTRACKED|CSBS_MAPMODE);
    CoolSB_SetThumbAlways(hWnd, SB_VERT, TRUE);
}

LRESULT CALLBACK CDocScroll::HandleCustomDraw( WPARAM /*wParam*/, NMCSBCUSTOMDRAW * pCustDraw )
{
    const int margin = 2;
    // inserted buttons do not use PREPAINT etc..
    if (pCustDraw->nBar == SB_INSBUT)
    {
        return CDRF_DODEFAULT;
    }

    if (pCustDraw->dwDrawStage == CDDS_PREPAINT)
    {
        return CDRF_SKIPDEFAULT;
    }

    if (pCustDraw->dwDrawStage == CDDS_POSTPAINT)
    {

    }

    if (pCustDraw->nBar == SB_BOTH)
    {
        // the sizing gripper in the bottom-right corner
        FillSolidRect(pCustDraw->hdc, pCustDraw->rect.left, pCustDraw->rect.top, pCustDraw->rect.right, pCustDraw->rect.bottom, ::GetSysColor(COLOR_SCROLLBAR));
    }
    else if (pCustDraw->nBar == SB_HORZ)
    {
        COLORREF scroll = ::GetSysColor(COLOR_3DFACE);
        COLORREF thumb = ::GetSysColor(COLOR_SCROLLBAR);
        if (pCustDraw->uState == CDIS_HOT)
            thumb = RGB(200,200,255);

        switch (pCustDraw->uItem)
        {
        case HTSCROLL_LEFT:
            {
                FillSolidRect(pCustDraw->hdc, pCustDraw->rect.left, pCustDraw->rect.top, pCustDraw->rect.right, pCustDraw->rect.bottom, scroll);
                HGDIOBJ brush = CreateSolidBrush(thumb);
                HGDIOBJ oldbrush = SelectObject(pCustDraw->hdc, brush);
                HGDIOBJ oldpen = SelectObject(pCustDraw->hdc, GetStockObject(NULL_PEN));
                POINT triangle[3];
                triangle[0].x = pCustDraw->rect.left + margin;
                triangle[0].y = pCustDraw->rect.top + (pCustDraw->rect.bottom-pCustDraw->rect.top)/2;
                triangle[1].x = pCustDraw->rect.right - margin;
                triangle[1].y = pCustDraw->rect.top + margin;
                triangle[2].x = triangle[1].x;
                triangle[2].y = pCustDraw->rect.bottom - margin;
                Polygon(pCustDraw->hdc, triangle, 3);
                //RoundRect(pCustDraw->hdc, pCustDraw->rect.left+2, pCustDraw->rect.top+2, pCustDraw->rect.right-2, pCustDraw->rect.bottom-2, 5, 5);
                SelectObject(pCustDraw->hdc, oldpen);
                SelectObject(pCustDraw->hdc, oldbrush);
                DeleteObject(brush);
            }
            break;
        case HTSCROLL_RIGHT:
            {
                FillSolidRect(pCustDraw->hdc, pCustDraw->rect.left, pCustDraw->rect.top, pCustDraw->rect.right, pCustDraw->rect.bottom, scroll);
                HGDIOBJ brush = CreateSolidBrush(thumb);
                HGDIOBJ oldbrush = SelectObject(pCustDraw->hdc, brush);
                HGDIOBJ oldpen = SelectObject(pCustDraw->hdc, GetStockObject(NULL_PEN));
                POINT triangle[3];
                triangle[0].x = pCustDraw->rect.right - margin;
                triangle[0].y = pCustDraw->rect.top + (pCustDraw->rect.bottom-pCustDraw->rect.top)/2;
                triangle[1].x = pCustDraw->rect.left + margin;
                triangle[1].y = pCustDraw->rect.top + margin;
                triangle[2].x = triangle[1].x;
                triangle[2].y = pCustDraw->rect.bottom - margin;
                Polygon(pCustDraw->hdc, triangle, 3);
                //RoundRect(pCustDraw->hdc, pCustDraw->rect.left+2, pCustDraw->rect.top+2, pCustDraw->rect.right-2, pCustDraw->rect.bottom-2, 5, 5);
                SelectObject(pCustDraw->hdc, oldpen);
                SelectObject(pCustDraw->hdc, oldbrush);
                DeleteObject(brush);
            }
            break;
        case HTSCROLL_THUMB:
            FillSolidRect(pCustDraw->hdc, pCustDraw->rect.left, pCustDraw->rect.top, pCustDraw->rect.right, pCustDraw->rect.bottom, thumb);
            break;
        default:
            FillSolidRect(pCustDraw->hdc, pCustDraw->rect.left, pCustDraw->rect.top, pCustDraw->rect.right, pCustDraw->rect.bottom, scroll);
            break;
        }
        return CDRF_SKIPDEFAULT;
    }
    else if (pCustDraw->nBar == SB_VERT)
    {
        COLORREF scroll = ::GetSysColor(COLOR_3DFACE);
        COLORREF thumb = ::GetSysColor(COLOR_SCROLLBAR);
        if (pCustDraw->uState == CDIS_HOT)
            thumb = RGB(200,200,255);

        switch (pCustDraw->uItem)
        {
        case HTSCROLL_DOWN:
            {
                FillSolidRect(pCustDraw->hdc, pCustDraw->rect.left, pCustDraw->rect.top, pCustDraw->rect.right, pCustDraw->rect.bottom, scroll);
                HGDIOBJ brush = CreateSolidBrush(thumb);
                HGDIOBJ oldbrush = SelectObject(pCustDraw->hdc, brush);
                HGDIOBJ oldpen = SelectObject(pCustDraw->hdc, GetStockObject(NULL_PEN));
                POINT triangle[3];
                triangle[0].x = pCustDraw->rect.left + (pCustDraw->rect.right-pCustDraw->rect.left)/2;
                triangle[0].y = pCustDraw->rect.bottom - margin;
                triangle[1].x = pCustDraw->rect.left + margin;
                triangle[1].y = pCustDraw->rect.top + margin;
                triangle[2].x = pCustDraw->rect.right - margin;
                triangle[2].y = pCustDraw->rect.top + margin;
                Polygon(pCustDraw->hdc, triangle, 3);
                //RoundRect(pCustDraw->hdc, pCustDraw->rect.left+2, pCustDraw->rect.top+2, pCustDraw->rect.right-2, pCustDraw->rect.bottom-2, 5, 5);
                SelectObject(pCustDraw->hdc, oldpen);
                SelectObject(pCustDraw->hdc, oldbrush);
                DeleteObject(brush);
            }
            break;
        case HTSCROLL_UP:
            {
                FillSolidRect(pCustDraw->hdc, pCustDraw->rect.left, pCustDraw->rect.top, pCustDraw->rect.right, pCustDraw->rect.bottom, scroll);
                HGDIOBJ brush = CreateSolidBrush(thumb);
                HGDIOBJ oldbrush = SelectObject(pCustDraw->hdc, brush);
                HGDIOBJ oldpen = SelectObject(pCustDraw->hdc, GetStockObject(NULL_PEN));
                POINT triangle[3];
                triangle[0].x = pCustDraw->rect.left + (pCustDraw->rect.right-pCustDraw->rect.left)/2;
                triangle[0].y = pCustDraw->rect.top + margin;
                triangle[1].x = pCustDraw->rect.left + margin;
                triangle[1].y = pCustDraw->rect.bottom - margin;
                triangle[2].x = pCustDraw->rect.right - margin;
                triangle[2].y = pCustDraw->rect.bottom - margin;
                Polygon(pCustDraw->hdc, triangle, 3);
                //RoundRect(pCustDraw->hdc, pCustDraw->rect.left+2, pCustDraw->rect.top+2, pCustDraw->rect.right-2, pCustDraw->rect.bottom-2, 5, 5);
                SelectObject(pCustDraw->hdc, oldpen);
                SelectObject(pCustDraw->hdc, oldbrush);
                DeleteObject(brush);
            }
            break;
        case HTSCROLL_THUMB:
            {
                HGDIOBJ pen = CreatePen(PS_SOLID, 2, thumb);
                HGDIOBJ oldbrush = SelectObject(pCustDraw->hdc, GetStockObject(NULL_BRUSH));
                HGDIOBJ oldpen = SelectObject(pCustDraw->hdc, pen);
                RoundRect(pCustDraw->hdc, pCustDraw->rect.left+2, pCustDraw->rect.top+2, pCustDraw->rect.right-2, pCustDraw->rect.bottom-2, 5, 5);
                SelectObject(pCustDraw->hdc, oldpen);
                SelectObject(pCustDraw->hdc, oldbrush);
                DeleteObject(pen);
            }
            break;
        case HTSCROLL_PAGEFULL:
            {
                FillSolidRect(pCustDraw->hdc, pCustDraw->rect.left, pCustDraw->rect.top, pCustDraw->rect.right, pCustDraw->rect.bottom, scroll);
                LONG linepos = LONG(pCustDraw->rect.top + (pCustDraw->rect.bottom-pCustDraw->rect.top)*m_line/m_lines);
                FillSolidRect(pCustDraw->hdc, pCustDraw->rect.left, linepos, pCustDraw->rect.right, linepos+2, RGB(0,200,0));
            }
            break;
        }
        return CDRF_SKIPDEFAULT;
    }
    // INSERTED BUTTONS are handled here...
    else if (pCustDraw->nBar == SB_INSBUT)
    {
        return CDRF_DODEFAULT;
    }

    return CDRF_SKIPDEFAULT;
}
