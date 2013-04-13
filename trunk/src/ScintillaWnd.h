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
#pragma once
#include "BaseWindow.h"
#include "Scintilla.h"
#include "Document.h"
#include "LexStyles.h"
#include "DocScroll.h"

#define STYLE_SELECTION_MARK        (STYLE_DEFAULT-1)


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
    void MarginClick(Scintilla::SCNotification * pNotification);
    void MarkSelectedWord();
    void MatchBraces();
    bool GetSelectedCount(size_t& selByte, size_t& selLine);
    LRESULT CALLBACK HandleScrollbarCustomDraw( WPARAM wParam, NMCSBCUSTOMDRAW * pCustDraw );

protected:
    virtual LRESULT CALLBACK WinMsgHandler( HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam );

    void SetupLexer(const LexerData& lexerdata, const std::map<int, std::string>& langdata);
    void SetupDefaultStyles();
    void Expand(int &line, bool doExpand, bool force = false, int visLevels = 0, int level = -1);
    void Fold(int line, bool mode);
private:
    SciFnDirect                 m_pSciMsg;
    sptr_t                      m_pSciWndData;
    CDocScroll                  m_docScroll;
};

