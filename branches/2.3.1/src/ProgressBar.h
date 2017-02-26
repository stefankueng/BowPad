// This file is part of BowPad.
//
// Copyright (C) 2016-2017 - Stefan Kueng
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

class CProgressBar : public CWindow
{
public :
    CProgressBar(HINSTANCE hInst)
        : CWindow(hInst)
    {};
    virtual ~CProgressBar()
    {
    }

    bool Init(HINSTANCE hInst, HWND hParent);
    void SetRange(DWORD32 start, DWORD32 end);
    void SetPos(DWORD32 pos);
    void SetDarkMode(bool bDark, COLORREF bkgnd);
    void ShowWindow(UINT delay);
protected:
    LRESULT CALLBACK WinMsgHandler(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) override;

private:
    ULONGLONG m_startTicks = 0;
    ULONGLONG m_delay = 0;
};
