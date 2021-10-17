// This file is part of BowPad.
//
// Copyright (C) 2013, 2015-2017, 2020-2021 - Stefan Kueng
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
#include "Scintilla.h"
#include "IniSettings.h"
#include "Theme.h"
#include "OnOutOfScope.h"

#include <Commdlg.h>

void CCmdPrint::Print(bool bShowDlg)
{
    PRINTDLGEX pDlg  = {0};
    pDlg.lStructSize = sizeof(PRINTDLGEX);
    pDlg.hwndOwner   = GetHwnd();
    pDlg.hInstance   = nullptr;
    pDlg.Flags       = PD_USEDEVMODECOPIESANDCOLLATE | PD_ALLPAGES | PD_RETURNDC | PD_NOCURRENTPAGE | PD_NOPAGENUMS;
    pDlg.nMinPage    = 1;
    pDlg.nMaxPage    = 0xffffU; // We do not know how many pages in the document
    pDlg.nCopies     = 1;
    pDlg.hDC         = nullptr;
    pDlg.nStartPage  = START_PAGE_GENERAL;

    // See if a range has been selected
    size_t startPos = Scintilla().SelectionStart();
    size_t endPos   = Scintilla().SelectionEnd();

    if (startPos == endPos)
        pDlg.Flags |= PD_NOSELECTION;
    else
        pDlg.Flags |= PD_SELECTION;

    if (!bShowDlg)
    {
        // Don't display dialog box, just use the default printer and options
        pDlg.Flags |= PD_RETURNDEFAULT;
    }
    HRESULT hResult = PrintDlgEx(&pDlg);
    if ((hResult != S_OK) || ((pDlg.dwResultAction != PD_RESULT_PRINT) && bShowDlg))
        return;

    // set the theme to normal
    bool dark = CTheme::Instance().IsDarkTheme();
    if (dark)
        CTheme::Instance().SetDarkTheme(false);
    OnOutOfScope(
        if (dark)
            CTheme::Instance()
                .SetDarkTheme(dark););

    // reset all indicators
    size_t length = Scintilla().Length();
    for (int i = INDIC_CONTAINER; i <= INDIC_MAX; ++i)
    {
        Scintilla().SetIndicatorCurrent(i);
        Scintilla().IndicatorClearRange(0, length);
    }
    // store and reset UI settings
    auto viewWs = Scintilla().ViewWS();
    Scintilla().SetViewWS(Scintilla::WhiteSpace::Invisible);
    auto edgeMode = Scintilla().EdgeMode();
    Scintilla().SetEdgeMode(Scintilla::EdgeVisualStyle::None);
    Scintilla().SetWrapVisualFlags(Scintilla::WrapVisualFlag::End);

    HDC hdc = pDlg.hDC;

    RECT  rectMargins, rectPhysMargins;
    POINT ptPage;
    POINT ptDpi;

    // Get printer resolution
    ptDpi.x = GetDeviceCaps(hdc, LOGPIXELSX); // dpi in X direction
    ptDpi.y = GetDeviceCaps(hdc, LOGPIXELSY); // dpi in Y direction

    // Start by getting the physical page size (in device units).
    ptPage.x = GetDeviceCaps(hdc, PHYSICALWIDTH);  // device units
    ptPage.y = GetDeviceCaps(hdc, PHYSICALHEIGHT); // device units

    // Get the dimensions of the unprintable
    // part of the page (in device units).
    rectPhysMargins.left = GetDeviceCaps(hdc, PHYSICALOFFSETX);
    rectPhysMargins.top  = GetDeviceCaps(hdc, PHYSICALOFFSETY);

    // To get the right and lower unprintable area,
    // we take the entire width and height of the paper and
    // subtract everything else.
    rectPhysMargins.right = ptPage.x                      // total paper width
                            - GetDeviceCaps(hdc, HORZRES) // printable width
                            - rectPhysMargins.left;       // left unprintable margin

    rectPhysMargins.bottom = ptPage.y                      // total paper height
                             - GetDeviceCaps(hdc, VERTRES) // printable height
                             - rectPhysMargins.top;        // right unprintable margin

    wchar_t localeInfo[3];
    GetLocaleInfo(LOCALE_USER_DEFAULT, LOCALE_IMEASURE, localeInfo, 3);
    // Metric system. '1' is US System
    int  defaultMargin = localeInfo[0] == '0' ? 2540 : 1000;
    RECT pagesetupMargin;
    pagesetupMargin.left   = static_cast<long>(CIniSettings::Instance().GetInt64(L"print", L"pagesetupmarginleft", defaultMargin));
    pagesetupMargin.top    = static_cast<long>(CIniSettings::Instance().GetInt64(L"print", L"pagesetupmargintop", defaultMargin));
    pagesetupMargin.right  = static_cast<long>(CIniSettings::Instance().GetInt64(L"print", L"pagesetupmarginright", defaultMargin));
    pagesetupMargin.bottom = static_cast<long>(CIniSettings::Instance().GetInt64(L"print", L"pagesetupmarginbottom", defaultMargin));

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
            rectSetup.left   = MulDiv(pagesetupMargin.left, ptDpi.x, 2540);
            rectSetup.top    = MulDiv(pagesetupMargin.top, ptDpi.y, 2540);
            rectSetup.right  = MulDiv(pagesetupMargin.right, ptDpi.x, 2540);
            rectSetup.bottom = MulDiv(pagesetupMargin.bottom, ptDpi.y, 2540);
        }
        else
        {
            rectSetup.left   = MulDiv(pagesetupMargin.left, ptDpi.x, 1000);
            rectSetup.top    = MulDiv(pagesetupMargin.top, ptDpi.y, 1000);
            rectSetup.right  = MulDiv(pagesetupMargin.right, ptDpi.x, 1000);
            rectSetup.bottom = MulDiv(pagesetupMargin.bottom, ptDpi.y, 1000);
        }

        // Don't reduce margins below the minimum printable area
        rectMargins.left   = max(rectPhysMargins.left, rectSetup.left);
        rectMargins.top    = max(rectPhysMargins.top, rectSetup.top);
        rectMargins.right  = max(rectPhysMargins.right, rectSetup.right);
        rectMargins.bottom = max(rectPhysMargins.bottom, rectSetup.bottom);
    }
    else
    {
        rectMargins.left   = rectPhysMargins.left;
        rectMargins.top    = rectPhysMargins.top;
        rectMargins.right  = rectPhysMargins.right;
        rectMargins.bottom = rectPhysMargins.bottom;
    }

    // rectMargins now contains the values used to shrink the printable
    // area of the page.

    // Convert device coordinates into logical coordinates
    DPtoLP(hdc, reinterpret_cast<LPPOINT>(&rectMargins), 2);
    DPtoLP(hdc, reinterpret_cast<LPPOINT>(&rectPhysMargins), 2);

    // Convert page size to logical units and we're done!
    DPtoLP(hdc, static_cast<LPPOINT>(&ptPage), 1);

    DOCINFO      di      = {sizeof(DOCINFO), nullptr, nullptr, nullptr, 0};
    std::wstring docName = GetCurrentTitle();
    di.lpszDocName       = docName.c_str();
    di.lpszOutput        = nullptr;
    di.lpszDatatype      = nullptr;
    di.fwType            = 0;
    if (::StartDoc(hdc, &di) < 0)
    {
        ::DeleteDC(hdc);
        return;
    }

    size_t lengthDoc     = Scintilla().Length();
    size_t lengthDocMax  = lengthDoc;
    size_t lengthPrinted = 0;

    // Requested to print selection
    if (pDlg.Flags & PD_SELECTION)
    {
        if (startPos > endPos)
        {
            lengthPrinted = endPos;
            lengthDoc     = startPos;
        }
        else
        {
            lengthPrinted = startPos;
            lengthDoc     = endPos;
        }

        if (lengthDoc > lengthDocMax)
            lengthDoc = lengthDocMax;
    }

    // We must subtract the physical margins from the printable area
    Sci_RangeToFormat frPrint;
    frPrint.hdc           = hdc;
    frPrint.hdcTarget     = hdc;
    frPrint.rc.left       = rectMargins.left - rectPhysMargins.left;
    frPrint.rc.top        = rectMargins.top - rectPhysMargins.top;
    frPrint.rc.right      = ptPage.x - rectMargins.right - rectPhysMargins.left;
    frPrint.rc.bottom     = ptPage.y - rectMargins.bottom - rectPhysMargins.top;
    frPrint.rcPage.left   = 0;
    frPrint.rcPage.top    = 0;
    frPrint.rcPage.right  = ptPage.x - rectPhysMargins.left - rectPhysMargins.right - 1;
    frPrint.rcPage.bottom = ptPage.y - rectPhysMargins.top - rectPhysMargins.bottom - 1;

    // Print each page
    while (lengthPrinted < lengthDoc)
    {
        ::StartPage(hdc);

        frPrint.chrg.cpMin = static_cast<Sci_PositionCR>(lengthPrinted);
        frPrint.chrg.cpMax = static_cast<Sci_PositionCR>(lengthDoc);

        lengthPrinted = Scintilla().FormatRange(true, &frPrint);

        ::EndPage(hdc);
    }

    Scintilla().FormatRange(FALSE, nullptr);

    ::EndDoc(hdc);
    ::DeleteDC(hdc);

    if (pDlg.hDevMode != nullptr)
        GlobalFree(pDlg.hDevMode);
    if (pDlg.hDevNames != nullptr)
        GlobalFree(pDlg.hDevNames);
    if (pDlg.lpPageRanges != nullptr)
        GlobalFree(pDlg.lpPageRanges);

    // reset the UI
    Scintilla().SetViewWS(viewWs);
    Scintilla().SetEdgeMode(edgeMode);
    Scintilla().SetWrapVisualFlags(Scintilla::WrapVisualFlag::None);
}

