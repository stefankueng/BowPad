// This file is part of BowPad.
//
// Copyright (C) 2014-2015 - Stefan Kueng
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

#include <future>
#include <fstream>
#include <locale>
#include <codecvt>
#include <VersionHelpers.h>


CPluginsConfigDlg::CPluginsConfigDlg(void * obj)
    : ICommand(obj)
{}

CPluginsConfigDlg::~CPluginsConfigDlg(void)
{}

LRESULT CPluginsConfigDlg::DlgFunc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (uMsg)
    {
        case WM_INITDIALOG:
        {
            InitDialog(hwndDlg, IDI_BOWPAD);

            m_resizer.Init(hwndDlg);
            m_resizer.AddControl(hwndDlg, IDC_PLUGINGROUP, RESIZER_TOPLEFTBOTTOMRIGHT);
            m_resizer.AddControl(hwndDlg, IDC_PLUGINSLIST, RESIZER_TOPLEFTBOTTOMRIGHT);
            m_resizer.AddControl(hwndDlg, IDC_DESC, RESIZER_BOTTOMLEFTRIGHT);
            m_resizer.AddControl(hwndDlg, IDC_INFO, RESIZER_BOTTOMLEFTRIGHT);
            m_resizer.AddControl(hwndDlg, IDOK, RESIZER_BOTTOMRIGHT);
            InitPluginsList();

            auto t = std::thread([=]
            {
                CDownloadFile filedownloader(L"BowPad", NULL);
                std::wstring tempfile = CTempFiles::Instance().GetTempFilePath(true);
                filedownloader.DownloadFile(L"https://svn.code.sf.net/p/bowpad-sk/code/trunk/plugins/plugins.txt", tempfile);

                //CopyFile(L"D:\\Development\\BowPad\\BowPad\\plugins\\plugins.txt", tempfile.c_str(), FALSE);
                // parse the file and fill in the m_plugins set
                std::map<std::wstring, PluginInfo> plugins;

                std::wifstream fin(tempfile);
                fin.imbue(std::locale(fin.getloc(), new std::codecvt_utf8_utf16<wchar_t>));
                if (fin.is_open())
                {
                    std::wstring line;
                    while (std::getline(fin, line))
                    {
                        PluginInfo info;
                        info.name = line;
                        if (std::getline(fin, line))
                        {
                            info.version = _wtoi(line.c_str());
                            if (std::getline(fin, line))
                            {
                                info.minversion = _wtoi(line.c_str());
                                if (std::getline(fin, line))
                                {
                                    info.description = line;
                                    SearchReplace(info.description, L"\\n", L"\r\n");

                                    info.installedversion = CCommandHandler::Instance().GetPluginVersion(info.name);
                                    if (info.minversion <= (BP_VERMAJOR * 100 + BP_VERMINOR))
                                        plugins[info.name] = info;
                                }
                            }
                        }
                    }
                    fin.close();
                }
                for (auto it : plugins)
                    m_plugins.push_back(it.second);
                InitPluginsList();
            });
            t.detach();
        }
            return FALSE;
        case WM_SIZE:
            m_resizer.DoResize(LOWORD(lParam), HIWORD(lParam));
            break;
        case WM_COMMAND:
            return DoCommand(LOWORD(wParam), HIWORD(wParam));
        case WM_NOTIFY:
            switch (wParam)
            {
                case IDC_PLUGINSLIST:
                    return DoListNotify((LPNMITEMACTIVATE)lParam);
                default:
                    return FALSE;
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
            bool bChanged = false;
            int index = 0;
            HWND hListCtrl = GetDlgItem(*this, IDC_PLUGINSLIST);
            for (const auto& info : m_plugins)
            {
                if (info.minversion >= (BP_VERMAJOR * 100 + BP_VERMINOR))
                {
                    if (ListView_GetCheckState(hListCtrl, index))
                    {
                        if (info.version > info.installedversion)
                        {
                            CDownloadFile filedownloader(L"BowPad", NULL);
                            std::wstring tempfile = CTempFiles::Instance().GetTempFilePath(true, L".zip");
                            std::wstring pluginurl = L"https://svn.code.sf.net/p/bowpad-sk/code/trunk/plugins/";
                            pluginurl += info.name;
                            pluginurl += L".zip";
                            filedownloader.DownloadFile(pluginurl, tempfile);


                            //std::wstring pluginlocal = L"D:\\Development\\BowPad\\BowPad\\plugins\\";
                            //pluginlocal += info.name;
                            //pluginlocal += L".zip";
                            //CopyFile(pluginlocal.c_str(), tempfile.c_str(), FALSE);
                            CreateDirectory(CAppUtils::GetDataPath().c_str(), NULL);
                            std::wstring pluginsdir = CAppUtils::GetDataPath() + L"\\plugins";
                            CreateDirectory(pluginsdir.c_str(), NULL);
                            if (!CPathUtils::Unzip2Folder(tempfile.c_str(), pluginsdir.c_str()))
                            {
                                // failed to unzip/install the plugin
                                ResString rPluginInstallFailed(hRes, IDS_PLUGINS_INSTALLFAILED);
                                std::wstring sMsg = CStringUtils::Format((LPCWSTR)rPluginInstallFailed, info.name.c_str());
                                MessageBox(GetHwnd(), sMsg.c_str(), L"BowPad", MB_ICONINFORMATION);
                            }
                            else
                                bChanged = true;
                        }
                    }
                    else if (info.installedversion > 0)
                    {
                        // remove the already installed plugin
                        std::wstring pluginsdir = CAppUtils::GetDataPath() + L"\\plugins\\";
                        pluginsdir += info.name;

                        IFileOperationPtr pfo = NULL;
                        HRESULT hr = pfo.CreateInstance(CLSID_FileOperation, NULL, CLSCTX_ALL);

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
                                IShellItemPtr psiFileToDelete = NULL;
                                hr = SHCreateItemFromParsingName(pluginsdir.c_str(), NULL, IID_PPV_ARGS(&psiFileToDelete));

                                if (!CAppUtils::FailedShowMessage(hr))
                                {
                                    // Declare this shell item (file) to be deleted
                                    hr = pfo->DeleteItem(psiFileToDelete, NULL);
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
                ResString rRestartRequired(hRes, IDS_PLUGINS_RESTARTREQUIRED);
                MessageBox(GetHwnd(), (LPCWSTR)rRestartRequired, L"BowPad", MB_ICONINFORMATION);
            }
        }
            // intentional fall through
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
            if (lpNMItemActivate->iItem >= (int)m_plugins.size())
            {
                SetDlgItemText(*this, IDC_DESC, L"");
                return 0;
            }
            PluginInfo info = m_plugins[lpNMItemActivate->iItem];
            SetDlgItemText(*this, IDC_DESC, info.description.c_str());
        }
            break;
        default:
            break;
    }
    return 0;

}

void CPluginsConfigDlg::InitPluginsList()
{
    HWND hListControl = GetDlgItem(*this, IDC_PLUGINSLIST);
    DWORD exStyle = LVS_EX_DOUBLEBUFFER | LVS_EX_INFOTIP | LVS_EX_FULLROWSELECT | LVS_EX_CHECKBOXES;
    SendMessage(hListControl, WM_SETREDRAW, FALSE, 0);
    ListView_DeleteAllItems(hListControl);

    int c = Header_GetItemCount(ListView_GetHeader(hListControl)) - 1;
    while (c >= 0)
        ListView_DeleteColumn(hListControl, c--);

    ListView_SetExtendedListViewStyle(hListControl, exStyle);

    ResString sName(hRes, IDS_PLUGINLIST_NAME);
    ResString sVersion(hRes, IDS_PLUGINLIST_VERSION);
    ResString sInstVersion(hRes, IDS_PLUGINLIST_INSTVERSION);

    LVCOLUMN lvc = { 0 };
    lvc.mask = LVCF_TEXT | LVCF_FMT;
    lvc.fmt = LVCFMT_LEFT;
    lvc.cx = -1;
    lvc.pszText = const_cast<LPWSTR>((LPCWSTR)sName);
    ListView_InsertColumn(hListControl, 0, &lvc);
    lvc.pszText = const_cast<LPWSTR>((LPCWSTR)sVersion);
    lvc.fmt = LVCFMT_RIGHT;
    ListView_InsertColumn(hListControl, 1, &lvc);
    lvc.fmt = LVCFMT_LEFT;
    lvc.pszText = const_cast<LPWSTR>((LPCWSTR)sInstVersion);
    ListView_InsertColumn(hListControl, 2, &lvc);

    int index = 0;
    for (auto info : m_plugins)
    {
        wchar_t buf[1024] = { 0 };
        wcscpy_s(buf, info.name.c_str());
        LVITEM item = { 0 };
        item.iItem = index;
        item.mask = LVIF_TEXT;
        item.pszText = buf;
        index = ListView_InsertItem(hListControl, &item);
        wcscpy_s(buf, CStringUtils::Format(L"%d.%d", info.version / 100, info.version % 100).c_str());
        ListView_SetItemText(hListControl, index, 1, buf);
        wcscpy_s(buf, CStringUtils::Format(L"%d.%d", info.installedversion / 100, info.installedversion % 100).c_str());
        ListView_SetItemText(hListControl, index, 2, buf);
        ListView_SetCheckState(hListControl, index, info.installedversion > 0);
        ++index;
    }

    ListView_SetColumnWidth(hListControl, 0, LVSCW_AUTOSIZE_USEHEADER);
    ListView_SetColumnWidth(hListControl, 1, LVSCW_AUTOSIZE_USEHEADER);
    ListView_SetColumnWidth(hListControl, 2, LVSCW_AUTOSIZE_USEHEADER);

    SetWindowTheme(hListControl, L"Explorer", NULL);

    SendMessage(hListControl, WM_SETREDRAW, TRUE, 0);
}

bool CCmdPluginsConfig::Execute()
{
    CPluginsConfigDlg dlg(m_Obj);
    dlg.DoModal(hRes, IDD_PLUGINSCONFIGDLG, GetHwnd());

    return true;
}
