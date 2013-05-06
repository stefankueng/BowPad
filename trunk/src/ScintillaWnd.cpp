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
#include "BowPad.h"
#include "BowPadUI.h"
#include "XPMIcons.h"
#include "UnicodeUtils.h"
#include "SciLexer.h"

#include <UIRibbon.h>
#include <UIRibbonPropertyHelpers.h>

const int SC_MARGE_LINENUMBER = 0;
const int SC_MARGE_SYBOLE = 1;
const int SC_MARGE_FOLDER = 2;

const int MARK_BOOKMARK = 24;
const int MARK_HIDELINESBEGIN = 23;
const int MARK_HIDELINESEND = 22;


extern IUIFramework * g_pFramework;

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

    m_docScroll.InitScintilla(this);

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

    Call(SCI_SETCARETSTYLE, 1);
    Call(SCI_SETCARETLINEVISIBLE, true);
    Call(SCI_SETCARETLINEVISIBLEALWAYS, true);
    Call(SCI_SETCARETLINEBACK, ::GetSysColor(COLOR_WINDOWTEXT));
    Call(SCI_SETCARETLINEBACKALPHA, 20);
    Call(SCI_SETWHITESPACEFORE, true, RGB(255, 181, 106));
    Call(SCI_SETWHITESPACESIZE, 2);

    // For Ctrl+C, use SCI_COPYALLOWLINE instead of SCI_COPY
    Call(SCI_ASSIGNCMDKEY, 'C'+(SCMOD_CTRL<<16), SCI_COPYALLOWLINE);

    Call(SCI_SETBUFFEREDDRAW, true);
    Call(SCI_SETTWOPHASEDRAW, true);

    Call(SCI_USEPOPUP, 0);

    SetupDefaultStyles();

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
    case WM_CONTEXTMENU:
        {
            POINT pt;
            POINTSTOPOINT(pt, lParam);

            if (pt.x == -1 && pt.y == -1)
            {
                HRESULT hr = E_FAIL;

                // Display the menu in the upper-left corner of the client area, below the ribbon.
                IUIRibbon * pRibbon;
                hr = g_pFramework->GetView(0, IID_PPV_ARGS(&pRibbon));
                if (SUCCEEDED(hr))
                {
                    UINT32 uRibbonHeight = 0;
                    hr = pRibbon->GetHeight(&uRibbonHeight);
                    if (SUCCEEDED(hr))
                    {
                        pt.x = 0;
                        pt.y = uRibbonHeight;
                        ClientToScreen(hwnd, &pt);
                    }
                    pRibbon->Release();
                }
                if (FAILED(hr))
                {
                    // Default to just the upper-right corner of the entire screen.
                    pt.x = 0;
                    pt.y = 0;
                }
            }

            HRESULT hr = E_FAIL;

            // The basic pattern of how to show Contextual UI in a specified location.
            IUIContextualUI * pContextualUI = NULL;
            if (SUCCEEDED(g_pFramework->GetView(cmdContextMap, IID_PPV_ARGS(&pContextualUI))))
            {
                hr = pContextualUI->ShowAtLocation(pt.x, pt.y);
                pContextualUI->Release();
            }

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

        if (it.second.FontStyle & FONTSTYLE_BOLD)
            Call(SCI_STYLESETBOLD, it.first, 1);
        if (it.second.FontStyle & FONTSTYLE_ITALIC)
            Call(SCI_STYLESETITALIC, it.first, 1);
        if (it.second.FontStyle & FONTSTYLE_UNDERLINED)
            Call(SCI_STYLESETUNDERLINE, it.first, 1);

        if (it.second.FontSize)
            Call(SCI_STYLESETSIZE, it.first, it.second.FontSize);
    }
    for (auto it: langdata)
    {
        Call(SCI_SETKEYWORDS, it.first-1, (LPARAM)it.second.c_str());
    }

    SetupDefaultStyles();
}

