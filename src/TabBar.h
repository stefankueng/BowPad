// This file is part of BowPad.
//
// Copyright (C) 2013-2018, 2020 - Stefan Kueng
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
#include "DocumentManager.h"
#include "AnimationManager.h"

#define TCN_TABDROPPED                  (WM_USER + 1)
#define TCN_TABDROPPEDOUTSIDE           (WM_USER + 2)
#define TCN_TABDELETE                   (WM_USER + 3)
#define TCN_GETFOCUSEDTAB               (WM_USER + 4)
#define TCN_ORDERCHANGED                (WM_USER + 5)
#define TCN_REFRESH                     (WM_USER + 6)
#define TCN_GETCOLOR                    (WM_USER + 7)
#define TCN_GETDROPICON                 (WM_USER + 8)

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
    CloseButtonZone(): m_width(11), m_height(11), m_fromRight(7){};
    bool IsHit(int x, int y, const RECT & testZone) const;
    RECT GetButtonRectFrom(const RECT & tabItemRect) const;
    void SetDPIScale(float scale) { m_width = int(m_width * scale); m_height = int(m_height * scale); m_fromRight = int(m_fromRight * scale); }

    int m_width;
    int m_height;
    int m_fromRight;    // distance from right in pixel
};

enum class TabButtonType
{
    None,
    Selected,
    Close,
    CloseHover,
    ClosePush
};

const int nbCtrlMax = 10;

class CTabBar : public CWindow
{
public :
    CTabBar(HINSTANCE hInst);
    virtual ~CTabBar();

    enum tabColourIndex
    {
        activeText, activeFocusedTop, activeUnfocusedTop, inactiveText, inactiveBg
    };

    bool                        Init(HINSTANCE hInst, HWND hParent);
    LRESULT CALLBACK            WinMsgHandler(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) override;
    int                         InsertAtEnd(const TCHAR *subTabName);
    int                         InsertAfter(int index, const TCHAR *subTabName);
    inline void                        SelectChanging() const;
    inline void                        SelectChange(int index) const;
    void                        ActivateAt(int index) const;
    void                        GetTitle(int index, TCHAR *title, int titleLen) const;
    std::wstring                GetTitle(int index) const;
    void                        GetCurrentTitle(TCHAR *title, int titleLen) const;
    std::wstring                GetCurrentTitle() const;
    void                        SetCurrentTitle(LPCTSTR title);
    void                        SetTitle(int index, LPCTSTR title);
    int                         GetCurrentTabIndex() const;
    DocID                       GetCurrentTabId() const;
    void                        SetCurrentTabId(DocID id);
    void                        DeleteItemAt(int index);
    DocID                       GetIDFromIndex(int index) const;
    int                         GetIndexFromID(DocID id) const;
    void                        DeletAllItems();
    HIMAGELIST                  SetImageList(HIMAGELIST himl);
    int                         GetItemCount() const { return m_nItems; }
    void                        SetFont(const TCHAR *fontName, int fontSize);
    int                         GetSrcTab() const { return m_nSrcTab; }
    int                         GetDstTab() const { return m_nTabDragged; }
protected:
    static LRESULT CALLBACK     TabBar_Proc(HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam);
    void                        ExchangeItemData(POINT point);
    LRESULT                     RunProc(HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam);
    COLORREF                    GetTabColor(bool bSelected, UINT item) const;
    void                        DrawMainBorder(const LPDRAWITEMSTRUCT lpDrawItemStruct) const;
    void                        DrawItem(const LPDRAWITEMSTRUCT lpDrawItemStruct, float fraction) const;
    void                        DraggingCursor(POINT screenPoint, UINT item);
    int                         GetTabIndexAt(const POINT & p) const { return GetTabIndexAt(p.x, p.y); }
    int                         GetTabIndexAt(int x, int y) const;
    bool                        IsPointInParentZone(POINT screenPoint) const;
    void                        NotifyTabDelete(int tab);

private:
    int                         m_nItems;
    bool                        m_bHasImgList;
    HFONT                       m_hFont;
    HFONT                       m_hBoldFont;
    HFONT                       m_hSymbolFont;
    HFONT                       m_hSymbolBoldFont;
    int                         m_tabID;

    int                         m_ctrlID;

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
    wchar_t                     m_closeChar;
    wchar_t                     m_activeChar;

    std::map<int, IUIAnimationVariablePtr>  m_animVars;

    ULONG_PTR                   gdiplusToken;
};
