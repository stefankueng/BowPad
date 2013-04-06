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
#include "stdafx.h"
#include "StatusBar.h"

#include <Commctrl.h>

bool CStatusBar::Init(HINSTANCE /*hInst*/, HWND hParent, int nbParts)
{
    InitCommonControls();

    CreateEx(0, WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP, hParent, 0, STATUSCLASSNAME);

    if (!*this)
    {
        return false;
    }

    // Get the coordinates of the parent window's client area.
    RECT rcClient;
    GetClientRect(hParent, &rcClient);

    // Allocate an array for holding the right edge coordinates.
    HLOCAL  hloc = LocalAlloc(LHND, sizeof(int) * nbParts);
    PINT paParts = (PINT) LocalLock(hloc);

    // Calculate the right edge coordinate for each part, and
    // copy the coordinates to the array.
    int nWidth = rcClient.right / nbParts;
    int rightEdge = nWidth;
    for (int i = 0; i < nbParts; i++)
    {
        paParts[i] = rightEdge;
        rightEdge += nWidth;
    }

    // Tell the status bar to create the window parts.
    SendMessage(*this, SB_SETPARTS, (WPARAM) nbParts, (LPARAM)paParts);

    // Free the array, and return.
    LocalUnlock(hloc);
    LocalFree(hloc);

    GetClientRect(*this, &rcClient);
    m_height = rcClient.bottom-rcClient.top;
    return true;
}

LRESULT CALLBACK CStatusBar::WinMsgHandler( HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam )
{
    if (prevWndProc)
        return prevWndProc(hwnd, uMsg, wParam, lParam);
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

