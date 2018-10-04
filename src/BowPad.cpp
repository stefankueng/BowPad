// This file is part of BowPad.
//
// Copyright (C) 2013-2018 - Stefan Kueng
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
#include "BowPad.h"
#include "MainWindow.h"
#include "CmdLineParser.h"
#include "KeyboardShortcutHandler.h"
#include "BaseDialog.h"
#include "AppUtils.h"
#include "SmartHandle.h"
#include "PathUtils.h"
#include "StringUtils.h"
#include "ProgressDlg.h"
#include "DownloadFile.h"
#include "SysInfo.h"
#include "OnOutOfScope.h"
#include "version.h"
#include "CommandHandler.h"
#include "JumpListHelpers.h"
#include <wrl.h>
using Microsoft::WRL::ComPtr;

HINSTANCE hInst;
HINSTANCE hRes;

static void LoadLanguage(HINSTANCE hInstance)
{
    // load the language dll if required
    std::wstring lang = CIniSettings::Instance().GetString(L"UI", L"language", L"");
    if (!lang.empty())
    {
        std::wstring langdllpath = CAppUtils::GetDataPath(hInstance);
        langdllpath += L"\\BowPad_";
        langdllpath += lang;
        langdllpath += L".lang";
        if (!CAppUtils::HasSameMajorVersion(langdllpath))
        {
            // the language dll does not exist or does not match:
            // try downloading the new language dll right now
            // so the user gets the selected language immediately after
            // updating BowPad
            std::wstring sLangURL = CStringUtils::Format(L"https://github.com/stefankueng/BowPad/raw/%d.%d.%d/Languages/%s/BowPad_%s.lang", BP_VERMAJOR, BP_VERMINOR, BP_VERMICRO, LANGPLAT, lang.c_str());

            // note: text below is in English and not translatable because
            // we try to download the translation file here, so there's no
            // point in having this translated...
            CProgressDlg progDlg;
            progDlg.SetTitle(L"BowPad Update");
            progDlg.SetLine(1, L"Downloading BowPad Language file...");
            progDlg.ResetTimer();
            progDlg.SetTime();
            progDlg.ShowModal(nullptr);

            CDownloadFile filedownloader(L"BowPad", &progDlg);

            if (!filedownloader.DownloadFile(sLangURL, langdllpath))
            {
                DeleteFile(langdllpath.c_str());
            }
        }
        if (CAppUtils::HasSameMajorVersion(langdllpath))
        {
            hRes = LoadLibraryEx(langdllpath.c_str(), nullptr, DONT_RESOLVE_DLL_REFERENCES | LOAD_LIBRARY_AS_IMAGE_RESOURCE | LOAD_LIBRARY_AS_DATAFILE);
            if (hRes == nullptr)
                hRes = hInst;
        }
    }
}

static void SetIcon()
{
    HKEY hKey = 0;
    if (RegOpenKey(HKEY_CURRENT_USER, L"Software\\Classes\\Applications\\BowPad.exe", &hKey) == ERROR_SUCCESS)
    {
        // registry key exists, which means at least one file type was associated with BowPad by the user
        RegCloseKey(hKey);
        if (RegOpenKey(HKEY_CURRENT_USER, L"Software\\Classes\\Applications\\BowPad.exe\\DefaultIcon", &hKey) != ERROR_SUCCESS)
        {
            // but the default icon hasn't been set yet: set the default icon now
            if (RegCreateKey(HKEY_CURRENT_USER, L"Software\\Classes\\Applications\\BowPad.exe\\DefaultIcon", &hKey) == ERROR_SUCCESS)
            {
                OnOutOfScope(RegCloseKey(hKey););
                std::wstring sIconPath = CStringUtils::Format(L"%s,-%d", CPathUtils::GetLongPathname(CPathUtils::GetModulePath()).c_str(), IDI_BOWPAD_DOC);
                if (RegSetValue(hKey, nullptr, REG_SZ, sIconPath.c_str(), 0) == ERROR_SUCCESS)
                {
                    // now tell the shell about the changed icon
                    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
                }
            }
        }
        else
        {
            RegCloseKey(hKey);
        }
    }
}

