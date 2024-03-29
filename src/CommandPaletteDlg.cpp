﻿// This file is part of BowPad.
//
// Copyright (C) 2013, 2015-2017, 2020-2022 - Stefan Kueng
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
#include "CommandPaletteDlg.h"
#include "BowPadUI.h"
#include "CommandHandler.h"
#include "KeyboardShortcutHandler.h"
#include "resource.h"
#include "AppUtils.h"
#include "CmdScripts.h"
#include "StringUtils.h"
#include "Theme.h"
#include "OnOutOfScope.h"
#include "PropertySet.h"
#include "UICollection.h"
#include "ResString.h"
#include <string>
#include <algorithm>
#include <memory>
#include <Commdlg.h>
#include <strsafe.h>

extern HINSTANCE     g_hRes;
extern IUIFramework* g_pFramework;

CCommandPaletteDlg::CCommandPaletteDlg(HWND hParent)
    : m_hParent(hParent)
    , m_hFilter(nullptr)
    , m_hResults(nullptr)
    , m_pCmd(nullptr)
{
}

CCommandPaletteDlg::~CCommandPaletteDlg()
{
}

void CCommandPaletteDlg::ClearFilterText()
{
    SetDlgItemText(*this, IDC_COLLECTIONNAME, L"");
    m_pCmd = nullptr;
    m_collectionResults.clear();
    SetWindowText(m_hFilter, L"");
    FillResults(false);
    SetFocus(m_hFilter);
}

