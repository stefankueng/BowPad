// This file is part of BowPad.
//
// Copyright (C) 2013-2019 - Stefan Kueng
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
#include "UnicodeUtils.h"
#include "StringUtils.h"
#include "AppUtils.h"
#include "Theme.h"
#include "SciLexer.h"
#include "Document.h"
#include "LexStyles.h"
#include "DocScroll.h"
#include "CommandHandler.h"
#include "SmartHandle.h"
#include "DPIAware.h"

#include <UIRibbon.h>
#include <UIRibbonPropertyHelpers.h>
#include <uxtheme.h>
#include <chrono>

extern IUIFramework * g_pFramework;
extern std::string    g_sHighlightString;       // from CmdFindReplace
extern int            g_searchMarkerCount;      // from CmdFindReplace

namespace
{
    typedef BOOL(WINAPI * GetGestureInfoFN)(HGESTUREINFO hGestureInfo, PGESTUREINFO pGestureInfo);
    GetGestureInfoFN GetGestureInfoFunc = nullptr;
    typedef BOOL(WINAPI * CloseGestureInfoHandleFN)(HGESTUREINFO hGestureInfo);
    CloseGestureInfoHandleFN CloseGestureInfoHandleFunc = nullptr;
    typedef BOOL(WINAPI * SetGestureConfigFN)(HWND hwnd, DWORD dwReserved, UINT cIDs, PGESTURECONFIG pGestureConfig, UINT cbSize);
    SetGestureConfigFN SetGestureConfigFunc = nullptr;

    typedef BOOL(WINAPI * BeginPanningFeedbackFN)(HWND hwnd);
    BeginPanningFeedbackFN BeginPanningFeedbackFunc = nullptr;
    typedef BOOL(WINAPI * EndPanningFeedbackFN)(HWND hwnd, BOOL fAnimateBack);
    EndPanningFeedbackFN EndPanningFeedbackFunc = nullptr;
    typedef BOOL(WINAPI * UpdatePanningFeedbackFN)(HWND hwnd, LONG lTotalOverpanOffsetX, LONG lTotalOverpanOffsetY, BOOL fInInertia);
    UpdatePanningFeedbackFN UpdatePanningFeedbackFunc = nullptr;

    CAutoLibrary hUxThemeDll = LoadLibrary(L"uxtheme.dll");
}

UINT32 g_contextID = cmdContextMap;

const int TIM_HIDECURSOR = 101;
const int TIM_BRACEHIGHLIGHTTEXT = 102;
const int TIM_BRACEHIGHLIGHTTEXTCLEAR = 103;

static bool g_initialized = false;
static bool g_scintillaInitialized = false;

const int color_folding_fore_inactive = 255;
const int color_folding_back_inactive = 220;
const int color_folding_backsel_inactive = 150;
const int color_folding_fore_active = 250;
const int color_folding_back_active = 100;
const int color_folding_backsel_active = 20;
const int color_linenr_inactive = 109;
const int color_linenr_active = 60;
const double folding_color_animation_time = 0.3;

CScintillaWnd::CScintillaWnd(HINSTANCE hInst)
    : CWindow(hInst)
    , m_pSciMsg(nullptr)
    , m_pSciWndData(0)
    , m_selTextMarkerCount(0)
    , m_bCursorShown(true)
    , m_bScratch(false)
    , m_eraseBkgnd(true)
    , m_cursorTimeout(-1)
    , m_ScrollTool(hInst)
    , m_hasConsolas(false)
    , m_bInFolderMargin(false)
    , m_LineToScrollToAfterPaint((size_t)-1)
    , m_WrapOffsetToScrollToAfterPaint(0)
{
    HFONT hFont = CreateFont(0, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                             OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, FIXED_PITCH, L"Consolas");
    if (hFont)
    {
        DeleteObject(hFont);
        m_hasConsolas = true;
    }

    m_animVarGrayFore = Animator::Instance().CreateAnimationVariable(color_folding_fore_inactive);
    m_animVarGrayBack = Animator::Instance().CreateAnimationVariable(color_folding_back_inactive);
    m_animVarGraySel = Animator::Instance().CreateAnimationVariable(color_folding_backsel_inactive);
    m_animVarGrayLineNr = Animator::Instance().CreateAnimationVariable(color_linenr_inactive);
}

CScintillaWnd::~CScintillaWnd()
{
    if (m_bScratch)
    {
        DestroyWindow(*this);
        m_hwnd = nullptr;
    }
}

bool CScintillaWnd::Init(HINSTANCE hInst, HWND hParent)
{
    if (!g_scintillaInitialized)
    {
        Scintilla_RegisterClasses(hInst);
        INITCOMMONCONTROLSEX icce;
        icce.dwSize = sizeof(icce);
        icce.dwICC = ICC_BAR_CLASSES;
        InitCommonControlsEx(&icce);
    }

    if (hParent)
        CreateEx(0, WS_CHILD | WS_VISIBLE, hParent, 0, L"Scintilla");
    else
        CreateEx(0, 0, 0, 0, L"Scintilla");

    if (!*this)
    {
        return false;
    }

    m_pSciMsg = (SciFnDirect)SendMessage(*this, SCI_GETDIRECTFUNCTION, 0, 0);
    m_pSciWndData = (sptr_t)SendMessage(*this, SCI_GETDIRECTPOINTER, 0, 0);

    if (m_pSciMsg==nullptr || m_pSciWndData==0)
        return false;

    m_docScroll.InitScintilla(this);

    Call(SCI_SETMODEVENTMASK, SC_MOD_INSERTTEXT | SC_MOD_DELETETEXT | SC_PERFORMED_UNDO |
        SC_PERFORMED_REDO | SC_MULTISTEPUNDOREDO | SC_LASTSTEPINUNDOREDO |
        SC_MOD_BEFOREINSERT | SC_MOD_BEFOREDELETE | SC_MULTILINEUNDOREDO);
    bool bUseD2D = CIniSettings::Instance().GetInt64(L"View", L"d2d", 1) != 0;
    Call(SCI_SETTECHNOLOGY, bUseD2D ? SC_TECHNOLOGY_DIRECTWRITERETAIN : SC_TECHNOLOGY_DEFAULT);

    Call(SCI_SETMARGINMASKN, SC_MARGE_FOLDER, SC_MASK_FOLDERS);
    Call(SCI_SETMARGINWIDTHN, SC_MARGE_FOLDER, CDPIAware::Instance().Scale(14));
    Call(SCI_SETMARGINCURSORN, SC_MARGE_FOLDER, SC_CURSORARROW);
    Call(SCI_SETAUTOMATICFOLD, SC_AUTOMATICFOLD_SHOW|SC_AUTOMATICFOLD_CLICK|SC_AUTOMATICFOLD_CHANGE);

    Call(SCI_SETMARGINMASKN, SC_MARGE_SYMBOL, (1<<MARK_BOOKMARK));
    Call(SCI_SETMARGINWIDTHN, SC_MARGE_SYMBOL, CDPIAware::Instance().Scale(14));
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
        Call(SCI_SETCARETLINEFRAME, CDPIAware::Instance().Scale(1));
    Call(SCI_SETWHITESPACESIZE, CDPIAware::Instance().Scale(1));
    Call(SCI_SETMULTIPLESELECTION, 1);
    Call(SCI_SETMOUSESELECTIONRECTANGULARSWITCH, true);
    Call(SCI_SETADDITIONALSELECTIONTYPING, true);
    Call(SCI_SETADDITIONALCARETSBLINK, false);
    Call(SCI_SETMULTIPASTE, SC_MULTIPASTE_EACH);
    Call(SCI_SETVIRTUALSPACEOPTIONS, SCVS_RECTANGULARSELECTION);

    Call(SCI_SETMOUSEWHEELCAPTURES, 0);

    // For Ctrl+C, use SCI_COPYALLOWLINE instead of SCI_COPY
    Call(SCI_ASSIGNCMDKEY, 'C'+(SCMOD_CTRL<<16), SCI_COPYALLOWLINE);

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
    Call(SCI_SETPHASESDRAW,bUseD2D ? SC_PHASES_MULTIPLE : SC_PHASES_TWO);
    Call(SCI_SETLAYOUTCACHE, SC_CACHE_PAGE);

    Call(SCI_USEPOPUP, 0);  // no default context menu

    Call(SCI_SETMOUSEDWELLTIME, GetDoubleClickTime());
    Call(SCI_CALLTIPSETPOSITION, 1);

    Call(SCI_SETPASTECONVERTENDINGS, 1);

    Call(SCI_SETSELALPHA, 128);

    Call(SCI_SETCHARACTERCATEGORYOPTIMIZATION, 0x10000);
    SetupDefaultStyles();

    if (!g_initialized)
    {
        // delay loaded functions, not available on Vista
        HMODULE hDll = GetModuleHandle(TEXT("user32.dll"));
        if (hDll)
        {
            GetGestureInfoFunc = (GetGestureInfoFN)::GetProcAddress(hDll, "GetGestureInfo");
            CloseGestureInfoHandleFunc = (CloseGestureInfoHandleFN)::GetProcAddress(hDll, "CloseGestureInfoHandle");
            SetGestureConfigFunc = (SetGestureConfigFN)::GetProcAddress(hDll, "SetGestureConfig");
        }
        if (hUxThemeDll)
        {
            BeginPanningFeedbackFunc = (BeginPanningFeedbackFN)::GetProcAddress(hUxThemeDll, "BeginPanningFeedback");
            EndPanningFeedbackFunc = (EndPanningFeedbackFN)::GetProcAddress(hUxThemeDll, "EndPanningFeedback");
            UpdatePanningFeedbackFunc = (UpdatePanningFeedbackFN)::GetProcAddress(hUxThemeDll, "UpdatePanningFeedback");
        }
        g_initialized = true;
    }

    return true;
}


