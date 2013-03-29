#include "stdafx.h"
#include "TabBar.h"
#include "resource.h"

#include <Uxtheme.h>
#include <vsstyle.h>


#pragma comment(lib, "UxTheme.lib")

COLORREF CTabBar::m_activeTextColour = ::GetSysColor(COLOR_BTNTEXT);
COLORREF CTabBar::m_activeTopBarFocusedColour = RGB(250, 170, 60);
COLORREF CTabBar::m_activeTopBarUnfocusedColour = RGB(250, 210, 150);
COLORREF CTabBar::m_inactiveTextColour = RGB(128, 128, 128);
COLORREF CTabBar::m_inactiveBgColour = RGB(192, 192, 192);

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

    CreateEx(0, WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | WS_VISIBLE | TCS_FOCUSNEVER | TCS_TABS | TCS_OWNERDRAWFIXED | WS_TABSTOP, hParent, 0, WC_TABCONTROL);

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
    tie.mask = TCIF_TEXT | TCIF_IMAGE;
    int index = -1;

    if (m_bHasImgList)
        index = 0;
    tie.iImage = index;
    tie.pszText = (TCHAR *)subTabName;
    return int(::SendMessage(*this, TCM_INSERTITEM, m_nItems++, reinterpret_cast<LPARAM>(&tie)));
}

void CTabBar::GetCurrentTitle(TCHAR *title, int titleLen)
{
    TCITEM tci;
    tci.mask = TCIF_TEXT;
    tci.pszText = title;
    tci.cchTextMax = titleLen-1;
    ::SendMessage(*this, TCM_GETITEM, GetCurrentTabIndex(), reinterpret_cast<LPARAM>(&tci));
}

void CTabBar::SetFont(TCHAR *fontName, size_t fontSize)
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
    if (GetCurrentTabIndex() != index)
    {
        ::SendMessage(*this, TCM_SETCURSEL, index, 0);
    }
    TBHDR nmhdr;
    nmhdr.hdr.hwndFrom = *this;
    nmhdr.hdr.code = TCN_SELCHANGE;
    nmhdr.hdr.idFrom = reinterpret_cast<unsigned int>(this);
    nmhdr.tabOrigin = index;
    ::SendMessage(m_hParent, WM_NOTIFY, 0, reinterpret_cast<LPARAM>(&nmhdr));
}

void CTabBar::DeletItemAt(size_t index)
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
    ::SendMessage(m_hwndArray[0], TCM_SETPADDING, 0, MAKELPARAM(6, 0));
    for (int i = 0 ; i < m_nControls ; i++)
    {
        if (m_hwndArray[i])
        {
            ::InvalidateRect(m_hwndArray[i], NULL, TRUE);
            const int base = 6;
            ::SendMessage(m_hwndArray[i], TCM_SETPADDING, 0, MAKELPARAM(base+3, 0));
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
        case WM_RBUTTONDOWN :   //rightclick selects tab aswell
        {
            ::CallWindowProc(m_TabBarDefaultProc, hwnd, WM_LBUTTONDOWN, wParam, lParam);
            return TRUE;
        }
//#define NPPM_INTERNALm_bIsDragging 40926
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
                //::SendMessage(h, NPPM_INTERNALm_bIsDragging, 0, 0);
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
            break;
        }

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
                nmhdr.tabOrigin = currentTabOn;
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

            break;
        }

        case WM_CAPTURECHANGED :
        {
            if (m_bIsDragging)
            {
                m_bIsDragging = false;
                return TRUE;
            }
            break;
        }

        case WM_DRAWITEM :
        {
            DrawItem((DRAWITEMSTRUCT *)lParam);
            return TRUE;
        }

        case WM_KEYDOWN :
        {
            if (wParam == VK_LCONTROL)
                ::SetCursor(::LoadCursor(hResource, MAKEINTRESOURCE(IDC_DRAG_PLUS_TAB)));
            return TRUE;
        }

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
    }
    return ::CallWindowProc(m_TabBarDefaultProc, hwnd, Message, wParam, lParam);
}

