// This file is part of BowPad.
//
// Copyright (C) 2013-2015 - Stefan Kueng
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

#pragma warning(push)
#pragma warning(disable: 4458) // declaration of 'xxx' hides class member
#include <gdiplus.h>
#pragma warning(pop)
#include <Uxtheme.h>
#include <vsstyle.h>
#include <UIRibbon.h>
#include <UIRibbonPropertyHelpers.h>

#pragma comment(lib, "UxTheme.lib")

extern IUIFramework * g_pFramework;

const int TABBAR_SHOWDISKICON = 0;

static COLORREF Darker(COLORREF crBase, float fFactor)
{
    ASSERT(fFactor < 1.0f && fFactor > 0.0f);

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

static COLORREF Lighter(COLORREF crBase, float fFactor)
{
    ASSERT(fFactor > 1.0f);

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

static void FillSolidRect(HDC hDC, int left, int top, int right, int bottom, COLORREF clr)
{
    ::SetBkColor(hDC, clr);
    RECT rect;
    rect.left = left;
    rect.top = top;
    rect.right = right;
    rect.bottom = bottom;
    ::ExtTextOut(hDC, 0, 0, ETO_OPAQUE, &rect, nullptr, 0, nullptr);
}

COLORREF CTabBar::m_activeTextColour = CTheme::Instance().GetThemeColor(::GetSysColor(COLOR_BTNTEXT));
COLORREF CTabBar::m_activeTopBarFocusedColour = CTheme::Instance().GetThemeColor(RGB(250, 170, 60));
COLORREF CTabBar::m_activeTopBarUnfocusedColour = CTheme::Instance().GetThemeColor(RGB(250, 210, 150));
COLORREF CTabBar::m_inactiveTextColour = CTheme::Instance().GetThemeColor(RGB(128, 128, 128));
COLORREF CTabBar::m_inactiveBgColour = CTheme::Instance().GetThemeColor(RGB(192, 192, 192));

HWND CTabBar::m_hwndArray[nbCtrlMax] = { nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr };
int CTabBar::m_nControls = 0;


CTabBar::CTabBar(HINSTANCE hInst)
        : CWindow(hInst)
        , m_nItems(0)
        , m_bHasImgList(false)
        , m_hFont(nullptr)
        , m_ctrlID(-1)
        , m_bIsDragging(false)
        , m_bIsDraggingInside(false)
        , m_nSrcTab(-1)
        , m_nTabDragged(-1)
        , m_TabBarDefaultProc(nullptr)
        , m_currentHoverTabItem(-1)
        , m_bIsCloseHover(false)
        , m_whichCloseClickDown(-1)
        , m_lmbdHit(false)
        , m_tabID(0)
{
    Gdiplus::GdiplusStartupInput gdiplusStartupInput;
    Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, nullptr);
    m_draggingPoint = {};
    m_currentHoverTabRect = {};
};

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
        for (; i < nbCtrlMax && !found; i++)
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

    if (m_hFont == nullptr)
    {
        m_hFont = (HFONT)::GetStockObject(DEFAULT_GUI_FONT);
        ::SendMessage(*this, WM_SETFONT, reinterpret_cast<WPARAM>(m_hFont), 0);
    }

    TabCtrl_SetMinTabWidth(*this, LPARAM(GetSystemMetrics(SM_CXSMICON) * m_dpiScale * 4.0));
    m_closeButtonZone.SetDPIScale(m_dpiScale);

    DoOwnerDrawTab();
    return true;
}

LRESULT CALLBACK CTabBar::WinMsgHandler(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
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
    tie.iImage = TABBAR_SHOWDISKICON ? index : 0;
    tie.pszText = (TCHAR *)subTabName;
    tie.lParam = m_tabID++;
    return TabCtrl_InsertItem(*this, m_nItems++, &tie);
}