void CScintillaWnd::SetupDefaultStyles()
{
    Call(SCI_SETSTYLEBITS, 8);
    Call(SCI_STYLERESETDEFAULT);
    // if possible, use the Consolas font
    // to determine whether Consolas is available, try to create
    // a font with it.
    HFONT hFont = CreateFont(0, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, FIXED_PITCH, L"Consolas");
    if (hFont)
    {
        DeleteObject(hFont);
        Call(SCI_STYLESETFONT, STYLE_DEFAULT, (LPARAM)"Consolas");
    }
    else
        Call(SCI_STYLESETFONT, STYLE_DEFAULT, (LPARAM)"Courier New");
    Call(SCI_STYLESETSIZE, STYLE_DEFAULT, 10);

    Call(SCI_INDICSETSTYLE, INDIC_SELECTION_MARK, INDIC_ROUNDBOX);
    Call(SCI_INDICSETALPHA, INDIC_SELECTION_MARK, 100);
    Call(SCI_INDICSETUNDER, INDIC_SELECTION_MARK, true);
    Call(SCI_INDICSETFORE,  INDIC_SELECTION_MARK, RGB(0,255,0));

    Call(SCI_INDICSETSTYLE, INDIC_FINDTEXT_MARK, INDIC_ROUNDBOX);
    Call(SCI_INDICSETALPHA, INDIC_FINDTEXT_MARK, 100);
    Call(SCI_INDICSETUNDER, INDIC_FINDTEXT_MARK, true);
    Call(SCI_INDICSETFORE,  INDIC_FINDTEXT_MARK, RGB(255,255,0));

    Call(SCI_STYLESETFORE, STYLE_BRACELIGHT, RGB(0,150,0));
    Call(SCI_STYLESETBOLD, STYLE_BRACELIGHT, 1);
    Call(SCI_STYLESETFORE, STYLE_BRACEBAD, RGB(255,0,0));
    Call(SCI_STYLESETBOLD, STYLE_BRACEBAD, 1);

    Call(SCI_STYLESETFORE, STYLE_INDENTGUIDE, RGB(150,150,150));

    Call(SCI_INDICSETFORE, INDIC_TAGMATCH, RGB(0x80, 0x00, 0xFF));
    Call(SCI_INDICSETFORE, INDIC_TAGATTR, RGB(0xFF, 0xFF, 0x00));
    Call(SCI_INDICSETSTYLE, INDIC_TAGMATCH, INDIC_ROUNDBOX);
    Call(SCI_INDICSETSTYLE, INDIC_TAGATTR, INDIC_ROUNDBOX);
    Call(SCI_INDICSETALPHA, INDIC_TAGMATCH, 100);
    Call(SCI_INDICSETALPHA, INDIC_TAGATTR, 100);
    Call(SCI_INDICSETUNDER, INDIC_TAGMATCH, true);
    Call(SCI_INDICSETUNDER, INDIC_TAGATTR, true);

    Call(SCI_SETTABWIDTH, 4);
    Call(SCI_SETINDENTATIONGUIDES, SC_IV_LOOKBOTH);
}

void CScintillaWnd::Center(long posStart, long posEnd)
{
    // to make sure the found result is visible
    // When searching up, the beginning of the (possible multiline) result is important, when scrolling down the end
    long testPos = (posStart > posEnd) ? posEnd : posStart;
    Call(SCI_SETCURRENTPOS, testPos);
    long currentlineNumberDoc = (long)Call(SCI_LINEFROMPOSITION, testPos);
    long currentlineNumberVis = (long)Call(SCI_VISIBLEFROMDOCLINE, currentlineNumberDoc);
    Call(SCI_ENSUREVISIBLE, currentlineNumberDoc);    // make sure target line is unfolded

    long firstVisibleLineVis =   (long)Call(SCI_GETFIRSTVISIBLELINE);
    long linesVisible =          (long)Call(SCI_LINESONSCREEN) - 1; //-1 for the scrollbar
    long lastVisibleLineVis =    (long)linesVisible + firstVisibleLineVis;

    // if out of view vertically, scroll line into (center of) view
    int linesToScroll = 0;
    if (currentlineNumberVis < firstVisibleLineVis)
    {
        linesToScroll = currentlineNumberVis - firstVisibleLineVis;
        // use center
        linesToScroll -= linesVisible/2;
    }
    else if (currentlineNumberVis > lastVisibleLineVis)
    {
        linesToScroll = currentlineNumberVis - lastVisibleLineVis;
        // use center
        linesToScroll += linesVisible/2;
    }
    Call(SCI_LINESCROLL, 0, linesToScroll);

    // Make sure the caret is visible, scroll horizontally
    Call(SCI_GOTOPOS, posStart);
    Call(SCI_GOTOPOS, posEnd);

    Call(SCI_SETANCHOR, posStart);
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
    m_docScroll.VisibleLinesChanged();
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
    m_docScroll.VisibleLinesChanged();
}

