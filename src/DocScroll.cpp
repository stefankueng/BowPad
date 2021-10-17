// This file is part of BowPad.
//
// Copyright (C) 2013-2018, 2020-2021 - Stefan Kueng
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
#include "Theme.h"
#include "GDIHelpers.h"
#include "DPIAware.h"

#pragma warning(push)
#pragma warning(disable : 4458) // declaration of 'xxx' hides class member
#include <gdiplus.h>
#pragma warning(pop)

static COLORREF GetThumbColor(double hotFraction)
{
    auto& theme = CTheme::Instance();
    // Use the first "if" to make things more readable in the dark theme. Or comment the "if" part out and use
    // the else path is the original theme code that works but makes things arguably less readable in the dark theme
    // because a small dark blue vertical scroll bar isn't that readable on a black background.
    if (theme.IsDarkTheme())
        return theme.GetThemeColor(GDIHelpers::InterpolateColors(::GetSysColor(COLOR_3DDKSHADOW), RGB(70, 70, 70), hotFraction));
    else
        return theme.GetThemeColor(GDIHelpers::InterpolateColors(RGB(150, 150, 200), ::GetSysColor(COLOR_3DDKSHADOW), hotFraction));
}

static void DrawThumb(HWND hwnd, HDC hdc, COLORREF thumb, const RECT& rect, UINT uBar)
{
    Gdiplus::Graphics graphics(hdc);
    graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    Gdiplus::Color c1;
    c1.SetValue(GDIHelpers::MakeARGB(152, GetRValue(thumb), GetGValue(thumb), GetBValue(thumb)));
    Gdiplus::SolidBrush brush(c1);
    const int           margin = CDPIAware::Instance().Scale(hwnd, 3);
    Gdiplus::Rect       rc(
        (uBar == SB_HORZ ? rect.left : rect.left + margin),
        (uBar == SB_HORZ ? rect.top + margin : rect.top),
        (uBar == SB_HORZ ? rect.right - rect.left : rect.right - rect.left - margin * 2 - 1),
        (uBar == SB_HORZ ? rect.bottom - rect.top - margin * 2 - 1 : rect.bottom - rect.top));
    Gdiplus::GraphicsPath path;
    path.AddRectangle(rc);

    graphics.FillPath(&brush, &path);
}

static void DrawTriangle(HDC hdc, COLORREF scroll, COLORREF thumb, const RECT& rect, UINT uBar, LONG direction)
{
    GDIHelpers::FillSolidRect(hdc, rect.left, rect.top, rect.right, rect.bottom, scroll);

    Gdiplus::Graphics graphics(hdc);
    graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    Gdiplus::Point trianglePts[3] = {};
    Gdiplus::Color c2;
    c2.SetFromCOLORREF(thumb);
    Gdiplus::SolidBrush triBrush(c2);
    auto                margin = (rect.bottom - rect.top) / 3;
    if (uBar == SB_HORZ)
    {
        trianglePts[0].Y = (rect.top + rect.bottom) / 2;
        trianglePts[1].Y = rect.top + margin - 1;
        trianglePts[2].Y = rect.bottom - margin;
        margin += (trianglePts[2].Y - trianglePts[1].Y) / 6;
        if (direction == HTSCROLL_LEFT)
        {
            trianglePts[0].X = rect.left + margin;
            trianglePts[1].X = trianglePts[2].X = rect.right - margin;
        }
        else if (direction == HTSCROLL_RIGHT)
        {
            trianglePts[0].X = rect.right - margin;
            trianglePts[1].X = trianglePts[2].X = rect.left + margin;
        }
    }
    else if (uBar == SB_VERT)
    {
        trianglePts[0].X = (rect.left + rect.right) / 2;
        trianglePts[1].X = rect.left + margin - 1;
        trianglePts[2].X = rect.right - margin;
        margin += (trianglePts[2].X - trianglePts[1].X) / 6;
        if (direction == HTSCROLL_UP)
        {
            trianglePts[0].Y = rect.top + margin;
            trianglePts[1].Y = trianglePts[2].Y = rect.bottom - margin;
        }
        else if (direction == HTSCROLL_DOWN)
        {
            trianglePts[0].Y = rect.bottom - margin;
            trianglePts[1].Y = trianglePts[2].Y = rect.top + margin;
        }
    }
    graphics.FillPolygon(&triBrush, trianglePts, 3);
}

CDocScroll::~CDocScroll()
{
    Gdiplus::GdiplusShutdown(m_gdiplusToken);
}

