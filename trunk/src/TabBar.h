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

    virtual LRESULT CALLBACK    WinMsgHandler( HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam );

    int                         InsertAtEnd(const TCHAR *subTabName);
    void                        ActivateAt(int index) const;
    void                        GetCurrentTitle(TCHAR *title, int titleLen);
    int                         GetCurrentTabIndex() const;
    void                        DeletItemAt(size_t index);

    void                        DeletAllItems();

    void                        SetImageList(HIMAGELIST himl);

    int                         GetItemCount() const { return (int)m_nItems; }

    void                        SetFont(TCHAR *fontName, size_t fontSize);
    int                         GetSrcTab() const { return m_nSrcTab; }
    int                         GetDstTab() const { return m_nTabDragged; }
protected:
    static void                 DoOwnerDrawTab();

    static void                 SetColour(COLORREF colour2Set, tabColourIndex i);

    static LRESULT CALLBACK     TabBar_Proc(HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam);
    void                        ExchangeItemData(POINT point);
    LRESULT                     RunProc(HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam);

    COLORREF                    GetTabColor(bool bSelected);
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