bool CScintillaWnd::InitScratch( HINSTANCE hInst )
{
    Scintilla_RegisterClasses(hInst);

    CreateEx(WS_EX_NOPARENTNOTIFY, 0, nullptr, nullptr, L"Scintilla");

    if (!*this)
        return false;

    m_pSciMsg = (SciFnDirect)SendMessage(*this, SCI_GETDIRECTFUNCTION, 0, 0);
    m_pSciWndData = (sptr_t)SendMessage(*this, SCI_GETDIRECTPOINTER, 0, 0);

    if (m_pSciMsg==nullptr || m_pSciWndData==0)
        return false;

    m_bScratch = true;

    return true;
}

bool bEatNextEnterKey = false;

LRESULT CALLBACK CScintillaWnd::WinMsgHandler( HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam )
{
    NMHDR *hdr = (NMHDR *)lParam;
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
                HDC hdc = (HDC)wParam;
                auto bgc = CTheme::Instance().GetThemeColor(::GetSysColor(COLOR_3DFACE));
                auto oldBgc = SetBkColor(hdc, bgc);
                RECT r;
                GetClientRect(hwnd, &r);
                ExtTextOut(hdc, r.left, r.top, ETO_CLIPPED | ETO_OPAQUE, &r, L"", 0, nullptr);
                SetBkColor(hdc, oldBgc);
            }
            return TRUE;
    }
        break;
    case WM_NOTIFY:
        if(hdr->code == NM_COOLSB_CUSTOMDRAW)
            return m_docScroll.HandleCustomDraw(wParam, (NMCSBCUSTOMDRAW *)lParam);
        break;
    case WM_CONTEXTMENU:
        {
            POINT pt;
            POINTSTOPOINT(pt, lParam);

            if (pt.x == -1 && pt.y == -1)
            {
                // Display the menu in the upper-left corner of the client area, below the ribbon.
                IUIRibbonPtr pRibbon;
                HRESULT hr = g_pFramework->GetView(0, IID_PPV_ARGS(&pRibbon));
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
                if (bEatNextEnterKey)
                {
                    bEatNextEnterKey = false;
                    return 0;
                }
                if ((GetKeyState(VK_CONTROL) & 0x8000) || (GetKeyState(VK_SHIFT) & 0x8000))
                {
                    return 0;
                }
            }
            if (AutoBraces(wParam))
                return 0;
        }
        break;
    case WM_KEYDOWN:
        {
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
                char c = (char)Call(SCI_GETCHARAT, Call(SCI_GETCURRENTPOS) - 1);
                char c1 = (char)Call(SCI_GETCHARAT, Call(SCI_GETCURRENTPOS));
                if ((c == '{') && (c1 == '}'))
                {
                    Call(SCI_NEWLINE);
                    Call(SCI_BEGINUNDOACTION);
                    Call(SCI_LINEUP);
                    Call(SCI_LINEEND);
                    Call(SCI_NEWLINE);
                    Call(SCI_TAB);
                    Call(SCI_ENDUNDOACTION);
                    bEatNextEnterKey = true;
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
            m_cursorTimeout = (int) CIniSettings::Instance().GetInt64(L"View", L"hidecursortimeout", 3000);
        UINT elapse = (UINT)m_cursorTimeout;
        if (elapse != 0)
            SetTimer(*this, TIM_HIDECURSOR, elapse, nullptr);
        if (!m_bCursorShown)
        {
            Call(SCI_SETCURSOR, (uptr_t)-1);
            m_bCursorShown = true;
        }
        if (uMsg == WM_VSCROLL)
        {
            switch (LOWORD(wParam))
            {
                case SB_THUMBPOSITION:
                case SB_ENDSCROLL:
                m_ScrollTool.Clear();
                break;
                case SB_THUMBTRACK:
                {
                    RECT thumbrect;
                    GetClientRect(*this, &thumbrect);
                    MapWindowPoints(*this, nullptr, (LPPOINT)&thumbrect, 2);

                    SCROLLINFO si = { 0 };
                    si.cbSize = sizeof(SCROLLINFO);
                    si.fMask = SIF_ALL;
                    CoolSB_GetScrollInfo(*this, SB_VERT, &si);

                    auto linecount = Call(SCI_GETLINECOUNT);
                    auto w = m_ScrollTool.GetTextWidth(CStringUtils::Format(L"Line: %ld", linecount).c_str());
                    POINT thumbpoint;
                    thumbpoint.x = thumbrect.right - w;
                    thumbpoint.y = thumbrect.top + ((thumbrect.bottom - thumbrect.top)*si.nTrackPos) / (si.nMax - si.nMin);

                    m_ScrollTool.Init();
                    m_ScrollTool.SetText(&thumbpoint, L"Line: %ld", si.nTrackPos+1);
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
                rc.right += (long)Call(SCI_GETMARGINWIDTHN, i);
            POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            if (PtInRect(&rc, pt))
            {
                if (!m_bInFolderMargin)
                {
                    // animate the colors of the folder margin lines and symbols
                    auto transFore = Animator::Instance().CreateLinearTransition(folding_color_animation_time, color_folding_fore_active);
                    auto transBack = Animator::Instance().CreateLinearTransition(folding_color_animation_time, color_folding_back_active);
                    auto transSel = Animator::Instance().CreateLinearTransition(folding_color_animation_time, color_folding_backsel_active);
                    auto transNr = Animator::Instance().CreateLinearTransition(folding_color_animation_time, color_linenr_active);

                    auto storyBoard = Animator::Instance().CreateStoryBoard();
                    storyBoard->AddTransition(m_animVarGrayFore, transFore);
                    storyBoard->AddTransition(m_animVarGrayBack, transBack);
                    storyBoard->AddTransition(m_animVarGraySel, transSel);
                    storyBoard->AddTransition(m_animVarGrayLineNr, transNr);
                    Animator::Instance().RunStoryBoard(storyBoard, [this]()
                    {
                        auto gf = Animator::GetIntegerValue(m_animVarGrayFore);
                        auto gb = Animator::GetIntegerValue(m_animVarGrayBack);
                        auto sb = Animator::GetIntegerValue(m_animVarGraySel);
                        auto ln = Animator::GetIntegerValue(m_animVarGrayLineNr);
                        SetupFoldingColors(RGB(gf, gf, gf),
                                           RGB(gb, gb, gb),
                                           RGB(sb, sb, sb));
                        Call(SCI_STYLESETFORE, STYLE_LINENUMBER, CTheme::Instance().GetThemeColor(RGB(ln, ln, ln)));
                    });

                    m_bInFolderMargin = true;
                    TRACKMOUSEEVENT tme = { 0 };
                    tme.cbSize = sizeof(TRACKMOUSEEVENT);
                    tme.dwFlags = TME_LEAVE;
                    tme.hwndTrack = *this;
                    TrackMouseEvent(&tme);
                }
            }
            else
            {
                if (m_bInFolderMargin)
                {
                    // animate the colors of the folder margin lines and symbols
                    auto transFore = Animator::Instance().CreateLinearTransition(folding_color_animation_time, color_folding_fore_inactive);
                    auto transBack = Animator::Instance().CreateLinearTransition(folding_color_animation_time, color_folding_back_inactive);
                    auto transSel = Animator::Instance().CreateLinearTransition(folding_color_animation_time, color_folding_backsel_inactive);
                    auto transNr = Animator::Instance().CreateLinearTransition(folding_color_animation_time, color_linenr_inactive);

                    auto storyBoard = Animator::Instance().CreateStoryBoard();
                    storyBoard->AddTransition(m_animVarGrayFore, transFore);
                    storyBoard->AddTransition(m_animVarGrayBack, transBack);
                    storyBoard->AddTransition(m_animVarGraySel, transSel);
                    storyBoard->AddTransition(m_animVarGrayLineNr, transNr);
                    Animator::Instance().RunStoryBoard(storyBoard, [this]()
                    {
                        auto gf = Animator::GetIntegerValue(m_animVarGrayFore);
                        auto gb = Animator::GetIntegerValue(m_animVarGrayBack);
                        auto sb = Animator::GetIntegerValue(m_animVarGraySel);
                        auto ln = Animator::GetIntegerValue(m_animVarGrayLineNr);
                        SetupFoldingColors(RGB(gf, gf, gf),
                                           RGB(gb, gb, gb),
                                           RGB(sb, sb, sb));
                        Call(SCI_STYLESETFORE, STYLE_LINENUMBER, CTheme::Instance().GetThemeColor(RGB(ln, ln, ln)));
                    });
                    m_bInFolderMargin = false;
                }
            }
        }
    }
    break;
    case WM_MOUSELEAVE:
    {
        TRACKMOUSEEVENT tme = { 0 };
        tme.cbSize = sizeof(TRACKMOUSEEVENT);
        tme.dwFlags = TME_LEAVE | TME_CANCEL;
        tme.hwndTrack = *this;
        TrackMouseEvent(&tme);
        if (m_bInFolderMargin)
        {
            // animate the colors of the folder margin lines and symbols
            auto transFore = Animator::Instance().CreateLinearTransition(folding_color_animation_time, color_folding_fore_inactive);
            auto transBack = Animator::Instance().CreateLinearTransition(folding_color_animation_time, color_folding_back_inactive);
            auto transSel = Animator::Instance().CreateLinearTransition(folding_color_animation_time, color_folding_backsel_inactive);
            auto transNr = Animator::Instance().CreateLinearTransition(folding_color_animation_time, color_linenr_inactive);

            auto storyBoard = Animator::Instance().CreateStoryBoard();
            storyBoard->AddTransition(m_animVarGrayFore, transFore);
            storyBoard->AddTransition(m_animVarGrayBack, transBack);
            storyBoard->AddTransition(m_animVarGraySel, transSel);
            storyBoard->AddTransition(m_animVarGrayLineNr, transNr);
            Animator::Instance().RunStoryBoard(storyBoard, [this]()
            {
                auto gf = Animator::GetIntegerValue(m_animVarGrayFore);
                auto gb = Animator::GetIntegerValue(m_animVarGrayBack);
                auto sb = Animator::GetIntegerValue(m_animVarGraySel);
                auto ln = Animator::GetIntegerValue(m_animVarGrayLineNr);
                SetupFoldingColors(RGB(gf, gf, gf),
                                   RGB(gb, gb, gb),
                                   RGB(sb, sb, sb));
                Call(SCI_STYLESETFORE, STYLE_LINENUMBER, CTheme::Instance().GetThemeColor(RGB(ln, ln, ln)));
            });
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
                    POINT pt;
                    pt.x = GET_X_LPARAM(pos);
                    pt.y = GET_Y_LPARAM(pos);
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
                            SendMessage(GetParent(*this), WM_CANHIDECURSOR, WPARAM(0), LPARAM(&hide));
                            if (hide)
                            {
                                m_bCursorShown = false;
                                SetCursor(nullptr);
                                Call(SCI_SETCURSOR, (uptr_t)-2);
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
                Call(SCI_SETCURSOR, (uptr_t)-2);
            }

            auto cur = Call(SCI_GETCURSOR);
            if ((cur == 8) || (cur == 0))
                Call(SCI_SETCURSOR, (uptr_t)SC_CURSORNORMAL);
        }
        break;
    case WM_GESTURENOTIFY:
    {
        if (SetGestureConfigFunc)
        {
            DWORD panWant = GC_PAN | GC_PAN_WITH_INERTIA | GC_PAN_WITH_SINGLE_FINGER_VERTICALLY;
            GESTURECONFIG gestureConfig[] =
            {
                { GID_PAN, panWant, GC_PAN_WITH_GUTTER },
                { GID_TWOFINGERTAP, GC_TWOFINGERTAP, 0 },
            };
            SetGestureConfigFunc(*this, 0, _countof(gestureConfig), gestureConfig, sizeof(GESTURECONFIG));
        }
        return 0;
    }
    case WM_GESTURE:
    {
        static int scale = 8;   // altering the scale value will change how fast the page scrolls
        static int lastY = 0;   // used for panning calculations (initial / previous vertical position)
        static int lastX = 0;   // used for panning calculations (initial / previous horizontal position)
        static long xOverpan = 0;
        static long yOverpan = 0;

        if ((GetGestureInfoFunc == nullptr) || (CloseGestureInfoHandleFunc == nullptr) ||
            (BeginPanningFeedbackFunc == nullptr) || (EndPanningFeedbackFunc == nullptr) ||
            (UpdatePanningFeedbackFunc == nullptr))
            break;

        GESTUREINFO gi;
        ZeroMemory(&gi, sizeof(GESTUREINFO));
        gi.cbSize = sizeof(GESTUREINFO);
        BOOL bResult = GetGestureInfoFunc((HGESTUREINFO)lParam, &gi);

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

                    SCROLLINFO siv = { 0 };
                    siv.cbSize = sizeof(siv);
                    siv.fMask = SIF_ALL;
                    CoolSB_GetScrollInfo(*this, SB_VERT, &siv);
                    if (scrollY)
                    {
                        siv.nPos -= scrollY;
                        siv.fMask = SIF_POS;
                        CoolSB_SetScrollInfo(hwnd, SB_VERT, &siv, TRUE);
                        SendMessage(hwnd, WM_VSCROLL, MAKEWPARAM(SB_THUMBPOSITION, siv.nPos), 0L);
                        lastY = gi.ptsLocation.y;
                    }

                    SCROLLINFO sih = { 0 };
                    sih.cbSize = sizeof(sih);
                    sih.fMask = SIF_ALL;
                    CoolSB_GetScrollInfo(*this, SB_HORZ, &sih);
                    if (scrollX)
                    {
                        sih.nPos -= scrollX;
                        sih.fMask = SIF_POS;
                        CoolSB_SetScrollInfo(hwnd, SB_HORZ, &sih, TRUE);
                        SendMessage(hwnd, WM_HSCROLL, MAKEWPARAM(SB_THUMBPOSITION, sih.nPos), 0L);
                        lastX = gi.ptsLocation.x;
                    }

                    yOverpan -= lastY - gi.ptsLocation.y;
                    xOverpan -= lastX - gi.ptsLocation.x;

                    if (gi.dwFlags & GF_BEGIN)
                    {
                        BeginPanningFeedbackFunc(hwnd);
                        yOverpan = 0;
                        xOverpan = 0;
                    }
                    else if (gi.dwFlags & GF_END)
                    {
                        EndPanningFeedbackFunc(hwnd, TRUE);
                        yOverpan = 0;
                        xOverpan = 0;
                    }

                    if ((siv.nPos == siv.nMin) || (siv.nPos >= (siv.nMax - int(siv.nPage))))
                    {
                        // we reached the bottom / top, pan
                        UpdatePanningFeedbackFunc(hwnd, 0, yOverpan, gi.dwFlags & GF_INERTIA);
                    }
                    if ((sih.nPos == sih.nMin) || (sih.nPos >= (sih.nMax - int(sih.nPage))))
                    {
                        // we reached the bottom / top, pan
                        UpdatePanningFeedbackFunc(hwnd, xOverpan, 0, gi.dwFlags & GF_INERTIA);
                    }
                    UpdateWindow(hwnd);
                    CloseGestureInfoHandleFunc((HGESTUREINFO)lParam);
                    return 0;
                }
                    break;
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

void CScintillaWnd::UpdateLineNumberWidth()
{
    bool bShowLineNumbers = CIniSettings::Instance().GetInt64(L"View", L"linenumbers", 1) != 0;
    if (!bShowLineNumbers)
    {
        Call(SCI_SETMARGINWIDTHN, SC_MARGE_LINENUMBER, 0);
        return;
    }
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

void CScintillaWnd::SaveCurrentPos(CPosData& pos)
{
    // don't save the position if the window has never been
    // painted: if there's still a valid m_LineToScrollToAfterPaint
    // then the scroll position hasn't been set properly yet
    // and reading the positions would be wrong.
    if (m_LineToScrollToAfterPaint == (size_t)-1)
    {
        auto firstVisibleLine   = Call(SCI_GETFIRSTVISIBLELINE);
        pos.m_nFirstVisibleLine   = Call(SCI_DOCLINEFROMVISIBLE, firstVisibleLine);
        if (Call(SCI_GETWRAPMODE) != SC_WRAP_NONE)
            pos.m_nWrapLineOffset = firstVisibleLine - Call(SCI_VISIBLEFROMDOCLINE, pos.m_nFirstVisibleLine);

        pos.m_nStartPos           = Call(SCI_GETANCHOR);
        pos.m_nEndPos             = Call(SCI_GETCURRENTPOS);
        pos.m_xOffset             = Call(SCI_GETXOFFSET);
        pos.m_nSelMode            = Call(SCI_GETSELECTIONMODE);
        pos.m_nScrollWidth        = Call(SCI_GETSCROLLWIDTH);
    }
    else
    {
        m_LineToScrollToAfterPaint = (size_t)-1;
    }
}

void CScintillaWnd::RestoreCurrentPos(const CPosData& pos)
{
    Call(SCI_GOTOPOS, 0);

    Call(SCI_SETSELECTIONMODE, pos.m_nSelMode);
    Call(SCI_SETANCHOR, pos.m_nStartPos);
    Call(SCI_SETCURRENTPOS, pos.m_nEndPos);
    Call(SCI_CANCEL);
    auto wrapmode = Call(SCI_GETWRAPMODE);
    if (wrapmode == SC_WRAP_NONE)
    {
        // only offset if not wrapping, otherwise the offset isn't needed at all
        Call(SCI_SETSCROLLWIDTH, pos.m_nScrollWidth);
        Call(SCI_SETXOFFSET, pos.m_xOffset);
    }
    Call(SCI_CHOOSECARETX);

    size_t lineToShow = Call(SCI_VISIBLEFROMDOCLINE, pos.m_nFirstVisibleLine);
    if (wrapmode != SC_WRAP_NONE)
    {
        // if wrapping is enabled, scrolling to a line won't work
        // properly until scintilla has painted the document, because
        // the wrap calculations aren't finished until then.
        // So we set the scroll position here to a member variable,
        // which then is used to scroll scintilla to that line after
        // the first SCN_PAINTED event.
        m_LineToScrollToAfterPaint = lineToShow;
        m_WrapOffsetToScrollToAfterPaint = pos.m_nWrapLineOffset;
    }
    else
        Call(SCI_LINESCROLL, 0, lineToShow);
    // call UpdateLineNumberWidth() here, just in case the SCI_LINESCROLL call
    // above does not scroll the window.
    UpdateLineNumberWidth();
}

void CScintillaWnd::SetupLexerForLang( const std::string& lang )
{
    const auto& lexerdata = CLexStyles::Instance().GetLexerDataForLang(lang);
    const auto& keywords = CLexStyles::Instance().GetKeywordsForLang(lang);
    const auto& theme = CTheme::Instance();

    // first set up only the default styles
    std::wstring defaultFont;
    if (m_hasConsolas)
        defaultFont = L"Consolas";
    else
        defaultFont = L"Courier New";
    std::string sFontName = CUnicodeUtils::StdGetUTF8(CIniSettings::Instance().GetString(L"View", L"FontName", defaultFont.c_str()));
    Call(SCI_STYLESETFONT, STYLE_DEFAULT, (LPARAM)sFontName.c_str());
    bool bBold = !!CIniSettings::Instance().GetInt64(L"View", L"FontBold", false);
    bool bItalic = !!CIniSettings::Instance().GetInt64(L"View", L"FontItalic", false);
    int fontsize = (int)CIniSettings::Instance().GetInt64(L"View", L"FontSize", 11);
    Call(SCI_STYLESETBOLD, STYLE_DEFAULT, bBold);
    Call(SCI_STYLESETITALIC, STYLE_DEFAULT, bItalic);
    Call(SCI_STYLESETSIZE, STYLE_DEFAULT, fontsize);
    Call(SCI_STYLESETFORE, STYLE_DEFAULT, theme.GetThemeColor(RGB(0, 0, 0)));
    Call(SCI_STYLESETBACK, STYLE_DEFAULT, theme.GetThemeColor(RGB(255, 255, 255)));

    // now call SCI_STYLECLEARALL to copy the default style to all styles
    Call(SCI_STYLECLEARALL);

    // now set up the out own styles
    SetupDefaultStyles();

    // and now set the lexer styles
    Call(SCI_SETLEXER, lexerdata.ID);

    for (const auto& it: lexerdata.Properties)
    {
        Call(SCI_SETPROPERTY, (WPARAM)it.first.c_str(), (LPARAM)it.second.c_str());
    }
    for (const auto& it: lexerdata.Styles)
    {
        const int styleId = it.first;
        const auto& sd = it.second;
        Call(SCI_STYLESETFORE, styleId, theme.GetThemeColor(sd.ForegroundColor));
        Call(SCI_STYLESETBACK, styleId, theme.GetThemeColor(sd.BackgroundColor));
        if (!sd.FontName.empty())
            Call(SCI_STYLESETFONT, styleId, (sptr_t)CUnicodeUtils::StdGetUTF8(sd.FontName).c_str());

        if ((it.second.FontStyle & FONTSTYLE_BOLD) != 0)
            Call(SCI_STYLESETBOLD, styleId, 1);
        if ((it.second.FontStyle & FONTSTYLE_ITALIC) != 0)
            Call(SCI_STYLESETITALIC, styleId, 1);
        if ((it.second.FontStyle & FONTSTYLE_UNDERLINED) !=0)
            Call(SCI_STYLESETUNDERLINE, styleId, 1);

        if (it.second.FontSize)
            Call(SCI_STYLESETSIZE, styleId, sd.FontSize);
        if (it.second.eolfilled)
            Call(SCI_STYLESETEOLFILLED, styleId, 1);
    }
    for (const auto& it: keywords)
    {
        Call(SCI_SETKEYWORDS, it.first-1, (LPARAM)it.second.c_str());
    }
    Call(SCI_SETLINEENDTYPESALLOWED, Call(SCI_GETLINEENDTYPESSUPPORTED));
}

void CScintillaWnd::SetupDefaultStyles()
{
    auto& theme = CTheme::Instance();
    std::wstring defaultFont;
    if (m_hasConsolas)
        defaultFont = L"Consolas";
    else
        defaultFont = L"Courier New";
    std::string sFontName = CUnicodeUtils::StdGetUTF8(CIniSettings::Instance().GetString(L"View", L"FontName", defaultFont.c_str()));
    Call(SCI_STYLESETFONT, STYLE_LINENUMBER, (LPARAM)sFontName.c_str());

    Call(SCI_STYLESETFORE, STYLE_LINENUMBER, theme.GetThemeColor(RGB(color_linenr_inactive, color_linenr_inactive, color_linenr_inactive)));
    Call(SCI_STYLESETBACK, STYLE_LINENUMBER, theme.GetThemeColor(RGB(230, 230, 230)));

    Call(SCI_INDICSETSTYLE, INDIC_SELECTION_MARK, INDIC_ROUNDBOX);
    Call(SCI_INDICSETALPHA, INDIC_SELECTION_MARK, 50);
    Call(SCI_INDICSETUNDER, INDIC_SELECTION_MARK, true);
    Call(SCI_INDICSETFORE,  INDIC_SELECTION_MARK, theme.GetThemeColor(RGB(0,255,0)));

    Call(SCI_INDICSETSTYLE, INDIC_FINDTEXT_MARK, INDIC_ROUNDBOX);
    Call(SCI_INDICSETALPHA, INDIC_FINDTEXT_MARK, theme.IsDarkTheme() ? 100 : 200);
    Call(SCI_INDICSETUNDER, INDIC_FINDTEXT_MARK, true);
    Call(SCI_INDICSETFORE,  INDIC_FINDTEXT_MARK, theme.GetThemeColor(RGB(255,255,0)));

    Call(SCI_INDICSETSTYLE, INDIC_URLHOTSPOT, INDIC_HIDDEN);
    Call(SCI_INDICSETALPHA, INDIC_URLHOTSPOT, 5);
    Call(SCI_INDICSETUNDER, INDIC_URLHOTSPOT, true);
    Call(SCI_INDICSETHOVERSTYLE, INDIC_URLHOTSPOT, INDIC_DOTS);
    Call(SCI_INDICSETHOVERFORE, INDIC_URLHOTSPOT, theme.GetThemeColor(RGB(0, 0, 255)));

    Call(SCI_INDICSETSTYLE, INDIC_BRACEMATCH, INDIC_ROUNDBOX);
    Call(SCI_INDICSETALPHA, INDIC_BRACEMATCH, 30);
    Call(SCI_INDICSETOUTLINEALPHA, INDIC_BRACEMATCH, 0);
    Call(SCI_INDICSETUNDER, INDIC_BRACEMATCH, true);
    Call(SCI_INDICSETFORE, INDIC_BRACEMATCH, theme.GetThemeColor(RGB(0, 150, 0)));

    Call(SCI_INDICSETSTYLE, INDIC_MISSPELLED, INDIC_SQUIGGLE);
    Call(SCI_INDICSETFORE, INDIC_MISSPELLED, theme.GetThemeColor(RGB(255, 0, 0)));
    Call(SCI_INDICSETUNDER, INDIC_MISSPELLED, true);

    Call(SCI_STYLESETFORE, STYLE_BRACELIGHT, theme.GetThemeColor(RGB(0,150,0)));
    Call(SCI_STYLESETBOLD, STYLE_BRACELIGHT, 1);
    Call(SCI_STYLESETFORE, STYLE_BRACEBAD, theme.GetThemeColor(RGB(255,0,0)));
    Call(SCI_STYLESETBOLD, STYLE_BRACEBAD, 1);

    Call(SCI_STYLESETFORE, STYLE_INDENTGUIDE, theme.GetThemeColor(RGB(200,200,200)));

    Call(SCI_INDICSETFORE, INDIC_TAGMATCH, theme.GetThemeColor(RGB(0x80, 0x00, 0xFF)));
    Call(SCI_INDICSETFORE, INDIC_TAGATTR, theme.GetThemeColor(RGB(0xFF, 0xFF, 0x00)));
    Call(SCI_INDICSETSTYLE, INDIC_TAGMATCH, INDIC_ROUNDBOX);
    Call(SCI_INDICSETSTYLE, INDIC_TAGATTR, INDIC_ROUNDBOX);
    Call(SCI_INDICSETALPHA, INDIC_TAGMATCH, 100);
    Call(SCI_INDICSETALPHA, INDIC_TAGATTR, 100);
    Call(SCI_INDICSETUNDER, INDIC_TAGMATCH, true);
    Call(SCI_INDICSETUNDER, INDIC_TAGATTR, true);

    Call(SCI_SETFOLDMARGINCOLOUR, true, theme.GetThemeColor(RGB(240, 240, 240)));
    Call(SCI_SETFOLDMARGINHICOLOUR, true, theme.GetThemeColor(RGB(255, 255, 255)));

    Call(SCI_MARKERSETFORE, MARK_BOOKMARK, CTheme::Instance().GetThemeColor(RGB(80, 0, 0)));
    Call(SCI_MARKERSETBACK, MARK_BOOKMARK, CTheme::Instance().GetThemeColor(RGB(255, 0, 0)));

    SetTabSettings(Default);
    Call(SCI_SETINDENTATIONGUIDES, (uptr_t)CIniSettings::Instance().GetInt64(L"View", L"indent", SC_IV_LOOKBOTH));

    SetupFoldingColors(RGB(color_folding_fore_inactive, color_folding_fore_inactive, color_folding_fore_inactive),
                       RGB(color_folding_back_inactive, color_folding_back_inactive, color_folding_back_inactive),
                       RGB(color_folding_backsel_inactive, color_folding_backsel_inactive, color_folding_backsel_inactive));

    bool bBold = !!CIniSettings::Instance().GetInt64(L"View", L"FontBold", false);
    bool bItalic = !!CIniSettings::Instance().GetInt64(L"View", L"FontItalic", false);
    int fontsize = (int)CIniSettings::Instance().GetInt64(L"View", L"FontSize", 11);
    Call(SCI_STYLESETBOLD, STYLE_FOLDDISPLAYTEXT, bBold);
    Call(SCI_STYLESETITALIC, STYLE_FOLDDISPLAYTEXT, bItalic);
    Call(SCI_STYLESETSIZE, STYLE_FOLDDISPLAYTEXT, fontsize);

    Call(SCI_STYLESETFORE, STYLE_DEFAULT, theme.GetThemeColor(RGB(0, 0, 0)));
    Call(SCI_STYLESETBACK, STYLE_DEFAULT, theme.GetThemeColor(RGB(255, 255, 255)));
    Call(SCI_SETSELFORE, TRUE, theme.GetThemeColor(RGB(0, 0, 0)));
    Call(SCI_SETSELBACK, TRUE, theme.GetThemeColor(RGB(51, 153, 255)));
    Call(SCI_SETCARETFORE, theme.GetThemeColor(RGB(0, 0, 0)));
    Call(SCI_SETADDITIONALCARETFORE, theme.GetThemeColor(RGB(0, 0, 80)));

    if (CIniSettings::Instance().GetInt64(L"View", L"caretlineframe", 1) != 0)
    {
        Call(SCI_SETCARETLINEBACK, theme.GetThemeColor(RGB(0, 0, 0)));
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
            Call(SCI_SETCARETLINEBACK, theme.GetThemeColor(RGB(0, 0, 0)));
            Call(SCI_SETCARETLINEBACKALPHA, 25);
        }
    }
    Call(SCI_SETWHITESPACEFORE, true, theme.IsDarkTheme() ? RGB(20, 72, 82) : RGB(43, 145, 175));
    
    auto modEventMask = Call(SCI_GETMODEVENTMASK);
    Call(SCI_SETMODEVENTMASK, 0);

    Call(SCI_COLOURISE, 0, -1);
    Call(SCI_SETCODEPAGE, CP_UTF8);
    Call(SCI_SETMODEVENTMASK, modEventMask);
}

void CScintillaWnd::SetupFoldingColors(COLORREF fore, COLORREF back, COLORREF backsel)
{
    // set the folding colors according to the parameters, but leave the
    // colors for the collapsed buttons (+ button) in the active color.
    auto foldmarkfore = CTheme::Instance().GetThemeColor(fore);
    auto foldmarkforeActive = CTheme::Instance().GetThemeColor(RGB(color_folding_fore_active, color_folding_fore_active, color_folding_fore_active));
    Call(SCI_MARKERSETFORE, SC_MARKNUM_FOLDEROPEN, foldmarkfore);
    Call(SCI_MARKERSETFORE, SC_MARKNUM_FOLDER, foldmarkforeActive);
    Call(SCI_MARKERSETFORE, SC_MARKNUM_FOLDERSUB, foldmarkfore);
    Call(SCI_MARKERSETFORE, SC_MARKNUM_FOLDERTAIL, foldmarkfore);
    Call(SCI_MARKERSETFORE, SC_MARKNUM_FOLDEREND, foldmarkforeActive);
    Call(SCI_MARKERSETFORE, SC_MARKNUM_FOLDEROPENMID, foldmarkfore);
    Call(SCI_MARKERSETFORE, SC_MARKNUM_FOLDERMIDTAIL, foldmarkfore);

    auto foldmarkback = CTheme::Instance().GetThemeColor(back);
    auto foldmarkbackActive = CTheme::Instance().GetThemeColor(RGB(color_folding_back_active, color_folding_back_active, color_folding_back_active));
    Call(SCI_MARKERSETBACK, SC_MARKNUM_FOLDEROPEN, foldmarkback);
    Call(SCI_MARKERSETBACK, SC_MARKNUM_FOLDER, foldmarkbackActive);
    Call(SCI_MARKERSETBACK, SC_MARKNUM_FOLDERSUB, foldmarkback);
    Call(SCI_MARKERSETBACK, SC_MARKNUM_FOLDERTAIL, foldmarkback);
    Call(SCI_MARKERSETBACK, SC_MARKNUM_FOLDEREND, foldmarkbackActive);
    Call(SCI_MARKERSETBACK, SC_MARKNUM_FOLDEROPENMID, foldmarkback);
    Call(SCI_MARKERSETBACK, SC_MARKNUM_FOLDERMIDTAIL, foldmarkback);

    auto foldmarkbackselected = CTheme::Instance().GetThemeColor(backsel);
    auto foldmarkbackselectedActive = CTheme::Instance().GetThemeColor(RGB(color_folding_backsel_active, color_folding_backsel_active, color_folding_backsel_active));
    Call(SCI_MARKERSETBACKSELECTED, SC_MARKNUM_FOLDEROPEN, foldmarkbackselected);
    Call(SCI_MARKERSETBACKSELECTED, SC_MARKNUM_FOLDER, foldmarkbackselectedActive);
    Call(SCI_MARKERSETBACKSELECTED, SC_MARKNUM_FOLDERSUB, foldmarkbackselected);
    Call(SCI_MARKERSETBACKSELECTED, SC_MARKNUM_FOLDERTAIL, foldmarkbackselected);
    Call(SCI_MARKERSETBACKSELECTED, SC_MARKNUM_FOLDEREND, foldmarkbackselectedActive);
    Call(SCI_MARKERSETBACKSELECTED, SC_MARKNUM_FOLDEROPENMID, foldmarkbackselected);
    Call(SCI_MARKERSETBACKSELECTED, SC_MARKNUM_FOLDERMIDTAIL, foldmarkbackselected);

    Call(SCI_STYLESETFORE, STYLE_FOLDDISPLAYTEXT, foldmarkfore);
    Call(SCI_STYLESETBACK, STYLE_FOLDDISPLAYTEXT, foldmarkback);
}

void CScintillaWnd::GotoLine(long line)
{
    long linepos = (long)Call(SCI_POSITIONFROMLINE, line);
    Center(linepos, linepos);
}

void CScintillaWnd::Center(sptr_t posStart, sptr_t posEnd)
{
    // to make sure the found result is visible
    // When searching up, the beginning of the (possible multiline) result is important, when scrolling down the end
    auto testPos = (posStart > posEnd) ? posEnd : posStart;
    Call(SCI_SETCURRENTPOS, testPos);
    auto currentlineNumberDoc = Call(SCI_LINEFROMPOSITION, testPos);
    auto currentlineNumberVis = Call(SCI_VISIBLEFROMDOCLINE, currentlineNumberDoc);
    // SCI_ENSUREVISIBLE resets the line-wrapping cache, so only
    // call that if it's really necessary.
    if (Call(SCI_GETLINEVISIBLE, currentlineNumberDoc) == 0)
        Call(SCI_ENSUREVISIBLE, currentlineNumberDoc);    // make sure target line is unfolded

    auto firstVisibleLineVis =   Call(SCI_GETFIRSTVISIBLELINE);
    auto linesVisible        =   Call(SCI_LINESONSCREEN) - 1; //-1 for the scrollbar
    auto lastVisibleLineVis  =   linesVisible + firstVisibleLineVis;

    // if out of view vertically, scroll line into (center of) view
    decltype(firstVisibleLineVis) linesToScroll = 0;
    if (currentlineNumberVis < (firstVisibleLineVis+(linesVisible/4)))
    {
        linesToScroll = currentlineNumberVis - firstVisibleLineVis;
        // use center
        linesToScroll -= linesVisible/2;
    }
    else if (currentlineNumberVis > (lastVisibleLineVis - (linesVisible / 4)))
    {
        linesToScroll = currentlineNumberVis - lastVisibleLineVis;
        // use center
        linesToScroll += linesVisible/2;
    }
    auto wrapmode = Call(SCI_GETWRAPMODE);
    if (wrapmode != SC_WRAP_NONE)
    {
        // if wrapping is enabled, scrolling to a line won't work
        // properly until scintilla has painted the document, because
        // the wrap calculations aren't finished until then.
        // So we set the scroll position here to a member variable,
        // which then is used to scroll scintilla to that line after
        // the first SCN_PAINTED event.
        m_LineToScrollToAfterPaint = currentlineNumberDoc;
        m_WrapOffsetToScrollToAfterPaint = 0;
    }
    else
        Call(SCI_LINESCROLL, 0, linesToScroll);

    // Make sure the caret is visible, scroll horizontally
    Call(SCI_GOTOPOS, posStart);
    Call(SCI_GOTOPOS, posEnd);

    Call(SCI_SETANCHOR, posStart);
}

void CScintillaWnd::MarginClick( SCNotification * pNotification )
{
    if ((pNotification->margin == SC_MARGE_SYMBOL) && !pNotification->modifiers)
        BookmarkToggle((int)Call(SCI_LINEFROMPOSITION, pNotification->position));
}


void CScintillaWnd::MarkSelectedWord( bool clear )
{
    static std::string lastSelText;
    static Sci_PositionCR lastStopPosition = 0;
    LRESULT firstline = Call(SCI_GETFIRSTVISIBLELINE);
    LRESULT lastline = firstline + Call(SCI_LINESONSCREEN);
    long startstylepos = (long)Call(SCI_POSITIONFROMLINE, firstline);
    startstylepos = max(startstylepos, 0);
    long endstylepos = (long)Call(SCI_POSITIONFROMLINE, lastline) + (long)Call(SCI_LINELENGTH, lastline);
    if (endstylepos < 0)
        endstylepos = (long)Call(SCI_GETLENGTH)-startstylepos;

    int len = endstylepos - startstylepos;
    if (len <= 0)
        return;

    // reset indicators
    Call(SCI_SETINDICATORCURRENT, INDIC_SELECTION_MARK);
    Call(SCI_INDICATORCLEARRANGE, startstylepos, len);

    int selTextLen = (int)Call(SCI_GETSELTEXT);
    if ((selTextLen <= 1)||(clear)) // Includes zero terminator so 1 means 0.
    {
        lastSelText.clear();
        m_docScroll.Clear(DOCSCROLLTYPE_SELTEXT);
        m_selTextMarkerCount = 0;
        SendMessage(*this, WM_NCPAINT, (WPARAM)1, 0);
        return;
    }

    size_t selStartLine = Call(SCI_LINEFROMPOSITION, Call(SCI_GETSELECTIONSTART), 0);
    size_t selEndLine   = Call(SCI_LINEFROMPOSITION, Call(SCI_GETSELECTIONEND), 0);
    if (selStartLine != selEndLine)
    {
        lastSelText.clear();
        m_docScroll.Clear(DOCSCROLLTYPE_SELTEXT);
        m_selTextMarkerCount = 0;
        SendMessage(*this, WM_NCPAINT, (WPARAM)1, 0);
        return;
    }

    size_t selStartPos = Call(SCI_GETSELECTIONSTART);
    auto seltextbuffer = std::make_unique<char[]>(selTextLen + 1);
    Call(SCI_GETSELTEXT, 0, (LPARAM)seltextbuffer.get());
    if (seltextbuffer[0] == 0)
    {
        lastSelText.clear();
        m_docScroll.Clear(DOCSCROLLTYPE_SELTEXT);
        m_selTextMarkerCount = 0;
        SendMessage(*this, WM_NCPAINT, (WPARAM)1, 0);
        return;
    }
    std::string sSelText = seltextbuffer.get();
    CStringUtils::trim(sSelText);
    if (sSelText.empty())
    {
        lastSelText.clear();
        m_docScroll.Clear(DOCSCROLLTYPE_SELTEXT);
        m_selTextMarkerCount = 0;
        SendMessage(*this, WM_NCPAINT, (WPARAM)1, 0);
        return;
    }
    // don't mark the text again if it's already marked by the search feature
    if (_stricmp(g_sHighlightString.c_str(), seltextbuffer.get()) == 0)
    {
        m_selTextMarkerCount = g_searchMarkerCount;
        return;
    }

    auto textbuffer = std::make_unique<char[]>(len + 1);
    Sci_TextRange textrange;
    textrange.lpstrText = textbuffer.get();
    textrange.chrg.cpMin = startstylepos;
    textrange.chrg.cpMax = endstylepos;
    Call(SCI_GETTEXTRANGE, 0, (LPARAM)&textrange);

    const char* startPos = strstr(textbuffer.get(), seltextbuffer.get());
    while (startPos)
    {
        // don't style the selected text itself
        if (selStartPos != size_t(startstylepos + (startPos - textbuffer.get())))
            Call(SCI_INDICATORFILLRANGE, startstylepos + (startPos - textbuffer.get()), selTextLen-1);
        startPos = strstr(startPos+1, seltextbuffer.get());
    }

    int lineCount = (int)Call(SCI_GETLINECOUNT);
    if ((selTextLen > 2) || (lineCount < 100000))
    {
        auto selTextDifferent = lastSelText.compare(seltextbuffer.get());
        if (selTextDifferent || (lastStopPosition != 0))
        {
            auto start = std::chrono::steady_clock::now();
            if (selTextDifferent)
            {
                m_docScroll.Clear(DOCSCROLLTYPE_SELTEXT);
                m_selTextMarkerCount = 0;
            }
            Sci_TextToFind FindText;
            FindText.chrg.cpMin = lastStopPosition;
            FindText.chrg.cpMax = (long)Call(SCI_GETLENGTH);
            FindText.lpstrText = seltextbuffer.get();
            lastStopPosition = 0;
            const auto selTextColor = CTheme::Instance().GetThemeColor(RGB(0, 255, 0));
            while (Call(SCI_FINDTEXT, SCFIND_MATCHCASE, (LPARAM)&FindText) >= 0)
            {
                size_t line = Call(SCI_LINEFROMPOSITION, FindText.chrgText.cpMin);
                m_docScroll.AddLineColor(DOCSCROLLTYPE_SELTEXT, line, selTextColor);
                ++m_selTextMarkerCount;
                if (FindText.chrg.cpMin >= FindText.chrgText.cpMax)
                    break;
                FindText.chrg.cpMin = FindText.chrgText.cpMax;
                
                // stop after 1.5 seconds - users don't want to wait for too long
                auto end = std::chrono::steady_clock::now();
                if (std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() > 1500)
                {
                    lastStopPosition = FindText.chrg.cpMin;
                    break;
                }
            }
            SendMessage(*this, WM_NCPAINT, (WPARAM)1, 0);
        }
        lastSelText = seltextbuffer.get();
    }
}

void CScintillaWnd::MatchBraces(BraceMatch what)
{
    static int lastIndicatorStart = 0;
    static int lastIndicatorLength = 0;
    static int lastCaretPos = 0;

    int braceAtCaret = -1;
    int braceOpposite = -1;

    // find matching brace position
    int caretPos = int(Call(SCI_GETCURRENTPOS));

    // setting the highlighting style triggers an UI update notification,
    // which in return calls MatchBraces(false). So to avoid an endless
    // loop, we bail out if the caret position has not changed.
    if ((what==BraceMatch::Braces) && (caretPos == lastCaretPos))
        return;
    lastCaretPos = caretPos;

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
                lastIndicatorStart = braceAtCaret < braceOpposite ? braceAtCaret : braceOpposite;
                lastIndicatorLength = braceAtCaret < braceOpposite ? braceOpposite - braceAtCaret : braceAtCaret - braceOpposite;
                ++lastIndicatorLength;
                int startLine = int(Call(SCI_LINEFROMPOSITION, lastIndicatorStart));
                int endLine = int(Call(SCI_LINEFROMPOSITION, lastIndicatorStart + lastIndicatorLength));
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
            int columnAtCaret = int(Call(SCI_GETCOLUMN, braceAtCaret));
            int columnOpposite = int(Call(SCI_GETCOLUMN, braceOpposite));
            Call(SCI_SETHIGHLIGHTGUIDE, (columnAtCaret < columnOpposite) ? columnAtCaret : columnOpposite);
        }
    }
}

// find matching brace position
void CScintillaWnd::GotoBrace()
{
    static constexpr wchar_t brackets[] = L"[](){}";

    int lengthDoc = int(Call(SCI_GETLENGTH));
    if (lengthDoc <= 1)
        return;

    int braceAtCaret = -1;
    int braceOpposite = -1;
    WCHAR charBefore = '\0';
    WCHAR charAfter = '\0';
    bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;

    int caretPos = int(Call(SCI_GETCURRENTPOS));
    if (caretPos > 0)
    {
        charBefore = WCHAR(Call(SCI_GETCHARAT, caretPos - 1, 0));
        if (charBefore && wcschr(brackets, charBefore)) // Priority goes to character before the caret.
            braceAtCaret = caretPos - 1;
    }

    if (braceAtCaret < 0) // No brace found so check other side
    {        
        charAfter = WCHAR(Call(SCI_GETCHARAT, caretPos, 0));
        if (charAfter && wcschr(brackets, charAfter))
            braceAtCaret = caretPos;
    }
    if (braceAtCaret >= 0)
    {
        braceOpposite = int(Call(SCI_BRACEMATCH, braceAtCaret, 0));
        if (braceOpposite >= 0)
        {
            if (shift)
            {
                if (braceOpposite >= braceAtCaret)
                    ++braceOpposite;
                else
                    ++braceAtCaret;
            }
            else if (braceAtCaret >= 0 && braceOpposite >= 0 && braceOpposite > braceAtCaret
                && braceOpposite == braceAtCaret + 1 && charBefore)
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
        for (const auto& attr : attributes)
        {
            Call(SCI_INDICATORFILLRANGE, attr.first, attr.second - attr.first);
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
    auto selCount = Call(SCI_GETSELECTIONS);
    selByte = 0;
    selLine = 0;
    for (decltype(selCount) i = 0; i < selCount; ++i)
    {
        size_t start = Call(SCI_GETSELECTIONNSTART, i);
        size_t end = Call(SCI_GETSELECTIONNEND, i);
        selByte += (start < end) ? end - start : start - end;

        start = Call(SCI_LINEFROMPOSITION, start);
        end = Call(SCI_LINEFROMPOSITION, end);
        selLine += (start < end) ? end - start : start - end;
        ++selLine;
    }

    return true;
};

LRESULT CALLBACK CScintillaWnd::HandleScrollbarCustomDraw( WPARAM wParam, NMCSBCUSTOMDRAW * pCustDraw )
{
    m_docScroll.SetCurrentPos(Call(SCI_VISIBLEFROMDOCLINE, GetCurrentLineNumber()), CTheme::Instance().GetThemeColor(RGB(40,40,40)));
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
                if (!tagName.empty())
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

                } // end if !tagName.empty()
            } // end open tag test
        }
    }
    return tagFound;
}

FindResult CScintillaWnd::FindText( const char *text, size_t start, size_t end, int flags )
{
    FindResult returnValue = { 0 };

    Sci_TextToFind search;
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

size_t CScintillaWnd::FindText(const std::string& tofind, long startpos, long endpos)
{
    Sci_TextToFind ttf = { 0 };
    ttf.chrg.cpMin = startpos;
    ttf.chrg.cpMax = endpos;
    ttf.lpstrText = const_cast<char*>(tofind.c_str());
    return Call(SCI_FINDTEXT, 0, (sptr_t)&ttf);
}

FindResult CScintillaWnd::FindOpenTag(const std::string& tagName, size_t start, size_t end)
{
    std::string search("<");
    search.append(tagName);
    FindResult openTagFound = { 0 };
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
    FindResult closeTagFound = { 0 };
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
    auto buf = std::make_unique<char[]>(bufLen+1);
    Sci_TextRange tr;
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

bool CScintillaWnd::AutoBraces( WPARAM wParam )
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
        (wParam == '[') )
    {
        if (CIniSettings::Instance().GetInt64(L"View", L"autobrace", 1) == 0)
            return false;
        char braceBuf[2] = { 0 };
        braceBuf[0] = (char)wParam;
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
        bool bSelEmpty          = !!Call(SCI_GETSELECTIONEMPTY);
        size_t lineStartStart   = 0;
        size_t lineEndEnd       = 0;
        if (!bSelEmpty)
        {
            size_t selStart  = Call(SCI_GETSELECTIONSTART);
            size_t selEnd    = Call(SCI_GETSELECTIONEND);
            size_t lineStart = Call(SCI_LINEFROMPOSITION, selStart);
            size_t lineEnd   = Call(SCI_LINEFROMPOSITION, selEnd);
            if (Call(SCI_POSITIONFROMLINE, lineEnd) == (sptr_t)selEnd)
            {
                --lineEnd;
                selEnd = Call(SCI_GETLINEENDPOSITION, lineEnd);
            }
            lineStartStart  = Call(SCI_POSITIONFROMLINE, lineStart);
            lineEndEnd      = Call(SCI_GETLINEENDPOSITION, lineEnd);
            if ((lineStartStart != selStart) || (lineEndEnd != selEnd) || (wParam == '(') || (wParam == '['))
            {
                // insert the brace before the selection and the closing brace after the selection
                Call(SCI_SETSEL, (uptr_t)-1, selStart);
                Call(SCI_BEGINUNDOACTION);
                Call(SCI_INSERTTEXT, selStart, (sptr_t)braceBuf);
                Call(SCI_INSERTTEXT, selEnd+1, (sptr_t)braceCloseBuf);
                Call(SCI_SETSEL, selStart+1, selStart+1);
                Call(SCI_ENDUNDOACTION);
                return true;
            }
            else
            {
                // get info
                size_t tabIndent = Call(SCI_GETTABWIDTH);
                int indentAmount = (int)Call(SCI_GETLINEINDENTATION, lineStart > 0 ? lineStart-1 : lineStart);
                int indentAmountfirst = (int)Call(SCI_GETLINEINDENTATION, lineStart);
                if (indentAmount == 0)
                    indentAmount = indentAmountfirst;
                Call(SCI_BEGINUNDOACTION);

                // insert a new line at the end of the selected lines
                Call(SCI_SETSEL, lineEndEnd, lineEndEnd);
                Call(SCI_NEWLINE);
                // now insert the end-brace and indent it
                Call(SCI_INSERTTEXT, (uptr_t)-1, (sptr_t)braceCloseBuf);
                Call(SCI_SETLINEINDENTATION, lineEnd+1, indentAmount);

                Call(SCI_SETSEL, lineStartStart, lineStartStart);
                // now insert the start-brace and a newline after it
                Call(SCI_INSERTTEXT, (uptr_t)-1, (sptr_t)braceBuf);
                Call(SCI_SETSEL, lineStartStart+1, lineStartStart+1);
                Call(SCI_NEWLINE);
                // now indent the line with the start brace
                Call(SCI_SETLINEINDENTATION, lineStart, indentAmount);

                // increase the indentation of all selected lines
                if (indentAmount == indentAmountfirst)
                {
                    for (size_t line = lineStart+1; line <= lineEnd+1; ++line)
                    {
                        Call(SCI_SETLINEINDENTATION, line, Call(SCI_GETLINEINDENTATION, line)+tabIndent);
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
                Call(SCI_ADDTEXT, 1, (sptr_t)braceBuf);
                // insert the closing brace
                Call(SCI_ADDTEXT, 1, (sptr_t)braceCloseBuf);
                // go back one char
                Call(SCI_CHARLEFT);
                Call(SCI_ENDUNDOACTION);
                return true;
            }
        }
    }
    else if (wParam == '>')
    {
        lexer = Call(SCI_GETLEXER);
        switch (lexer)
        {
            // add the closing tag only for xml and html lexers
            case SCLEX_XML:
            case SCLEX_HTML:
                break;
            default:
                return false;
        }
        if (CIniSettings::Instance().GetInt64(L"View", L"autobracexml", 1) == 0)
            return false;

        // check if there's a '/' char before the opening '<' (searching backwards)
        FindResult result1, result2;
        size_t currentpos = Call(SCI_GETCURRENTPOS);
        result1 = FindText("/", currentpos, 0, 0);
        result2 = FindText("<", currentpos, 0, 0);
        if (result2.success)
        {
            if (!result1.success || (result2.start > result1.start))
            {
                // seems like an opening xml tag
                // prevent closing tags that are not full tags
                auto c = Call(SCI_GETCHARAT, result2.start + 1, 0);
                if ((c == '?') || (c == '%'))
                    return false;
                // insert the closing tag now
                Call(SCI_ADDTEXT, 1, (sptr_t)">");
                size_t cursorPos = Call(SCI_GETCURRENTPOS);
                // find the tag id
                std::string tagName;
                size_t position = result2.start + 1;
                int nextChar = (int)Call(SCI_GETCHARAT, position);
                while(position < currentpos && !IsXMLWhitespace(nextChar) && nextChar != '/' && nextChar != '>' && nextChar != '\"' && nextChar != '\'')
                {
                    tagName.push_back((char)nextChar);
                    ++position;
                    nextChar = (int)Call(SCI_GETCHARAT, position);
                }
                if (!tagName.empty())
                {
                    Call(SCI_BEGINUNDOACTION);
                    // we found the tag id, now insert the closing xml tag
                    Call(SCI_ADDTEXT, 2, (sptr_t)"</");
                    Call(SCI_ADDTEXT, tagName.size(), (sptr_t)tagName.c_str());
                    Call(SCI_ADDTEXT, 1, (sptr_t)">");
                    Call(SCI_GOTOPOS, cursorPos);
                    Call(SCI_ENDUNDOACTION);
                }
                return true;
            }
        }

    }
    return false;
}

void CScintillaWnd::ReflectEvents(SCNotification * pScn)
{
    switch (pScn->nmhdr.code)
    {
    case SCN_PAINTED:
        if (m_LineToScrollToAfterPaint != (size_t)-1)
        {
            auto visLineToScrollTo = Call(SCI_VISIBLEFROMDOCLINE, m_LineToScrollToAfterPaint);
            visLineToScrollTo += m_WrapOffsetToScrollToAfterPaint;
            auto currentVisPos = Call(SCI_VISIBLEFROMDOCLINE, Call(SCI_GETFIRSTVISIBLELINE));
            visLineToScrollTo -= currentVisPos;
            Call(SCI_LINESCROLL, 0, visLineToScrollTo);
            m_LineToScrollToAfterPaint = (size_t)-1;
            m_WrapOffsetToScrollToAfterPaint = 0;
            UpdateLineNumberWidth();
        }
        break;
    }
}

void CScintillaWnd::SetTabSettings(TabSpace ts)
{
    Call(SCI_SETTABWIDTH, (uptr_t)CIniSettings::Instance().GetInt64(L"View", L"tabsize", 4));
    if (ts == Default)
        Call(SCI_SETUSETABS, (uptr_t)CIniSettings::Instance().GetInt64(L"View", L"usetabs", 1));
    else
        Call(SCI_SETUSETABS, ts == Tabs ? 1 : 0);
    Call(SCI_SETBACKSPACEUNINDENTS, 1);
    Call(SCI_SETTABINDENTS, 1);
    Call(SCI_SETTABDRAWMODE, 1);
}

void CScintillaWnd::SetReadDirection(ReadDirection rd)
{
    //auto ex = GetWindowLongPtr(*this, GWL_EXSTYLE);
    //if (rd != R2L)
    //    ex &= WS_EX_LAYOUTRTL/*WS_EX_RTLREADING*/;
    //else
    //    ex |= WS_EX_LAYOUTRTL/*WS_EX_RTLREADING*/;
    //SetWindowLongPtr(*this, GWL_EXSTYLE, ex);
    Call(SCI_SETBIDIRECTIONAL, rd);
}

void CScintillaWnd::BookmarkAdd( long lineno )
{
    if (lineno == -1)
        lineno = GetCurrentLineNumber();
    if (!IsBookmarkPresent(lineno))
    {
        Call(SCI_MARKERADD, lineno, MARK_BOOKMARK);
        m_docScroll.AddLineColor(DOCSCROLLTYPE_BOOKMARK, lineno, CTheme::Instance().GetThemeColor(RGB(255,0,0)));
        DocScrollUpdate();
    }
}

void CScintillaWnd::BookmarkDelete( int lineno )
{
    if (lineno == -1)
        lineno = GetCurrentLineNumber();
    if ( IsBookmarkPresent(lineno))
    {
        Call(SCI_MARKERDELETE, lineno, MARK_BOOKMARK);
        m_docScroll.RemoveLine(DOCSCROLLTYPE_BOOKMARK, lineno);
        DocScrollUpdate();
    }
}

bool CScintillaWnd::IsBookmarkPresent( int lineno )
{
    if (lineno == -1)
        lineno = GetCurrentLineNumber();
    LRESULT state = Call(SCI_MARKERGET, lineno);
    return ((state & (1 << MARK_BOOKMARK)) != 0);
}

void CScintillaWnd::BookmarkToggle( int lineno )
{
    if (lineno == -1)
        lineno = GetCurrentLineNumber();

    if (IsBookmarkPresent(lineno))
        BookmarkDelete(lineno);
    else
        BookmarkAdd(lineno);
}

void CScintillaWnd::MarkBookmarksInScrollbar()
{
    const auto bmColor = CTheme::Instance().GetThemeColor(RGB(255, 0, 0));
    m_docScroll.Clear(DOCSCROLLTYPE_BOOKMARK);
    for (int line = -1;;)
    {
        line = (int)Call(SCI_MARKERNEXT, line + 1, (1 << MARK_BOOKMARK));
        if (line < 0)
            break;
        m_docScroll.AddLineColor(DOCSCROLLTYPE_BOOKMARK, line, bmColor);
    }
    DocScrollUpdate();
}

void CScintillaWnd::DocScrollUpdate()
{
    InvalidateRect(*this, nullptr, TRUE);
    SCNotification Scn = { 0 };
    Scn.message = SCN_UPDATEUI;
    Scn.updated = SC_UPDATE_CONTENT;
    Scn.nmhdr.code = SCN_UPDATEUI;
    Scn.nmhdr.hwndFrom = *this;
    Scn.nmhdr.idFrom = (uptr_t)this;
    SendMessage(GetParent(*this), WM_NOTIFY, (WPARAM)(this), (LPARAM)&Scn);

    // force the scrollbar to redraw

    bool ok = SetWindowPos(*this, nullptr, 0, 0, 0, 0,
                           SWP_FRAMECHANGED | // NO to everything else
                           SWP_NOZORDER | SWP_NOMOVE | SWP_NOSIZE | SWP_NOOWNERZORDER |
                           SWP_NOACTIVATE | SWP_NOSENDCHANGING
                           ) != FALSE;
    APPVERIFY(ok);
}

void CScintillaWnd::SetEOLType(int eolType)
{
    Call(SCI_SETEOLMODE, eolType);
}

void CScintillaWnd::AppendText(int len, const char* buf)
{
    Call(SCI_APPENDTEXT, len, reinterpret_cast<LPARAM>(buf));
}

std::string CScintillaWnd::GetLine(long line) const
{
    size_t linesize = ConstCall(SCI_GETLINE, line, 0);
    auto pLine = std::make_unique<char[]>(linesize + 1);
    ConstCall(SCI_GETLINE, line, (sptr_t)pLine.get());
    pLine[linesize] = 0;
    return pLine.get();
}

std::string CScintillaWnd::GetTextRange(Sci_Position startpos, Sci_Position endpos) const
{
    assert(endpos - startpos >= 0);
    if (endpos < startpos)
        return "";
    auto strbuf = std::make_unique<char[]>(endpos - startpos + 5);
    Sci_TextRange rangestart;
    rangestart.chrg.cpMin = (Sci_PositionCR)startpos;
    rangestart.chrg.cpMax = (Sci_PositionCR)endpos;
    rangestart.lpstrText = strbuf.get();
    ConstCall(SCI_GETTEXTRANGE, 0, (sptr_t)&rangestart);
    return strbuf.get();
}

std::string CScintillaWnd::GetSelectedText(bool useCurrentWordIfSelectionEmpty) const
{
    int selTextLen = (int)ConstCall(SCI_GETSELTEXT);
    auto seltextbuffer = std::make_unique<char[]>(selTextLen + 1);
    ConstCall(SCI_GETSELTEXT, 0, (LPARAM)seltextbuffer.get());
    std::string selText = seltextbuffer.get();
    if (selText.empty() && useCurrentWordIfSelectionEmpty)
    {
        selText = GetCurrentWord();
    }
    return selText;
}

std::string CScintillaWnd::GetCurrentWord() const
{
    long currentPos = (long)ConstCall(SCI_GETCURRENTPOS);
    long startPos = (long)ConstCall(SCI_WORDSTARTPOSITION, currentPos, true);
    long endPos = (long)ConstCall(SCI_WORDENDPOSITION, currentPos, true);
    return GetTextRange(startPos, endPos);
}

std::string CScintillaWnd::GetCurrentLine() const
{
    int LineLen = (int)ConstCall(SCI_GETCURLINE);
    auto linebuffer = std::make_unique<char[]>(LineLen + 1);
    ConstCall(SCI_GETCURLINE, LineLen + 1, (LPARAM)linebuffer.get());
    return linebuffer.get();
}

std::string CScintillaWnd::GetWordChars() const
{
    int len = (int)ConstCall(SCI_GETWORDCHARS);
    auto linebuffer = std::make_unique<char[]>(len + 1);
    ConstCall(SCI_GETWORDCHARS, 0, (LPARAM)linebuffer.get());
    linebuffer[len] = '\0';
    return linebuffer.get();
}

std::string CScintillaWnd::GetWhitespaceChars() const
{
    int len = (int)ConstCall(SCI_GETWHITESPACECHARS);
    auto linebuffer = std::make_unique<char[]>(len + 1);
    ConstCall(SCI_GETWHITESPACECHARS, 0, (LPARAM)linebuffer.get());
    return linebuffer.get();
}

long CScintillaWnd::GetCurrentLineNumber() const
{
    return (long)ConstCall(SCI_LINEFROMPOSITION, ConstCall(SCI_GETCURRENTPOS));
}