void CScintillaWnd::MarkSelectedWord()
{
    static std::string lastSelText;
    LRESULT firstline = Call(SCI_GETFIRSTVISIBLELINE);
    LRESULT lastline = firstline + Call(SCI_LINESONSCREEN);
    long startstylepos = (long)Call(SCI_POSITIONFROMLINE, firstline);
    long endstylepos = (long)Call(SCI_POSITIONFROMLINE, lastline) + (long)Call(SCI_LINELENGTH, lastline);
    if (endstylepos < 0)
        endstylepos = (long)Call(SCI_GETLENGTH)-startstylepos;

    int len = endstylepos - startstylepos + 1;
    // reset indicators
    Call(SCI_SETINDICATORCURRENT, INDIC_SELECTION_MARK);
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

    if (lastSelText.compare(seltextbuffer.get()))
    {
        m_docScroll.Clear(DOCSCROLLTYPE_SELTEXT);
        Scintilla::Sci_TextToFind FindText;
        FindText.chrg.cpMin = 0;
        FindText.chrg.cpMax = (long)Call(SCI_GETLENGTH);
        FindText.lpstrText = seltextbuffer.get();
        while (Call(SCI_FINDTEXT, SCFIND_MATCHCASE, (LPARAM)&FindText) >= 0)
        {
            size_t line = Call(SCI_LINEFROMPOSITION, FindText.chrgText.cpMin);
            m_docScroll.AddLineColor(DOCSCROLLTYPE_SELTEXT, line, RGB(0,255,0));
            FindText.chrg.cpMin = FindText.chrgText.cpMax;
        }
        SendMessage(*this, WM_NCPAINT, (WPARAM)1, 0);
    }
    lastSelText = seltextbuffer.get();
}

void CScintillaWnd::MatchBraces()
{
    int braceAtCaret = -1;
    int braceOpposite = -1;

    // find matching brace position
    int caretPos = int(Call(SCI_GETCURRENTPOS));
    WCHAR charBefore = '\0';

    int lengthDoc = int(Call(SCI_GETLENGTH));

    if ((lengthDoc > 0) && (caretPos > 0))
    {
        charBefore = WCHAR(Call(SCI_GETCHARAT, caretPos - 1, 0));
    }
    // Priority goes to character before the caret
    if (charBefore && wcschr(L"[](){}", charBefore))
    {
        braceAtCaret = caretPos - 1;
    }

    if (lengthDoc > 0  && (braceAtCaret < 0))
    {
        // No brace found so check other side
        WCHAR charAfter = WCHAR(Call(SCI_GETCHARAT, caretPos, 0));
        if (charAfter && wcschr(L"[](){}", charAfter))
        {
            braceAtCaret = caretPos;
        }
    }
    if (braceAtCaret >= 0)
        braceOpposite = int(Call(SCI_BRACEMATCH, braceAtCaret, 0));


    if ((braceAtCaret != -1) && (braceOpposite == -1))
    {
        Call(SCI_BRACEBADLIGHT, braceAtCaret);
        Call(SCI_SETHIGHLIGHTGUIDE, 0);
    }
    else
    {
        Call(SCI_BRACEHIGHLIGHT, braceAtCaret, braceOpposite);

        if (Call(SCI_GETINDENTATIONGUIDES) != 0)
        {
            int columnAtCaret = int(Call(SCI_GETCOLUMN, braceAtCaret));
            int columnOpposite = int(Call(SCI_GETCOLUMN, braceOpposite));
            Call(SCI_SETHIGHLIGHTGUIDE, (columnAtCaret < columnOpposite)?columnAtCaret:columnOpposite);
        }
    }
}

