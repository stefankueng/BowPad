// This file is part of BowPad.
//
// Copyright (C) 2013-2017 - Stefan Kueng
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
#include "GDIHelpers.h"

#pragma warning(push)
#pragma warning(disable: 4458) // declaration of 'xxx' hides class member
#include <gdiplus.h>
#pragma warning(pop)


static COLORREF GetThumbColor(const NMCSBCUSTOMDRAW& custDraw)
{
    const bool hot = (custDraw.uState & CDIS_HOT) != 0;
    auto& theme = CTheme::Instance();
    // Use the first "if" to make things more readable in the dark theme. Or comment the "if" part out and use
    // the else path is the original theme code that works but makes things arguably less readable in the dark theme
    // because a small dark blue vertical scroll bar isn't that readable on a black background.
    if (theme.IsDarkTheme())
        return hot ? theme.GetThemeColor(RGB(128, 128, 128)) : theme.GetThemeColor(::GetSysColor(COLOR_3DSHADOW));
    else
        return theme.GetThemeColor(hot ? RGB(200, 200, 255) : ::GetSysColor(COLOR_3DSHADOW));
}

static void DrawTriangle(HDC hdc, COLORREF scroll, COLORREF thumb, const RECT& rect, LONG x0, LONG y0, LONG x1, LONG y1, LONG x2, LONG y2)
{
    Gdiplus::Graphics graphics(hdc);
    graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    Gdiplus::Color c1;
    c1.SetFromCOLORREF(scroll);
    Gdiplus::SolidBrush brush(c1);
    graphics.FillRectangle(&brush, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top);

    Gdiplus::Point trianglepts[3] = { {x0, y0}, {x1, y1}, {x2, y2} };
    Gdiplus::Color c2;
    c2.SetFromCOLORREF(thumb);
    Gdiplus::SolidBrush tribrush(c2);
    graphics.FillPolygon(&tribrush, trianglepts, 3);
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
    Gdiplus::GdiplusStartup(&m_gdiplusToken, &gdiplusStartupInput, nullptr);
}

void CDocScroll::InitScintilla(CScintillaWnd * pScintilla)
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

