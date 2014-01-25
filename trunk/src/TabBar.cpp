// This file is part of BowPad.
//
// Copyright (C) 2013-2014 - Stefan Kueng
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
#include "TabBar.h"
#include "resource.h"
#include "AppUtils.h"
#include "Theme.h"
#include "BowPad.h"
#include "BowPadUI.h"

#include <Uxtheme.h>
#include <vsstyle.h>
#include <UIRibbon.h>
#include <UIRibbonPropertyHelpers.h>


#pragma comment(lib, "UxTheme.lib")

extern IUIFramework * g_pFramework;

COLORREF Darker(COLORREF crBase, float fFactor)
{
    ASSERT (fFactor < 1.0f && fFactor > 0.0f);

    fFactor = min(fFactor, 1.0f);
    fFactor = max(fFactor, 0.0f);

    BYTE bRed, bBlue, bGreen;
    BYTE bRedShadow, bBlueShadow, bGreenShadow;

    bRed = GetRValue(crBase);
    bBlue = GetBValue(crBase);
    bGreen = GetGValue(crBase);

    bRedShadow = (BYTE)(bRed * fFactor);
    bBlueShadow = (BYTE)(bBlue * fFactor);
    bGreenShadow = (BYTE)(bGreen * fFactor);

    return RGB(bRedShadow, bGreenShadow, bBlueShadow);
}

COLORREF Lighter(COLORREF crBase, float fFactor)
{
    ASSERT (fFactor > 1.0f);

    fFactor = max(fFactor, 1.0f);

    BYTE bRed, bBlue, bGreen;
    BYTE bRedHilite, bBlueHilite, bGreenHilite;

    bRed = GetRValue(crBase);
    bBlue = GetBValue(crBase);
    bGreen = GetGValue(crBase);

    bRedHilite = (BYTE)min((int)(bRed * fFactor), 255);
    bBlueHilite = (BYTE)min((int)(bBlue * fFactor), 255);
    bGreenHilite = (BYTE)min((int)(bGreen * fFactor), 255);

    return RGB(bRedHilite, bGreenHilite, bBlueHilite);
}

void FillSolidRect(HDC hDC, int left, int top, int right, int bottom, COLORREF clr)
{
    ::SetBkColor(hDC, clr);
    RECT rect;
    rect.left = left;
    rect.top = top;
    rect.right = right;
    rect.bottom = bottom;
    ::ExtTextOut(hDC, 0, 0, ETO_OPAQUE, &rect, NULL, 0, NULL);
}


COLORREF CTabBar::m_activeTextColour = CTheme::Instance().GetThemeColor(::GetSysColor(COLOR_BTNTEXT));
COLORREF CTabBar::m_activeTopBarFocusedColour = CTheme::Instance().GetThemeColor(RGB(250, 170, 60));
COLORREF CTabBar::m_activeTopBarUnfocusedColour = CTheme::Instance().GetThemeColor(RGB(250, 210, 150));
COLORREF CTabBar::m_inactiveTextColour = CTheme::Instance().GetThemeColor(RGB(128, 128, 128));
COLORREF CTabBar::m_inactiveBgColour = CTheme::Instance().GetThemeColor(RGB(192, 192, 192));

HWND CTabBar::m_hwndArray[nbCtrlMax] = {NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL};
int CTabBar::m_nControls = 0;


CTabBar::~CTabBar()
{
    if (m_hFont)
        DeleteObject(m_hFont);
    Gdiplus::GdiplusShutdown(gdiplusToken);
}

bool CTabBar::Init(HINSTANCE /*hInst*/, HWND hParent)
{
    INITCOMMONCONTROLSEX icce;
    icce.dwSize = sizeof(icce);
    icce.dwICC = ICC_TAB_CLASSES;
    InitCommonControlsEx(&icce);

    CreateEx(WS_EX_COMPOSITED, WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | WS_VISIBLE | TCS_FOCUSNEVER | TCS_TOOLTIPS | TCS_TABS | TCS_OWNERDRAWFIXED, hParent, 0, WC_TABCONTROL);

    if (!*this)
    {
        return false;
    }

    if (!m_hwndArray[m_nControls])
    {
        m_hwndArray[m_nControls] = *this;
        m_ctrlID = m_nControls;
    }
    else
    {
        int i = 0;
        bool found = false;
        for ( ; i < nbCtrlMax && !found ; i++)
            if (!m_hwndArray[i])
                found = true;
        if (!found)
        {
            m_ctrlID = -1;
            return false;
        }
        m_hwndArray[i] = *this;
        m_ctrlID = i;
    }
    m_nControls++;

    ::SetWindowLongPtr(*this, GWLP_USERDATA, (LONG_PTR)this);
    m_TabBarDefaultProc = reinterpret_cast<WNDPROC>(::SetWindowLongPtr(*this, GWLP_WNDPROC, (LONG_PTR)TabBar_Proc));

    m_hFont = (HFONT)::SendMessage(*this, WM_GETFONT, 0, 0);

    if (m_hFont == NULL)
        m_hFont = (HFONT)::GetStockObject(DEFAULT_GUI_FONT);

    DoOwnerDrawTab();
    return true;
}

