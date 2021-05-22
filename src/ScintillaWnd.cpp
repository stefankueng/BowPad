// This file is part of BowPad.
//
// Copyright (C) 2013-2021 - Stefan Kueng
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
#include "BowPadUI.h"
#include "UnicodeUtils.h"
#include "StringUtils.h"
#include "Theme.h"
#include "DarkModeHelper.h"
#include "SciLexer.h"
#include "Document.h"
#include "LexStyles.h"
#include "DocScroll.h"
#include "CommandHandler.h"
#include "DPIAware.h"
#include "../ext/scintilla/include/ILexer.h"
#include "../ext/lexilla/lexlib/LexerModule.h"
#include "Lexilla.h"

#include <UIRibbon.h>
#include <UIRibbonPropertyHelpers.h>
#include <uxtheme.h>
#include <chrono>

extern IUIFramework*          g_pFramework;
extern std::string            g_sHighlightString;  // from CmdFindReplace
extern sptr_t                 g_searchMarkerCount; // from CmdFindReplace
extern Scintilla::LexerModule lmSimple;
extern Scintilla::LexerModule lmLog;
extern Scintilla::LexerModule lmSnippets;

UINT32 g_contextID = cmdContextMap;

const int TIM_HIDECURSOR              = 101;
const int TIM_BRACEHIGHLIGHTTEXT      = 102;
const int TIM_BRACEHIGHLIGHTTEXTCLEAR = 103;

static bool g_scintillaInitialized = false;

const int    color_folding_fore_inactive    = 255;
const int    color_folding_back_inactive    = 220;
const int    color_folding_backsel_inactive = 150;
const int    color_folding_fore_active      = 250;
const int    color_folding_back_active      = 100;
const int    color_folding_backsel_active   = 20;
const int    color_linenr_inactive          = 109;
const int    color_linenr_active            = 60;
const double folding_color_animation_time   = 0.3;

CScintillaWnd::CScintillaWnd(HINSTANCE hInst)
    : CWindow(hInst)
    , m_pSciMsg(nullptr)
    , m_pSciWndData(0)
    , m_scrollTool(hInst)
    , m_selTextMarkerCount(0)
    , m_bCursorShown(true)
    , m_bScratch(false)
    , m_eraseBkgnd(true)
    , m_cursorTimeout(-1)
    , m_bInFolderMargin(false)
    , m_hasConsolas(false)
    , m_lineToScrollToAfterPaint(-1)
    , m_wrapOffsetToScrollToAfterPaint(0)
    , m_lineToScrollToAfterPaintCounter(0)
    , m_lastMousePos(-1)
{
    HFONT hFont = CreateFont(0, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                             OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, FIXED_PITCH, L"Consolas");
    if (hFont)
    {
        DeleteObject(hFont);
        m_hasConsolas = true;
    }

    m_animVarGrayFore   = Animator::Instance().CreateAnimationVariable(color_folding_fore_inactive, color_folding_fore_active);
    m_animVarGrayBack   = Animator::Instance().CreateAnimationVariable(color_folding_back_inactive, color_folding_back_active);
    m_animVarGraySel    = Animator::Instance().CreateAnimationVariable(color_folding_backsel_inactive, color_folding_backsel_active);
    m_animVarGrayLineNr = Animator::Instance().CreateAnimationVariable(color_linenr_inactive, color_linenr_active);
}

CScintillaWnd::~CScintillaWnd()
{
    if (m_bScratch)
    {
        DestroyWindow(*this);
        m_hwnd = nullptr;
    }
}

bool CScintillaWnd::Init(HINSTANCE hInst, HWND hParent, HWND hWndAttachTo)
{
    if (!g_scintillaInitialized)
    {
        Scintilla_RegisterClasses(hInst);
        INITCOMMONCONTROLSEX icce{};
        icce.dwSize = sizeof(icce);
        icce.dwICC  = ICC_BAR_CLASSES;
        InitCommonControlsEx(&icce);
        CreateLexer(""); // initializes all lexers
    }

    if (hWndAttachTo == nullptr)
    {
        if (hParent)
            CreateEx(0, WS_CHILD | WS_VISIBLE, hParent, nullptr, L"Scintilla");
        else
            CreateEx(0, 0, nullptr, nullptr, L"Scintilla");
    }
    else
        m_hwnd = hWndAttachTo;

    if (!*this)
    {
        return false;
    }

    m_pSciMsg     = reinterpret_cast<SciFnDirect>(SendMessage(*this, SCI_GETDIRECTFUNCTION, 0, 0));
    m_pSciWndData = static_cast<sptr_t>(SendMessage(*this, SCI_GETDIRECTPOINTER, 0, 0));

    if (m_pSciMsg == nullptr || m_pSciWndData == 0)
        return false;

    m_docScroll.InitScintilla(this);

    Call(SCI_SETMODEVENTMASK, SC_MOD_INSERTTEXT | SC_MOD_DELETETEXT | SC_PERFORMED_UNDO |
                                  SC_PERFORMED_REDO | SC_MULTISTEPUNDOREDO | SC_LASTSTEPINUNDOREDO |
                                  SC_MOD_BEFOREINSERT | SC_MOD_BEFOREDELETE | SC_MULTILINEUNDOREDO | SC_MOD_CHANGEFOLD);
    bool bUseD2D = CIniSettings::Instance().GetInt64(L"View", L"d2d", 1) != 0;
    Call(SCI_SETTECHNOLOGY, bUseD2D ? SC_TECHNOLOGY_DIRECTWRITERETAIN : SC_TECHNOLOGY_DEFAULT);

    Call(SCI_SETMARGINMASKN, SC_MARGE_FOLDER, SC_MASK_FOLDERS);
    Call(SCI_SETMARGINWIDTHN, SC_MARGE_FOLDER, CDPIAware::Instance().Scale(*this, 14));
    Call(SCI_SETMARGINCURSORN, SC_MARGE_FOLDER, SC_CURSORARROW);
    Call(SCI_SETAUTOMATICFOLD, SC_AUTOMATICFOLD_SHOW | SC_AUTOMATICFOLD_CHANGE);

    Call(SCI_SETMARGINMASKN, SC_MARGE_SYMBOL, (1 << MARK_BOOKMARK));
    Call(SCI_SETMARGINWIDTHN, SC_MARGE_SYMBOL, CDPIAware::Instance().Scale(*this, 14));
    Call(SCI_SETMARGINCURSORN, SC_MARGE_SYMBOL, SC_CURSORARROW);
    Call(SCI_MARKERSETALPHA, MARK_BOOKMARK, 70);
    Call(SCI_MARKERDEFINE, MARK_BOOKMARK, SC_MARK_VERTICALBOOKMARK);

    Call(SCI_SETMARGINSENSITIVEN, SC_MARGE_FOLDER, true);
    Call(SCI_SETMARGINSENSITIVEN, SC_MARGE_SYMBOL, true);

    Call(SCI_SETSCROLLWIDTHTRACKING, true);
    Call(SCI_SETSCROLLWIDTH, 1); // default empty document: override default width

    Call(SCI_MARKERDEFINE, SC_MARKNUM_FOLDEROPEN, SC_MARK_BOXMINUS);
    Call(SCI_MARKERDEFINE, SC_MARKNUM_FOLDER, SC_MARK_BOXPLUS);
    Call(SCI_MARKERDEFINE, SC_MARKNUM_FOLDERSUB, SC_MARK_VLINE);
    Call(SCI_MARKERDEFINE, SC_MARKNUM_FOLDERTAIL, SC_MARK_LCORNERCURVE);
    Call(SCI_MARKERDEFINE, SC_MARKNUM_FOLDEREND, SC_MARK_BOXPLUSCONNECTED);
    Call(SCI_MARKERDEFINE, SC_MARKNUM_FOLDEROPENMID, SC_MARK_BOXMINUSCONNECTED);
    Call(SCI_MARKERDEFINE, SC_MARKNUM_FOLDERMIDTAIL, SC_MARK_TCORNERCURVE);
    Call(SCI_MARKERENABLEHIGHLIGHT, 1);
    Call(SCI_SETFOLDFLAGS, SC_FOLDFLAG_LINEAFTER_CONTRACTED);
    Call(SCI_FOLDDISPLAYTEXTSETSTYLE, SC_FOLDDISPLAYTEXT_STANDARD);

    Call(SCI_SETWRAPVISUALFLAGS, SC_WRAPVISUALFLAG_NONE);
    Call(SCI_SETWRAPMODE, SC_WRAP_NONE);

    Call(SCI_SETFONTQUALITY, SC_EFF_QUALITY_LCD_OPTIMIZED);

    Call(SCI_STYLESETVISIBLE, STYLE_CONTROLCHAR, TRUE);

    Call(SCI_SETCARETSTYLE, 1);
    Call(SCI_SETCARETLINEVISIBLE, true);
    Call(SCI_SETCARETLINEVISIBLEALWAYS, true);
    Call(SCI_SETCARETWIDTH, bUseD2D ? 2 : 1);
    if (CIniSettings::Instance().GetInt64(L"View", L"caretlineframe", 1) != 0)
        Call(SCI_SETCARETLINEFRAME, CDPIAware::Instance().Scale(*this, 1));
    Call(SCI_SETWHITESPACESIZE, CDPIAware::Instance().Scale(*this, 1));
    Call(SCI_SETMULTIPLESELECTION, 1);
    Call(SCI_SETMOUSESELECTIONRECTANGULARSWITCH, true);
    Call(SCI_SETADDITIONALSELECTIONTYPING, true);
    Call(SCI_SETADDITIONALCARETSBLINK, true);
    Call(SCI_SETMULTIPASTE, SC_MULTIPASTE_EACH);
    Call(SCI_SETVIRTUALSPACEOPTIONS, SCVS_RECTANGULARSELECTION);

    Call(SCI_SETMOUSEWHEELCAPTURES, 0);

    // For Ctrl+C, use SCI_COPYALLOWLINE instead of SCI_COPY
    Call(SCI_ASSIGNCMDKEY, 'C' + (SCMOD_CTRL << 16), SCI_COPYALLOWLINE);

    // change the home and end key to honor wrapped lines
    // but allow the line-home and line-end to be used with the ALT key
    Call(SCI_ASSIGNCMDKEY, SCK_HOME, SCI_VCHOMEWRAP);
    Call(SCI_ASSIGNCMDKEY, SCK_END, SCI_LINEENDWRAP);
    Call(SCI_ASSIGNCMDKEY, SCK_HOME + (SCMOD_SHIFT << 16), SCI_VCHOMEWRAPEXTEND);
    Call(SCI_ASSIGNCMDKEY, SCK_END + (SCMOD_SHIFT << 16), SCI_LINEENDWRAPEXTEND);
    Call(SCI_ASSIGNCMDKEY, SCK_HOME + (SCMOD_ALT << 16), SCI_VCHOME);
    Call(SCI_ASSIGNCMDKEY, SCK_END + (SCMOD_ALT << 16), SCI_LINEEND);

    // line cut for Ctrl+L
    Call(SCI_ASSIGNCMDKEY, 'L' + (SCMOD_CTRL << 16), SCI_LINECUT);

    Call(SCI_SETBUFFEREDDRAW, bUseD2D ? false : true);
    Call(SCI_SETPHASESDRAW, bUseD2D ? SC_PHASES_MULTIPLE : SC_PHASES_TWO);
    Call(SCI_SETLAYOUTCACHE, SC_CACHE_PAGE);

    Call(SCI_USEPOPUP, 0); // no default context menu

    Call(SCI_SETMOUSEDWELLTIME, GetDoubleClickTime());
    Call(SCI_CALLTIPSETPOSITION, 1);

    Call(SCI_SETPASTECONVERTENDINGS, 1);

    Call(SCI_SETSELALPHA, 128);

    Call(SCI_SETCHARACTERCATEGORYOPTIMIZATION, 0x10000);
    Call(SCI_SETACCESSIBILITY, SC_ACCESSIBILITY_ENABLED);

    SetTabSettings(TabSpace::Default);

    SetupDefaultStyles();

    return true;
}

bool CScintillaWnd::InitScratch(HINSTANCE hInst)
{
    Scintilla_RegisterClasses(hInst);

    CreateEx(WS_EX_NOPARENTNOTIFY, 0, nullptr, nullptr, L"Scintilla");

    if (!*this)
        return false;

    m_pSciMsg     = reinterpret_cast<SciFnDirect>(SendMessage(*this, SCI_GETDIRECTFUNCTION, 0, 0));
    m_pSciWndData = static_cast<sptr_t>(SendMessage(*this, SCI_GETDIRECTPOINTER, 0, 0));

    if (m_pSciMsg == nullptr || m_pSciWndData == 0)
        return false;

    m_bScratch = true;

    return true;
}

bool bEatNextTabOrEnterKey = false;