void CScintillaWnd::MatchTags()
{
    // basically the same as MatchBraces(), but much more complicated since
    // finding xml tags is harder than just finding braces...

    size_t docStart = 0;
    size_t docEnd = Call(SCI_GETLENGTH);
    Call(SCI_SETINDICATORCURRENT, INDIC_TAGMATCH);
    Call(SCI_INDICATORCLEARRANGE, docStart, docEnd-docStart);
    Call(SCI_SETINDICATORCURRENT, INDIC_TAGATTR);
    Call(SCI_INDICATORCLEARRANGE, docStart, docEnd-docStart);

    int lexer = (int)Call(SCI_GETLEXER);
    if ((lexer != SCLEX_HTML) &&
        (lexer != SCLEX_XML) &&
        (lexer != SCLEX_PHPSCRIPT))
        return;

    // Get the original targets and search options to restore after tag matching operation
    size_t originalStartPos = Call(SCI_GETTARGETSTART);
    size_t originalEndPos = Call(SCI_GETTARGETEND);
    size_t originalSearchFlags = Call(SCI_GETSEARCHFLAGS);

    XmlMatchedTagsPos xmlTags = {0};

    // Detect if it's a xml/html tag
    if (GetXmlMatchedTagsPos(xmlTags))
    {
        Call(SCI_SETINDICATORCURRENT, INDIC_TAGMATCH);
        int openTagTailLen = 2;

        // Coloring the close tag first
        if ((xmlTags.tagCloseStart != -1) && (xmlTags.tagCloseEnd != -1))
        {
            Call(SCI_INDICATORFILLRANGE,  xmlTags.tagCloseStart, xmlTags.tagCloseEnd - xmlTags.tagCloseStart);
            // tag close is present, so it's not single tag
            openTagTailLen = 1;
        }

        // Coloring the open tag
        Call(SCI_INDICATORFILLRANGE,  xmlTags.tagOpenStart, xmlTags.tagNameEnd - xmlTags.tagOpenStart);
        Call(SCI_INDICATORFILLRANGE,  xmlTags.tagOpenEnd - openTagTailLen, openTagTailLen);


        // Coloring its attributes
        std::vector<std::pair<size_t, size_t>> attributes = GetAttributesPos(xmlTags.tagNameEnd, xmlTags.tagOpenEnd - openTagTailLen);
        Call(SCI_SETINDICATORCURRENT,  INDIC_TAGATTR);
        for (size_t i = 0 ; i < attributes.size() ; i++)
        {
            Call(SCI_INDICATORFILLRANGE,  attributes[i].first, attributes[i].second - attributes[i].first);
        }

        // Coloring indent guide line position
        if (Call(SCI_GETINDENTATIONGUIDES) != 0)
        {
            size_t columnAtCaret  = Call(SCI_GETCOLUMN, xmlTags.tagOpenStart);
            size_t columnOpposite = Call(SCI_GETCOLUMN, xmlTags.tagCloseStart);

            size_t lineAtCaret  = Call(SCI_LINEFROMPOSITION, xmlTags.tagOpenStart);
            size_t lineOpposite = Call(SCI_LINEFROMPOSITION, xmlTags.tagCloseStart);

            if (xmlTags.tagCloseStart != -1 && lineAtCaret != lineOpposite)
            {
                Call(SCI_BRACEHIGHLIGHT, xmlTags.tagOpenStart, xmlTags.tagCloseEnd-1);
                Call(SCI_SETHIGHLIGHTGUIDE, (columnAtCaret < columnOpposite)?columnAtCaret:columnOpposite);
            }
        }
    }

    // restore the original targets and search options to avoid the conflict with search/replace function
    Call(SCI_SETTARGETSTART, originalStartPos);
    Call(SCI_SETTARGETEND, originalEndPos);
    Call(SCI_SETSEARCHFLAGS, originalSearchFlags);

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
    m_docScroll.SetCurrentPos(Call(SCI_VISIBLEFROMDOCLINE, Call(SCI_LINEFROMPOSITION, Call(SCI_GETCURRENTPOS))), RGB(40,40,40));
    m_docScroll.SetTotalLines(Call(SCI_VISIBLEFROMDOCLINE, (Call(SCI_GETLINECOUNT))));
    return m_docScroll.HandleCustomDraw(wParam, pCustDraw);
}

