// This file is part of BowPad.
//
// Copyright (C) 2013, 2016 - Stefan Kueng
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
#include "ColorButton.h"
#include <StringUtils.h>

#include <Commdlg.h>
#include <wingdi.h>

namespace
{
    COLORREF g_acrCustClr[16];

    static inline std::wstring GetWindowText(HWND hwnd)
    {
        wchar_t buf[20]; // We're dealing with color values no longer than three chars.
        ::GetWindowText(hwnd, buf, _countof(buf));
        return buf;
    }

    static BOOL CALLBACK EnumChildWindowProc(HWND hwnd, LPARAM lParam)
    {
        auto& state = *reinterpret_cast<std::pair<std::wstring, std::vector<HWND>>*>(lParam);

        if (state.first.empty())
            state.second.push_back(hwnd);
        else
        {
            wchar_t className[257];
            auto status = ::GetClassName(hwnd, className, _countof(className));
            if (status > 0)
            {
                if (_wcsicmp(className, state.first.c_str()) == 0)
                    state.second.push_back(hwnd);
            }
        }
        return TRUE;
    }

    // Empty classname means match child windows of ANY classname.
    inline std::vector<HWND> GetChildWindows(HWND hwnd, const std::wstring& classname)
    {
        std::pair<std::wstring, std::vector<HWND>> state;
        state.first = classname;
        EnumChildWindows(hwnd, EnumChildWindowProc, reinterpret_cast<LPARAM>(&state));
        return state.second;
    }
}

CColorButton::CColorButton()
{
}

CColorButton::~CColorButton()
{
}

bool CColorButton::ConvertToColorButton(HWND hwndCtl)
{
    // Subclass the existing control.
    m_pfnOrigCtlProc = (WNDPROC)GetWindowLongPtr(hwndCtl, GWLP_WNDPROC);
    SetWindowLongPtr(hwndCtl, GWLP_WNDPROC, (LONG_PTR)_ColorButtonProc);
    SetWindowLongPtr(hwndCtl, GWLP_USERDATA, (LPARAM)this);
    m_hwnd = hwndCtl;
    return true;
}

BOOL CColorButton::ConvertToColorButton(HWND hwndParent, UINT uiCtlId)
{
    m_ctlId = uiCtlId;
    return ConvertToColorButton(GetDlgItem(hwndParent, uiCtlId));
}

LRESULT CALLBACK CColorButton::_ColorButtonProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    CColorButton* pColorButton = GetObjectFromWindow(hwnd);
    switch (message)
    {
        case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc;
            hdc = BeginPaint(hwnd, &ps);
            RECT rc;
            GetClientRect(hwnd, &rc);
            SetBkColor(hdc, pColorButton->m_color);
            ExtTextOut(hdc, rc.left, rc.top, ETO_CLIPPED | ETO_OPAQUE, &rc, L"", 0, nullptr);
            EndPaint(hwnd, &ps);
            return 0L;
        }
        break;
        case WM_ERASEBKGND:
        return TRUE;
        case WM_KEYUP:
        {
            if (wParam != VK_SPACE)
                break;
        }
        // Fall through
        case WM_LBUTTONUP:
        {
            CHOOSECOLOR cc = { 0 };
            cc.lStructSize = sizeof(CHOOSECOLOR);
            cc.hwndOwner = hwnd;
            cc.rgbResult = pColorButton->m_color;
            cc.lpCustColors = g_acrCustClr;
            cc.lCustData = (LPARAM)pColorButton;
            cc.lpfnHook = CCHookProc;
            cc.Flags = CC_ANYCOLOR | CC_RGBINIT | CC_ENABLEHOOK | CC_FULLOPEN;

            if (ChooseColor(&cc) != FALSE)
            {
                pColorButton->m_dialogResult = ColorButtonDialogResult::Ok;
            }
            else if (pColorButton->m_hasLastColor)
            {
                pColorButton->SetColor(cc.rgbResult);
                pColorButton->m_dialogResult = ColorButtonDialogResult::Cancel;
                SendMessage(GetParent(hwnd), WM_COMMAND, pColorButton->m_ctlId, (LPARAM)hwnd);
            }
            return 0;
        }
        break;
        case WM_DESTROY:
        SetWindowLongPtr(hwnd, GWLP_WNDPROC, (LONG_PTR)pColorButton->m_pfnOrigCtlProc);
        break;
    }

    return CallWindowProc(pColorButton->m_pfnOrigCtlProc, hwnd, message, wParam, lParam);
}

