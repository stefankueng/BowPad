// This file is part of BowPad.
//
// Copyright (C) 2013-2014, 2016, 2020 Stefan Kueng
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
#include <functional>

using ThemeChangeCallback = std::function<void(void)>;

class CTheme
{
private:
    CTheme();
    ~CTheme();

public:
    static CTheme& Instance();

    /// call this on every WM_SYSCOLORCHANGED message
    void     OnSysColorChanged();
    void     SetDarkTheme(bool b = true);
    bool     IsDarkTheme() const { return m_dark; }
    bool     IsHighContrastMode() const;
    bool     IsHighContrastModeDark() const;
    COLORREF GetThemeColor(COLORREF clr, bool fixed = false) const;
    int      RegisterThemeChangeCallback(ThemeChangeCallback&& cb);

private:
    void Load();

private:
    bool                                         m_bLoaded;
    std::unordered_map<COLORREF, COLORREF>       m_colorMap;
    bool                                         m_isHighContrastMode;
    bool                                         m_isHighContrastModeDark;
    bool                                         m_dark;
    std::unordered_map<int, ThemeChangeCallback> m_themeChangeCallbacks;
    int                                          m_lastThemeChangeCallbackId;
};
