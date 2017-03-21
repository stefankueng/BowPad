﻿// This file is part of BowPad.
//
// Copyright (C) 2013-2017 - Stefan Kueng
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
#include "GDIHelpers.h"

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
const double hoverFraction = 0.6;

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

    ::SetWindowLongPtr(*this, GWLP_USERDATA, (LONG_PTR)this);
    m_TabBarDefaultProc = reinterpret_cast<WNDPROC>(::SetWindowLongPtr(*this, GWLP_WNDPROC, (LONG_PTR)TabBar_Proc));

    m_hFont = (HFONT)::SendMessage(*this, WM_GETFONT, 0, 0);

    if (m_hFont == nullptr)
    {
        NONCLIENTMETRICS ncm;
        ncm.cbSize = sizeof(NONCLIENTMETRICS);
        SystemParametersInfo(SPI_GETNONCLIENTMETRICS, sizeof(NONCLIENTMETRICS), &ncm, 0U);
        m_hFont = CreateFontIndirect(&ncm.lfSmCaptionFont);
        ::SendMessage(*this, WM_SETFONT, reinterpret_cast<WPARAM>(m_hFont), 0);
    }

    TabCtrl_SetMinTabWidth(*this, LPARAM(GetSystemMetrics(SM_CXSMICON) * m_dpiScaleX * 4.0));
    m_closeButtonZone.SetDPIScale(m_dpiScaleX);

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
    int imageIndex = -1;

    if (m_bHasImgList)
        imageIndex = 0;
    tie.iImage = TABBAR_SHOWDISKICON ? imageIndex : 0;
    tie.pszText = const_cast<TCHAR *>(subTabName);
    m_animVars[m_tabID] = Animator::Instance().CreateAnimationVariable(1.0);
    tie.lParam = m_tabID++;
    int index = TabCtrl_InsertItem(*this, m_nItems++, &tie);
    // if this is the first item added to the tab bar, it is automatically
    // selected but without the proper notifications.
    // remove the selection so it can be selected properly later.
    if (m_nItems == 1)
        TabCtrl_SetCurSel(*this, -1);
    return index;
}

int CTabBar::InsertAfter(int index, const TCHAR *subTabName)
{
    TCITEM tie;
    tie.mask = TCIF_TEXT | TCIF_IMAGE | TCIF_PARAM;
    int imgindex = -1;

    if (m_bHasImgList)
        imgindex = 0;
    tie.iImage = TABBAR_SHOWDISKICON ? imgindex : 0;
    tie.pszText = const_cast<TCHAR *>(subTabName);
    m_animVars[m_tabID] = Animator::Instance().CreateAnimationVariable(1.0);
    tie.lParam = m_tabID++;
    if ((index + 1) >= m_nItems)
        index = m_nItems - 1;
    int ret = TabCtrl_InsertItem(*this, index + 1, &tie);
    // if this is the first item added to the tab bar, it is automatically
    // selected but without the proper notifications.
    // remove the selection so it can be selected properly later.
    if (m_nItems == 0)
        TabCtrl_SetCurSel(*this, -1);
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
    // Decrement the item count before calling delete as this delete
    // seems to initiate events (like paint) that will assume the extra
    // item is present if it's not already accounted as gone before then,
    // even though it is gone, but the delete itself is initiating the actions.
    // Failure to decrement the count first will show up as assertion
    // failures in calls to GetItemRect arond line 320 & 530 in the
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
        RECT itemRect;
        TabCtrl_GetItemRect(*this, m_nItems - 1, &itemRect);
        RECT tabRect;
        GetClientRect(*this, &tabRect);
        TC_HITTESTINFO hti;
        hti.pt = { 14, 14 }; // arbitrary value: just inside the first visible tab
        LRESULT scrollTabIndex = ::SendMessage(*this, TCM_HITTEST, 0, (LPARAM)&hti);
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
            return{};
    }
    TCITEM tci;
    tci.mask = TCIF_PARAM;
    BOOL result = TabCtrl_GetItem(*this, index, &tci);
    if (!result)
    {
        assert(false);
        return{};
    }
    return DocID((int)tci.lParam);
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
    TCITEM tci;
    tci.mask = TCIF_PARAM;
    tci.lParam = id.GetValue();
    TabCtrl_SetItem(*this, index, &tci);
}

void CTabBar::DeletAllItems()
{
    TabCtrl_DeleteAllItems(*this);
    m_nItems = 0;
}

