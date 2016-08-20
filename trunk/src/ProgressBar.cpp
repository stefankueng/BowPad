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

#include "stdafx.h"
#include "ProgressBar.h"
#include "Theme.h"
#include "GDIHelpers.h"
#include "BowPad.h"
#include <thread>
#include <Uxtheme.h>

bool CProgressBar::Init(HINSTANCE /*hInst*/, HWND hParent)
{
    InitCommonControls();

    // WS_POPUP is required instead of WS_CHILD because this
    // control is to be used while WM_SETREDRAW is set to false
    // on the main window. With WS_CHILD, the progress bar
    // wouldn't update either.
    CreateEx(0, WS_POPUP, hParent, nullptr, PROGRESS_CLASS);

    if (!*this)
        return false;

    return true;
}

void CProgressBar::SetRange(DWORD32 start, DWORD32 end)
{
    SendMessage(*this, PBM_SETRANGE32, start, end);
}

void CProgressBar::SetPos(DWORD32 pos)
{
    SendMessage(*this, PBM_SETPOS, pos, 0);
}

void CProgressBar::SetDarkMode(bool bDark, COLORREF bkgnd)
{
    if (bDark)
    {
        // color can only be changed if the control isn't themed.
        // TODO: maybe a custom control where everything is painted ourselves
        // might be better than using the Windows progress bar control.
        SetWindowTheme(*this, L"", L"");
        SendMessage(*this, PBM_SETBKCOLOR, 0, bkgnd);
    }
    else
    {
        SetWindowTheme(*this, L"Explorer", nullptr);
    }
}

LRESULT CALLBACK CProgressBar::WinMsgHandler(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    if (prevWndProc)
        return prevWndProc(hwnd, uMsg, wParam, lParam);
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

