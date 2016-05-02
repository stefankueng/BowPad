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
#include "CommandHandler.h"

extern IUIFramework *g_pFramework;  // Reference to the Ribbon framework.

CTheme::CTheme()
    : m_bLoaded(false)
    , dark(false)
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
        ok = CAppUtils::HexStringToCOLORREF(it, &clr1);
        APPVERIFY(ok);

        COLORREF clr2;
        s = themeIni.GetValue(L"SubstColors", it);
        ok = CAppUtils::HexStringToCOLORREF(s.c_str(), &clr2);
        APPVERIFY(ok);

        m_colorMap[clr1] = clr2;
    }

    dark = CIniSettings::Instance().GetInt64(L"View", L"darktheme", 0) != 0;

    m_bLoaded = true;
}

void CTheme::RGBToHSB( COLORREF rgb, BYTE& hue, BYTE& saturation, BYTE& brightness )
{
    BYTE r = GetRValue(rgb);
    BYTE g = GetGValue(rgb);
    BYTE b = GetBValue(rgb);
    BYTE minRGB = min(min(r, g), b);
    BYTE maxRGB = max(max(r, g), b);
    BYTE delta = maxRGB - minRGB;
    double l = double(maxRGB);
    double s = 0.0;
    double h = 0.0;
    if (maxRGB)
        s = (255.0 * delta) / maxRGB;

    if (s != 0.0)
    {
        if (r == maxRGB)
            h = double(g - b) / delta;
        else if (g == maxRGB)
            h = 2.0 + double(b - r) / delta;
        else if (b == maxRGB)
            h = 4.0 + double(r - g) / delta;
    }
    else
        h = -1.0;
    h = h * 60.0;
    if (h < 0.0)
        h = h + 360.0;

    hue         = BYTE(h);
    saturation  = BYTE(s * 100.0 / 255.0);
    brightness  = BYTE(l * 100.0 / 255.0);
}

void CTheme::RGBtoHSL(COLORREF color, float& h, float& s, float& l)
{
    float r_percent = float(GetRValue(color)) / 255;
    float g_percent = float(GetGValue(color)) / 255;
    float b_percent = float(GetBValue(color)) / 255;

    float max_color = 0;
    if ((r_percent >= g_percent) && (r_percent >= b_percent))
        max_color = r_percent;
    else if ((g_percent >= r_percent) && (g_percent >= b_percent))
        max_color = g_percent;
    else if ((b_percent >= r_percent) && (b_percent >= g_percent))
        max_color = b_percent;

    float min_color = 0;
    if ((r_percent <= g_percent) && (r_percent <= b_percent))
        min_color = r_percent;
    else if ((g_percent <= r_percent) && (g_percent <= b_percent))
        min_color = g_percent;
    else if ((b_percent <= r_percent) && (b_percent <= g_percent))
        min_color = b_percent;

    float L = 0, S = 0, H = 0;

    L = (max_color + min_color)/2;

    if (max_color == min_color)
    {
        S = 0;
        H = 0;
    }
    else
    {
        auto d = max_color - min_color;
        if (L < .50)
            S = d / (max_color + min_color);
        else
            S = d / ((2.0f - max_color) - min_color);

        if (max_color == r_percent)
            H = (g_percent - b_percent) / d;

        else if (max_color == g_percent)
            H = 2.0f + (b_percent - r_percent) / d;

        else if (max_color == b_percent)
            H = 4.0f + (r_percent - g_percent) / d;
    }
    H = H*60;
    if (H < 0)
        H += 360;
    s = S*100;
    l = L*100;
    h = H;
}

static void HSLtoRGB_Subfunction(float& pc, float temp1, float temp2, float temp3)
{
    if ((temp3 * 6) < 1)
        pc = (temp2 + (temp1 - temp2)*6*temp3)*100;
    else if ((temp3 * 2) < 1)
        pc = temp1*100;
    else if ((temp3 * 3) < 2)
        pc = (temp2 + (temp1 - temp2)*(.66666f - temp3)*6)*100;
    else
        pc = temp2*100;
    return;
}

COLORREF CTheme::HSLtoRGB(float h, float s, float l)
{
    float pcr, pcg, pcb;

    if (s == 0)
    {
        BYTE t = BYTE(l/100*255);
        return RGB(t,t,t);
    }
    float L = l/100;
    float S = s/100;
    float H = h/360;
    float temp1 = (L < .50) ? L*(1 + S) : L + S - (L*S);
    float temp2 = 2*L - temp1;
    float temp3 = 0;
    temp3 = H + .33333f;
    if (temp3 > 1)
        temp3 -= 1;
    HSLtoRGB_Subfunction(pcr,temp1,temp2,temp3);
    temp3 = H;
    HSLtoRGB_Subfunction(pcg,temp1,temp2,temp3);
    temp3 = H - .33333f;
    if (temp3 < 0)
        temp3 += 1;
    HSLtoRGB_Subfunction(pcb,temp1,temp2,temp3);
    BYTE r = BYTE(pcr/100*255);
    BYTE g = BYTE(pcg/100*255);
    BYTE b = BYTE(pcb/100*255);
    return RGB(r,g,b);
}