CDocScroll::CDocScroll()
    : m_visibleLines(0)
    , m_lines(0)
    , m_curPosVisLine(0)
    , m_curPosColor(0)
    , m_pScintilla(nullptr)
    , m_bDirty(false)
    , m_bHotHL(false)
    , m_bHotHR(false)
    , m_bHotHT(false)
    , m_bHotVL(false)
    , m_bHotVR(false)
    , m_bHotVT(false)
{
    Gdiplus::GdiplusStartupInput gdiplusStartupInput;
    Gdiplus::GdiplusStartup(&m_gdiplusToken, &gdiplusStartupInput, nullptr);

    m_animVarHL = Animator::Instance().CreateAnimationVariable(0.0, 1.0);
    m_animVarHR = Animator::Instance().CreateAnimationVariable(0.0, 1.0);
    m_animVarHT = Animator::Instance().CreateAnimationVariable(0.0, 1.0);
    m_animVarVL = Animator::Instance().CreateAnimationVariable(0.0, 1.0);
    m_animVarVR = Animator::Instance().CreateAnimationVariable(0.0, 1.0);
    m_animVarVT = Animator::Instance().CreateAnimationVariable(0.0, 1.0);
}

void CDocScroll::InitScintilla(CScintillaWnd* pScintilla)
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

LRESULT CALLBACK CDocScroll::HandleCustomDraw(WPARAM /*wParam*/, NMCSBCUSTOMDRAW* pCustDraw)
{
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
        GDIHelpers::FillSolidRect(pCustDraw->hdc,
                                  pCustDraw->rect.left, pCustDraw->rect.top, pCustDraw->rect.right, pCustDraw->rect.bottom,
                                  CTheme::Instance().GetThemeColor(::GetSysColor(COLOR_3DFACE)));
    }
    else if (pCustDraw->dwDrawStage == CDDS_ITEMPREPAINT)
    {
        const COLORREF scroll = CTheme::Instance().GetThemeColor(::GetSysColor(COLOR_3DFACE));
        if (pCustDraw->nBar == SB_HORZ)
        {
            switch (pCustDraw->uItem)
            {
                case HTSCROLL_LEFT:
                {
                    bool hotNow = (pCustDraw->uState & CDIS_HOT) != 0;
                    if (m_bHotHL != hotNow)
                    {
                        m_bHotHL = hotNow;
                        AnimateFraction(m_animVarHL, hotNow ? 1.0 : 0.0);
                    }
                    auto thumb = GetThumbColor(Animator::GetValue(m_animVarHL));
                    DrawTriangle(pCustDraw->hdc, scroll, thumb, pCustDraw->rect,
                                 pCustDraw->nBar, pCustDraw->uItem);
                }
                break;
                case HTSCROLL_RIGHT:
                {
                    bool hotNow = (pCustDraw->uState & CDIS_HOT) != 0;
                    if (m_bHotHR != hotNow)
                    {
                        m_bHotHR = hotNow;
                        AnimateFraction(m_animVarHR, hotNow ? 1.0 : 0.0);
                    }
                    auto thumb = GetThumbColor(Animator::GetValue(m_animVarHR));
                    DrawTriangle(pCustDraw->hdc, scroll, thumb, pCustDraw->rect,
                                 pCustDraw->nBar, pCustDraw->uItem);
                }
                break;
                case HTSCROLL_THUMB:
                {
                    bool hotNow = (pCustDraw->uState & CDIS_HOT) != 0;
                    if (m_bHotHT != hotNow)
                    {
                        m_bHotHT = hotNow;
                        AnimateFraction(m_animVarHT, hotNow ? 1.0 : 0.0);
                    }
                    auto thumb = GetThumbColor(Animator::GetValue(m_animVarHT));
                    DrawThumb(pCustDraw->hdr.hwndFrom, pCustDraw->hdc, thumb, pCustDraw->rect, pCustDraw->nBar);
                }
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
            switch (pCustDraw->uItem)
            {
                case HTSCROLL_DOWN:
                {
                    bool hotNow = (pCustDraw->uState & CDIS_HOT) != 0;
                    if (m_bHotVL != hotNow)
                    {
                        m_bHotVL = hotNow;
                        AnimateFraction(m_animVarVL, hotNow ? 1.0 : 0.0);
                    }
                    auto thumb = GetThumbColor(Animator::GetValue(m_animVarVL));
                    DrawTriangle(pCustDraw->hdc, scroll, thumb, pCustDraw->rect,
                                 pCustDraw->nBar, pCustDraw->uItem);
                }
                break;
                case HTSCROLL_UP:
                {
                    bool hotNow = (pCustDraw->uState & CDIS_HOT) != 0;
                    if (m_bHotVR != hotNow)
                    {
                        m_bHotVR = hotNow;
                        AnimateFraction(m_animVarVR, hotNow ? 1.0 : 0.0);
                    }
                    auto thumb = GetThumbColor(Animator::GetValue(m_animVarVR));
                    DrawTriangle(pCustDraw->hdc, scroll, thumb, pCustDraw->rect,
                                 pCustDraw->nBar, pCustDraw->uItem);
                }
                break;
                case HTSCROLL_THUMB:
                {
                    bool hotNow = (pCustDraw->uState & CDIS_HOT) != 0;
                    if (m_bHotVT != hotNow)
                    {
                        m_bHotVT = hotNow;
                        AnimateFraction(m_animVarVT, hotNow ? 1.0 : 0.0);
                    }
                    auto thumb = GetThumbColor(Animator::GetValue(m_animVarVT));
                    DrawThumb(pCustDraw->hdr.hwndFrom, pCustDraw->hdc, thumb, pCustDraw->rect, pCustDraw->nBar);
                }
                break;
                case HTSCROLL_PAGEFULL:
                {
                    Gdiplus::Graphics graphics(pCustDraw->hdc);
                    Gdiplus::Color    c1;
                    c1.SetFromCOLORREF(scroll);
                    Gdiplus::SolidBrush brush(c1);
                    graphics.FillRectangle(&brush, static_cast<INT>(pCustDraw->rect.left),
                                           static_cast<INT>(pCustDraw->rect.top),
                                           static_cast<INT>(pCustDraw->rect.right - pCustDraw->rect.left),
                                           static_cast<INT>(pCustDraw->rect.bottom - pCustDraw->rect.top));

                    if (m_bDirty)
                        CalcLines();

                    LONG     lastLinePos = -1;
                    COLORREF lastColor   = static_cast<COLORREF>(-1);
                    int      colCount    = DOCSCROLLTYPE_END;
                    int      width       = pCustDraw->rect.right - pCustDraw->rect.left;
                    int      colWidth    = width / (colCount - 1);
                    for (int c = 1; c < colCount; ++c)
                    {
                        int drawX = pCustDraw->rect.left + (c - 1) * colWidth;
                        for (const auto& line : m_visibleLineColors[c])
                        {
                            LONG linePos = static_cast<LONG>(pCustDraw->rect.top + (pCustDraw->rect.bottom - pCustDraw->rect.top) * line.first / m_visibleLines);
                            if ((linePos > (lastLinePos + 1)) || (lastColor != line.second))
                            {
                                Gdiplus::Color c2;
                                c2.SetFromCOLORREF(line.second);
                                Gdiplus::SolidBrush brushLine(c2);
                                graphics.FillRectangle(&brushLine, drawX, linePos, colWidth, CDPIAware::Instance().Scale(pCustDraw->hdr.hwndFrom, 2));
                                lastLinePos = linePos;
                                lastColor   = line.second;
                            }
                        }
                    }
                    LONG linePos = static_cast<LONG>(pCustDraw->rect.top + (pCustDraw->rect.bottom - pCustDraw->rect.top) * m_curPosVisLine / m_visibleLines);

                    Gdiplus::Color c3;
                    c3.SetFromCOLORREF(m_curPosColor);
                    Gdiplus::SolidBrush brushCurLine(c3);
                    graphics.FillRectangle(&brushCurLine, pCustDraw->rect.left, linePos, pCustDraw->rect.right - pCustDraw->rect.left, CDPIAware::Instance().Scale(pCustDraw->hdr.hwndFrom, 2));
                }
                break;
                default:
                    break;
            }
            return CDRF_SKIPDEFAULT;
        }
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
            m_visibleLineColors[lc.first.type][m_pScintilla->Scintilla().VisibleFromDocLine(lc.first.line)] = lc.second;
        }
        m_visibleLines = m_pScintilla->Scintilla().VisibleFromDocLine(m_lines);
        m_bDirty       = false;
    }
}