HIMAGELIST CTabBar::SetImageList(HIMAGELIST himl)
{
    m_bHasImgList = (himl != nullptr);
    return TabCtrl_SetImageList(*this, himl);
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

            ::CallWindowProc(m_TabBarDefaultProc, hwnd, Message, wParam, lParam);

            // calc total tab width
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
            auto count = GetItemCount();
            if (!count) // no pages added
            {
                SelectObject(hDC, hOldFont);
                EndPaint(*this, &ps);
                return 0;
            }

            auto nSel = TabCtrl_GetCurSel(*this);

            for (int nTab = count-1; nTab >= 0; --nTab)
            {
                if (nTab != nSel)
                {
                    dis.itemID = nTab;
                    dis.itemState = 0;
                    bool got = TabCtrl_GetItemRect(*this, nTab, &dis.rcItem) != FALSE;
                    APPVERIFY(got);
                    if (got)
                    {
                        dis.rcItem.bottom -= 2;
                        DrawItem(&dis, (float)Animator::GetValue(m_animVars[GetIDFromIndex(nTab).GetValue()]));
                        DrawItemBorder(&dis);
                    }
                }
            }

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
                    DrawItem(&dis, (float)Animator::GetValue(m_animVars[GetIDFromIndex(nSel).GetValue()]));
                    DrawItemBorder(&dis);
                }
            }

            SelectObject(hDC, hOldFont);
            EndPaint(*this, &ps);
        }
            break;
        case WM_MOUSEWHEEL:
        {
            // Scroll or move tabs:
            // Mousewheel: scrolls the tab bar area like Firefox does it. Only works if at least one tab is hidden.
            // Ctrl+Mousewheel: moves the current tab left/right, loops around at the beginning/end
            // Shift+Mousewheel: moves the current tab left/right, stops at the beginning/end
            // Ctrl+Shift+Mousewheel: switch current tab to the first/last tab

            if (m_bIsDragging)
                return TRUE;

            const bool isForward = ((short)HIWORD(wParam)) < 0; // wheel down: forward, wheel up: backward
            const LRESULT lastTabIndex = ::SendMessage(*this, TCM_GETITEMCOUNT, 0, 0) - 1;

            if ((wParam & MK_CONTROL) && (wParam & MK_SHIFT))
            {
                ::SendMessage(*this, TCM_SETCURFOCUS, (isForward ? lastTabIndex : 0), 0);
            }
            else if (wParam & (MK_CONTROL | MK_SHIFT))
            {
                LRESULT tabIndex = ::SendMessage(*this, TCM_GETCURSEL, 0, 0) + (isForward ? 1 : -1);
                if (tabIndex < 0)
                {
                    if (wParam & MK_CONTROL)
                        tabIndex = lastTabIndex; // wrap scrolling
                    else
                        return TRUE;
                }
                else if (tabIndex > lastTabIndex)
                {
                    if (wParam & MK_CONTROL)
                        tabIndex = 0; // wrap scrolling
                    else
                        return TRUE;
                }
                ::SendMessage(*this, TCM_SETCURFOCUS, tabIndex, 0);
            }
            else
            {
                RECT rcTabCtrl, rcLastTab;
                ::SendMessage(*this, TCM_GETITEMRECT, lastTabIndex, (LPARAM)&rcLastTab);
                ::GetClientRect(*this, &rcTabCtrl);

                // get index of the first visible tab
                TC_HITTESTINFO hti;
                LONG xy = 14; // arbitrary value: just inside the first tab
                hti.pt = { xy, xy };
                LRESULT scrollTabIndex = ::SendMessage(*this, TCM_HITTEST, 0, (LPARAM)&hti);

                if (scrollTabIndex < 1 && (rcLastTab.right < rcTabCtrl.right)) // nothing to scroll
                    return TRUE;

                // maximal width/height of the msctls_updown32 class (arrow box in the tab bar), 
                // this area may hide parts of the last tab and needs to be excluded
                LONG maxLengthUpDownCtrl = 45; // sufficient static value

                // scroll forward as long as the last tab is hidden; scroll backward till the first tab
                if ((((rcTabCtrl.right - rcLastTab.right) < maxLengthUpDownCtrl)) || !isForward)
                {
                    if (isForward)
                        ++scrollTabIndex;
                    else
                        --scrollTabIndex;

                    if (scrollTabIndex < 0 || scrollTabIndex > lastTabIndex)
                        return TRUE;

                    // clear hover state of the close button,
                    // WM_MOUSEMOVE won't handle this properly since the tab position will change
                    if (m_bIsCloseHover)
                    {
                        m_bIsCloseHover = false;
                        ::InvalidateRect(*this, &m_currentHoverTabRect, false);
                    }

                    ::SendMessage(*this, WM_HSCROLL, MAKEWPARAM(SB_THUMBPOSITION, scrollTabIndex), 0);
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
                if (m_currentHoverTabItem != oldIndex)
                {
                    if (oldIndex != -1)
                    {
                        auto transHot = Animator::Instance().CreateLinearTransition(0.3, 1.0);
                        auto storyBoard = Animator::Instance().CreateStoryBoard();
                        storyBoard->AddTransition(m_animVars[GetIDFromIndex(oldIndex).GetValue()], transHot);
                        Animator::Instance().RunStoryBoard(storyBoard, [hwnd,oldRect]()
                        {
                            InvalidateRect(hwnd, &oldRect, FALSE);
                        });
                    }
                    auto transHot = Animator::Instance().CreateLinearTransition(0.1, hoverFraction);
                    auto storyBoard = Animator::Instance().CreateStoryBoard();
                    storyBoard->AddTransition(m_animVars[GetIDFromIndex(m_currentHoverTabItem).GetValue()], transHot);
                    Animator::Instance().RunStoryBoard(storyBoard, [hwnd, this]()
                    {
                        InvalidateRect(hwnd, &m_currentHoverTabRect, FALSE);
                    });
                }
            }
            else if (m_currentHoverTabItem != index)
            {
                auto transHot = Animator::Instance().CreateLinearTransition(0.3, 1.0);
                auto storyBoard = Animator::Instance().CreateStoryBoard();
                storyBoard->AddTransition(m_animVars[GetIDFromIndex(m_currentHoverTabItem).GetValue()], transHot);
                auto animRect = m_currentHoverTabRect;
                Animator::Instance().RunStoryBoard(storyBoard, [hwnd, animRect]()
                {
                    InvalidateRect(hwnd, &animRect, FALSE);
                });
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
            auto transHot = Animator::Instance().CreateLinearTransition(0.3, 1.0);
            auto storyBoard = Animator::Instance().CreateStoryBoard();
            storyBoard->AddTransition(m_animVars[GetIDFromIndex(m_currentHoverTabItem).GetValue()], transHot);
            auto oldRect = m_currentHoverTabRect;
            Animator::Instance().RunStoryBoard(storyBoard, [hwnd, oldRect]()
            {
                InvalidateRect(hwnd, &oldRect, FALSE);
            });
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
                TBHDR nmhdr;
                nmhdr.hdr.hwndFrom = *this;
                nmhdr.hdr.code = m_bIsDraggingInside ? TCN_TABDROPPED : TCN_TABDROPPEDOUTSIDE;
                nmhdr.hdr.idFrom = reinterpret_cast<UINT_PTR>(this);
                nmhdr.tabOrigin = m_nTabDragged;
                ::SendMessage(m_hParent, WM_NOTIFY, 0, reinterpret_cast<LPARAM>(&nmhdr));
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
                IUIRibbonPtr pRibbon;
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

    return ::CallWindowProc(m_TabBarDefaultProc, hwnd, Message, wParam, lParam);
}

COLORREF CTabBar::GetTabColor(bool bSelected, UINT item) const
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
        return GDIHelpers::Lighter(clr, lighterfactor);
    }
    return GDIHelpers::Darker(clr, 0.9f);
}