LRESULT CCommandPaletteDlg::DlgFunc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
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
            // initialize the controls
            m_hFilter  = GetDlgItem(*this, IDC_FILTER);
            m_hResults = GetDlgItem(*this, IDC_RESULTS);

            ResString sFilterCue(g_hRes, IDS_COMMANDPALETTE_FILTERCUE);
            SendMessage(m_hFilter, EM_SETCUEBANNER, 1, reinterpret_cast<LPARAM>(sFilterCue.c_str()));
            SetWindowSubclass(m_hFilter, EditSubClassProc, 0, reinterpret_cast<DWORD_PTR>(this));

            const auto& resourceData = CKeyboardShortcutHandler::Instance().GetResourceData();
            const auto& commands     = CCommandHandler::Instance().GetCommands();
            for (const auto& [cmdId, pCommand] : commands)
            {
                auto whereAt = std::find_if(resourceData.begin(), resourceData.end(),
                                            [&](const auto& item) { return (static_cast<UINT>(item.second) == cmdId); });
                if (whereAt != resourceData.end())
                {
                    auto pScript = dynamic_cast<CCmdScript*>(pCommand.get());
                    if (pScript)
                    {
                        CmdPalData data;
                        data.cmdId   = cmdId;
                        data.command = CCommandHandler::Instance().GetPluginMap().at(cmdId);
                        SearchReplace(data.command, L"&", L"");
                        data.shortcut = CKeyboardShortcutHandler::Instance().GetShortCutStringForCommand(static_cast<WORD>(cmdId));
                        if (!data.command.empty())
                            m_allResults.push_back(data);
                    }
                    else
                    {
                        CmdPalData data;
                        data.cmdId  = cmdId;

                        auto sID    = whereAt->first + L"_TooltipDescription_RESID";

                        auto ttIDit = resourceData.find(sID);
                        if (ttIDit != resourceData.end())
                        {
                            auto sRes        = LoadResourceWString(g_hRes, ttIDit->second);
                            data.description = sRes;
                        }

                        sID         = whereAt->first + L"_LabelTitle_RESID";

                        auto ltIDit = resourceData.find(sID);
                        if (ltIDit != resourceData.end())
                        {
                            auto sRes    = LoadResourceWString(g_hRes, ltIDit->second);
                            data.command = sRes;
                            SearchReplace(data.command, L"&", L"");
                        }

                        data.shortcut = CKeyboardShortcutHandler::Instance().GetShortCutStringForCommand(static_cast<WORD>(cmdId));
                        if (!data.command.empty())
                            m_allResults.push_back(data);
                    }
                }
            }
            const auto& noDelCommands = CCommandHandler::Instance().GetNoDeleteCommands();
            for (const auto& [cmdId, pCommand] : noDelCommands)
            {
                auto whereAt = std::find_if(resourceData.begin(), resourceData.end(),
                                            [&](const auto& item) { return (static_cast<UINT>(item.second) == cmdId); });
                if (whereAt != resourceData.end())
                {
                    CmdPalData data;
                    data.cmdId  = cmdId;

                    auto sID    = whereAt->first + L"_TooltipDescription_RESID";

                    auto ttIDit = resourceData.find(sID);
                    if (ttIDit != resourceData.end())
                    {
                        auto sRes        = LoadResourceWString(g_hRes, ttIDit->second);
                        data.description = sRes;
                    }

                    sID         = whereAt->first + L"_LabelTitle_RESID";

                    auto ltIDit = resourceData.find(sID);
                    if (ltIDit != resourceData.end())
                    {
                        auto sRes    = LoadResourceWString(g_hRes, ltIDit->second);
                        data.command = sRes;
                        SearchReplace(data.command, L"&", L"");
                    }

                    data.shortcut = CKeyboardShortcutHandler::Instance().GetShortCutStringForCommand(static_cast<WORD>(cmdId));
                    if (!data.command.empty())
                        m_allResults.push_back(data);
                }
            }
            std::sort(m_allResults.begin(), m_allResults.end(),
                      [](const CmdPalData& a, const CmdPalData& b) -> bool {
                          return StrCmpLogicalW(a.command.c_str(), b.command.c_str()) < 0;
                      });
            m_resizer.UseSizeGrip(false);
            m_resizer.Init(hwndDlg);
            m_resizer.UseSizeGrip(!CTheme::Instance().IsDarkTheme());
            m_resizer.AddControl(hwndDlg, IDC_FILTER, RESIZER_TOPLEFTRIGHT);
            m_resizer.AddControl(hwndDlg, IDC_COLLECTIONNAME, RESIZER_TOPLEFTRIGHT);
            m_resizer.AddControl(hwndDlg, IDC_RESULTS, RESIZER_TOPLEFTBOTTOMRIGHT);
            InvalidateRect(*this, nullptr, true);
            InitResultsList();
            FillResults(true);
        }
            return TRUE;
        case WM_COMMAND:
            return DoCommand(LOWORD(wParam), HIWORD(wParam));
        case WM_NOTIFY:
            switch (wParam)
            {
                case IDC_RESULTS:
                    return DoListNotify(reinterpret_cast<LPNMITEMACTIVATE>(lParam));
                default:
                    return FALSE;
            }
        case WM_TIMER:
        {
            FillResults(false);
            KillTimer(*this, 101);
        }
        break;
        case WM_ACTIVATE:
            if (wParam == WA_INACTIVE)
            {
                ShowWindow(*this, SW_HIDE);
            }
            else
            {
                SetFocus(m_hFilter);
            }
            m_pCmd = nullptr;
            m_collectionResults.clear();
            SetDlgItemText(*this, IDC_COLLECTIONNAME, L"");
            break;
        case WM_SIZE:
        {
            int newWidth  = LOWORD(lParam);
            int newHeight = HIWORD(lParam);
            m_resizer.DoResize(newWidth, newHeight);
        }
        break;
        case WM_GETMINMAXINFO:
        {
            MINMAXINFO* mmi       = reinterpret_cast<MINMAXINFO*>(lParam);
            mmi->ptMinTrackSize.x = m_resizer.GetDlgRectScreen()->right;
            mmi->ptMinTrackSize.y = m_resizer.GetDlgRectScreen()->bottom;
            return 0;
        }
        case WM_CONTEXTMENU:
        {
            if (GetFocus() == m_hResults)
            {
                POINT pt{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
                int   selIndex = -1;
                if (pt.x == -1 && pt.y == -1)
                {
                    selIndex = ListView_GetNextItem(m_hResults, -1, LVNI_SELECTED);
                    RECT rc{};
                    ListView_GetItemRect(m_hResults, selIndex, &rc, LVIR_LABEL);
                    pt.x = rc.left;
                    pt.y = rc.top;
                    ClientToScreen(m_hResults, &pt);
                }
                else
                {
                    ScreenToClient(m_hResults, &pt);

                    LV_HITTESTINFO lvHti{};
                    lvHti.pt = pt;
                    ListView_HitTest(m_hResults, &lvHti);
                    if ((lvHti.flags & LVHT_ONITEM) != 0)
                    {
                        selIndex = lvHti.iItem;
                    }
                    ClientToScreen(m_hResults, &pt);
                }
                if (selIndex < 0)
                    break;

                HMENU hPopup = CreatePopupMenu();
                if (hPopup)
                {
                    OnOutOfScope(
                        DestroyMenu(hPopup));
                    ResString sFindAll(g_hRes, IDS_ADDTOQAT);
                    AppendMenu(hPopup, MF_STRING, IDS_ADDTOQAT, sFindAll);
                    auto cmd = TrackPopupMenu(hPopup, TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RETURNCMD, pt.x, pt.y, 0, *this, nullptr);
                    if (cmd == IDS_ADDTOQAT)
                    {
                        PROPVARIANT propVar;
                        if (SUCCEEDED(g_pFramework->GetUICommandProperty(cmdQat, UI_PKEY_ItemsSource, &propVar)))
                        {
                            IUICollectionPtr pCollection = static_cast<IUICollection*>(propVar.punkVal);
                            if (pCollection)
                            {
                                const auto& data = m_results[selIndex];

                                CAppUtils::AddCommandItem(pCollection, UI_COLLECTION_INVALIDINDEX, data.cmdId, UI_COMMANDTYPE_ACTION);
                            }
                        }
                    }
                }
            }
        }
        break;
        default:
            break;
    }
    return FALSE;
}

