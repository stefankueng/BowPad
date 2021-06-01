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
#include "SettingsDlg.h"
#include "BowPad.h"
#include "OnOutOfScope.h"
#include "AppUtils.h"
#include "PathUtils.h"
#include "CommandHandler.h"
#include "Theme.h"
#include "ResString.h"

#include <locale>

CSettingsDlg::CSettingsDlg()
{
}

LRESULT CSettingsDlg::DlgFunc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (uMsg)
    {
        case WM_INITDIALOG:
        {
            SetWindowTheme(GetDlgItem(*this, IDC_SETTINGSLIST), L"Explorer", nullptr);
            CTheme::Instance().RegisterThemeChangeCallback(
                [this]() {
                    CTheme::Instance().SetThemeForDialog(*this, CTheme::Instance().IsDarkTheme());
                });
            CTheme::Instance().SetThemeForDialog(*this, CTheme::Instance().IsDarkTheme());
            InitDialog(hwndDlg, IDI_BOWPAD);
            CTheme::Instance().SetThemeForDialog(*this, CTheme::Instance().IsDarkTheme());

            m_resizer.Init(hwndDlg);
            m_resizer.UseSizeGrip(!CTheme::Instance().IsDarkTheme());
            m_resizer.AddControl(hwndDlg, IDC_SETTINGSLIST, RESIZER_TOPLEFTBOTTOMRIGHT);
            m_resizer.AddControl(hwndDlg, IDC_LABEL, RESIZER_BOTTOMLEFTRIGHT);
            m_resizer.AddControl(hwndDlg, IDC_SAVE, RESIZER_BOTTOMRIGHT);
            m_resizer.AddControl(hwndDlg, IDCANCEL, RESIZER_BOTTOMRIGHT);

            m_settings.push_back({L"updatecheck", L"auto", ResString(g_hRes, IDS_CHECKFORUPDATES), ResString(g_hRes, IDS_CHECKFORUPDATES_DESC), SettingType::Boolean, true});
            m_settings.push_back({L"view", L"scrollstyle", ResString(g_hRes, IDS_SETTINGS_SCROLLSTYLE), ResString(g_hRes, IDS_SETTINGS_SCROLLSTYLE_DESC), SettingType::Boolean, true});
            m_settings.push_back({L"view", L"d2d", ResString(g_hRes, IDS_SETTING_D2D), ResString(g_hRes, IDS_SETTING_D2D_DESC), SettingType::Boolean, true});
            m_settings.push_back({L"view", L"caretlineframe", ResString(g_hRes, IDS_SETTING_CARETLINEFRAME), ResString(g_hRes, IDS_SETTING_CARETLINEFRAME_DESC), SettingType::Boolean, false});
            m_settings.push_back({L"view", L"bracehighlighttext", ResString(g_hRes, IDS_SETTING_BRACEHIGHLIGHT), ResString(g_hRes, IDS_SETTING_BRACEHIGHLIGHT_DESC), SettingType::Boolean, true});

            InitSettingsList();
        }
            return FALSE;
        case WM_SIZE:
            m_resizer.DoResize(LOWORD(lParam), HIWORD(lParam));
            break;
        case WM_GETMINMAXINFO:
        {
            MINMAXINFO* mmi       = reinterpret_cast<MINMAXINFO*>(lParam);
            mmi->ptMinTrackSize.x = m_resizer.GetDlgRectScreen()->right;
            mmi->ptMinTrackSize.y = m_resizer.GetDlgRectScreen()->bottom;
            return 0;
        }
        case WM_COMMAND:
            return DoCommand(LOWORD(wParam), HIWORD(wParam));
        case WM_NOTIFY:
            switch (wParam)
            {
                case IDC_SETTINGSLIST:
                    return DoListNotify(reinterpret_cast<LPNMITEMACTIVATE>(lParam));
                default:
                    break;
            }
            return FALSE;
        case WM_KEYDOWN:
            switch (wParam)
            {
                case VK_F2:
                {
                    HWND hListCtrl = GetDlgItem(*this, IDC_SETTINGSLIST);
                    ListView_EditLabel(hListCtrl, ListView_GetSelectionMark(hListCtrl));
                }
                break;
                default:
                    break;
            }
            break;
        default:
            break;
    }
    return 0;
}

LRESULT CSettingsDlg::DoCommand(int id, int /*msg*/)
{
    switch (id)
    {
        case IDC_SAVE:
        {
            HWND hListCtrl = GetDlgItem(*this, IDC_SETTINGSLIST);

            for (size_t i = 0; i < m_settings.size(); ++i)
            {
                wchar_t buf[1024]{};
                ListView_GetItemText(hListCtrl, i, 0, buf, _countof(buf));
                auto& setting = m_settings[i];
                if (buf[0] == 0)
                    CIniSettings::Instance().Delete(setting.section.c_str(), setting.key.c_str());
                else
                {
                    switch (setting.type)
                    {
                        case SettingType::Boolean:
                        {
                            CIniSettings::Instance().SetInt64(setting.section.c_str(), setting.key.c_str(), _wcsicmp(buf, L"true") == 0 ? 1 : 0);
                        }
                        break;
                        case SettingType::Number:
                        {
                            CIniSettings::Instance().SetString(setting.section.c_str(), setting.key.c_str(), buf);
                        }
                        break;
                        case SettingType::String:
                        {
                            CIniSettings::Instance().SetString(setting.section.c_str(), setting.key.c_str(), buf);
                        }
                        default:
                            break;
                    }
                }
            }
            CIniSettings::Instance().Save();
        }
            [[fallthrough]];
        case IDCANCEL:
            EndDialog(*this, id);
            break;
        default:
            break;
    }
    return 1;
}