LRESULT CALLBACK CScintillaWnd::WinMsgHandler(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    NMHDR* hdr = reinterpret_cast<NMHDR*>(lParam);
    switch (uMsg)
    {
        case WM_ERASEBKGND:
        {
            // only erase the background during startup:
            // it helps reduce the white background shown in dark mode
            // but once the window is drawn, erasing the background every time
            // causes an annoying flicker when resizing the window.
            if (m_eraseBkgnd)
            {
                HDC  hdc    = reinterpret_cast<HDC>(wParam);
                auto bgc    = CTheme::Instance().GetThemeColor(::GetSysColor(COLOR_3DFACE));
                auto oldBgc = SetBkColor(hdc, bgc);
                RECT r{};
                GetClientRect(hwnd, &r);
                ExtTextOut(hdc, r.left, r.top, ETO_CLIPPED | ETO_OPAQUE, &r, L"", 0, nullptr);
                SetBkColor(hdc, oldBgc);
            }
            return TRUE;
        }
        case WM_NOTIFY:
            if (hdr->code == NM_COOLSB_CUSTOMDRAW)
                return m_docScroll.HandleCustomDraw(wParam, reinterpret_cast<NMCSBCUSTOMDRAW*>(lParam));
            break;
        case WM_CONTEXTMENU:
        {
            POINT pt{};
            POINTSTOPOINT(pt, lParam);

            if (pt.x == -1 && pt.y == -1)
            {
                // Display the menu in the upper-left corner of the client area, below the ribbon.
                IUIRibbonPtr pRibbon;
                HRESULT      hr = g_pFramework->GetView(0, IID_PPV_ARGS(&pRibbon));
                if (SUCCEEDED(hr))
                {
                    UINT32 uRibbonHeight = 0;
                    hr                   = pRibbon->GetHeight(&uRibbonHeight);
                    if (SUCCEEDED(hr))
                    {
                        pt.x = 0;
                        pt.y = uRibbonHeight;
                        ClientToScreen(hwnd, &pt);
                    }
                }
                if (FAILED(hr))
                {
                    // Default to just the upper-right corner of the entire screen.
                    pt.x = 0;
                    pt.y = 0;
                }
            }

            // The basic pattern of how to show Contextual UI in a specified location.
            IUIContextualUIPtr pContextualUI = nullptr;
            if (SUCCEEDED(g_pFramework->GetView(g_contextID, IID_PPV_ARGS(&pContextualUI))))
            {
                pContextualUI->ShowAtLocation(pt.x, pt.y);
            }
        }
        break;
        case WM_CHAR:
        {
            if ((wParam == VK_RETURN) || (wParam == '\n'))
            {
                if (bEatNextTabOrEnterKey)
                {
                    bEatNextTabOrEnterKey = false;
                    return 0;
                }
                if ((GetKeyState(VK_CONTROL) & 0x8000) || (GetKeyState(VK_SHIFT) & 0x8000))
                {
                    return 0;
                }
            }
            if (wParam == VK_TAB && bEatNextTabOrEnterKey)
            {
                bEatNextTabOrEnterKey = false;
                return 0;
            }
            if (AutoBraces(wParam))
                return 0;
        }
        break;
        case WM_KEYDOWN:
        {
            auto ret = SendMessage(GetParent(*this), WM_SCICHAR, wParam, lParam);
            if (ret == 0)
            {
                bEatNextTabOrEnterKey = true;
                return 0;
            }

            if ((wParam == VK_RETURN) || (wParam == '\n'))
            {
                if ((GetKeyState(VK_CONTROL) & 0x8000) || (GetKeyState(VK_SHIFT) & 0x8000))
                {
                    Call(SCI_BEGINUNDOACTION);
                    if (GetKeyState(VK_CONTROL) & 0x8000)
                    {
                        // Ctrl+Return: insert a line above the current line
                        // Cursor-Up, End, Return
                        Call(SCI_LINEUP);
                    }
                    Call(SCI_LINEEND);
                    Call(SCI_NEWLINE);
                    Call(SCI_ENDUNDOACTION);
                    return 0;
                }
                char c  = static_cast<char>(Call(SCI_GETCHARAT, Call(SCI_GETCURRENTPOS) - 1));
                char c1 = static_cast<char>(Call(SCI_GETCHARAT, Call(SCI_GETCURRENTPOS)));
                if ((c == '{') && (c1 == '}'))
                {
                    Call(SCI_NEWLINE);
                    Call(SCI_BEGINUNDOACTION);
                    Call(SCI_LINEUP);
                    Call(SCI_LINEEND);
                    Call(SCI_NEWLINE);
                    Call(SCI_TAB);
                    Call(SCI_ENDUNDOACTION);
                    bEatNextTabOrEnterKey = true;
                    return 0;
                }
            }
        }
        break;
        case WM_MOUSEMOVE:
        case WM_LBUTTONDBLCLK:
        case WM_LBUTTONDOWN:
        case WM_RBUTTONDBLCLK:
        case WM_RBUTTONDOWN:
        case WM_VSCROLL:
        case WM_HSCROLL:
        case WM_MOUSEWHEEL:
        case WM_MOUSEHWHEEL:
        {
            // mouse cursor moved, ensure it's visible
            // but set a timer to hide it after a while
            if (m_cursorTimeout == -1)
                m_cursorTimeout = static_cast<int>(CIniSettings::Instance().GetInt64(L"View", L"hidecursortimeout", 3000));
            UINT elapse = static_cast<UINT>(m_cursorTimeout);
            if (elapse != 0)
                SetTimer(*this, TIM_HIDECURSOR, elapse, nullptr);
            if (!m_bCursorShown)
            {
                Call(SCI_SETCURSOR, static_cast<uptr_t>(-1));
                m_bCursorShown = true;
            }
            if (uMsg == WM_VSCROLL)
            {
                switch (LOWORD(wParam))
                {
                    case SB_THUMBPOSITION:
                    case SB_ENDSCROLL:
                        m_scrollTool.Clear();
                        break;
                    case SB_THUMBTRACK:
                    {
                        RECT thumbRect;
                        GetClientRect(*this, &thumbRect);
                        MapWindowPoints(*this, nullptr, reinterpret_cast<LPPOINT>(&thumbRect), 2);

                        SCROLLINFO si = {0};
                        si.cbSize     = sizeof(SCROLLINFO);
                        si.fMask      = SIF_ALL;
                        CoolSB_GetScrollInfo(*this, SB_VERT, &si);

                        auto  lineCount = Call(SCI_GETLINECOUNT);
                        auto  w         = m_scrollTool.GetTextWidth(CStringUtils::Format(L"Line: %lld", lineCount).c_str());
                        POINT thumbPoint{};
                        thumbPoint.x = thumbRect.right - w;
                        thumbPoint.y = thumbRect.top + ((thumbRect.bottom - thumbRect.top) * si.nTrackPos) / (si.nMax - si.nMin);
                        auto docLine = Call(SCI_DOCLINEFROMVISIBLE, si.nTrackPos);
                        m_scrollTool.Init();
                        DarkModeHelper::Instance().AllowDarkModeForWindow(m_scrollTool, CTheme::Instance().IsDarkTheme());
                        SetWindowTheme(m_scrollTool, L"Explorer", nullptr);

                        m_scrollTool.SetText(&thumbPoint, L"Line: %lld", docLine + 1);
                    }
                    break;
                }
            }
            if (uMsg == WM_MOUSEMOVE)
            {
                RECT rc;
                GetClientRect(*this, &rc);
                rc.right = rc.left;
                for (int i = 0; i <= SC_MARGE_FOLDER; ++i)
                    rc.right += static_cast<long>(Call(SCI_GETMARGINWIDTHN, i));
                POINT pt       = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
                auto  mousePos = Call(SCI_POSITIONFROMPOINT, pt.x, pt.y);
                if (mousePos != m_lastMousePos)
                {
                    SCNotification scn = {};
                    scn.nmhdr.code     = SCN_DWELLEND;
                    scn.position       = mousePos;
                    scn.x              = static_cast<int>(pt.x);
                    scn.y              = static_cast<int>(pt.y);
                    scn.nmhdr.hwndFrom = *this;
                    scn.nmhdr.idFrom   = ::GetDlgCtrlID(*this);
                    ::SendMessage(::GetParent(*this), WM_NOTIFY,
                                  ::GetDlgCtrlID(*this), reinterpret_cast<LPARAM>(&scn));
                }
                m_lastMousePos = mousePos;
                if (PtInRect(&rc, pt))
                {
                    if (!m_bInFolderMargin)
                    {
                        // animate the colors of the folder margin lines and symbols
                        auto transFore = Animator::Instance().CreateSmoothStopTransition(m_animVarGrayFore, folding_color_animation_time, color_folding_fore_active);
                        auto transBack = Animator::Instance().CreateSmoothStopTransition(m_animVarGrayBack, folding_color_animation_time, color_folding_back_active);
                        auto transSel  = Animator::Instance().CreateSmoothStopTransition(m_animVarGraySel, folding_color_animation_time, color_folding_backsel_active);
                        auto transNr   = Animator::Instance().CreateSmoothStopTransition(m_animVarGrayLineNr, folding_color_animation_time, color_linenr_active);

                        if (transFore && transBack && transSel && transNr)
                        {
                            auto storyBoard = Animator::Instance().CreateStoryBoard();
                            storyBoard->AddTransition(m_animVarGrayFore.m_animVar, transFore);
                            storyBoard->AddTransition(m_animVarGrayBack.m_animVar, transBack);
                            storyBoard->AddTransition(m_animVarGraySel.m_animVar, transSel);
                            storyBoard->AddTransition(m_animVarGrayLineNr.m_animVar, transNr);
                            Animator::Instance().RunStoryBoard(storyBoard, [this]() {
                                auto gf = Animator::GetIntegerValue(m_animVarGrayFore);
                                auto gb = Animator::GetIntegerValue(m_animVarGrayBack);
                                auto sb = Animator::GetIntegerValue(m_animVarGraySel);
                                auto ln = Animator::GetIntegerValue(m_animVarGrayLineNr);
                                SetupFoldingColors(RGB(gf, gf, gf),
                                                   RGB(gb, gb, gb),
                                                   RGB(sb, sb, sb));
                                Call(SCI_STYLESETFORE, STYLE_LINENUMBER, CTheme::Instance().GetThemeColor(RGB(ln, ln, ln), true));
                            });
                        }
                        else
                        {
                            auto gf = Animator::GetIntegerValue(m_animVarGrayFore);
                            auto gb = Animator::GetIntegerValue(m_animVarGrayBack);
                            auto sb = Animator::GetIntegerValue(m_animVarGraySel);
                            auto ln = Animator::GetIntegerValue(m_animVarGrayLineNr);
                            SetupFoldingColors(RGB(gf, gf, gf),
                                               RGB(gb, gb, gb),
                                               RGB(sb, sb, sb));
                            Call(SCI_STYLESETFORE, STYLE_LINENUMBER, CTheme::Instance().GetThemeColor(RGB(ln, ln, ln), true));
                        }

                        m_bInFolderMargin   = true;
                        TRACKMOUSEEVENT tme = {0};
                        tme.cbSize          = sizeof(TRACKMOUSEEVENT);
                        tme.dwFlags         = TME_LEAVE;
                        tme.hwndTrack       = *this;
                        TrackMouseEvent(&tme);
                    }
                }
                else
                {
                    if (m_bInFolderMargin)
                    {
                        // animate the colors of the folder margin lines and symbols
                        auto transFore = Animator::Instance().CreateSmoothStopTransition(m_animVarGrayFore, folding_color_animation_time, color_folding_fore_inactive);
                        auto transBack = Animator::Instance().CreateSmoothStopTransition(m_animVarGrayBack, folding_color_animation_time, color_folding_back_inactive);
                        auto transSel  = Animator::Instance().CreateSmoothStopTransition(m_animVarGraySel, folding_color_animation_time, color_folding_backsel_inactive);
                        auto transNr   = Animator::Instance().CreateSmoothStopTransition(m_animVarGrayLineNr, folding_color_animation_time, color_linenr_inactive);

                        if (transFore && transBack && transSel && transNr)
                        {
                            auto storyBoard = Animator::Instance().CreateStoryBoard();
                            storyBoard->AddTransition(m_animVarGrayFore.m_animVar, transFore);
                            storyBoard->AddTransition(m_animVarGrayBack.m_animVar, transBack);
                            storyBoard->AddTransition(m_animVarGraySel.m_animVar, transSel);
                            storyBoard->AddTransition(m_animVarGrayLineNr.m_animVar, transNr);
                            Animator::Instance().RunStoryBoard(storyBoard, [this]() {
                                auto gf = Animator::GetIntegerValue(m_animVarGrayFore);
                                auto gb = Animator::GetIntegerValue(m_animVarGrayBack);
                                auto sb = Animator::GetIntegerValue(m_animVarGraySel);
                                auto ln = Animator::GetIntegerValue(m_animVarGrayLineNr);
                                SetupFoldingColors(RGB(gf, gf, gf),
                                                   RGB(gb, gb, gb),
                                                   RGB(sb, sb, sb));
                                Call(SCI_STYLESETFORE, STYLE_LINENUMBER, CTheme::Instance().GetThemeColor(RGB(ln, ln, ln), true));
                            });
                        }
                        else
                        {
                            auto gf = Animator::GetIntegerValue(m_animVarGrayFore);
                            auto gb = Animator::GetIntegerValue(m_animVarGrayBack);
                            auto sb = Animator::GetIntegerValue(m_animVarGraySel);
                            auto ln = Animator::GetIntegerValue(m_animVarGrayLineNr);
                            SetupFoldingColors(RGB(gf, gf, gf),
                                               RGB(gb, gb, gb),
                                               RGB(sb, sb, sb));
                            Call(SCI_STYLESETFORE, STYLE_LINENUMBER, CTheme::Instance().GetThemeColor(RGB(ln, ln, ln), true));
                        }
                        m_bInFolderMargin = false;
                    }
                }
            }
        }
        break;
        case WM_MOUSELEAVE:
        {
            TRACKMOUSEEVENT tme = {0};
            tme.cbSize          = sizeof(TRACKMOUSEEVENT);
            tme.dwFlags         = TME_LEAVE | TME_CANCEL;
            tme.hwndTrack       = *this;
            TrackMouseEvent(&tme);
            if (m_bInFolderMargin)
            {
                // animate the colors of the folder margin lines and symbols
                auto transFore = Animator::Instance().CreateSmoothStopTransition(m_animVarGrayFore, folding_color_animation_time, color_folding_fore_inactive);
                auto transBack = Animator::Instance().CreateSmoothStopTransition(m_animVarGrayBack, folding_color_animation_time, color_folding_back_inactive);
                auto transSel  = Animator::Instance().CreateSmoothStopTransition(m_animVarGraySel, folding_color_animation_time, color_folding_backsel_inactive);
                auto transNr   = Animator::Instance().CreateSmoothStopTransition(m_animVarGrayLineNr, folding_color_animation_time, color_linenr_inactive);

                if (transFore && transBack && transSel && transNr)
                {
                    auto storyBoard = Animator::Instance().CreateStoryBoard();
                    storyBoard->AddTransition(m_animVarGrayFore.m_animVar, transFore);
                    storyBoard->AddTransition(m_animVarGrayBack.m_animVar, transBack);
                    storyBoard->AddTransition(m_animVarGraySel.m_animVar, transSel);
                    storyBoard->AddTransition(m_animVarGrayLineNr.m_animVar, transNr);
                    Animator::Instance().RunStoryBoard(storyBoard, [this]() {
                        auto gf = Animator::GetIntegerValue(m_animVarGrayFore);
                        auto gb = Animator::GetIntegerValue(m_animVarGrayBack);
                        auto sb = Animator::GetIntegerValue(m_animVarGraySel);
                        auto ln = Animator::GetIntegerValue(m_animVarGrayLineNr);
                        SetupFoldingColors(RGB(gf, gf, gf),
                                           RGB(gb, gb, gb),
                                           RGB(sb, sb, sb));
                        Call(SCI_STYLESETFORE, STYLE_LINENUMBER, CTheme::Instance().GetThemeColor(RGB(ln, ln, ln), true));
                    });
                }
                else
                {
                    auto gf = Animator::GetIntegerValue(m_animVarGrayFore);
                    auto gb = Animator::GetIntegerValue(m_animVarGrayBack);
                    auto sb = Animator::GetIntegerValue(m_animVarGraySel);
                    auto ln = Animator::GetIntegerValue(m_animVarGrayLineNr);
                    SetupFoldingColors(RGB(gf, gf, gf),
                                       RGB(gb, gb, gb),
                                       RGB(sb, sb, sb));
                    Call(SCI_STYLESETFORE, STYLE_LINENUMBER, CTheme::Instance().GetThemeColor(RGB(ln, ln, ln), true));
                }
                m_bInFolderMargin = false;
            }
        }
        break;
        case WM_TIMER:
            switch (wParam)
            {
                case TIM_HIDECURSOR:
                    // hide the mouse cursor so it does not get in the way
                    if (m_bCursorShown)
                    {
                        DWORD pos = GetMessagePos();
                        POINT pt{};
                        pt.x        = GET_X_LPARAM(pos);
                        pt.y        = GET_Y_LPARAM(pos);
                        auto hptWnd = WindowFromPoint(pt);
                        if (hptWnd == *this && hptWnd == GetFocus())
                        {
                            RECT rc;
                            GetClientRect(*this, &rc);
                            ScreenToClient(*this, &pt);
                            if (PtInRect(&rc, pt))
                            {
                                // We want to hide the cusor, let the parent override that choice.
                                // If they don't doesn't understand the message then the result
                                // should remain TRUE and we'll go ahead and hide.
                                BOOL hide = TRUE;
                                SendMessage(GetParent(*this), WM_CANHIDECURSOR, static_cast<WPARAM>(0), reinterpret_cast<LPARAM>(&hide));
                                if (hide)
                                {
                                    m_bCursorShown = false;
                                    SetCursor(nullptr);
                                    Call(SCI_SETCURSOR, static_cast<uptr_t>(-2));
                                }
                            }
                        }
                    }
                    break;
                case TIM_BRACEHIGHLIGHTTEXT:
                    MatchBraces(BraceMatch::Highlight);
                    break;
                case TIM_BRACEHIGHLIGHTTEXTCLEAR:
                    MatchBraces(BraceMatch::Clear);
                    break;
            }
            break;
        case WM_SETCURSOR:
        {
            if (!m_bCursorShown)
            {
                SetCursor(nullptr);
                Call(SCI_SETCURSOR, static_cast<uptr_t>(-2));
            }

            auto cur = Call(SCI_GETCURSOR);
            if ((cur == 8) || (cur == 0))
                Call(SCI_SETCURSOR, static_cast<uptr_t>(SC_CURSORNORMAL));
        }
        break;
        case WM_GESTURENOTIFY:
        {
            DWORD         panWant = GC_PAN | GC_PAN_WITH_INERTIA | GC_PAN_WITH_SINGLE_FINGER_VERTICALLY;
            GESTURECONFIG gestureConfig[] =
                {
                    {GID_PAN, panWant, GC_PAN_WITH_GUTTER},
                    {GID_TWOFINGERTAP, GC_TWOFINGERTAP, 0},
                };
            SetGestureConfig(*this, 0, _countof(gestureConfig), gestureConfig, sizeof(GESTURECONFIG));
            return 0;
        }
        case WM_GESTURE:
        {
            static int  scale    = 8; // altering the scale value will change how fast the page scrolls
            static int  lastY    = 0; // used for panning calculations (initial / previous vertical position)
            static int  lastX    = 0; // used for panning calculations (initial / previous horizontal position)
            static long xOverPan = 0;
            static long yOverPan = 0;

            GESTUREINFO gi;
            ZeroMemory(&gi, sizeof(GESTUREINFO));
            gi.cbSize    = sizeof(GESTUREINFO);
            BOOL bResult = GetGestureInfo(reinterpret_cast<HGESTUREINFO>(lParam), &gi);

            if (bResult)
            {
                // now interpret the gesture
                switch (gi.dwID)
                {
                    case GID_BEGIN:
                        lastY = 0;
                        lastX = 0;
                        break;
                    case GID_PAN:
                    {
                        if ((lastY == 0) && (lastX == 0))
                        {
                            lastY = gi.ptsLocation.y;
                            lastX = gi.ptsLocation.x;
                            break;
                        }
                        // Get all the vertical scroll bar information
                        int scrollX = (gi.ptsLocation.x - lastX) / scale;
                        int scrollY = (gi.ptsLocation.y - lastY) / scale;

                        SCROLLINFO siv = {0};
                        siv.cbSize     = sizeof(siv);
                        siv.fMask      = SIF_ALL;
                        CoolSB_GetScrollInfo(*this, SB_VERT, &siv);
                        if (scrollY)
                        {
                            siv.nPos -= scrollY;
                            siv.fMask = SIF_POS;
                            CoolSB_SetScrollInfo(hwnd, SB_VERT, &siv, TRUE);
                            SendMessage(hwnd, WM_VSCROLL, MAKEWPARAM(SB_THUMBPOSITION, siv.nPos), 0L);
                            lastY = gi.ptsLocation.y;
                        }

                        SCROLLINFO sih = {0};
                        sih.cbSize     = sizeof(sih);
                        sih.fMask      = SIF_ALL;
                        CoolSB_GetScrollInfo(*this, SB_HORZ, &sih);
                        if (scrollX)
                        {
                            sih.nPos -= scrollX;
                            sih.fMask = SIF_POS;
                            CoolSB_SetScrollInfo(hwnd, SB_HORZ, &sih, TRUE);
                            SendMessage(hwnd, WM_HSCROLL, MAKEWPARAM(SB_THUMBPOSITION, sih.nPos), 0L);
                            lastX = gi.ptsLocation.x;
                        }

                        yOverPan -= lastY - gi.ptsLocation.y;
                        xOverPan -= lastX - gi.ptsLocation.x;

                        if (gi.dwFlags & GF_BEGIN)
                        {
                            BeginPanningFeedback(hwnd);
                            yOverPan = 0;
                            xOverPan = 0;
                        }
                        else if (gi.dwFlags & GF_END)
                        {
                            EndPanningFeedback(hwnd, TRUE);
                            yOverPan = 0;
                            xOverPan = 0;
                        }

                        if ((siv.nPos == siv.nMin) || (siv.nPos >= (siv.nMax - static_cast<int>(siv.nPage))))
                        {
                            // we reached the bottom / top, pan
                            UpdatePanningFeedback(hwnd, 0, yOverPan, gi.dwFlags & GF_INERTIA);
                        }
                        if ((sih.nPos == sih.nMin) || (sih.nPos >= (sih.nMax - static_cast<int>(sih.nPage))))
                        {
                            // we reached the bottom / top, pan
                            UpdatePanningFeedback(hwnd, xOverPan, 0, gi.dwFlags & GF_INERTIA);
                        }
                        UpdateWindow(hwnd);
                        CloseGestureInfoHandle(reinterpret_cast<HGESTUREINFO>(lParam));
                        return 0;
                    }
                    case GID_TWOFINGERTAP:
                        Call(SCI_SETZOOM, 0);
                        //UpdateStatusBar(false);
                        break;
                    default:
                        // You have encountered an unknown gesture
                        break;
                }
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

void CScintillaWnd::UpdateLineNumberWidth() const
{
    bool bShowLineNumbers = CIniSettings::Instance().GetInt64(L"View", L"linenumbers", 1) != 0;
    if (!bShowLineNumbers)
    {
        Call(SCI_SETMARGINWIDTHN, SC_MARGE_LINENUMBER, 0);
        return;
    }
    auto linesVisible = Call(SCI_LINESONSCREEN);
    if (linesVisible)
    {
        auto   firstVisibleLineVis = Call(SCI_GETFIRSTVISIBLELINE);
        auto   lastVisibleLineVis  = linesVisible + firstVisibleLineVis + 1;
        sptr_t i                   = 0;
        while (lastVisibleLineVis)
        {
            lastVisibleLineVis /= 10;
            i++;
        }
        i = max(i, 3);
        {
            int pixelWidth = static_cast<int>(CDPIAware::Instance().Scale(*this, 8) + i * Call(SCI_TEXTWIDTH, STYLE_LINENUMBER, reinterpret_cast<LPARAM>("8")));
            Call(SCI_SETMARGINWIDTHN, SC_MARGE_LINENUMBER, pixelWidth);
        }
    }
}

void CScintillaWnd::SaveCurrentPos(CPosData& pos)
{
    // don't save the position if the window has never been
    // painted: if there's still a valid m_LineToScrollToAfterPaint
    // then the scroll position hasn't been set properly yet
    // and reading the positions would be wrong.
    if (m_lineToScrollToAfterPaint == -1)
    {
        auto firstVisibleLine   = Call(SCI_GETFIRSTVISIBLELINE);
        pos.m_nFirstVisibleLine = Call(SCI_DOCLINEFROMVISIBLE, firstVisibleLine);
        if (Call(SCI_GETWRAPMODE) != SC_WRAP_NONE)
            pos.m_nWrapLineOffset = firstVisibleLine - Call(SCI_VISIBLEFROMDOCLINE, pos.m_nFirstVisibleLine);

        pos.m_nStartPos    = Call(SCI_GETANCHOR);
        pos.m_nEndPos      = Call(SCI_GETCURRENTPOS);
        pos.m_xOffset      = Call(SCI_GETXOFFSET);
        pos.m_nSelMode     = Call(SCI_GETSELECTIONMODE);
        pos.m_nScrollWidth = Call(SCI_GETSCROLLWIDTH);

        pos.m_lineStateVector.clear();
        pos.m_lastStyleLine             = 0;
        sptr_t contractedFoldHeaderLine = 0;
        do
        {
            contractedFoldHeaderLine = static_cast<sptr_t>(Call(SCI_CONTRACTEDFOLDNEXT, contractedFoldHeaderLine));
            if (contractedFoldHeaderLine != -1)
            {
                // Store contracted line
                pos.m_lineStateVector.push_back(contractedFoldHeaderLine);
                pos.m_lastStyleLine = max(pos.m_lastStyleLine, (sptr_t)Call(SCI_GETLASTCHILD, contractedFoldHeaderLine, -1));
                // Start next search with next line
                ++contractedFoldHeaderLine;
            }
        } while (contractedFoldHeaderLine != -1);
    }
    else
    {
        m_lineToScrollToAfterPaint = -1;
    }
}

void CScintillaWnd::RestoreCurrentPos(const CPosData& pos)
{
    // if the document hasn't been styled yet, do it now
    // otherwise restoring the folds won't work.
    if (!pos.m_lineStateVector.empty())
    {
        auto endStyled = Call(SCI_GETENDSTYLED);
        auto len       = Call(SCI_GETTEXTLENGTH);
        if (endStyled < len && pos.m_lastStyleLine)
            Call(SCI_COLOURISE, 0, Call(SCI_POSITIONFROMLINE, pos.m_lastStyleLine + 1));
    }

    for (const auto& foldLine : pos.m_lineStateVector)
    {
        auto level = Call(SCI_GETFOLDLEVEL, foldLine);

        auto headerLine = 0;
        if (level & SC_FOLDLEVELHEADERFLAG)
            headerLine = static_cast<int>(foldLine);
        else
        {
            headerLine = static_cast<int>(Call(SCI_GETFOLDPARENT, foldLine));
            if (headerLine == -1)
                return;
        }

        if (Call(SCI_GETFOLDEXPANDED, headerLine) != 0)
            Call(SCI_TOGGLEFOLD, headerLine);
    }

    Call(SCI_GOTOPOS, 0);

    Call(SCI_SETSELECTIONMODE, pos.m_nSelMode);
    Call(SCI_SETANCHOR, pos.m_nStartPos);
    Call(SCI_SETCURRENTPOS, pos.m_nEndPos);
    Call(SCI_CANCEL);
    auto wrapMode = Call(SCI_GETWRAPMODE);
    if (wrapMode == SC_WRAP_NONE)
    {
        // only offset if not wrapping, otherwise the offset isn't needed at all
        Call(SCI_SETSCROLLWIDTH, pos.m_nScrollWidth);
        Call(SCI_SETXOFFSET, pos.m_xOffset);
    }
    Call(SCI_CHOOSECARETX);

    auto lineToShow = Call(SCI_VISIBLEFROMDOCLINE, pos.m_nFirstVisibleLine);
    if (wrapMode != SC_WRAP_NONE)
    {
        // if wrapping is enabled, scrolling to a line won't work
        // properly until scintilla has painted the document, because
        // the wrap calculations aren't finished until then.
        // So we set the scroll position here to a member variable,
        // which then is used to scroll scintilla to that line after
        // the first SCN_PAINTED event.
        m_lineToScrollToAfterPaint        = pos.m_nFirstVisibleLine;
        m_wrapOffsetToScrollToAfterPaint  = pos.m_nWrapLineOffset;
        m_lineToScrollToAfterPaintCounter = 5;
    }
    else
        Call(SCI_LINESCROLL, 0, lineToShow);
    // call UpdateLineNumberWidth() here, just in case the SCI_LINESCROLL call
    // above does not scroll the window.
    UpdateLineNumberWidth();
}

void CScintillaWnd::SetupLexerForLang(const std::string& lang) const
{
    const auto& lexerData = CLexStyles::Instance().GetLexerDataForLang(lang);
    const auto& keywords  = CLexStyles::Instance().GetKeywordsForLang(lang);
    const auto& theme     = CTheme::Instance();

    wchar_t localeName[100];
    GetUserDefaultLocaleName(localeName, _countof(localeName));
    Call(SCI_SETFONTLOCALE, 0, reinterpret_cast<sptr_t>(const_cast<char*>(CUnicodeUtils::StdGetUTF8(localeName).c_str())));

    // first set up only the default styles
    std::wstring defaultFont;
    if (m_hasConsolas)
        defaultFont = L"Consolas";
    else
        defaultFont = L"Courier New";
    std::string sFontName = CUnicodeUtils::StdGetUTF8(CIniSettings::Instance().GetString(L"View", L"FontName", defaultFont.c_str()));
    Call(SCI_STYLESETFONT, STYLE_DEFAULT, reinterpret_cast<LPARAM>(sFontName.c_str()));
    bool bBold    = !!CIniSettings::Instance().GetInt64(L"View", L"FontBold", false);
    bool bItalic  = !!CIniSettings::Instance().GetInt64(L"View", L"FontItalic", false);
    int  fontSize = static_cast<int>(CIniSettings::Instance().GetInt64(L"View", L"FontSize", 11));
    Call(SCI_STYLESETBOLD, STYLE_DEFAULT, bBold);
    Call(SCI_STYLESETITALIC, STYLE_DEFAULT, bItalic);
    Call(SCI_STYLESETSIZE, STYLE_DEFAULT, fontSize);
    Call(SCI_STYLESETFORE, STYLE_DEFAULT, theme.GetThemeColor(RGB(0, 0, 0), true));
    Call(SCI_STYLESETBACK, STYLE_DEFAULT, theme.GetThemeColor(RGB(255, 255, 255), true));

    // now call SCI_STYLECLEARALL to copy the default style to all styles
    Call(SCI_STYLECLEARALL);

    // now set up the out own styles
    SetupDefaultStyles();

    // and now set the lexer styles
    Scintilla::ILexer5* lexer = nullptr;

    if (lexerData.name == "bp_simle")
        lexer = lmSimple.Create();
    if (lexerData.name == "bp_log")
        lexer = lmLog.Create();
    if (lexerData.name == "bp_snippets")
        lexer = lmSnippets.Create();
    if (lexer == nullptr && lexerData.name.empty())
    {
        switch (lexerData.id)
        {
            case 1100:
                lexer = lmSimple.Create();
                break;
            case 1101:
                lexer = lmLog.Create();
                break;
            default:
                break;
        }
    }

    if (lexer == nullptr)
        lexer = CreateLexer(lexerData.name.c_str());
    assert(lexer);
    Call(SCI_SETILEXER, 0, reinterpret_cast<sptr_t>(lexer));

    for (const auto& [propName, propValue] : lexerData.properties)
    {
        Call(SCI_SETPROPERTY, reinterpret_cast<WPARAM>(propName.c_str()), reinterpret_cast<LPARAM>(propValue.c_str()));
    }
    for (const auto& [styleId, styleData] : lexerData.styles)
    {
        Call(SCI_STYLESETFORE, styleId, theme.GetThemeColor(styleData.foregroundColor, true));
        Call(SCI_STYLESETBACK, styleId, theme.GetThemeColor(styleData.backgroundColor, true));
        if (!styleData.fontName.empty())
            Call(SCI_STYLESETFONT, styleId, reinterpret_cast<sptr_t>(CUnicodeUtils::StdGetUTF8(styleData.fontName).c_str()));

        if ((styleData.fontStyle & Fontstyle_Bold) != 0)
            Call(SCI_STYLESETBOLD, styleId, 1);
        if ((styleData.fontStyle & Fontstyle_Italic) != 0)
            Call(SCI_STYLESETITALIC, styleId, 1);
        if ((styleData.fontStyle & Fontstyle_Underlined) != 0)
            Call(SCI_STYLESETUNDERLINE, styleId, 1);

        if (styleData.fontSize)
            Call(SCI_STYLESETSIZE, styleId, styleData.fontSize);
        if (styleData.eolFilled)
            Call(SCI_STYLESETEOLFILLED, styleId, 1);
    }
    for (const auto& [keyWordId, keyWord] : keywords)
    {
        Call(SCI_SETKEYWORDS, keyWordId - 1LL, reinterpret_cast<LPARAM>(keyWord.c_str()));
    }
    Call(SCI_SETLINEENDTYPESALLOWED, Call(SCI_GETLINEENDTYPESSUPPORTED));
    CCommandHandler::Instance().OnStylesSet();
}

void CScintillaWnd::SetupDefaultStyles() const
{
    auto&        theme = CTheme::Instance();
    std::wstring defaultFont;
    if (m_hasConsolas)
        defaultFont = L"Consolas";
    else
        defaultFont = L"Courier New";
    std::string sFontName = CUnicodeUtils::StdGetUTF8(CIniSettings::Instance().GetString(L"View", L"FontName", defaultFont.c_str()));
    Call(SCI_STYLESETFONT, STYLE_LINENUMBER, reinterpret_cast<LPARAM>(sFontName.c_str()));

    Call(SCI_STYLESETFORE, STYLE_LINENUMBER, theme.GetThemeColor(RGB(color_linenr_inactive, color_linenr_inactive, color_linenr_inactive), true));
    Call(SCI_STYLESETBACK, STYLE_LINENUMBER, theme.GetThemeColor(RGB(230, 230, 230), true));

    Call(SCI_INDICSETSTYLE, INDIC_SELECTION_MARK, INDIC_ROUNDBOX);
    Call(SCI_INDICSETALPHA, INDIC_SELECTION_MARK, 50);
    Call(SCI_INDICSETUNDER, INDIC_SELECTION_MARK, true);
    Call(SCI_INDICSETFORE, INDIC_SELECTION_MARK, theme.GetThemeColor(RGB(0, 255, 0), true));

    Call(SCI_INDICSETSTYLE, INDIC_SNIPPETPOS, INDIC_ROUNDBOX);
    Call(SCI_INDICSETALPHA, INDIC_SNIPPETPOS, 50);
    Call(SCI_INDICSETOUTLINEALPHA, INDIC_SNIPPETPOS, 255);
    Call(SCI_INDICSETUNDER, INDIC_SNIPPETPOS, true);
    Call(SCI_INDICSETFORE, INDIC_SNIPPETPOS, theme.GetThemeColor(RGB(180, 180, 0), true));

    Call(SCI_INDICSETSTYLE, INDIC_FINDTEXT_MARK, INDIC_ROUNDBOX);
    Call(SCI_INDICSETALPHA, INDIC_FINDTEXT_MARK, theme.IsDarkTheme() ? 100 : 200);
    Call(SCI_INDICSETUNDER, INDIC_FINDTEXT_MARK, true);
    Call(SCI_INDICSETFORE, INDIC_FINDTEXT_MARK, theme.GetThemeColor(RGB(255, 255, 0), true));

    Call(SCI_INDICSETSTYLE, INDIC_URLHOTSPOT, INDIC_HIDDEN);
    Call(SCI_INDICSETALPHA, INDIC_URLHOTSPOT, 5);
    Call(SCI_INDICSETUNDER, INDIC_URLHOTSPOT, true);
    Call(SCI_INDICSETHOVERSTYLE, INDIC_URLHOTSPOT, INDIC_DOTS);
    Call(SCI_INDICSETHOVERFORE, INDIC_URLHOTSPOT, theme.GetThemeColor(RGB(0, 0, 255), true));

    Call(SCI_INDICSETSTYLE, INDIC_BRACEMATCH, INDIC_ROUNDBOX);
    Call(SCI_INDICSETALPHA, INDIC_BRACEMATCH, 30);
    Call(SCI_INDICSETOUTLINEALPHA, INDIC_BRACEMATCH, 0);
    Call(SCI_INDICSETUNDER, INDIC_BRACEMATCH, true);
    Call(SCI_INDICSETFORE, INDIC_BRACEMATCH, theme.GetThemeColor(RGB(0, 150, 0), true));

    Call(SCI_INDICSETSTYLE, INDIC_MISSPELLED, INDIC_SQUIGGLE);
    Call(SCI_INDICSETFORE, INDIC_MISSPELLED, theme.GetThemeColor(RGB(255, 0, 0), true));
    Call(SCI_INDICSETUNDER, INDIC_MISSPELLED, true);

    Call(SCI_STYLESETFORE, STYLE_BRACELIGHT, theme.GetThemeColor(RGB(0, 150, 0), true));
    Call(SCI_STYLESETBOLD, STYLE_BRACELIGHT, 1);
    Call(SCI_STYLESETFORE, STYLE_BRACEBAD, theme.GetThemeColor(RGB(255, 0, 0), true));
    Call(SCI_STYLESETBOLD, STYLE_BRACEBAD, 1);

    Call(SCI_STYLESETFORE, STYLE_INDENTGUIDE, theme.GetThemeColor(RGB(200, 200, 200), true));

    Call(SCI_INDICSETFORE, INDIC_TAGMATCH, theme.GetThemeColor(RGB(0x80, 0x00, 0xFF), true));
    Call(SCI_INDICSETFORE, INDIC_TAGATTR, theme.GetThemeColor(RGB(0xFF, 0xFF, 0x00), true));
    Call(SCI_INDICSETSTYLE, INDIC_TAGMATCH, INDIC_ROUNDBOX);
    Call(SCI_INDICSETSTYLE, INDIC_TAGATTR, INDIC_ROUNDBOX);
    Call(SCI_INDICSETALPHA, INDIC_TAGMATCH, 100);
    Call(SCI_INDICSETALPHA, INDIC_TAGATTR, 100);
    Call(SCI_INDICSETUNDER, INDIC_TAGMATCH, true);
    Call(SCI_INDICSETUNDER, INDIC_TAGATTR, true);

    Call(SCI_CALLTIPUSESTYLE, 0);
    Call(SCI_STYLESETFORE, STYLE_CALLTIP, theme.GetThemeColor(GetSysColor(COLOR_INFOTEXT)));
    Call(SCI_STYLESETBACK, STYLE_CALLTIP, theme.GetThemeColor(GetSysColor(COLOR_INFOBK)));

    std::vector captureColors = {RGB(80, 0, 0),
                                 RGB(0, 80, 0),
                                 RGB(0, 0, 80),
                                 RGB(0, 80, 80),
                                 RGB(80, 80, 0),
                                 RGB(80, 0, 80),
                                 RGB(80, 80, 80),
                                 RGB(80, 40, 40),
                                 RGB(40, 80, 40),
                                 RGB(40, 40, 80)};
    for (uptr_t i = INDIC_REGEXCAPTURE; i < INDIC_REGEXCAPTURE_END; ++i)
    {
        Call(SCI_INDICSETSTYLE, i, INDIC_BOX);
        Call(SCI_INDICSETFORE, i, CTheme::Instance().GetThemeColor(captureColors[i - INDIC_REGEXCAPTURE]));
    }

    Call(SCI_SETFOLDMARGINCOLOUR, true, theme.GetThemeColor(RGB(240, 240, 240), true));
    Call(SCI_SETFOLDMARGINHICOLOUR, true, theme.GetThemeColor(RGB(255, 255, 255), true));

    Call(SCI_MARKERSETFORE, MARK_BOOKMARK, CTheme::Instance().GetThemeColor(RGB(80, 0, 0), true));
    Call(SCI_MARKERSETBACK, MARK_BOOKMARK, CTheme::Instance().GetThemeColor(RGB(255, 0, 0), true));

    Call(SCI_SETINDENTATIONGUIDES, static_cast<uptr_t>(CIniSettings::Instance().GetInt64(L"View", L"indent", SC_IV_LOOKBOTH)));

    SetupFoldingColors(RGB(color_folding_fore_inactive, color_folding_fore_inactive, color_folding_fore_inactive),
                       RGB(color_folding_back_inactive, color_folding_back_inactive, color_folding_back_inactive),
                       RGB(color_folding_backsel_inactive, color_folding_backsel_inactive, color_folding_backsel_inactive));

    bool bBold    = !!CIniSettings::Instance().GetInt64(L"View", L"FontBold", false);
    int  fontSize = static_cast<int>(CIniSettings::Instance().GetInt64(L"View", L"FontSize", 11));
    Call(SCI_STYLESETBOLD, STYLE_FOLDDISPLAYTEXT, bBold);
    Call(SCI_STYLESETITALIC, STYLE_FOLDDISPLAYTEXT, true);
    Call(SCI_STYLESETSIZE, STYLE_FOLDDISPLAYTEXT, fontSize);
    Call(SCI_STYLESETFORE, STYLE_FOLDDISPLAYTEXT, theme.GetThemeColor(RGB(120, 120, 120), true));
    Call(SCI_STYLESETBACK, STYLE_FOLDDISPLAYTEXT, theme.GetThemeColor(RGB(230, 230, 230), true));

    Call(SCI_STYLESETFORE, STYLE_DEFAULT, theme.GetThemeColor(RGB(0, 0, 0), true));
    Call(SCI_STYLESETBACK, STYLE_DEFAULT, theme.GetThemeColor(RGB(255, 255, 255), true));
    Call(SCI_SETSELFORE, TRUE, theme.GetThemeColor(RGB(0, 0, 0), true));
    Call(SCI_SETSELBACK, TRUE, theme.GetThemeColor(RGB(51, 153, 255), true));
    Call(SCI_SETCARETFORE, theme.GetThemeColor(RGB(0, 0, 0), true));
    Call(SCI_SETADDITIONALCARETFORE, theme.GetThemeColor(RGB(0, 0, 80), true));

    if (CIniSettings::Instance().GetInt64(L"View", L"caretlineframe", 1) != 0)
    {
        Call(SCI_SETCARETLINEBACK, theme.GetThemeColor(RGB(0, 0, 0), true));
        Call(SCI_SETCARETLINEBACKALPHA, 80);
    }
    else
    {
        if (theme.IsDarkTheme())
        {
            Call(SCI_SETCARETLINEBACK, RGB(0, 0, 0));
            Call(SCI_SETCARETLINEBACKALPHA, SC_ALPHA_NOALPHA);
        }
        else
        {
            Call(SCI_SETCARETLINEBACK, theme.GetThemeColor(RGB(0, 0, 0), true));
            Call(SCI_SETCARETLINEBACKALPHA, 25);
        }
    }
    Call(SCI_SETWHITESPACEFORE, true, theme.IsDarkTheme() ? RGB(20, 72, 82) : RGB(43, 145, 175));

    auto modEventMask = Call(SCI_GETMODEVENTMASK);
    Call(SCI_SETMODEVENTMASK, 0);

    Call(SCI_COLOURISE, 0, Call(SCI_POSITIONFROMLINE, Call(SCI_LINESONSCREEN) + 1));
    Call(SCI_SETCODEPAGE, CP_UTF8);
    Call(SCI_SETMODEVENTMASK, modEventMask);

    // set up unicode representations for control chars
    for (int c = 0; c < 0x20; ++c)
    {
        const char sC[2] = {static_cast<char>(c), 0};
        Call(SCI_SETREPRESENTATION, reinterpret_cast<uptr_t>(sC), reinterpret_cast<sptr_t>(CStringUtils::Format("x%02X", c).c_str()));
    }
    for (int c = 0x80; c < 0xA0; ++c)
    {
        const char sC[3] = {'\xc2', static_cast<char>(c), 0};
        Call(SCI_SETREPRESENTATION, reinterpret_cast<uptr_t>(sC), reinterpret_cast<sptr_t>(CStringUtils::Format("x%02X", c).c_str()));
    }
    Call(SCI_SETREPRESENTATION, reinterpret_cast<uptr_t>("\x7F"), reinterpret_cast<sptr_t>("x7F"));

    if (theme.IsDarkTheme())
    {
        Call(SCI_SETELEMENTCOLOUR, SC_ELEMENT_LIST, RGB(187, 187, 187));
        Call(SCI_SETELEMENTCOLOUR, SC_ELEMENT_LIST_BACK, RGB(15, 15, 15));
        Call(SCI_SETELEMENTCOLOUR, SC_ELEMENT_LIST_SELECTED, RGB(187, 187, 187));
        Call(SCI_SETELEMENTCOLOUR, SC_ELEMENT_LIST_SELECTED_BACK, RGB(80, 80, 80));
    }
    else
    {
        Call(SCI_RESETELEMENTCOLOUR, SC_ELEMENT_LIST);
        Call(SCI_RESETELEMENTCOLOUR, SC_ELEMENT_LIST_BACK);
        Call(SCI_RESETELEMENTCOLOUR, SC_ELEMENT_LIST_SELECTED);
        Call(SCI_RESETELEMENTCOLOUR, SC_ELEMENT_LIST_SELECTED_BACK);
    }
}

void CScintillaWnd::SetupFoldingColors(COLORREF fore, COLORREF back, COLORREF backSel) const
{
    // set the folding colors according to the parameters, but leave the
    // colors for the collapsed buttons (+ button) in the active color.
    auto foldMarkFore       = CTheme::Instance().GetThemeColor(fore, true);
    auto foldMarkForeActive = CTheme::Instance().GetThemeColor(RGB(color_folding_fore_active, color_folding_fore_active, color_folding_fore_active), true);
    Call(SCI_MARKERSETFORE, SC_MARKNUM_FOLDEROPEN, foldMarkFore);
    Call(SCI_MARKERSETFORE, SC_MARKNUM_FOLDER, foldMarkForeActive);
    Call(SCI_MARKERSETFORE, SC_MARKNUM_FOLDERSUB, foldMarkFore);
    Call(SCI_MARKERSETFORE, SC_MARKNUM_FOLDERTAIL, foldMarkFore);
    Call(SCI_MARKERSETFORE, SC_MARKNUM_FOLDEREND, foldMarkForeActive);
    Call(SCI_MARKERSETFORE, SC_MARKNUM_FOLDEROPENMID, foldMarkFore);
    Call(SCI_MARKERSETFORE, SC_MARKNUM_FOLDERMIDTAIL, foldMarkFore);

    auto foldMarkBack       = CTheme::Instance().GetThemeColor(back, true);
    auto foldMarkBackActive = CTheme::Instance().GetThemeColor(RGB(color_folding_back_active, color_folding_back_active, color_folding_back_active), true);
    Call(SCI_MARKERSETBACK, SC_MARKNUM_FOLDEROPEN, foldMarkBack);
    Call(SCI_MARKERSETBACK, SC_MARKNUM_FOLDER, foldMarkBackActive);
    Call(SCI_MARKERSETBACK, SC_MARKNUM_FOLDERSUB, foldMarkBack);
    Call(SCI_MARKERSETBACK, SC_MARKNUM_FOLDERTAIL, foldMarkBack);
    Call(SCI_MARKERSETBACK, SC_MARKNUM_FOLDEREND, foldMarkBackActive);
    Call(SCI_MARKERSETBACK, SC_MARKNUM_FOLDEROPENMID, foldMarkBack);
    Call(SCI_MARKERSETBACK, SC_MARKNUM_FOLDERMIDTAIL, foldMarkBack);

    auto foldMarkBackSelected       = CTheme::Instance().GetThemeColor(backSel, true);
    auto foldMarkBackSelectedActive = CTheme::Instance().GetThemeColor(RGB(color_folding_backsel_active, color_folding_backsel_active, color_folding_backsel_active), true);
    Call(SCI_MARKERSETBACKSELECTED, SC_MARKNUM_FOLDEROPEN, foldMarkBackSelected);
    Call(SCI_MARKERSETBACKSELECTED, SC_MARKNUM_FOLDER, foldMarkBackSelectedActive);
    Call(SCI_MARKERSETBACKSELECTED, SC_MARKNUM_FOLDERSUB, foldMarkBackSelected);
    Call(SCI_MARKERSETBACKSELECTED, SC_MARKNUM_FOLDERTAIL, foldMarkBackSelected);
    Call(SCI_MARKERSETBACKSELECTED, SC_MARKNUM_FOLDEREND, foldMarkBackSelectedActive);
    Call(SCI_MARKERSETBACKSELECTED, SC_MARKNUM_FOLDEROPENMID, foldMarkBackSelected);
    Call(SCI_MARKERSETBACKSELECTED, SC_MARKNUM_FOLDERMIDTAIL, foldMarkBackSelected);

    //Call(SCI_STYLESETFORE, STYLE_FOLDDISPLAYTEXT, foldmarkfore);
    //Call(SCI_STYLESETBACK, STYLE_FOLDDISPLAYTEXT, foldmarkback);
}

void CScintillaWnd::GotoLine(sptr_t line)
{
    auto linePos = Call(SCI_POSITIONFROMLINE, line);
    Center(linePos, linePos);
}

void CScintillaWnd::Center(sptr_t posStart, sptr_t posEnd)
{
    // to make sure the found result is visible
    // When searching up, the beginning of the (possible multiline) result is important, when scrolling down the end
    auto testPos = (posStart > posEnd) ? posEnd : posStart;
    Call(SCI_SETCURRENTPOS, testPos);
    auto currentLineNumberDoc = Call(SCI_LINEFROMPOSITION, testPos);
    auto currentLineNumberVis = Call(SCI_VISIBLEFROMDOCLINE, currentLineNumberDoc);
    // SCI_ENSUREVISIBLE resets the line-wrapping cache, so only
    // call that if it's really necessary.
    if (Call(SCI_GETLINEVISIBLE, currentLineNumberDoc) == 0)
        Call(SCI_ENSUREVISIBLE, currentLineNumberDoc); // make sure target line is unfolded

    auto firstVisibleLineVis = Call(SCI_GETFIRSTVISIBLELINE);
    auto linesVisible        = Call(SCI_LINESONSCREEN) - 1; //-1 for the scrollbar
    auto lastVisibleLineVis  = linesVisible + firstVisibleLineVis;

    // if out of view vertically, scroll line into (center of) view
    // If m_LineToScrollToAfterPaint is not equal -1, modify its value
    // and wait for the paint to complete, then go to the line.
    if (m_lineToScrollToAfterPaint != -1)
    {
        m_lineToScrollToAfterPaint        = currentLineNumberDoc;
        m_wrapOffsetToScrollToAfterPaint  = 0;
        m_lineToScrollToAfterPaintCounter = 5;
    }
    else
    {
        decltype(firstVisibleLineVis) linesToScroll = 0;
        if (currentLineNumberVis < (firstVisibleLineVis + (linesVisible / 4)))
        {
            linesToScroll = currentLineNumberVis - firstVisibleLineVis;
            // use center
            linesToScroll -= linesVisible / 2;
        }
        else if (currentLineNumberVis > (lastVisibleLineVis - (linesVisible / 4)))
        {
            linesToScroll = currentLineNumberVis - lastVisibleLineVis;
            // use center
            linesToScroll += linesVisible / 2;
        }
        Call(SCI_LINESCROLL, 0, linesToScroll);
    }
    // Make sure the caret is visible, scroll horizontally
    Call(SCI_GOTOPOS, posStart);
    Call(SCI_GOTOPOS, posEnd);

    Call(SCI_SETANCHOR, posStart);
}

void CScintillaWnd::MarginClick(SCNotification* pNotification)
{
    if ((pNotification->margin == SC_MARGE_SYMBOL) && !pNotification->modifiers)
        BookmarkToggle(Call(SCI_LINEFROMPOSITION, pNotification->position));
    m_docScroll.VisibleLinesChanged();
}

void CScintillaWnd::SelectionUpdated() const
{
    auto selStart     = Call(SCI_GETSELECTIONNSTART);
    auto selEnd       = Call(SCI_GETSELECTIONEND);
    auto selLineStart = Call(SCI_LINEFROMPOSITION, selStart);
    auto selLineEnd   = Call(SCI_LINEFROMPOSITION, selEnd);
    if (selLineStart != selLineEnd)
    {
        if (Call(SCI_GETFOLDLEVEL, selLineEnd - 1) & SC_FOLDLEVELHEADERFLAG)
        {
            // line is a fold header
            if (Call(SCI_GETLINEVISIBLE, selLineEnd) == 0)
            {
                if (Call(SCI_POSITIONFROMLINE, selLineEnd) == selEnd)
                {
                    auto lastLine = Call(SCI_GETLINECOUNT);
                    while ((selLineEnd < lastLine) && (Call(SCI_GETLINEVISIBLE, selLineEnd) == 0))
                        ++selLineEnd;
                    if (selLineEnd < lastLine)
                    {
                        if (Call(SCI_GETANCHOR) == selStart)
                            Call(SCI_SETCURRENTPOS, Call(SCI_POSITIONFROMLINE, selLineEnd));
                        else
                            Call(SCI_SETANCHOR, Call(SCI_POSITIONFROMLINE, selLineEnd));
                    }
                }
            }
        }
    }
}

void CScintillaWnd::MarkSelectedWord(bool clear, bool edit)
{
    static std::string    lastSelText;
    static Sci_PositionCR lastStopPosition = 0;
    auto                  firstLine        = Call(SCI_GETFIRSTVISIBLELINE);
    auto                  lastLine         = firstLine + Call(SCI_LINESONSCREEN);
    auto                  startStylePos    = Call(SCI_POSITIONFROMLINE, firstLine);
    startStylePos                          = max(startStylePos, 0);
    auto endStylePos                       = Call(SCI_POSITIONFROMLINE, lastLine) + Call(SCI_LINELENGTH, lastLine);
    if (endStylePos < 0)
        endStylePos = Call(SCI_GETLENGTH);

    int len = endStylePos - startStylePos;
    if (len <= 0)
        return;

    // reset indicators
    Call(SCI_SETINDICATORCURRENT, INDIC_SELECTION_MARK);
    Call(SCI_INDICATORCLEARRANGE, startStylePos, len);

    auto selTextLen = Call(SCI_GETSELTEXT);
    if ((selTextLen <= 1) || (clear)) // Includes zero terminator so 1 means 0.
    {
        lastSelText.clear();
        m_docScroll.Clear(DOCSCROLLTYPE_SELTEXT);
        m_selTextMarkerCount = 0;
        SendMessage(*this, WM_NCPAINT, static_cast<WPARAM>(1), 0);
        return;
    }
    auto origSelStart = Call(SCI_GETSELECTIONSTART);
    auto origSelEnd   = Call(SCI_GETSELECTIONEND);
    auto selStartLine = Call(SCI_LINEFROMPOSITION, origSelStart, 0);
    auto selEndLine   = Call(SCI_LINEFROMPOSITION, origSelEnd, 0);
    if (selStartLine != selEndLine)
    {
        lastSelText.clear();
        m_docScroll.Clear(DOCSCROLLTYPE_SELTEXT);
        m_selTextMarkerCount = 0;
        SendMessage(*this, WM_NCPAINT, static_cast<WPARAM>(1), 0);
        return;
    }

    auto selStartPos   = Call(SCI_GETSELECTIONSTART);
    auto selTextBuffer = std::make_unique<char[]>(selTextLen + 1);
    Call(SCI_GETSELTEXT, 0, reinterpret_cast<LPARAM>(selTextBuffer.get()));
    if (selTextBuffer[0] == 0)
    {
        lastSelText.clear();
        m_docScroll.Clear(DOCSCROLLTYPE_SELTEXT);
        m_selTextMarkerCount = 0;
        SendMessage(*this, WM_NCPAINT, static_cast<WPARAM>(1), 0);
        return;
    }
    std::string sSelText = selTextBuffer.get();
    CStringUtils::trim(sSelText);
    if (sSelText.empty())
    {
        lastSelText.clear();
        m_docScroll.Clear(DOCSCROLLTYPE_SELTEXT);
        m_selTextMarkerCount = 0;
        SendMessage(*this, WM_NCPAINT, static_cast<WPARAM>(1), 0);
        return;
    }
    // don't mark the text again if it's already marked by the search feature
    if (_stricmp(g_sHighlightString.c_str(), selTextBuffer.get()) == 0)
    {
        m_selTextMarkerCount = g_searchMarkerCount;
        return;
    }

    auto          textBuffer = std::make_unique<char[]>(len + 1LL);
    Sci_TextRange textRange{};
    textRange.lpstrText  = textBuffer.get();
    textRange.chrg.cpMin = startStylePos;
    textRange.chrg.cpMax = endStylePos;
    Call(SCI_GETTEXTRANGE, 0, reinterpret_cast<LPARAM>(&textRange));

    const char* startPos = strstr(textBuffer.get(), selTextBuffer.get());
    while (startPos)
    {
        // don't style the selected text itself
        if (selStartPos != static_cast<sptr_t>(startStylePos + (startPos - textBuffer.get())))
            Call(SCI_INDICATORFILLRANGE, startStylePos + (startPos - textBuffer.get()), selTextLen - 1);
        startPos = strstr(startPos + 1, selTextBuffer.get());
    }

    auto lineCount = Call(SCI_GETLINECOUNT);
    if ((selTextLen > 2) || (lineCount < 100000))
    {
        auto selTextDifferent = lastSelText.compare(selTextBuffer.get());
        if (selTextDifferent || (lastStopPosition != 0) || edit)
        {
            auto start = std::chrono::steady_clock::now();
            if (selTextDifferent)
            {
                m_docScroll.Clear(DOCSCROLLTYPE_SELTEXT);
                m_selTextMarkerCount = 0;
            }
            Sci_TextToFind findText{};
            findText.chrg.cpMin     = lastStopPosition;
            findText.chrg.cpMax     = Call(SCI_GETLENGTH);
            findText.lpstrText      = selTextBuffer.get();
            lastStopPosition        = 0;
            const auto selTextColor = CTheme::Instance().GetThemeColor(RGB(0, 255, 0), true);
            while (Call(SCI_FINDTEXT, SCFIND_MATCHCASE, reinterpret_cast<LPARAM>(&findText)) >= 0)
            {
                if (edit)
                {
                    if ((origSelStart != findText.chrgText.cpMin) || (origSelEnd != findText.chrgText.cpMax))
                        Call(SCI_ADDSELECTION, findText.chrgText.cpMax, findText.chrgText.cpMin);
                }
                auto line = Call(SCI_LINEFROMPOSITION, findText.chrgText.cpMin);
                m_docScroll.AddLineColor(DOCSCROLLTYPE_SELTEXT, line, selTextColor);
                ++m_selTextMarkerCount;
                if (findText.chrg.cpMin >= findText.chrgText.cpMax)
                    break;
                findText.chrg.cpMin = findText.chrgText.cpMax;

                if (!edit)
                {
                    // stop after 1.5 seconds - users don't want to wait for too long
                    auto end = std::chrono::steady_clock::now();
                    if (std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() > 1500)
                    {
                        lastStopPosition = findText.chrg.cpMin;
                        break;
                    }
                }
            }
            if (edit)
                Call(SCI_ADDSELECTION, origSelEnd, origSelStart);
            SendMessage(*this, WM_NCPAINT, static_cast<WPARAM>(1), 0);
        }
        lastSelText = selTextBuffer.get();
    }
}

void CScintillaWnd::MatchBraces(BraceMatch what)
{
    static sptr_t lastIndicatorStart  = 0;
    static sptr_t lastIndicatorLength = 0;
    static sptr_t lastCaretPos        = 0;

    sptr_t braceAtCaret  = -1;
    sptr_t braceOpposite = -1;

    // find matching brace position
    auto caretPos = Call(SCI_GETCURRENTPOS);

    // setting the highlighting style triggers an UI update notification,
    // which in return calls MatchBraces(false). So to avoid an endless
    // loop, we bail out if the caret position has not changed.
    if ((what == BraceMatch::Braces) && (caretPos == lastCaretPos))
        return;
    lastCaretPos = caretPos;

    WCHAR charBefore = '\0';

    auto lengthDoc = Call(SCI_GETLENGTH);

    if ((lengthDoc > 0) && (caretPos > 0))
    {
        charBefore = static_cast<WCHAR>(Call(SCI_GETCHARAT, caretPos - 1, 0));
    }
    // Priority goes to character before the caret
    if (charBefore && wcschr(L"[](){}", charBefore))
    {
        braceAtCaret = caretPos - 1;
    }

    if (lengthDoc > 0 && (braceAtCaret < 0))
    {
        // No brace found so check other side
        WCHAR charAfter = static_cast<WCHAR>(Call(SCI_GETCHARAT, caretPos, 0));
        if (charAfter && wcschr(L"[](){}", charAfter))
        {
            braceAtCaret = caretPos;
        }
    }
    if (braceAtCaret >= 0)
        braceOpposite = Call(SCI_BRACEMATCH, braceAtCaret, 0);

    KillTimer(*this, TIM_BRACEHIGHLIGHTTEXT);
    KillTimer(*this, TIM_BRACEHIGHLIGHTTEXTCLEAR);
    Call(SCI_SETHIGHLIGHTGUIDE, 0);
    if ((braceAtCaret != -1) && (braceOpposite == -1))
    {
        Call(SCI_BRACEBADLIGHT, braceAtCaret);
        if (lastIndicatorLength && CIniSettings::Instance().GetInt64(L"View", L"bracehighlighttext", 1))
        {
            Call(SCI_SETINDICATORCURRENT, INDIC_BRACEMATCH);
            Call(SCI_INDICATORCLEARRANGE, lastIndicatorStart, lastIndicatorLength);
            lastIndicatorLength = 0;
        }
    }
    else
    {
        Call(SCI_BRACEHIGHLIGHT, braceAtCaret, braceOpposite);
        if (CIniSettings::Instance().GetInt64(L"View", L"bracehighlighttext", 1))
        {
            if (lastIndicatorLength)
            {
                Call(SCI_SETINDICATORCURRENT, INDIC_BRACEMATCH);
                Call(SCI_INDICATORCLEARRANGE, lastIndicatorStart, lastIndicatorLength);
            }
            if (what == BraceMatch::Highlight)
            {
                Call(SCI_SETINDICATORCURRENT, INDIC_BRACEMATCH);
                lastIndicatorStart  = braceAtCaret < braceOpposite ? braceAtCaret : braceOpposite;
                lastIndicatorLength = braceAtCaret < braceOpposite ? braceOpposite - braceAtCaret : braceAtCaret - braceOpposite;
                ++lastIndicatorLength;
                auto startLine = Call(SCI_LINEFROMPOSITION, lastIndicatorStart);
                auto endLine   = Call(SCI_LINEFROMPOSITION, lastIndicatorStart + lastIndicatorLength);
                if (endLine != startLine)
                    Call(SCI_INDICSETALPHA, INDIC_BRACEMATCH, CTheme::Instance().IsDarkTheme() ? 3 : 10);
                else
                    Call(SCI_INDICSETALPHA, INDIC_BRACEMATCH, 40);

                Call(SCI_INDICATORFILLRANGE, lastIndicatorStart, lastIndicatorLength);
                SetTimer(*this, TIM_BRACEHIGHLIGHTTEXTCLEAR, 5000, nullptr);
            }
            else if (what == BraceMatch::Braces)
                SetTimer(*this, TIM_BRACEHIGHLIGHTTEXT, 1000, nullptr);
        }

        if ((what == BraceMatch::Highlight) && (Call(SCI_GETINDENTATIONGUIDES) != 0))
        {
            auto columnAtCaret  = Call(SCI_GETCOLUMN, braceAtCaret);
            auto columnOpposite = Call(SCI_GETCOLUMN, braceOpposite);
            Call(SCI_SETHIGHLIGHTGUIDE, (columnAtCaret < columnOpposite) ? columnAtCaret : columnOpposite);
        }
    }
}

// find matching brace position
void CScintillaWnd::GotoBrace() const
{
    static constexpr wchar_t brackets[] = L"[](){}";

    auto lengthDoc = Call(SCI_GETLENGTH);
    if (lengthDoc <= 1)
        return;

    sptr_t braceAtCaret  = -1;
    sptr_t braceOpposite = -1;
    WCHAR  charBefore    = '\0';
    WCHAR  charAfter     = '\0';
    bool   shift         = (GetKeyState(VK_SHIFT) & 0x8000) != 0;

    auto caretPos = Call(SCI_GETCURRENTPOS);
    if (caretPos > 0)
    {
        charBefore = static_cast<WCHAR>(Call(SCI_GETCHARAT, caretPos - 1, 0));
        if (charBefore && wcschr(brackets, charBefore)) // Priority goes to character before the caret.
            braceAtCaret = caretPos - 1;
    }

    if (braceAtCaret < 0) // No brace found so check other side
    {
        charAfter = static_cast<WCHAR>(Call(SCI_GETCHARAT, caretPos, 0));
        if (charAfter && wcschr(brackets, charAfter))
            braceAtCaret = caretPos;
    }
    if (braceAtCaret >= 0)
    {
        braceOpposite = Call(SCI_BRACEMATCH, braceAtCaret, 0);
        if (braceOpposite >= 0)
        {
            if (shift)
            {
                if (braceOpposite >= braceAtCaret)
                    ++braceOpposite;
                else
                    ++braceAtCaret;
            }
            else if (braceAtCaret >= 0 && braceOpposite >= 0 && braceOpposite > braceAtCaret && braceOpposite == braceAtCaret + 1 && charBefore)
            {
                --braceOpposite;
            }
        }
    }

    if (braceOpposite >= 0)
    {
        Call(SCI_SETSEL, shift ? braceAtCaret : braceOpposite, braceOpposite);
    }
}

void CScintillaWnd::MatchTags() const
{
    // basically the same as MatchBraces(), but much more complicated since
    // finding xml tags is harder than just finding braces...

    sptr_t docStart = 0;
    sptr_t docEnd   = Call(SCI_GETLENGTH);
    Call(SCI_SETINDICATORCURRENT, INDIC_TAGMATCH);
    Call(SCI_INDICATORCLEARRANGE, docStart, docEnd - docStart);
    Call(SCI_SETINDICATORCURRENT, INDIC_TAGATTR);
    Call(SCI_INDICATORCLEARRANGE, docStart, docEnd - docStart);

    int lexer = static_cast<int>(Call(SCI_GETLEXER));
    if ((lexer != SCLEX_HTML) &&
        (lexer != SCLEX_XML) &&
        (lexer != SCLEX_PHPSCRIPT))
        return;

    // Get the original targets and search options to restore after tag matching operation
    auto originalStartPos    = Call(SCI_GETTARGETSTART);
    auto originalEndPos      = Call(SCI_GETTARGETEND);
    auto originalSearchFlags = Call(SCI_GETSEARCHFLAGS);

    XmlMatchedTagsPos xmlTags = {0};

    // Detect if it's a xml/html tag
    if (GetXmlMatchedTagsPos(xmlTags))
    {
        Call(SCI_SETINDICATORCURRENT, INDIC_TAGMATCH);
        int openTagTailLen = 2;

        // Coloring the close tag first
        if ((xmlTags.tagCloseStart != -1) && (xmlTags.tagCloseEnd != -1))
        {
            Call(SCI_INDICATORFILLRANGE, xmlTags.tagCloseStart, xmlTags.tagCloseEnd - xmlTags.tagCloseStart);
            // tag close is present, so it's not single tag
            openTagTailLen = 1;
        }

        // Coloring the open tag
        Call(SCI_INDICATORFILLRANGE, xmlTags.tagOpenStart, xmlTags.tagNameEnd - xmlTags.tagOpenStart);
        Call(SCI_INDICATORFILLRANGE, xmlTags.tagOpenEnd - openTagTailLen, openTagTailLen);

        // Coloring its attributes
        auto attributes = GetAttributesPos(xmlTags.tagNameEnd, xmlTags.tagOpenEnd - openTagTailLen);
        Call(SCI_SETINDICATORCURRENT, INDIC_TAGATTR);
        for (const auto& [startPos, endPos] : attributes)
        {
            Call(SCI_INDICATORFILLRANGE, startPos, endPos - startPos);
        }

        // Coloring indent guide line position
        if (Call(SCI_GETINDENTATIONGUIDES) != 0)
        {
            auto columnAtCaret  = Call(SCI_GETCOLUMN, xmlTags.tagOpenStart);
            auto columnOpposite = Call(SCI_GETCOLUMN, xmlTags.tagCloseStart);

            auto lineAtCaret  = Call(SCI_LINEFROMPOSITION, xmlTags.tagOpenStart);
            auto lineOpposite = Call(SCI_LINEFROMPOSITION, xmlTags.tagCloseStart);

            if (xmlTags.tagCloseStart != -1 && lineAtCaret != lineOpposite)
            {
                Call(SCI_BRACEHIGHLIGHT, xmlTags.tagOpenStart, xmlTags.tagCloseEnd - 1);
                Call(SCI_SETHIGHLIGHTGUIDE, (columnAtCaret < columnOpposite) ? columnAtCaret : columnOpposite);
            }
        }
    }

    // restore the original targets and search options to avoid the conflict with search/replace function
    Call(SCI_SETTARGETSTART, originalStartPos);
    Call(SCI_SETTARGETEND, originalEndPos);
    Call(SCI_SETSEARCHFLAGS, originalSearchFlags);
}

bool CScintillaWnd::GetSelectedCount(sptr_t& selByte, sptr_t& selLine) const
{
    auto selCount = Call(SCI_GETSELECTIONS);
    selByte       = 0;
    selLine       = 0;
    for (decltype(selCount) i = 0; i < selCount; ++i)
    {
        auto start = Call(SCI_GETSELECTIONNSTART, i);
        auto end   = Call(SCI_GETSELECTIONNEND, i);
        selByte += (start < end) ? end - start : start - end;

        start = Call(SCI_LINEFROMPOSITION, start);
        end   = Call(SCI_LINEFROMPOSITION, end);
        selLine += (start < end) ? end - start : start - end;
        ++selLine;
    }

    return true;
};

LRESULT CALLBACK CScintillaWnd::HandleScrollbarCustomDraw(WPARAM wParam, NMCSBCUSTOMDRAW* pCustomDraw)
{
    m_docScroll.SetCurrentPos(Call(SCI_VISIBLEFROMDOCLINE, GetCurrentLineNumber()), CTheme::Instance().GetThemeColor(RGB(40, 40, 40), true));
    m_docScroll.SetTotalLines(Call(SCI_GETLINECOUNT));
    return m_docScroll.HandleCustomDraw(wParam, pCustomDraw);
}

bool CScintillaWnd::GetXmlMatchedTagsPos(XmlMatchedTagsPos& xmlTags) const
{
    bool       tagFound         = false;
    auto       caret            = Call(SCI_GETCURRENTPOS);
    auto       searchStartPoint = caret;
    sptr_t     styleAt          = 0;
    FindResult openFound{};

    // Search back for the previous open angle bracket.
    // Keep looking whilst the angle bracket found is inside an XML attribute
    do
    {
        openFound        = FindText("<", searchStartPoint, 0, 0);
        styleAt          = Call(SCI_GETSTYLEAT, openFound.start);
        searchStartPoint = openFound.start - 1;
    } while (openFound.success && (styleAt == SCE_H_DOUBLESTRING || styleAt == SCE_H_SINGLESTRING) && searchStartPoint > 0);

    if (openFound.success && styleAt != SCE_H_CDATA)
    {
        // Found the "<" before the caret, now check there isn't a > between that position and the caret.
        FindResult closeFound{};
        searchStartPoint = openFound.start;
        do
        {
            closeFound       = FindText(">", searchStartPoint, caret, 0);
            styleAt          = Call(SCI_GETSTYLEAT, closeFound.start);
            searchStartPoint = closeFound.end;
        } while (closeFound.success && (styleAt == SCE_H_DOUBLESTRING || styleAt == SCE_H_SINGLESTRING) && searchStartPoint <= caret);

        if (!closeFound.success)
        {
            // We're in a tag (either a start tag or an end tag)
            auto nextChar = Call(SCI_GETCHARAT, openFound.start + 1);

            /////////////////////////////////////////////////////////////////////////
            // CURSOR IN CLOSE TAG
            /////////////////////////////////////////////////////////////////////////
            if ('/' == nextChar)
            {
                xmlTags.tagCloseStart = openFound.start;
                auto docLength        = Call(SCI_GETLENGTH);
                auto endCloseTag      = FindText(">", caret, docLength, 0);
                if (endCloseTag.success)
                {
                    xmlTags.tagCloseEnd = endCloseTag.end;
                }
                // Now find the tagName
                auto position = openFound.start + 2;

                // UTF-8 or ASCII tag name
                std::string tagName;
                nextChar = Call(SCI_GETCHARAT, position);
                // Checking for " or ' is actually wrong here, but it means it works better with invalid XML
                while (position < docLength && !IsXMLWhitespace(nextChar) && nextChar != '/' && nextChar != '>' && nextChar != '\"' && nextChar != '\'')
                {
                    tagName.push_back(static_cast<char>(nextChar));
                    ++position;
                    nextChar = Call(SCI_GETCHARAT, position);
                }

                // Now we know where the end of the tag is, and we know what the tag is called
                if (!tagName.empty())
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
                    auto       currentEndPoint   = xmlTags.tagCloseStart;
                    auto       openTagsRemaining = 1;
                    FindResult nextOpenTag{};
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
                            FindResult inBetweenCloseTag{};
                            auto       currentStartPosition = nextOpenTag.end;
                            sptr_t     closeTagsFound       = 0;
                            bool       forwardSearch        = (currentStartPosition < currentEndPoint);

                            do
                            {
                                inBetweenCloseTag = FindCloseTag(tagName, currentStartPosition, currentEndPoint);

                                if (inBetweenCloseTag.success)
                                {
                                    ++closeTagsFound;
                                    if (forwardSearch)
                                    {
                                        currentStartPosition = inBetweenCloseTag.end;
                                    }
                                    else
                                    {
                                        currentStartPosition = inBetweenCloseTag.start - 1;
                                    }
                                }

                            } while (inBetweenCloseTag.success);

                            // If we didn't find any close tags between the open and our close,
                            // and there's no open tags remaining to find
                            // then the open we found was the right one, and we can return it
                            if (0 == closeTagsFound && 0 == openTagsRemaining)
                            {
                                xmlTags.tagOpenStart = nextOpenTag.start;
                                xmlTags.tagOpenEnd   = nextOpenTag.end + 1;
                                xmlTags.tagNameEnd   = nextOpenTag.start + static_cast<sptr_t>(tagName.size()) + 1; /* + 1 to account for '<' */
                                tagFound             = true;
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
                auto position  = openFound.start + 1;
                auto docLength = Call(SCI_GETLENGTH);

                xmlTags.tagOpenStart = openFound.start;

                std::string tagName;
                nextChar = Call(SCI_GETCHARAT, position);
                // Checking for " or ' is actually wrong here, but it means it works better with invalid XML
                while (position < docLength && !IsXMLWhitespace(nextChar) && nextChar != '/' && nextChar != '>' && nextChar != '\"' && nextChar != '\'')
                {
                    tagName.push_back(static_cast<char>(nextChar));
                    ++position;
                    nextChar = Call(SCI_GETCHARAT, position);
                }

                // Now we know where the end of the tag is, and we know what the tag is called
                if (!tagName.empty())
                {
                    // First we need to check if this is a self-closing tag.
                    // If it is, then we can just return this tag to highlight itself.
                    xmlTags.tagNameEnd      = openFound.start + tagName.size() + 1;
                    auto closeAnglePosition = FindCloseAngle(position, docLength);
                    if (-1 != closeAnglePosition)
                    {
                        xmlTags.tagOpenEnd = closeAnglePosition + 1;
                        // If it's a self closing tag
                        if (Call(SCI_GETCHARAT, closeAnglePosition - 1) == '/')
                        {
                            // Set it as found, and mark that there's no close tag
                            xmlTags.tagCloseEnd   = -1;
                            xmlTags.tagCloseStart = -1;
                            tagFound              = true;
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
                            auto       currentStartPosition = xmlTags.tagOpenEnd;
                            auto       closeTagsRemaining   = 1;
                            FindResult nextCloseTag{};
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
                                    FindResult inBetweenOpenTag{};
                                    auto       currentEndPosition = nextCloseTag.start;
                                    sptr_t     openTagsFound      = 0;

                                    do
                                    {
                                        inBetweenOpenTag = FindOpenTag(tagName, currentStartPosition, currentEndPosition);

                                        if (inBetweenOpenTag.success)
                                        {
                                            ++openTagsFound;
                                            currentStartPosition = inBetweenOpenTag.end;
                                        }

                                    } while (inBetweenOpenTag.success);

                                    // If we didn't find any open tags between our open and the close,
                                    // and there's no close tags remaining to find
                                    // then the close we found was the right one, and we can return it
                                    if (0 == openTagsFound && 0 == closeTagsRemaining)
                                    {
                                        xmlTags.tagCloseStart = nextCloseTag.start;
                                        xmlTags.tagCloseEnd   = nextCloseTag.end + 1;
                                        tagFound              = true;
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
                    }     // end if (-1 != closeAngle)  {

                } // end if !tagName.empty()
            }     // end open tag test
        }
    }
    return tagFound;
}

FindResult CScintillaWnd::FindText(const char* text, sptr_t start, sptr_t end, int flags) const
{
    FindResult returnValue = {0};

    Sci_TextToFind search{};
    search.lpstrText  = const_cast<char*>(text);
    search.chrg.cpMin = start;
    search.chrg.cpMax = end;
    auto result       = Call(SCI_FINDTEXT, flags, reinterpret_cast<LPARAM>(&search));
    if (-1 == result)
    {
        returnValue.success = false;
    }
    else
    {
        returnValue.success = true;
        returnValue.start   = search.chrgText.cpMin;
        returnValue.end     = search.chrgText.cpMax;
    }
    return returnValue;
}

sptr_t CScintillaWnd::FindText(const std::string& toFind, sptr_t startPos, sptr_t endPos) const
{
    Sci_TextToFind ttf = {0};
    ttf.chrg.cpMin     = startPos;
    ttf.chrg.cpMax     = endPos;
    ttf.lpstrText      = const_cast<char*>(toFind.c_str());
    return Call(SCI_FINDTEXT, 0, reinterpret_cast<sptr_t>(&ttf));
}

FindResult CScintillaWnd::FindOpenTag(const std::string& tagName, sptr_t start, sptr_t end) const
{
    std::string search("<");
    search.append(tagName);
    FindResult openTagFound = {0};
    openTagFound.success    = false;
    FindResult result{};
    int        nextChar      = 0;
    sptr_t     styleAt       = 0;
    auto       searchStart   = start;
    auto       searchEnd     = end;
    bool       forwardSearch = (start < end);
    do
    {
        result = FindText(search.c_str(), searchStart, searchEnd, 0);
        if (result.success)
        {
            nextChar = static_cast<int>(Call(SCI_GETCHARAT, result.end));
            styleAt  = Call(SCI_GETSTYLEAT, result.start);
            if (styleAt != SCE_H_CDATA && styleAt != SCE_H_DOUBLESTRING && styleAt != SCE_H_SINGLESTRING)
            {
                // We've got an open tag for this tag name (i.e. nextChar was space or '>')
                // Now we need to find the end of the start tag.

                // Common case, the tag is an empty tag with no whitespace. e.g. <TAGNAME>
                if (nextChar == '>')
                {
                    openTagFound.end     = result.end;
                    openTagFound.success = true;
                }
                else if (IsXMLWhitespace(nextChar))
                {
                    auto closeAnglePosition = FindCloseAngle(result.end, forwardSearch ? end : start);
                    if (-1 != closeAnglePosition && '/' != Call(SCI_GETCHARAT, closeAnglePosition - 1))
                    {
                        openTagFound.end     = closeAnglePosition;
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

sptr_t CScintillaWnd::FindCloseAngle(sptr_t startPosition, sptr_t endPosition) const
{
    // We'll search for the next '>', and check it's not in an attribute using the style
    FindResult closeAngle{};

    bool   isValidClose   = false;
    sptr_t returnPosition = -1;

    // Only search forwards
    if (startPosition > endPosition)
    {
        auto temp     = endPosition;
        endPosition   = startPosition;
        startPosition = temp;
    }

    do
    {
        isValidClose = false;

        closeAngle = FindText(">", startPosition, endPosition, 0);
        if (closeAngle.success)
        {
            int style = static_cast<int>(Call(SCI_GETSTYLEAT, closeAngle.start));
            // As long as we're not in an attribute (  <TAGNAME attrib="val>ue"> is VALID XML. )
            if (style != SCE_H_DOUBLESTRING && style != SCE_H_SINGLESTRING)
            {
                returnPosition = closeAngle.start;
                isValidClose   = true;
            }
            else
            {
                startPosition = closeAngle.end;
            }
        }

    } while (closeAngle.success && isValidClose == false);

    return returnPosition;
}

FindResult CScintillaWnd::FindCloseTag(const std::string& tagName, sptr_t start, sptr_t end) const
{
    std::string search("</");
    search.append(tagName);
    FindResult closeTagFound = {0};
    closeTagFound.success    = false;
    FindResult result{};
    int        nextChar      = 0;
    sptr_t     styleAt       = 0;
    auto       searchStart   = start;
    auto       searchEnd     = end;
    bool       forwardSearch = (start < end);
    bool       validCloseTag = false;
    do
    {
        validCloseTag = false;
        result        = FindText(search.c_str(), searchStart, searchEnd, 0);
        if (result.success)
        {
            nextChar = static_cast<int>(Call(SCI_GETCHARAT, result.end));
            styleAt  = Call(SCI_GETSTYLEAT, result.start);

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
                    validCloseTag         = true;
                    closeTagFound.start   = result.start;
                    closeTagFound.end     = result.end;
                    closeTagFound.success = true;
                }
                else if (IsXMLWhitespace(nextChar)) // Otherwise, if it's whitespace, then allow whitespace until a '>' - any other character is invalid.
                {
                    auto whitespacePoint = result.end;
                    do
                    {
                        ++whitespacePoint;
                        nextChar = static_cast<int>(Call(SCI_GETCHARAT, whitespacePoint));

                    } while (IsXMLWhitespace(nextChar));

                    if (nextChar == '>')
                    {
                        validCloseTag         = true;
                        closeTagFound.start   = result.start;
                        closeTagFound.end     = whitespacePoint;
                        closeTagFound.success = true;
                    }
                }
            }
        }

    } while (result.success && !validCloseTag);

    return closeTagFound;
}

std::vector<std::pair<sptr_t, sptr_t>> CScintillaWnd::GetAttributesPos(sptr_t start, sptr_t end) const
{
    std::vector<std::pair<sptr_t, sptr_t>> attributes;

    auto          bufLen = end - start + 1;
    auto          buf    = std::make_unique<char[]>(bufLen + 1);
    Sci_TextRange tr{};
    tr.chrg.cpMin = start;
    tr.chrg.cpMax = end;
    tr.lpstrText  = buf.get();
    Call(SCI_GETTEXTRANGE, 0, reinterpret_cast<LPARAM>(&tr));

    enum class AttrStates
    {
        Attr_Invalid,
        Attr_Key,
        Attr_Pre_Assign,
        Attr_Assign,
        Attr_String,
        Attr_Value,
        Attr_Valid
    } state = AttrStates::Attr_Invalid;

    sptr_t startPos    = -1;
    sptr_t oneMoreChar = 1;
    sptr_t i           = 0;
    for (; i < bufLen; i++)
    {
        switch (buf[i])
        {
            case ' ':
            case '\t':
            case '\n':
            case '\r':
            {
                if (state == AttrStates::Attr_Key)
                    state = AttrStates::Attr_Pre_Assign;
                else if (state == AttrStates::Attr_Value)
                {
                    state       = AttrStates::Attr_Valid;
                    oneMoreChar = 0;
                }
            }
            break;

            case '=':
            {
                if (state == AttrStates::Attr_Key || state == AttrStates::Attr_Pre_Assign)
                    state = AttrStates::Attr_Assign;
                else if (state == AttrStates::Attr_Assign || state == AttrStates::Attr_Value)
                    state = AttrStates::Attr_Invalid;
            }
            break;

            case '"':
            {
                if (state == AttrStates::Attr_String)
                {
                    state       = AttrStates::Attr_Valid;
                    oneMoreChar = 1;
                }
                else if (state == AttrStates::Attr_Key || state == AttrStates::Attr_Pre_Assign || state == AttrStates::Attr_Value)
                    state = AttrStates::Attr_Invalid;
                else if (state == AttrStates::Attr_Assign)
                    state = AttrStates::Attr_String;
            }
            break;

            default:
            {
                if (state == AttrStates::Attr_Invalid)
                {
                    state    = AttrStates::Attr_Key;
                    startPos = i;
                }
                else if (state == AttrStates::Attr_Pre_Assign)
                    state = AttrStates::Attr_Invalid;
                else if (state == AttrStates::Attr_Assign)
                    state = AttrStates::Attr_Value;
            }
        }

        if (state == AttrStates::Attr_Valid)
        {
            attributes.push_back(std::pair<sptr_t, sptr_t>(start + startPos, start + i + oneMoreChar));
            state = AttrStates::Attr_Invalid;
        }
    }
    if (state == AttrStates::Attr_Value)
        attributes.push_back(std::pair<sptr_t, sptr_t>(start + startPos, start + i - 1));

    return attributes;
}

bool CScintillaWnd::AutoBraces(WPARAM wParam) const
{
    auto lexer = Call(SCI_GETLEXER);
    switch (lexer)
    {
        case SCLEX_CONTAINER:
        case SCLEX_NULL:
        case SCLEX_PROPERTIES:
        case SCLEX_ERRORLIST:
        case SCLEX_MARKDOWN:
        case SCLEX_TXT2TAGS:
            return false;
        default:
            break;
    }
    if ((wParam == '(') ||
        (wParam == '{') ||
        (wParam == '[') ||
        (wParam == '-'))
    {
        if (CIniSettings::Instance().GetInt64(L"View", L"autobrace", 1) == 0)
            return false;
        char braceBuf[2]      = {0};
        braceBuf[0]           = static_cast<char>(wParam);
        char braceCloseBuf[2] = {0};
        switch (wParam)
        {
            case '(':
                braceCloseBuf[0] = ')';
                break;
            case '{':
                braceCloseBuf[0] = '}';
                break;
            case '[':
                braceCloseBuf[0] = ']';
                break;
        }

        // Get Selection
        bool   bSelEmpty      = !!Call(SCI_GETSELECTIONEMPTY);
        sptr_t lineStartStart = 0;
        sptr_t lineEndEnd     = 0;
        if (!bSelEmpty && braceCloseBuf[0])
        {
            auto selStart  = Call(SCI_GETSELECTIONSTART);
            auto selEnd    = Call(SCI_GETSELECTIONEND);
            auto lineStart = Call(SCI_LINEFROMPOSITION, selStart);
            auto lineEnd   = Call(SCI_LINEFROMPOSITION, selEnd);
            if (Call(SCI_POSITIONFROMLINE, lineEnd) == selEnd)
            {
                --lineEnd;
                selEnd = Call(SCI_GETLINEENDPOSITION, lineEnd);
            }
            lineStartStart = Call(SCI_POSITIONFROMLINE, lineStart);
            lineEndEnd     = Call(SCI_GETLINEENDPOSITION, lineEnd);
            if ((lineStartStart != selStart) || (lineEndEnd != selEnd) || (wParam == '(') || (wParam == '['))
            {
                // insert the brace before the selection and the closing brace after the selection
                Call(SCI_SETSEL, static_cast<uptr_t>(-1), selStart);
                Call(SCI_BEGINUNDOACTION);
                Call(SCI_INSERTTEXT, selStart, reinterpret_cast<sptr_t>(braceBuf));
                Call(SCI_INSERTTEXT, selEnd + 1, reinterpret_cast<sptr_t>(braceCloseBuf));
                Call(SCI_SETSEL, selStart + 1, selStart + 1);
                Call(SCI_ENDUNDOACTION);
                return true;
            }
            else
            {
                // get info
                auto tabIndent         = Call(SCI_GETTABWIDTH);
                auto indentAmount      = Call(SCI_GETLINEINDENTATION, lineStart > 0 ? lineStart - 1 : lineStart);
                auto indentAmountFirst = Call(SCI_GETLINEINDENTATION, lineStart);
                if (indentAmount == 0)
                    indentAmount = indentAmountFirst;
                Call(SCI_BEGINUNDOACTION);

                // insert a new line at the end of the selected lines
                Call(SCI_SETSEL, lineEndEnd, lineEndEnd);
                Call(SCI_NEWLINE);
                // now insert the end-brace and indent it
                Call(SCI_INSERTTEXT, static_cast<uptr_t>(-1), reinterpret_cast<sptr_t>(braceCloseBuf));
                Call(SCI_SETLINEINDENTATION, lineEnd + 1, indentAmount);

                Call(SCI_SETSEL, lineStartStart, lineStartStart);
                // now insert the start-brace and a newline after it
                Call(SCI_INSERTTEXT, static_cast<uptr_t>(-1), reinterpret_cast<sptr_t>(braceBuf));
                Call(SCI_SETSEL, lineStartStart + 1, lineStartStart + 1);
                Call(SCI_NEWLINE);
                // now indent the line with the start brace
                Call(SCI_SETLINEINDENTATION, lineStart, indentAmount);

                // increase the indentation of all selected lines
                if (indentAmount == indentAmountFirst)
                {
                    for (sptr_t line = lineStart + 1; line <= lineEnd + 1; ++line)
                    {
                        Call(SCI_SETLINEINDENTATION, line, Call(SCI_GETLINEINDENTATION, line) + tabIndent);
                    }
                }
                Call(SCI_ENDUNDOACTION);
                return true;
            }
        }
        else
        {
            if (wParam == '{')
            {
                // Auto-add the closing brace, then the closing brace
                Call(SCI_BEGINUNDOACTION);
                // insert the opening brace first
                Call(SCI_ADDTEXT, 1, reinterpret_cast<sptr_t>(braceBuf));
                // insert the closing brace
                Call(SCI_ADDTEXT, 1, reinterpret_cast<sptr_t>(braceCloseBuf));
                // go back one char
                Call(SCI_CHARLEFT);
                Call(SCI_ENDUNDOACTION);
                return true;
            }
        }
    }
    return false;
}

void CScintillaWnd::ReflectEvents(SCNotification* pScn)
{
    switch (pScn->nmhdr.code)
    {
        case SCN_PAINTED:
            if (m_lineToScrollToAfterPaint != -1)
            {
                auto visLineToScrollTo = Call(SCI_VISIBLEFROMDOCLINE, m_lineToScrollToAfterPaint);
                visLineToScrollTo += m_wrapOffsetToScrollToAfterPaint;
                Call(SCI_SETFIRSTVISIBLELINE, visLineToScrollTo);
                --m_lineToScrollToAfterPaintCounter;
                if ((m_lineToScrollToAfterPaintCounter <= 0) || ((Call(SCI_GETFIRSTVISIBLELINE) + 2) > Call(SCI_VISIBLEFROMDOCLINE, m_lineToScrollToAfterPaint)))
                {
                    m_lineToScrollToAfterPaint        = -1;
                    m_wrapOffsetToScrollToAfterPaint  = 0;
                    m_lineToScrollToAfterPaintCounter = 0;
                }
                UpdateLineNumberWidth();
            }
            break;
    }
}

void CScintillaWnd::SetTabSettings(TabSpace ts) const
{
    Call(SCI_SETTABWIDTH, static_cast<uptr_t>(CIniSettings::Instance().GetInt64(L"View", L"tabsize", 4)));
    if (ts == TabSpace::Default)
        Call(SCI_SETUSETABS, static_cast<uptr_t>(CIniSettings::Instance().GetInt64(L"View", L"usetabs", 1)));
    else
        Call(SCI_SETUSETABS, ts == TabSpace::Tabs ? 1 : 0);
    Call(SCI_SETBACKSPACEUNINDENTS, 1);
    Call(SCI_SETTABINDENTS, 1);
    Call(SCI_SETTABDRAWMODE, 1);
}

void CScintillaWnd::SetReadDirection(ReadDirection rd) const
{
    //auto ex = GetWindowLongPtr(*this, GWL_EXSTYLE);
    //if (rd != R2L)
    //    ex &= WS_EX_LAYOUTRTL/*WS_EX_RTLREADING*/;
    //else
    //    ex |= WS_EX_LAYOUTRTL/*WS_EX_RTLREADING*/;
    //SetWindowLongPtr(*this, GWL_EXSTYLE, ex);
    Call(SCI_SETBIDIRECTIONAL, static_cast<int>(rd));
}

void CScintillaWnd::BookmarkAdd(sptr_t lineNo)
{
    if (lineNo == -1)
        lineNo = GetCurrentLineNumber();
    if (!IsBookmarkPresent(lineNo))
    {
        Call(SCI_MARKERADD, lineNo, MARK_BOOKMARK);
        m_docScroll.AddLineColor(DOCSCROLLTYPE_BOOKMARK, lineNo, CTheme::Instance().GetThemeColor(RGB(255, 0, 0), true));
        DocScrollUpdate();
    }
}

void CScintillaWnd::BookmarkDelete(sptr_t lineNo)
{
    if (lineNo == -1)
        lineNo = GetCurrentLineNumber();
    if (IsBookmarkPresent(lineNo))
    {
        Call(SCI_MARKERDELETE, lineNo, MARK_BOOKMARK);
        m_docScroll.RemoveLine(DOCSCROLLTYPE_BOOKMARK, lineNo);
        DocScrollUpdate();
    }
}

bool CScintillaWnd::IsBookmarkPresent(sptr_t lineNo) const
{
    if (lineNo == -1)
        lineNo = GetCurrentLineNumber();
    LRESULT state = Call(SCI_MARKERGET, lineNo);
    return ((state & (1 << MARK_BOOKMARK)) != 0);
}

void CScintillaWnd::BookmarkToggle(sptr_t lineNo)
{
    if (lineNo == -1)
        lineNo = GetCurrentLineNumber();

    if (IsBookmarkPresent(lineNo))
        BookmarkDelete(lineNo);
    else
        BookmarkAdd(lineNo);
}

void CScintillaWnd::MarkBookmarksInScrollbar()
{
    const auto bmColor = CTheme::Instance().GetThemeColor(RGB(255, 0, 0), true);
    m_docScroll.Clear(DOCSCROLLTYPE_BOOKMARK);
    for (sptr_t line = -1;;)
    {
        line = Call(SCI_MARKERNEXT, line + 1, (1 << MARK_BOOKMARK));
        if (line < 0)
            break;
        m_docScroll.AddLineColor(DOCSCROLLTYPE_BOOKMARK, line, bmColor);
    }
    DocScrollUpdate();
}

void CScintillaWnd::DocScrollUpdate()
{
    InvalidateRect(*this, nullptr, TRUE);
    SCNotification scn = {nullptr};
    scn.message        = SCN_UPDATEUI;
    scn.updated        = SC_UPDATE_CONTENT;
    scn.nmhdr.code     = SCN_UPDATEUI;
    scn.nmhdr.hwndFrom = *this;
    scn.nmhdr.idFrom   = reinterpret_cast<uptr_t>(this);
    SendMessage(GetParent(*this), WM_NOTIFY, reinterpret_cast<WPARAM>(this), reinterpret_cast<LPARAM>(&scn));

    // force the scrollbar to redraw

    bool ok = SetWindowPos(*this, nullptr, 0, 0, 0, 0,
                           SWP_FRAMECHANGED | // NO to everything else
                               SWP_NOZORDER | SWP_NOMOVE | SWP_NOSIZE | SWP_NOOWNERZORDER |
                               SWP_NOACTIVATE | SWP_NOSENDCHANGING) != FALSE;
    APPVERIFY(ok);
}

void CScintillaWnd::SetEOLType(int eolType) const
{
    Call(SCI_SETEOLMODE, eolType);
}

void CScintillaWnd::AppendText(sptr_t len, const char* buf) const
{
    Call(SCI_APPENDTEXT, len, reinterpret_cast<LPARAM>(buf));
}

std::string CScintillaWnd::GetLine(sptr_t line) const
{
    auto lineSize = ConstCall(SCI_GETLINE, line, 0);
    auto pLine    = std::make_unique<char[]>(lineSize + 1);
    ConstCall(SCI_GETLINE, line, reinterpret_cast<sptr_t>(pLine.get()));
    pLine[lineSize] = 0;
    return pLine.get();
}

std::string CScintillaWnd::GetTextRange(Sci_Position startPos, Sci_Position endPos) const
{
    assert(endPos - startPos >= 0);
    if (endPos < startPos)
        return "";
    auto          strBuf = std::make_unique<char[]>(endPos - startPos + 5);
    Sci_TextRange rangeStart{};
    rangeStart.chrg.cpMin = static_cast<Sci_PositionCR>(startPos);
    rangeStart.chrg.cpMax = static_cast<Sci_PositionCR>(endPos);
    rangeStart.lpstrText  = strBuf.get();
    ConstCall(SCI_GETTEXTRANGE, 0, reinterpret_cast<sptr_t>(&rangeStart));
    return strBuf.get();
}

std::string CScintillaWnd::GetSelectedText(bool useCurrentWordIfSelectionEmpty) const
{
    auto selTextLen    = ConstCall(SCI_GETSELTEXT);
    auto selTextBuffer = std::make_unique<char[]>(selTextLen + 1);
    ConstCall(SCI_GETSELTEXT, 0, reinterpret_cast<LPARAM>(selTextBuffer.get()));
    std::string selText = selTextBuffer.get();
    if (selText.empty() && useCurrentWordIfSelectionEmpty)
    {
        selText = GetCurrentWord();
    }
    return selText;
}

std::string CScintillaWnd::GetCurrentWord() const
{
    auto currentPos = ConstCall(SCI_GETCURRENTPOS);
    auto startPos   = ConstCall(SCI_WORDSTARTPOSITION, currentPos, true);
    auto endPos     = ConstCall(SCI_WORDENDPOSITION, currentPos, true);
    return GetTextRange(startPos, endPos);
}

std::string CScintillaWnd::GetCurrentLine() const
{
    auto lineLen    = ConstCall(SCI_GETCURLINE);
    auto lineBuffer = std::make_unique<char[]>(lineLen + 1);
    ConstCall(SCI_GETCURLINE, lineLen + 1, reinterpret_cast<LPARAM>(lineBuffer.get()));
    return lineBuffer.get();
}

std::string CScintillaWnd::GetWordChars() const
{
    auto len        = ConstCall(SCI_GETWORDCHARS);
    auto lineBuffer = std::make_unique<char[]>(len + 1);
    ConstCall(SCI_GETWORDCHARS, 0, reinterpret_cast<LPARAM>(lineBuffer.get()));
    lineBuffer[len] = '\0';
    return lineBuffer.get();
}

std::string CScintillaWnd::GetWhitespaceChars() const
{
    auto len        = ConstCall(SCI_GETWHITESPACECHARS);
    auto lineBuffer = std::make_unique<char[]>(len + 1);
    ConstCall(SCI_GETWHITESPACECHARS, 0, reinterpret_cast<LPARAM>(lineBuffer.get()));
    return lineBuffer.get();
}

sptr_t CScintillaWnd::GetCurrentLineNumber() const
{
    return ConstCall(SCI_LINEFROMPOSITION, ConstCall(SCI_GETCURRENTPOS));
}
