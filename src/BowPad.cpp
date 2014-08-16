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
#include "version.h"

HINSTANCE hInst;
HINSTANCE hRes;

#pragma pack(push)  // push current alignment to stack
#pragma pack(1)     // set alignment to 1 byte boundary

extern const char *const bullet_red[] = {
    /* columns rows colors chars-per-pixel */
    "16 16 26 1",
    "  c #B4442F",
    ". c #CB4831",
    "X c #CA4A33",
    "o c #CB4A33",
    "O c #CC4932",
    "+ c #CC4A33",
    "@ c #CB4B34",
    "# c #CB4B35",
    "$ c #CD4C35",
    "% c #D95A42",
    "& c #DB5D44",
    "* c #DB5D45",
    "= c #FA6D4D",
    "- c #FB6D4D",
    "; c #F97355",
    ": c #FA795A",
    "> c #FD7B5F",
    ", c #FF7C5F",
    "< c #FF7C60",
    "1 c #FF886D",
    "2 c #FF886F",
    "3 c #FF8C73",
    "4 c #FDA590",
    "5 c #FCA690",
    "6 c #FFAC95",
    "7 c None",
    /* pixels */
    "7777777777777777",
    "7777777777777777",
    "7777777777777777",
    "7777777777777777",
    "777777$O.O$77777",
    "77777$*565&$7777",
    "77777+4:;:4+7777",
    "77777o1-=-1o7777",
    "77777#,<><,#7777",
    "77777 %232% 7777",
    "777777 @X@ 77777",
    "7777777777777777",
    "7777777777777777",
    "7777777777777777",
    "7777777777777777",
    "7777777777777777"
};

#pragma pack(pop)

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
            std::wstring sLangURL = CStringUtils::Format(L"https://bowpad.googlecode.com/svn/branches/%d.%d.%d/Languages/%s/BowPad_%s.lang", BP_VERMAJOR, BP_VERMINOR, BP_VERMICRO, LANGPLAT, lang.c_str());

            // note: text below is in English and not translatable because
            // we try to download the translation file here, so there's no
            // point in having this translated...
            CProgressDlg progDlg;
            progDlg.SetTitle(L"BowPad Update");
            progDlg.SetLine(1, L"Downloading BowPad Language file...");
            progDlg.ResetTimer();
            progDlg.SetTime();
            progDlg.ShowModal(NULL);

            CDownloadFile filedownloader(L"BowPad", &progDlg);

            if (!filedownloader.DownloadFile(sLangURL, langdllpath))
            {
                DeleteFile(langdllpath.c_str());
            }
        }
        if (CAppUtils::HasSameMajorVersion(langdllpath))
        {
            hRes = LoadLibraryEx(langdllpath.c_str(), NULL, DONT_RESOLVE_DLL_REFERENCES | LOAD_LIBRARY_AS_IMAGE_RESOURCE);
            if (hRes == NULL)
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
                std::wstring sIconPath = CStringUtils::Format(L"%s,-%d", CPathUtils::GetLongPathname(CPathUtils::GetModulePath()).c_str(), IDI_BOWPAD_DOC);
                if (RegSetValue(hKey, NULL, REG_SZ, sIconPath.c_str(), 0) == ERROR_SUCCESS)
                {
                    // now tell the shell about the changed icon
                    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, NULL, NULL);
                }
                RegCloseKey(hKey);
            }
        }
        else
            RegCloseKey(hKey);
    }
}

