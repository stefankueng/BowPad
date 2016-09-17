// This file is part of BowPad.
//
// Copyright (C) 2013-2014, 2016 Stefan Kueng
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
#include <unordered_map>
#include <UIRibbon.h>
#include <UIRibbonPropertyHelpers.h>

class CTheme
{
private:
    CTheme();
    ~CTheme();

public:
    static CTheme& Instance();

    void                            SetDarkTheme(bool b = true);
    bool                            IsDarkTheme() const { return m_dark; }
    COLORREF                        GetThemeColor(COLORREF clr) const;

    void                            SetRibbonColors(COLORREF text, COLORREF background, COLORREF highlight);
    void                            SetRibbonColorsHSB(UI_HSBCOLOR text, UI_HSBCOLOR background, UI_HSBCOLOR highlight);
    void                            GetRibbonColors(UI_HSBCOLOR& text, UI_HSBCOLOR& background, UI_HSBCOLOR& highlight) const;

    static void                     RGBToHSB(COLORREF rgb, BYTE& hue, BYTE& saturation, BYTE& brightness);
    static void                     RGBtoHSL(COLORREF color, float& h, float& s, float& l);
    static COLORREF                 HSLtoRGB(float h, float s, float l);

private:
    void                            Load();

private:
    bool                            m_bLoaded;
    std::unordered_map<COLORREF, COLORREF>    m_colorMap;
    bool                            m_dark;
};