LRESULT CALLBACK CTabBar::WinMsgHandler( HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam )
{
    if (prevWndProc)
        return CallWindowProc(prevWndProc, hwnd, uMsg, wParam, lParam);
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

int CTabBar::InsertAtEnd(const TCHAR *subTabName)
{
    TCITEM tie;
    tie.mask = TCIF_TEXT | TCIF_IMAGE | TCIF_PARAM;
    int index = -1;

    if (m_bHasImgList)
        index = 0;
    tie.iImage = index;
    tie.pszText = (TCHAR *)subTabName;
    tie.lParam = m_tabID++;
    return int(::SendMessage(*this, TCM_INSERTITEM, m_nItems++, reinterpret_cast<LPARAM>(&tie)));
}

void CTabBar::GetTitle(int index, TCHAR *title, int titleLen)
{
    TCITEM tci;
    tci.mask = TCIF_TEXT;
    tci.pszText = title;
    tci.cchTextMax = titleLen - 1;
    ::SendMessage(*this, TCM_GETITEM, index, reinterpret_cast<LPARAM>(&tci));
}

void CTabBar::GetCurrentTitle(TCHAR *title, int titleLen)
{
    TCITEM tci;
    tci.mask = TCIF_TEXT;
    tci.pszText = title;
    tci.cchTextMax = titleLen-1;
    ::SendMessage(*this, TCM_GETITEM, GetCurrentTabIndex(), reinterpret_cast<LPARAM>(&tci));
}

void CTabBar::SetCurrentTitle(LPCTSTR title)
{
    TCITEM tci;
    tci.mask = TCIF_TEXT;
    tci.pszText = const_cast<TCHAR*>(title);
    ::SendMessage(*this, TCM_SETITEM, GetCurrentTabIndex(), reinterpret_cast<LPARAM>(&tci));
}

void CTabBar::SetFont( TCHAR *fontName, int fontSize )
{
    if (m_hFont)
        ::DeleteObject(m_hFont);

    m_hFont = ::CreateFont( (int)fontSize, 0,
        0,
        0,
        FW_NORMAL,
        0, 0, 0, 0,
        0, 0, 0, 0,
        fontName);
    if (m_hFont)
        ::SendMessage(*this, WM_SETFONT, reinterpret_cast<WPARAM>(m_hFont), 0);
}

void CTabBar::ActivateAt(int index) const
{
    InvalidateRect(*this, NULL, TRUE);
    if (GetCurrentTabIndex() != index)
    {
        TBHDR nmhdr;
        nmhdr.hdr.hwndFrom = *this;
        nmhdr.hdr.code = TCN_SELCHANGING;
        nmhdr.hdr.idFrom = reinterpret_cast<unsigned int>(this);
        nmhdr.tabOrigin = GetCurrentTabIndex();
        ::SendMessage(m_hParent, WM_NOTIFY, 0, reinterpret_cast<LPARAM>(&nmhdr));
        if (index >= 0)
            ::SendMessage(*this, TCM_SETCURSEL, index, 0);
    }
    TBHDR nmhdr;
    nmhdr.hdr.hwndFrom = *this;
    nmhdr.hdr.code = TCN_SELCHANGE;
    nmhdr.hdr.idFrom = reinterpret_cast<unsigned int>(this);
    nmhdr.tabOrigin = index;
    ::SendMessage(m_hParent, WM_NOTIFY, 0, reinterpret_cast<LPARAM>(&nmhdr));
}

void CTabBar::DeletItemAt( int index )
{
    if ((index == m_nItems-1))
    {
        // prevent invisible tabs. If last visible tab is removed, other tabs are put in view but not redrawn
        // Therefore, scroll one tab to the left if only one tab visible
        if (m_nItems > 1)
        {
            RECT itemRect;
            ::SendMessage(*this, TCM_GETITEMRECT, (WPARAM)index, (LPARAM)&itemRect);
            if (itemRect.left < 5) // if last visible tab, scroll left once (no more than 5px away should be safe, usually 2px depending on the drawing)
            {
                // To scroll the tab control to the left, use the WM_HSCROLL notification
                int wParam = MAKEWPARAM(SB_THUMBPOSITION, index - 1);
                ::SendMessage(*this, WM_HSCROLL, wParam, 0);

                wParam = MAKEWPARAM(SB_ENDSCROLL, index - 1);
                ::SendMessage(*this, WM_HSCROLL, wParam, 0);
            }
        }
    }
    ::SendMessage(*this, TCM_DELETEITEM, index, 0);
    m_nItems--;
}

int CTabBar::GetCurrentTabIndex() const
{
    return (int)::SendMessage(*this, TCM_GETCURSEL, 0, 0);
}

int CTabBar::GetCurrentTabId() const
{
    int index = (int)::SendMessage(*this, TCM_GETCURSEL, 0, 0);
    if (index < 0)
    {
        index = (int)::SendMessage(*this, TCM_GETCURFOCUS, 0, 0);
        if (index < 0)
            return -1;
    }
    TCITEM tci;
    tci.mask = TCIF_PARAM;
    ::SendMessage(*this, TCM_GETITEM, index, reinterpret_cast<LPARAM>(&tci));
    return (int)tci.lParam;
}

void CTabBar::DeletAllItems()
{
    ::SendMessage(*this, TCM_DELETEALLITEMS, 0, 0);
    m_nItems = 0;
}

void CTabBar::SetImageList( HIMAGELIST himl )
{
    m_bHasImgList = true;
    ::SendMessage(*this, TCM_SETIMAGELIST, 0, (LPARAM)himl);
}

void CTabBar::DoOwnerDrawTab()
{
    int pad = GetSystemMetrics(SM_CXSMICON)/2;
    ::SendMessage(m_hwndArray[0], TCM_SETPADDING, 0, MAKELPARAM(pad, 0));
    for (int i = 0 ; i < m_nControls ; i++)
    {
        if (m_hwndArray[i])
        {
            ::InvalidateRect(m_hwndArray[i], NULL, TRUE);
            ::SendMessage(m_hwndArray[i], TCM_SETPADDING, 0, MAKELPARAM(pad, 0));
        }
    }
}

void CTabBar::SetColour(COLORREF colour2Set, tabColourIndex i)
{
    switch (i)
    {
    case activeText:
        m_activeTextColour = colour2Set;
        break;
    case activeFocusedTop:
        m_activeTopBarFocusedColour = colour2Set;
        break;
    case activeUnfocusedTop:
        m_activeTopBarUnfocusedColour = colour2Set;
        break;
    case inactiveText:
        m_inactiveTextColour = colour2Set;
        break;
    case inactiveBg :
        m_inactiveBgColour = colour2Set;
        break;
    default :
        return;
    }
    DoOwnerDrawTab();
}


LRESULT CTabBar::RunProc(HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam)
{
    switch (Message)
    {
    case WM_ERASEBKGND:
        {
            HDC hDC = (HDC)wParam;
            RECT rClient, rTab, rTotalTab, rBkgnd, rEdge;
            COLORREF crBack;
            int nTab, nTabHeight = 0;

            ::CallWindowProc(m_TabBarDefaultProc, hwnd, Message, wParam, lParam);

            // calc total tab width
            GetClientRect(*this, &rClient);
            nTab = GetItemCount();
            SetRectEmpty(&rTotalTab);

            while (nTab--)
            {
                ::SendMessage(*this, TCM_GETITEMRECT, nTab, (LPARAM)&rTab);
                UnionRect(&rTotalTab, &rTab, &rTotalTab);
            }

            nTabHeight = rTotalTab.bottom-rTotalTab.top;

            // add a bit
            InflateRect(&rTotalTab, 2, 3);
            rEdge = rTotalTab;

            // then if background color is set, paint the visible background
            // area of the tabs in the bkgnd color
            // note: the mfc code for drawing the tabs makes all sorts of assumptions
            // about the background color of the tab control being the same as the page
            // color - in some places the background color shows thru' the pages!!
            // so we must only paint the background color where we need to, which is that
            // portion of the tab area not excluded by the tabs themselves
            crBack = CTheme::Instance().GetThemeColor(::GetSysColor(COLOR_3DFACE));

            // full width of tab ctrl above top of tabs
            rBkgnd = rClient;
            rBkgnd.bottom = rTotalTab.top + 3;
            SetBkColor(hDC, crBack);
            ExtTextOut(hDC, rBkgnd.left, rBkgnd.top, ETO_CLIPPED | ETO_OPAQUE, &rBkgnd, L"", 0, NULL);

            // width of tab ctrl visible bkgnd including bottom pixel of tabs to left of tabs
            rBkgnd = rClient;
            rBkgnd.right = 2;
            rBkgnd.bottom = rBkgnd.top + (nTabHeight + 2);
            ExtTextOut(hDC, rBkgnd.left, rBkgnd.top, ETO_CLIPPED | ETO_OPAQUE, &rBkgnd, L"", 0, NULL);

            // to right of tabs
            rBkgnd = rClient;
            rBkgnd.left += (rTotalTab.right-(max(rTotalTab.left, 0))) - 2;
            rBkgnd.bottom = rBkgnd.top + (nTabHeight + 2);
            ExtTextOut(hDC, rBkgnd.left, rBkgnd.top, ETO_CLIPPED | ETO_OPAQUE, &rBkgnd, L"", 0, NULL);

            return TRUE;
        }
        break;
    case WM_PAINT:
        {
            PAINTSTRUCT ps = {0};
            HDC hDC = BeginPaint(*this, &ps);

            // prepare dc
            HGDIOBJ hOldFont = SelectObject(hDC, m_hFont);

            DRAWITEMSTRUCT dis;
            dis.CtlType = ODT_TAB;
            dis.CtlID = 0;
            dis.hwndItem = *this;
            dis.hDC = hDC;
            dis.itemAction = ODA_DRAWENTIRE;

            // draw the rest of the border
            RECT rPage;
            GetClientRect(*this, &dis.rcItem);
            rPage = dis.rcItem;
            SendMessage(*this, TCM_ADJUSTRECT, FALSE, (LPARAM)&rPage);
            dis.rcItem.top = rPage.top - 2;

            DrawMainBorder(&dis);

            // paint the tabs first and then the borders
            int nTab = GetItemCount();
            int nSel = (int)::SendMessage(*this, TCM_GETCURSEL, 0, 0);

            if (!nTab) // no pages added
            {
                SelectObject(hDC, hOldFont);
                EndPaint(*this, &ps);
                return 0;
            }

            while (nTab--)
            {
                if (nTab != nSel)
                {
                    dis.itemID = nTab;
                    dis.itemState = 0;

                    ::SendMessage(*this, TCM_GETITEMRECT, nTab, (LPARAM)&dis.rcItem);

                    dis.rcItem.bottom -= 2;
                    DrawItem(&dis);
                    DrawItemBorder(&dis);
                }
            }

            // now selected tab
            dis.itemID = nSel;
            dis.itemState = ODS_SELECTED;

            ::SendMessage(*this, TCM_GETITEMRECT, nSel, (LPARAM)&dis.rcItem);

            dis.rcItem.bottom += 2;
            dis.rcItem.top -= 2;
            DrawItem(&dis);
            DrawItemBorder(&dis);

            SelectObject(hDC, hOldFont);
            EndPaint(*this, &ps);
        }
        break;
    case WM_LBUTTONDOWN :
        {
            int xPos = LOWORD(lParam);
            int yPos = HIWORD(lParam);

            if (m_closeButtonZone.IsHit(xPos, yPos, m_currentHoverTabRect))
            {
                m_whichCloseClickDown = GetTabIndexAt(xPos, yPos);
                TBHDR nmhdr;
                nmhdr.hdr.hwndFrom = *this;
                nmhdr.hdr.code = TCN_REFRESH;
                nmhdr.hdr.idFrom = reinterpret_cast<unsigned int>(this);
                nmhdr.tabOrigin = 0;
                ::SendMessage(m_hParent, WM_NOTIFY, 0, reinterpret_cast<LPARAM>(&nmhdr));
                return TRUE;
            }

            ::CallWindowProc(m_TabBarDefaultProc, hwnd, Message, wParam, lParam);
            int currentTabOn = (int)::SendMessage(*this, TCM_GETCURSEL, 0, 0);

            if (wParam == 2)
                return TRUE;

            m_nSrcTab = m_nTabDragged = currentTabOn;

            POINT point;
            point.x = LOWORD(lParam);
            point.y = HIWORD(lParam);
            ::ClientToScreen(hwnd, &point);
            if(::DragDetect(hwnd, point))
            {
                // Yes, we're beginning to drag, so capture the mouse...
                m_bIsDragging = true;
                ::SetCapture(hwnd);
            }

            TBHDR nmhdr;
            nmhdr.hdr.hwndFrom = *this;
            nmhdr.hdr.code = NM_CLICK;
            nmhdr.hdr.idFrom = reinterpret_cast<unsigned int>(this);
            nmhdr.tabOrigin = currentTabOn;

            ::SendMessage(m_hParent, WM_NOTIFY, 0, reinterpret_cast<LPARAM>(&nmhdr));
            return TRUE;
        }
        break;
    case WM_RBUTTONDOWN :   //rightclick selects tab aswell
        {
            ::CallWindowProc(m_TabBarDefaultProc, hwnd, WM_LBUTTONDOWN, wParam, lParam);
            return TRUE;
        }
        break;
    case WM_MOUSEMOVE :
        {
            if (m_bIsDragging)
            {
                POINT p;
                p.x = LOWORD(lParam);
                p.y = HIWORD(lParam);
                ExchangeItemData(p);

                // Get cursor position of "Screen"
                // For using the function "WindowFromPoint" afterward!!!
                ::GetCursorPos(&m_draggingPoint);

                DraggingCursor(m_draggingPoint);
                return TRUE;
            }

            int xPos = LOWORD(lParam);
            int yPos = HIWORD(lParam);

            int index = GetTabIndexAt(xPos, yPos);

            if (index != -1)
            {
                // Reduce flicker by only redrawing needed tabs

                bool oldVal = m_bIsCloseHover;
                int oldIndex = m_currentHoverTabItem;
                RECT oldRect;

                ::SendMessage(*this, TCM_GETITEMRECT, index, (LPARAM)&m_currentHoverTabRect);
                ::SendMessage(*this, TCM_GETITEMRECT, oldIndex, (LPARAM)&oldRect);
                m_currentHoverTabItem = index;
                m_bIsCloseHover = m_closeButtonZone.IsHit(xPos, yPos, m_currentHoverTabRect);

                if (oldVal != m_bIsCloseHover)
                {
                    InvalidateRect(hwnd, &oldRect, FALSE);
                    InvalidateRect(hwnd, &m_currentHoverTabRect, FALSE);
                }
            }
        }
        break;
    case WM_LBUTTONUP :
        {
            int xPos = LOWORD(lParam);
            int yPos = HIWORD(lParam);
            int currentTabOn = GetTabIndexAt(xPos, yPos);
            if (m_bIsDragging)
            {
                if(::GetCapture() == *this)
                    ::ReleaseCapture();

                // Send a notification message to the parent with wParam = 0, lParam = 0
                // nmhdr.idFrom = this
                // destIndex = this->_nSrcTab
                // scrIndex  = this->_nTabDragged
                TBHDR nmhdr;
                nmhdr.hdr.hwndFrom = *this;
                nmhdr.hdr.code = m_bIsDraggingInside?TCN_TABDROPPED:TCN_TABDROPPEDOUTSIDE;
                nmhdr.hdr.idFrom = reinterpret_cast<unsigned int>(this);
                nmhdr.tabOrigin = m_nTabDragged;
                ::SendMessage(m_hParent, WM_NOTIFY, 0, reinterpret_cast<LPARAM>(&nmhdr));
                return TRUE;
            }

            if ((m_whichCloseClickDown == currentTabOn) && m_closeButtonZone.IsHit(xPos, yPos, m_currentHoverTabRect))
            {
                TBHDR nmhdr;
                nmhdr.hdr.hwndFrom = *this;
                nmhdr.hdr.code = TCN_TABDELETE;
                nmhdr.hdr.idFrom = reinterpret_cast<unsigned int>(this);
                nmhdr.tabOrigin = currentTabOn;
                ::SendMessage(m_hParent, WM_NOTIFY, 0, reinterpret_cast<LPARAM>(&nmhdr));

                m_whichCloseClickDown = -1;
                return TRUE;
            }
            m_whichCloseClickDown = -1;
        }
        break;
    case WM_CAPTURECHANGED :
        {
            if (m_bIsDragging)
            {
                m_bIsDragging = false;
                return TRUE;
            }
        }
        break;
    case WM_DRAWITEM :
        {
            DrawItem((DRAWITEMSTRUCT *)lParam);
            return TRUE;
        }
        break;
    case WM_KEYDOWN :
        {
            if (wParam == VK_LCONTROL)
                ::SetCursor(::LoadCursor(hResource, MAKEINTRESOURCE(IDC_DRAG_PLUS_TAB)));
            return TRUE;
        }
        break;
    case WM_MBUTTONUP:
        {
            int xPos = LOWORD(lParam);
            int yPos = HIWORD(lParam);
            int currentTabOn = GetTabIndexAt(xPos, yPos);
            TBHDR nmhdr;
            nmhdr.hdr.hwndFrom = *this;
            nmhdr.hdr.code = TCN_TABDELETE;
            nmhdr.hdr.idFrom = reinterpret_cast<unsigned int>(this);
            nmhdr.tabOrigin = currentTabOn;
            ::SendMessage(m_hParent, WM_NOTIFY, 0, reinterpret_cast<LPARAM>(&nmhdr));

            return TRUE;
        }
        break;
    case WM_CONTEXTMENU:
        {
            POINT pt;
            POINTSTOPOINT(pt, lParam);

            if (pt.x == -1 && pt.y == -1)
            {
                HRESULT hr = E_FAIL;

                // Display the menu in the upper-left corner of the client area, below the ribbon.
                IUIRibbon * pRibbon;
                hr = g_pFramework->GetView(0, IID_PPV_ARGS(&pRibbon));
                if (SUCCEEDED(hr))
                {
                    UINT32 uRibbonHeight = 0;
                    hr = pRibbon->GetHeight(&uRibbonHeight);
                    if (SUCCEEDED(hr))
                    {
                        pt.x = 0;
                        pt.y = uRibbonHeight;
                        ClientToScreen(hwnd, &pt);
                    }
                    pRibbon->Release();
                }
                if (FAILED(hr))
                {
                    // Default to just the upper-right corner of the entire screen.
                    pt.x = 0;
                    pt.y = 0;
                }
            }

            HRESULT hr = E_FAIL;

            // The basic pattern of how to show Contextual UI in a specified location.
            IUIContextualUI * pContextualUI = NULL;
            if (SUCCEEDED(g_pFramework->GetView(cmdTabContextMap, IID_PPV_ARGS(&pContextualUI))))
            {
                hr = pContextualUI->ShowAtLocation(pt.x, pt.y);
                pContextualUI->Release();
            }
        }
        break;
    }
    return ::CallWindowProc(m_TabBarDefaultProc, hwnd, Message, wParam, lParam);
}

COLORREF CTabBar::GetTabColor(bool bSelected, UINT item)
{
    TBHDR nmhdr;
    nmhdr.hdr.hwndFrom = *this;
    nmhdr.hdr.code = TCN_GETCOLOR;
    nmhdr.hdr.idFrom = reinterpret_cast<unsigned int>(this);
    nmhdr.tabOrigin = item;
    COLORREF clr = (COLORREF)::SendMessage(m_hParent, WM_NOTIFY, 0, reinterpret_cast<LPARAM>(&nmhdr));
    float lighterfactor = 1.1f;
    if (clr == 0)
    {
        clr = CTheme::Instance().GetThemeColor(GetSysColor(COLOR_3DFACE));
        lighterfactor = 1.4f;
    }
    if (bSelected)
    {
        return Lighter(clr, lighterfactor);
    }
    return Darker(clr, 0.9f);
}

void CTabBar::DrawItemBorder(LPDRAWITEMSTRUCT lpdis)
{
    bool bSelected = (lpdis->itemID == (UINT)::SendMessage(*this, TCM_GETCURSEL, 0, 0));

    RECT rItem(lpdis->rcItem);

    COLORREF crTab = GetTabColor(bSelected, lpdis->itemID);
    COLORREF crHighlight = Lighter(crTab, 1.5f);
    COLORREF crShadow = Darker(crTab, 0.75f);

    rItem.bottom += bSelected ? -1 : 1;

    // edges

    FillSolidRect(lpdis->hDC, rItem.left, rItem.top, rItem.left + 1, rItem.bottom, crHighlight);
    FillSolidRect(lpdis->hDC, rItem.left, rItem.top, rItem.right, rItem.top + 1, crHighlight);
    FillSolidRect(lpdis->hDC, rItem.right - 1, rItem.top, rItem.right, rItem.bottom, crShadow);
}

void CTabBar::DrawMainBorder(LPDRAWITEMSTRUCT lpdis)
{
    RECT rBorder(lpdis->rcItem);

    COLORREF crTab = GetTabColor(false, lpdis->itemID);
    COLORREF crHighlight = Lighter(crTab, 1.5f);
    COLORREF crShadow = Darker(crTab, 0.75f);

    LONG x = rBorder.left;
    LONG y = rBorder.top;
    LONG cx = rBorder.right-rBorder.left;
    LONG cy = rBorder.bottom-rBorder.top;

    FillSolidRect(lpdis->hDC, x,        y,      x + cx - 1, y + 1,      crHighlight);
    FillSolidRect(lpdis->hDC, x,        y,      x + 1,      y + cy - 1, crHighlight);
    FillSolidRect(lpdis->hDC, x + cx,   y,      x - 1,      y + cy,     crShadow);
    FillSolidRect(lpdis->hDC, x,        y + cy, x + cx,     y - 1,      crShadow);
}

void CTabBar::DrawItem(LPDRAWITEMSTRUCT pDrawItemStruct)
{
    TC_ITEM tci;
    HIMAGELIST hilTabs = (HIMAGELIST)TabCtrl_GetImageList(*this);

    bool bSelected = (pDrawItemStruct->itemID == (UINT)::SendMessage(*this, TCM_GETCURSEL, 0, 0));

    RECT rItem(pDrawItemStruct->rcItem);

    if (bSelected)
        rItem.bottom -= 1;
    else
        rItem.bottom += 2;

    // tab
    // blend from back color to COLOR_3DFACE if 16 bit mode or better
    COLORREF crFrom = GetTabColor(bSelected, pDrawItemStruct->itemID);

    COLORREF crTo = bSelected ? ::GetSysColor(COLOR_3DFACE) : Darker(::GetSysColor(COLOR_3DFACE), 0.7f);
    crTo = CTheme::Instance().GetThemeColor(crTo);

    int nROrg = GetRValue(crFrom);
    int nGOrg = GetGValue(crFrom);
    int nBOrg = GetBValue(crFrom);
    int nRDiff = GetRValue(crTo) - nROrg;
    int nGDiff = GetGValue(crTo) - nGOrg;
    int nBDiff = GetBValue(crTo) - nBOrg;

    int nHeight = rItem.bottom-rItem.top;

    for (int nLine = 0; nLine < nHeight; nLine += 2)
    {
        int nRed = nROrg + (nLine * nRDiff) / nHeight;
        int nGreen = nGOrg + (nLine * nGDiff) / nHeight;
        int nBlue = nBOrg + (nLine * nBDiff) / nHeight;

        FillSolidRect(pDrawItemStruct->hDC, rItem.left, rItem.top + nLine, rItem.right, rItem.top + nLine + 2, RGB(nRed, nGreen, nBlue));
    }
    wchar_t buf[100] = { 0 };
    tci.mask = TCIF_TEXT | TCIF_IMAGE;
    tci.pszText = buf;
    tci.cchTextMax = 99;
    ::SendMessage(*this, TCM_GETITEM, pDrawItemStruct->itemID, reinterpret_cast<LPARAM>(&tci));
    if (bSelected)
    {
        // draw a line at the bottom indicating the active tab:
        // green if the tab is not modified, red if it is modified and needs saving
        FillSolidRect(pDrawItemStruct->hDC, rItem.left, rItem.bottom - 5, rItem.right, rItem.bottom,
                      CTheme::Instance().GetThemeColor(tci.iImage == UNSAVED_IMG_INDEX ? RGB(200, 0, 0) : RGB(0, 200, 0)));
    }
    if (tci.iImage == UNSAVED_IMG_INDEX)
        wcscat_s(buf, L"*");

    const int PADDING = 2;
    // text & icon
    rItem.left += PADDING;
    rItem.top += PADDING + (bSelected ? 1 : 0);

    SetBkMode(pDrawItemStruct->hDC, TRANSPARENT);

    // draw close button
    RECT closeButtonRect = m_closeButtonZone.GetButtonRectFrom(pDrawItemStruct->rcItem);
    if (bSelected)
        closeButtonRect.left -= 2;
    // 3 status for each inactive tab and selected tab close item :
    // normal / hover / pushed
    int idCloseImg = IDR_CLOSETAB;
    if (m_bIsCloseHover && (m_currentHoverTabItem == (int)pDrawItemStruct->itemID) && (m_whichCloseClickDown == -1)) // hover
        idCloseImg = IDR_CLOSETAB_HOVER;
    else if (m_bIsCloseHover && (m_currentHoverTabItem == (int)pDrawItemStruct->itemID) && (m_whichCloseClickDown == m_currentHoverTabItem)) // pushed
        idCloseImg = IDR_CLOSETAB_PUSH;
    else
        idCloseImg = bSelected?IDR_CLOSETAB:IDR_CLOSETAB_INACT;
    HDC hdcMemory;
    hdcMemory = ::CreateCompatibleDC(pDrawItemStruct->hDC);
    HBITMAP hBmp = ::LoadBitmap(hResource, MAKEINTRESOURCE(idCloseImg));
    BITMAP bmp;
    ::GetObject(hBmp, sizeof(bmp), &bmp);
    rItem.right = closeButtonRect.left;
    ::SelectObject(hdcMemory, hBmp);
    ::BitBlt(pDrawItemStruct->hDC, closeButtonRect.left, closeButtonRect.top, bmp.bmWidth, bmp.bmHeight, hdcMemory, 0, 0, SRCCOPY);
    ::DeleteDC(hdcMemory);
    ::DeleteObject(hBmp);

    // icon
    if (hilTabs)
    {
        ImageList_Draw(hilTabs, tci.iImage, pDrawItemStruct->hDC, rItem.left, rItem.top, ILD_TRANSPARENT);
        rItem.left += 16 + PADDING;
    }

    // text
    rItem.right -= PADDING;
    UINT uFlags = DT_CALCRECT | DT_SINGLELINE | DT_MODIFYSTRING | DT_END_ELLIPSIS;
    ::DrawText(pDrawItemStruct->hDC, buf, -1, &rItem, uFlags);

    SetTextColor(pDrawItemStruct->hDC, bSelected ? CTheme::Instance().GetThemeColor(::GetSysColor(COLOR_WINDOWTEXT)) : CTheme::Instance().GetThemeColor(Darker(::GetSysColor(COLOR_3DFACE), 0.5f)));
    DrawText(pDrawItemStruct->hDC, buf, -1, &rItem, DT_NOPREFIX | DT_CENTER);
}


void CTabBar::DraggingCursor(POINT screenPoint)
{
    HWND hWin = ::WindowFromPoint(screenPoint);
    if (*this == hWin)
        ::SetCursor(::LoadCursor(NULL, IDC_ARROW));
    else
    {
        TCHAR className[256];
        ::GetClassName(hWin, className, 256);
        if ((!lstrcmp(className, TEXT("Scintilla"))) || (!lstrcmp(className, WC_TABCONTROL)))
        {
            if (::GetKeyState(VK_LCONTROL) & 0x80000000)
                ::SetCursor(::LoadCursor(hResource, MAKEINTRESOURCE(IDC_DRAG_PLUS_TAB)));
            else
                ::SetCursor(::LoadCursor(hResource, MAKEINTRESOURCE(IDC_DRAG_TAB)));
        }
        else if (IsPointInParentZone(screenPoint))
            ::SetCursor(::LoadCursor(hResource, MAKEINTRESOURCE(IDC_DRAG_INTERDIT_TAB)));
        else // drag out of application
            ::SetCursor(::LoadCursor(hResource, MAKEINTRESOURCE(IDC_DRAG_OUT_TAB)));
    }
}

void CTabBar::ExchangeItemData(POINT point)
{
    // Find the destination tab...
    int nTab = GetTabIndexAt(point);

    // The position is over a tab.
    //if (hitinfo.flags != TCHT_NOWHERE)
    if (nTab != -1)
    {
        m_bIsDraggingInside = true;

        if (nTab != m_nTabDragged)
        {
            //1. set to focus
            ::SendMessage(*this, TCM_SETCURSEL, nTab, 0);

            //2. shift their data, and insert the source
            TCITEM itemData_nDraggedTab, itemData_shift;
            itemData_nDraggedTab.mask = itemData_shift.mask = TCIF_IMAGE | TCIF_TEXT | TCIF_PARAM;
            const int stringSize = 256;
            TCHAR str1[stringSize];
            TCHAR str2[stringSize];

            itemData_nDraggedTab.pszText = str1;
            itemData_nDraggedTab.cchTextMax = (stringSize);

            itemData_shift.pszText = str2;
            itemData_shift.cchTextMax = (stringSize);

            ::SendMessage(*this, TCM_GETITEM, m_nTabDragged, reinterpret_cast<LPARAM>(&itemData_nDraggedTab));

            if (m_nTabDragged > nTab)
            {
                for (int i = m_nTabDragged ; i > nTab ; i--)
                {
                    ::SendMessage(*this, TCM_GETITEM, i-1, reinterpret_cast<LPARAM>(&itemData_shift));
                    ::SendMessage(*this, TCM_SETITEM, i, reinterpret_cast<LPARAM>(&itemData_shift));
                }
            }
            else
            {
                for (int i = m_nTabDragged ; i < nTab ; i++)
                {
                    ::SendMessage(*this, TCM_GETITEM, i+1, reinterpret_cast<LPARAM>(&itemData_shift));
                    ::SendMessage(*this, TCM_SETITEM, i, reinterpret_cast<LPARAM>(&itemData_shift));
                }
            }
            ::SendMessage(*this, TCM_SETITEM, nTab, reinterpret_cast<LPARAM>(&itemData_nDraggedTab));

            //3. update the current index
            m_nTabDragged = nTab;

            TBHDR nmhdr;
            nmhdr.hdr.hwndFrom = *this;
            nmhdr.hdr.code = TCN_ORDERCHANGED;
            nmhdr.hdr.idFrom = reinterpret_cast<unsigned int>(this);
            nmhdr.tabOrigin = nTab;
            ::SendMessage(m_hParent, WM_NOTIFY, 0, reinterpret_cast<LPARAM>(&nmhdr));
        }
    }
    else
    {
        m_bIsDraggingInside = false;
    }

}

int CTabBar::GetTabIndexAt( int x, int y ) const
{
    TCHITTESTINFO hitInfo;
    hitInfo.pt.x = x;
    hitInfo.pt.y = y;
    return (int)::SendMessage(*this, TCM_HITTEST, 0, (LPARAM)&hitInfo);
}

bool CTabBar::IsPointInParentZone( POINT screenPoint ) const
{
    RECT parentZone;
    ::GetWindowRect(m_hParent, &parentZone);
    return (((screenPoint.x >= parentZone.left) && (screenPoint.x <= parentZone.right)) &&
        (screenPoint.y >= parentZone.top) && (screenPoint.y <= parentZone.bottom));
}

LRESULT CALLBACK CTabBar::TabBar_Proc( HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam )
{
    return (((CTabBar *)(::GetWindowLongPtr(hwnd, GWLP_USERDATA)))->RunProc(hwnd, Message, wParam, lParam));
}

int CTabBar::GetIDFromIndex( int index )
{
    TCITEM tci;
    tci.mask = TCIF_PARAM;
    ::SendMessage(*this, TCM_GETITEM, index, reinterpret_cast<LPARAM>(&tci));
    return (int)tci.lParam;
}

int CTabBar::GetIndexFromID( int id )
{
    for (int i = 0; i < m_nItems; ++i)
    {
        TCITEM tci;
        tci.mask = TCIF_PARAM;
        ::SendMessage(*this, TCM_GETITEM, i, reinterpret_cast<LPARAM>(&tci));
        if (id == tci.lParam)
            return i;
    }
    return (-1);
}

bool CloseButtonZone::IsHit(int x, int y, const RECT & testZone) const
{
    if (((x + m_width + m_fromRight) < testZone.right) || (x > (testZone.right - m_fromRight)))
        return false;

    if (((y - m_height - m_fromTop) > testZone.top) || (y < (testZone.top + m_fromTop)))
        return false;

    return true;
}

RECT CloseButtonZone::GetButtonRectFrom(const RECT & tabItemRect) const
{
    RECT rect;
    rect.right = tabItemRect.right - m_fromRight;
    rect.left = rect.right - m_width;
    rect.top = tabItemRect.top + m_fromTop;
    rect.bottom = rect.top + m_height;

    return rect;
}
