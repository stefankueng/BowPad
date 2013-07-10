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
#include "stdafx.h"
#include "Theme.h"
#include "BowPad.h"
#include "SysInfo.h"

extern IUIFramework *g_pFramework;  // Reference to the Ribbon framework.

CTheme::CTheme(void)
    : m_bLoaded(false)
{
}

CTheme::~CTheme(void)
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
    m_bLoaded = true;

    CSimpleIni themeIni;

    HRSRC hRes = FindResource(NULL, MAKEINTRESOURCE(IDR_DARKTHEME), L"config");
    if (hRes)
    {
        HGLOBAL hResourceLoaded = LoadResource(NULL, hRes);
        if (hResourceLoaded)
        {
            char * lpResLock = (char *) LockResource(hResourceLoaded);
            DWORD dwSizeRes = SizeofResource(NULL, hRes);
            if (lpResLock)
            {
                themeIni.LoadFile(lpResLock, dwSizeRes);
            }
        }
    }


    CSimpleIni::TNamesDepend colors;
    themeIni.GetAllKeys(L"SubstColors", colors);

    wchar_t * endptr;
    unsigned long hexval;
    for (const auto& it : colors)
    {
        hexval = wcstol(it, &endptr, 16);
        COLORREF col1 = (RGB((hexval >> 16) & 0xFF, (hexval >> 8) & 0xFF, hexval & 0xFF)) | (hexval & 0xFF000000);

        std::wstring s = themeIni.GetValue(L"SubstColors", it);
        hexval = wcstol(s.c_str(), &endptr, 16);
        COLORREF col2 = (RGB((hexval >> 16) & 0xFF, (hexval >> 8) & 0xFF, hexval & 0xFF)) | (hexval & 0xFF000000);

        m_colorMap[col1] = col2;
    }

    m_bLoaded = true;
}

void CTheme::RGBToHSB( COLORREF rgb, BYTE& hue, BYTE& saturation, BYTE& brightness )
{
    double minRGB = min(min(GetRValue(rgb), GetGValue(rgb)), GetBValue(rgb));
    double maxRGB = max(max(GetRValue(rgb), GetGValue(rgb)), GetBValue(rgb));
    double delta = maxRGB - minRGB;
    double b = maxRGB;
    double s = 0.0;
    double h = 0.0;
    if (maxRGB)
        s = 255 * delta / maxRGB;

    if (s != 0.0)
    {
        if (GetRValue(rgb) == maxRGB)
            h = (double(GetGValue(rgb)) - double(GetBValue(rgb))) / delta;
        else
        {
            if (GetGValue(rgb) == maxRGB)
                h = 2.0 + (double(GetBValue(rgb)) - double(GetRValue(rgb))) / delta;
            else if (GetBValue(rgb) == maxRGB)
                h = 4.0 + (double(GetRValue(rgb)) - double(GetGValue(rgb))) / delta;
        }
    }
    else
        h = -1.0;
    h = h * 60.0;
    if (h < 0.0)
        h = h + 360.0;

    hue         = BYTE(h);
    saturation  = BYTE(s * 100.0 / 255.0);
    brightness  = BYTE(b * 100.0 / 255.0);
}

void CTheme::RGBtoHSL(COLORREF color, unsigned int& h, unsigned int& s, unsigned int& l)
{
    UINT r = GetRValue(color);
    UINT g = GetGValue(color);
    UINT b = GetBValue(color);

    float r_percent = ((float)r)/255;
    float g_percent = ((float)g)/255;
    float b_percent = ((float)b)/255;

    float max_color = 0;
    if((r_percent >= g_percent) && (r_percent >= b_percent))
    {
        max_color = r_percent;
    }
    if((g_percent >= r_percent) && (g_percent >= b_percent))
        max_color = g_percent;
    if((b_percent >= r_percent) && (b_percent >= g_percent))
        max_color = b_percent;

    float min_color = 0;
    if((r_percent <= g_percent) && (r_percent <= b_percent))
        min_color = r_percent;
    if((g_percent <= r_percent) && (g_percent <= b_percent))
        min_color = g_percent;
    if((b_percent <= r_percent) && (b_percent <= g_percent))
        min_color = b_percent;

    float L = 0;
    float S = 0;
    float H = 0;

    L = (max_color + min_color)/2;

    if(max_color == min_color)
    {
        S = 0;
        H = 0;
    }
    else
    {
        if(L < .50)
        {
            S = (max_color - min_color)/(max_color + min_color);
        }
        else
        {
            S = (max_color - min_color)/(2 - max_color - min_color);
        }
        if(max_color == r_percent)
        {
            H = (g_percent - b_percent)/(max_color - min_color);
        }
        if(max_color == g_percent)
        {
            H = 2 + (b_percent - r_percent)/(max_color - min_color);
        }
        if(max_color == b_percent)
        {
            H = 4 + (r_percent - g_percent)/(max_color - min_color);
        }
    }
    s = (unsigned int)(S*100);
    l = (unsigned int)(L*100);
    H = H*60;
    if(H < 0)
        H += 360;
    h = (unsigned int)H;
}

