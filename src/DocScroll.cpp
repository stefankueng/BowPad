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
#include "DocScroll.h"
#include "ScintillaWnd.h"
#include "AppUtils.h"
#include "Theme.h"

#include <gdiplus.h>

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
    Gdiplus::GdiplusShutdown(m_gdiplusToken);
}

CDocScroll::CDocScroll()
    : m_pScintilla(nullptr)
    , m_lines(0)
    , m_bDirty(false)
    , m_visibleLines(0)
    , m_curPosVisLine(0)
    , m_curPosColor(0)
{
    Gdiplus::GdiplusStartupInput gdiplusStartupInput;
    Gdiplus::GdiplusStartup(&m_gdiplusToken, &gdiplusStartupInput, NULL);
}

void CDocScroll::InitScintilla( CScintillaWnd * pScintilla )
{
    m_pScintilla = pScintilla;
    InitializeCoolSB(*m_pScintilla);
    UINT style = CSBS_HOTTRACKED;
    CoolSB_SetStyle(*m_pScintilla, SB_HORZ, style);
    style |= CIniSettings::Instance().GetInt64(L"View", L"scrollstyle", 1) > 0 ? CSBS_MAPMODE : 0;
    CoolSB_SetStyle(*m_pScintilla, SB_VERT, style);
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
        FillSolidRect(pCustDraw->hdc, pCustDraw->rect.left, pCustDraw->rect.top, pCustDraw->rect.right, pCustDraw->rect.bottom, CTheme::Instance().GetThemeColor(::GetSysColor(COLOR_SCROLLBAR)));
    }
    else if (pCustDraw->nBar == SB_HORZ)
    {
        COLORREF scroll = CTheme::Instance().GetThemeColor(::GetSysColor(COLOR_3DFACE));
        COLORREF thumb = CTheme::Instance().GetThemeColor(::GetSysColor(COLOR_3DSHADOW));
        if (pCustDraw->uState == CDIS_HOT)
            thumb = CTheme::Instance().GetThemeColor(RGB(200,200,255));

        switch (pCustDraw->uItem)
        {
        case HTSCROLL_LEFT:
            {
                Gdiplus::Graphics graphics(pCustDraw->hdc);
                graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
                Gdiplus::Color c1;
                c1.SetFromCOLORREF(scroll);
                Gdiplus::SolidBrush brush(c1);
                graphics.FillRectangle(&brush, pCustDraw->rect.left, pCustDraw->rect.top, pCustDraw->rect.right-pCustDraw->rect.left, pCustDraw->rect.bottom-pCustDraw->rect.top);

                Gdiplus::Point trianglepts[3];
                trianglepts[0].X = pCustDraw->rect.left + margin;
                trianglepts[0].Y = pCustDraw->rect.top + (pCustDraw->rect.bottom-pCustDraw->rect.top)/2;
                trianglepts[1].X = pCustDraw->rect.right - margin;
                trianglepts[1].Y = pCustDraw->rect.top + margin;
                trianglepts[2].X = trianglepts[1].X;
                trianglepts[2].Y = pCustDraw->rect.bottom - margin;
                Gdiplus::Color c2;
                c2.SetFromCOLORREF(thumb);
                Gdiplus::SolidBrush tribrush(c2);
                graphics.FillPolygon(&tribrush, trianglepts, 3);
            }
            break;
        case HTSCROLL_RIGHT:
            {
                Gdiplus::Graphics graphics(pCustDraw->hdc);
                graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
                Gdiplus::Color c1;
                c1.SetFromCOLORREF(scroll);
                Gdiplus::SolidBrush brush(c1);
                graphics.FillRectangle(&brush, pCustDraw->rect.left, pCustDraw->rect.top, pCustDraw->rect.right-pCustDraw->rect.left, pCustDraw->rect.bottom-pCustDraw->rect.top);

                Gdiplus::Point trianglepts[3];
                trianglepts[0].X = pCustDraw->rect.right - margin;
                trianglepts[0].Y = pCustDraw->rect.top + (pCustDraw->rect.bottom-pCustDraw->rect.top)/2;
                trianglepts[1].X = pCustDraw->rect.left + margin;
                trianglepts[1].Y = pCustDraw->rect.top + margin;
                trianglepts[2].X = trianglepts[1].X;
                trianglepts[2].Y = pCustDraw->rect.bottom - margin;
                Gdiplus::Color c2;
                c2.SetFromCOLORREF(thumb);
                Gdiplus::SolidBrush tribrush(c2);
                graphics.FillPolygon(&tribrush, trianglepts, 3);
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
        COLORREF scroll = CTheme::Instance().GetThemeColor(::GetSysColor(COLOR_3DFACE));
        COLORREF thumb = CTheme::Instance().GetThemeColor(::GetSysColor(COLOR_3DSHADOW));
        if (pCustDraw->uState == CDIS_HOT)
            thumb = CTheme::Instance().GetThemeColor(RGB(200,200,255));

        switch (pCustDraw->uItem)
        {
        case HTSCROLL_DOWN:
            {
                Gdiplus::Graphics graphics(pCustDraw->hdc);
                graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
                Gdiplus::Color c1;
                c1.SetFromCOLORREF(scroll);
                Gdiplus::SolidBrush brush(c1);
                graphics.FillRectangle(&brush, pCustDraw->rect.left, pCustDraw->rect.top, pCustDraw->rect.right-pCustDraw->rect.left, pCustDraw->rect.bottom-pCustDraw->rect.top);

                Gdiplus::Point trianglepts[3];
                trianglepts[0].X = pCustDraw->rect.left + (pCustDraw->rect.right-pCustDraw->rect.left)/2;
                trianglepts[0].Y = pCustDraw->rect.bottom - margin;
                trianglepts[1].X = pCustDraw->rect.left + margin;
                trianglepts[1].Y = pCustDraw->rect.top + margin;
                trianglepts[2].X = pCustDraw->rect.right - margin;
                trianglepts[2].Y = pCustDraw->rect.top + margin;
                Gdiplus::Color c2;
                c2.SetFromCOLORREF(thumb);
                Gdiplus::SolidBrush tribrush(c2);
                graphics.FillPolygon(&tribrush, trianglepts, 3);
            }
            break;
        case HTSCROLL_UP:
            {
                Gdiplus::Graphics graphics(pCustDraw->hdc);
                graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
                Gdiplus::Color c1;
                c1.SetFromCOLORREF(scroll);
                Gdiplus::SolidBrush brush(c1);
                graphics.FillRectangle(&brush, pCustDraw->rect.left, pCustDraw->rect.top, pCustDraw->rect.right-pCustDraw->rect.left, pCustDraw->rect.bottom-pCustDraw->rect.top);

                Gdiplus::Point trianglepts[3];
                trianglepts[0].X = pCustDraw->rect.left + (pCustDraw->rect.right-pCustDraw->rect.left)/2;
                trianglepts[0].Y = pCustDraw->rect.top + margin;
                trianglepts[1].X = pCustDraw->rect.left + margin;
                trianglepts[1].Y = pCustDraw->rect.bottom - margin;
                trianglepts[2].X = pCustDraw->rect.right - margin;
                trianglepts[2].Y = pCustDraw->rect.bottom - margin;
                Gdiplus::Color c2;
                c2.SetFromCOLORREF(thumb);
                Gdiplus::SolidBrush tribrush(c2);
                graphics.FillPolygon(&tribrush, trianglepts, 3);
            }
            break;
        case HTSCROLL_THUMB:
            {
                Gdiplus::Graphics graphics(pCustDraw->hdc);
                graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
                Gdiplus::Color c1;
                c1.SetFromCOLORREF(thumb);
                Gdiplus::Pen p1(c1);
                p1.SetWidth(2.0);

                RECT rc = pCustDraw->rect;
                InflateRect(&rc, -2, -2);
                Gdiplus::GraphicsPath path;
                // top left
                path.AddArc(rc.left, rc.top, 4, 4, 180, 90);
                // top right
                path.AddArc(rc.right-4-1, rc.top, 4, 4, 270, 90);
                // bottom right
                path.AddArc(rc.right-4-1, rc.bottom-4-1, 4, 4, 0, 90);
                // bottom left
                path.AddArc(rc.left, rc.bottom-4-1, 4, 4, 90, 90);
                path.CloseFigure();
                graphics.DrawPath(&p1, &path);
            }
            break;
        case HTSCROLL_PAGEFULL:
            {
                Gdiplus::Graphics graphics(pCustDraw->hdc);
                Gdiplus::Color c1;
                c1.SetFromCOLORREF(scroll);
                Gdiplus::SolidBrush brush(c1);
                graphics.FillRectangle(&brush, pCustDraw->rect.left, pCustDraw->rect.top, pCustDraw->rect.right-pCustDraw->rect.left, pCustDraw->rect.bottom-pCustDraw->rect.top);

                if (m_bDirty)
                    CalcLines();

                LONG lastLinePos = -1;
                COLORREF lastColor = (COLORREF)-1;
                int colcount = DOCSCROLLTYPE_END;
                int width = pCustDraw->rect.right-pCustDraw->rect.left;
                int colwidth = width / (colcount-1);
                for (int c = 1; c < colcount; ++c)
                {
                    int drawx = pCustDraw->rect.left + (c-1)*colwidth;
                    for (auto line : m_visibleLineColors[c])
                    {
                        LONG linepos = LONG(pCustDraw->rect.top + (pCustDraw->rect.bottom-pCustDraw->rect.top)*line.first/m_lines);
                        if ((linepos > (lastLinePos+1)) || (lastColor != line.second))
                        {
                            Gdiplus::Color c2(150, GetRValue(line.second), GetGValue(line.second), GetBValue(line.second));
                            c2.SetFromCOLORREF(line.second);
                            Gdiplus::SolidBrush brushline(c2);
                            graphics.FillRectangle(&brushline, drawx, linepos, colwidth, 2);
                            lastLinePos = linepos;
                            lastColor = line.second;
                        }
                    }
                }
                LONG linepos = LONG(pCustDraw->rect.top + (pCustDraw->rect.bottom-pCustDraw->rect.top)*m_curPosVisLine/m_lines);

                Gdiplus::Color c3;
                c3.SetFromCOLORREF(m_curPosColor);
                Gdiplus::SolidBrush brushcurline(c3);
                graphics.FillRectangle(&brushcurline, pCustDraw->rect.left, linepos, pCustDraw->rect.right-pCustDraw->rect.left, 2);
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
        for (int i=0; i<DOCSCROLLTYPE_END; ++i)
            m_visibleLineColors[i].clear();
        for (auto it : m_lineColors)
        {
            m_visibleLineColors[std::get<0>(it.first)][m_pScintilla->Call(SCI_VISIBLEFROMDOCLINE, std::get<1>(it.first))] = it.second;
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

void CDocScroll::RemoveLine( int type, size_t line )
{
    auto t = std::make_tuple(type,line);
    auto foundIt = m_lineColors.find(t);
    if (foundIt != m_lineColors.end())
    {
        m_lineColors.erase(foundIt);
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
    {
        if (!m_lineColors.empty())
            m_bDirty = true;
        m_lineColors.clear();
    }
    else
    {
        auto it = m_lineColors.begin();
        for (; it != m_lineColors.end(); )
        {
            if (std::get<0>(it->first) == type)
            {
                it = m_lineColors.erase(it);
                m_bDirty = true;
            }
            else
                ++it;
        }
    }
    if (m_bDirty)
    {
        for (int i=0; i<DOCSCROLLTYPE_END; ++i)
            m_visibleLineColors[i].clear();
    }
}
