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

class CScintillaWnd;

class CDocScroll
{
public:
    CDocScroll();
    ~CDocScroll();

    void                        InitScintilla(CScintillaWnd * pScintilla);
    LRESULT CALLBACK            HandleCustomDraw( WPARAM wParam, NMCSBCUSTOMDRAW * pCustDraw );
    void                        SetTotalLines(size_t lines);
    void                        Clear() { m_lineColors.clear(); m_visibleLineColors.clear(); }
    void                        AddLineColor(size_t line, COLORREF clr);
    void                        VisibleLinesChanged() { m_bDirty = true; }
private:
    void                        CalcLines();


    std::map<size_t, COLORREF>  m_visibleLineColors;
    std::map<size_t, COLORREF>  m_lineColors;
    size_t                      m_visibleLines;
    size_t                      m_lines;
    CScintillaWnd *             m_pScintilla;
    bool                        m_bDirty;
};