void CTabBar::DrawItemBorder(const LPDRAWITEMSTRUCT lpdis) const
{
    int curSel = TabCtrl_GetCurSel(*this);
    bool bSelected = (lpdis->itemID == (UINT)curSel);

    RECT rItem(lpdis->rcItem);

    COLORREF crTab = GetTabColor(bSelected, lpdis->itemID);
    COLORREF crHighlight = GDIHelpers::Lighter(crTab, 1.5f);
    COLORREF crShadow = GDIHelpers::Darker(crTab, 0.75f);

    rItem.bottom += bSelected ? -1 : 1;

    // edges

    GDIHelpers::FillSolidRect(lpdis->hDC, rItem.left, rItem.top, rItem.left + 1, rItem.bottom, crHighlight);
    GDIHelpers::FillSolidRect(lpdis->hDC, rItem.left, rItem.top, rItem.right, rItem.top + 1, crHighlight);
    GDIHelpers::FillSolidRect(lpdis->hDC, rItem.right - 1, rItem.top, rItem.right, rItem.bottom, crShadow);
}

void CTabBar::DrawMainBorder(const LPDRAWITEMSTRUCT lpdis) const
{
    GDIHelpers::FillSolidRect(lpdis->hDC, &lpdis->rcItem, CTheme::Instance().GetThemeColor(::GetSysColor(COLOR_3DFACE)));
}

