#include "stdafx.h"
#include "MainWindow.h"
#include "RibbonFramework.h"

#include <memory>

CMainWindow::CMainWindow(HINSTANCE hInst, const WNDCLASSEX* wcx /* = NULL*/)
    : CWindow(hInst, wcx)
{
}

CMainWindow::~CMainWindow(void)
{
}

bool CMainWindow::RegisterAndCreateWindow()
{
    WNDCLASSEX wcx;

    // Fill in the window class structure with default parameters
    wcx.cbSize = sizeof(WNDCLASSEX);
    wcx.style = 0; // Don't use CS_HREDRAW or CS_VREDRAW with a Ribbon
    wcx.lpfnWndProc = CWindow::stWinMsgHandler;
    wcx.cbClsExtra = 0;
    wcx.cbWndExtra = 0;
    wcx.hInstance = hResource;
    wcx.hCursor = NULL;
    wcx.lpszClassName = ResString(hResource, IDC_RIBBONNOTEPAD);
    wcx.hIcon = LoadIcon(hResource, MAKEINTRESOURCE(IDI_SMALL));
    wcx.hbrBackground = (HBRUSH)(COLOR_3DFACE+1);
    wcx.lpszMenuName = NULL;
    wcx.hIconSm = LoadIcon(wcx.hInstance, MAKEINTRESOURCE(IDI_SMALL));
    if (RegisterWindow(&wcx))
    {
        if (Create(WS_CAPTION | WS_MAXIMIZEBOX | WS_MINIMIZEBOX | WS_SIZEBOX | WS_SYSMENU | WS_CLIPCHILDREN, NULL))
        {
            ShowWindow(*this, SW_SHOW);
            UpdateWindow(*this);
            return true;
        }
    }
    return false;
}

LRESULT CALLBACK CMainWindow::WinMsgHandler(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_CREATE:
        {
            m_hwnd = hwnd;
            Initialize();
        }
        break;
    case WM_COMMAND:
        {
            return DoCommand(LOWORD(wParam));
        }
        break;
    case WM_SIZE:
        {
            RECT rect;
            GetClientRect(*this, &rect);
        }
        break;
    case WM_GETMINMAXINFO:
        {
            MINMAXINFO * mmi = (MINMAXINFO*)lParam;
            mmi->ptMinTrackSize.x = 100;
            mmi->ptMinTrackSize.y = 100;
            return 0;
        }
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    case WM_CLOSE:
        DestroyFramework();
        ::DestroyWindow(m_hwnd);
        break;
    default:
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }

    return 0;
};

LRESULT CMainWindow::DoCommand(int id)
{
    switch (id)
    {
    //case IDM_EXIT:
    //    ::PostQuitMessage(0);
    //    return 0;
    default:
        break;
    };
    return 1;
}

bool CMainWindow::Initialize()
{
    // Initializes the Ribbon framework.
    return InitializeFramework(*this);
}