bool CCmdPageSetup::Execute()
{
    PAGESETUPDLG pDlg = {0};
    pDlg.lStructSize  = sizeof(PAGESETUPDLG);
    pDlg.hwndOwner    = GetHwnd();
    pDlg.hInstance    = nullptr;
    pDlg.Flags        = PSD_DEFAULTMINMARGINS | PSD_MARGINS | PSD_DISABLEPAPER | PSD_DISABLEORIENTATION;

    pDlg.rtMargin.left   = static_cast<long>(CIniSettings::Instance().GetInt64(L"print", L"pagesetupmarginleft", 2540));
    pDlg.rtMargin.top    = static_cast<long>(CIniSettings::Instance().GetInt64(L"print", L"pagesetupmargintop", 2540));
    pDlg.rtMargin.right  = static_cast<long>(CIniSettings::Instance().GetInt64(L"print", L"pagesetupmarginright", 2540));
    pDlg.rtMargin.bottom = static_cast<long>(CIniSettings::Instance().GetInt64(L"print", L"pagesetupmarginbottom", 2540));

    if (!PageSetupDlg(&pDlg))
        return false;

    CIniSettings::Instance().SetInt64(L"print", L"pagesetupmarginleft", pDlg.rtMargin.left);
    CIniSettings::Instance().SetInt64(L"print", L"pagesetupmargintop", pDlg.rtMargin.top);
    CIniSettings::Instance().SetInt64(L"print", L"pagesetupmarginright", pDlg.rtMargin.right);
    CIniSettings::Instance().SetInt64(L"print", L"pagesetupmarginbottom", pDlg.rtMargin.bottom);

    return true;
}
