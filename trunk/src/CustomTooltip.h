﻿// This file is part of BowPad.
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
#include "AnimationManager.h"

class CCustomToolTip : public CWindow
{
public:
    CCustomToolTip(HINSTANCE hInst)
        : CWindow(hInst)
    {
        m_AnimVarAlpha = Animator::Instance().CreateAnimationVariable(0);
    }
    virtual ~CCustomToolTip()
    {
        DeleteObject(m_hFont);
    }

    void Init(HWND hParent);

    void ShowTip(POINT screenPt, const std::wstring& text, COLORREF * color);
    void HideTip();
protected:
    void OnPaint(HDC hdc, RECT * pRc);
    LRESULT CALLBACK WinMsgHandler(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) override;

private:
    std::wstring    m_infoText;
    COLORREF        m_color = 0;
    HFONT           m_hFont = nullptr;
    bool            m_bShowColorBox = false;
    IUIAnimationVariablePtr m_AnimVarAlpha;
};

