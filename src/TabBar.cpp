// This file is part of BowPad.
//
// Copyright (C) 2013-2021 - Stefan Kueng
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
#include "AppUtils.h"
#include "Theme.h"
#include "BowPad.h"
#include "BowPadUI.h"
#include "GDIHelpers.h"
#include "DPIAware.h"

#pragma warning(push)
#pragma warning(disable : 4458) // declaration of 'xxx' hides class member
#include <gdiplus.h>
#pragma warning(pop)
#include <Uxtheme.h>
#include <UIRibbon.h>
#include <UIRibbonPropertyHelpers.h>

#pragma comment(lib, "UxTheme.lib")

extern IUIFramework *g_pFramework;

//#define TABBAR_SHOWDISKICON 1
const double hoverFraction = 0.4;

CTabBar::CTabBar(HINSTANCE hInst)
    : CWindow(hInst)
    , m_nItems(0)
    , m_bHasImgList(false)
    , m_hFont(nullptr)
    , m_hBoldFont(nullptr)
    , m_hSymbolFont(nullptr)
    , m_hSymbolBoldFont(nullptr)
    , m_tabID(0)
    , m_ctrlID(-1)
    , m_bIsDragging(false)
    , m_bIsDraggingInside(false)
    , m_nSrcTab(-1)
    , m_nTabDragged(-1)
    , m_tabBarDefaultProc(nullptr)
    , m_currentHoverTabItem(-1)
    , m_bIsCloseHover(false)
    , m_whichCloseClickDown(-1)
    , m_lmbdHit(false)
    , m_closeChar(L'\u274C')
    , m_modifiedChar(L'\u25CF')
{
    Gdiplus::GdiplusStartupInput gdiplusStartupInput;
    Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, nullptr);
    m_draggingPoint       = {};
    m_currentHoverTabRect = {};
    if (IsWindows10OrGreater())
        m_modifiedChar = L'\u2B24';
};

CTabBar::~CTabBar()
{
    if (m_hFont)
        DeleteObject(m_hFont);
    if (m_hBoldFont)
        DeleteObject(m_hBoldFont);
    if (m_hSymbolFont)
        DeleteObject(m_hSymbolFont);
    if (m_hSymbolBoldFont)
        DeleteObject(m_hSymbolBoldFont);
    Gdiplus::GdiplusShutdown(gdiplusToken);
}

