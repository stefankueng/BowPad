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
#include "stdafx.h"
#include "ScintillaWnd.h"
#include "XPMIcons.h"
#include "UnicodeUtils.h"

const int SC_MARGE_LINENUMBER = 0;
const int SC_MARGE_SYBOLE = 1;
const int SC_MARGE_FOLDER = 2;

const int MARK_BOOKMARK = 24;
const int MARK_HIDELINESBEGIN = 23;
const int MARK_HIDELINESEND = 22;

bool CScintillaWnd::Init(HINSTANCE hInst, HWND hParent)
{
    Scintilla_RegisterClasses(hInst);

    CreateEx(0, WS_CHILD | WS_VISIBLE, hParent, 0, L"Scintilla");

    if (!*this)
    {
        return false;
    }

    m_pSciMsg = (SciFnDirect)SendMessage(*this, SCI_GETDIRECTFUNCTION, 0, 0);
    m_pSciWndData = (sptr_t)SendMessage(*this, SCI_GETDIRECTPOINTER, 0, 0);

    if (m_pSciMsg==nullptr || m_pSciWndData==0)
        return false;

    m_docScroll.InitScintilla(*this);

    Call(SCI_SETMARGINMASKN, SC_MARGE_FOLDER, SC_MASK_FOLDERS);
    Call(SCI_SETMARGINWIDTHN, SC_MARGE_FOLDER, 14);

    Call(SCI_SETMARGINMASKN, SC_MARGE_SYBOLE, (1<<MARK_BOOKMARK) | (1<<MARK_HIDELINESBEGIN) | (1<<MARK_HIDELINESEND));
    Call(SCI_MARKERSETALPHA, MARK_BOOKMARK, 70);
    Call(SCI_MARKERDEFINEPIXMAP, MARK_BOOKMARK, (LPARAM)bookmark_xpm);
    Call(SCI_MARKERDEFINEPIXMAP, MARK_HIDELINESBEGIN, (LPARAM)acTop_xpm);
    Call(SCI_MARKERDEFINEPIXMAP, MARK_HIDELINESEND, (LPARAM)acBottom_xpm);

    Call(SCI_SETMARGINSENSITIVEN, SC_MARGE_FOLDER, true);
    Call(SCI_SETMARGINSENSITIVEN, SC_MARGE_SYBOLE, true);

    Call(SCI_SETFOLDFLAGS, 16);
    Call(SCI_SETSCROLLWIDTHTRACKING, true);
    Call(SCI_SETSCROLLWIDTH, 1); // default empty document: override default width

    Call(SCI_MARKERDEFINE, SC_MARKNUM_FOLDEROPEN, SC_MARK_BOXMINUS);
    Call(SCI_MARKERDEFINE, SC_MARKNUM_FOLDER, SC_MARK_BOXPLUS);
    Call(SCI_MARKERDEFINE, SC_MARKNUM_FOLDERSUB, SC_MARK_VLINE);
    Call(SCI_MARKERDEFINE, SC_MARKNUM_FOLDERTAIL, SC_MARK_LCORNER);
    Call(SCI_MARKERDEFINE, SC_MARKNUM_FOLDEREND, SC_MARK_BOXPLUSCONNECTED);
    Call(SCI_MARKERDEFINE, SC_MARKNUM_FOLDEROPENMID, SC_MARK_BOXMINUSCONNECTED);
    Call(SCI_MARKERDEFINE, SC_MARKNUM_FOLDERMIDTAIL, SC_MARK_TCORNER);
    Call(SCI_MARKERENABLEHIGHLIGHT, 0);

    const unsigned long foldmarkfore = RGB(250,250,250);
    Call(SCI_MARKERSETFORE, SC_MARKNUM_FOLDEROPEN, foldmarkfore);
    Call(SCI_MARKERSETFORE, SC_MARKNUM_FOLDER, foldmarkfore);
    Call(SCI_MARKERSETFORE, SC_MARKNUM_FOLDERSUB, foldmarkfore);
    Call(SCI_MARKERSETFORE, SC_MARKNUM_FOLDERTAIL, foldmarkfore);
    Call(SCI_MARKERSETFORE, SC_MARKNUM_FOLDEREND, foldmarkfore);
    Call(SCI_MARKERSETFORE, SC_MARKNUM_FOLDEROPENMID, foldmarkfore);
    Call(SCI_MARKERSETFORE, SC_MARKNUM_FOLDERMIDTAIL, foldmarkfore);

    const unsigned long foldmarkback = RGB(50,50,50);
    Call(SCI_MARKERSETBACK, SC_MARKNUM_FOLDEROPEN, foldmarkback);
    Call(SCI_MARKERSETBACK, SC_MARKNUM_FOLDER, foldmarkback);
    Call(SCI_MARKERSETBACK, SC_MARKNUM_FOLDERSUB, foldmarkback);
    Call(SCI_MARKERSETBACK, SC_MARKNUM_FOLDERTAIL, foldmarkback);
    Call(SCI_MARKERSETBACK, SC_MARKNUM_FOLDEREND, foldmarkback);
    Call(SCI_MARKERSETBACK, SC_MARKNUM_FOLDEROPENMID, foldmarkback);
    Call(SCI_MARKERSETBACK, SC_MARKNUM_FOLDERMIDTAIL, foldmarkback);

    Call(SCI_SETWRAPVISUALFLAGS, SC_WRAPVISUALFLAG_NONE);
    Call(SCI_SETWRAPMODE, SC_WRAP_NONE);

    Call(SCI_STYLESETFORE, STYLE_DEFAULT, ::GetSysColor(COLOR_WINDOWTEXT));
    Call(SCI_STYLESETBACK, STYLE_DEFAULT, ::GetSysColor(COLOR_WINDOW));
    Call(SCI_SETSELFORE, TRUE, ::GetSysColor(COLOR_HIGHLIGHTTEXT));
    Call(SCI_SETSELBACK, TRUE, ::GetSysColor(COLOR_HIGHLIGHT));
    Call(SCI_SETCARETFORE, ::GetSysColor(COLOR_WINDOWTEXT));
    Call(SCI_SETFONTQUALITY, SC_EFF_QUALITY_LCD_OPTIMIZED);

    Call(SCI_STYLESETVISIBLE, STYLE_CONTROLCHAR, TRUE);

    Call(SCI_SETCARETLINEVISIBLE, true);
    Call(SCI_SETCARETLINEVISIBLEALWAYS, true);
    Call(SCI_SETCARETLINEBACK, ::GetSysColor(COLOR_WINDOWTEXT));
    Call(SCI_SETCARETLINEBACKALPHA, 20);

    // For Ctrl+C, use SCI_COPYALLOWLINE instead of SCI_COPY
    Call(SCI_ASSIGNCMDKEY, 'C'+(SCMOD_CTRL<<16), SCI_COPYALLOWLINE);

    return true;
}


