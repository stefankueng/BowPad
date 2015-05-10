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
#include <windows.h>

class CColorButton
{
public:
    CColorButton(void);
    virtual ~CColorButton(void);

    BOOL ConvertToColorButton(HWND hwndParent, UINT uiCtlId);

    void SetColor(COLORREF clr);
    COLORREF GetColor() const { return m_color; }

protected:

private:
    WNDPROC     m_pfnOrigCtlProc;
    COLORREF    m_color;
    HWND        m_hwnd;
    UINT        m_ctlId;

    BOOL ConvertToColorButton(HWND hwndCtl);
    inline static CColorButton * GetObjectFromWindow(HWND hWnd)
    {
        return (CColorButton *)GetWindowLongPtr(hWnd, GWLP_USERDATA);
    }
    static LRESULT CALLBACK _ColorButtonProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
};
