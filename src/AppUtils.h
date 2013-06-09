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
#include <string>
#include <UIRibbon.h>
#include <UIRibbonPropertyHelpers.h>

class CAppUtils
{
public:
    CAppUtils(void);
    ~CAppUtils(void);

    static std::wstring             GetDataPath(HMODULE hMod = NULL);
    static COLORREF                 GetThemeColor(COLORREF clr);
    static std::wstring             GetSessionID();
    static bool                     CheckForUpdate(bool force);
    static bool                     DownloadUpdate(HWND hWnd, bool bInstall);
    static bool                     ShowUpdateAvailableDialog(HWND hWnd);
    static HRESULT CALLBACK         TDLinkClickCallback(HWND hwnd, UINT uNotification, WPARAM wParam, LPARAM lParam, LONG_PTR dwRefData);
    static void                     SetRibbonColors(COLORREF text, COLORREF background, COLORREF highlight);
    static void                     SetRibbonColorsHSB(UI_HSBCOLOR text, UI_HSBCOLOR background, UI_HSBCOLOR highlight);
    static void                     GetRibbonColors(UI_HSBCOLOR& text, UI_HSBCOLOR& background, UI_HSBCOLOR& highlight);

    static void                     RGBToHSB(COLORREF rgb, BYTE& hue, BYTE& saturation, BYTE& brightness);
    static void                     RGBtoHSL(COLORREF color, unsigned int& h, unsigned int& s, unsigned int& l);
    static COLORREF                 HSLtoRGB(const unsigned int& h, const unsigned int& s, const unsigned int& l);

    static void                     SetDarkTheme(bool b = true) { dark = b; }
    static bool                     IsDarkTheme() { return dark; }
private:
    static std::wstring             updatefilename;
    static std::wstring             updateurl;
    static bool                     dark;
};

