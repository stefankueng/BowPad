// RibbonNotepad.cpp : Defines the entry point for the application.
//

#include "stdafx.h"
#include "RibbonNotepad.h"
#include "MainWindow.h"

HINSTANCE hInst;

int APIENTRY _tWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPTSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);
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




