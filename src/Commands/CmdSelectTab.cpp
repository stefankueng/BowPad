// This file is part of BowPad.
//
// Copyright (C) 2021 - Stefan Kueng
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
#include "CmdSelectTab.h"
#include "OnOutOfScope.h"
#include "Theme.h"
#include "GDIHelpers.h"
#include "DPIAware.h"
#include "resource.h"
#include "BowPad.h"

bool CCmdSelectTab::Execute()
{
    // since this is a 'dummy' command, only executed via keyboard shortcuts
    // Ctrl+1 ... Ctrl+9
    // we have to find out here which key was pressed and activate the
    // corresponding tab
    auto curTab = GetActiveTabIndex();
    for (int key = 1; key < 9; ++key)
    {
        if ((GetKeyState(key + '0') & 0x8000) || (GetKeyState(key + VK_NUMPAD0) & 0x8000))
        {
            if (GetTabCount() >= key)
            {
                if (key - 1 != curTab)
                    TabActivateAt(key - 1);
                return true;
            }
        }
    }

    if ((GetKeyState('9') & 0x8000) || (GetKeyState(VK_NUMPAD9) & 0x8000))
    {
        auto lastTab = GetTabCount() - 1;
        if (curTab != lastTab)
            TabActivateAt(lastTab);
        return true;
    }
    if (GetKeyState(VK_TAB) & 0x8000)
    {
        std::deque<std::tuple<std::wstring, DocID>> tablist;
        for (const auto& docId : m_docIds)
        {
            auto title = GetTitleForDocID(docId);
            tablist.push_back(std::make_tuple(title, docId));
        }
        if (!m_dlg)
            m_dlg = std::make_unique<TabListDialog>(GetScintillaWnd(),
                                                    [this](DocID id) { TabActivateAt(GetTabIndexFromDocID(id)); });

        auto sz = m_dlg->SetTabList(std::move(tablist));
        RECT rect{};
        GetClientRect(GetScintillaWnd(), &rect);
        sz.cx = min(sz.cx, rect.right - rect.left);
        sz.cy = min(sz.cy, rect.bottom - rect.top - 40);
        RECT wndRect{};
        wndRect.left   = (rect.right - rect.left) / 2 - sz.cx / 2;
        wndRect.right  = wndRect.left + sz.cx;
        wndRect.top    = rect.top + 20;
        wndRect.bottom = wndRect.top + sz.cy;

        m_dlg->ShowModeless(g_hRes, IDD_EMPTYDLG, GetScintillaWnd(), false);

        MapWindowPoints(GetScintillaWnd(), nullptr, (LPPOINT)&wndRect, 2);

        SetWindowPos(*m_dlg, nullptr,
                     wndRect.left, wndRect.top, wndRect.right - wndRect.left, wndRect.bottom - wndRect.top,
                     SWP_NOOWNERZORDER | SWP_NOZORDER | SWP_SHOWWINDOW | SWP_NOCOPYBITS);
        SetFocus(*m_dlg);
        return true;
    }

    return true;
}

void CCmdSelectTab::TabNotify(TBHDR* ptbhdr)
{
    if (ptbhdr->hdr.code == TCN_SELCHANGE)
    {
        DocID docId   = GetDocIDFromTabIndex(GetActiveTabIndex());
        auto  foundIt = std::find(m_docIds.begin(), m_docIds.end(), docId);
        if (foundIt == m_docIds.end())
            m_docIds.push_front(docId);
        else
        {
            m_docIds.erase(foundIt);
            m_docIds.push_front(docId);
        }
    }
}

TabListDialog::TabListDialog(HWND hParent, std::function<void(DocID)>&& execFunc)
    : m_hParent(hParent)
    , m_textHeight(12)
    , m_hFont(nullptr)
    , m_currentItem(0)
    , m_execFunction(execFunc)
{
    NONCLIENTMETRICS ncm{};
    ncm.cbSize = sizeof(NONCLIENTMETRICS);
    SystemParametersInfo(SPI_GETNONCLIENTMETRICS, sizeof(NONCLIENTMETRICS), &ncm, 0U);
    ncm.lfSmCaptionFont.lfWeight = FW_NORMAL;
    m_hFont                      = CreateFontIndirect(&ncm.lfCaptionFont);
}

TabListDialog::~TabListDialog()
{
    if (m_hFont)
        DeleteObject(m_hFont);
}

SIZE TabListDialog::SetTabList(std::deque<std::tuple<std::wstring, DocID>>&& list)
{
    m_tabList         = list;
    const auto twodpi = CDPIAware::Instance().Scale(*this, 2);
    auto       dc     = CreateCompatibleDC(nullptr);
    OnOutOfScope(ReleaseDC(nullptr, dc));
    auto hOldFont = SelectObject(dc, m_hFont);

    SIZE sz{};
    m_textHeight = 0;
    for (const auto& [title, docId] : m_tabList)
    {
        RECT textRect{0, 0, 100, 1200};
        DrawText(dc, title.c_str(), -1, &textRect, DT_SINGLELINE | DT_NOPREFIX | DT_NOCLIP | DT_TOP | DT_CALCRECT);
        sz.cx = max(sz.cx, textRect.right);
        sz.cy += textRect.bottom;
        sz.cy += twodpi;
        m_textHeight = max(m_textHeight, textRect.bottom);
    }
    sz.cy += twodpi;
    sz.cx += 2 * twodpi;
    SelectObject(dc, hOldFont);
    m_currentItem = 1;
    return sz;
}