COLORREF CTheme::GetThemeColor( COLORREF clr ) const
{
    if (dark)
    {
        auto cIt = m_colorMap.find(clr);
        if (cIt != m_colorMap.end())
            return cIt->second;

        float h, s, l;
        RGBtoHSL(clr, h, s, l);
        l = 100 - l;
        return HSLtoRGB(h, s, l);
    }

    return clr;
}

void CTheme::SetRibbonColors( COLORREF text, COLORREF background, COLORREF highlight )
{
    IPropertyStorePtr spPropertyStore;

    // g_pFramework is a pointer to the IUIFramework interface that is assigned
    // when the Ribbon is initialized.
    if (SUCCEEDED(g_pFramework->QueryInterface(&spPropertyStore)))
    {
        PROPVARIANT propvarBackground;
        PROPVARIANT propvarHighlight;
        PROPVARIANT propvarText;

        // UI_HSBCOLOR is a type defined in UIRibbon.h that is composed of
        // three component values: hue, saturation and brightness, respectively.
        BYTE h, s, b;
        RGBToHSB(text, h, s, b);
        UI_HSBCOLOR TextColor = UI_HSB(h, s, b);
        RGBToHSB(background, h, s, b);
        UI_HSBCOLOR BackgroundColor = UI_HSB(h, s, b);
        RGBToHSB(highlight, h, s, b);
        UI_HSBCOLOR HighlightColor = UI_HSB(h, s, b);

        InitPropVariantFromUInt32(BackgroundColor, &propvarBackground);
        InitPropVariantFromUInt32(HighlightColor, &propvarHighlight);
        InitPropVariantFromUInt32(TextColor, &propvarText);

        spPropertyStore->SetValue(UI_PKEY_GlobalBackgroundColor, propvarBackground);
        spPropertyStore->SetValue(UI_PKEY_GlobalHighlightColor, propvarHighlight);
        spPropertyStore->SetValue(UI_PKEY_GlobalTextColor, propvarText);

        spPropertyStore->Commit();
    }
}

void CTheme::SetRibbonColorsHSB( UI_HSBCOLOR text, UI_HSBCOLOR background, UI_HSBCOLOR highlight )
{
    IPropertyStorePtr spPropertyStore;

    APPVERIFY(g_pFramework != nullptr);
    // g_pFramework is a pointer to the IUIFramework interface that is assigned
    // when the Ribbon is initialized.
    HRESULT hr = g_pFramework->QueryInterface(&spPropertyStore);
    if (SUCCEEDED(hr))
    {
        PROPVARIANT propvarBackground;
        PROPVARIANT propvarHighlight;
        PROPVARIANT propvarText;

        InitPropVariantFromUInt32(background, &propvarBackground);
        InitPropVariantFromUInt32(highlight, &propvarHighlight);
        InitPropVariantFromUInt32(text, &propvarText);

        spPropertyStore->SetValue(UI_PKEY_GlobalBackgroundColor, propvarBackground);
        spPropertyStore->SetValue(UI_PKEY_GlobalHighlightColor, propvarHighlight);
        spPropertyStore->SetValue(UI_PKEY_GlobalTextColor, propvarText);

        spPropertyStore->Commit();
    }
}

void CTheme::GetRibbonColors( UI_HSBCOLOR& text, UI_HSBCOLOR& background, UI_HSBCOLOR& highlight ) const
{
    IPropertyStorePtr spPropertyStore;

    // g_pFramework is a pointer to the IUIFramework interface that is assigned
    // when the Ribbon is initialized.
    HRESULT hr = g_pFramework->QueryInterface(&spPropertyStore);
    if (SUCCEEDED(hr))
    {
        PROPVARIANT propvarBackground;
        PROPVARIANT propvarHighlight;
        PROPVARIANT propvarText;

        spPropertyStore->GetValue(UI_PKEY_GlobalBackgroundColor, &propvarBackground);
        spPropertyStore->GetValue(UI_PKEY_GlobalHighlightColor, &propvarHighlight);
        spPropertyStore->GetValue(UI_PKEY_GlobalTextColor, &propvarText);

        text = propvarText.uintVal;
        background = propvarBackground.uintVal;
        highlight = propvarHighlight.uintVal;
    }
}

void CTheme::SetDarkTheme(bool b /*= true*/)
{
    dark = b;
    CCommandHandler::Instance().OnThemeChanged(dark);
}

