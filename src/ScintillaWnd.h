#pragma once
#include "BaseWindow.h"
#include "Scintilla.h"

class CScintillaWnd : public CWindow
{
public :
    CScintillaWnd(HINSTANCE hInst)
        : CWindow(hInst)
        , m_pSciMsg(nullptr)
        , m_pSciWndData(0)
    {};
    virtual ~CScintillaWnd(){}

    bool Init(HINSTANCE hInst, HWND hParent);

    sptr_t Call(unsigned int iMessage, uptr_t wParam = 0, sptr_t lParam = 0)
    {
        return m_pSciMsg(m_pSciWndData, iMessage, wParam, lParam);
    }

protected:
    virtual LRESULT CALLBACK WinMsgHandler( HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam );

private:
    SciFnDirect                 m_pSciMsg;
    sptr_t                      m_pSciWndData;
};