void CTabBar::DrawItem(const LPDRAWITEMSTRUCT pDrawItemStruct, float fraction) const
{
    HIMAGELIST hilTabs = (HIMAGELIST)TabCtrl_GetImageList(*this);

    int curSel = TabCtrl_GetCurSel(*this);
    bool bSelected = (pDrawItemStruct->itemID == (UINT)curSel);

    RECT rItem(pDrawItemStruct->rcItem);

    if (bSelected)
        rItem.bottom -= 1;
    else
        rItem.bottom += 2;

    // tab
    // blend from back color to COLOR_3DFACE if 16 bit mode or better
    COLORREF crFrom = GetTabColor(bSelected, pDrawItemStruct->itemID);

    COLORREF crTo = bSelected ? ::GetSysColor(COLOR_3DFACE) : GDIHelpers::Darker(::GetSysColor(COLOR_3DFACE), 0.95f);
    crTo = GDIHelpers::Darker(crTo, fraction);

    crTo = CTheme::Instance().GetThemeColor(crTo);

    const int nROrg = GetRValue(crFrom);
    const int nGOrg = GetGValue(crFrom);
    const int nBOrg = GetBValue(crFrom);
    const int nRDiff = GetRValue(crTo) - nROrg;
    const int nGDiff = GetGValue(crTo) - nGOrg;
    const int nBDiff = GetBValue(crTo) - nBOrg;

    int nHeight = rItem.bottom - rItem.top;

    for (int nLine = 0; nLine < nHeight; nLine += 2)
    {
        int nRed = nROrg + (nLine * nRDiff) / nHeight;
        int nGreen = nGOrg + (nLine * nGDiff) / nHeight;
        int nBlue = nBOrg + (nLine * nBDiff) / nHeight;

        GDIHelpers::FillSolidRect(pDrawItemStruct->hDC, rItem.left, rItem.top + nLine, rItem.right, rItem.top + nLine + 2, RGB(nRed, nGreen, nBlue));
    }
    wchar_t buf[100] = { 0 };
    TC_ITEM tci;
    tci.mask = TCIF_TEXT | TCIF_IMAGE;
    tci.pszText = buf;
    tci.cchTextMax = _countof(buf) - 1;
    TabCtrl_GetItem(*this, pDrawItemStruct->itemID, &tci);
    if (bSelected)
    {
        // draw a line at the bottom indicating the active tab:
        // green if the tab is not modified, red if it is modified and needs saving
        COLORREF indicColor;
        if (tci.iImage == REDONLY_IMG_INDEX)
            indicColor = RGB(80, 80, 80);
        else if (tci.iImage == UNSAVED_IMG_INDEX)
            indicColor = RGB(200, 0, 0);
        else
            indicColor = RGB(0, 200, 0);
        GDIHelpers::FillSolidRect(pDrawItemStruct->hDC, rItem.left, rItem.bottom - 5, rItem.right, rItem.bottom,
                                  CTheme::Instance().GetThemeColor(indicColor));
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
    HBITMAP hBmp = (HBITMAP)::LoadImage(hResource, MAKEINTRESOURCE(idCloseImg), IMAGE_BITMAP, int(11 * m_dpiScaleX), int(11 * m_dpiScaleY), 0);
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
    COLORREF textColor;
    if (tci.iImage == REDONLY_IMG_INDEX)
    {
        if (bSelected)
            textColor = CTheme::Instance().GetThemeColor(GDIHelpers::Darker(::GetSysColor(COLOR_GRAYTEXT), 0.8f));
        else
            textColor = CTheme::Instance().GetThemeColor(::GetSysColor(COLOR_GRAYTEXT));
    }
    else if (tci.iImage == UNSAVED_IMG_INDEX)
        textColor = CTheme::Instance().GetThemeColor(RGB(100, 0, 0));
    else if (bSelected)
        textColor = CTheme::Instance().GetThemeColor(::GetSysColor(COLOR_WINDOWTEXT));
    else
        textColor = CTheme::Instance().GetThemeColor(GDIHelpers::Darker(::GetSysColor(COLOR_3DDKSHADOW), 0.5f));
    SetTextColor(pDrawItemStruct->hDC, textColor);
    DrawText(pDrawItemStruct->hDC, buf, -1, &rItem, uFlags);
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

DocID CTabBar::GetIDFromIndex(int index) const
{
    TCITEM tci;
    tci.mask = TCIF_PARAM;
    auto result = TabCtrl_GetItem(*this, index, &tci);
    // Easier to set a break point on failed results.
    if (result)
        return DocID((int)tci.lParam);
    else
        return{};
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