LRESULT CCommandPaletteDlg::DoCommand(int id, int code)
{
    switch (id)
    {
        case IDOK:
        {
            auto i = ListView_GetSelectionMark(m_hResults);
            if (i >= 0)
            {
                const auto& data    = m_results[i];
                auto*       cmd     = CCommandHandler::Instance().GetCommand(data.cmdId);
                BOOL        enabled = TRUE;
                if (cmd)
                {
                    PROPVARIANT propVarCurrentValue = {};
                    PROPVARIANT propVarNewValue     = {};
                    if (SUCCEEDED(cmd->IUICommandHandlerUpdateProperty(UI_PKEY_Enabled, &propVarCurrentValue, &propVarNewValue)))
                    {
                        if (FAILED(UIPropertyToBoolean(UI_PKEY_Enabled, propVarNewValue, &enabled)))
                            enabled = TRUE;
                    }
                }
                if (!enabled)
                    break;

                if (cmd && cmd->IsItemsSourceCommand())
                {
                    if (m_pCmd)
                    {
                        // execute the collection item
                        PROPVARIANT propIndex = {};
                        UIInitPropertyFromUInt32(UI_PKEY_SelectedItem, data.collectionIndex, &propIndex);
                        m_pCmd->IUICommandHandlerExecute(UI_EXECUTIONVERB_EXECUTE, &UI_PKEY_SelectedItem, &propIndex, nullptr);
                        m_collectionResults.clear();
                        m_pCmd = nullptr;
                        SetDlgItemText(*this, IDC_COLLECTIONNAME, L"");
                    }
                    else
                    {
                        m_collectionResults.clear();
                        m_pCmd = cmd;
                        SetDlgItemText(*this, IDC_COLLECTIONNAME, data.command.c_str());
                        CUICollection collection;
                        PROPVARIANT   propVarCurrentValue = {};
                        PROPVARIANT   propVarNewValue     = {};
                        UIInitPropertyFromInterface(UI_PKEY_ItemsSource, &collection, &propVarCurrentValue);
                        UIInitPropertyFromInterface(UI_PKEY_ItemsSource, &collection, &propVarNewValue);
                        m_pCmd->IUICommandHandlerUpdateProperty(UI_PKEY_ItemsSource, &propVarCurrentValue, &propVarNewValue);
                        UINT32 count = 0;
                        collection.GetCount(&count);
                        if (count == 0)
                        {
                            m_pCmd->IUICommandHandlerUpdateProperty(UI_PKEY_RecentItems, &propVarCurrentValue, &propVarNewValue);
                            SAFEARRAY* psa = nullptr;
                            UIPropertyToIUnknownArrayAlloc(UI_PKEY_RecentItems, propVarNewValue, &psa);
                            if (psa)
                            {
                                for (LONG a = 0; a < static_cast<LONG>(psa->rgsabound->cElements); ++a)
                                {
                                    IUnknown* iu{};
                                    SafeArrayGetElement(psa, &a, &iu);

                                    collection.Add(iu);
                                }
                                SafeArrayDestroy(psa);
                            }
                            collection.GetCount(&count);
                        }
                        for (UINT32 c = 0; c < count; ++c)
                        {
                            CmdPalData colData;
                            colData.cmdId                 = data.cmdId;
                            colData.collectionIndex       = c;
                            IUISimplePropertySetPtr pItem = nullptr;
                            collection.GetItem(c, reinterpret_cast<IUnknown**>(&pItem));
                            if (pItem)
                            {
                                PROPVARIANT propVar = {};
                                if (SUCCEEDED(pItem->GetValue(UI_PKEY_LabelDescription, &propVar)))
                                    colData.command = propVar.bstrVal;
                                else if (SUCCEEDED(pItem->GetValue(UI_PKEY_Label, &propVar)))
                                    colData.command = propVar.bstrVal;
                                PropVariantClear(&propVar);
                                if (SUCCEEDED(pItem->GetValue(UI_PKEY_TooltipDescription, &propVar)))
                                    colData.description = propVar.bstrVal;
                                PropVariantClear(&propVar);
                                m_collectionResults.push_back(colData);
                            }
                        }
                        PropVariantClear(&propVarCurrentValue);
                        PropVariantClear(&propVarNewValue);
                    }
                    SetWindowText(m_hFilter, L"");
                    FillResults(true);
                }
                else
                {
                    SetDlgItemText(*this, IDC_COLLECTIONNAME, L"");
                    m_pCmd = nullptr;
                    m_collectionResults.clear();
                    SendMessage(m_hParent, WM_COMMAND, MAKEWPARAM(data.cmdId, 1), 0);
                }
            }
        }
        break;
        case IDCANCEL:
            SetDlgItemText(*this, IDC_COLLECTIONNAME, L"");
            if (!m_pCmd && m_collectionResults.empty())
            {
                ShowWindow(*this, SW_HIDE);
                m_pCmd = nullptr;
                m_collectionResults.clear();
            }
            else
            {
                m_pCmd = nullptr;
                m_collectionResults.clear();
                FillResults(true);
            }
            break;
        case IDC_FILTER:
            if (code == EN_CHANGE)
                SetTimer(*this, 101, 50, nullptr);
            break;
    }
    return 1;
}

