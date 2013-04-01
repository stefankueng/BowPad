#pragma once
#include "BaseWindow.h"
#include "Scintilla.h"
#include "Document.h"
#include "LexStyles.h"

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
    bool InitScratch(HINSTANCE hInst);

    sptr_t Call(unsigned int iMessage, uptr_t wParam = 0, sptr_t lParam = 0)
    {
        return m_pSciMsg(m_pSciWndData, iMessage, wParam, lParam);
    }

    void UpdateLineNumberWidth();
    void SaveCurrentPos(CPosData * pPos);
    void RestoreCurrentPos(CPosData pos);
    void SetupLexerForExt(const std::wstring& ext);
    void SetupLexerForLang(const std::wstring& lang);

protected:
    virtual LRESULT CALLBACK WinMsgHandler( HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam );

    void SetupLexer(const LexerData& lexerdata, const std::map<int, std::string>& langdata);
    void SetupDefaultStyles();

private:
    SciFnDirect                 m_pSciMsg;
    sptr_t                      m_pSciWndData;
};

