// This file is part of BowPad.
//
// Copyright (C) 2013-2017, 2020 - Stefan Kueng
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
#include "Theme.h"
#include "BowPad.h"
#include "SysInfo.h"
#include "AppUtils.h"
#include "GDIHelpers.h"

CTheme::CTheme()
    : m_bLoaded(false)
    , m_dark(false)
    , m_lastThemeChangeCallbackId(0)
    , m_isHighContrastMode(false)
    , m_isHighContrastModeDark(false)
{
}

CTheme::~CTheme()
{
}

CTheme& CTheme::Instance()
{
    static CTheme instance;
    if (!instance.m_bLoaded)
        instance.Load();

    return instance;
}

void CTheme::Load()
{
    CSimpleIni themeIni;

    DWORD       resLen  = 0;
    const char* resData = CAppUtils::GetResourceData(L"config", IDR_DARKTHEME, resLen);
    if (resData != nullptr)
    {
        themeIni.LoadFile(resData, resLen);
    }

    CSimpleIni::TNamesDepend colors;
    themeIni.GetAllKeys(L"SubstColors", colors);

    std::wstring s;
    bool         ok;
    for (const auto& it : colors)
    {
        COLORREF clr1;
        s  = it;
        ok = GDIHelpers::HexStringToCOLORREF(s, &clr1);
        APPVERIFY(ok);

        COLORREF clr2;
        s  = themeIni.GetValue(L"SubstColors", it, L"");
        ok = GDIHelpers::HexStringToCOLORREF(s, &clr2);
        APPVERIFY(ok);

        m_colorMap[clr1] = clr2;
    }

    OnSysColorChanged();

    m_bLoaded = true;
}

void CTheme::OnSysColorChanged()
{
    m_isHighContrastModeDark = false;
    m_isHighContrastMode     = false;
    HIGHCONTRAST hc          = {sizeof(HIGHCONTRAST)};
    SystemParametersInfo(SPI_GETHIGHCONTRAST, sizeof(HIGHCONTRAST), &hc, FALSE);
    if ((hc.dwFlags & HCF_HIGHCONTRASTON) != 0)
    {
        m_isHighContrastMode = true;
        // check if the high contrast mode is dark
        float h1, h2, s1, s2, l1, l2;
        GDIHelpers::RGBtoHSL(::GetSysColor(COLOR_WINDOWTEXT), h1, s1, l1);
        GDIHelpers::RGBtoHSL(::GetSysColor(COLOR_WINDOW), h2, s2, l2);
        m_isHighContrastModeDark = l2 < l1;
    }
    m_dark = CIniSettings::Instance().GetInt64(L"View", L"darktheme", 0) != 0 && !IsHighContrastMode();
}

bool CTheme::IsHighContrastMode() const
{
    return m_isHighContrastMode;
}

bool CTheme::IsHighContrastModeDark() const
{
    return m_isHighContrastModeDark;
}

COLORREF CTheme::GetThemeColor(COLORREF clr, bool fixed /*= false*/) const
{
    if (m_dark || (fixed && m_isHighContrastModeDark))
    {
        auto cIt = m_colorMap.find(clr);
        if (cIt != m_colorMap.end())
            return cIt->second;

        float h, s, l;
        GDIHelpers::RGBtoHSL(clr, h, s, l);
        l = 100 - l;
        return GDIHelpers::HSLtoRGB(h, s, l);
    }

    return clr;
}

int CTheme::RegisterThemeChangeCallback(ThemeChangeCallback&& cb)
{
    ++m_lastThemeChangeCallbackId;
    int nextThemeCallBackId = m_lastThemeChangeCallbackId;
    m_themeChangeCallbacks.emplace(nextThemeCallBackId, std::move(cb));
    return nextThemeCallBackId;
}

void CTheme::SetDarkTheme(bool b /*= true*/)
{
    if (IsHighContrastMode())
        return;
    m_dark = b;
    CIniSettings::Instance().SetInt64(L"View", L"darktheme", b ? 1 : 0);
    for (auto& cb : m_themeChangeCallbacks)
        cb.second();
}