static void SetUserStringKey(LPCWSTR keyName, LPCWSTR subKeyName, const std::wstring& keyValue)
{
    DWORD dwSizeInBytes = DWORD((keyValue.length() + 1) * sizeof(WCHAR));
    auto  status        = SHSetValue(HKEY_CURRENT_USER, keyName, subKeyName, REG_SZ, keyValue.c_str(), dwSizeInBytes);
    if (status != ERROR_SUCCESS)
    {
        std::wstring msg = CStringUtils::Format(L"Registry key '%s' (subkey: '%s') could not be set.",
                                                keyName, subKeyName ? subKeyName : L"(none)");
        MessageBox(nullptr, msg.c_str(), L"BowPad", MB_ICONINFORMATION);
    }
}

static void RegisterContextMenu(bool bAdd)
{
    if (bAdd)
    {
        auto         modulePath = CPathUtils::GetLongPathname(CPathUtils::GetModulePath());
        std::wstring sIconPath  = CStringUtils::Format(L"%s,-%d", modulePath.c_str(), IDI_BOWPAD);
        std::wstring sExePath   = CStringUtils::Format(L"%s /path:\"%%1\"", modulePath.c_str());
        SetUserStringKey(L"Software\\Classes\\*\\shell\\BowPad", nullptr, L"Edit in BowPad");
        SetUserStringKey(L"Software\\Classes\\*\\shell\\BowPad", L"Icon", sIconPath);
        SetUserStringKey(L"Software\\Classes\\*\\shell\\BowPad", L"MultiSelectModel", L"Player");
        SetUserStringKey(L"Software\\Classes\\*\\shell\\BowPad\\Command", nullptr, sExePath);
        SetUserStringKey(L"Software\\Classes\\Directory\\shell\\BowPad", nullptr, L"Open Folder with BowPad");
        SetUserStringKey(L"Software\\Classes\\Directory\\shell\\BowPad", L"Icon", sIconPath);
        SetUserStringKey(L"Software\\Classes\\Directory\\shell\\BowPad\\Command", nullptr, sExePath);
        SetUserStringKey(L"Software\\Classes\\Directory\\Background\\shell\\BowPad", nullptr, L"Open Folder with BowPad");
        SetUserStringKey(L"Software\\Classes\\Directory\\Background\\shell\\BowPad", L"Icon", sIconPath);

        sExePath = CStringUtils::Format(L"%s /path:\"%%V\"", modulePath.c_str());
        SetUserStringKey(L"Software\\Classes\\Directory\\Background\\shell\\BowPad\\Command", nullptr, sExePath);
    }
    else
    {
        SHDeleteKey(HKEY_CURRENT_USER, L"Software\\Classes\\*\\shell\\BowPad");
        SHDeleteKey(HKEY_CURRENT_USER, L"Software\\Classes\\Directory\\shell\\BowPad");
        SHDeleteKey(HKEY_CURRENT_USER, L"Software\\Classes\\Directory\\Background\\shell\\BowPad");
    }
}

static void SetJumplist(LPCTSTR appID)
{
    CoInitialize(nullptr);
    OnOutOfScope(CoUninitialize());
    ComPtr<ICustomDestinationList> pcdl;
    HRESULT                        hr = CoCreateInstance(CLSID_DestinationList, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(pcdl.GetAddressOf()));
    if (SUCCEEDED(hr))
    {
        hr = pcdl->DeleteList(appID);
        hr = pcdl->SetAppID(appID);
        if (FAILED(hr))
            return;

        UINT                 uMaxSlots;
        ComPtr<IObjectArray> poaRemoved;
        hr = pcdl->BeginList(&uMaxSlots, IID_PPV_ARGS(&poaRemoved));
        if (FAILED(hr))
            return;

        ComPtr<IObjectCollection> poc;
        hr = CoCreateInstance(CLSID_EnumerableObjectCollection, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(poc.GetAddressOf()));
        if (FAILED(hr))
            return;

        if (!SysInfo::Instance().IsElevated())
        {
            ResString sTemp(hRes, IDS_RUNASADMIN);

            ComPtr<IShellLink> psladmin;
            hr = CreateShellLink(L"/multiple", sTemp, 4, true, &psladmin);
            if (SUCCEEDED(hr))
            {
                hr = poc->AddObject(psladmin.Get());
            }
        }

        ComPtr<IObjectArray> poa;
        hr = poc.As(&poa);
        if (SUCCEEDED(hr))
        {
            hr = pcdl->AppendKnownCategory(KDC_FREQUENT);
            hr = pcdl->AppendKnownCategory(KDC_RECENT);
            if (!SysInfo::Instance().IsElevated())
                hr = pcdl->AddUserTasks(poa.Get());
            hr = pcdl->CommitList();
        }
    }
}

