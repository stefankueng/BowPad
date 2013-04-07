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
#include "BaseWindow.h"
#include <Commctrl.h>

class CStatusBar : public CWindow
{
public :
    CStatusBar(HINSTANCE hInst) : CWindow(hInst) {};
    virtual ~CStatusBar(){}

    bool Init(HINSTANCE hInst, HWND hParent, int nbParts);
    int GetHeight() const { return m_height; }
    bool SetText(const TCHAR *str, int whichPart) const
    {
        return (::SendMessage(*this, SB_SETTEXT, whichPart, (LPARAM)str) == TRUE);
    };


    virtual LRESULT CALLBACK WinMsgHandler( HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam );

private:
    int     m_height;
};