int CTabBar::InsertAfter(int index, const TCHAR *subTabName)
{
    TCITEM tie;
    tie.mask = TCIF_TEXT | TCIF_IMAGE | TCIF_PARAM;
    int imgindex = -1;

    if (m_bHasImgList)
        imgindex = 0;
    tie.iImage = TABBAR_SHOWDISKICON ? imgindex : 0;
    tie.pszText = (TCHAR *)subTabName;
    tie.lParam = m_tabID++;
    if ((index + 1) >= m_nItems)
        index = m_nItems - 1;
    int ret = TabCtrl_InsertItem(*this, index + 1, &tie);
    ++m_nItems;
    return ret;
}

void CTabBar::GetTitle(int index, TCHAR *title, int titleLen) const
{
    TCITEM tci;
    tci.mask = TCIF_TEXT;
    tci.pszText = title;
    tci.cchTextMax = titleLen - 1;
    TabCtrl_GetItem(*this,index,&tci);
}

std::wstring CTabBar::GetTitle(int index) const
{
    wchar_t buf[100];
    const auto bufCount = _countof(buf);

    TCITEM tci;
    tci.mask = TCIF_TEXT;
    tci.pszText = buf;
    tci.cchTextMax = bufCount - 1;
    auto ret = TabCtrl_GetItem(*this, index, &tci);
    // In case buffer fills completely, ensure last character is terminated.
    buf[bufCount-1] = L'\0';
    // Documentation suggests GetItem might not use our buffer
    // or might null it out on error.
    return (ret && tci.pszText != nullptr) ? buf : L"";
}

void CTabBar::GetCurrentTitle(TCHAR *title, int titleLen) const
{
    GetTitle(GetCurrentTabIndex(), title, titleLen);
}

std::wstring CTabBar::GetCurrentTitle() const
{
    return GetTitle(GetCurrentTabIndex());
}

void CTabBar::SetCurrentTitle(LPCTSTR title)
{
    TCITEM tci;
    tci.mask = TCIF_TEXT;
    tci.pszText = const_cast<TCHAR*>(title);
    TabCtrl_SetItem(*this, GetCurrentTabIndex(), &tci);
}