static void SetAppID()
{
    // set the AppID
    typedef HRESULT STDAPICALLTYPE SetCurrentProcessExplicitAppUserModelIDFN(PCWSTR AppID);
    CAutoLibrary hShell = LoadLibraryExW(L"Shell32.dll", NULL, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (hShell)
    {
        SetCurrentProcessExplicitAppUserModelIDFN *pfnSetCurrentProcessExplicitAppUserModelID = (SetCurrentProcessExplicitAppUserModelIDFN*)GetProcAddress(hShell, "SetCurrentProcessExplicitAppUserModelID");
        if (pfnSetCurrentProcessExplicitAppUserModelID)
        {
            if (SysInfo::Instance().IsUACEnabled() && SysInfo::Instance().IsElevated())
                pfnSetCurrentProcessExplicitAppUserModelID(APP_ID_ELEVATED);
            else
                pfnSetCurrentProcessExplicitAppUserModelID(APP_ID);
        }
    }
}

static void ForwardToOtherInstance(HWND hBowPadWnd, LPTSTR lpCmdLine, CCmdLineParser& parser)
{
    int nCmdShow = 0;

    if (::IsZoomed(hBowPadWnd))
        nCmdShow = SW_MAXIMIZE;
    else if (::IsIconic(hBowPadWnd))
        nCmdShow = SW_RESTORE;
    else
        nCmdShow = SW_SHOW;

    ::ShowWindow(hBowPadWnd, nCmdShow);

    ::SetForegroundWindow(hBowPadWnd);

    size_t cmdLineLen = wcslen(lpCmdLine);
    if (cmdLineLen)
    {
        COPYDATASTRUCT cds;
        cds.dwData = CD_COMMAND_LINE;
        if (!parser.HasVal(L"path"))
        {
            // create our own command line with all paths converted to long/full paths
            // since the CWD of the other instance is most likely different
            int nArgs;
            std::wstring sCmdLine;
            LPWSTR * szArglist = CommandLineToArgvW(GetCommandLineW(), &nArgs);
            if (szArglist)
            {
                for (int i = 0; i < nArgs; i++)
                {
                    if (szArglist[i][0] != '/')
                    {
                        std::wstring path = szArglist[i];
                        CPathUtils::NormalizeFolderSeparators(path);
                        path = CPathUtils::GetLongPathname(path);
                        sCmdLine += L"\"" + path + L"\" ";
                    }
                    else
                    {
                        sCmdLine += szArglist[i];
                        sCmdLine += L" ";
                    }
                }

                // Free memory allocated for CommandLineToArgvW arguments.
                LocalFree(szArglist);
            }
            auto ownCmdLine = std::make_unique<wchar_t[]>(sCmdLine.size() + 2);
            wcscpy_s(ownCmdLine.get(), sCmdLine.size() + 2, sCmdLine.c_str());
            cds.cbData = (DWORD)((sCmdLine.size() + 1)*sizeof(wchar_t));
            cds.lpData = ownCmdLine.get();
            SendMessage(hBowPadWnd, WM_COPYDATA, NULL, (LPARAM)(LPVOID)&cds);
        }
        else
        {
            cds.cbData = (DWORD)((cmdLineLen + 1)*sizeof(wchar_t));
            cds.lpData = lpCmdLine;
            SendMessage(hBowPadWnd, WM_COPYDATA, NULL, (LPARAM)(LPVOID)&cds);
        }
    }
}

static bool CheckIfAlreadyRunning()
{
    bool bAlreadyRunning = false;
    ::SetLastError(NO_ERROR);
    std::wstring sID = L"BowPad_EFA99E4D-68EB-4EFA-B8CE-4F5B41104540_" + CAppUtils::GetSessionID();
    ::CreateMutex(NULL, false, sID.c_str());
    if ((GetLastError() == ERROR_ALREADY_EXISTS) ||
        (GetLastError() == ERROR_ACCESS_DENIED))
        bAlreadyRunning = true;
    return bAlreadyRunning;
}

static HWND FindAndWaitForBowPad()
{
    // don't start another instance: reuse the existing one
    // find the window of the existing instance
    ResString clsResName(hInst, IDC_BOWPAD);
    std::wstring clsName = (LPCWSTR)clsResName + CAppUtils::GetSessionID();

    HWND hBowPadWnd = ::FindWindow(clsName.c_str(), NULL);
    // if we don't have a window yet, wait a little while
    // to give the other process time to create the window
    for (int i = 0; !hBowPadWnd && i < 5; i++)
    {
        Sleep(100);
        hBowPadWnd = ::FindWindow(clsName.c_str(), NULL);
    }
    return hBowPadWnd;
}

static void ShowBowPadCommandLineHelp()
{
    std::wstring sMessage = CStringUtils::Format(L"BowPad version %d.%d.%d.%d\nusage: BowPad.exe /path:\"PATH\" [/line:number] [/multiple]\nor: BowPad.exe PATH [/line:number] [/multiple]\nwith /multiple forcing BowPad to open a new instance even if there's already an instance running."
                                                    , BP_VERMAJOR, BP_VERMINOR, BP_VERMICRO, BP_VERBUILD);
    MessageBox(NULL, sMessage.c_str(), L"BowPad", MB_ICONINFORMATION);
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
            mainWindow.SetTabMove(parser.GetVal(L"path"), parser.GetVal(L"savepath"), !!parser.HasKey(L"modified"), (long)line);
            mainWindow.SetFileOpenMRU(false);
        }
    }
    else
    {
        // find out if there are paths specified without the key/value pair syntax
        int nArgs;

        LPWSTR * szArglist = CommandLineToArgvW(GetCommandLineW(), &nArgs);
        if (szArglist)
        {
            size_t line = (size_t)-1;
            if (parser.HasVal(L"line"))
            {
                line = parser.GetLongVal(L"line") - 1;
            }
            for (int i = 1; i < nArgs; i++)
            {
                if (szArglist[i][0] != '/')
                {
                    std::wstring path = szArglist[i];
                    CPathUtils::NormalizeFolderSeparators(path);
                    path = CPathUtils::GetLongPathname(path);
                    mainWindow.SetFileToOpen(path, line);
                }
            }

            // Free memory allocated for CommandLineToArgvW arguments.
            LocalFree(szArglist);
        }
    }
}

