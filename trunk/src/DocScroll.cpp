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
#include "ScintillaWnd.h"

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
    : m_pScintilla(nullptr)
    , m_lines(0)
    , m_bDirty(false)
{

}

void CDocScroll::InitScintilla( CScintillaWnd * pScintilla )
{
    m_pScintilla = pScintilla;
    InitializeCoolSB(*m_pScintilla);
    CoolSB_SetStyle(*m_pScintilla, SB_HORZ, CSBS_HOTTRACKED);
    CoolSB_SetStyle(*m_pScintilla, SB_VERT, CSBS_HOTTRACKED|CSBS_MAPMODE);
    CoolSB_SetThumbAlways(*m_pScintilla, SB_VERT, TRUE);
    CoolSB_SetMinThumbSize(*m_pScintilla, SB_BOTH, 10);
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
                if (m_bDirty)
                    CalcLines();

                for (auto line : m_visibleLineColors)
                {
                    LONG linepos = LONG(pCustDraw->rect.top + (pCustDraw->rect.bottom-pCustDraw->rect.top)*line.first/m_lines);
                    FillSolidRect(pCustDraw->hdc, pCustDraw->rect.left, linepos, pCustDraw->rect.right, linepos+2, line.second);
                }
                LONG linepos = LONG(pCustDraw->rect.top + (pCustDraw->rect.bottom-pCustDraw->rect.top)*m_curPosVisLine/m_lines);
                FillSolidRect(pCustDraw->hdc, pCustDraw->rect.left, linepos, pCustDraw->rect.right, linepos+2, m_curPosColor);
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

void CDocScroll::CalcLines()
{
    if (m_bDirty)
    {
        m_visibleLineColors.clear();
        for (auto it : m_lineColors)
        {
            m_visibleLineColors[m_pScintilla->Call(SCI_VISIBLEFROMDOCLINE, std::get<1>(it.first))] = it.second;
        }
        m_visibleLines = m_pScintilla->Call(SCI_VISIBLEFROMDOCLINE, m_lines);
    }
    m_bDirty = false;
}

void CDocScroll::AddLineColor( int type, size_t line, COLORREF clr )
{
    auto t = std::make_tuple(type,line);
    auto foundIt = m_lineColors.find(t);
    if (foundIt == m_lineColors.end())
    {
        m_lineColors[t] = clr;
        m_bDirty = true;
    }
}

void CDocScroll::SetTotalLines( size_t lines )
{
    if (m_lines != lines)
        m_bDirty = true;
    m_lines = lines;
}

void CDocScroll::Clear( int type )
{
    if (type == 0)
        m_lineColors.clear();
    else
    {
        auto it = m_lineColors.begin();
        for (; it != m_lineColors.end(); )
        {
            if (std::get<0>(it->first) == type)
                it = m_lineColors.erase(it);
            else
                ++it;
        }
    }
    m_visibleLineColors.clear();
    m_bDirty = true;
}