static void HSLtoRGB_Subfunction(unsigned int& c, const float& temp1, const float& temp2, const float& temp3)
{
    if((temp3 * 6) < 1)
        c = (unsigned int)((temp2 + (temp1 - temp2)*6*temp3)*100);
    else
        if((temp3 * 2) < 1)
            c = (unsigned int)(temp1*100);
        else
            if((temp3 * 3) < 2)
                c = (unsigned int)((temp2 + (temp1 - temp2)*(.66666 - temp3)*6)*100);
            else
                c = (unsigned int)(temp2*100);
    return;
}

COLORREF CTheme::HSLtoRGB(const unsigned int& h, const unsigned int& s, const unsigned int& l)
{
    unsigned int r = 0;
    unsigned int g = 0;
    unsigned int b = 0;

    float L = ((float)l)/100;
    float S = ((float)s)/100;
    float H = ((float)h)/360;

    if(s == 0)
    {
        r = l;
        g = l;
        b = l;
    }
    else
    {
        float temp1 = 0;
        if(L < .50)
        {
            temp1 = L*(1 + S);
        }
        else
        {
            temp1 = L + S - (L*S);
        }

        float temp2 = 2*L - temp1;

        float temp3 = 0;
        for(int i = 0 ; i < 3 ; i++)
        {
            switch(i)
            {
            case 0: // red
                {
                    temp3 = H + .33333f;
                    if(temp3 > 1)
                        temp3 -= 1;
                    HSLtoRGB_Subfunction(r,temp1,temp2,temp3);
                    break;
                }
            case 1: // green
                {
                    temp3 = H;
                    HSLtoRGB_Subfunction(g,temp1,temp2,temp3);
                    break;
                }
            case 2: // blue
                {
                    temp3 = H - .33333f;
                    if(temp3 < 0)
                        temp3 += 1;
                    HSLtoRGB_Subfunction(b,temp1,temp2,temp3);
                    break;
                }
            default:
                {

                }
            }
        }
    }
    r = (unsigned int)((((float)r)/100)*255);
    g = (unsigned int)((((float)g)/100)*255);
    b = (unsigned int)((((float)b)/100)*255);
    return RGB(r,g,b);
}

COLORREF CTheme::GetThemeColor( COLORREF clr )
{
    if (dark)
    {
        auto cIt = m_colorMap.find(clr);
        if (cIt != m_colorMap.end())
            return cIt->second;

        unsigned int h;
        unsigned int s;
        unsigned int l;
        RGBtoHSL(clr, h, s, l);
        l = 100 - l;
        return HSLtoRGB(h, s, l);
    }

    return clr;
}

void CTheme::SetRibbonColors( COLORREF text, COLORREF background, COLORREF highlight )
{
    CComPtr<IPropertyStore> spPropertyStore;

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
    CComPtr<IPropertyStore> spPropertyStore;

    // g_pFramework is a pointer to the IUIFramework interface that is assigned 
    // when the Ribbon is initialized.
    if (SUCCEEDED(g_pFramework->QueryInterface(&spPropertyStore)))
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

void CTheme::GetRibbonColors( UI_HSBCOLOR& text, UI_HSBCOLOR& background, UI_HSBCOLOR& highlight )
{
    CComPtr<IPropertyStore> spPropertyStore;

    // g_pFramework is a pointer to the IUIFramework interface that is assigned 
    // when the Ribbon is initialized.
    if (SUCCEEDED(g_pFramework->QueryInterface(&spPropertyStore)))
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

