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
#include <map>
#include <UIRibbon.h>
#include <UIRibbonPropertyHelpers.h>

class CTheme
{
private:
    CTheme(void);
    ~CTheme(void);

public:
    static CTheme& Instance();

    void                            SetDarkTheme(bool b = true) { dark = b; }
    bool                            IsDarkTheme() { return dark; }
    COLORREF                        GetThemeColor(COLORREF clr);

    void                            SetRibbonColors(COLORREF text, COLORREF background, COLORREF highlight);
    void                            SetRibbonColorsHSB(UI_HSBCOLOR text, UI_HSBCOLOR background, UI_HSBCOLOR highlight);
    void                            GetRibbonColors(UI_HSBCOLOR& text, UI_HSBCOLOR& background, UI_HSBCOLOR& highlight);

    void                            RGBToHSB(COLORREF rgb, BYTE& hue, BYTE& saturation, BYTE& brightness);
    void                            RGBtoHSL(COLORREF color, unsigned int& h, unsigned int& s, unsigned int& l);
    COLORREF                        HSLtoRGB(const unsigned int& h, const unsigned int& s, const unsigned int& l);

private:
    void                            Load();

private:
    bool                            m_bLoaded;
    std::map<COLORREF, COLORREF>    m_colorMap;
    bool                            dark;
};
