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
#pragma once
#include "BaseWindow.h"
#include <Commctrl.h>
#include <gdiplus.h>

#define TCN_TABDROPPED                  (WM_USER + 1)
#define TCN_TABDROPPEDOUTSIDE           (WM_USER + 2)
#define TCN_TABDELETE                   (WM_USER + 3)
#define TCN_GETFOCUSEDTAB               (WM_USER + 4)
#define TCN_ORDERCHANGED                (WM_USER + 5)
#define TCN_REFRESH                     (WM_USER + 6)
#define TCN_GETCOLOR                    (WM_USER + 7)

const int SAVED_IMG_INDEX = 0;
const int UNSAVED_IMG_INDEX = 1;
const int REDONLY_IMG_INDEX = 2;

struct TBHDR
{
    NMHDR hdr;
    int tabOrigin;
};

struct CloseButtonZone
{
    CloseButtonZone(): m_width(11), m_height(11), m_fromTop(3), m_fromRight(3){};
    bool IsHit(int x, int y, const RECT & testZone) const;
    RECT GetButtonRectFrom(const RECT & tabItemRect) const;
    void SetDPIScale(float scale) { m_width = int(m_width * scale); m_height = int(m_height * scale); m_fromRight = int(m_fromRight * scale); m_fromTop = int (m_fromTop * scale); }

    int m_width;
    int m_height;
    int m_fromTop;      // distance from top in pixel
    int m_fromRight;    // distance from right in pixel
};

const int nbCtrlMax = 10;

class CTabBar : public CWindow
{
public :
    CTabBar(HINSTANCE hInst)
        : CWindow(hInst)
        , m_nItems(0)
        , m_bHasImgList(false)
        , m_hFont(NULL)
        , m_ctrlID(-1)
        , m_bIsDragging(false)
        , m_bIsDraggingInside(false)
        , m_nSrcTab(-1)
        , m_nTabDragged(-1)
        , m_TabBarDefaultProc(NULL)
        , m_currentHoverTabItem(-1)
        , m_bIsCloseHover(false)
        , m_whichCloseClickDown(-1)
        , m_lmbdHit(false)
        , m_tabID(0)
    {
        Gdiplus::GdiplusStartupInput gdiplusStartupInput;
        Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);
    };
    virtual ~CTabBar();

    enum tabColourIndex
    {
        activeText, activeFocusedTop, activeUnfocusedTop, inactiveText, inactiveBg
    };

    bool                        Init(HINSTANCE hInst, HWND hParent);

    virtual LRESULT CALLBACK    WinMsgHandler(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) override;

    int                         InsertAtEnd(const TCHAR *subTabName);
    void                        ActivateAt(int index) const;
    void                        GetTitle(int index, TCHAR *title, int titleLen);
    void                        GetCurrentTitle(TCHAR *title, int titleLen);
    void                        SetCurrentTitle(LPCTSTR title);
    int                         GetCurrentTabIndex() const;
    int                         GetCurrentTabId() const;
    void                        DeletItemAt(int index);
    int                         GetIDFromIndex(int index);
    int                         GetIndexFromID(int id);

    void                        DeletAllItems();

    void                        SetImageList(HIMAGELIST himl);

    int                         GetItemCount() const { return (int)m_nItems; }

    void                        SetFont(TCHAR *fontName, int fontSize);
    int                         GetSrcTab() const { return m_nSrcTab; }
    int                         GetDstTab() const { return m_nTabDragged; }
protected:
    void                        DoOwnerDrawTab();

    void                        SetColour(COLORREF colour2Set, tabColourIndex i);

    static LRESULT CALLBACK     TabBar_Proc(HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam);
    void                        ExchangeItemData(POINT point);
    LRESULT                     RunProc(HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam);

    COLORREF                    GetTabColor(bool bSelected, UINT item);
    void                        DrawMainBorder(LPDRAWITEMSTRUCT lpDrawItemStruct);
    void                        DrawItem(LPDRAWITEMSTRUCT lpDrawItemStruct);
    void                        DrawItemBorder(LPDRAWITEMSTRUCT lpDrawItemStruct);

    void                        DraggingCursor(POINT screenPoint);

    int                         GetTabIndexAt(const POINT & p) const { return GetTabIndexAt(p.x, p.y); }

    int                         GetTabIndexAt(int x, int y) const;

    bool                        IsPointInParentZone(POINT screenPoint) const;
private:
    int                         m_nItems;
    bool                        m_bHasImgList;
    HFONT                       m_hFont;
    int                         m_tabID;


    int                         m_ctrlID;

    static bool                 m_bDoDragNDrop;
    bool                        m_bIsDragging;
    bool                        m_bIsDraggingInside;
    int                         m_nSrcTab;
    int                         m_nTabDragged;
    POINT                       m_draggingPoint; // screen coordinates
    WNDPROC                     m_TabBarDefaultProc;

    RECT                        m_currentHoverTabRect;
    int                         m_currentHoverTabItem;

    CloseButtonZone             m_closeButtonZone;
    bool                        m_bIsCloseHover;
    int                         m_whichCloseClickDown;
    bool                        m_lmbdHit; // Left Mouse Button Down Hit

    static COLORREF             m_activeTextColour;
    static COLORREF             m_activeTopBarFocusedColour;
    static COLORREF             m_activeTopBarUnfocusedColour;
    static COLORREF             m_inactiveTextColour;
    static COLORREF             m_inactiveBgColour;

    static int                  m_nControls;
    static HWND                 m_hwndArray[nbCtrlMax];

    ULONG_PTR                   gdiplusToken;

};