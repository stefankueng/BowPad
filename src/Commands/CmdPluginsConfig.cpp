// This file is part of BowPad.
//
// Copyright (C) 2014-2018, 2020-2021 - Stefan Kueng
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
#include "CmdPluginsConfig.h"
#include "BowPad.h"
#include "AppUtils.h"
#include "UnicodeUtils.h"
#include "StringUtils.h"
#include "PathUtils.h"
#include "DownloadFile.h"
#include "TempFile.h"
#include "CommandHandler.h"
#include "version.h"
#include "Theme.h"
#include "ResString.h"

#include <future>
#include <fstream>
#include <locale>
#include <VersionHelpers.h>

const int WM_INITPLUGINS = (WM_APP + 10);

CPluginsConfigDlg::CPluginsConfigDlg(void* obj)
    : ICommand(obj)
    , m_threadEnded(false)
{
}

LRESULT CPluginsConfigDlg::DlgFunc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (uMsg)
    {
        case WM_INITDIALOG:
        {
            InitDialog(hwndDlg, IDI_BOWPAD);
            CTheme::Instance().SetThemeForDialog(*this, CTheme::Instance().IsDarkTheme());

            m_resizer.Init(hwndDlg);
            m_resizer.UseSizeGrip(!CTheme::Instance().IsDarkTheme());
            m_resizer.AddControl(hwndDlg, IDC_PLUGINGROUP, RESIZER_TOPLEFTBOTTOMRIGHT);
            m_resizer.AddControl(hwndDlg, IDC_PLUGINSLIST, RESIZER_TOPLEFTBOTTOMRIGHT);
            m_resizer.AddControl(hwndDlg, IDC_DESC, RESIZER_BOTTOMLEFTRIGHT);
            m_resizer.AddControl(hwndDlg, IDC_INFO, RESIZER_BOTTOMLEFTRIGHT);
            m_resizer.AddControl(hwndDlg, IDOK, RESIZER_BOTTOMRIGHT);

            SetWindowTheme(GetDlgItem(*this, IDC_PLUGINSLIST), L"Explorer", nullptr);

            HWND hThisWnd = *this;
            std::thread([hThisWnd] {
                CDownloadFile filedownloader(L"BowPad", nullptr);
                std::wstring  tempfile = CTempFiles::Instance().GetTempFilePath(true);
                filedownloader.DownloadFile(L"https://raw.githubusercontent.com/stefankueng/BowPad/main/plugins/plugins.txt", tempfile);

                // parse the file and fill in the m_plugins set
                auto          plugins = std::make_unique<std::map<std::wstring, PluginInfo>>();
                std::ifstream fin(tempfile);
                if (fin.is_open())
                {
                    std::string lineA;
                    while (std::getline(fin, lineA))
                    {
                        auto       line = CUnicodeUtils::StdGetUnicode(lineA);
                        PluginInfo info;
                        info.name = line;
                        if (std::getline(fin, lineA))
                        {
                            line         = CUnicodeUtils::StdGetUnicode(lineA);
                            info.version = _wtoi(line.c_str());
                            if (std::getline(fin, lineA))
                            {
                                line            = CUnicodeUtils::StdGetUnicode(lineA);
                                info.minVersion = _wtoi(line.c_str());
                                if (std::getline(fin, lineA))
                                {
                                    line             = CUnicodeUtils::StdGetUnicode(lineA);
                                    info.description = line;
                                    SearchReplace(info.description, L"\\n", L"\r\n");

                                    info.installedVersion = CCommandHandler::Instance().GetPluginVersion(info.name);
                                    if (info.minVersion <= (BP_VERMAJOR * 100 + BP_VERMINOR))
                                        (*plugins)[info.name] = info;
                                }
                            }
                        }
                    }
                    fin.close();
                }
                PostMessage(hThisWnd, WM_INITPLUGINS, 0, reinterpret_cast<LPARAM>(plugins.release()));
            }).detach();
        }
            return FALSE;
        case WM_INITPLUGINS:
        {
            std::unique_ptr<std::map<std::wstring, PluginInfo>> plugins(
                reinterpret_cast<std::map<std::wstring, PluginInfo>*>(lParam));
            for (const auto& [name, pluginInfo] : *plugins)
                m_plugins.push_back(pluginInfo);
            InitPluginsList();
            m_threadEnded = true;
            break;
        }
        case WM_SIZE:
            m_resizer.DoResize(LOWORD(lParam), HIWORD(lParam));
            break;
        case WM_COMMAND:
            return DoCommand(LOWORD(wParam), HIWORD(wParam));
        case WM_NOTIFY:
            switch (wParam)
            {
                case IDC_PLUGINSLIST:
                    return DoListNotify(reinterpret_cast<LPNMITEMACTIVATE>(lParam));
            }
            return FALSE;
    }
    return FALSE;
}