bool CTabBar::Init(HINSTANCE /*hInst*/, HWND hParent)
{
    INITCOMMONCONTROLSEX icce{};
    icce.dwSize = sizeof(icce);
    icce.dwICC  = ICC_TAB_CLASSES;
    InitCommonControlsEx(&icce);

    CreateEx(WS_EX_COMPOSITED, WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | WS_VISIBLE | TCS_FOCUSNEVER | TCS_TOOLTIPS | TCS_TABS | TCS_OWNERDRAWFIXED, hParent, nullptr, WC_TABCONTROL);

    if (!*this)
    {
        return false;
    }

    ::SetWindowLongPtr(*this, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
    m_tabBarDefaultProc = reinterpret_cast<WNDPROC>(::SetWindowLongPtr(*this, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(TabBar_Proc)));

    NONCLIENTMETRICS ncm{};
    ncm.cbSize = sizeof(NONCLIENTMETRICS);
    SystemParametersInfo(SPI_GETNONCLIENTMETRICS, sizeof(NONCLIENTMETRICS), &ncm, 0U);
    ncm.lfSmCaptionFont.lfWeight = FW_NORMAL;
    m_hFont                      = CreateFontIndirect(&ncm.lfSmCaptionFont);
    ncm.lfSmCaptionFont.lfWeight = FW_EXTRABOLD;
    m_hBoldFont                  = CreateFontIndirect(&ncm.lfSmCaptionFont);
    wcscpy_s(ncm.lfSmCaptionFont.lfFaceName, L"Segoe UI Symbol");
    ncm.lfSmCaptionFont.lfWeight = FW_NORMAL;
    m_hSymbolFont                = CreateFontIndirect(&ncm.lfSmCaptionFont);
    ncm.lfSmCaptionFont.lfWeight = FW_EXTRABOLD;
    m_hSymbolBoldFont            = CreateFontIndirect(&ncm.lfSmCaptionFont);
    ::SendMessage(*this, WM_SETFONT, reinterpret_cast<WPARAM>(m_hFont), 0);

    TabCtrl_SetItemSize(*this, CDPIAware::Instance().Scale(*this, 300), CDPIAware::Instance().Scale(*this, 25));
    TabCtrl_SetPadding(*this, CDPIAware::Instance().Scale(*this, 13), 0);
    m_closeButtonZone.m_height = m_closeButtonZone.m_width = -ncm.lfSmCaptionFont.lfHeight;
    m_closeButtonZone.m_fromRight                          = CDPIAware::Instance().Scale(*this, m_closeButtonZone.m_fromRight);

    return true;
}

LRESULT CALLBACK CTabBar::WinMsgHandler(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    if (prevWndProc)
        return CallWindowProc(prevWndProc, hwnd, uMsg, wParam, lParam);
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

int CTabBar::InsertAtEnd(const wchar_t *subTabName)
{
    TCITEM tie{};
    tie.mask = TCIF_TEXT | TCIF_IMAGE | TCIF_PARAM;
#ifdef TABBAR_SHOWDISKICON
    int imageIndex = -1;

    if (m_bHasImgList)
        imageIndex = 0;
    tie.iImage = imageIndex;
#else
    tie.iImage = 0;
#endif
    tie.pszText         = const_cast<wchar_t *>(subTabName);
    m_animVars[m_tabID] = Animator::Instance().CreateAnimationVariable(1.0, 1.0);
    tie.lParam          = m_tabID++;
    int index           = TabCtrl_InsertItem(*this, m_nItems++, &tie);
    // if this is the first item added to the tab bar, it is automatically
    // selected but without the proper notifications.
    // remove the selection so it can be selected properly later.
    if (m_nItems == 1)
        TabCtrl_SetCurSel(*this, -1);
    InvalidateRect(*this, nullptr, FALSE);
    return index;
}

int CTabBar::InsertAfter(int index, const wchar_t *subTabName)
{
    TCITEM tie{};
    tie.mask = TCIF_TEXT | TCIF_IMAGE | TCIF_PARAM;
#ifdef TABBAR_SHOWDISKICON
    int imageIndex = -1;

    if (m_bHasImgList)
        imageIndex = 0;
    tie.iImage = imageIndex;
#else
    tie.iImage = 0;
#endif
    tie.pszText         = const_cast<wchar_t *>(subTabName);
    m_animVars[m_tabID] = Animator::Instance().CreateAnimationVariable(1.0, 1.0);
    tie.lParam          = m_tabID++;
    if ((index + 1) >= m_nItems)
        index = m_nItems - 1;
    int ret = TabCtrl_InsertItem(*this, index + 1, &tie);
    // if this is the first item added to the tab bar, it is automatically
    // selected but without the proper notifications.
    // remove the selection so it can be selected properly later.
    if (m_nItems == 0)
        TabCtrl_SetCurSel(*this, -1);
    ++m_nItems;
    InvalidateRect(*this, nullptr, FALSE);
    return ret;
}

void CTabBar::GetTitle(int index, wchar_t *title, int titleLen) const
{
    TCITEM tci{};
    tci.mask       = TCIF_TEXT;
    tci.pszText    = title;
    tci.cchTextMax = titleLen - 1;
    TabCtrl_GetItem(*this, index, &tci);
}

std::wstring CTabBar::GetTitle(int index) const
{
    wchar_t    buf[100] = {};
    const auto bufCount = _countof(buf);

    TCITEM tci{};
    tci.mask       = TCIF_TEXT;
    tci.pszText    = buf;
    tci.cchTextMax = bufCount - 1;
    auto ret       = TabCtrl_GetItem(*this, index, &tci);
    // In case buffer fills completely, ensure last character is terminated.
    buf[bufCount - 1] = L'\0';
    // Documentation suggests GetItem might not use our buffer
    // or might null it out on error.
    return (ret && tci.pszText != nullptr) ? buf : L"";
}

void CTabBar::GetCurrentTitle(wchar_t *title, int titleLen) const
{
    GetTitle(GetCurrentTabIndex(), title, titleLen);
}

std::wstring CTabBar::GetCurrentTitle() const
{
    return GetTitle(GetCurrentTabIndex());
}

void CTabBar::SetCurrentTitle(LPCTSTR title)
{
    TCITEM tci{};
    tci.mask    = TCIF_TEXT;
    tci.pszText = const_cast<wchar_t *>(title);
    TabCtrl_SetItem(*this, GetCurrentTabIndex(), &tci);
    InvalidateRect(*this, nullptr, FALSE);
}

void CTabBar::SetTitle(int index, LPCTSTR title)
{
    TCITEM tci{};
    tci.mask    = TCIF_TEXT;
    tci.pszText = const_cast<wchar_t *>(title);
    TabCtrl_SetItem(*this, index, &tci);
    InvalidateRect(*this, nullptr, FALSE);
}

void CTabBar::SetFont(const wchar_t *fontName, int fontSize)
{
    if (m_hFont)
        ::DeleteObject(m_hFont);
    if (m_hBoldFont)
        ::DeleteObject(m_hBoldFont);

    m_hFont = ::CreateFont(static_cast<int>(fontSize), 0,
                           0,
                           0,
                           FW_NORMAL,
                           0, 0, 0, 0,
                           0, 0, 0, 0,
                           fontName);
    if (m_hFont)
        ::SendMessage(*this, WM_SETFONT, reinterpret_cast<WPARAM>(m_hFont), 0);
    m_hBoldFont = ::CreateFont(static_cast<int>(fontSize), 0,
                               0,
                               0,
                               FW_EXTRABOLD,
                               0, 0, 0, 0,
                               0, 0, 0, 0,
                               fontName);
}

void CTabBar::SelectChanging() const
{
    NMHDR nmhdr    = {};
    nmhdr.hwndFrom = *this;
    nmhdr.code     = TCN_SELCHANGING;
    ::SendMessage(m_hParent, WM_NOTIFY, 0, reinterpret_cast<LPARAM>(&nmhdr));
}

void CTabBar::SelectChange(int index) const
{
    if (index >= 0)
        TabCtrl_SetCurSel(*this, index);

    NMHDR nmhdr    = {};
    nmhdr.hwndFrom = *this;
    nmhdr.code     = TCN_SELCHANGE;
    ::SendMessage(m_hParent, WM_NOTIFY, 0, reinterpret_cast<LPARAM>(&nmhdr));
    InvalidateRect(*this, nullptr, FALSE);
}

void CTabBar::ActivateAt(int index) const
{
    SelectChanging();
    SelectChange(index);
    InvalidateRect(*this, nullptr, FALSE);
}

void CTabBar::DeleteItemAt(int index)
{
    // Decrement the item count before calling delete as this delete
    // seems to initiate events (like paint) that will assume the extra
    // item is present if it's not already accounted as gone before then,
    // even though it is gone, but the delete itself is initiating the actions.
    // Failure to decrement the count first will show up as assertion
    // failures in calls to GetItemRect around line 320 & 530 in the
    // paint logic.
    auto foundIt = m_animVars.find(GetIDFromIndex(index).GetValue());
    assert(foundIt != m_animVars.end());
    m_animVars.erase(foundIt);

    m_nItems--;
    bool deleted = TabCtrl_DeleteItem(*this, index) != FALSE;
    APPVERIFY(deleted);
    if (m_currentHoverTabItem == index)
        m_currentHoverTabItem = -1;
    // prevent invisible tabs. If last visible tab is removed, other tabs are put in view but not redrawn
    // Therefore, scroll until the last tab is at the very right
    if (m_nItems > 1)
    {
        RECT itemRect{};
        TabCtrl_GetItemRect(*this, m_nItems - 1, &itemRect);
        RECT tabRect;
        GetClientRect(*this, &tabRect);
        TC_HITTESTINFO hti{};
        hti.pt                 = {14, 14}; // arbitrary value: just inside the first visible tab
        LRESULT scrollTabIndex = ::SendMessage(*this, TCM_HITTEST, 0, reinterpret_cast<LPARAM>(&hti));
        do
        {
            if (itemRect.right < tabRect.right - (itemRect.right - itemRect.left))
            {
                --scrollTabIndex;
                if (scrollTabIndex >= 0)
                {
                    int wParam = MAKEWPARAM(SB_THUMBPOSITION, scrollTabIndex);
                    ::SendMessage(*this, WM_HSCROLL, wParam, 0);
                }
                else
                    break;
            }
            TabCtrl_GetItemRect(*this, m_nItems - 1, &itemRect);
        } while (itemRect.right < tabRect.right - (itemRect.right - itemRect.left));
    }
    InvalidateRect(*this, nullptr, FALSE);
}

int CTabBar::GetCurrentTabIndex() const
{
    return TabCtrl_GetCurSel(*this);
}

DocID CTabBar::GetCurrentTabId() const
{
    int index = TabCtrl_GetCurSel(*this);
    if (index < 0)
    {
        index = TabCtrl_GetCurFocus(*this);
        if (index < 0)
            return {};
    }
    TCITEM tci{};
    tci.mask    = TCIF_PARAM;
    BOOL result = TabCtrl_GetItem(*this, index, &tci);
    if (!result)
    {
        assert(false);
        return {};
    }
    return DocID(static_cast<int>(tci.lParam));
}

void CTabBar::SetCurrentTabId(DocID id)
{
    int index = TabCtrl_GetCurSel(*this);
    if (index < 0)
    {
        index = TabCtrl_GetCurFocus(*this);
        if (index < 0)
            return;
    }
    TCITEM tci{};
    tci.mask   = TCIF_PARAM;
    tci.lParam = id.GetValue();
    TabCtrl_SetItem(*this, index, &tci);
}

void CTabBar::DeleteAllItems()
{
    TabCtrl_DeleteAllItems(*this);
    m_nItems = 0;
    InvalidateRect(*this, nullptr, FALSE);
}

HIMAGELIST CTabBar::SetImageList(HIMAGELIST himl)
{
    m_bHasImgList = (himl != nullptr);
    return TabCtrl_SetImageList(*this, himl);
}

LRESULT CTabBar::RunProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
        case WM_ERASEBKGND:
        {
            HDC      hDC = reinterpret_cast<HDC>(wParam);
            RECT     rClient{}, rTab{}, rTotalTab{}, rBkgnd{};
            COLORREF crBack;

            if (CTheme::Instance().IsHighContrastMode())
                return ::CallWindowProc(m_tabBarDefaultProc, hwnd, message, wParam, lParam);
            else
                ::CallWindowProc(m_tabBarDefaultProc, hwnd, message, wParam, lParam);

            // calculate total tab width
            GetClientRect(*this, &rClient);
            SetRectEmpty(&rTotalTab);

            for (int nTab = GetItemCount() - 1; nTab >= 0; --nTab)
            {
                bool ok = TabCtrl_GetItemRect(*this, nTab, &rTab) != FALSE;
                APPVERIFY(ok);
                UnionRect(&rTotalTab, &rTab, &rTotalTab);
            }

            int nTabHeight = rTotalTab.bottom - rTotalTab.top;

            // add a bit
            InflateRect(&rTotalTab, CDPIAware::Instance().Scale(*this, 2), CDPIAware::Instance().Scale(*this, 3));

            // then if background color is set, paint the visible background
            // area of the tabs in the bkgnd color
            // note: the MFC code for drawing the tabs makes all sorts of assumptions
            // about the background color of the tab control being the same as the page
            // color - in some places the background color shows through the pages!!
            // so we must only paint the background color where we need to, which is that
            // portion of the tab area not excluded by the tabs themselves
            crBack = CTheme::Instance().GetThemeColor(::GetSysColor(COLOR_3DFACE));

            // full width of tab ctrl above top of tabs
            rBkgnd        = rClient;
            rBkgnd.bottom = rTotalTab.top + CDPIAware::Instance().Scale(*this, 3);
            SetBkColor(hDC, crBack);
            ExtTextOut(hDC, rBkgnd.left, rBkgnd.top, ETO_CLIPPED | ETO_OPAQUE, &rBkgnd, L"", 0, nullptr);

            // width of tab ctrl visible bkgnd including bottom pixel of tabs to left of tabs
            rBkgnd        = rClient;
            rBkgnd.right  = 2;
            rBkgnd.bottom = rBkgnd.top + (nTabHeight + CDPIAware::Instance().Scale(*this, 2));
            ExtTextOut(hDC, rBkgnd.left, rBkgnd.top, ETO_CLIPPED | ETO_OPAQUE, &rBkgnd, L"", 0, nullptr);

            // to right of tabs
            rBkgnd = rClient;
            rBkgnd.left += (rTotalTab.right - (max(rTotalTab.left, 0))) - CDPIAware::Instance().Scale(*this, 2);
            rBkgnd.bottom = rBkgnd.top + (nTabHeight + CDPIAware::Instance().Scale(*this, 2));
            ExtTextOut(hDC, rBkgnd.left, rBkgnd.top, ETO_CLIPPED | ETO_OPAQUE, &rBkgnd, L"", 0, nullptr);

            return TRUE;
        }
        case WM_PAINT:
        {
            PAINTSTRUCT ps; // ps is a totally out parameter, no init needed.
            HDC         hDC = BeginPaint(*this, &ps);

            // prepare dc
            HGDIOBJ hOldFont = SelectObject(hDC, m_hFont);

            DRAWITEMSTRUCT dis = {0};
            dis.CtlType        = ODT_TAB;
            dis.CtlID          = 0;
            dis.hwndItem       = *this;
            dis.hDC            = hDC;
            dis.itemAction     = ODA_DRAWENTIRE;

            // draw the rest of the border
            RECT rPage;
            GetClientRect(*this, &dis.rcItem);
            rPage = dis.rcItem;
            TabCtrl_AdjustRect(*this, FALSE, &rPage);
            dis.rcItem.top = rPage.top - CDPIAware::Instance().Scale(*this, 2);

            DrawMainBorder(&dis);

            // paint the tabs first and then the borders
            auto count = GetItemCount();
            if (!count) // no pages added
            {
                SelectObject(hDC, hOldFont);
                EndPaint(*this, &ps);
                return 0;
            }

            auto nSel = TabCtrl_GetCurSel(*this);

            for (int nTab = count - 1; nTab >= 0; --nTab)
            {
                if (nTab != nSel)
                {
                    dis.itemID    = nTab;
                    dis.itemState = 0;
                    bool got      = TabCtrl_GetItemRect(*this, nTab, &dis.rcItem) != FALSE;
                    APPVERIFY(got);
                    if (got)
                    {
                        DrawItem(&dis, static_cast<float>(Animator::GetValue(m_animVars[GetIDFromIndex(nTab).GetValue()])));
                    }
                }
            }

            if (nSel != -1)
            {
                APPVERIFY(nSel >= 0 && nSel < count);
                // now selected tab
                dis.itemID    = nSel;
                dis.itemState = ODS_SELECTED;

                bool got = TabCtrl_GetItemRect(*this, nSel, &dis.rcItem) != FALSE;
                APPVERIFY(got);
                if (got)
                {
                    DrawItem(&dis, static_cast<float>(Animator::GetValue(m_animVars[GetIDFromIndex(nSel).GetValue()])));
                }
            }

            SelectObject(hDC, hOldFont);
            EndPaint(*this, &ps);
        }
        break;
        case WM_MOUSEWHEEL:
        {
            if (m_bIsDragging)
                return TRUE;

            const short wheelDelta = GET_WHEEL_DELTA_WPARAM(wParam);
            const short keyState   = GET_KEYSTATE_WPARAM(wParam);

            // Positive wheel delta means wheel moved forward (away from user).
            // Negative wheel delta means wheel moved backward (towards user).
            const bool wheelForward = wheelDelta >= 0;

            const int tabCount = TabCtrl_GetItemCount(*this);
            if (tabCount == 0)
                return TRUE;
            const int lastTabIndex = tabCount - 1;

            // If wheel moves forward, we wish to scroll backward (towards beginning).
            // If wheel moves backward, we wish to scroll forward (towards end).
            const bool scrollForward = !wheelForward;

            // Mousewheel: scrolls the tab bar area like Firefox does it.
            // Only works if at least one tab is hidden.
            // 1.
            // Wheel forward + control + shift == focus on first tab.
            // Wheel backward + control + shift == focus on last tab.
            // 2.
            // Wheel forward + control == focus on next tab (wrap).
            // Wheel backward + control == focus on previous tab (wrap).
            // 3.
            // Wheel forward + shift == focus on next tab (no wrap).
            // Wheel backward + shift == focus on previous tab (no wrap).
            // 4.
            // Wheel forward + no keys == scroll towards beginning.
            // Wheel backward + no keys == scroll towards end.

            if ((keyState & MK_CONTROL) && (keyState & MK_SHIFT))
            {
                TabCtrl_SetCurFocus(*this, scrollForward ? lastTabIndex : 0);
                return TRUE;
            }
            else if (keyState & (MK_CONTROL | MK_SHIFT))
            {
                int curTabIndex = TabCtrl_GetCurSel(*this);
                int newTabIndex = curTabIndex + (scrollForward ? 1 : -1);
                if (newTabIndex < 0)
                {
                    if (keyState & MK_CONTROL)
                        newTabIndex = lastTabIndex; // Wrap to last tab.
                    else
                        return TRUE;
                }
                else if (newTabIndex > lastTabIndex)
                {
                    if (keyState & MK_CONTROL)
                        newTabIndex = 0; // Wrap to first tab.
                    else
                        return TRUE;
                }
                TabCtrl_SetCurFocus(*this, newTabIndex);
                return TRUE;
            }
            else
            {
                RECT rcTabCtrl{}, rcLastTab{};
                BOOL gotItemRect = TabCtrl_GetItemRect(*this, lastTabIndex, &rcLastTab);
                if (!gotItemRect)
                    return TRUE;
                ::GetClientRect(*this, &rcTabCtrl);

                // Get index of the first visible tab.
                TC_HITTESTINFO hti{};
                LONG           xy  = 14; // Arbitrary value: just inside the first tab.
                hti.pt             = {xy, xy};
                int scrollTabIndex = TabCtrl_HitTest(*this, &hti);

                if (scrollTabIndex < 1 && (rcLastTab.right < rcTabCtrl.right)) // Nothing to scroll.
                    return TRUE;

                // Maximal width/height of the msctls_updown32 class (arrow box in the tab bar),
                // This area may hide parts of the last tab and needs to be excluded.
                const LONG maxLengthUpDownCtrl = 45; // sufficient static value

                // Scroll forward as long as the last tab is hidden; scroll backward till the first tab
                if ((rcTabCtrl.right - rcLastTab.right < maxLengthUpDownCtrl) || !scrollForward)
                {
                    if (scrollForward)
                        ++scrollTabIndex;
                    else
                        --scrollTabIndex;

                    if (scrollTabIndex < 0 || scrollTabIndex > lastTabIndex)
                        return TRUE;

                    // Clear hover state of the close button,
                    // WM_MOUSEMOVE won't handle this properly since the tab position will change.
                    if (m_bIsCloseHover)
                    {
                        m_bIsCloseHover = false;
                        ::InvalidateRect(*this, &m_currentHoverTabRect, FALSE);
                    }

                    ::SendMessage(*this, WM_HSCROLL, MAKEWPARAM(SB_THUMBPOSITION, scrollTabIndex), 0);

                    return TRUE;
                }
            }
            return TRUE;
        }
        case WM_LBUTTONDOWN:
        {
            int xPos = GET_X_LPARAM(lParam);
            int yPos = GET_Y_LPARAM(lParam);

            if (m_closeButtonZone.IsHit(xPos, yPos, m_currentHoverTabRect))
            {
                m_whichCloseClickDown = GetTabIndexAt(xPos, yPos);
                APPVERIFY(m_whichCloseClickDown >= 0);
                TBHDR nmHdr{};
                nmHdr.hdr.hwndFrom = *this;
                nmHdr.hdr.code     = TCN_REFRESH;
                nmHdr.hdr.idFrom   = reinterpret_cast<UINT_PTR>(this);
                nmHdr.tabOrigin    = 0;
                ::SendMessage(m_hParent, WM_NOTIFY, 0, reinterpret_cast<LPARAM>(&nmHdr));
                return TRUE;
            }

            ::CallWindowProc(m_tabBarDefaultProc, hwnd, message, wParam, lParam);
            if (wParam == 2)
                return TRUE;
            int currentTabOn = TabCtrl_GetCurSel(*this);

            m_nSrcTab = m_nTabDragged = currentTabOn;

            POINT point{};
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

            TBHDR nmHdr{};
            nmHdr.hdr.hwndFrom = *this;
            nmHdr.hdr.code     = NM_CLICK;
            nmHdr.hdr.idFrom   = reinterpret_cast<UINT_PTR>(this);
            nmHdr.tabOrigin    = currentTabOn;

            ::SendMessage(m_hParent, WM_NOTIFY, 0, reinterpret_cast<LPARAM>(&nmHdr));
            return TRUE;
        }
        case WM_RBUTTONDOWN: //right click selects tab as well
        {
            ::CallWindowProc(m_tabBarDefaultProc, hwnd, WM_LBUTTONDOWN, wParam, lParam);
            return TRUE;
        }
        case WM_MOUSEMOVE:
        {
            int xPos = GET_X_LPARAM(lParam);
            int yPos = GET_Y_LPARAM(lParam);

            if (m_bIsDragging)
            {
                POINT p{};
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
                int  oldIndex      = m_currentHoverTabItem;
                RECT oldRect       = {0};

                bool ok;
                ok = TabCtrl_GetItemRect(*this, index, &m_currentHoverTabRect) != FALSE;
                APPVERIFY(ok);
                if (oldIndex != -1)
                {
                    ok = TabCtrl_GetItemRect(*this, oldIndex, &oldRect) != FALSE;
                    APPVERIFY(ok);
                }
                m_currentHoverTabItem = index;
                m_bIsCloseHover       = m_closeButtonZone.IsHit(xPos, yPos, m_currentHoverTabRect);

                {
                    TRACKMOUSEEVENT tme = {0};
                    tme.cbSize          = sizeof(TRACKMOUSEEVENT);
                    tme.dwFlags         = TME_LEAVE;
                    tme.hwndTrack       = *this;
                    TrackMouseEvent(&tme);
                }
                if (wasCloseHover != m_bIsCloseHover)
                {
                    if (oldIndex != -1)
                        InvalidateRect(hwnd, &oldRect, FALSE);
                    InvalidateRect(hwnd, &m_currentHoverTabRect, FALSE);
                }
                if (m_currentHoverTabItem != oldIndex)
                {
                    if (oldIndex != -1)
                    {
                        auto transHot   = Animator::Instance().CreateLinearTransition(m_animVars[GetIDFromIndex(oldIndex).GetValue()], 0.3, 1.0);
                        auto storyBoard = Animator::Instance().CreateStoryBoard();
                        if (storyBoard && transHot)
                        {
                            storyBoard->AddTransition(m_animVars[GetIDFromIndex(oldIndex).GetValue()].m_animVar, transHot);
                            Animator::Instance().RunStoryBoard(storyBoard, [hwnd, oldRect]() {
                                InvalidateRect(hwnd, &oldRect, FALSE);
                            });
                        }
                        else
                            InvalidateRect(hwnd, &oldRect, FALSE);
                    }
                    auto transHot   = Animator::Instance().CreateLinearTransition(m_animVars[GetIDFromIndex(m_currentHoverTabItem).GetValue()], 0.1, hoverFraction);
                    auto storyBoard = Animator::Instance().CreateStoryBoard();
                    if (storyBoard && transHot)
                    {
                        storyBoard->AddTransition(m_animVars[GetIDFromIndex(m_currentHoverTabItem).GetValue()].m_animVar, transHot);
                        Animator::Instance().RunStoryBoard(storyBoard, [hwnd, this]() {
                            InvalidateRect(hwnd, &m_currentHoverTabRect, FALSE);
                        });
                    }
                    else
                        InvalidateRect(hwnd, &m_currentHoverTabRect, FALSE);
                }
            }
            else if (m_currentHoverTabItem != index)
            {
                auto transHot   = Animator::Instance().CreateLinearTransition(m_animVars[GetIDFromIndex(m_currentHoverTabItem).GetValue()], 0.3, 1.0);
                auto storyBoard = Animator::Instance().CreateStoryBoard();
                if (storyBoard && transHot)
                {
                    storyBoard->AddTransition(m_animVars[GetIDFromIndex(m_currentHoverTabItem).GetValue()].m_animVar, transHot);
                    auto animRect = m_currentHoverTabRect;
                    Animator::Instance().RunStoryBoard(storyBoard, [hwnd, animRect]() {
                        InvalidateRect(hwnd, &animRect, FALSE);
                    });
                }
                else
                    InvalidateRect(hwnd, &m_currentHoverTabRect, FALSE);
                m_currentHoverTabItem = index;
            }
        }
        break;
        case WM_MOUSELEAVE:
        {
            TRACKMOUSEEVENT tme = {0};
            tme.cbSize          = sizeof(TRACKMOUSEEVENT);
            tme.dwFlags         = TME_LEAVE | TME_CANCEL;
            tme.hwndTrack       = *this;
            TrackMouseEvent(&tme);
            m_bIsCloseHover = false;
            auto transHot   = Animator::Instance().CreateLinearTransition(m_animVars[GetIDFromIndex(m_currentHoverTabItem).GetValue()], 0.3, 1.0);
            auto storyBoard = Animator::Instance().CreateStoryBoard();
            if (storyBoard && transHot)
            {
                storyBoard->AddTransition(m_animVars[GetIDFromIndex(m_currentHoverTabItem).GetValue()].m_animVar, transHot);
                auto oldRect = m_currentHoverTabRect;
                Animator::Instance().RunStoryBoard(storyBoard, [hwnd, oldRect]() {
                    InvalidateRect(hwnd, &oldRect, FALSE);
                });
            }
            else
                InvalidateRect(hwnd, &m_currentHoverTabRect, FALSE);
            m_currentHoverTabItem = -1;
        }
        break;
        case WM_LBUTTONUP:
        {
            int xPos = GET_X_LPARAM(lParam);
            int yPos = GET_Y_LPARAM(lParam);
            if (m_bIsDragging)
            {
                if (::GetCapture() == *this)
                    ::ReleaseCapture();
                UnregisterHotKey(hwnd, 0 /* id */);

                // Send a notification message to the parent with wParam = 0, lParam = 0
                // nmhdr.idFrom = this
                // destIndex = this->_nSrcTab
                // scrIndex  = this->_nTabDragged
                TBHDR nmHdr{};
                nmHdr.hdr.hwndFrom = *this;
                nmHdr.hdr.code     = m_bIsDraggingInside ? TCN_TABDROPPED : TCN_TABDROPPEDOUTSIDE;
                nmHdr.hdr.idFrom   = reinterpret_cast<UINT_PTR>(this);
                nmHdr.tabOrigin    = m_nTabDragged;
                ::SendMessage(m_hParent, WM_NOTIFY, 0, reinterpret_cast<LPARAM>(&nmHdr));
                return TRUE;
            }
            int currentTabOn = GetTabIndexAt(xPos, yPos);
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
            //case WM_DRAWITEM:
            //{
            //    DrawItem((DRAWITEMSTRUCT *)lParam);
            //    return TRUE;
            //}
            //break;
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
            int xPos         = GET_X_LPARAM(lParam);
            int yPos         = GET_Y_LPARAM(lParam);
            int currentTabOn = GetTabIndexAt(xPos, yPos);
            NotifyTabDelete(currentTabOn);

            return TRUE;
        }
        case WM_CONTEXTMENU:
        {
            int xPos = GET_X_LPARAM(lParam);
            int yPos = GET_Y_LPARAM(lParam);

            POINT pt{};
            pt.x = xPos;
            pt.y = yPos;

            if (pt.x == -1 && pt.y == -1)
            {
                HRESULT hr;

                // Display the menu in the upper-left corner of the client area, below the ribbon.
                IUIRibbonPtr pRibbon;
                hr = g_pFramework->GetView(0, IID_PPV_ARGS(&pRibbon));
                if (SUCCEEDED(hr))
                {
                    UINT32 uRibbonHeight = 0;
                    hr                   = pRibbon->GetHeight(&uRibbonHeight);
                    if (SUCCEEDED(hr))
                    {
                        pt.x = 0;
                        pt.y = uRibbonHeight;
                        ClientToScreen(hwnd, &pt);
                    }
                }
                if (FAILED(hr))
                {
                    // Default to just the upper-right corner of the entire screen.
                    pt.x = 0;
                    pt.y = 0;
                }
            }

            // The basic pattern of how to show Contextual UI in a specified location.
            IUIContextualUIPtr pContextualUI = nullptr;
            if (SUCCEEDED(g_pFramework->GetView(cmdTabContextMap, IID_PPV_ARGS(&pContextualUI))))
            {
                pContextualUI->ShowAtLocation(pt.x, pt.y);
            }
        }
        break;
    }

    return ::CallWindowProc(m_tabBarDefaultProc, hwnd, message, wParam, lParam);
}