static void ForwardToOtherInstance(HWND hBowPadWnd, LPCTSTR lpCmdLine, CCmdLineParser& parser)
{
    int nCmdShow = 0;

    if (::IsZoomed(hBowPadWnd))
        nCmdShow = SW_MAXIMIZE;
    else if (::IsIconic(hBowPadWnd))
        nCmdShow = SW_RESTORE;
    else
        nCmdShow = SW_SHOW;
    // if the window is not yet visible, we wait a little bit
    // and we don't make the window visible here: the message we send
    // to open the file might get handled before the RegisterAndCreateWindow
    // in MainWindow.cpp hasn't finished yet. Just let that function make
    // the window visible in the right position.
    if (IsWindowVisible(hBowPadWnd))
    {
        ::ShowWindow(hBowPadWnd, nCmdShow);
        ::SetForegroundWindow(hBowPadWnd);
    }
    else
        Sleep(500);

    size_t cmdLineLen = wcslen(lpCmdLine);
    if (cmdLineLen)
    {
        COPYDATASTRUCT cds;
        cds.dwData = CD_COMMAND_LINE;
        if (!parser.HasVal(L"path"))
        {
            // create our own command line with all paths converted to long/full paths
            // since the CWD of the other instance is most likely different
            int                nArgs;
            std::wstring       sCmdLine;
            const std::wstring commandLine = GetCommandLineW();
            LPWSTR*            szArglist   = CommandLineToArgvW(commandLine.c_str(), &nArgs);
            if (szArglist)
            {
                OnOutOfScope(LocalFree(szArglist););
                bool bOmitNext = false;
                for (int i = 0; i < nArgs; i++)
                {
                    if (bOmitNext)
                    {
                        bOmitNext = false;
                        continue;
                    }
                    if ((szArglist[i][0] != '/') && (szArglist[i][0] != '-'))
                    {
                        std::wstring path = szArglist[i];
                        CPathUtils::NormalizeFolderSeparators(path);
                        path = CPathUtils::GetLongPathname(path);
                        if (!PathFileExists(path.c_str()))
                        {
                            auto pathpos = commandLine.find(szArglist[i]);
                            if (pathpos != std::wstring::npos)
                            {
                                auto tempPath = commandLine.substr(pathpos);
                                if (PathFileExists(tempPath.c_str()))
                                {
                                    CPathUtils::NormalizeFolderSeparators(tempPath);
                                    path = CPathUtils::GetLongPathname(tempPath);
                                    sCmdLine += L"\"" + path + L"\" ";
                                    break;
                                }
                            }
                        }
                        sCmdLine += L"\"" + path + L"\" ";
                    }
                    else
                    {
                        if (wcscmp(&szArglist[i][1], L"z") == 0)
                            bOmitNext = true;
                        else
                        {
                            sCmdLine += szArglist[i];
                            sCmdLine += L" ";
                        }
                    }
                }
            }
            auto ownCmdLine = std::make_unique<wchar_t[]>(sCmdLine.size() + 2);
            wcscpy_s(ownCmdLine.get(), sCmdLine.size() + 2, sCmdLine.c_str());
            cds.cbData = (DWORD)((sCmdLine.size() + 1) * sizeof(wchar_t));
            cds.lpData = ownCmdLine.get();
            SendMessage(hBowPadWnd, WM_COPYDATA, 0, (LPARAM)&cds);
        }
        else
        {
            cds.cbData = (DWORD)((cmdLineLen + 1) * sizeof(wchar_t));
            cds.lpData = (PVOID)lpCmdLine;
            SendMessage(hBowPadWnd, WM_COPYDATA, 0, (LPARAM)&cds);
        }
    }
}

static HWND FindAndWaitForBowPad()
{
    // don't start another instance: reuse the existing one
    // find the window of the existing instance
    ResString    clsResName(hInst, IDC_BOWPAD);
    std::wstring clsName = (LPCWSTR)clsResName + CAppUtils::GetSessionID();

    HWND hBowPadWnd = ::FindWindow(clsName.c_str(), nullptr);
    // if we don't have a window yet, wait a little while
    // to give the other process time to create the window
    for (int i = 0; !hBowPadWnd && i < 20; i++)
    {
        Sleep(100);
        hBowPadWnd = ::FindWindow(clsName.c_str(), NULL);
    }
    // also wait for the window to become visible first
    for (int i = 0; !IsWindowVisible(hBowPadWnd) && i < 20; i++)
    {
        Sleep(100);
    }
    return hBowPadWnd;
}

