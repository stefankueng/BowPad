// This file is part of BowPad.
//
// Copyright (C) 2016 - Stefan Kueng
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
#include "BaseWindow.h"

class CTabBtn : public CWindow
{
public :
    CTabBtn(HINSTANCE hInst)
        : CWindow(hInst)
        , m_hFont(nullptr)
    {};
    virtual ~CTabBtn()
    {
        if (m_hFont)
            DeleteObject(m_hFont);
    }

    bool Init(HINSTANCE hInst, HWND hParent, HMENU id);
    void SetFont(const TCHAR *fontName, int fontSize);
    bool SetText(const TCHAR *str);

protected:
    LRESULT CALLBACK WinMsgHandler(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) override;

private:
    HFONT       m_hFont;
};
