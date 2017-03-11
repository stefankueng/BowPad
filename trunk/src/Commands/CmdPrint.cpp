// This file is part of BowPad.
//
// Copyright (C) 2013, 2015-2017 - Stefan Kueng
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
#include "CmdPrint.h"
#include "UnicodeUtils.h"
#include "Scintilla.h"
#include "IniSettings.h"
#include "Theme.h"
#include "OnOutOfScope.h"

#include <Commdlg.h>

void CCmdPrint::Print( bool bShowDlg )
{
    PRINTDLGEX pdlg = {0};
    pdlg.lStructSize = sizeof(PRINTDLGEX);
    pdlg.hwndOwner = GetHwnd();
    pdlg.hInstance = nullptr;
    pdlg.Flags = PD_USEDEVMODECOPIESANDCOLLATE | PD_ALLPAGES | PD_RETURNDC | PD_NOCURRENTPAGE | PD_NOPAGENUMS;
    pdlg.nMinPage = 1;
    pdlg.nMaxPage = 0xffffU; // We do not know how many pages in the document
    pdlg.nCopies = 1;
    pdlg.hDC = 0;
    pdlg.nStartPage = START_PAGE_GENERAL;

    // See if a range has been selected
    size_t startPos = ScintillaCall(SCI_GETSELECTIONSTART);
    size_t endPos = ScintillaCall(SCI_GETSELECTIONEND);

    if (startPos == endPos)
        pdlg.Flags |= PD_NOSELECTION;
    else
        pdlg.Flags |= PD_SELECTION;

    if (!bShowDlg)
    {
        // Don't display dialog box, just use the default printer and options
        pdlg.Flags |= PD_RETURNDEFAULT;
    }
    HRESULT hResult = PrintDlgEx(&pdlg);
    if ((hResult != S_OK) || ((pdlg.dwResultAction != PD_RESULT_PRINT) && bShowDlg))
        return;

    // set the theme to normal
    bool dark = CTheme::Instance().IsDarkTheme();
    if (dark)
        CTheme::Instance().SetDarkTheme(false);
    OnOutOfScope(
        if (dark)
            CTheme::Instance().SetDarkTheme(dark);
        );

    // reset all indicators
    size_t endpos = ScintillaCall(SCI_GETLENGTH);
    for (int i = INDIC_CONTAINER; i <= INDIC_MAX; ++i)
    {
        ScintillaCall(SCI_SETINDICATORCURRENT, i);
        ScintillaCall(SCI_INDICATORCLEARRANGE, 0, endpos);
    }
    // store and reset UI settings
    int viewws = (int)ScintillaCall(SCI_GETVIEWWS);
    ScintillaCall(SCI_SETVIEWWS, 0);
    int edgemode = (int)ScintillaCall(SCI_GETEDGEMODE);
    ScintillaCall(SCI_SETEDGEMODE, EDGE_NONE);
    ScintillaCall(SCI_SETWRAPVISUALFLAGS, SC_WRAPVISUALFLAG_END);

    HDC hdc = pdlg.hDC;

    RECT rectMargins, rectPhysMargins;
    POINT ptPage;
    POINT ptDpi;

    // Get printer resolution
    ptDpi.x = GetDeviceCaps(hdc, LOGPIXELSX);    // dpi in X direction
    ptDpi.y = GetDeviceCaps(hdc, LOGPIXELSY);    // dpi in Y direction

    // Start by getting the physical page size (in device units).
    ptPage.x = GetDeviceCaps(hdc, PHYSICALWIDTH);   // device units
    ptPage.y = GetDeviceCaps(hdc, PHYSICALHEIGHT);  // device units

    // Get the dimensions of the unprintable
    // part of the page (in device units).
    rectPhysMargins.left = GetDeviceCaps(hdc, PHYSICALOFFSETX);
    rectPhysMargins.top = GetDeviceCaps(hdc, PHYSICALOFFSETY);

    // To get the right and lower unprintable area,
    // we take the entire width and height of the paper and
    // subtract everything else.
    rectPhysMargins.right = ptPage.x                        // total paper width
        - GetDeviceCaps(hdc, HORZRES)                       // printable width
        - rectPhysMargins.left;                             // left unprintable margin

    rectPhysMargins.bottom = ptPage.y                       // total paper height
        - GetDeviceCaps(hdc, VERTRES)                       // printable height
        - rectPhysMargins.top;                              // right unprintable margin

    TCHAR localeInfo[3];
    GetLocaleInfo(LOCALE_USER_DEFAULT, LOCALE_IMEASURE, localeInfo, 3);
    // Metric system. '1' is US System
    int defaultMargin = localeInfo[0] == '0' ? 2540 : 1000;
    RECT pagesetupMargin;
    pagesetupMargin.left   = (long)CIniSettings::Instance().GetInt64(L"print", L"pagesetupmarginleft", defaultMargin);
    pagesetupMargin.top    = (long)CIniSettings::Instance().GetInt64(L"print", L"pagesetupmargintop", defaultMargin);
    pagesetupMargin.right  = (long)CIniSettings::Instance().GetInt64(L"print", L"pagesetupmarginright", defaultMargin);
    pagesetupMargin.bottom = (long)CIniSettings::Instance().GetInt64(L"print", L"pagesetupmarginbottom", defaultMargin);

    if (pagesetupMargin.left != 0 || pagesetupMargin.right != 0 ||
        pagesetupMargin.top != 0 || pagesetupMargin.bottom != 0)
    {
            RECT rectSetup;

            // Convert the hundredths of millimeters (HiMetric) or
            // thousandths of inches (HiEnglish) margin values
            // from the Page Setup dialog to device units.
            // (There are 2540 hundredths of a mm in an inch.)

            if (localeInfo[0] == '0')
            {
                // Metric system. '1' is US System
                rectSetup.left      = MulDiv (pagesetupMargin.left, ptDpi.x, 2540);
                rectSetup.top       = MulDiv (pagesetupMargin.top, ptDpi.y, 2540);
                rectSetup.right     = MulDiv(pagesetupMargin.right, ptDpi.x, 2540);
                rectSetup.bottom    = MulDiv(pagesetupMargin.bottom, ptDpi.y, 2540);
            }
            else
            {
                rectSetup.left      = MulDiv(pagesetupMargin.left, ptDpi.x, 1000);
                rectSetup.top       = MulDiv(pagesetupMargin.top, ptDpi.y, 1000);
                rectSetup.right     = MulDiv(pagesetupMargin.right, ptDpi.x, 1000);
                rectSetup.bottom    = MulDiv(pagesetupMargin.bottom, ptDpi.y, 1000);
            }

            // Don't reduce margins below the minimum printable area
            rectMargins.left    = max(rectPhysMargins.left, rectSetup.left);
            rectMargins.top     = max(rectPhysMargins.top, rectSetup.top);
            rectMargins.right   = max(rectPhysMargins.right, rectSetup.right);
            rectMargins.bottom  = max(rectPhysMargins.bottom, rectSetup.bottom);
    }
    else
    {
        rectMargins.left    = rectPhysMargins.left;
        rectMargins.top     = rectPhysMargins.top;
        rectMargins.right   = rectPhysMargins.right;
        rectMargins.bottom  = rectPhysMargins.bottom;
    }

    // rectMargins now contains the values used to shrink the printable
    // area of the page.

    // Convert device coordinates into logical coordinates
    DPtoLP(hdc, (LPPOINT) &rectMargins, 2);
    DPtoLP(hdc, (LPPOINT)&rectPhysMargins, 2);

    // Convert page size to logical units and we're done!
    DPtoLP(hdc, (LPPOINT) &ptPage, 1);


    DOCINFO di = {sizeof(DOCINFO), 0, 0, 0, 0};
    std::wstring docname = GetCurrentTitle();
    di.lpszDocName = docname.c_str();
    di.lpszOutput = 0;
    di.lpszDatatype = 0;
    di.fwType = 0;
    if (::StartDoc(hdc, &di) < 0)
    {
        ::DeleteDC(hdc);
        return;
    }

    size_t lengthDoc = ScintillaCall(SCI_GETLENGTH);
    size_t lengthDocMax = lengthDoc;
    size_t lengthPrinted = 0;

    // Requested to print selection
    if (pdlg.Flags & PD_SELECTION)
    {
        if (startPos > endPos)
        {
            lengthPrinted = endPos;
            lengthDoc = startPos;
        }
        else
        {
            lengthPrinted = startPos;
            lengthDoc = endPos;
        }

        if (lengthDoc > lengthDocMax)
            lengthDoc = lengthDocMax;
    }

    // We must subtract the physical margins from the printable area
    Sci_RangeToFormat frPrint;
    frPrint.hdc             = hdc;
    frPrint.hdcTarget       = hdc;
    frPrint.rc.left         = rectMargins.left - rectPhysMargins.left;
    frPrint.rc.top          = rectMargins.top - rectPhysMargins.top;
    frPrint.rc.right        = ptPage.x - rectMargins.right - rectPhysMargins.left;
    frPrint.rc.bottom       = ptPage.y - rectMargins.bottom - rectPhysMargins.top;
    frPrint.rcPage.left     = 0;
    frPrint.rcPage.top      = 0;
    frPrint.rcPage.right    = ptPage.x - rectPhysMargins.left - rectPhysMargins.right - 1;
    frPrint.rcPage.bottom   = ptPage.y - rectPhysMargins.top - rectPhysMargins.bottom - 1;

    // Print each page
    while (lengthPrinted < lengthDoc)
    {
        ::StartPage(hdc);

        frPrint.chrg.cpMin = (long)lengthPrinted;
        frPrint.chrg.cpMax = (long)lengthDoc;

        lengthPrinted = ScintillaCall(SCI_FORMATRANGE, true, reinterpret_cast<LPARAM>(&frPrint));

        ::EndPage(hdc);
    }

    ScintillaCall(SCI_FORMATRANGE, FALSE, 0);

    ::EndDoc(hdc);
    ::DeleteDC(hdc);

    if (pdlg.hDevMode != nullptr)
        GlobalFree(pdlg.hDevMode);
    if (pdlg.hDevNames != nullptr)
        GlobalFree(pdlg.hDevNames);
    if (pdlg.lpPageRanges != nullptr)
        GlobalFree(pdlg.lpPageRanges);

    // reset the UI
    ScintillaCall(SCI_SETVIEWWS, viewws);
    ScintillaCall(SCI_SETEDGEMODE, edgemode);
    ScintillaCall(SCI_SETWRAPVISUALFLAGS, SC_WRAPVISUALFLAG_NONE);
}


