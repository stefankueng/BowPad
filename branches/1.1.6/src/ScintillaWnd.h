// This file is part of BowPad.
//
// Copyright (C) 2013-2014 - Stefan Kueng
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

#include <vector>

#define INDIC_SELECTION_MARK        (INDIC_CONTAINER+1)
#define INDIC_TAGMATCH              (INDIC_CONTAINER+2)
#define INDIC_TAGATTR               (INDIC_CONTAINER+3)
#define INDIC_FINDTEXT_MARK         (INDIC_CONTAINER+4)

const int SC_MARGE_LINENUMBER = 0;
const int SC_MARGE_SYBOLE = 1;
const int SC_MARGE_FOLDER = 2;

const int MARK_BOOKMARK = 24;

struct XmlMatchedTagsPos
{
    size_t tagOpenStart;
    size_t tagNameEnd;
    size_t tagOpenEnd;

    size_t tagCloseStart;
    size_t tagCloseEnd;
};

struct FindResult
{
    size_t start;
    size_t end;
    bool success;
};

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
    void MarkSelectedWord(bool clear);
    void MatchBraces();
    void GotoBrace();
    void MatchTags();
    bool GetSelectedCount(size_t& selByte, size_t& selLine);
    void DocScrollClear(int type) { m_docScroll.Clear(type); }
    void DocScrollAddLineColor(int type, size_t line, COLORREF clr) { m_docScroll.AddLineColor(type, line, clr); }
    void DocScrollUpdate();
    void DocScrollRemoveLine(int type, size_t line) { m_docScroll.RemoveLine(type, line); }
    void MarkBookmarksInScrollbar();
    void Center(long posStart, long posEnd);
    void SetTabSettings();

    LRESULT CALLBACK HandleScrollbarCustomDraw( WPARAM wParam, NMCSBCUSTOMDRAW * pCustDraw );

protected:
    virtual LRESULT CALLBACK WinMsgHandler(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) override;

    void SetupLexer(const LexerData& lexerdata, const std::map<int, std::string>& langdata);
    void SetupDefaultStyles();

    bool GetXmlMatchedTagsPos( XmlMatchedTagsPos& xmlTags );
    FindResult FindText(const char *text, size_t start, size_t end, int flags);
    FindResult FindOpenTag(const std::string& tagName, size_t start, size_t end);
    size_t FindCloseAngle(size_t startPosition, size_t endPosition);
    FindResult FindCloseTag(const std::string& tagName, size_t start, size_t end);
    std::vector<std::pair<size_t, size_t>> GetAttributesPos(size_t start, size_t end);
    bool IsXMLWhitespace(int ch) { return ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n'; }
    bool AutoBraces( WPARAM wParam );

    void BookmarkAdd(long lineno);
    void BookmarkDelete(int lineno);
    bool IsBookmarkPresent(int lineno);
    void BookmarkToggle(int lineno);

private:
    SciFnDirect                 m_pSciMsg;
    sptr_t                      m_pSciWndData;
    CDocScroll                  m_docScroll;
};