COLORREF CTabBar::GetTabColor(UINT item) const
{
    TBHDR nmHdr{};
    nmHdr.hdr.hwndFrom = *this;
    nmHdr.hdr.code     = TCN_GETCOLOR;
    nmHdr.hdr.idFrom   = reinterpret_cast<UINT_PTR>(this);
    nmHdr.tabOrigin    = item;
    COLORREF clr       = static_cast<COLORREF>(::SendMessage(m_hParent, WM_NOTIFY, 0, reinterpret_cast<LPARAM>(&nmHdr)));
    if (clr == 0 || CTheme::Instance().IsHighContrastMode())
    {
        clr = CTheme::Instance().GetThemeColor(GetSysColor(COLOR_3DFACE));
    }
    return clr;
}

void CTabBar::DrawMainBorder(const LPDRAWITEMSTRUCT lpdis) const
{
    GDIHelpers::FillSolidRect(lpdis->hDC, &lpdis->rcItem, CTheme::Instance().GetThemeColor(::GetSysColor(COLOR_3DFACE)));
}

void CTabBar::DrawItem(const LPDRAWITEMSTRUCT lpDrawItemStruct, float fraction) const
{
    int  curSel    = TabCtrl_GetCurSel(*this);
    bool bSelected = (lpDrawItemStruct->itemID == static_cast<UINT>(curSel));

    RECT rItem(lpDrawItemStruct->rcItem);

    auto crBkgnd = GetTabColor(lpDrawItemStruct->itemID);

    auto crFill = GDIHelpers::InterpolateColors(crBkgnd,
                                                CTheme::Instance().GetThemeColor(RGB(250, 250, 250), true),
                                                max(0.0, fraction - (bSelected ? 0.6 : 0.1)));

    GDIHelpers::FillSolidRect(lpDrawItemStruct->hDC, rItem.left, rItem.top, rItem.right, rItem.bottom, crBkgnd);

    auto borderWidth = CDPIAware::Instance().Scale(*this, 1);
    if (bSelected)
        borderWidth *= 2;
    rItem.left += borderWidth;
    rItem.right -= borderWidth;
    rItem.top += borderWidth;
    if (!bSelected)
        rItem.bottom -= (2 * borderWidth);

    GDIHelpers::FillSolidRect(lpDrawItemStruct->hDC, rItem.left, rItem.top, rItem.right, rItem.bottom, crFill);

    wchar_t buf[256] = {0};
    TC_ITEM tci{};
    tci.mask       = TCIF_TEXT | TCIF_IMAGE;
    tci.pszText    = buf;
    tci.cchTextMax = _countof(buf) - 2;
    TabCtrl_GetItem(*this, lpDrawItemStruct->itemID, &tci);

    COLORREF textColor;
    if (tci.iImage == REDONLY_IMG_INDEX)
    {
        if (bSelected)
            textColor = CTheme::Instance().GetThemeColor(GDIHelpers::Darker(::GetSysColor(COLOR_GRAYTEXT), 0.8f));
        else
            textColor = CTheme::Instance().GetThemeColor(::GetSysColor(COLOR_GRAYTEXT));
    }
    else if (tci.iImage == UNSAVED_IMG_INDEX)
        textColor = CTheme::Instance().GetThemeColor(RGB(100, 0, 0), true);
    else if (bSelected)
        textColor = CTheme::Instance().GetThemeColor(::GetSysColor(COLOR_WINDOWTEXT));
    else
        textColor = CTheme::Instance().GetThemeColor(GDIHelpers::Darker(::GetSysColor(COLOR_3DDKSHADOW), 0.5f));
    SetTextColor(lpDrawItemStruct->hDC, textColor);

    const int PADDING = CDPIAware::Instance().Scale(*this, 2);
    // text & icon
    rItem.left += PADDING;
    rItem.right -= PADDING;
    rItem.bottom = lpDrawItemStruct->rcItem.bottom;
    rItem.top    = lpDrawItemStruct->rcItem.top;

    SetBkMode(lpDrawItemStruct->hDC, TRANSPARENT);

    // draw close/active button
    RECT closeButtonRect = m_closeButtonZone.GetButtonRectFrom(lpDrawItemStruct->rcItem);

    TabButtonType buttonType = TabButtonType::None;
    if (bSelected)
        buttonType = TabButtonType::Close;
    if (tci.iImage == UNSAVED_IMG_INDEX)
        buttonType = TabButtonType::Modified;
    else if (m_currentHoverTabItem == static_cast<int>(lpDrawItemStruct->itemID))
        buttonType = TabButtonType::Close;
    if (m_bIsCloseHover && (m_currentHoverTabItem == static_cast<int>(lpDrawItemStruct->itemID)))
        buttonType = TabButtonType::CloseHover;

    switch (buttonType)
    {
        case TabButtonType::Modified:
        {
            auto oldFont = static_cast<HFONT>(SelectObject(lpDrawItemStruct->hDC, m_hSymbolFont));
            ::DrawText(lpDrawItemStruct->hDC, &m_modifiedChar, 1, &closeButtonRect, DT_SINGLELINE | DT_NOPREFIX | DT_CENTER | DT_VCENTER);
            SelectObject(lpDrawItemStruct->hDC, oldFont);
        }
        break;
        case TabButtonType::Close:
        {
            auto oldFont = static_cast<HFONT>(SelectObject(lpDrawItemStruct->hDC, m_hSymbolFont));
            ::DrawText(lpDrawItemStruct->hDC, &m_closeChar, 1, &closeButtonRect, DT_SINGLELINE | DT_NOPREFIX | DT_CENTER | DT_VCENTER);
            SelectObject(lpDrawItemStruct->hDC, oldFont);
        }
        break;
        case TabButtonType::CloseHover:
        {
            auto oldFont = static_cast<HFONT>(SelectObject(lpDrawItemStruct->hDC, m_hSymbolBoldFont));
            ::DrawText(lpDrawItemStruct->hDC, &m_closeChar, 1, &closeButtonRect, DT_SINGLELINE | DT_NOPREFIX | DT_CENTER | DT_VCENTER);
            SelectObject(lpDrawItemStruct->hDC, oldFont);
        }
        break;
        case TabButtonType::None:
            break;
        case TabButtonType::ClosePush:
            break;
    }
    rItem.right = closeButtonRect.left;

    // icon
#ifdef TABBAR_SHOWDISKICON
    if (hilTabs)
    {
        HIMAGELIST hilTabs = static_cast<HIMAGELIST>(TabCtrl_GetImageList(*this));
        ImageList_Draw(hilTabs, tci.iImage, lpDrawItemStruct->hDC, rItem.left, rItem.top, ILD_TRANSPARENT);
        rItem.left += CDPIAware::Instance().Scale(*this, 16);
    }
#endif
    // text
    rItem.right -= (2 * PADDING);

    rItem.bottom  = lpDrawItemStruct->rcItem.bottom;
    rItem.top     = lpDrawItemStruct->rcItem.top;
    HFONT oldFont = nullptr;
    if (bSelected)
        oldFont = static_cast<HFONT>(SelectObject(lpDrawItemStruct->hDC, m_hBoldFont));
    ::DrawText(lpDrawItemStruct->hDC, buf, -1, &rItem, DT_SINGLELINE | DT_MODIFYSTRING | DT_END_ELLIPSIS | DT_NOPREFIX | DT_CENTER | DT_VCENTER);
    if (bSelected)
        SelectObject(lpDrawItemStruct->hDC, oldFont);
}