bool CCmdPageSetup::Execute()
{
    PAGESETUPDLG pdlg = {0};
    pdlg.lStructSize = sizeof(PAGESETUPDLG);
    pdlg.hwndOwner = GetHwnd();
    pdlg.hInstance = nullptr;
    pdlg.Flags = PSD_DEFAULTMINMARGINS|PSD_MARGINS|PSD_DISABLEPAPER|PSD_DISABLEORIENTATION;

    pdlg.rtMargin.left   = (long)CIniSettings::Instance().GetInt64(L"print", L"pagesetupmarginleft", 2540);
    pdlg.rtMargin.top    = (long)CIniSettings::Instance().GetInt64(L"print", L"pagesetupmargintop", 2540);
    pdlg.rtMargin.right  = (long)CIniSettings::Instance().GetInt64(L"print", L"pagesetupmarginright", 2540);
    pdlg.rtMargin.bottom = (long)CIniSettings::Instance().GetInt64(L"print", L"pagesetupmarginbottom", 2540);

    if (!PageSetupDlg(&pdlg))
        return false;

    CIniSettings::Instance().SetInt64(L"print", L"pagesetupmarginleft", pdlg.rtMargin.left);
    CIniSettings::Instance().SetInt64(L"print", L"pagesetupmargintop", pdlg.rtMargin.top);
    CIniSettings::Instance().SetInt64(L"print", L"pagesetupmarginright", pdlg.rtMargin.right);
    CIniSettings::Instance().SetInt64(L"print", L"pagesetupmarginbottom", pdlg.rtMargin.bottom);

    return true;
}