void CCommandPaletteDlg::InitResultsList() const
{
    SetWindowTheme(m_hResults, L"Explorer", nullptr);
    ListView_SetItemCountEx(m_hResults, 0, 0);

    auto hListHeader = ListView_GetHeader(m_hResults);
    int  c           = Header_GetItemCount(hListHeader) - 1;
    while (c >= 0)
        ListView_DeleteColumn(m_hResults, c--);

    DWORD exStyle = LVS_EX_DOUBLEBUFFER | LVS_EX_INFOTIP | LVS_EX_LABELTIP | LVS_EX_FULLROWSELECT;
    ListView_SetExtendedListViewStyle(m_hResults, exStyle);

    LVCOLUMN lvc{};
    lvc.mask = LVCF_FMT;
    lvc.fmt  = LVCFMT_LEFT;
    ListView_InsertColumn(m_hResults, 0, &lvc);
    lvc.fmt = LVCFMT_RIGHT;
    ListView_InsertColumn(m_hResults, 1, &lvc);
    lvc.fmt = LVCFMT_LEFT;
    ListView_InsertColumn(m_hResults, 2, &lvc);

    ListView_SetColumnWidth(m_hResults, 0, LVSCW_AUTOSIZE);
    ListView_SetColumnWidth(m_hResults, 1, LVSCW_AUTOSIZE);
    ListView_SetColumnWidth(m_hResults, 2, LVSCW_AUTOSIZE);
}

