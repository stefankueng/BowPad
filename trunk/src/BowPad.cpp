// This file is part of BowPad.
//
// Copyright (C) 2013 - Stefan Kueng
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

#include <Shellapi.h>

HINSTANCE hInst;

int APIENTRY _tWinMain(_In_ HINSTANCE hInstance,
                       _In_opt_ HINSTANCE hPrevInstance,
                       _In_ LPTSTR  lpCmdLine,
                       _In_ int     nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(nCmdShow);

    HRESULT hr = CoInitialize(NULL);
    if (FAILED(hr))
    {
        return FALSE;
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

    MSG msg = {0};
    CMainWindow mainWindow(hInstance);
    if (mainWindow.RegisterAndCreateWindow())
    {
        CCmdLineParser parser(lpCmdLine);
        if (parser.HasVal(L"path"))
        {
            mainWindow.OpenFile(parser.GetVal(L"path"));
            if (parser.HasVal(L"line"))
            {
                mainWindow.GoToLine(parser.GetLongVal(L"line")-1);
            }
        }
        else
        {
            // find out if there are paths specified without the key/value pair syntax
            int nArgs;
            int i;

            LPWSTR * szArglist = CommandLineToArgvW(GetCommandLineW(), &nArgs);
            if( szArglist )
            {
                for( i=1; i<nArgs; i++)
                {
                    std::wstring path = CPathUtils::GetLongPathname(szArglist[i]);
                    mainWindow.OpenFile(path);
                }

                // Free memory allocated for CommandLineToArgvW arguments.
                LocalFree(szArglist);
            }
        }
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