void CTabBar::DraggingCursor(POINT screenPoint, UINT item)
{
    HWND hWin = ::WindowFromPoint(screenPoint);
    if (*this == hWin)
        ::SetCursor(::LoadCursor(nullptr, IDC_ARROW));
    else
    {
        wchar_t className[256] = {};
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
            TBHDR nmHdr{};
            nmHdr.hdr.hwndFrom = *this;
            nmHdr.hdr.code     = TCN_GETDROPICON;
            nmHdr.hdr.idFrom   = reinterpret_cast<UINT_PTR>(this);
            nmHdr.tabOrigin    = item;
            HICON icon         = reinterpret_cast<HICON>(::SendMessage(m_hParent, WM_NOTIFY, 0, reinterpret_cast<LPARAM>(&nmHdr)));
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
            TCITEM itemDataNDraggedTab{}, itemDataShift{};
            itemDataNDraggedTab.mask = itemDataShift.mask = TCIF_IMAGE | TCIF_TEXT | TCIF_PARAM;
            const int stringSize                          = 256;
            wchar_t   str1[stringSize]                    = {};
            wchar_t   str2[stringSize]                    = {};

            itemDataNDraggedTab.pszText    = str1;
            itemDataNDraggedTab.cchTextMax = (stringSize);

            itemDataShift.pszText    = str2;
            itemDataShift.cchTextMax = (stringSize);

            bool ok;
            ok = TabCtrl_GetItem(*this, m_nTabDragged, &itemDataNDraggedTab) != FALSE;
            APPVERIFY(ok);

            if (m_nTabDragged > nTab)
            {
                for (int i = m_nTabDragged; i > nTab; i--)
                {
                    ok = TabCtrl_GetItem(*this, i - 1, &itemDataShift) != FALSE;
                    APPVERIFY(ok);
                    ok = TabCtrl_SetItem(*this, i, &itemDataShift) != FALSE;
                    APPVERIFY(ok);
                }
            }
            else
            {
                for (int i = m_nTabDragged; i < nTab; i++)
                {
                    ok = TabCtrl_GetItem(*this, i + 1, &itemDataShift) != FALSE;
                    APPVERIFY(ok);
                    ok = TabCtrl_SetItem(*this, i, &itemDataShift) != FALSE;
                    APPVERIFY(ok);
                }
            }
            ok = TabCtrl_SetItem(*this, nTab, &itemDataNDraggedTab) != FALSE;
            APPVERIFY(ok);

            //3. update the current index
            m_nTabDragged = nTab;

            TBHDR nmHdr{};
            nmHdr.hdr.hwndFrom = *this;
            nmHdr.hdr.code     = TCN_ORDERCHANGED;
            nmHdr.hdr.idFrom   = reinterpret_cast<UINT_PTR>(this);
            nmHdr.tabOrigin    = nTab;
            ::SendMessage(m_hParent, WM_NOTIFY, 0, reinterpret_cast<LPARAM>(&nmHdr));
        }
    }
    else
    {
        m_bIsDraggingInside = false;
    }
}