void CColorButton::SetColor(COLORREF clr)
{
    m_color = clr;
    InvalidateRect(m_hwnd, nullptr, FALSE);
}

// This function is attempts to hijack and hack the standard basic Windows
// Color Control into allow repeated colors to be selected without having
// to press OK each time. Each selection initiates a WM_COMMAND.
// If Cancel is chosen, a WM_COMMAND is sent that undoes the previous color set.
// The logic in this function is based on an idea from this article:
// http://stackoverflow.com/questions/16583139/get-color-property-while-colordialog-still-open-before-confirming-the-dialog
// As the article states, it is questionable to do what we do in this function
// as the code i subject to break if the Windows Color Control changes
// in the future.

UINT_PTR CALLBACK CColorButton::CCHookProc(
    _In_ HWND   hdlg,
    _In_ UINT   uiMsg,
    _In_ WPARAM wParam,
    _In_ LPARAM lParam
    )
{
    UNREFERENCED_PARAMETER(wParam);
    switch (uiMsg)
    {
        case WM_INITDIALOG:
        {
            const CHOOSECOLOR& cc = *reinterpret_cast<CHOOSECOLOR*>(lParam);
            // Crude attempt to assert this data slot isn't used by anybody but us.
            assert(GetWindowLongPtr(hdlg, GWLP_USERDATA) == LONG_PTR(0));
            SetWindowLongPtr(hdlg, GWLP_USERDATA, cc.lCustData);
            CColorButton* pColorBtn = reinterpret_cast<CColorButton*>(cc.lCustData);
            auto mainWindow = GetAncestor(hdlg, GA_ROOT);
            pColorBtn->m_colorEdits = GetChildWindows(mainWindow, L"Edit");
            assert(pColorBtn->m_colorEdits.size() == 6);
        }
        break;
        case WM_CTLCOLOREDIT:
        {
            // InitDialog should have set this.
            CColorButton* pColorButton = GetObjectFromWindow(hdlg);
            assert(pColorButton != nullptr);
            assert(pColorButton->m_colorEdits.size() == 6);

            // See top of function for what's going on here.
            std::vector<std::wstring> colorText;
            for (auto hwnd : pColorButton->m_colorEdits)
                colorText.push_back(GetWindowText(hwnd));

            COLORREF color = 0;
            try
            {
                BYTE r = (BYTE)std::stoi(colorText[3]);
                BYTE g = (BYTE)std::stoi(colorText[4]);
                BYTE b = (BYTE)std::stoi(colorText[5]);
                color = RGB(r, g, b);
            }
            catch (const std::exception& /*ex*/)
            {
                return 0;
            }
            if (!pColorButton->m_hasLastColor || color != pColorButton->m_lastColor)
            {
                pColorButton->m_lastColor = color;
                pColorButton->m_hasLastColor = true;
#ifdef _DEBUG
                std::wstring msg = CStringUtils::Format(L"RGB(%d,%d,%d)\n",
                                                        (int)GetRValue(color), (int)GetGValue(color), (int)GetBValue(color));
                OutputDebugString(msg.c_str());
#endif
                pColorButton->SetColor(color);
                SendMessage(GetParent(hdlg), WM_COMMAND,
                            pColorButton->m_ctlId, LPARAM(GetParent(hdlg)));
            }
            break;
        }
    }
    return 0;
}
