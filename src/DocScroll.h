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
#pragma once
#include "coolscroll.h"
#include <deque>
#include <tuple>

#define DOCSCROLLTYPE_SELTEXT           1
#define DOCSCROLLTYPE_SEARCHTEXT        2

class CScintillaWnd;

class CDocScroll
{
public:
    CDocScroll();
    ~CDocScroll();

    void                        InitScintilla(CScintillaWnd * pScintilla);
    LRESULT CALLBACK            HandleCustomDraw( WPARAM wParam, NMCSBCUSTOMDRAW * pCustDraw );
    void                        SetTotalLines(size_t lines);
    void                        Clear(int type);
    void                        AddLineColor(int type, size_t line, COLORREF clr);
    void                        SetCurrentPos(size_t visibleline, COLORREF clr) { m_curPosVisLine = visibleline; m_curPosColor = clr; }
    void                        VisibleLinesChanged() { m_bDirty = true; }
private:
    void                        CalcLines();


    std::map<size_t, COLORREF>                  m_visibleLineColors;
    std::map<std::tuple<int,size_t>,COLORREF>   m_lineColors;
    size_t                                      m_visibleLines;
    size_t                                      m_lines;
    size_t                                      m_curPosVisLine;
    COLORREF                                    m_curPosColor;
    CScintillaWnd *                             m_pScintilla;
    bool                                        m_bDirty;
};