int CTabBar::GetTabIndexAt(int x, int y) const
{
    TCHITTESTINFO hitInfo{};
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

LRESULT CALLBACK CTabBar::TabBar_Proc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    return (reinterpret_cast<CTabBar *>(::GetWindowLongPtr(hwnd, GWLP_USERDATA))->RunProc(hwnd, message, wParam, lParam));
}

DocID CTabBar::GetIDFromIndex(int index) const
{
    TCITEM tci{};
    tci.mask    = TCIF_PARAM;
    auto result = TabCtrl_GetItem(*this, index, &tci);
    // Easier to set a break point on failed results.
    if (result)
        return DocID(static_cast<int>(tci.lParam));
    else
        return {};
}

int CTabBar::GetIndexFromID(DocID id) const
{
    if (id.IsValid()) // Only look for an id that's legal to begin with.
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
    TBHDR nmHdr{};
    nmHdr.hdr.hwndFrom = *this;
    nmHdr.hdr.code     = TCN_TABDELETE;
    nmHdr.hdr.idFrom   = reinterpret_cast<UINT_PTR>(this);
    nmHdr.tabOrigin    = tab;
    ::SendMessage(m_hParent, WM_NOTIFY, 0, reinterpret_cast<LPARAM>(&nmHdr));
}

bool CloseButtonZone::IsHit(int x, int y, const RECT &testZone) const
{
    if (((x + m_width + m_fromRight) < testZone.right) || (x > (testZone.right - m_fromRight)))
        return false;
    auto fromTop = (testZone.bottom - testZone.top - m_height) / 2;
    if (((y - m_height - fromTop) > testZone.top) || (y < (testZone.top + fromTop)))
        return false;

    return true;
}

RECT CloseButtonZone::GetButtonRectFrom(const RECT &tabItemRect) const
{
    auto fromTop = (tabItemRect.bottom - tabItemRect.top - m_height) / 2;

    RECT rect{};
    rect.right  = tabItemRect.right - m_fromRight;
    rect.left   = rect.right - m_width;
    rect.top    = tabItemRect.top + fromTop;
    rect.bottom = rect.top + m_height;

    return rect;
}