bool CScintillaWnd::GetXmlMatchedTagsPos( XmlMatchedTagsPos& xmlTags )
{
    bool tagFound = false;
    size_t caret = Call(SCI_GETCURRENTPOS);
    size_t searchStartPoint = caret;
    size_t styleAt;
    FindResult openFound;

    // Search back for the previous open angle bracket.
    // Keep looking whilst the angle bracket found is inside an XML attribute
    do
    {
        openFound = FindText("<", searchStartPoint, 0, 0);
        styleAt = Call(SCI_GETSTYLEAT, openFound.start);
        searchStartPoint = openFound.start - 1;
    } while (openFound.success && (styleAt == SCE_H_DOUBLESTRING || styleAt == SCE_H_SINGLESTRING) && (int)searchStartPoint > 0);

    if (openFound.success && styleAt != SCE_H_CDATA)
    {
        // Found the "<" before the caret, now check there isn't a > between that position and the caret.
        FindResult closeFound;
        searchStartPoint = openFound.start;
        do
        {
            closeFound = FindText(">", searchStartPoint, caret, 0);
            styleAt = (int)Call(SCI_GETSTYLEAT, closeFound.start);
            searchStartPoint = closeFound.end;
        } while (closeFound.success && (styleAt == SCE_H_DOUBLESTRING || styleAt == SCE_H_SINGLESTRING) && (int)searchStartPoint <= caret);

        if (!closeFound.success)
        {
            // We're in a tag (either a start tag or an end tag)
            int nextChar = (int)Call(SCI_GETCHARAT, openFound.start + 1);


            /////////////////////////////////////////////////////////////////////////
            // CURSOR IN CLOSE TAG
            /////////////////////////////////////////////////////////////////////////
            if ('/' == nextChar)
            {
                xmlTags.tagCloseStart = openFound.start;
                size_t docLength = Call(SCI_GETLENGTH);
                FindResult endCloseTag = FindText(">", caret, docLength, 0);
                if (endCloseTag.success)
                {
                    xmlTags.tagCloseEnd = endCloseTag.end;
                }
                // Now find the tagName
                size_t position = openFound.start + 2;

                // UTF-8 or ASCII tag name
                std::string tagName;
                nextChar = (int)Call(SCI_GETCHARAT, position);
                // Checking for " or ' is actually wrong here, but it means it works better with invalid XML
                while(position < docLength && !IsXMLWhitespace(nextChar) && nextChar != '/' && nextChar != '>' && nextChar != '\"' && nextChar != '\'')
                {
                    tagName.push_back((char)nextChar);
                    ++position;
                    nextChar = (int)Call(SCI_GETCHARAT, position);
                }

                // Now we know where the end of the tag is, and we know what the tag is called
                if (tagName.size() != 0)
                {
                    /* Now we need to find the open tag.  The logic here is that we search for "<TAGNAME",
                     * then check the next character - if it's one of '>', ' ', '\"' then we know we've found
                     * a relevant tag.
                     * We then need to check if either
                     *    a) this tag is a self-closed tag - e.g. <TAGNAME attrib="value" />
                     * or b) this tag has another closing tag after it and before our closing tag
                     *       e.g.  <TAGNAME attrib="value">some text</TAGNAME></TAGNA|ME>
                     *             (cursor represented by |)
                     * If it's either of the above, then we continue searching, but only up to the
                     * the point of the last find. (So in the (b) example above, we'd only search backwards
                     * from the first "<TAGNAME...", as we know there's a close tag for the opened tag.

                     * NOTE::  NEED TO CHECK THE ROTTEN CASE: ***********************************************************
                     * <TAGNAME attrib="value"><TAGNAME>something</TAGNAME></TAGNAME></TAGNA|ME>
                     * Maybe count all closing tags between start point and start of our end tag.???
                     */
                    size_t currentEndPoint = xmlTags.tagCloseStart;
                    size_t openTagsRemaining = 1;
                    FindResult nextOpenTag;
                    do
                    {
                        nextOpenTag = FindOpenTag(tagName, currentEndPoint, 0);
                        if (nextOpenTag.success)
                        {
                            --openTagsRemaining;
                            // Open tag found
                            // Now we need to check how many close tags there are between the open tag we just found,
                            // and our close tag
                            // eg. (Cursor == | )
                            // <TAGNAME attrib="value"><TAGNAME>something</TAGNAME></TAGNAME></TAGNA|ME>
                            //                         ^^^^^^^^ we've found this guy
                            //                                           ^^^^^^^^^^ ^^^^^^^^ Now we need to count these fellas
                            FindResult inbetweenCloseTag;
                            size_t currentStartPosition = nextOpenTag.end;
                            size_t closeTagsFound = 0;
                            bool forwardSearch = (currentStartPosition < currentEndPoint);

                            do
                            {
                                inbetweenCloseTag = FindCloseTag(tagName, currentStartPosition, currentEndPoint);

                                if (inbetweenCloseTag.success)
                                {
                                    ++closeTagsFound;
                                    if (forwardSearch)
                                    {
                                        currentStartPosition = inbetweenCloseTag.end;
                                    }
                                    else
                                    {
                                        currentStartPosition = inbetweenCloseTag.start - 1;
                                    }
                                }

                            } while(inbetweenCloseTag.success);

                            // If we didn't find any close tags between the open and our close,
                            // and there's no open tags remaining to find
                            // then the open we found was the right one, and we can return it
                            if (0 == closeTagsFound && 0 == openTagsRemaining)
                            {
                                xmlTags.tagOpenStart = nextOpenTag.start;
                                xmlTags.tagOpenEnd = nextOpenTag.end + 1;
                                xmlTags.tagNameEnd = nextOpenTag.start + (int)tagName.size() + 1;  /* + 1 to account for '<' */
                                tagFound = true;
                            }
                            else
                            {

                                // Need to find the same number of opening tags, without closing tags etc.
                                openTagsRemaining += closeTagsFound;
                                currentEndPoint = nextOpenTag.start;
                            }
                        }
                    } while (!tagFound && openTagsRemaining > 0 && nextOpenTag.success);
                }
            }
            else
            {
            /////////////////////////////////////////////////////////////////////////
            // CURSOR IN OPEN TAG
            /////////////////////////////////////////////////////////////////////////
                size_t position = openFound.start + 1;
                size_t docLength = (int)Call(SCI_GETLENGTH);

                xmlTags.tagOpenStart = openFound.start;

                std::string tagName;
                nextChar = (int)Call(SCI_GETCHARAT, position);
                // Checking for " or ' is actually wrong here, but it means it works better with invalid XML
                while(position < docLength && !IsXMLWhitespace(nextChar) && nextChar != '/' && nextChar != '>' && nextChar != '\"' && nextChar != '\'')
                {
                    tagName.push_back((char)nextChar);
                    ++position;
                    nextChar = (int)Call(SCI_GETCHARAT, position);
                }

                // Now we know where the end of the tag is, and we know what the tag is called
                if (tagName.size() != 0)
                {
                    // First we need to check if this is a self-closing tag.
                    // If it is, then we can just return this tag to highlight itself.
                    xmlTags.tagNameEnd = openFound.start + tagName.size() + 1;
                    size_t closeAnglePosition = FindCloseAngle(position, docLength);
                    if (-1 != closeAnglePosition)
                    {
                        xmlTags.tagOpenEnd = closeAnglePosition + 1;
                        // If it's a self closing tag
                        if (Call(SCI_GETCHARAT, closeAnglePosition - 1) == '/')
                        {
                            // Set it as found, and mark that there's no close tag
                            xmlTags.tagCloseEnd = (size_t)-1;
                            xmlTags.tagCloseStart = (size_t)-1;
                            tagFound = true;
                        }
                        else
                        {
                            // It's a normal open tag



                            /* Now we need to find the close tag.  The logic here is that we search for "</TAGNAME",
                             * then check the next character - if it's '>' or whitespace followed by '>' then we've
                             * found a relevant tag.
                             * We then need to check if
                             * our tag has another opening tag after it and before the closing tag we've found
                             *       e.g.  <TA|GNAME><TAGNAME attrib="value">some text</TAGNAME></TAGNAME>
                             *             (cursor represented by |)
                             */
                            size_t currentStartPosition = xmlTags.tagOpenEnd;
                            size_t closeTagsRemaining = 1;
                            FindResult nextCloseTag;
                            do
                            {
                                nextCloseTag = FindCloseTag(tagName, currentStartPosition, docLength);
                                if (nextCloseTag.success)
                                {
                                    --closeTagsRemaining;
                                    // Open tag found
                                    // Now we need to check how many close tags there are between the open tag we just found,
                                    // and our close tag
                                    // eg. (Cursor == | )
                                    // <TAGNAM|E attrib="value"><TAGNAME>something</TAGNAME></TAGNAME></TAGNAME>
                                    //                                            ^^^^^^^^ we've found this guy
                                    //                         ^^^^^^^^^ Now we need to find this fella
                                    FindResult inbetweenOpenTag;
                                    size_t currentEndPosition = nextCloseTag.start;
                                    size_t openTagsFound = 0;

                                    do
                                    {
                                        inbetweenOpenTag = FindOpenTag(tagName, currentStartPosition, currentEndPosition);

                                        if (inbetweenOpenTag.success)
                                        {
                                            ++openTagsFound;
                                            currentStartPosition = inbetweenOpenTag.end;
                                        }

                                    } while(inbetweenOpenTag.success);

                                    // If we didn't find any open tags between our open and the close,
                                    // and there's no close tags remaining to find
                                    // then the close we found was the right one, and we can return it
                                    if (0 == openTagsFound && 0 == closeTagsRemaining)
                                    {
                                        xmlTags.tagCloseStart = nextCloseTag.start;
                                        xmlTags.tagCloseEnd = nextCloseTag.end + 1;
                                        tagFound = true;
                                    }
                                    else
                                    {

                                        // Need to find the same number of closing tags, without opening tags etc.
                                        closeTagsRemaining += openTagsFound;
                                        currentStartPosition = nextCloseTag.end;
                                    }
                                }
                            } while (!tagFound && closeTagsRemaining > 0 && nextCloseTag.success);
                        } // end if (selfclosingtag)... else {
                    } // end if (-1 != closeAngle)  {

                } // end if tagName.size() != 0
            } // end open tag test
        }
    }
    return tagFound;
}

