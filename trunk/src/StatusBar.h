#pragma once
#include "BaseWindow.h"

class CStatusBar : public CWindow
{
public :
    CStatusBar(HINSTANCE hInst) : CWindow(hInst) {};
    virtual ~CStatusBar(){}

    bool Init(HINSTANCE hInst, HWND hParent, int nbParts);
    int GetHeight() const { return m_height; }

    virtual LRESULT CALLBACK WinMsgHandler( HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam );

private:
    int     m_height;
};