int APIENTRY _tWinMain(_In_ HINSTANCE hInstance,
                       _In_opt_ HINSTANCE hPrevInstance,
                       _In_ LPTSTR  lpCmdLine,
                       _In_ int     nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(nCmdShow);

    SetDllDirectory(L"");
    HRESULT hr = CoInitialize(NULL);
    if (FAILED(hr))
        return FALSE;

    CCmdLineParser parser(lpCmdLine);
    if (parser.HasKey(L"?") || parser.HasKey(L"help"))
    {
        ShowBowPadCommandLineHelp();
        return FALSE;
    }

    bool bAlreadyRunning = CheckIfAlreadyRunning();
    if (bAlreadyRunning && !parser.HasKey(L"multiple"))
    {
        HWND hBowPadWnd = FindAndWaitForBowPad();
        if (hBowPadWnd)
        {
            ForwardToOtherInstance(hBowPadWnd, lpCmdLine, parser);
            return 0;
        }
    }

    SetAppID();

    CIniSettings::Instance().SetIniPath(CAppUtils::GetDataPath() + L"\\settings");
    hInst = hInstance;
    hRes = hInstance;
    LoadLanguage(hInstance);

    SetIcon();

    MSG msg = { 0 };
    CMainWindow mainWindow(hRes);
    if (mainWindow.RegisterAndCreateWindow())
    {
        ParseCommandLine(parser, mainWindow);

        // force CWD to the install path to avoid the CWD being locked:
        // if BowPad is started from another path (e.g. via double click on a text file in
        // explorer), the CWD is the directory of that file. As long as BowPad runs with the CWD
        // set to that dir, that dir can't be removed or renamed due to the lock.
        ::SetCurrentDirectory(CPathUtils::GetModuleDir().c_str());

        mainWindow.EnsureAtLeastOneTab();
        // Main message loop:
        while (GetMessage(&msg, NULL, 0, 0))
        {
            if (!CKeyboardShortcutHandler::Instance().TranslateAccelerator(mainWindow, msg.message, msg.wParam, msg.lParam) &&
                !CDialog::IsDialogMessage(&msg))
            {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        }
    }

    CoUninitialize();

    return (int)msg.wParam;
}