FindResult CScintillaWnd::FindText( const char *text, size_t start, size_t end, int flags )
{
    FindResult returnValue;

    Scintilla::Sci_TextToFind search;
    search.lpstrText = const_cast<char *>(text);
    search.chrg.cpMin = (long)start;
    search.chrg.cpMax = (long)end;
    size_t result = Call(SCI_FINDTEXT, flags, reinterpret_cast<LPARAM>(&search));
    if (-1 == result)
    {
        returnValue.success = false;
    }
    else
    {
        returnValue.success = true;
        returnValue.start = search.chrgText.cpMin;
        returnValue.end = search.chrgText.cpMax;
    }
    return returnValue;
}

FindResult CScintillaWnd::FindOpenTag(const std::string& tagName, size_t start, size_t end)
{
    std::string search("<");
    search.append(tagName);
    FindResult openTagFound;
    openTagFound.success = false;
    FindResult result;
    int nextChar = 0;
    size_t styleAt;
    size_t searchStart = start;
    size_t searchEnd = end;
    bool forwardSearch = (start < end);
    do
    {

        result = FindText(search.c_str(), searchStart, searchEnd, 0);
        if (result.success)
        {
            nextChar = (int)Call(SCI_GETCHARAT, result.end);
            styleAt = Call(SCI_GETSTYLEAT, result.start);
            if (styleAt != SCE_H_CDATA && styleAt != SCE_H_DOUBLESTRING && styleAt != SCE_H_SINGLESTRING)
            {
                // We've got an open tag for this tag name (i.e. nextChar was space or '>')
                // Now we need to find the end of the start tag.

                // Common case, the tag is an empty tag with no whitespace. e.g. <TAGNAME>
                if (nextChar == '>')
                {
                    openTagFound.end = result.end;
                    openTagFound.success = true;
                }
                else if (IsXMLWhitespace(nextChar))
                {
                    size_t closeAnglePosition = FindCloseAngle(result.end, forwardSearch ? end : start);
                    if (-1 != closeAnglePosition && '/' != Call(SCI_GETCHARAT, closeAnglePosition - 1))
                    {
                        openTagFound.end = closeAnglePosition;
                        openTagFound.success = true;
                    }
                }
            }

        }

        if (forwardSearch)
        {
            searchStart = result.end + 1;
        }
        else
        {
            searchStart = result.start - 1;
        }

        // Loop while we've found a <TAGNAME, but it's either in a CDATA section,
        // or it's got more none whitespace characters after it. e.g. <TAGNAME2
    } while (result.success && !openTagFound.success);

    openTagFound.start = result.start;


    return openTagFound;

}


