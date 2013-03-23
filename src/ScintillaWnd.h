#pragma once
#include "BaseWindow.h"
#include "Scintilla.h"

class CScintillaWnd : public CWindow
{
public :
    CScintillaWnd(HINSTANCE hInst) : CWindow(hInst) {};
    virtual ~CScintillaWnd(){}

    bool Init(HINSTANCE hInst, HWND hParent);

    virtual LRESULT CALLBACK WinMsgHandler( HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam );

private:
};

