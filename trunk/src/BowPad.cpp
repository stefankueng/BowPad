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

HINSTANCE hInst;

int APIENTRY _tWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPTSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(nCmdShow);

    HRESULT hr = CoInitialize(NULL);
    if (FAILED(hr))
    {
        return FALSE;
    }

    hInst = hInstance;

    MSG msg = {0};
    CMainWindow mainWindow(hInstance);
    if (mainWindow.RegisterAndCreateWindow())
    {
        CCmdLineParser parser(lpCmdLine);
        if (parser.HasVal(L"path"))
        {
            std::vector<std::wstring> files;
            files.push_back(parser.GetVal(L"path"));
            mainWindow.OpenFiles(files);
            if (parser.HasVal(L"line"))
            {
                mainWindow.GoToLine(parser.GetLongVal(L"line")-1);
            }
        }
        mainWindow.EnsureAtLeastOneTab();
        // Main message loop:
        while (GetMessage(&msg, NULL, 0, 0))
        {
            if (!CKeyboardShortcutHandler::Instance().TranslateAccelerator(mainWindow, msg.message, msg.wParam, msg.lParam))
            {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        }
    }

    CoUninitialize();

    return (int) msg.wParam;
}




