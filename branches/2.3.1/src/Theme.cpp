// This file is part of BowPad.
//
// Copyright (C) 2013-2016 - Stefan Kueng
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

    HRSRC hResource = FindResource(NULL, MAKEINTRESOURCE(IDR_DARKTHEME), L"config");
    if (hResource)
    {
        HGLOBAL hResourceLoaded = LoadResource(NULL, hResource);
        if (hResourceLoaded)
        {
            const char * lpResLock = (const char *) LockResource(hResourceLoaded);
            DWORD dwSizeRes = SizeofResource(NULL, hResource);
            if (lpResLock)
            {
                themeIni.LoadFile(lpResLock, dwSizeRes);
            }
        }
    }


    CSimpleIni::TNamesDepend colors;
    themeIni.GetAllKeys(L"SubstColors", colors);

    std::wstring s;
    bool ok;
    for (const auto& it : colors)
    {
        COLORREF clr1;
        s = it;
        ok = CAppUtils::HexStringToCOLORREF(s, &clr1);
        APPVERIFY(ok);

        COLORREF clr2;
        s = themeIni.GetValue(L"SubstColors", it, L"");
        ok = CAppUtils::HexStringToCOLORREF(s, &clr2);
        APPVERIFY(ok);

        m_colorMap[clr1] = clr2;
    }

    m_dark = CIniSettings::Instance().GetInt64(L"View", L"darktheme", 0) != 0;

    m_bLoaded = true;
}

COLORREF CTheme::GetThemeColor( COLORREF clr ) const
{
    if (m_dark)
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
    m_dark = b;
    CIniSettings::Instance().SetInt64(L"View", L"darktheme", b ? 1 : 0);
    for (auto& cb : m_themeChangeCallbacks)
        cb.second();
}