LRESULT CCommandPaletteDlg::DoListNotify(LPNMITEMACTIVATE lpNMItemActivate)
{
    switch (lpNMItemActivate->hdr.code)
    {
        case LVN_GETINFOTIP:
        {
            LPNMLVGETINFOTIP tip       = reinterpret_cast<LPNMLVGETINFOTIPW>(lpNMItemActivate);
            int              itemIndex = static_cast<size_t>(tip->iItem);
            if (itemIndex < 0 || itemIndex >= static_cast<int>(m_results.size()))
            {
                assert(false);
                return 0;
            }
            const auto& data = m_results[itemIndex];
            if (data.description.empty())
            {
                _snwprintf_s(tip->pszText, tip->cchTextMax, _TRUNCATE, L"%s\t%s",
                             data.command.c_str(), data.shortcut.c_str());
            }
            else
            {
                wcscpy_s(tip->pszText, tip->cchTextMax, data.description.c_str());
            }
        }
        break;
        case LVN_GETDISPINFO:
            // Note: the listview is Owner Draw which means you draw it,
            // but you still need to tell the list what exactly the text is
            // that you intend to draw or things like clicking the header to
            // re-size won't work.
            return GetListItemDispInfo(reinterpret_cast<NMLVDISPINFO*>(lpNMItemActivate));

        case NM_RETURN:
        case NM_DBLCLK:
            // execute the selected command
            if (lpNMItemActivate->iItem >= 0 && lpNMItemActivate->iItem < static_cast<int>(m_results.size()))
            {
                SendMessage(*this, WM_COMMAND, MAKEWPARAM(IDOK, 1), 0);
            }
            break;
        case NM_CUSTOMDRAW:
            return DrawListItem(reinterpret_cast<NMLVCUSTOMDRAW*>(lpNMItemActivate));
        default:
            break;
    }
    return 0;
}

LRESULT CCommandPaletteDlg::DrawListItem(NMLVCUSTOMDRAW* pLVCD) const
{
    switch (pLVCD->nmcd.dwDrawStage)
    {
        case CDDS_PREPAINT:
            return CDRF_NOTIFYITEMDRAW;
        case CDDS_ITEMPREPAINT:
            // Tell Windows to send draw notifications for each subitem.
            return CDRF_NOTIFYSUBITEMDRAW;
        case CDDS_ITEMPREPAINT | CDDS_ITEM | CDDS_SUBITEM:
        {
            pLVCD->clrText      = CTheme::Instance().GetThemeColor(::GetSysColor(COLOR_WINDOWTEXT));
            const auto& data    = m_results[pLVCD->nmcd.dwItemSpec];
            auto*       cmd     = CCommandHandler::Instance().GetCommand(data.cmdId);
            BOOL        enabled = TRUE;
            if (cmd)
            {
                PROPVARIANT propVarCurrentValue = {};
                PROPVARIANT propVarNewValue     = {};
                if (SUCCEEDED(cmd->IUICommandHandlerUpdateProperty(UI_PKEY_Enabled, &propVarCurrentValue, &propVarNewValue)))
                {
                    if (FAILED(UIPropertyToBoolean(UI_PKEY_Enabled, propVarNewValue, &enabled)))
                        enabled = TRUE;
                }
            }
            if (!enabled)
                pLVCD->clrText = CTheme::Instance().GetThemeColor(::GetSysColor(COLOR_GRAYTEXT));
            else if (ListView_GetItemState(m_hResults, pLVCD->nmcd.dwItemSpec, LVIS_SELECTED) & LVIS_SELECTED)
                pLVCD->clrText = CTheme::Instance().GetThemeColor(::GetSysColor(COLOR_HOTLIGHT));
        }
        break;
    }
    return CDRF_DODEFAULT;
}