void CTabBar::DrawItem(DRAWITEMSTRUCT *pDrawItemStruct)
{
    RECT rect = pDrawItemStruct->rcItem;

    int nTab = pDrawItemStruct->itemID;
    if (nTab < 0)
    {
        ::MessageBox(NULL, TEXT("nTab < 0"), TEXT(""), MB_OK);
        //return ::CallWindowProc(_tabBarDefaultProc, hwnd, Message, wParam, lParam);
    }
    bool isSelected = (nTab == ::SendMessage(*this, TCM_GETCURSEL, 0, 0));

    TCHAR label[MAX_PATH];
    TCITEM tci;
    tci.mask = TCIF_TEXT|TCIF_IMAGE;
    tci.pszText = label;
    tci.cchTextMax = MAX_PATH-1;

    if (!::SendMessage(*this, TCM_GETITEM, nTab, reinterpret_cast<LPARAM>(&tci)))
    {
        ::MessageBox(NULL, TEXT("! TCM_GETITEM"), TEXT(""), MB_OK);
    }
    HDC hDC = pDrawItemStruct->hDC;

    int nSavedDC = ::SaveDC(hDC);

    ::SetBkMode(hDC, TRANSPARENT);
    HBRUSH hBrush = 0;
    HTHEME hTheme = 0;
    if (IsAppThemed())
    {
        hTheme = OpenThemeData(*this, L"Tab");
        
        DrawThemeBackground(hTheme, hDC, TABP_TABITEM, isSelected ? TIS_SELECTED : TIS_NORMAL, &rect, NULL);
    }
    else
    {
        hBrush = ::CreateSolidBrush(::GetSysColor(COLOR_BTNFACE));
        ::FillRect(hDC, &rect, hBrush);
        ::DeleteObject((HGDIOBJ)hBrush);
    }

    TBHDR nmhdr;
    nmhdr.hdr.hwndFrom = *this;
    nmhdr.hdr.code = TCN_GETCOLOR;
    nmhdr.hdr.idFrom = reinterpret_cast<unsigned int>(this);
    nmhdr.tabOrigin = nTab;
    COLORREF clr = (COLORREF)::SendMessage(m_hParent, WM_NOTIFY, 0, reinterpret_cast<LPARAM>(&nmhdr));
    if (clr)
    {
        Gdiplus::Graphics myGraphics(hDC);
        Gdiplus::LinearGradientBrush linGrBrush(Gdiplus::Rect(rect.left, rect.top, rect.right-rect.left, rect.bottom-rect.top), 
                                                Gdiplus::Color(0x99,  GetRValue(clr), GetGValue(clr), GetBValue(clr)),
                                                Gdiplus::Color(0x99/3,  GetRValue(clr), GetGValue(clr), GetBValue(clr)),
                                                Gdiplus::LinearGradientModeVertical);
        myGraphics.FillRectangle(&linGrBrush,rect.left, rect.top, rect.right-rect.left, rect.bottom-rect.top); 
    }

    if (isSelected)
    {
        RECT barRect = rect;

        barRect.bottom = barRect.top + 6;
        rect.top += 2;

        TBHDR nmhdr;
        nmhdr.hdr.hwndFrom = *this;
        nmhdr.hdr.code = TCN_GETFOCUSEDTAB;
        nmhdr.hdr.idFrom = reinterpret_cast<unsigned int>(this);
        nmhdr.tabOrigin = nTab;

        if (::SendMessage(m_hParent, WM_NOTIFY, 0, reinterpret_cast<LPARAM>(&nmhdr)))
            hBrush = ::CreateSolidBrush(m_activeTopBarFocusedColour); // #FAAA3C
        else
            hBrush = ::CreateSolidBrush(m_activeTopBarUnfocusedColour); // #FAD296

        ::FillRect(hDC, &barRect, hBrush);
        ::DeleteObject((HGDIOBJ)hBrush);
    }
    else
    {
        RECT barRect = rect;

        hBrush = ::CreateSolidBrush(m_inactiveBgColour);
        ::FillRect(hDC, &barRect, hBrush);
        ::DeleteObject((HGDIOBJ)hBrush);
    }

    RECT closeButtonRect = m_closeButtonZone.GetButtonRectFrom(rect);
    if (isSelected)
    {
        closeButtonRect.left -= 2;
    }


    // 3 status for each inactive tab and selected tab close item :
    // normal / hover / pushed
    int idCloseImg;

    if (m_bIsCloseHover && (m_currentHoverTabItem == nTab) && (m_whichCloseClickDown == -1)) // hover
        idCloseImg = IDR_CLOSETAB_HOVER;
    else if (m_bIsCloseHover && (m_currentHoverTabItem == nTab) && (m_whichCloseClickDown == m_currentHoverTabItem)) // pushed
        idCloseImg = IDR_CLOSETAB_PUSH;
    else
        idCloseImg = isSelected?IDR_CLOSETAB:IDR_CLOSETAB_INACT;


    HDC hdcMemory;
    hdcMemory = ::CreateCompatibleDC(hDC);
    HBITMAP hBmp = ::LoadBitmap(hResource, MAKEINTRESOURCE(idCloseImg));
    BITMAP bmp;
    ::GetObject(hBmp, sizeof(bmp), &bmp);

    rect.right = closeButtonRect.left;

    ::SelectObject(hdcMemory, hBmp);
    ::BitBlt(hDC, closeButtonRect.left, closeButtonRect.top, bmp.bmWidth, bmp.bmHeight, hdcMemory, 0, 0, SRCCOPY);
    ::DeleteDC(hdcMemory);
    ::DeleteObject(hBmp);

    // Draw image
    HIMAGELIST hImgLst = (HIMAGELIST)::SendMessage(*this, TCM_GETIMAGELIST, 0, 0);

    SIZE charPixel;
    ::GetTextExtentPoint(hDC, TEXT(" "), 1, &charPixel);
    int spaceUnit = charPixel.cx;

    if (hImgLst && tci.iImage >= 0)
    {
        IMAGEINFO info;
        int yPos = 0;
        int marge = 0;

        ImageList_GetImageInfo(hImgLst, tci.iImage, &info);

        RECT & imageRect = info.rcImage;

        yPos = (rect.top + (rect.bottom - rect.top)/2 + (isSelected?0:2)) - (imageRect.bottom - imageRect.top)/2;

        if (isSelected)
            marge = spaceUnit*2;
        else
            marge = spaceUnit;

        rect.left += marge;
        ImageList_Draw(hImgLst, tci.iImage, hDC, rect.left, yPos, isSelected?ILD_TRANSPARENT:ILD_SELECTED);
        rect.left += imageRect.right - imageRect.left;
    }

    SelectObject(hDC, m_hFont);

    int Flags = DT_SINGLELINE;

    Flags |= DT_LEFT;

    // the following uses pixel values the fix alignments issues with DrawText
    // and font's that are rotated 90 degrees
    if (isSelected)
    {
        ::SetTextColor(hDC, m_activeTextColour);

        rect.top -= ::GetSystemMetrics(SM_CYEDGE);
        rect.top += 3;
        rect.left += spaceUnit;

        Flags |= DT_VCENTER;
    }
    else
    {
        ::SetTextColor(hDC, m_inactiveTextColour);
        rect.left   += spaceUnit;

        Flags |= DT_BOTTOM;
    }
    if (hTheme)
        DrawThemeText(hTheme, hDC, TABP_TABITEM, isSelected ? TIS_SELECTED : TIS_NORMAL, label, lstrlen(label), Flags, 0, &rect);
    else
        ::DrawText(hDC, label, lstrlen(label), &rect, Flags);
    ::RestoreDC(hDC, nSavedDC);
    if (hTheme)
        CloseThemeData(hTheme);
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
        //::SetCursor(::LoadCursor(hResource, MAKEINTRESOURCE(IDC_DRAG_TAB)));
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