LRESULT TabListDialog::DlgFunc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (uMsg)
    {
        case WM_INITDIALOG:
        {
            CTheme::Instance().RegisterThemeChangeCallback(
                [this]() {
                    CTheme::Instance().SetThemeForDialog(*this, CTheme::Instance().IsDarkTheme());
                });
            CTheme::Instance().SetThemeForDialog(*this, CTheme::Instance().IsDarkTheme());
            InitDialog(hwndDlg, IDI_BOWPAD, false);
            CTheme::Instance().SetThemeForDialog(*this, CTheme::Instance().IsDarkTheme());

            InvalidateRect(*this, nullptr, true);
        }
            return TRUE;
        case WM_ACTIVATE:
            if (wParam == WA_INACTIVE)
            {
                ShowWindow(*this, SW_HIDE);
            }
            break;
        case WM_KEYUP:
        {
            if (wParam == VK_CONTROL)
            {
                ShowWindow(*this, SW_HIDE);
                m_execFunction(std::get<1>(m_tabList[m_currentItem]));
            }
        }
        break;
        case WM_LBUTTONDOWN:
        {
            POINT pt{};
            pt.x = GET_X_LPARAM(lParam);
            pt.y = GET_Y_LPARAM(lParam);

            RECT rect;
            GetClientRect(*this, &rect);
            const auto twodpi       = CDPIAware::Instance().Scale(*this, 2);
            auto       maxItems     = (rect.bottom - rect.top - twodpi - twodpi) / (m_textHeight + twodpi);
            size_t     indexToStart = 0;
            if (maxItems < m_tabList.size())
            {
                if (maxItems <= m_currentItem)
                    indexToStart = m_currentItem - maxItems;
            }
            auto item = pt.y / (m_textHeight + twodpi);
            if (item >= 0 && item < m_tabList.size())
            {
                ShowWindow(*this, SW_HIDE);
                auto docId = std::get<1>(m_tabList[item]);
                m_execFunction(docId);
            }
        }
        break;
        case WM_KEYDOWN:
        {
            if (wParam == VK_TAB)
            {
                auto shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
                if (shift)
                {
                    if (m_currentItem)
                        --m_currentItem;
                    else
                        m_currentItem = m_tabList.size() - 1;
                }
                else
                {
                    ++m_currentItem;
                    if (m_currentItem >= m_tabList.size())
                        m_currentItem = 0;
                }
                InvalidateRect(*this, nullptr, true);
            }
        }
        break;
        case WM_GETDLGCODE:
            return DLGC_WANTALLKEYS;
        case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC         hdc = BeginPaint(*this, &ps);
            RECT        rect;
            GetClientRect(*this, &rect);

            auto hMyMemDC   = ::CreateCompatibleDC(hdc);
            auto hBitmap    = CreateCompatibleBitmap(hdc, rect.right - rect.left, rect.bottom - rect.top);
            auto hOldBitmap = (HBITMAP)SelectObject(hMyMemDC, hBitmap);

            auto foreColor = CTheme::Instance().GetThemeColor(GetSysColor(COLOR_WINDOWTEXT));
            auto backColor = CTheme::Instance().GetThemeColor(GetSysColor(COLOR_3DFACE));
            GDIHelpers::FillSolidRect(hMyMemDC, &rect, backColor);

            SetTextColor(hMyMemDC, foreColor);
            SetBkColor(hMyMemDC, backColor);
            auto hOldFont = SelectObject(hMyMemDC, m_hFont);

            const auto     onedpi        = CDPIAware::Instance().Scale(*this, 1);
            const auto     twodpi        = CDPIAware::Instance().Scale(*this, 2);
            constexpr auto mainDrawFlags = DT_SINGLELINE | DT_NOPREFIX | DT_TOP;

            auto   maxItems     = (rect.bottom - rect.top - twodpi - twodpi) / (m_textHeight + twodpi);
            size_t indexToStart = 0;
            if (maxItems < m_tabList.size())
            {
                if (maxItems <= m_currentItem)
                    indexToStart = m_currentItem - maxItems;
            }
            long   yPos  = 0;
            size_t index = 0;
            for (const auto& [title, docId] : m_tabList)
            {
                ++index;
                if (index <= indexToStart)
                    continue;
                RECT textRect{twodpi, yPos, rect.right - twodpi, rect.bottom};
                DrawText(hMyMemDC, title.c_str(), -1, &textRect, mainDrawFlags);
                yPos += m_textHeight;
                yPos += twodpi;
            }
            auto selColor     = CTheme::Instance().GetThemeColor(GetSysColor(COLOR_HIGHLIGHT));
            auto selColorText = CTheme::Instance().GetThemeColor(GetSysColor(COLOR_HIGHLIGHTTEXT));
            SetTextColor(hMyMemDC, selColorText);
            SetBkColor(hMyMemDC, selColor);
            yPos = (long)(m_currentItem - indexToStart) * (m_textHeight + twodpi) - onedpi;
            RECT textRect{0, yPos, rect.right, yPos + m_textHeight + onedpi};
            const auto& [title, docId] = m_tabList[m_currentItem];
            GDIHelpers::FillSolidRect(hMyMemDC, &textRect, selColor);
            textRect.left = twodpi;
            DrawText(hMyMemDC, title.c_str(), -1, &textRect, mainDrawFlags);

            // Copy the off screen bitmap onto the screen.
            BitBlt(hdc, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, hMyMemDC, rect.left, rect.top, SRCCOPY);
            //Swap back the original bitmap.
            SelectObject(hMyMemDC, hOldFont);
            SelectObject(hMyMemDC, hOldBitmap);
            DeleteObject(hBitmap);
            DeleteDC(hMyMemDC);

            EndPaint(*this, &ps);
            return 0;
        }
        break;
        case WM_ERASEBKGND:
            return TRUE;
    }
    return FALSE;
}