void CDocScroll::AnimateFraction(AnimationVariable& animVar, double endVal)
{
    auto transHot   = Animator::Instance().CreateSmoothStopTransition(animVar, 0.3, endVal);
    auto storyBoard = Animator::Instance().CreateStoryBoard();
    if (storyBoard && transHot)
    {
        storyBoard->AddTransition(animVar.m_animVar, transHot);
        Animator::Instance().RunStoryBoard(storyBoard, [this]() {
            SetWindowPos(*m_pScintilla, nullptr, 0, 0, 0, 0,
                         SWP_DRAWFRAME | SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOZORDER);
        });
    }
    else
        SetWindowPos(*m_pScintilla, nullptr, 0, 0, 0, 0,
                     SWP_DRAWFRAME | SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOZORDER);
}

void CDocScroll::AddLineColor(int type, size_t line, COLORREF clr)
{
    auto foundIt = m_lineColors.emplace(LineColor(type, line), clr);
    if (foundIt.second)
        m_bDirty = true;
    else
    {
        if (foundIt.first->second != clr)
        {
            foundIt.first->second = clr;
            m_bDirty              = true;
        }
    }
}

void CDocScroll::RemoveLine(int type, size_t line)
{
    if (m_lineColors.erase(LineColor(type, line)) > 0)
        m_bDirty = true;
}

void CDocScroll::VisibleLinesChanged()
{
    auto visibleLines = m_pScintilla->Scintilla().VisibleFromDocLine(m_lines);
    if (m_visibleLines != static_cast<size_t>(visibleLines))
    {
        m_bDirty = true;
        SetWindowPos(*m_pScintilla, nullptr, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED | SWP_DRAWFRAME);
    }
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
        for (auto it = m_lineColors.begin(); it != m_lineColors.end();)
        {
            if (it->first.type == type)
            {
                it       = m_lineColors.erase(it);
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
