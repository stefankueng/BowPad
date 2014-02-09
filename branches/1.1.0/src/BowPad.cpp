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

#include <Shellapi.h>

HINSTANCE hInst;
HINSTANCE hRes;


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
    {
        return FALSE;
    }

    bool bAlreadyRunning = false;
    ::SetLastError(NO_ERROR);
    std::wstring sID = L"BowPad_EFA99E4D-68EB-4EFA-B8CE-4F5B41104540_" + CAppUtils::GetSessionID();
    ::CreateMutex(NULL, false, sID.c_str());
    if ((GetLastError() == ERROR_ALREADY_EXISTS) ||
        (GetLastError() == ERROR_ACCESS_DENIED))
        bAlreadyRunning = true;

    CCmdLineParser parser(lpCmdLine);

    if (bAlreadyRunning && !parser.HasKey(L"multiple"))
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

        if (hBowPadWnd)
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
                                std::wstring path = CPathUtils::GetLongPathname(szArglist[i]);
                                std::replace(path.begin(), path.end(), '/', '\\');
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
                    std::unique_ptr<wchar_t[]> ownCmdLine(new wchar_t[sCmdLine.size() + 2]);
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
            return 0;
        }
    }

    // set the AppID
    typedef HRESULT STDAPICALLTYPE SetCurrentProcessExplicitAppUserModelIDFN(PCWSTR AppID);
    CAutoLibrary hShell = AtlLoadSystemLibraryUsingFullPath(_T("shell32.dll"));
    if (hShell)
    {
        SetCurrentProcessExplicitAppUserModelIDFN *pfnSetCurrentProcessExplicitAppUserModelID = (SetCurrentProcessExplicitAppUserModelIDFN*)GetProcAddress(hShell, "SetCurrentProcessExplicitAppUserModelID");
        if (pfnSetCurrentProcessExplicitAppUserModelID)
        {
            pfnSetCurrentProcessExplicitAppUserModelID(APP_ID);
        }
    }

    CIniSettings::Instance().SetIniPath(CAppUtils::GetDataPath() + L"\\settings");
    hInst = hInstance;
    hRes = hInstance;

    // load the language dll if required
    std::wstring lang = CIniSettings::Instance().GetString(L"UI", L"language", L"");
    if (!lang.empty())
    {
        std::wstring langdllpath = CAppUtils::GetDataPath(hInstance);
        langdllpath += L"\\BowPad_";
        langdllpath += lang;
        langdllpath += L".lang";
        if (CAppUtils::HasSameMajorVersion(langdllpath))
        {
            hRes = LoadLibraryEx(langdllpath.c_str(), NULL, DONT_RESOLVE_DLL_REFERENCES | LOAD_LIBRARY_AS_IMAGE_RESOURCE);
            if (hRes == NULL)
                hRes = hInst;
        }
    }

    MSG msg = { 0 };
    CMainWindow mainWindow(hRes);
    if (mainWindow.RegisterAndCreateWindow())
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
            }
            if (parser.HasKey(L"tabmove") && parser.HasKey(L"savepath"))
            {
                mainWindow.SetTabMove(parser.GetVal(L"path"), parser.GetVal(L"savepath"), !!parser.HasKey(L"modified"), (long)line);
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
                        std::wstring path = CPathUtils::GetLongPathname(szArglist[i]);
                        std::replace(path.begin(), path.end(), '/', '\\');
                        mainWindow.SetFileToOpen(path, line);
                    }
                }

                // Free memory allocated for CommandLineToArgvW arguments.
                LocalFree(szArglist);
            }
        }

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