LRESULT CALLBACK CDocScroll::HandleCustomDraw(WPARAM /*wParam*/, NMCSBCUSTOMDRAW * pCustDraw)
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

    if (pCustDraw->nBar == SB_BOTH)
    {
        // the sizing gripper in the bottom-right corner
        GDIHelpers::FillSolidRect(pCustDraw->hdc, pCustDraw->rect.left, pCustDraw->rect.top, pCustDraw->rect.right, pCustDraw->rect.bottom, CTheme::Instance().GetThemeColor(::GetSysColor(COLOR_SCROLLBAR)));
    }
    else if (pCustDraw->nBar == SB_HORZ)
    {
        const COLORREF scroll = CTheme::Instance().GetThemeColor(::GetSysColor(COLOR_3DFACE));
        const COLORREF thumb = GetThumbColor(*pCustDraw);

        switch (pCustDraw->uItem)
        {
            case HTSCROLL_LEFT:
            DrawTriangle(pCustDraw->hdc, scroll, thumb,
                         pCustDraw->rect,
                         pCustDraw->rect.left + margin,
                         pCustDraw->rect.top + (pCustDraw->rect.bottom - pCustDraw->rect.top) / 2,
                         pCustDraw->rect.right - margin,
                         pCustDraw->rect.top + margin,
                         pCustDraw->rect.right - margin,
                         pCustDraw->rect.bottom - margin);
            break;
            case HTSCROLL_RIGHT:
            DrawTriangle(pCustDraw->hdc, scroll, thumb, pCustDraw->rect,
                         pCustDraw->rect.right - margin,
                         pCustDraw->rect.top + (pCustDraw->rect.bottom - pCustDraw->rect.top) / 2,
                         pCustDraw->rect.left + margin,
                         pCustDraw->rect.top + margin,
                         pCustDraw->rect.left + margin,
                         pCustDraw->rect.bottom - margin);
            break;
            case HTSCROLL_THUMB:
            GDIHelpers::FillSolidRect(pCustDraw->hdc, pCustDraw->rect.left, pCustDraw->rect.top,
                          pCustDraw->rect.right, pCustDraw->rect.bottom, thumb);
            break;
            default:
            GDIHelpers::FillSolidRect(pCustDraw->hdc, pCustDraw->rect.left, pCustDraw->rect.top,
                          pCustDraw->rect.right, pCustDraw->rect.bottom, scroll);
            break;
        }
        return CDRF_SKIPDEFAULT;
    }
    else if (pCustDraw->nBar == SB_VERT)
    {
        const COLORREF scroll = CTheme::Instance().GetThemeColor(::GetSysColor(COLOR_3DFACE));
        const COLORREF thumb = GetThumbColor(*pCustDraw);

        switch (pCustDraw->uItem)
        {
            case HTSCROLL_DOWN:
            DrawTriangle(pCustDraw->hdc, scroll, thumb, pCustDraw->rect,
                         pCustDraw->rect.left + (pCustDraw->rect.right - pCustDraw->rect.left) / 2,
                         pCustDraw->rect.bottom - margin,
                         pCustDraw->rect.left + margin,
                         pCustDraw->rect.top + margin,
                         pCustDraw->rect.right - margin,
                         pCustDraw->rect.top + margin);
            break;
            case HTSCROLL_UP:
            DrawTriangle(pCustDraw->hdc, scroll, thumb, pCustDraw->rect,
                         pCustDraw->rect.left + (pCustDraw->rect.right - pCustDraw->rect.left) / 2,
                         pCustDraw->rect.top + margin,
                         pCustDraw->rect.left + margin,
                         pCustDraw->rect.bottom - margin,
                         pCustDraw->rect.right - margin,
                         pCustDraw->rect.bottom - margin);
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
                path.AddArc(rc.right - 4 - 1, rc.top, 4, 4, 270, 90);
                // bottom right
                path.AddArc(rc.right - 4 - 1, rc.bottom - 4 - 1, 4, 4, 0, 90);
                // bottom left
                path.AddArc(rc.left, rc.bottom - 4 - 1, 4, 4, 90, 90);
                path.CloseFigure();
                graphics.DrawPath(&p1, &path);

                // Fill the vertical thumb with only part of the color
                // so the document info (selections, cursor pos, ...) can
                // still be seen through it.
                c1.SetValue(GDIHelpers::MakeARGB(100, GetRValue(thumb), GetGValue(thumb), GetBValue(thumb)));
                Gdiplus::SolidBrush brush(c1);
                graphics.FillPath(&brush, &path);
            }
            break;
            case HTSCROLL_PAGEFULL:
            {
                Gdiplus::Graphics graphics(pCustDraw->hdc);
                Gdiplus::Color c1;
                c1.SetFromCOLORREF(scroll);
                Gdiplus::SolidBrush brush(c1);
                graphics.FillRectangle(&brush, pCustDraw->rect.left, pCustDraw->rect.top, pCustDraw->rect.right - pCustDraw->rect.left, pCustDraw->rect.bottom - pCustDraw->rect.top);

                if (m_bDirty)
                    CalcLines();

                LONG lastLinePos = -1;
                COLORREF lastColor = (COLORREF)-1;
                int colcount = DOCSCROLLTYPE_END;
                int width = pCustDraw->rect.right - pCustDraw->rect.left;
                int colwidth = width / (colcount - 1);
                for (int c = 1; c < colcount; ++c)
                {
                    int drawx = pCustDraw->rect.left + (c - 1)*colwidth;
                    for (auto line : m_visibleLineColors[c])
                    {
                        LONG linepos = LONG(pCustDraw->rect.top + (pCustDraw->rect.bottom - pCustDraw->rect.top)*line.first / m_lines);
                        if ((linepos > (lastLinePos + 1)) || (lastColor != line.second))
                        {
                            Gdiplus::Color c2;
                            c2.SetFromCOLORREF(line.second);
                            Gdiplus::SolidBrush brushline(c2);
                            graphics.FillRectangle(&brushline, drawx, linepos, colwidth, 2);
                            lastLinePos = linepos;
                            lastColor = line.second;
                        }
                    }
                }
                LONG linepos = LONG(pCustDraw->rect.top + (pCustDraw->rect.bottom - pCustDraw->rect.top)*m_curPosVisLine / m_lines);

                Gdiplus::Color c3;
                c3.SetFromCOLORREF(m_curPosColor);
                Gdiplus::SolidBrush brushcurline(c3);
                graphics.FillRectangle(&brushcurline, pCustDraw->rect.left, linepos, pCustDraw->rect.right - pCustDraw->rect.left, 2);
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
        for (int i = 0; i < DOCSCROLLTYPE_END; ++i)
            m_visibleLineColors[i].clear();
        for (const auto& lc : m_lineColors)
        {
            m_visibleLineColors[lc.first.type][m_pScintilla->Call(SCI_VISIBLEFROMDOCLINE, lc.first.line)] = lc.second;
        }
        m_visibleLines = m_pScintilla->Call(SCI_VISIBLEFROMDOCLINE, m_lines);
        m_bDirty = false;
    }
}

void CDocScroll::AddLineColor(int type, size_t line, COLORREF clr)
{
    auto foundIt = m_lineColors.emplace(LineColor(type,line) , clr);
    if (foundIt.second)
        m_bDirty = true;
    else
    {
        if (foundIt.first->second != clr)
        {
            foundIt.first->second = clr;
            m_bDirty = true;
        }
    }
}

void CDocScroll::RemoveLine(int type, size_t line)
{
    if (m_lineColors.erase(LineColor(type, line)) > 0)
        m_bDirty = true;
}

void CDocScroll::SetTotalLines(size_t lines)
{
    if (m_lines != lines)
        m_bDirty = true;
    m_lines = lines;
}

void CDocScroll::Clear(int type)
{
    if (type == 0)
    {
        if (!m_lineColors.empty())
            m_bDirty = true;
        m_lineColors.clear();
    }
    else
    {
        for (auto it = m_lineColors.begin(); it != m_lineColors.end(); )
        {
            if (it->first.type == type)
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
        for (int i = 0; i < DOCSCROLLTYPE_END; ++i)
            m_visibleLineColors[i].clear();
    }
}