LRESULT CPluginsConfigDlg::DoCommand(int id, int /*msg*/)
{
    switch (id)
    {
        case IDOK:
        {
            bool bChanged  = false;
            int  index     = 0;
            HWND hListCtrl = GetDlgItem(*this, IDC_PLUGINSLIST);
            for (const auto& info : m_plugins)
            {
                if (info.minVersion <= (BP_VERMAJOR * 100 + BP_VERMINOR))
                {
                    if (ListView_GetCheckState(hListCtrl, index))
                    {
                        if (info.version > info.installedVersion)
                        {
                            CDownloadFile filedownloader(L"BowPad", nullptr);
                            std::wstring  tempfile  = CTempFiles::Instance().GetTempFilePath(true, L".zip");
                            std::wstring  pluginurl = L"https://raw.githubusercontent.com/stefankueng/BowPad/main/plugins/";
                            pluginurl += info.name;
                            pluginurl += L".zip";
                            filedownloader.DownloadFile(pluginurl, tempfile);

                            //std::wstring pluginlocal = L"D:\\Development\\BowPad\\BowPad\\plugins\\";
                            //pluginlocal += info.name;
                            //pluginlocal += L".zip";
                            //CopyFile(pluginlocal.c_str(), tempfile.c_str(), FALSE);
                            CreateDirectory(CAppUtils::GetDataPath().c_str(), nullptr);
                            std::wstring pluginsdir = CAppUtils::GetDataPath() + L"\\plugins";
                            CreateDirectory(pluginsdir.c_str(), nullptr);
                            if (!CPathUtils::Unzip2Folder(tempfile.c_str(), pluginsdir.c_str()))
                            {
                                // failed to unzip/install the plugin
                                ResString    rPluginInstallFailed(g_hRes, IDS_PLUGINS_INSTALLFAILED);
                                std::wstring sMsg = CStringUtils::Format(static_cast<LPCWSTR>(rPluginInstallFailed), info.name.c_str());
                                MessageBox(GetHwnd(), sMsg.c_str(), L"BowPad", MB_ICONINFORMATION);
                            }
                            else
                                bChanged = true;
                        }
                    }
                    else if (info.installedVersion > 0)
                    {
                        // remove the already installed plugin
                        std::wstring pluginsdir = CAppUtils::GetDataPath() + L"\\plugins\\";
                        pluginsdir += info.name;

                        IFileOperationPtr pfo = nullptr;
                        HRESULT           hr  = pfo.CreateInstance(CLSID_FileOperation, nullptr, CLSCTX_ALL);

                        if (!CAppUtils::FailedShowMessage(hr))
                        {
                            // Set parameters for current operation
                            DWORD opFlags = FOF_ALLOWUNDO | FOF_NO_CONNECTED_ELEMENTS | FOF_NOCONFIRMATION | FOF_NORECURSION | FOF_SILENT | FOFX_SHOWELEVATIONPROMPT;
                            if (IsWindows8OrGreater())
                                opFlags |= FOFX_RECYCLEONDELETE | FOFX_ADDUNDORECORD;
                            hr = pfo->SetOperationFlags(opFlags);

                            if (!CAppUtils::FailedShowMessage(hr))
                            {
                                // Create IShellItem instance associated to file to delete
                                IShellItemPtr psiFileToDelete = nullptr;
                                hr                            = SHCreateItemFromParsingName(pluginsdir.c_str(), nullptr, IID_PPV_ARGS(&psiFileToDelete));

                                if (!CAppUtils::FailedShowMessage(hr))
                                {
                                    // Declare this shell item (file) to be deleted
                                    hr = pfo->DeleteItem(psiFileToDelete, nullptr);
                                }
                            }
                            pfo->SetOwnerWindow(GetHwnd());
                            if (!CAppUtils::FailedShowMessage(hr))
                            {
                                // Perform the deleting operation
                                hr = pfo->PerformOperations();
                                if (CAppUtils::FailedShowMessage(hr))
                                {
                                    continue;
                                }
                                bChanged = true;
                            }
                        }
                    }
                }
                ++index;
            }
            if (bChanged)
            {
                ResString rRestartRequired(g_hRes, IDS_PLUGINS_RESTARTREQUIRED);
                MessageBox(GetHwnd(), static_cast<LPCWSTR>(rRestartRequired), L"BowPad", MB_ICONINFORMATION);
            }
        }
            [[fallthrough]];
        case IDCANCEL:
            EndDialog(*this, id);
            break;
    }
    return 1;
}