LRESULT CSettingsDlg::DoListNotify(LPNMITEMACTIVATE lpNMItemActivate)
{
    switch (lpNMItemActivate->hdr.code)
    {
        case NM_CLICK:
        {
            if (lpNMItemActivate->iItem < 0 || lpNMItemActivate->iItem >= static_cast<int>(m_settings.size()))
            {
                SetDlgItemText(*this, IDC_LABEL, L"");
                return 0;
            }
            auto& setting = m_settings[lpNMItemActivate->iItem];
            SetDlgItemText(*this, IDC_LABEL, setting.description.c_str());
        }
        break;
        case NM_DBLCLK:
        {
            if (lpNMItemActivate->iItem < static_cast<int>(m_settings.size()))
            {
                ListView_EditLabel(GetDlgItem(*this, IDC_SETTINGSLIST), lpNMItemActivate->iItem);
            }
        }
        break;
        case LVN_BEGINLABELEDIT:
            return 0;
        case LVN_ENDLABELEDIT:
        {
            auto pDispInfo = reinterpret_cast<NMLVDISPINFO*>(lpNMItemActivate);
            if (pDispInfo->item.pszText == nullptr)
                break;

            bool allowEdit = false;
            switch (m_settings[pDispInfo->item.iItem].type)
            {
                case SettingType::Boolean:
                {
                    if ((pDispInfo->item.pszText[0] == 0) ||
                        (wcscmp(pDispInfo->item.pszText, L"true") == 0) ||
                        (wcscmp(pDispInfo->item.pszText, L"false") == 0))
                    {
                        allowEdit = true;
                    }
                }
                break;
                case SettingType::Number:
                {
                    wchar_t* pChar = pDispInfo->item.pszText;
                    allowEdit      = true;
                    while (*pChar)
                    {
                        if (!iswdigit(*pChar))
                        {
                            allowEdit = false;
                            break;
                        }
                        pChar++;
                    }
                }
                break;
                case SettingType::String:
                    allowEdit = true;
                    break;
                default:
                    break;
            }

            return allowEdit ? TRUE : FALSE;
        }
        default:
            break;
    }
    return 0;
}

void CSettingsDlg::InitSettingsList()
{
    HWND  hListControl = GetDlgItem(*this, IDC_SETTINGSLIST);
    DWORD exStyle      = LVS_EX_DOUBLEBUFFER | LVS_EX_FULLROWSELECT;
    SendMessage(hListControl, WM_SETREDRAW, FALSE, 0);
    OnOutOfScope(SendMessage(hListControl, WM_SETREDRAW, TRUE, 0));
    ListView_DeleteAllItems(hListControl);

    int c = Header_GetItemCount(ListView_GetHeader(hListControl)) - 1;
    while (c >= 0)
        ListView_DeleteColumn(hListControl, c--);

    ListView_SetExtendedListViewStyle(hListControl, exStyle);

    ResString sSetting(g_hRes, IDS_SETTING_SETTINGCOL);
    ResString sValue(g_hRes, IDS_SETTING_VALUECOL);
    LVCOLUMN  lvc = {0};
    lvc.mask      = LVCF_TEXT | LVCF_FMT;
    lvc.fmt       = LVCFMT_LEFT;
    lvc.cx        = -1;
    lvc.pszText   = const_cast<LPWSTR>(sValue.c_str());
    ListView_InsertColumn(hListControl, 0, &lvc);
    lvc.pszText = const_cast<LPWSTR>(sSetting.c_str());
    ListView_InsertColumn(hListControl, 1, &lvc);

    int index = 0;
    for (const auto& setting : m_settings)
    {
        wchar_t buf[1024];
        wcscpy_s(buf, setting.name.c_str());
        LVITEM item  = {0};
        item.iItem   = index;
        item.mask    = LVIF_TEXT;
        item.pszText = buf;
        index        = ListView_InsertItem(hListControl, &item);
        ListView_SetItemText(hListControl, index, 1, buf);
        switch (setting.type)
        {
            case SettingType::Boolean:
            {
                auto val = CIniSettings::Instance().GetInt64(setting.section.c_str(), setting.key.c_str(), setting.def.b);
                ListView_SetItemText(hListControl, index, 0, const_cast<LPWSTR>(val != 0 ? L"true" : L"false"));
            }
            break;
            case SettingType::Number:
            {
                auto val = CIniSettings::Instance().GetInt64(setting.section.c_str(), setting.key.c_str(), setting.def.l);
                ListView_SetItemText(hListControl, index, 0, const_cast<LPWSTR>(std::to_wstring(val).c_str()));
            }
            break;
            case SettingType::String:
            {
                auto val = CIniSettings::Instance().GetString(setting.section.c_str(), setting.key.c_str(), setting.def.s);
                ListView_SetItemText(hListControl, index, 0, const_cast<LPWSTR>(val));
            }
            break;
            default:
                break;
        }

        ++index;
    }

    ListView_SetColumnWidth(hListControl, 0, LVSCW_AUTOSIZE_USEHEADER);
    ListView_SetColumnWidth(hListControl, 1, LVSCW_AUTOSIZE_USEHEADER);

    int arr[2] = {1, 0};
    ListView_SetColumnOrderArray(hListControl, 2, arr);

    SetWindowTheme(hListControl, L"Explorer", nullptr);
}