size_t CScintillaWnd::FindCloseAngle(size_t startPosition, size_t endPosition)
{
    // We'll search for the next '>', and check it's not in an attribute using the style
    FindResult closeAngle;

    bool isValidClose;
    size_t returnPosition = (size_t)-1;

    // Only search forwards
    if (startPosition > endPosition)
    {
        size_t temp = endPosition;
        endPosition = startPosition;
        startPosition = temp;
    }

    do
    {
        isValidClose = false;

        closeAngle = FindText(">", startPosition, endPosition, 0);
        if (closeAngle.success)
        {
            int style = (int)Call(SCI_GETSTYLEAT, closeAngle.start);
            // As long as we're not in an attribute (  <TAGNAME attrib="val>ue"> is VALID XML. )
            if (style != SCE_H_DOUBLESTRING && style != SCE_H_SINGLESTRING)
            {
                returnPosition = closeAngle.start;
                isValidClose = true;
            }
            else
            {
                startPosition = closeAngle.end;
            }
        }

    } while (closeAngle.success && isValidClose == false);

    return returnPosition;
}


FindResult CScintillaWnd::FindCloseTag(const std::string& tagName, size_t start, size_t end)
{
    std::string search("</");
    search.append(tagName);
    FindResult closeTagFound;
    closeTagFound.success = false;
    FindResult result;
    int nextChar;
    size_t styleAt;
    size_t searchStart = start;
    size_t searchEnd = end;
    bool forwardSearch = (start < end);
    bool validCloseTag;
    do
    {
        validCloseTag = false;
        result = FindText(search.c_str(), searchStart, searchEnd, 0);
        if (result.success)
        {
            nextChar = (int)Call(SCI_GETCHARAT, result.end);
            styleAt = Call(SCI_GETSTYLEAT, result.start);

            // Setup the parameters for the next search, if there is one.
            if (forwardSearch)
            {
                searchStart = result.end + 1;
            }
            else
            {
                searchStart = result.start - 1;
            }

            if (styleAt != SCE_H_CDATA && styleAt != SCE_H_SINGLESTRING && styleAt != SCE_H_DOUBLESTRING) // If what we found was in CDATA section, it's not a valid tag.
            {
                // Common case - '>' follows the tag name directly
                if (nextChar == '>')
                {
                    validCloseTag = true;
                    closeTagFound.start = result.start;
                    closeTagFound.end = result.end;
                    closeTagFound.success = true;
                }
                else if (IsXMLWhitespace(nextChar))  // Otherwise, if it's whitespace, then allow whitespace until a '>' - any other character is invalid.
                {
                    size_t whitespacePoint = result.end;
                    do
                    {
                        ++whitespacePoint;
                        nextChar = (int)Call(SCI_GETCHARAT, whitespacePoint);

                    } while(IsXMLWhitespace(nextChar));

                    if (nextChar == '>')
                    {
                        validCloseTag = true;
                        closeTagFound.start = result.start;
                        closeTagFound.end = whitespacePoint;
                        closeTagFound.success = true;
                    }
                }
            }
        }

    } while (result.success && !validCloseTag);

    return closeTagFound;

}