void CTabBar::SetFont(const TCHAR *fontName, int fontSize)
{
    if (m_hFont)
        ::DeleteObject(m_hFont);

    m_hFont = ::CreateFont((int)fontSize, 0,
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
    InvalidateRect(*this, nullptr, TRUE);
    NMHDR nmhdr = {};
    nmhdr.hwndFrom = *this;
    nmhdr.code = TCN_SELCHANGING;
    ::SendMessage(m_hParent, WM_NOTIFY, 0, reinterpret_cast<LPARAM>(&nmhdr));
    if (index >= 0)
        TabCtrl_SetCurSel(*this, index);

    nmhdr.code = TCN_SELCHANGE;
    ::SendMessage(m_hParent, WM_NOTIFY, 0, reinterpret_cast<LPARAM>(&nmhdr));
}

void CTabBar::DeleteItemAt(int index)
{
    if (index == m_nItems - 1)
    {
        // prevent invisible tabs. If last visible tab is removed, other tabs are put in view but not redrawn
        // Therefore, scroll one tab to the left if only one tab visible
        if (m_nItems > 1)
        {
            RECT itemRect;
            bool got = TabCtrl_GetItemRect(*this, index, &itemRect) != FALSE;
            APPVERIFY(got);
            if (got)
            {
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
    }
    // Decrement the item count before calling delete as this delete
    // seems to initiate events (like paint) that will assume the extra
    // item is present if it's not already accounted as gone before then,
    // even though it is gone, but the delete itself is initiating the actions.
    // Failure to decrement the count first will show up as assertion
    // failures in calls to GetItemRect arond line 320 & 530 in the
    // paint logic.
    m_nItems--;
    bool deleted = TabCtrl_DeleteItem(*this, index) != FALSE;
    APPVERIFY(deleted);
    if (m_currentHoverTabItem == index)
        m_currentHoverTabItem = -1;
}

int CTabBar::GetCurrentTabIndex() const
{
    return TabCtrl_GetCurSel(*this);
}

int CTabBar::GetCurrentTabId() const
{
    int index = TabCtrl_GetCurSel(*this);
    if (index < 0)
    {
        index = TabCtrl_GetCurFocus(*this);
        if (index < 0)
            return -1;
    }
    TCITEM tci;
    tci.mask = TCIF_PARAM;
    BOOL result = TabCtrl_GetItem(*this, index, &tci);
    if (!result)
    {
        assert(false);
        return -1;
    }
    return (int)tci.lParam;
}

void CTabBar::SetCurrentTabId(int id)
{
    int index = TabCtrl_GetCurSel(*this);
    if (index < 0)
    {
        index = TabCtrl_GetCurFocus(*this);
        if (index < 0)
            return;
    }
    TCITEM tci;
    tci.mask = TCIF_PARAM;
    tci.lParam = id;
    TabCtrl_SetItem(*this, index, &tci);
}

void CTabBar::DeletAllItems()
{
    TabCtrl_DeleteAllItems(*this);
    m_nItems = 0;
}

void CTabBar::SetImageList(HIMAGELIST himl)
{
    m_bHasImgList = true;
    TabCtrl_SetImageList(*this, himl);
}

void CTabBar::DoOwnerDrawTab()
{
    int padx = int(GetSystemMetrics(SM_CXSMICON) / 2 * m_dpiScale);
    int pady = int(3.0 * m_dpiScale);
    TabCtrl_SetPadding(m_hwndArray[0], padx, pady);
    for (int i = 0; i < m_nControls; i++)
    {
        if (m_hwndArray[i])
        {
            ::InvalidateRect(m_hwndArray[i], nullptr, TRUE);
            TabCtrl_SetPadding(m_hwndArray[i], padx, pady);
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
        case inactiveBg:
            m_inactiveBgColour = colour2Set;
            break;
        default:
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

            while (nTab > 0)
            {
                --nTab;
                bool ok = TabCtrl_GetItemRect(*this, nTab, &rTab) != FALSE;
                APPVERIFY(ok);
                UnionRect(&rTotalTab, &rTab, &rTotalTab);
            }

            nTabHeight = rTotalTab.bottom - rTotalTab.top;

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
            ExtTextOut(hDC, rBkgnd.left, rBkgnd.top, ETO_CLIPPED | ETO_OPAQUE, &rBkgnd, L"", 0, nullptr);

            // width of tab ctrl visible bkgnd including bottom pixel of tabs to left of tabs
            rBkgnd = rClient;
            rBkgnd.right = 2;
            rBkgnd.bottom = rBkgnd.top + (nTabHeight + 2);
            ExtTextOut(hDC, rBkgnd.left, rBkgnd.top, ETO_CLIPPED | ETO_OPAQUE, &rBkgnd, L"", 0, nullptr);

            // to right of tabs
            rBkgnd = rClient;
            rBkgnd.left += (rTotalTab.right - (max(rTotalTab.left, 0))) - 2;
            rBkgnd.bottom = rBkgnd.top + (nTabHeight + 2);
            ExtTextOut(hDC, rBkgnd.left, rBkgnd.top, ETO_CLIPPED | ETO_OPAQUE, &rBkgnd, L"", 0, nullptr);

            return TRUE;
        }
            break;
        case WM_PAINT:
        {
            PAINTSTRUCT ps; // ps is a totally out parameter, no init needed.
            HDC hDC = BeginPaint(*this, &ps);

            // prepare dc
            HGDIOBJ hOldFont = SelectObject(hDC, m_hFont);

            DRAWITEMSTRUCT dis = { 0 };
            dis.CtlType = ODT_TAB;
            dis.CtlID = 0;
            dis.hwndItem = *this;
            dis.hDC = hDC;
            dis.itemAction = ODA_DRAWENTIRE;

            // draw the rest of the border
            RECT rPage;
            GetClientRect(*this, &dis.rcItem);
            rPage = dis.rcItem;
            TabCtrl_AdjustRect(*this, FALSE, &rPage);
            dis.rcItem.top = rPage.top - 2;

            DrawMainBorder(&dis);

            // paint the tabs first and then the borders
            int count = GetItemCount();
            APPVERIFY(count>=0);
            int nTab = count;

            if (!nTab) // no pages added
            {
                SelectObject(hDC, hOldFont);
                EndPaint(*this, &ps);
                return 0;
            }

            int nSel = TabCtrl_GetCurSel(*this);

            do
            {
                --nTab;
                if (nTab != nSel)
                {
                    APPVERIFY(nTab>=0 && nTab<count);
                    dis.itemID = nTab;
                    dis.itemState = 0;

                    bool got = TabCtrl_GetItemRect(*this, nTab, &dis.rcItem) != FALSE;
                    APPVERIFY(got);
                    if (got)
                    {
                        dis.rcItem.bottom -= 2;
                        DrawItem(&dis);
                        DrawItemBorder(&dis);
                    }
                }
            } while (nTab > 0);

            if (nSel != -1)
            {
                APPVERIFY(nSel>=0 && nSel<count);
                // now selected tab
                dis.itemID = nSel;
                dis.itemState = ODS_SELECTED;

                bool got = TabCtrl_GetItemRect(*this, nSel, &dis.rcItem) != FALSE;
                APPVERIFY(got);
                if (got)
                {
                    dis.rcItem.bottom += 2;
                    dis.rcItem.top -= 2;
                    DrawItem(&dis);
                    DrawItemBorder(&dis);
                }
            }

            SelectObject(hDC, hOldFont);
            EndPaint(*this, &ps);
        }
            break;
        case WM_LBUTTONDOWN:
        {
            int xPos = GET_X_LPARAM(lParam);
            int yPos = GET_Y_LPARAM(lParam);

            if (m_closeButtonZone.IsHit(xPos, yPos, m_currentHoverTabRect))
            {
                m_whichCloseClickDown = GetTabIndexAt(xPos, yPos);
                TBHDR nmhdr;
                nmhdr.hdr.hwndFrom = *this;
                nmhdr.hdr.code = TCN_REFRESH;
                nmhdr.hdr.idFrom = reinterpret_cast<UINT_PTR>(this);
                nmhdr.tabOrigin = 0;
                ::SendMessage(m_hParent, WM_NOTIFY, 0, reinterpret_cast<LPARAM>(&nmhdr));
                return TRUE;
            }

            ::CallWindowProc(m_TabBarDefaultProc, hwnd, Message, wParam, lParam);
            if (wParam == 2)
                return TRUE;
            int currentTabOn = TabCtrl_GetCurSel(*this);

            m_nSrcTab = m_nTabDragged = currentTabOn;

            POINT point;
            point.x = xPos;
            point.y = yPos;
            ::ClientToScreen(hwnd, &point);
            if (::DragDetect(hwnd, point))
            {
                // Yes, we're beginning to drag, so capture the mouse...
                m_bIsDragging = true;
                ::SetCapture(hwnd);
                RegisterHotKey(hwnd, 0 /* id */, 0, VK_ESCAPE);
            }

            TBHDR nmhdr;
            nmhdr.hdr.hwndFrom = *this;
            nmhdr.hdr.code = NM_CLICK;
            nmhdr.hdr.idFrom = reinterpret_cast<UINT_PTR>(this);
            nmhdr.tabOrigin = currentTabOn;

            ::SendMessage(m_hParent, WM_NOTIFY, 0, reinterpret_cast<LPARAM>(&nmhdr));
            return TRUE;
        }
            break;
        case WM_RBUTTONDOWN:   //rightclick selects tab aswell
        {
            ::CallWindowProc(m_TabBarDefaultProc, hwnd, WM_LBUTTONDOWN, wParam, lParam);
            return TRUE;
        }
            break;
        case WM_MOUSEMOVE:
        {
            int xPos = GET_X_LPARAM(lParam);
            int yPos = GET_Y_LPARAM(lParam);

            if (m_bIsDragging)
            {
                POINT p;
                p.x = xPos;
                p.y = yPos;
                ExchangeItemData(p);

                // Get cursor position of "Screen"
                // For using the function "WindowFromPoint" afterward!!!
                ::GetCursorPos(&m_draggingPoint);

                DraggingCursor(m_draggingPoint, m_nTabDragged);
                return TRUE;
            }

            int index = GetTabIndexAt(xPos, yPos);
            if (index != -1)
            {
                // Reduce flicker by only redrawing needed tabs

                bool wasCloseHover = m_bIsCloseHover;
                int oldIndex = m_currentHoverTabItem;
                RECT oldRect = { 0 };

                bool ok;
                ok = TabCtrl_GetItemRect(*this, index, &m_currentHoverTabRect) != FALSE;
                APPVERIFY(ok);
                if (oldIndex != -1)
                {
                    ok = TabCtrl_GetItemRect(*this, oldIndex, &oldRect) != FALSE;
                    APPVERIFY(ok);
                }
                m_currentHoverTabItem = index;
                m_bIsCloseHover = m_closeButtonZone.IsHit(xPos, yPos, m_currentHoverTabRect);

                {
                    TRACKMOUSEEVENT tme = { 0 };
                    tme.cbSize = sizeof(TRACKMOUSEEVENT);
                    tme.dwFlags = TME_LEAVE;
                    tme.hwndTrack = *this;
                    TrackMouseEvent(&tme);
                }
                if (wasCloseHover != m_bIsCloseHover)
                {
                    if (oldIndex != -1)
                        InvalidateRect(hwnd, &oldRect, FALSE);
                    InvalidateRect(hwnd, &m_currentHoverTabRect, FALSE);
                }
                else if (m_currentHoverTabItem != oldIndex)
                {
                    if (oldIndex != -1)
                        InvalidateRect(hwnd, &oldRect, FALSE);
                    InvalidateRect(hwnd, &m_currentHoverTabRect, FALSE);
                }
            }
            else if (m_currentHoverTabItem != index)
            {
                InvalidateRect(hwnd, &m_currentHoverTabRect, FALSE);
                m_currentHoverTabItem = index;
            }
        }
            break;
        case WM_MOUSELEAVE:
        {
            TRACKMOUSEEVENT tme = { 0 };
            tme.cbSize = sizeof(TRACKMOUSEEVENT);
            tme.dwFlags = TME_LEAVE | TME_CANCEL;
            tme.hwndTrack = *this;
            TrackMouseEvent(&tme);
            m_bIsCloseHover = false;
            m_currentHoverTabItem = -1;
            InvalidateRect(hwnd, &m_currentHoverTabRect, FALSE);
        }
            break;
        case WM_LBUTTONUP:
        {
            int xPos = GET_X_LPARAM(lParam);
            int yPos = GET_Y_LPARAM(lParam);
            int currentTabOn = GetTabIndexAt(xPos, yPos);
            if (m_bIsDragging)
            {
                if (::GetCapture() == *this)
                    ::ReleaseCapture();
                UnregisterHotKey(hwnd, 0 /* id */);

                // Send a notification message to the parent with wParam = 0, lParam = 0
                // nmhdr.idFrom = this
                // destIndex = this->_nSrcTab
                // scrIndex  = this->_nTabDragged
                TBHDR nmhdr;
                nmhdr.hdr.hwndFrom = *this;
                nmhdr.hdr.code = m_bIsDraggingInside ? TCN_TABDROPPED : TCN_TABDROPPEDOUTSIDE;
                nmhdr.hdr.idFrom = reinterpret_cast<UINT_PTR>(this);
                nmhdr.tabOrigin = m_nTabDragged;
                ::SendMessage(m_hParent, WM_NOTIFY, 0, reinterpret_cast<LPARAM>(&nmhdr));
                return TRUE;
            }

            if ((m_whichCloseClickDown == currentTabOn) && m_closeButtonZone.IsHit(xPos, yPos, m_currentHoverTabRect))
            {
                NotifyTabDelete(currentTabOn);

                // Don't remember the close button location, it will change if the user
                // responds to the notification by deleting the tab and if they
                // don't delete the tab, it'll get calculated again when they move
                // the mouse.
                ::SetRectEmpty(&m_currentHoverTabRect);
                m_whichCloseClickDown = -1;

                return TRUE;
            }
            m_whichCloseClickDown = -1;
        }
            break;
        case WM_CAPTURECHANGED:
        {
            if (m_bIsDragging)
            {
                m_bIsDragging = false;
                return TRUE;
            }
        }
            break;
        case WM_DRAWITEM:
        {
            DrawItem((DRAWITEMSTRUCT *)lParam);
            return TRUE;
        }
            break;
        case WM_HOTKEY:
        {
            if (m_bIsDragging &&
                VK_ESCAPE == HIWORD(lParam) &&
                IDHOT_SNAPDESKTOP != wParam &&
                IDHOT_SNAPWINDOW != wParam)
            {
                // handle ESC keypress
                UnregisterHotKey(hwnd, 0 /* id */);
                if (::GetCapture() == *this)
                    ::ReleaseCapture();
                m_bIsDragging = false;
            }
        }
        break;
        case WM_MBUTTONUP:
        {
            int xPos = GET_X_LPARAM(lParam);
            int yPos = GET_Y_LPARAM(lParam);
            int currentTabOn = GetTabIndexAt(xPos, yPos);
            NotifyTabDelete(currentTabOn);

            return TRUE;
        }
            break;
        case WM_CONTEXTMENU:
        {
            int xPos = GET_X_LPARAM(lParam);
            int yPos = GET_Y_LPARAM(lParam);

            POINT pt;
            pt.x = xPos;
            pt.y = yPos;

            if (pt.x == -1 && pt.y == -1)
            {
                HRESULT hr;

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

            // The basic pattern of how to show Contextual UI in a specified location.
            IUIContextualUI * pContextualUI = nullptr;
            if (SUCCEEDED(g_pFramework->GetView(cmdTabContextMap, IID_PPV_ARGS(&pContextualUI))))
            {
                pContextualUI->ShowAtLocation(pt.x, pt.y);
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
    nmhdr.hdr.idFrom = reinterpret_cast<UINT_PTR>(this);
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
    int curSel = TabCtrl_GetCurSel(*this);
    bool bSelected = (lpdis->itemID == (UINT)curSel);

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
    FillSolidRect(lpdis->hDC, rBorder.left, rBorder.top, rBorder.right, rBorder.bottom, CTheme::Instance().GetThemeColor(::GetSysColor(COLOR_3DFACE)));
}

void CTabBar::DrawItem(LPDRAWITEMSTRUCT pDrawItemStruct)
{
    TC_ITEM tci;
    HIMAGELIST hilTabs = (HIMAGELIST)TabCtrl_GetImageList(*this);

    int curSel = TabCtrl_GetCurSel(*this);
    bool bSelected = (pDrawItemStruct->itemID == (UINT)curSel);
    bool bMouseOver = (pDrawItemStruct->itemID == (UINT)m_currentHoverTabItem);

    RECT rItem(pDrawItemStruct->rcItem);

    if (bSelected)
        rItem.bottom -= 1;
    else
        rItem.bottom += 2;

    // tab
    // blend from back color to COLOR_3DFACE if 16 bit mode or better
    COLORREF crFrom = GetTabColor(bSelected, pDrawItemStruct->itemID);

    COLORREF crTo = bSelected ? ::GetSysColor(COLOR_3DFACE) : Darker(::GetSysColor(COLOR_3DFACE), 0.95f);
    if (bMouseOver)
        crTo = Darker(crTo, 0.7f);

    crTo = CTheme::Instance().GetThemeColor(crTo);

    int nROrg = GetRValue(crFrom);
    int nGOrg = GetGValue(crFrom);
    int nBOrg = GetBValue(crFrom);
    int nRDiff = GetRValue(crTo) - nROrg;
    int nGDiff = GetGValue(crTo) - nGOrg;
    int nBDiff = GetBValue(crTo) - nBOrg;

    int nHeight = rItem.bottom - rItem.top;

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
    tci.cchTextMax = _countof(buf) - 1;
    TabCtrl_GetItem(*this, pDrawItemStruct->itemID, &tci);
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
    {
        // The pushed state doesn't work and this line related to it creates painting glitches.
        // So just disable the line until someone cares about the pushed state enough
        // to make it work properly.
        // idCloseImg = IDR_CLOSETAB_PUSH;
    }
    else
        idCloseImg = bSelected ? IDR_CLOSETAB : IDR_CLOSETAB_INACT;
    HDC hdcMemory;
    hdcMemory = ::CreateCompatibleDC(pDrawItemStruct->hDC);
    HBITMAP hBmp = (HBITMAP)::LoadImage(hResource, MAKEINTRESOURCE(idCloseImg), IMAGE_BITMAP, int(11 * m_dpiScale), int(11 * m_dpiScale), 0);
    BITMAP bmp;
    ::GetObject(hBmp, sizeof(bmp), &bmp);
    rItem.right = closeButtonRect.left;
    ::SelectObject(hdcMemory, hBmp);
    ::BitBlt(pDrawItemStruct->hDC, closeButtonRect.left, closeButtonRect.top, bmp.bmWidth, bmp.bmHeight, hdcMemory, 0, 0, SRCCOPY);
    ::DeleteDC(hdcMemory);
    ::DeleteObject(hBmp);

    // icon
    if (TABBAR_SHOWDISKICON && hilTabs)
    {
        ImageList_Draw(hilTabs, tci.iImage, pDrawItemStruct->hDC, rItem.left, rItem.top, ILD_TRANSPARENT);
        rItem.left += 16 + PADDING;
    }
    else
        rItem.left += PADDING;

    // text
    rItem.right -= PADDING;
    UINT uFlags = DT_SINGLELINE | DT_MODIFYSTRING | DT_END_ELLIPSIS | DT_NOPREFIX | DT_CENTER;
    ::DrawText(pDrawItemStruct->hDC, buf, -1, &rItem, uFlags);
    COLORREF textColor = CTheme::Instance().GetThemeColor(Darker(::GetSysColor(COLOR_3DDKSHADOW), 0.5f));
    if (bSelected)
        textColor = CTheme::Instance().GetThemeColor(::GetSysColor(COLOR_WINDOWTEXT));
    if (tci.iImage == UNSAVED_IMG_INDEX)
        textColor = CTheme::Instance().GetThemeColor(RGB(100, 0, 0));
    SetTextColor(pDrawItemStruct->hDC, textColor);
    DrawText(pDrawItemStruct->hDC, buf, -1, &rItem, DT_SINGLELINE | DT_MODIFYSTRING | DT_END_ELLIPSIS | DT_NOPREFIX | DT_CENTER);
}


void CTabBar::DraggingCursor(POINT screenPoint, UINT item)
{
    HWND hWin = ::WindowFromPoint(screenPoint);
    if (*this == hWin)
        ::SetCursor(::LoadCursor(nullptr, IDC_ARROW));
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
        else
        {
            TBHDR nmhdr;
            nmhdr.hdr.hwndFrom = *this;
            nmhdr.hdr.code = TCN_GETDROPICON;
            nmhdr.hdr.idFrom = reinterpret_cast<UINT_PTR>(this);
            nmhdr.tabOrigin = item;
            HICON icon = (HICON)::SendMessage(m_hParent, WM_NOTIFY, 0, reinterpret_cast<LPARAM>(&nmhdr));
            if (icon)
                ::SetCursor(icon);
            else if (IsPointInParentZone(screenPoint))
                ::SetCursor(::LoadCursor(hResource, MAKEINTRESOURCE(IDC_DRAG_INTERDIT_TAB)));
            else // drag out of application
                ::SetCursor(::LoadCursor(hResource, MAKEINTRESOURCE(IDC_DRAG_OUT_TAB)));
        }
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
            TabCtrl_SetCurSel(*this, nTab);

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

            bool ok;
            ok = TabCtrl_GetItem(*this, m_nTabDragged, &itemData_nDraggedTab) != FALSE;
            APPVERIFY(ok);

            if (m_nTabDragged > nTab)
            {
                for (int i = m_nTabDragged; i > nTab; i--)
                {
                    ok = TabCtrl_GetItem(*this, i - 1, &itemData_shift) != FALSE;
                    APPVERIFY(ok);
                    ok = TabCtrl_SetItem(*this, i, &itemData_shift) != FALSE;
                    APPVERIFY(ok);
                }
            }
            else
            {
                for (int i = m_nTabDragged; i < nTab; i++)
                {
                    ok = TabCtrl_GetItem(*this, i + 1, &itemData_shift) != FALSE;
                    APPVERIFY(ok);
                    ok = TabCtrl_SetItem(*this, i, &itemData_shift) != FALSE;
                    APPVERIFY(ok);
                }
            }
            ok = TabCtrl_SetItem(*this, nTab, &itemData_nDraggedTab) != FALSE;
            APPVERIFY(ok);

            //3. update the current index
            m_nTabDragged = nTab;

            TBHDR nmhdr;
            nmhdr.hdr.hwndFrom = *this;
            nmhdr.hdr.code = TCN_ORDERCHANGED;
            nmhdr.hdr.idFrom = reinterpret_cast<UINT_PTR>(this);
            nmhdr.tabOrigin = nTab;
            ::SendMessage(m_hParent, WM_NOTIFY, 0, reinterpret_cast<LPARAM>(&nmhdr));
        }
    }
    else
    {
        m_bIsDraggingInside = false;
    }

}

int CTabBar::GetTabIndexAt(int x, int y) const
{
    TCHITTESTINFO hitInfo;
    hitInfo.pt.x = x;
    hitInfo.pt.y = y;
    return TabCtrl_HitTest(*this, &hitInfo);
}

bool CTabBar::IsPointInParentZone(POINT screenPoint) const
{
    RECT parentZone;
    ::GetWindowRect(m_hParent, &parentZone);
    return (((screenPoint.x >= parentZone.left) && (screenPoint.x <= parentZone.right)) &&
            (screenPoint.y >= parentZone.top) && (screenPoint.y <= parentZone.bottom));
}

LRESULT CALLBACK CTabBar::TabBar_Proc(HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam)
{
    return (((CTabBar *)(::GetWindowLongPtr(hwnd, GWLP_USERDATA)))->RunProc(hwnd, Message, wParam, lParam));
}

int CTabBar::GetIDFromIndex(int index) const
{
    TCITEM tci;
    tci.mask = TCIF_PARAM;
    auto result = TabCtrl_GetItem(*this, index, &tci);
    // Easier to set a break point on failed results.
    if (result)
        return (int)tci.lParam;
    else
        return -1;
}

int CTabBar::GetIndexFromID(int id) const
{
    if (id >= 0) // Only look for an id that's legal to begin with.
    {
        for (int i = 0; i < m_nItems; ++i)
        {
            if (GetIDFromIndex(i) == id)
                return i;
        }
    }
    return -1;
}

void CTabBar::NotifyTabDelete(int tab)
{
    TBHDR nmhdr;
    nmhdr.hdr.hwndFrom = *this;
    nmhdr.hdr.code = TCN_TABDELETE;
    nmhdr.hdr.idFrom = reinterpret_cast<UINT_PTR>(this);
    nmhdr.tabOrigin = tab;
    ::SendMessage(m_hParent, WM_NOTIFY, 0, reinterpret_cast<LPARAM>(&nmhdr));
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
