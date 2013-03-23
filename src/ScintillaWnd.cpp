#include "stdafx.h"
#include "ScintillaWnd.h"

bool CScintillaWnd::Init(HINSTANCE hInst, HWND hParent)
{
    Scintilla_RegisterClasses(hInst);

    CreateEx(WS_EX_ACCEPTFILES, WS_CHILD | WS_VISIBLE | WS_BORDER, hParent, 0, L"Scintilla");

    if (!*this)
    {
        return false;
    }

    return true;
}

LRESULT CALLBACK CScintillaWnd::WinMsgHandler( HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam )
{
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