bool CScintillaWnd::InitScratch( HINSTANCE hInst )
{
    Scintilla_RegisterClasses(hInst);

    CreateEx(0, 0, NULL, 0, L"Scintilla");

    if (!*this)
    {
        return false;
    }

    m_pSciMsg = (SciFnDirect)SendMessage(*this, SCI_GETDIRECTFUNCTION, 0, 0);
    m_pSciWndData = (sptr_t)SendMessage(*this, SCI_GETDIRECTPOINTER, 0, 0);

    if (m_pSciMsg==nullptr || m_pSciWndData==0)
        return false;

    return true;
}

LRESULT CALLBACK CScintillaWnd::WinMsgHandler( HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam )
{
    NMHDR *hdr = (NMHDR *)lParam;
    switch (uMsg)
    {
    case WM_NOTIFY:
        if(hdr->code == NM_COOLSB_CUSTOMDRAW)
        {
            return m_docScroll.HandleCustomDraw(wParam, (NMCSBCUSTOMDRAW *)lParam);
        }
        break;
    default:
        break;
    }
    if (prevWndProc)
        return CallWindowProc(prevWndProc, hwnd, uMsg, wParam, lParam);
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

void CScintillaWnd::UpdateLineNumberWidth()
{
    int linesVisible = (int) Call(SCI_LINESONSCREEN);
    if (linesVisible)
    {
        int firstVisibleLineVis = (int) Call(SCI_GETFIRSTVISIBLELINE);
        int lastVisibleLineVis = linesVisible + firstVisibleLineVis + 1;
        int i = 0;
        while (lastVisibleLineVis)
        {
            lastVisibleLineVis /= 10;
            i++;
        }
        i = max(i, 3);
        {
            int pixelWidth = int(8 + i * Call(SCI_TEXTWIDTH, STYLE_LINENUMBER, (LPARAM)"8"));
            Call(SCI_SETMARGINWIDTHN, SC_MARGE_LINENUMBER, pixelWidth);
        }
    }

}

void CScintillaWnd::SaveCurrentPos(CPosData * pPos)
{
    pPos->m_nFirstVisibleLine   = Call(SCI_GETFIRSTVISIBLELINE);
    pPos->m_nFirstVisibleLine   = Call(SCI_DOCLINEFROMVISIBLE, pPos->m_nFirstVisibleLine);

    pPos->m_nStartPos           = Call(SCI_GETANCHOR);
    pPos->m_nEndPos             = Call(SCI_GETCURRENTPOS);
    pPos->m_xOffset             = Call(SCI_GETXOFFSET);
    pPos->m_nSelMode            = Call(SCI_GETSELECTIONMODE);
    pPos->m_nScrollWidth        = Call(SCI_GETSCROLLWIDTH);
}

void CScintillaWnd::RestoreCurrentPos(CPosData pos)
{
    Call(SCI_GOTOPOS, 0);

    Call(SCI_SETSELECTIONMODE, pos.m_nSelMode);
    Call(SCI_SETANCHOR, pos.m_nStartPos);
    Call(SCI_SETCURRENTPOS, pos.m_nEndPos);
    Call(SCI_CANCEL);
    if (Call(SCI_GETWRAPMODE) != SC_WRAP_WORD)
    {
        // only offset if not wrapping, otherwise the offset isn't needed at all
        Call(SCI_SETSCROLLWIDTH, pos.m_nScrollWidth);
        Call(SCI_SETXOFFSET, pos.m_xOffset);
    }
    Call(SCI_CHOOSECARETX);

    size_t lineToShow = Call(SCI_VISIBLEFROMDOCLINE, pos.m_nFirstVisibleLine);
    Call(SCI_LINESCROLL, 0, lineToShow);
}

void CScintillaWnd::SetupLexerForExt( const std::wstring& ext )
{
    auto lexerdata = CLexStyles::Instance().GetLexerDataForExt(CUnicodeUtils::StdGetUTF8(ext));
    auto langdata = CLexStyles::Instance().GetKeywordsForExt(CUnicodeUtils::StdGetUTF8(ext));
    SetupLexer(lexerdata, langdata);
}

void CScintillaWnd::SetupLexerForLang( const std::wstring& lang )
{
    auto lexerdata = CLexStyles::Instance().GetLexerDataForLang(CUnicodeUtils::StdGetUTF8(lang));
    auto langdata = CLexStyles::Instance().GetKeywordsForLang(CUnicodeUtils::StdGetUTF8(lang));
    SetupLexer(lexerdata, langdata);
}

void CScintillaWnd::SetupLexer( const LexerData& lexerdata, const std::map<int, std::string>& langdata )
{
    SetupDefaultStyles();
    Call(SCI_STYLECLEARALL);

    Call(SCI_SETLEXER, lexerdata.ID);

    for (auto it: lexerdata.Properties)
    {
        Call(SCI_SETPROPERTY, (WPARAM)it.first.c_str(), (LPARAM)it.second.c_str());
    }
    for (auto it: lexerdata.Styles)
    {
        Call(SCI_STYLESETFORE, it.first, it.second.ForegroundColor);
        Call(SCI_STYLESETBACK, it.first, it.second.BackgroundColor);
        if (!it.second.FontName.empty())
            Call(SCI_STYLESETFONT, it.first, (LPARAM)it.second.FontName.c_str());
        switch (it.second.FontStyle)
        {
        case FONTSTYLE_NORMAL:
            break;  // do nothing
        case FONTSTYLE_BOLD:
            Call(SCI_STYLESETBOLD, it.first, 1);
            break;
        case FONTSTYLE_ITALIC:
            Call(SCI_STYLESETITALIC, it.first, 1);
            break;
        case FONTSTYLE_UNDERLINED:
            Call(SCI_STYLESETUNDERLINE, it.first, 1);
            break;
        }
        if (it.second.FontSize)
            Call(SCI_STYLESETSIZE, it.first, it.second.FontSize);
    }
    for (auto it: langdata)
    {
        Call(SCI_SETKEYWORDS, it.first, (LPARAM)it.second.c_str());
    }
}

void CScintillaWnd::SetupDefaultStyles()
{
    Call(SCI_STYLERESETDEFAULT);
    // if possible, use the Consolas font
    // to determine whether Consolas is available, try to create
    // a font with it.
    HFONT hFont = CreateFont(0, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, FIXED_PITCH, L"Consolas");
    if (hFont)
    {
        Call(SCI_STYLESETFONT, STYLE_DEFAULT, (LPARAM)"Consolas");
        DeleteObject(hFont);
    }
    else
        Call(SCI_STYLESETFONT, STYLE_DEFAULT, (LPARAM)"Courier New");
    Call(SCI_STYLESETSIZE, STYLE_DEFAULT, 10);

    Call(SCI_INDICSETSTYLE, STYLE_SELECTION_MARK, INDIC_ROUNDBOX);
    Call(SCI_INDICSETALPHA, STYLE_SELECTION_MARK, 100);
    Call(SCI_INDICSETUNDER, STYLE_SELECTION_MARK, true);
    Call(SCI_INDICSETFORE, STYLE_SELECTION_MARK, RGB(0,255,0));
}

void CScintillaWnd::MarginClick( Scintilla::SCNotification * pNotification )
{
    int lineClick = int(Call(SCI_LINEFROMPOSITION, pNotification->position, 0));
    int levelClick = int(Call(SCI_GETFOLDLEVEL, lineClick, 0));
    if (levelClick & SC_FOLDLEVELHEADERFLAG)
    {
        if (pNotification->modifiers & SCMOD_SHIFT)
        {
            // Ensure all children visible
            Call(SCI_SETFOLDEXPANDED, lineClick, 1);
            Expand(lineClick, true, true, 100, levelClick);
        }
        else if (pNotification->modifiers & SCMOD_CTRL)
        {
            if (Call(SCI_GETFOLDEXPANDED, lineClick, 0))
            {
                // Contract this line and all children
                Call(SCI_SETFOLDEXPANDED, lineClick, 0);
                Expand(lineClick, false, true, 0, levelClick);
            }
            else
            {
                // Expand this line and all children
                Call(SCI_SETFOLDEXPANDED, lineClick, 1);
                Expand(lineClick, true, true, 100, levelClick);
            }
        }
        else
        {
            // Toggle this line
            bool mode = Call(SCI_GETFOLDEXPANDED, lineClick) != 0;
            Fold(lineClick, !mode);
        }
    }
}

void CScintillaWnd::Expand(int &line, bool doExpand, bool force, int visLevels, int level)
{
    int lineMaxSubord = int(Call(SCI_GETLASTCHILD, line, level & SC_FOLDLEVELNUMBERMASK));
    line++;
    while (line <= lineMaxSubord)
    {
        if (force)
        {
            if (visLevels > 0)
                Call(SCI_SHOWLINES, line, line);
            else
                Call(SCI_HIDELINES, line, line);
        }
        else
        {
            if (doExpand)
                Call(SCI_SHOWLINES, line, line);
        }
        int levelLine = level;
        if (levelLine == -1)
            levelLine = int(Call(SCI_GETFOLDLEVEL, line, 0));
        if (levelLine & SC_FOLDLEVELHEADERFLAG)
        {
            if (force)
            {
                if (visLevels > 1)
                    Call(SCI_SETFOLDEXPANDED, line, 1);
                else
                    Call(SCI_SETFOLDEXPANDED, line, 0);
                Expand(line, doExpand, force, visLevels - 1);
            }
            else
            {
                if (doExpand)
                {
                    if (!Call(SCI_GETFOLDEXPANDED, line, 0))
                        Call(SCI_SETFOLDEXPANDED, line, 1);

                    Expand(line, true, force, visLevels - 1);
                }
                else
                {
                    Expand(line, false, force, visLevels - 1);
                }
            }
        }
        else
        {
            line++;
        }
    }
}

void CScintillaWnd::Fold(int line, bool mode)
{
    int endStyled = (int)Call(SCI_GETENDSTYLED);
    int len = (int)Call(SCI_GETTEXTLENGTH);

    if (endStyled < len)
        Call(SCI_COLOURISE, 0, -1);

    int headerLine;
    int level = (int)Call(SCI_GETFOLDLEVEL, line);

    if (level & SC_FOLDLEVELHEADERFLAG)
        headerLine = line;
    else
    {
        headerLine = (int)Call(SCI_GETFOLDPARENT, line);
        if (headerLine == -1)
            return;
    }

    if ((Call(SCI_GETFOLDEXPANDED, headerLine) != 0) != mode)
    {
        Call(SCI_TOGGLEFOLD, headerLine);
    }
}

void CScintillaWnd::MarkSelectedWord()
{
    LRESULT firstline = Call(SCI_GETFIRSTVISIBLELINE);
    LRESULT lastline = firstline + Call(SCI_LINESONSCREEN);
    long startstylepos = (long)Call(SCI_POSITIONFROMLINE, firstline);
    long endstylepos = (long)Call(SCI_POSITIONFROMLINE, lastline) + (long)Call(SCI_LINELENGTH, lastline);
    if (endstylepos < 0)
        endstylepos = (long)Call(SCI_GETLENGTH)-startstylepos;

    int len = endstylepos - startstylepos + 1;
    // reset indicators
    Call(SCI_SETINDICATORCURRENT, STYLE_SELECTION_MARK);
    Call(SCI_INDICATORCLEARRANGE, startstylepos, len);

    int selTextLen = (int)Call(SCI_GETSELTEXT);
    if (selTextLen == 0)
        return;

    size_t selStartLine = Call(SCI_LINEFROMPOSITION, Call(SCI_GETSELECTIONSTART), 0);
    size_t selEndLine   = Call(SCI_LINEFROMPOSITION, Call(SCI_GETSELECTIONEND), 0);
    if (selStartLine != selEndLine)
        return;

    std::unique_ptr<char[]> seltextbuffer(new char[selTextLen + 1]);
    Call(SCI_GETSELTEXT, 0, (LPARAM)(char*)seltextbuffer.get());
    if (seltextbuffer[0] == 0)
        return;

    std::unique_ptr<char[]> textbuffer(new char[len + 1]);
    Scintilla::Sci_TextRange textrange;
    textrange.lpstrText = textbuffer.get();
    textrange.chrg.cpMin = startstylepos;
    textrange.chrg.cpMax = endstylepos;
    Call(SCI_GETTEXTRANGE, 0, (LPARAM)&textrange);

    char * startPos = strstr(textbuffer.get(), seltextbuffer.get());
    while (startPos)
    {
        Call(SCI_INDICATORFILLRANGE, startstylepos + ((char*)startPos - (char*)textbuffer.get()), selTextLen-1);
        startPos = strstr(startPos+1, seltextbuffer.get());
    }
}

bool CScintillaWnd::GetSelectedCount(size_t& selByte, size_t& selLine)
{
    // return false if it's multi-selection or rectangle selection
    if ((Call(SCI_GETSELECTIONS) > 1) || Call(SCI_SELECTIONISRECTANGLE))
        return false;
    size_t start = Call(SCI_GETSELECTIONSTART);
    size_t end = Call(SCI_GETSELECTIONEND);
    selByte = (start < end)?end-start:start-end;

    start = Call(SCI_LINEFROMPOSITION, start);
    end = Call(SCI_LINEFROMPOSITION, end);
    selLine = (start < end)?end-start:start-end;
    if (selLine)
        ++selLine;

    return true;
};

LRESULT CALLBACK CScintillaWnd::HandleScrollbarCustomDraw( WPARAM wParam, NMCSBCUSTOMDRAW * pCustDraw )
{
    m_docScroll.SetCurLine(Call(SCI_LINEFROMPOSITION, Call(SCI_GETCURRENTPOS)));
    m_docScroll.SetTotalLines(Call(SCI_GETLINECOUNT));
    return m_docScroll.HandleCustomDraw(wParam, pCustDraw);
}
