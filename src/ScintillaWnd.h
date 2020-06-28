// This file is part of BowPad.
//
// Copyright (C) 2013-2020 - Stefan Kueng
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
#include "DocScroll.h"
#include "ScrollTool.h"
#include "AnimationManager.h"

#include <vector>
#include <unordered_map>

class CPosData;

#define INDIC_SELECTION_MARK        (INDIC_CONTAINER+1)
#define INDIC_TAGMATCH              (INDIC_CONTAINER+2)
#define INDIC_TAGATTR               (INDIC_CONTAINER+3)
#define INDIC_FINDTEXT_MARK         (INDIC_CONTAINER+4)
#define INDIC_URLHOTSPOT            (INDIC_CONTAINER+5)
#define INDIC_BRACEMATCH            (INDIC_CONTAINER+6)
#define INDIC_MISSPELLED            (INDIC_CONTAINER+7)
#define INDIC_REGEXCAPTURE          (INDIC_CONTAINER+10)
#define INDIC_REGEXCAPTURE_END      (INDIC_CONTAINER+20)

const int SC_MARGE_LINENUMBER = 0;
const int SC_MARGE_SYMBOL = 1;
const int SC_MARGE_FOLDER = 2;

const int MARK_BOOKMARK = 24;

enum class BraceMatch
{
    Braces,
    Highlight,
    Clear,
};

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

class LexerData;

class CScintillaWnd : public CWindow
{
public :
    CScintillaWnd(HINSTANCE hInst);
    virtual ~CScintillaWnd();

    bool Init(HINSTANCE hInst, HWND hParent, HWND hWndAttachTo = nullptr);
    bool InitScratch(HINSTANCE hInst);
    void StartupDone() { m_eraseBkgnd = false; }

    sptr_t Call(unsigned int iMessage, uptr_t wParam = 0, sptr_t lParam = 0)
    {
        return m_pSciMsg(m_pSciWndData, iMessage, wParam, lParam);
    }
    sptr_t ConstCall(unsigned int iMessage, uptr_t wParam = 0, sptr_t lParam = 0) const
    {
        return m_pSciMsg(m_pSciWndData, iMessage, wParam, lParam);
    }

    void UpdateLineNumberWidth();
    void SaveCurrentPos(CPosData& pos);
    void RestoreCurrentPos(const CPosData& pos);
    void SetupLexerForLang(const std::string& lang);
    void MarginClick(SCNotification * pNotification);
    void MarkSelectedWord(bool clear);
    void MatchBraces(BraceMatch what);
    void GotoBrace();
    void MatchTags();
    bool GetSelectedCount(size_t& selByte, size_t& selLine);
    void DocScrollClear(int type) { m_docScroll.Clear(type); }
    void DocScrollAddLineColor(int type, size_t line, COLORREF clr) { m_docScroll.AddLineColor(type, line, clr); }
    void DocScrollUpdate();
    void DocScrollRemoveLine(int type, size_t line) { m_docScroll.RemoveLine(type, line); }
    void MarkBookmarksInScrollbar();
    void GotoLine(long line);
    void Center(sptr_t posStart, sptr_t posEnd);
    void SetTabSettings(TabSpace ts);
    void SetReadDirection(ReadDirection rd);
    void SetEOLType(int eolType);
    void AppendText(int len, const char* buf);
    std::string GetLine(long line) const;
    std::string GetTextRange(Sci_Position startpos, Sci_Position endpos) const;
    size_t FindText(const std::string& tofind, long startpos, long endpos);
    std::string GetSelectedText(bool useCurrentWordIfSelectionEmpty = false) const;
    std::string GetCurrentWord() const;
    std::string GetCurrentLine() const;
    std::string GetWordChars() const;
    std::string GetWhitespaceChars() const;
    long GetSelTextMarkerCount() const { return m_selTextMarkerCount; }
    long GetCurrentLineNumber() const;
    void VisibleLinesChanged() { m_docScroll.VisibleLinesChanged(); }

    LRESULT CALLBACK HandleScrollbarCustomDraw( WPARAM wParam, NMCSBCUSTOMDRAW * pCustDraw );
    void ReflectEvents(SCNotification * pScn);

protected:
    virtual LRESULT CALLBACK WinMsgHandler(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) override;

    void SetupDefaultStyles();
    void SetupFoldingColors(COLORREF fore, COLORREF back, COLORREF backsel);

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
    CScrollTool                 m_ScrollTool;
    long                        m_selTextMarkerCount;
    bool                        m_bCursorShown;
    bool                        m_bScratch;
    bool                        m_eraseBkgnd;
    int                         m_cursorTimeout;
    bool                        m_bInFolderMargin;
    bool                        m_hasConsolas;
    size_t                      m_LineToScrollToAfterPaint;
    size_t                      m_WrapOffsetToScrollToAfterPaint;
    IUIAnimationVariablePtr     m_animVarGrayFore;
    IUIAnimationVariablePtr     m_animVarGrayBack;
    IUIAnimationVariablePtr     m_animVarGraySel;
    IUIAnimationVariablePtr     m_animVarGrayLineNr;
};