static void ShowBowPadCommandLineHelp()
{
    std::wstring sMessage = CStringUtils::Format(L"BowPad version %d.%d.%d.%d\nusage: BowPad.exe /path:\"PATH\" [/line:number] [/multiple]\nor: BowPad.exe PATH [/line:number] [/multiple]\nwith /multiple forcing BowPad to open a new instance even if there's already an instance running.", BP_VERMAJOR, BP_VERMINOR, BP_VERMICRO, BP_VERBUILD);
    MessageBox(nullptr, sMessage.c_str(), L"BowPad", MB_ICONINFORMATION);
}

static void ParseCommandLine(CCmdLineParser& parser, CMainWindow& mainWindow)
{
    if (parser.HasVal(L"path"))
    {
        size_t line = (size_t)-1;
        if (parser.HasVal(L"line"))
        {
            line = parser.GetLongVal(L"line") - 1;
        }
        mainWindow.SetFileToOpen(parser.GetVal(L"path"), line);
        if (parser.HasKey(L"elevate") && parser.HasKey(L"savepath"))
        {
            mainWindow.SetElevatedSave(parser.GetVal(L"path"), parser.GetVal(L"savepath"), (long)line);
            mainWindow.SetFileOpenMRU(false);
        }
        if (parser.HasKey(L"tabmove") && parser.HasKey(L"savepath"))
        {
            std::wstring title = parser.HasVal(L"title") ? parser.GetVal(L"title") : L"";
            mainWindow.SetTabMove(parser.GetVal(L"path"), parser.GetVal(L"savepath"), !!parser.HasKey(L"modified"), (long)line, title);
            mainWindow.SetFileOpenMRU(false);
        }
    }
    else
    {
        // find out if there are paths specified without the key/value pair syntax
        int nArgs;

        const std::wstring commandLine = GetCommandLineW();
        LPWSTR*            szArglist   = CommandLineToArgvW(commandLine.c_str(), &nArgs);
        if (szArglist)
        {
            OnOutOfScope(LocalFree(szArglist););
            size_t line = (size_t)-1;
            if (parser.HasVal(L"line"))
            {
                line = parser.GetLongVal(L"line") - 1;
            }

            bool bOmitNext = false;
            for (int i = 1; i < nArgs; i++)
            {
                if (bOmitNext)
                {
                    bOmitNext = false;
                    continue;
                }
                if ((szArglist[i][0] != '/') && (szArglist[i][0] != '-'))
                {
                    auto pathpos = commandLine.find(szArglist[i]);
                    if (pathpos != std::wstring::npos)
                    {
                        auto tempPath = commandLine.substr(pathpos);
                        if (PathFileExists(tempPath.c_str()))
                        {
                            CPathUtils::NormalizeFolderSeparators(tempPath);
                            auto path = CPathUtils::GetLongPathname(tempPath);
                            mainWindow.SetFileToOpen(path, line);
                            break;
                        }
                    }

                    std::wstring path = szArglist[i];
                    CPathUtils::NormalizeFolderSeparators(path);
                    path = CPathUtils::GetLongPathname(path);
                    if (!PathFileExists(path.c_str()))
                    {
                        pathpos = commandLine.find(szArglist[i]);
                        if (pathpos != std::wstring::npos)
                        {
                            auto tempPath = commandLine.substr(pathpos);
                            if (PathFileExists(tempPath.c_str()))
                            {
                                CPathUtils::NormalizeFolderSeparators(tempPath);
                                path = CPathUtils::GetLongPathname(tempPath);
                                mainWindow.SetFileToOpen(path, line);
                                break;
                            }
                        }
                    }
                    mainWindow.SetFileToOpen(path, line);
                }
                else
                {
                    if (wcscmp(&szArglist[i][1], L"z") == 0)
                        bOmitNext = true;
                }
            }
        }
    }
}

int BPMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPCTSTR lpCmdLine, int nCmdShow, bool bAlreadyRunning)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(nCmdShow);

    SetDllDirectory(L"");
    HRESULT hr = CoInitialize(nullptr);
    if (FAILED(hr))
        return -1;
    OnOutOfScope(CoUninitialize(););

    auto parser = std::make_unique<CCmdLineParser>(lpCmdLine);
    if (parser->HasKey(L"?") || parser->HasKey(L"help"))
    {
        ShowBowPadCommandLineHelp();
        return 0;
    }
    if (parser->HasKey(L"register"))
    {
        RegisterContextMenu(true);
        return 0;
    }
    if ((parser->HasKey(L"unregister")) || (parser->HasKey(L"deregister")))
    {
        RegisterContextMenu(false);
        return 0;
    }

    bool isAdminMode = SysInfo::Instance().IsUACEnabled() && SysInfo::Instance().IsElevated();
    if (parser->HasKey(L"admin") && !isAdminMode)
    {
        std::wstring modpath = CPathUtils::GetModulePath();
        SHELLEXECUTEINFO shExecInfo = { sizeof(SHELLEXECUTEINFO) };

        shExecInfo.hwnd = nullptr;
        shExecInfo.lpVerb = L"runas";
        shExecInfo.lpFile = modpath.c_str();
        shExecInfo.lpParameters = parser->getCmdLine();
        shExecInfo.nShow = SW_NORMAL;

        if (ShellExecuteEx(&shExecInfo))
            return 0;
    }
    if (bAlreadyRunning && !parser->HasKey(L"multiple"))
    {
        HWND hBowPadWnd = FindAndWaitForBowPad();
        if (hBowPadWnd)
        {
            ForwardToOtherInstance(hBowPadWnd, lpCmdLine, *parser);
            return 0;
        }
    }

    auto appID = isAdminMode ? APP_ID_ELEVATED : APP_ID;
    SetAppID(appID);
    SetJumplist(appID);

    CIniSettings::Instance().SetIniPath(CAppUtils::GetDataPath() + L"\\settings");
    LoadLanguage(hInstance);

    SetIcon();

    CMainWindow mainWindow(hRes);

    if (!mainWindow.RegisterAndCreateWindow())
        return -1;

    ParseCommandLine(*parser, mainWindow);

    // Don't need the parser any more so don't keep it around taking up space.
    parser.reset();

    // force CWD to the install path to avoid the CWD being locked:
    // if BowPad is started from another path (e.g. via double click on a text file in
    // explorer), the CWD is the directory of that file. As long as BowPad runs with the CWD
    // set to that dir, that dir can't be removed or renamed due to the lock.
    ::SetCurrentDirectory(CPathUtils::GetModuleDir().c_str());

    std::wstring params = L" /multiple";
    if (isAdminMode)
        params += L" /admin";
   SetRelaunchCommand(mainWindow, appID, (CPathUtils::GetModulePath() + params).c_str(), L"BowPad");

    // Main message loop:
    MSG   msg = {0};
    auto& kb  = CKeyboardShortcutHandler::Instance();
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        if (!kb.TranslateAccelerator(mainWindow, msg.message, msg.wParam, msg.lParam) &&
            !CDialog::IsDialogMessage(&msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
    CCommandHandler::ShutDown();
    Animator::ShutDown();
    return (int)msg.wParam;
}

int APIENTRY _tWinMain(_In_ HINSTANCE hInstance,
                       _In_opt_ HINSTANCE hPrevInstance,
                       _In_ LPTSTR lpCmdLine,
                       _In_ int    nCmdShow)
{
    hInst = hInstance;
    hRes  = hInstance;

    const std::wstring sID = L"BowPad_EFA99E4D-68EB-4EFA-B8CE-4F5B41104540_" + CAppUtils::GetSessionID();
    ::SetLastError(NO_ERROR); // Don't do any work between these 3 statements to spoil the error code.
    HANDLE hAppMutex   = ::CreateMutex(nullptr, false, sID.c_str());
    DWORD  mutexStatus = GetLastError();
    OnOutOfScope(CloseHandle(hAppMutex););
    bool bAlreadyRunning = (mutexStatus == ERROR_ALREADY_EXISTS || mutexStatus == ERROR_ACCESS_DENIED);

    auto mainResult = BPMain(hInstance, hPrevInstance, lpCmdLine, nCmdShow, bAlreadyRunning);

    Scintilla_ReleaseResources();

    // Be careful shutting down Scintilla's resources here if any
    // global static objects contain things like CScintillaWnd as members
    // as they will destruct AFTER WinMain. That won't be a good thing
    // if we've released Scintilla resources IN WinMain.

    return mainResult;
}
