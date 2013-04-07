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

class CDocScroll
{
public:
    CDocScroll();
    ~CDocScroll();

    void                InitScintilla(HWND hWnd);
    LRESULT CALLBACK    HandleCustomDraw( WPARAM wParam, NMCSBCUSTOMDRAW * pCustDraw );
    void                SetCurLine(size_t line) { m_line = line; }
    void                SetTotalLines(size_t lines) { m_lines = lines; }
private:
    size_t              m_line;
    size_t              m_lines;
};