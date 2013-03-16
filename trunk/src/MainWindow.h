#pragma once
#include "BaseWindow.h"
#include "resource.h"

class CMainWindow : public CWindow
{
public:
    CMainWindow(HINSTANCE hInst, const WNDCLASSEX* wcx = NULL);
    ~CMainWindow(void);

    /**
     * Registers the window class and creates the window.
     */
    bool                RegisterAndCreateWindow();

    bool                Initialize();

protected:
    /// the message handler for this window
    LRESULT CALLBACK    WinMsgHandler(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    /// Handles all the WM_COMMAND window messages (e.g. menu commands)
    LRESULT             DoCommand(int id);
};