LRESULT CPluginsConfigDlg::DoListNotify(LPNMITEMACTIVATE lpNMItemActivate)
{
    switch (lpNMItemActivate->hdr.code)
    {
        case NM_CLICK:
        {
            if (lpNMItemActivate->iItem < 0 || lpNMItemActivate->iItem >= static_cast<int>(m_plugins.size()))
            {
                SetDlgItemText(*this, IDC_DESC, L"");
                return 0;
            }
            PluginInfo info = m_plugins[lpNMItemActivate->iItem];
            SetDlgItemText(*this, IDC_DESC, info.description.c_str());
        }
        break;
        case LVN_GETEMPTYMARKUP:
        {
            NMLVEMPTYMARKUP* lpNMEmptyMarkup = reinterpret_cast<NMLVEMPTYMARKUP*>(lpNMItemActivate);
            lpNMEmptyMarkup->dwFlags         = EMF_CENTERED;
            ResString rThreadIsRunning(g_hRes, IDS_FETCHING_PLUGINS_LIST);
            ResString rEmpty(g_hRes, IDS_NO_PLUGINS_AVAILABLE);
            wcscpy_s(lpNMEmptyMarkup->szMarkup, m_threadEnded ? rEmpty : rThreadIsRunning);
            lpNMEmptyMarkup->szMarkup;
            return TRUE;
        }
        default:
            break;
    }
    return 0;
}

void CPluginsConfigDlg::InitPluginsList()
{
    HWND  hListControl = GetDlgItem(*this, IDC_PLUGINSLIST);
    DWORD exStyle      = LVS_EX_DOUBLEBUFFER | LVS_EX_INFOTIP | LVS_EX_FULLROWSELECT | LVS_EX_CHECKBOXES;
    SendMessage(hListControl, WM_SETREDRAW, FALSE, 0);
    ListView_DeleteAllItems(hListControl);

    int c = Header_GetItemCount(ListView_GetHeader(hListControl)) - 1;
    while (c >= 0)
        ListView_DeleteColumn(hListControl, c--);

    ListView_SetExtendedListViewStyle(hListControl, exStyle);

    ResString sName(g_hRes, IDS_PLUGINLIST_NAME);
    ResString sVersion(g_hRes, IDS_PLUGINLIST_VERSION);
    ResString sInstVersion(g_hRes, IDS_PLUGINLIST_INSTVERSION);

    LVCOLUMN lvc = {0};
    lvc.mask     = LVCF_TEXT | LVCF_FMT;
    lvc.fmt      = LVCFMT_LEFT;
    lvc.cx       = -1;
    lvc.pszText  = const_cast<LPWSTR>(static_cast<LPCWSTR>(sName));
    ListView_InsertColumn(hListControl, 0, &lvc);
    lvc.pszText = const_cast<LPWSTR>(static_cast<LPCWSTR>(sVersion));
    lvc.fmt     = LVCFMT_RIGHT;
    ListView_InsertColumn(hListControl, 1, &lvc);
    lvc.fmt     = LVCFMT_LEFT;
    lvc.pszText = const_cast<LPWSTR>(static_cast<LPCWSTR>(sInstVersion));
    ListView_InsertColumn(hListControl, 2, &lvc);

    int index = 0;
    for (const auto& info : m_plugins)
    {
        wchar_t buf[1024];
        wcscpy_s(buf, info.name.c_str());
        LVITEM item  = {0};
        item.iItem   = index;
        item.mask    = LVIF_TEXT;
        item.pszText = buf;
        index        = ListView_InsertItem(hListControl, &item);
        wcscpy_s(buf, CStringUtils::Format(L"%d.%d", info.version / 100, info.version % 100).c_str());
        ListView_SetItemText(hListControl, index, 1, buf);
        wcscpy_s(buf, CStringUtils::Format(L"%d.%d", info.installedVersion / 100, info.installedVersion % 100).c_str());
        ListView_SetItemText(hListControl, index, 2, buf);
        ListView_SetCheckState(hListControl, index, info.installedVersion > 0);
        ++index;
    }

    ListView_SetColumnWidth(hListControl, 0, LVSCW_AUTOSIZE_USEHEADER);
    ListView_SetColumnWidth(hListControl, 1, LVSCW_AUTOSIZE_USEHEADER);
    ListView_SetColumnWidth(hListControl, 2, LVSCW_AUTOSIZE_USEHEADER);

    SetWindowTheme(hListControl, L"Explorer", nullptr);

    SendMessage(hListControl, WM_SETREDRAW, TRUE, 0);
}

bool CCmdPluginsConfig::Execute()
{
    CPluginsConfigDlg dlg(m_pMainWindow);
    dlg.DoModal(g_hRes, IDD_PLUGINSCONFIGDLG, GetHwnd());

    return true;
}