std::vector<std::pair<size_t, size_t>> CScintillaWnd::GetAttributesPos(size_t start, size_t end)
{
    std::vector<std::pair<size_t, size_t>> attributes;

    size_t bufLen = end - start + 1;
    std::unique_ptr<char[]> buf(new char[bufLen+1]);
    Scintilla::TextRange tr;
    tr.chrg.cpMin = (long)start;
    tr.chrg.cpMax = (long)end;
    tr.lpstrText = buf.get();
    Call(SCI_GETTEXTRANGE, 0, reinterpret_cast<LPARAM>(&tr));

    enum
    {
        attr_invalid,
        attr_key,
        attr_pre_assign,
        attr_assign,
        attr_string,
        attr_value,
        attr_valid
    } state = attr_invalid;

    size_t startPos = (size_t)-1;
    int oneMoreChar = 1;
    size_t i = 0;
    for (; i < bufLen ; i++)
    {
        switch (buf[i])
        {
        case ' ':
        case '\t':
        case '\n':
        case '\r':
            {
                if (state == attr_key)
                    state = attr_pre_assign;
                else if (state == attr_value)
                {
                    state = attr_valid;
                    oneMoreChar = 0;
                }
            }
            break;

        case '=':
            {
                if (state == attr_key || state == attr_pre_assign)
                    state = attr_assign;
                else if (state == attr_assign || state == attr_value)
                    state = attr_invalid;
            }
            break;

        case '"':
            {
                if (state == attr_string)
                {
                    state = attr_valid;
                    oneMoreChar = 1;
                }
                else if (state == attr_key || state == attr_pre_assign || state == attr_value)
                    state = attr_invalid;
                else if (state == attr_assign)
                    state = attr_string;
            }
            break;

        default:
            {
                if (state == attr_invalid)
                {
                    state = attr_key;
                    startPos = i;
                }
                else if (state == attr_pre_assign)
                    state = attr_invalid;
                else if (state == attr_assign)
                    state = attr_value;
            }
        }

        if (state == attr_valid)
        {
            attributes.push_back(std::pair<size_t, size_t>(start+startPos, start+i+oneMoreChar));
            state = attr_invalid;
        }
    }
    if (state == attr_value)
        attributes.push_back(std::pair<size_t, size_t>(start+startPos, start+i-1));

    return attributes;
}
