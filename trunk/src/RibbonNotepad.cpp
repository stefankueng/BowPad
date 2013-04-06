// RibbonNotepad.cpp : Defines the entry point for the application.
//

#include "stdafx.h"
#include "RibbonNotepad.h"
#include "MainWindow.h"
#include "CmdLineParser.h"

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
        HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_RIBBONNOTEPAD));
        while (GetMessage(&msg, NULL, 0, 0))
        {
            if (!TranslateAccelerator(mainWindow, hAccelTable, &msg))
            {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        }
    }

    CoUninitialize();

    return (int) msg.wParam;
}