LRESULT CCommandPaletteDlg::GetListItemDispInfo(NMLVDISPINFO* pDispInfo) const
{
    if ((pDispInfo->item.mask & LVIF_TEXT) != 0)
    {
        if (pDispInfo->item.pszText == nullptr)
            return 0;
        pDispInfo->item.pszText[0] = 0;
        int itemIndex              = pDispInfo->item.iItem;
        if (itemIndex >= static_cast<int>(m_results.size()))
            return 0;

        std::wstring sTemp;
        const auto&  item = m_results[itemIndex];
        switch (pDispInfo->item.iSubItem)
        {
            case 0:
                StringCchCopy(pDispInfo->item.pszText, pDispInfo->item.cchTextMax, item.command.c_str());
                break;

            case 1:
                StringCchCopy(pDispInfo->item.pszText, pDispInfo->item.cchTextMax, item.shortcut.c_str());
                break;
            case 2:
                StringCchCopy(pDispInfo->item.pszText, pDispInfo->item.cchTextMax, item.description.c_str());
                break;
            default:
                break;
        }
    }
    return 0;
}

void CCommandPaletteDlg::FillResults(bool force)
{
    static std::wstring lastFilterText;
    auto                filterText  = GetDlgItemText(IDC_FILTER);
    std::wstring        sFilterText = filterText.get();
    if (force || lastFilterText.empty() || _wcsicmp(sFilterText.c_str(), lastFilterText.c_str()))
    {
        m_results.clear();
        int                      col1Width  = 0;
        int                      col2Width  = 0;
        std::vector<CmdPalData>* allResults = &m_allResults;
        if (!m_collectionResults.empty())
            allResults = &m_collectionResults;

        for (const auto& cmd : *allResults)
        {
            if (sFilterText.empty() || StrStrIW(cmd.command.c_str(), sFilterText.c_str()) || StrStrIW(cmd.description.c_str(), sFilterText.c_str()))
            {
                col1Width = max(col1Width, ListView_GetStringWidth(m_hResults, cmd.command.c_str()));
                col2Width = max(col2Width, ListView_GetStringWidth(m_hResults, cmd.shortcut.c_str()));
                m_results.push_back(cmd);
            }
        }
        lastFilterText = sFilterText;
        ListView_SetItemCountEx(m_hResults, m_results.size(), 0);
        ListView_SetColumnWidth(m_hResults, 0, col1Width + 50LL);
        ListView_SetColumnWidth(m_hResults, 1, col2Width + 50LL);
        ListView_SetColumnWidth(m_hResults, 2, LVSCW_AUTOSIZE);
    }
}

LRESULT CALLBACK CCommandPaletteDlg::EditSubClassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData)
{
    CCommandPaletteDlg* pThis = reinterpret_cast<CCommandPaletteDlg*>(dwRefData);
    switch (uMsg)
    {
        case WM_NCDESTROY:
        {
            RemoveWindowSubclass(hWnd, EditSubClassProc, uIdSubclass);
            break;
        }
        case WM_KEYDOWN:
            if (wParam == VK_DOWN)
            {
                auto i = ListView_GetSelectionMark(pThis->m_hResults);
                ListView_SetItemState(pThis->m_hResults, i, 0, LVIS_SELECTED);
                ListView_SetSelectionMark(pThis->m_hResults, i + 1);
                ListView_SetItemState(pThis->m_hResults, i + 1, LVIS_SELECTED, LVIS_SELECTED);
                ListView_EnsureVisible(pThis->m_hResults, i + 1, false);
                return 0;
            }
            if (wParam == VK_UP)
            {
                auto i = ListView_GetSelectionMark(pThis->m_hResults);
                if (i > 0)
                {
                    ListView_SetItemState(pThis->m_hResults, i, 0, LVIS_SELECTED);
                    ListView_SetSelectionMark(pThis->m_hResults, i - 1);
                    ListView_SetItemState(pThis->m_hResults, i - 1, LVIS_SELECTED, LVIS_SELECTED);
                    ListView_EnsureVisible(pThis->m_hResults, i - 1, false);
                }
                return 0;
            }
            if (wParam == VK_RETURN)
            {
                auto i = ListView_GetSelectionMark(pThis->m_hResults);
                if (i >= 0)
                {
                    SendMessage(*pThis, WM_COMMAND, MAKEWPARAM(IDOK, 1), 0);
                }
            }
            break;
        default:
            break;
    }
    return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}
