// This file is part of BowPad.
//
// Copyright (C) 2013-2024 - Stefan Kueng
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
#include "CommandHandler.h"
#include "DarkModeHelper.h"
#include "DocScroll.h"
#include "Document.h"
#include "DPIAware.h"
#include "Lexilla.h"
#include "LexStyles.h"
#include "SciLexer.h"
#include "StringUtils.h"
#include "Theme.h"
#include "UnicodeUtils.h"
#include "../ext/lexilla/lexlib/LexerModule.h"
#include "../ext/scintilla/include/ILexer.h"
#include "../ext/scintilla/include/ScintillaStructures.h"

#include <chrono>
#include <UIRibbon.h>
#include <UIRibbonPropertyHelpers.h>
#include <uxtheme.h>

constexpr Scintilla::AutomaticFold operator|(Scintilla::AutomaticFold a, Scintilla::AutomaticFold b) noexcept
{
    return static_cast<Scintilla::AutomaticFold>(static_cast<int>(a) | static_cast<int>(b));
}

constexpr Scintilla::EOLAnnotationVisible operator|(Scintilla::EOLAnnotationVisible a, Scintilla::EOLAnnotationVisible b) noexcept
{
    return static_cast<Scintilla::EOLAnnotationVisible>(static_cast<int>(a) | static_cast<int>(b));
}

constexpr Scintilla::FoldLevel operator|(Scintilla::FoldLevel a, Scintilla::FoldLevel b) noexcept
{
    return static_cast<Scintilla::FoldLevel>(static_cast<int>(a) | static_cast<int>(b));
}

extern IUIFramework*        g_pFramework;
extern std::string          g_sHighlightString;  // from CmdFindReplace
extern sptr_t               g_searchMarkerCount; // from CmdFindReplace
extern Lexilla::LexerModule lmSimple;
extern Lexilla::LexerModule lmLog;
extern Lexilla::LexerModule lmSnippets;
extern Lexilla::LexerModule lmAhk;

UINT32                      g_contextID                    = cmdContextMap;

constexpr int               TIM_HIDECURSOR                 = 101;
constexpr int               TIM_BRACEHIGHLIGHTTEXT         = 102;
constexpr int               TIM_BRACEHIGHLIGHTTEXTCLEAR    = 103;

static bool                 g_scintillaInitialized         = false;

constexpr int               color_folding_fore_inactive    = 255;
constexpr int               color_folding_back_inactive    = 220;
constexpr int               color_folding_backsel_inactive = 150;
constexpr int               color_folding_fore_active      = 250;
constexpr int               color_folding_back_active      = 100;
constexpr int               color_folding_backsel_active   = 20;
constexpr int               color_linenr_inactive          = 109;
constexpr int               color_linenr_active            = 60;
constexpr double            folding_color_animation_time   = 0.3;

COLORREF                    toRgba(COLORREF c, BYTE alpha = 255)
{
    return (c | (static_cast<DWORD>(alpha) << 24));
}

CScintillaWnd::CScintillaWnd(HINSTANCE hInst)
    : CWindow(hInst)
    , m_scrollTool(hInst)
    , m_selTextMarkerCount(0)
    , m_bCursorShown(true)
    , m_bScratch(false)
    , m_eraseBkgnd(true)
    , m_hugeLevelReached(false)
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

    m_scintilla.SetFnPtr(reinterpret_cast<Scintilla::FunctionDirect>(SendMessage(*this, SCI_GETDIRECTFUNCTION, 0, 0)),
                         static_cast<sptr_t>(SendMessage(*this, SCI_GETDIRECTPOINTER, 0, 0)));

    if (!m_scintilla.IsValid())
        return false;

    m_docScroll.InitScintilla(this);

    m_scintilla.SetModEventMask(Scintilla::ModificationFlags::InsertText |
                                Scintilla::ModificationFlags::DeleteText |
                                Scintilla::ModificationFlags::Undo |
                                Scintilla::ModificationFlags::Redo |
                                Scintilla::ModificationFlags::MultiStepUndoRedo |
                                Scintilla::ModificationFlags::LastStepInUndoRedo |
                                Scintilla::ModificationFlags::BeforeInsert |
                                Scintilla::ModificationFlags::BeforeDelete |
                                Scintilla::ModificationFlags::MultilineUndoRedo |
                                Scintilla::ModificationFlags::ChangeFold |
                                Scintilla::ModificationFlags::ChangeStyle);
    bool bUseD2D = CIniSettings::Instance().GetInt64(L"View", L"d2d", 1) != 0;
    m_scintilla.SetTechnology(bUseD2D ? Scintilla::Technology::DirectWriteRetain : Scintilla::Technology::Default);
    m_scintilla.SetLayoutThreads(1000);

    m_scintilla.SetMarginMaskN(SC_MARGE_FOLDER, SC_MASK_FOLDERS);
    m_scintilla.SetMarginWidthN(SC_MARGE_FOLDER, CDPIAware::Instance().Scale(*this, 14));
    m_scintilla.SetMarginCursorN(SC_MARGE_FOLDER, Scintilla::CursorShape::Arrow);
    m_scintilla.SetAutomaticFold(Scintilla::AutomaticFold::Show | Scintilla::AutomaticFold::Change);

    m_scintilla.SetMarginMaskN(SC_MARGE_SYMBOL, (1 << MARK_BOOKMARK));
    m_scintilla.SetMarginWidthN(SC_MARGE_SYMBOL, CDPIAware::Instance().Scale(*this, 14));
    m_scintilla.SetMarginCursorN(SC_MARGE_SYMBOL, Scintilla::CursorShape::Arrow);
    m_scintilla.MarkerSetAlpha(MARK_BOOKMARK, static_cast<Scintilla::Alpha>(70));
    m_scintilla.MarkerDefine(MARK_BOOKMARK, Scintilla::MarkerSymbol::VerticalBookmark);

    m_scintilla.SetMarginSensitiveN(SC_MARGE_FOLDER, true);
    m_scintilla.SetMarginSensitiveN(SC_MARGE_SYMBOL, true);

    m_scintilla.SetScrollWidthTracking(true);
    m_scintilla.SetScrollWidth(1); // default empty document: override default width

    m_scintilla.MarkerDefine(SC_MARKNUM_FOLDEROPEN, Scintilla::MarkerSymbol::BoxMinus);
    m_scintilla.MarkerDefine(SC_MARKNUM_FOLDER, Scintilla::MarkerSymbol::BoxPlus);
    m_scintilla.MarkerDefine(SC_MARKNUM_FOLDERSUB, Scintilla::MarkerSymbol::VLine);
    m_scintilla.MarkerDefine(SC_MARKNUM_FOLDERTAIL, Scintilla::MarkerSymbol::LCornerCurve);
    m_scintilla.MarkerDefine(SC_MARKNUM_FOLDEREND, Scintilla::MarkerSymbol::BoxPlusConnected);
    m_scintilla.MarkerDefine(SC_MARKNUM_FOLDEROPENMID, Scintilla::MarkerSymbol::BoxMinusConnected);
    m_scintilla.MarkerDefine(SC_MARKNUM_FOLDERMIDTAIL, Scintilla::MarkerSymbol::TCornerCurve);
    m_scintilla.MarkerEnableHighlight(true);
    m_scintilla.SetFoldFlags(Scintilla::FoldFlag::LineAfterContracted);
    m_scintilla.FoldDisplayTextSetStyle(Scintilla::FoldDisplayTextStyle::Standard);

    m_scintilla.SetWrapVisualFlags(Scintilla::WrapVisualFlag::None);
    m_scintilla.SetWrapMode(Scintilla::Wrap::None);

    m_scintilla.SetFontQuality(Scintilla::FontQuality::QualityLcdOptimized);

    m_scintilla.StyleSetVisible(STYLE_CONTROLCHAR, TRUE);

    m_scintilla.SetCaretStyle(Scintilla::CaretStyle::Line);
    m_scintilla.SetCaretLineVisibleAlways(true);
    m_scintilla.SetCaretWidth(CDPIAware::Instance().Scale(*this, bUseD2D ? 2 : 1));
    if (CIniSettings::Instance().GetInt64(L"View", L"caretlineframe", 1) != 0)
        m_scintilla.SetCaretLineFrame(CDPIAware::Instance().Scale(*this, 1));
    m_scintilla.SetCaretLineHighlightSubLine(false);
    m_scintilla.SetWhitespaceSize(CDPIAware::Instance().Scale(*this, 1));
    m_scintilla.SetMultipleSelection(true);
    m_scintilla.SetMouseSelectionRectangularSwitch(true);
    m_scintilla.SetAdditionalSelectionTyping(true);
    m_scintilla.SetAdditionalCaretsBlink(true);
    m_scintilla.SetMultiPaste(Scintilla::MultiPaste::Each);
    m_scintilla.SetVirtualSpaceOptions(Scintilla::VirtualSpace::RectangularSelection);

    m_scintilla.SetMouseWheelCaptures(false);

    // For Ctrl+C, use SCI_COPYALLOWLINE instead of SCI_COPY
    m_scintilla.AssignCmdKey('C' + (SCMOD_CTRL << 16), SCI_COPYALLOWLINE);

    // change the home and end key to honor wrapped lines
    // but allow the line-home and line-end to be used with the ALT key
    m_scintilla.AssignCmdKey(SCK_HOME, SCI_VCHOMEWRAP);
    m_scintilla.AssignCmdKey(SCK_END, SCI_LINEENDWRAP);
    m_scintilla.AssignCmdKey(SCK_HOME + (SCMOD_SHIFT << 16), SCI_VCHOMEWRAPEXTEND);
    m_scintilla.AssignCmdKey(SCK_END + (SCMOD_SHIFT << 16), SCI_LINEENDWRAPEXTEND);
    m_scintilla.AssignCmdKey(SCK_HOME + (SCMOD_ALT << 16), SCI_VCHOME);
    m_scintilla.AssignCmdKey(SCK_END + (SCMOD_ALT << 16), SCI_LINEEND);

    // line cut for Ctrl+L
    m_scintilla.AssignCmdKey('L' + (SCMOD_CTRL << 16), SCI_LINECUT);

    m_scintilla.SetBufferedDraw(bUseD2D ? false : true);
    m_scintilla.SetPhasesDraw(bUseD2D ? Scintilla::PhasesDraw::Multiple : Scintilla::PhasesDraw::Two);
    m_scintilla.SetLayoutCache(Scintilla::LineCache::Page);

    m_scintilla.UsePopUp(Scintilla::PopUp::Never); // no default context menu

    m_scintilla.SetMouseDwellTime(GetDoubleClickTime());
    m_scintilla.CallTipSetPosition(true);

    m_scintilla.SetPasteConvertEndings(true);

    m_scintilla.SetCharacterCategoryOptimization(0x10000);
    m_scintilla.SetAccessibility(Scintilla::Accessibility::Enabled);

    m_scintilla.MarkerDefine(SC_MARKNUM_HISTORY_REVERTED_TO_ORIGIN, Scintilla::MarkerSymbol::LeftRect);
    m_scintilla.MarkerDefine(SC_MARKNUM_HISTORY_SAVED, Scintilla::MarkerSymbol::LeftRect);
    m_scintilla.MarkerDefine(SC_MARKNUM_HISTORY_MODIFIED, Scintilla::MarkerSymbol::LeftRect);
    m_scintilla.MarkerDefine(SC_MARKNUM_HISTORY_REVERTED_TO_MODIFIED, Scintilla::MarkerSymbol::LeftRect);

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

    m_scintilla.SetFnPtr(reinterpret_cast<Scintilla::FunctionDirect>(SendMessage(*this, SCI_GETDIRECTFUNCTION, 0, 0)),
                         static_cast<sptr_t>(SendMessage(*this, SCI_GETDIRECTPOINTER, 0, 0)));

    if (!m_scintilla.IsValid())
        return false;

    m_bScratch = true;

    return true;
}

bool             bEatNextTabOrEnterKey = false;

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
            switch (wParam)
            {
                case VK_RIGHT:
                case VK_LEFT:
                case VK_HOME:
                case VK_END:
                    if (Scintilla().SelectionMode() != Scintilla::SelectionMode::Stream)
                    {
                        // change to stream mode: converts the rectangular into a multiple selection
                        Scintilla().SetSelectionMode(Scintilla::SelectionMode::Stream);
                        // set stream mode again: cancels selection mode, so moving won't extend the selection anymore
                        Scintilla().SetSelectionMode(Scintilla::SelectionMode::Stream);
                    }
            }
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
                    m_scintilla.BeginUndoAction();
                    if (GetKeyState(VK_CONTROL) & 0x8000)
                    {
                        // Ctrl+Return: insert a line above the current line
                        // Cursor-Up, End, Return
                        m_scintilla.LineUp();
                    }
                    m_scintilla.LineEnd();
                    m_scintilla.NewLine();
                    m_scintilla.EndUndoAction();
                    return 0;
                }
                char c  = static_cast<char>(m_scintilla.CharAt(m_scintilla.CurrentPos() - 1));
                char c1 = static_cast<char>(m_scintilla.CharAt(m_scintilla.CurrentPos()));
                if ((c == '{') && (c1 == '}'))
                {
                    m_scintilla.NewLine();
                    m_scintilla.BeginUndoAction();
                    m_scintilla.LineUp();
                    m_scintilla.LineEnd();
                    m_scintilla.NewLine();
                    m_scintilla.Tab();
                    m_scintilla.EndUndoAction();
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
                m_scintilla.SetCursor(Scintilla::CursorShape::Normal);
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

                        auto  lineCount = m_scintilla.LineCount();
                        auto  w         = m_scrollTool.GetTextWidth(CStringUtils::Format(L"Line: %Id", lineCount).c_str());
                        POINT thumbPoint{};
                        thumbPoint.x = thumbRect.right - w;
                        thumbPoint.y = thumbRect.top + ((thumbRect.bottom - thumbRect.top) * si.nTrackPos) / (si.nMax - si.nMin);
                        auto docLine = m_scintilla.DocLineFromVisible(si.nTrackPos);
                        m_scrollTool.Init();
                        DarkModeHelper::Instance().AllowDarkModeForWindow(m_scrollTool, CTheme::Instance().IsDarkTheme());
                        SetWindowTheme(m_scrollTool, L"Explorer", nullptr);

                        m_scrollTool.SetText(&thumbPoint, L"Line: %Id", docLine + 1);
                    }
                    break;
                    default:
                        break;
                }
            }
            if (uMsg == WM_MOUSEMOVE)
            {
                RECT rc;
                GetClientRect(*this, &rc);
                rc.right = rc.left;
                for (int i = 0; i <= SC_MARGE_FOLDER; ++i)
                    rc.right += static_cast<long>(m_scintilla.MarginWidthN(i));
                POINT pt       = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
                auto  mousePos = m_scintilla.PositionFromPoint(pt.x, pt.y);
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
                                m_scintilla.StyleSetFore(STYLE_LINENUMBER, CTheme::Instance().GetThemeColor(RGB(ln, ln, ln), true));
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
                            m_scintilla.StyleSetFore(STYLE_LINENUMBER, CTheme::Instance().GetThemeColor(RGB(ln, ln, ln), true));
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
                                m_scintilla.StyleSetFore(STYLE_LINENUMBER, CTheme::Instance().GetThemeColor(RGB(ln, ln, ln), true));
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
                            m_scintilla.StyleSetFore(STYLE_LINENUMBER, CTheme::Instance().GetThemeColor(RGB(ln, ln, ln), true));
                        }
                        m_bInFolderMargin = false;
                    }
                }
            }
            {
                RECT rc;
                GetClientRect(*this, &rc);
                rc.right = rc.left;
                for (int i = 0; i <= SC_MARGE_FOLDER; ++i)
                    rc.right += static_cast<long>(m_scintilla.MarginWidthN(i));
                POINT          pt       = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
                auto           mousePos = m_scintilla.PositionFromPoint(pt.x, pt.y);
                SCNotification scn      = {};
                scn.nmhdr.code          = SCN_BP_MOUSEMSG;
                scn.position            = mousePos;
                scn.x                   = static_cast<int>(pt.x);
                scn.y                   = static_cast<int>(pt.y);
                scn.modifiers           = 0;
                scn.modificationType    = uMsg;
                scn.nmhdr.hwndFrom      = *this;
                scn.nmhdr.idFrom        = ::GetDlgCtrlID(*this);
                scn.wParam              = wParam;
                scn.lParam              = lParam;
                if (GetKeyState(VK_SHIFT) & 0x8000)
                    scn.modifiers |= SCMOD_SHIFT;
                if (GetKeyState(VK_CONTROL) & 0x8000)
                    scn.modifiers |= SCMOD_CTRL;
                if (GetKeyState(VK_MENU) & 0x8000)
                    scn.modifiers |= SCMOD_ALT;
                if (::SendMessage(::GetParent(*this), WM_NOTIFY,
                                  ::GetDlgCtrlID(*this), reinterpret_cast<LPARAM>(&scn)) == 0)
                    return 0;
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
                        m_scintilla.StyleSetFore(STYLE_LINENUMBER, CTheme::Instance().GetThemeColor(RGB(ln, ln, ln), true));
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
                    m_scintilla.StyleSetFore(STYLE_LINENUMBER, CTheme::Instance().GetThemeColor(RGB(ln, ln, ln), true));
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
                                    m_scintilla.SetCursor(static_cast<Scintilla::CursorShape>(-2));
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
                default:
                    break;
            }
            break;
        case WM_SETCURSOR:
        {
            if (!m_bCursorShown)
            {
                SetCursor(nullptr);
                m_scintilla.SetCursor(static_cast<Scintilla::CursorShape>(-2));
            }

            auto cur = m_scintilla.Cursor();
            if ((cur == static_cast<Scintilla::CursorShape>(8)) || (cur == static_cast<Scintilla::CursorShape>(0)))
                m_scintilla.SetCursor(Scintilla::CursorShape::Normal);
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
                        int        scrollX = (gi.ptsLocation.x - lastX) / scale;
                        int        scrollY = (gi.ptsLocation.y - lastY) / scale;

                        SCROLLINFO siv     = {0};
                        siv.cbSize         = sizeof(siv);
                        siv.fMask          = SIF_ALL;
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
                        m_scintilla.SetZoom(0);
                        // UpdateStatusBar(false);
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
        m_scintilla.SetMarginWidthN(SC_MARGE_LINENUMBER, 0);
        return;
    }
    auto linesVisible = m_scintilla.LinesOnScreen();
    if (linesVisible)
    {
        auto   firstVisibleLineVis = m_scintilla.FirstVisibleLine();
        auto   lastVisibleLineVis  = linesVisible + firstVisibleLineVis + 1;
        sptr_t i                   = 0;
        while (lastVisibleLineVis)
        {
            lastVisibleLineVis /= 10;
            i++;
        }
        i = max(i, 3);
        {
            int pixelWidth = static_cast<int>(CDPIAware::Instance().Scale(*this, 8) + i * m_scintilla.TextWidth(STYLE_LINENUMBER, "8"));
            m_scintilla.SetMarginWidthN(SC_MARGE_LINENUMBER, pixelWidth);
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
        auto firstVisibleLine   = m_scintilla.FirstVisibleLine();
        pos.m_nFirstVisibleLine = m_scintilla.DocLineFromVisible(firstVisibleLine);
        if (m_scintilla.WrapMode() != Scintilla::Wrap::None)
            pos.m_nWrapLineOffset = firstVisibleLine - m_scintilla.VisibleFromDocLine(pos.m_nFirstVisibleLine);

        pos.m_nStartPos    = m_scintilla.Anchor();
        pos.m_nEndPos      = m_scintilla.CurrentPos();
        pos.m_xOffset      = m_scintilla.XOffset();
        pos.m_nSelMode     = m_scintilla.SelectionMode();
        pos.m_nScrollWidth = m_scintilla.ScrollWidth();

        pos.m_lineStateVector.clear();
        pos.m_lastStyleLine             = 0;
        sptr_t contractedFoldHeaderLine = 0;
        do
        {
            contractedFoldHeaderLine = static_cast<sptr_t>(m_scintilla.ContractedFoldNext(contractedFoldHeaderLine));
            if (contractedFoldHeaderLine != -1)
            {
                // Store contracted line
                pos.m_lineStateVector.push_back(contractedFoldHeaderLine);
                pos.m_lastStyleLine = max(pos.m_lastStyleLine, (sptr_t)m_scintilla.LastChild(contractedFoldHeaderLine, (Scintilla::FoldLevel)-1));
                // Start next search with next line
                ++contractedFoldHeaderLine;
            }
        } while (contractedFoldHeaderLine != -1);
    }
    else
    {
        m_lineToScrollToAfterPaint = -1;
    }

    CUndoData undoData;
    undoData.m_savePoint     = m_scintilla.UndoSavePoint();
    undoData.m_currentAction = m_scintilla.UndoCurrent();
    undoData.m_tentative     = m_scintilla.UndoTentative();
    const int actions        = m_scintilla.UndoActions();
    for (int act = 0; act < actions; ++act)
    {
        CUndoAction undoAction;
        undoAction.m_type = m_scintilla.UndoActionType(act);
        const int actType = undoAction.m_type & 0xff;
        if (actType == 0 || actType == 1)
        {
            // Only insertions and deletions recorded, not container actions
            undoAction.m_position = m_scintilla.UndoActionPosition(act);
            undoAction.m_text     = m_scintilla.UndoActionText(act);
            undoData.m_actions.push_back(undoAction);
        }
    }
    pos.m_undoData = undoData;
}

void CScintillaWnd::RestoreCurrentPos(const CPosData& pos)
{
    if (!m_scintilla.CanUndo() && !m_scintilla.CanRedo() && !pos.m_undoData.m_actions.empty())
    {
        for (const auto& [type, position, text] : pos.m_undoData.m_actions)
        {
            m_scintilla.PushUndoActionType(type, position);
            m_scintilla.ChangeLastUndoActionText(text.length(), text.c_str());
        }
        if (const int savePoint = pos.m_undoData.m_savePoint; savePoint != -2)
            m_scintilla.SetUndoSavePoint(savePoint);
        if (const int tentative = pos.m_undoData.m_tentative; tentative != -2)
            m_scintilla.SetUndoTentative(tentative);
        if (const int current = pos.m_undoData.m_currentAction; current != -2)
            m_scintilla.SetUndoCurrent(current);
    }

    // if the document hasn't been styled yet, do it now
    // otherwise restoring the folds won't work.
    if (!pos.m_lineStateVector.empty())
    {
        auto endStyled = m_scintilla.EndStyled();
        auto len       = m_scintilla.TextLength();
        if (endStyled < len && pos.m_lastStyleLine)
            m_scintilla.Colourise(0, m_scintilla.PositionFromLine(pos.m_lastStyleLine + 1));
    }

    for (const auto& foldLine : pos.m_lineStateVector)
    {
        auto level      = m_scintilla.FoldLevel(foldLine);

        auto headerLine = 0;
        if ((level & Scintilla::FoldLevel::HeaderFlag) != Scintilla::FoldLevel::None)
            headerLine = static_cast<int>(foldLine);
        else
        {
            headerLine = static_cast<int>(m_scintilla.FoldParent(foldLine));
            if (headerLine == -1)
                return;
        }

        if (m_scintilla.FoldExpanded(headerLine) != 0)
            m_scintilla.ToggleFold(headerLine);
    }

    m_scintilla.GotoPos(0);

    m_scintilla.SetSelectionMode(pos.m_nSelMode);
    m_scintilla.SetAnchor(pos.m_nStartPos);
    m_scintilla.SetCurrentPos(pos.m_nEndPos);
    m_scintilla.Cancel();
    auto wrapMode = m_scintilla.WrapMode();
    if (wrapMode == Scintilla::Wrap::None)
    {
        // only offset if not wrapping, otherwise the offset isn't needed at all
        m_scintilla.SetScrollWidth(pos.m_nScrollWidth);
        m_scintilla.SetXOffset(pos.m_xOffset);
    }
    m_scintilla.ChooseCaretX();

    auto lineToShow = m_scintilla.VisibleFromDocLine(pos.m_nFirstVisibleLine);
    if (wrapMode != Scintilla::Wrap::None)
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
        m_scintilla.LineScroll(0, lineToShow);
    // call UpdateLineNumberWidth() here, just in case the SCI_LineScroll call
    // above does not scroll the window.
    UpdateLineNumberWidth();
}

void CScintillaWnd::SetupLexerForLang(const std::string& lang)
{
    const auto& lexerData = CLexStyles::Instance().GetLexerDataForLang(lang);
    const auto& keywords  = CLexStyles::Instance().GetKeywordsForLang(lang);
    const auto& theme     = CTheme::Instance();

    wchar_t     localeName[100];
    GetUserDefaultLocaleName(localeName, _countof(localeName));
    m_scintilla.SetFontLocale(CUnicodeUtils::StdGetUTF8(localeName).c_str());

    // first set up only the default styles
    std::wstring defaultFont;
    if (m_hasConsolas)
        defaultFont = L"Consolas";
    else
        defaultFont = L"Courier New";
    std::string sFontName = CUnicodeUtils::StdGetUTF8(CIniSettings::Instance().GetString(L"View", L"FontName", defaultFont.c_str()));
    m_scintilla.StyleSetFont(STYLE_DEFAULT, sFontName.c_str());
    bool bBold    = !!CIniSettings::Instance().GetInt64(L"View", L"FontBold", false);
    bool bItalic  = !!CIniSettings::Instance().GetInt64(L"View", L"FontItalic", false);
    int  fontSize = static_cast<int>(CIniSettings::Instance().GetInt64(L"View", L"FontSize", 11));
    m_scintilla.StyleSetBold(STYLE_DEFAULT, bBold);
    m_scintilla.StyleSetItalic(STYLE_DEFAULT, bItalic);
    m_scintilla.StyleSetSize(STYLE_DEFAULT, fontSize);
    m_scintilla.StyleSetFore(STYLE_DEFAULT, theme.GetThemeColor(RGB(0, 0, 0), true));
    m_scintilla.StyleSetBack(STYLE_DEFAULT, theme.GetThemeColor(RGB(255, 255, 255), true));

    // now call StyleClearAll to copy the default style to all styles
    m_scintilla.StyleClearAll();

    // now set up the out own styles
    SetupDefaultStyles();

    // and now set the lexer styles
    Scintilla::ILexer5* lexer = nullptr;
    m_scintilla.EOLAnnotationClearAll();
    m_scintilla.EOLAnnotationSetVisible(Scintilla::EOLAnnotationVisible::Hidden);

    if (!lexerData.annotations.empty())
        m_scintilla.EOLAnnotationSetVisible(Scintilla::EOLAnnotationVisible::Boxed | Scintilla::EOLAnnotationVisible::AngleCircle);

    if (lexerData.name == "bp_simple")
    {
        lexer = lmSimple.Create();
        lexer->PrivateCall(100, *this);
    }
    if (lexerData.name == "bp_log")
        lexer = lmLog.Create();
    if (lexerData.name == "bp_snippets")
        lexer = lmSnippets.Create();
    if (lexerData.name == "bp_ahk")
        lexer = lmAhk.Create();
    if (lexer == nullptr && lexerData.name.empty())
    {
        switch (lexerData.id)
        {
            case 1100:
                lexer = lmSimple.Create();
                lexer->PrivateCall(100, *this);
                break;
            case 1101:
                lexer = lmLog.Create();
                break;
            case 1102:
                lexer = lmAhk.Create();
                break;
            default:
                break;
        }
    }

    if (lexer == nullptr)
        lexer = CreateLexer(lexerData.name.c_str());
    assert(lexer);
    m_scintilla.SetILexer(lexer);

    for (const auto& [propName, propValue] : lexerData.properties)
    {
        m_scintilla.SetProperty(propName.c_str(), propValue.c_str());
    }
    auto length = m_scintilla.Length();
    m_hugeLevelReached = false;
    if (length > 500 * 1024 * 1024)
    {
        // turn off folding
        m_scintilla.SetProperty("fold", "0");
        m_hugeLevelReached = true;
    }
    for (const auto& [styleId, styleData] : lexerData.styles)
    {
        m_scintilla.StyleSetFore(styleId, theme.GetThemeColor(styleData.foregroundColor, true));
        m_scintilla.StyleSetBack(styleId, theme.GetThemeColor(styleData.backgroundColor, true));
        if (styleId == 0)
        {
            m_scintilla.StyleSetFore(STYLE_DEFAULT, theme.GetThemeColor(styleData.foregroundColor, true));
            m_scintilla.StyleSetBack(STYLE_DEFAULT, theme.GetThemeColor(styleData.backgroundColor, true));
        }
        if (!styleData.fontName.empty())
            m_scintilla.StyleSetFont(styleId, CUnicodeUtils::StdGetUTF8(styleData.fontName).c_str());

        if ((styleData.fontStyle & Fontstyle_Bold) != 0)
            m_scintilla.StyleSetBold(styleId, true);
        if ((styleData.fontStyle & Fontstyle_Italic) != 0)
            m_scintilla.StyleSetItalic(styleId, true);
        if ((styleData.fontStyle & Fontstyle_Underlined) != 0)
            m_scintilla.StyleSetUnderline(styleId, true);

        if (styleData.fontSize)
            m_scintilla.StyleSetSize(styleId, styleData.fontSize);
        if (styleData.eolFilled)
            m_scintilla.StyleSetEOLFilled(styleId, true);
    }
    for (const auto& [keyWordId, keyWord] : keywords)
    {
        m_scintilla.SetKeyWords(keyWordId - 1LL, keyWord.c_str());
    }
    m_scintilla.SetLineEndTypesAllowed(static_cast<Scintilla::LineEndType>(m_scintilla.LineEndTypesSupported()));
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
    m_scintilla.StyleSetFont(STYLE_LINENUMBER, sFontName.c_str());

    m_scintilla.StyleSetFore(STYLE_LINENUMBER, theme.GetThemeColor(RGB(color_linenr_inactive, color_linenr_inactive, color_linenr_inactive), true));
    m_scintilla.StyleSetBack(STYLE_LINENUMBER, theme.GetThemeColor(RGB(230, 230, 230), true));

    m_scintilla.IndicSetStyle(INDIC_SELECTION_MARK, Scintilla::IndicatorStyle::RoundBox);
    m_scintilla.IndicSetAlpha(INDIC_SELECTION_MARK, static_cast<Scintilla::Alpha>(50));
    m_scintilla.IndicSetUnder(INDIC_SELECTION_MARK, true);
    m_scintilla.IndicSetFore(INDIC_SELECTION_MARK, theme.GetThemeColor(RGB(0, 255, 0), true));

    m_scintilla.IndicSetStyle(INDIC_SNIPPETPOS, Scintilla::IndicatorStyle::RoundBox);
    m_scintilla.IndicSetAlpha(INDIC_SNIPPETPOS, static_cast<Scintilla::Alpha>(50));
    m_scintilla.IndicSetOutlineAlpha(INDIC_SNIPPETPOS, Scintilla::Alpha::Opaque);
    m_scintilla.IndicSetUnder(INDIC_SNIPPETPOS, true);
    m_scintilla.IndicSetFore(INDIC_SNIPPETPOS, theme.GetThemeColor(RGB(180, 180, 0), true));

    m_scintilla.IndicSetStyle(INDIC_FINDTEXT_MARK, Scintilla::IndicatorStyle::RoundBox);
    m_scintilla.IndicSetAlpha(INDIC_FINDTEXT_MARK, theme.IsDarkTheme() ? static_cast<Scintilla::Alpha>(100) : static_cast<Scintilla::Alpha>(200));
    m_scintilla.IndicSetUnder(INDIC_FINDTEXT_MARK, true);
    m_scintilla.IndicSetFore(INDIC_FINDTEXT_MARK, theme.GetThemeColor(RGB(255, 255, 0), true));

    m_scintilla.IndicSetStyle(INDIC_URLHOTSPOT, Scintilla::IndicatorStyle::Hidden);
    m_scintilla.IndicSetAlpha(INDIC_URLHOTSPOT, static_cast<Scintilla::Alpha>(5));
    m_scintilla.IndicSetUnder(INDIC_URLHOTSPOT, true);
    m_scintilla.IndicSetHoverStyle(INDIC_URLHOTSPOT, Scintilla::IndicatorStyle::Dots);
    m_scintilla.IndicSetHoverFore(INDIC_URLHOTSPOT, theme.GetThemeColor(RGB(0, 0, 255), true));

    m_scintilla.IndicSetStyle(INDIC_BRACEMATCH, Scintilla::IndicatorStyle::RoundBox);
    m_scintilla.IndicSetAlpha(INDIC_BRACEMATCH, static_cast<Scintilla::Alpha>(30));
    m_scintilla.IndicSetOutlineAlpha(INDIC_BRACEMATCH, Scintilla::Alpha::Transparent);
    m_scintilla.IndicSetUnder(INDIC_BRACEMATCH, true);
    m_scintilla.IndicSetFore(INDIC_BRACEMATCH, theme.GetThemeColor(RGB(0, 150, 0), true));

    m_scintilla.IndicSetStyle(INDIC_MISSPELLED, Scintilla::IndicatorStyle::Squiggle);
    m_scintilla.IndicSetFore(INDIC_MISSPELLED, theme.GetThemeColor(RGB(255, 0, 0), true));
    m_scintilla.IndicSetUnder(INDIC_MISSPELLED, true);
    m_scintilla.IndicSetStyle(INDIC_MISSPELLED_DEL, Scintilla::IndicatorStyle::Squiggle);
    m_scintilla.IndicSetFore(INDIC_MISSPELLED_DEL, theme.GetThemeColor(RGB(100, 255, 0), true));
    m_scintilla.IndicSetUnder(INDIC_MISSPELLED_DEL, true);

    m_scintilla.StyleSetFore(STYLE_BRACELIGHT, theme.GetThemeColor(RGB(0, 150, 0), true));
    m_scintilla.StyleSetBold(STYLE_BRACELIGHT, true);
    m_scintilla.StyleSetFore(STYLE_BRACEBAD, theme.GetThemeColor(RGB(255, 0, 0), true));
    m_scintilla.StyleSetBold(STYLE_BRACEBAD, true);

    m_scintilla.StyleSetFore(STYLE_INDENTGUIDE, theme.GetThemeColor(RGB(200, 200, 200), true));

    m_scintilla.IndicSetFore(INDIC_TAGMATCH, theme.GetThemeColor(RGB(0x80, 0x00, 0xFF), true));
    m_scintilla.IndicSetFore(INDIC_TAGATTR, theme.GetThemeColor(RGB(0xFF, 0xFF, 0x00), true));
    m_scintilla.IndicSetStyle(INDIC_TAGMATCH, Scintilla::IndicatorStyle::RoundBox);
    m_scintilla.IndicSetStyle(INDIC_TAGATTR, Scintilla::IndicatorStyle::RoundBox);
    m_scintilla.IndicSetAlpha(INDIC_TAGMATCH, static_cast<Scintilla::Alpha>(100));
    m_scintilla.IndicSetAlpha(INDIC_TAGATTR, static_cast<Scintilla::Alpha>(100));
    m_scintilla.IndicSetUnder(INDIC_TAGMATCH, true);
    m_scintilla.IndicSetUnder(INDIC_TAGATTR, true);

    m_scintilla.CallTipUseStyle(0);
    m_scintilla.StyleSetFore(STYLE_CALLTIP, theme.GetThemeColor(GetSysColor(COLOR_INFOTEXT)));
    m_scintilla.StyleSetBack(STYLE_CALLTIP, theme.GetThemeColor(GetSysColor(COLOR_INFOBK)));

    if (CIniSettings::Instance().GetInt64(L"View", L"changeHistory", 1) != 0)
    {
        m_scintilla.MarkerSetFore(SC_MARKNUM_HISTORY_REVERTED_TO_ORIGIN, CTheme::Instance().GetThemeColor(0x40A0BF, true));
        m_scintilla.MarkerSetBack(SC_MARKNUM_HISTORY_REVERTED_TO_ORIGIN, CTheme::Instance().GetThemeColor(0x40A0BF, true));

        m_scintilla.MarkerSetFore(SC_MARKNUM_HISTORY_SAVED, CTheme::Instance().GetThemeColor(0x00A000, true));
        m_scintilla.MarkerSetBack(SC_MARKNUM_HISTORY_SAVED, CTheme::Instance().GetThemeColor(0x00A000, true));

        m_scintilla.MarkerSetFore(SC_MARKNUM_HISTORY_MODIFIED, CTheme::Instance().GetThemeColor(0xA0A000, true));
        m_scintilla.MarkerSetBack(SC_MARKNUM_HISTORY_MODIFIED, CTheme::Instance().GetThemeColor(0xA0A000, true));

        m_scintilla.MarkerSetFore(SC_MARKNUM_HISTORY_REVERTED_TO_MODIFIED, CTheme::Instance().GetThemeColor(0xFF8000, true));
        m_scintilla.MarkerSetBack(SC_MARKNUM_HISTORY_REVERTED_TO_MODIFIED, CTheme::Instance().GetThemeColor(0xFF8000, true));
    }

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
        m_scintilla.IndicSetStyle(i, Scintilla::IndicatorStyle::Box);
        m_scintilla.IndicSetFore(i, CTheme::Instance().GetThemeColor(captureColors[i - INDIC_REGEXCAPTURE]));
    }

    for (uptr_t i = INDIC_CUSTOM_MARK_1; i <= INDIC_CUSTOM_MARK_4; ++i)
    {
        m_scintilla.IndicSetStyle(i, Scintilla::IndicatorStyle::RoundBox);
        m_scintilla.IndicSetAlpha(i, theme.IsDarkTheme() ? static_cast<Scintilla::Alpha>(100) : static_cast<Scintilla::Alpha>(200));
        m_scintilla.IndicSetUnder(i, true);
        m_scintilla.IndicSetFore(i, theme.GetThemeColor(customMarkColors[i - INDIC_CUSTOM_MARK_1], true));
    }

    m_scintilla.SetFoldMarginColour(true, theme.GetThemeColor(RGB(240, 240, 240), true));
    m_scintilla.SetFoldMarginHiColour(true, theme.GetThemeColor(RGB(255, 255, 255), true));

    m_scintilla.MarkerSetFore(MARK_BOOKMARK, CTheme::Instance().GetThemeColor(RGB(80, 0, 0), true));
    m_scintilla.MarkerSetBack(MARK_BOOKMARK, CTheme::Instance().GetThemeColor(RGB(255, 0, 0), true));

    m_scintilla.SetIndentationGuides(static_cast<Scintilla::IndentView>(CIniSettings::Instance().GetInt64(L"View", L"indent", SC_IV_LOOKBOTH)));

    SetupFoldingColors(RGB(color_folding_fore_inactive, color_folding_fore_inactive, color_folding_fore_inactive),
                       RGB(color_folding_back_inactive, color_folding_back_inactive, color_folding_back_inactive),
                       RGB(color_folding_backsel_inactive, color_folding_backsel_inactive, color_folding_backsel_inactive));

    bool bBold    = !!CIniSettings::Instance().GetInt64(L"View", L"FontBold", false);
    int  fontSize = static_cast<int>(CIniSettings::Instance().GetInt64(L"View", L"FontSize", 11));
    m_scintilla.StyleSetBold(STYLE_FOLDDISPLAYTEXT, bBold);
    m_scintilla.StyleSetItalic(STYLE_FOLDDISPLAYTEXT, true);
    m_scintilla.StyleSetSize(STYLE_FOLDDISPLAYTEXT, fontSize);
    m_scintilla.StyleSetFore(STYLE_FOLDDISPLAYTEXT, theme.GetThemeColor(RGB(120, 120, 120), true));
    m_scintilla.StyleSetBack(STYLE_FOLDDISPLAYTEXT, theme.GetThemeColor(RGB(230, 230, 230), true));

    m_scintilla.StyleSetFore(STYLE_DEFAULT, theme.GetThemeColor(RGB(0, 0, 0), true));
    m_scintilla.StyleSetBack(STYLE_DEFAULT, theme.GetThemeColor(RGB(255, 255, 255), true));
    m_scintilla.SetElementColour(Scintilla::Element::SelectionText, toRgba(theme.GetThemeColor(RGB(0, 0, 0), true), 128));
    m_scintilla.SetElementColour(Scintilla::Element::SelectionBack, toRgba(theme.GetThemeColor(RGB(51, 153, 255), true), 128));
    m_scintilla.SetElementColour(Scintilla::Element::Caret, toRgba(theme.GetThemeColor(RGB(0, 0, 0), true)));
    m_scintilla.SetElementColour(Scintilla::Element::CaretAdditional, toRgba(theme.GetThemeColor(RGB(0, 0, 80), true)));
    m_scintilla.SetSelectionLayer(Scintilla::Layer::UnderText);
    if (CIniSettings::Instance().GetInt64(L"View", L"caretlineframe", 1) != 0)
    {
        m_scintilla.SetElementColour(Scintilla::Element::CaretLineBack, toRgba(theme.GetThemeColor(RGB(0, 0, 0), true), 80));
        m_scintilla.SetCaretLineLayer(Scintilla::Layer::UnderText);
    }
    else
    {
        if (theme.IsDarkTheme())
        {
            m_scintilla.SetElementColour(Scintilla::Element::CaretLineBack, RGB(0, 0, 0));
        }
        else
        {
            m_scintilla.SetElementColour(Scintilla::Element::CaretLineBack, toRgba(theme.GetThemeColor(RGB(0, 0, 0), true), 25));
        }
    }
    m_scintilla.SetElementColour(Scintilla::Element::WhiteSpace, toRgba(theme.GetThemeColor(GetSysColor(COLOR_WINDOWTEXT)), 80));
    m_scintilla.SetElementColour(Scintilla::Element::FoldLine, toRgba(theme.GetThemeColor(GetSysColor(COLOR_WINDOWTEXT)), 140));

    auto modEventMask = m_scintilla.ModEventMask();
    m_scintilla.SetModEventMask(Scintilla::ModificationFlags::None);

    m_scintilla.Colourise(0, m_scintilla.PositionFromLine(m_scintilla.LinesOnScreen() + 1));
    m_scintilla.SetCodePage(CP_UTF8);
    m_scintilla.SetModEventMask(modEventMask);

    auto controlCharColor = toRgba(theme.IsDarkTheme() ? RGB(255, 255, 240) : RGB(40, 40, 0), 200);
    // set up unicode representations for control chars
    for (int c = 0; c < 0x20; ++c)
    {
        const char sC[2] = {static_cast<char>(c), 0};
        m_scintilla.SetRepresentation(sC, CStringUtils::Format("x%02X", c).c_str());
        m_scintilla.SetRepresentationAppearance(sC, Scintilla::RepresentationAppearance::Blob | Scintilla::RepresentationAppearance::Colour);
        m_scintilla.SetRepresentationColour(sC, controlCharColor);
    }
    for (int c = 0x80; c < 0xA0; ++c)
    {
        const char sC[3] = {'\xc2', static_cast<char>(c), 0};
        m_scintilla.SetRepresentation(sC, CStringUtils::Format("x%02X", c).c_str());
        m_scintilla.SetRepresentationAppearance(sC, Scintilla::RepresentationAppearance::Blob | Scintilla::RepresentationAppearance::Colour | Scintilla::RepresentationAppearance::Blob | Scintilla::RepresentationAppearance::Colour);
        m_scintilla.SetRepresentationColour(sC, controlCharColor);
    }
    m_scintilla.SetRepresentation("\x7F", "x7F");
    m_scintilla.SetRepresentationAppearance("\x7F", Scintilla::RepresentationAppearance::Blob | Scintilla::RepresentationAppearance::Colour | Scintilla::RepresentationAppearance::Blob | Scintilla::RepresentationAppearance::Colour);
    m_scintilla.SetRepresentationColour("\x7F", controlCharColor);

    if (theme.IsDarkTheme())
    {
        m_scintilla.SetElementColour(Scintilla::Element::List, toRgba(RGB(187, 187, 187)));
        m_scintilla.SetElementColour(Scintilla::Element::ListBack, toRgba(RGB(15, 15, 15)));
        m_scintilla.SetElementColour(Scintilla::Element::ListSelected, toRgba(RGB(187, 187, 187)));
        m_scintilla.SetElementColour(Scintilla::Element::ListSelectedBack, toRgba(RGB(80, 80, 80)));
    }
    else
    {
        m_scintilla.ResetElementColour(Scintilla::Element::List);
        m_scintilla.ResetElementColour(Scintilla::Element::ListBack);
        m_scintilla.ResetElementColour(Scintilla::Element::ListSelected);
        m_scintilla.ResetElementColour(Scintilla::Element::ListSelectedBack);
    }
}

void CScintillaWnd::SetupFoldingColors(COLORREF fore, COLORREF back, COLORREF backSel) const
{
    // set the folding colors according to the parameters, but leave the
    // colors for the collapsed buttons (+ button) in the active color.
    auto foldMarkFore       = CTheme::Instance().GetThemeColor(fore, true);
    auto foldMarkForeActive = CTheme::Instance().GetThemeColor(RGB(color_folding_fore_active, color_folding_fore_active, color_folding_fore_active), true);
    m_scintilla.MarkerSetFore(SC_MARKNUM_FOLDEROPEN, foldMarkFore);
    m_scintilla.MarkerSetFore(SC_MARKNUM_FOLDER, foldMarkForeActive);
    m_scintilla.MarkerSetFore(SC_MARKNUM_FOLDERSUB, foldMarkFore);
    m_scintilla.MarkerSetFore(SC_MARKNUM_FOLDERTAIL, foldMarkFore);
    m_scintilla.MarkerSetFore(SC_MARKNUM_FOLDEREND, foldMarkForeActive);
    m_scintilla.MarkerSetFore(SC_MARKNUM_FOLDEROPENMID, foldMarkFore);
    m_scintilla.MarkerSetFore(SC_MARKNUM_FOLDERMIDTAIL, foldMarkFore);

    auto foldMarkBack       = CTheme::Instance().GetThemeColor(back, true);
    auto foldMarkBackActive = CTheme::Instance().GetThemeColor(RGB(color_folding_back_active, color_folding_back_active, color_folding_back_active), true);
    m_scintilla.MarkerSetBack(SC_MARKNUM_FOLDEROPEN, foldMarkBack);
    m_scintilla.MarkerSetBack(SC_MARKNUM_FOLDER, foldMarkBackActive);
    m_scintilla.MarkerSetBack(SC_MARKNUM_FOLDERSUB, foldMarkBack);
    m_scintilla.MarkerSetBack(SC_MARKNUM_FOLDERTAIL, foldMarkBack);
    m_scintilla.MarkerSetBack(SC_MARKNUM_FOLDEREND, foldMarkBackActive);
    m_scintilla.MarkerSetBack(SC_MARKNUM_FOLDEROPENMID, foldMarkBack);
    m_scintilla.MarkerSetBack(SC_MARKNUM_FOLDERMIDTAIL, foldMarkBack);

    auto foldMarkBackSelected       = CTheme::Instance().GetThemeColor(backSel, true);
    auto foldMarkBackSelectedActive = CTheme::Instance().GetThemeColor(RGB(color_folding_backsel_active, color_folding_backsel_active, color_folding_backsel_active), true);
    m_scintilla.MarkerSetBackSelected(SC_MARKNUM_FOLDEROPEN, foldMarkBackSelected);
    m_scintilla.MarkerSetBackSelected(SC_MARKNUM_FOLDER, foldMarkBackSelectedActive);
    m_scintilla.MarkerSetBackSelected(SC_MARKNUM_FOLDERSUB, foldMarkBackSelected);
    m_scintilla.MarkerSetBackSelected(SC_MARKNUM_FOLDERTAIL, foldMarkBackSelected);
    m_scintilla.MarkerSetBackSelected(SC_MARKNUM_FOLDEREND, foldMarkBackSelectedActive);
    m_scintilla.MarkerSetBackSelected(SC_MARKNUM_FOLDEROPENMID, foldMarkBackSelected);
    m_scintilla.MarkerSetBackSelected(SC_MARKNUM_FOLDERMIDTAIL, foldMarkBackSelected);

    // m_scintilla.StyleSetFore(STYLE_FOLDDISPLAYTEXT, foldmarkfore);
    // m_scintilla.StyleSetBack(STYLE_FOLDDISPLAYTEXT, foldmarkback);
}

void CScintillaWnd::GotoLine(sptr_t line)
{
    auto linePos = m_scintilla.PositionFromLine(line);
    Center(linePos, linePos);
}

void CScintillaWnd::Center(sptr_t posStart, sptr_t posEnd)
{
    // to make sure the found result is visible
    // When searching up, the beginning of the (possible multiline) result is important, when scrolling down the end
    auto testPos = (posStart > posEnd) ? posEnd : posStart;
    m_scintilla.SetCurrentPos(testPos);
    auto currentLineNumberDoc = m_scintilla.LineFromPosition(testPos);
    auto currentLineNumberVis = m_scintilla.VisibleFromDocLine(currentLineNumberDoc);
    // EnsureVisible resets the line-wrapping cache, so only
    // call that if it's really necessary.
    if (m_scintilla.LineVisible(currentLineNumberDoc) == 0)
        m_scintilla.EnsureVisible(currentLineNumberDoc); // make sure target line is unfolded

    auto firstVisibleLineVis = m_scintilla.FirstVisibleLine();
    auto linesVisible        = m_scintilla.LinesOnScreen() - 1; //-1 for the scrollbar
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
        m_scintilla.LineScroll(0, linesToScroll);
    }
    // Make sure the caret is visible, scroll horizontally
    m_scintilla.GotoPos(posStart);
    m_scintilla.GotoPos(posEnd);

    m_scintilla.SetAnchor(posStart);
}

void CScintillaWnd::MarginClick(SCNotification* pNotification)
{
    if ((pNotification->margin == SC_MARGE_SYMBOL) && !pNotification->modifiers)
        BookmarkToggle(m_scintilla.LineFromPosition(pNotification->position));
    m_docScroll.VisibleLinesChanged();
}

void CScintillaWnd::SelectionUpdated() const
{
    auto selStart     = m_scintilla.SelectionStart();
    auto selEnd       = m_scintilla.SelectionEnd();
    auto selLineStart = m_scintilla.LineFromPosition(selStart);
    auto selLineEnd   = m_scintilla.LineFromPosition(selEnd);
    if (selLineStart != selLineEnd)
    {
        if ((m_scintilla.FoldLevel(selLineEnd - 1) & Scintilla::FoldLevel::HeaderFlag) != Scintilla::FoldLevel::None)
        {
            // line is a fold header
            if (m_scintilla.LineVisible(selLineEnd) == 0)
            {
                if (m_scintilla.PositionFromLine(selLineEnd) == selEnd)
                {
                    auto lastLine = m_scintilla.LineCount();
                    while ((selLineEnd < lastLine) && (m_scintilla.LineVisible(selLineEnd) == 0))
                        ++selLineEnd;
                    if (selLineEnd < lastLine)
                    {
                        if (m_scintilla.Anchor() == selStart)
                            m_scintilla.SetCurrentPos(m_scintilla.PositionFromLine(selLineEnd));
                        else
                            m_scintilla.SetAnchor(m_scintilla.PositionFromLine(selLineEnd));
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
    auto                  firstLine        = m_scintilla.FirstVisibleLine();
    auto                  lastLine         = firstLine + m_scintilla.LinesOnScreen();
    auto                  startStylePos    = m_scintilla.PositionFromLine(firstLine);
    startStylePos                          = max(startStylePos, 0);
    auto endStylePos                       = m_scintilla.PositionFromLine(lastLine) + m_scintilla.LineLength(lastLine);
    if (endStylePos < 0)
        endStylePos = m_scintilla.Length();

    int len = endStylePos - startStylePos;
    if (len <= 0)
        return;

    // reset indicators
    m_scintilla.SetIndicatorCurrent(INDIC_SELECTION_MARK);
    m_scintilla.IndicatorClearRange(startStylePos, len);

    auto selSpan = m_scintilla.SelectionSpan();
    if (clear || selSpan.Length() == 0 )
    {
        lastSelText.clear();
        m_docScroll.Clear(DOCSCROLLTYPE_SELTEXT);
        m_selTextMarkerCount = 0;
        SendMessage(*this, WM_NCPAINT, static_cast<WPARAM>(1), 0);
        return;
    }
    auto sSelText     = m_scintilla.GetSelText();
    auto selTextLen   = sSelText.size();
    auto origSelStart = m_scintilla.SelectionStart();
    auto origSelEnd   = m_scintilla.SelectionEnd();
    auto selStartLine = m_scintilla.LineFromPosition(origSelStart);
    auto selEndLine   = m_scintilla.LineFromPosition(origSelEnd);
    if (selStartLine != selEndLine)
    {
        lastSelText.clear();
        m_docScroll.Clear(DOCSCROLLTYPE_SELTEXT);
        m_selTextMarkerCount = 0;
        SendMessage(*this, WM_NCPAINT, static_cast<WPARAM>(1), 0);
        return;
    }

    auto selStartPos = m_scintilla.SelectionStart();
    auto selEndPos   = m_scintilla.SelectionEnd();
    bool wholeWord   = (selStartPos == m_scintilla.WordStartPosition(selStartPos, true)) &&
                     (selEndPos == m_scintilla.WordEndPosition(selEndPos, true)) &&
                     (selEndPos == m_scintilla.WordEndPosition(selStartPos, true));
    auto origSelText = sSelText;
    if (origSelText.empty())
    {
        lastSelText.clear();
        m_docScroll.Clear(DOCSCROLLTYPE_SELTEXT);
        m_selTextMarkerCount = 0;
        SendMessage(*this, WM_NCPAINT, static_cast<WPARAM>(1), 0);
        return;
    }
    if (!edit)
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
    if (_stricmp(g_sHighlightString.c_str(), origSelText.c_str()) == 0)
    {
        m_selTextMarkerCount = g_searchMarkerCount;
        return;
    }

    auto          textBuffer = std::make_unique<char[]>(len + 1LL);
    Sci_TextRange textRange{};
    textRange.lpstrText  = textBuffer.get();
    textRange.chrg.cpMin = startStylePos;
    textRange.chrg.cpMax = endStylePos;
    m_scintilla.GetTextRange(&textRange);

    const char* startPos = strstr(textBuffer.get(), origSelText.c_str());
    while (startPos)
    {
        // don't style the selected text itself
        if (selStartPos != static_cast<sptr_t>(startStylePos + (startPos - textBuffer.get())))
            m_scintilla.IndicatorFillRange(startStylePos + (startPos - textBuffer.get()), selTextLen);
        startPos = strstr(startPos + 1, origSelText.c_str());
    }

    auto lineCount = m_scintilla.LineCount();
    if ((selTextLen > 1) || (lineCount < 100000))
    {
        auto selTextDifferent = lastSelText.compare(origSelText.c_str());
        if (selTextDifferent || (lastStopPosition != 0) || edit)
        {
            int  addSelCount = 0;
            auto start       = std::chrono::steady_clock::now();
            if (selTextDifferent)
            {
                m_docScroll.Clear(DOCSCROLLTYPE_SELTEXT);
                m_selTextMarkerCount = 0;
            }
            Scintilla::TextToFindFull findText{};
            findText.chrg.cpMin     = lastStopPosition;
            findText.chrg.cpMax     = m_scintilla.Length();
            findText.lpstrText      = origSelText.c_str();
            lastStopPosition        = 0;
            const auto selTextColor = CTheme::Instance().GetThemeColor(RGB(0, 255, 0), true);
            auto       findOptions  = Scintilla::FindOption::MatchCase;
            if (wholeWord)
                findOptions |= Scintilla::FindOption::WholeWord;
            while (m_scintilla.FindTextFull(findOptions, &findText) >= 0)
            {
                if (edit)
                {
                    if ((origSelStart != findText.chrgText.cpMin) || (origSelEnd != findText.chrgText.cpMax))
                    {
                        m_scintilla.AddSelection(findText.chrgText.cpMax, findText.chrgText.cpMin);
                        ++addSelCount;
                    }
                }
                auto line = m_scintilla.LineFromPosition(findText.chrgText.cpMin);
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
            if (edit && addSelCount > 0)
                m_scintilla.AddSelection(origSelEnd, origSelStart);
            SendMessage(*this, WM_NCPAINT, static_cast<WPARAM>(1), 0);
        }
        lastSelText = origSelText.c_str();
    }
}

void CScintillaWnd::MatchBraces(BraceMatch what)
{
    static sptr_t lastIndicatorStart  = 0;
    static sptr_t lastIndicatorLength = 0;
    static sptr_t lastCaretPos        = 0;

    sptr_t        braceAtCaret        = -1;
    sptr_t        braceOpposite       = -1;

    // find matching brace position
    auto          caretPos            = m_scintilla.CurrentPos();

    // setting the highlighting style triggers an UI update notification,
    // which in return calls MatchBraces(false). So to avoid an endless
    // loop, we bail out if the caret position has not changed.
    if ((what == BraceMatch::Braces) && (caretPos == lastCaretPos))
        return;
    lastCaretPos     = caretPos;

    WCHAR charBefore = '\0';

    auto  lengthDoc  = m_scintilla.Length();

    if ((lengthDoc > 0) && (caretPos > 0))
    {
        charBefore = static_cast<WCHAR>(m_scintilla.CharAt(caretPos - 1));
    }
    // Priority goes to character before the caret
    if (charBefore && wcschr(L"[](){}", charBefore))
    {
        braceAtCaret = caretPos - 1;
    }

    if (lengthDoc > 0 && (braceAtCaret < 0))
    {
        // No brace found so check other side
        WCHAR charAfter = static_cast<WCHAR>(m_scintilla.CharAt(caretPos));
        if (charAfter && wcschr(L"[](){}", charAfter))
        {
            braceAtCaret = caretPos;
        }
    }
    if (braceAtCaret >= 0)
        braceOpposite = m_scintilla.BraceMatch(braceAtCaret, 0);

    KillTimer(*this, TIM_BRACEHIGHLIGHTTEXT);
    KillTimer(*this, TIM_BRACEHIGHLIGHTTEXTCLEAR);
    m_scintilla.SetHighlightGuide(0);
    if ((braceAtCaret != -1) && (braceOpposite == -1))
    {
        m_scintilla.BraceBadLight(braceAtCaret);
        if (lastIndicatorLength && CIniSettings::Instance().GetInt64(L"View", L"bracehighlightbkgnd", 1))
        {
            m_scintilla.SetIndicatorCurrent(INDIC_BRACEMATCH);
            m_scintilla.IndicatorClearRange(lastIndicatorStart, lastIndicatorLength);
            lastIndicatorLength = 0;
        }
    }
    else
    {
        m_scintilla.BraceHighlight(braceAtCaret, braceOpposite);
        if (CIniSettings::Instance().GetInt64(L"View", L"bracehighlightbkgnd", 1))
        {
            if (lastIndicatorLength)
            {
                m_scintilla.SetIndicatorCurrent(INDIC_BRACEMATCH);
                m_scintilla.IndicatorClearRange(lastIndicatorStart, lastIndicatorLength);
            }
            if (what == BraceMatch::Highlight)
            {
                m_scintilla.SetIndicatorCurrent(INDIC_BRACEMATCH);
                lastIndicatorStart  = braceAtCaret < braceOpposite ? braceAtCaret : braceOpposite;
                lastIndicatorLength = braceAtCaret < braceOpposite ? braceOpposite - braceAtCaret : braceAtCaret - braceOpposite;
                ++lastIndicatorLength;
                auto startLine = m_scintilla.LineFromPosition(lastIndicatorStart);
                auto endLine   = m_scintilla.LineFromPosition(lastIndicatorStart + lastIndicatorLength);
                if (endLine != startLine)
                    m_scintilla.IndicSetAlpha(INDIC_BRACEMATCH, CTheme::Instance().IsDarkTheme() ? static_cast<Scintilla::Alpha>(3) : static_cast<Scintilla::Alpha>(10));
                else
                    m_scintilla.IndicSetAlpha(INDIC_BRACEMATCH, static_cast<Scintilla::Alpha>(40));

                m_scintilla.IndicatorFillRange(lastIndicatorStart, lastIndicatorLength);
                SetTimer(*this, TIM_BRACEHIGHLIGHTTEXTCLEAR, 5000, nullptr);
            }
            else if (what == BraceMatch::Braces)
                SetTimer(*this, TIM_BRACEHIGHLIGHTTEXT, 1000, nullptr);
        }

        if ((braceAtCaret >= 0) && (braceOpposite >= 0) && (what == BraceMatch::Highlight) &&
            (m_scintilla.IndentationGuides() != Scintilla::IndentView::None))
        {
            auto columnAtCaret  = m_scintilla.Column(braceAtCaret);
            auto columnOpposite = m_scintilla.Column(braceOpposite);
            m_scintilla.SetHighlightGuide((columnAtCaret < columnOpposite) ? columnAtCaret : columnOpposite);
        }
    }
}

// find matching brace position
void CScintillaWnd::GotoBrace() const
{
    static constexpr wchar_t brackets[] = L"[](){}";

    auto                     lengthDoc  = m_scintilla.Length();
    if (lengthDoc <= 1)
        return;

    sptr_t braceAtCaret  = -1;
    sptr_t braceOpposite = -1;
    WCHAR  charBefore    = '\0';
    WCHAR  charAfter     = '\0';
    bool   shift         = (GetKeyState(VK_SHIFT) & 0x8000) != 0;

    auto   caretPos      = m_scintilla.CurrentPos();
    if (caretPos > 0)
    {
        charBefore = static_cast<WCHAR>(m_scintilla.CharAt(caretPos - 1));
        if (charBefore && wcschr(brackets, charBefore)) // Priority goes to character before the caret.
            braceAtCaret = caretPos - 1;
    }

    if (braceAtCaret < 0) // No brace found so check other side
    {
        charAfter = static_cast<WCHAR>(m_scintilla.CharAt(caretPos));
        if (charAfter && wcschr(brackets, charAfter))
            braceAtCaret = caretPos;
    }
    if (braceAtCaret >= 0)
    {
        braceOpposite = m_scintilla.BraceMatch(braceAtCaret, 0);
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
        m_scintilla.SetSel(shift ? braceAtCaret : braceOpposite, braceOpposite);
    }
}

void CScintillaWnd::MatchTags() const
{
    // basically the same as MatchBraces(), but much more complicated since
    // finding xml tags is harder than just finding braces...

    sptr_t docStart = 0;
    sptr_t docEnd   = m_scintilla.Length();
    m_scintilla.SetIndicatorCurrent(INDIC_TAGMATCH);
    m_scintilla.IndicatorClearRange(docStart, docEnd - docStart);
    m_scintilla.SetIndicatorCurrent(INDIC_TAGATTR);
    m_scintilla.IndicatorClearRange(docStart, docEnd - docStart);

    int lexer = static_cast<int>(m_scintilla.Lexer());
    if ((lexer != SCLEX_HTML) &&
        (lexer != SCLEX_XML) &&
        (lexer != SCLEX_PHPSCRIPT))
        return;

    // Get the original targets and search options to restore after tag matching operation
    auto              originalStartPos    = m_scintilla.TargetStart();
    auto              originalEndPos      = m_scintilla.TargetEnd();
    auto              originalSearchFlags = m_scintilla.SearchFlags();

    XmlMatchedTagsPos xmlTags             = {0};

    // Detect if it's a xml/html tag
    if (GetXmlMatchedTagsPos(xmlTags))
    {
        m_scintilla.SetIndicatorCurrent(INDIC_TAGMATCH);
        int openTagTailLen = 2;

        // Coloring the close tag first
        if ((xmlTags.tagCloseStart != -1) && (xmlTags.tagCloseEnd != -1))
        {
            m_scintilla.IndicatorFillRange(xmlTags.tagCloseStart, xmlTags.tagCloseEnd - xmlTags.tagCloseStart);
            // tag close is present, so it's not single tag
            openTagTailLen = 1;
        }

        // Coloring the open tag
        m_scintilla.IndicatorFillRange(xmlTags.tagOpenStart, xmlTags.tagNameEnd - xmlTags.tagOpenStart);
        m_scintilla.IndicatorFillRange(xmlTags.tagOpenEnd - openTagTailLen, openTagTailLen);

        // Coloring its attributes
        auto attributes = GetAttributesPos(xmlTags.tagNameEnd, xmlTags.tagOpenEnd - openTagTailLen);
        m_scintilla.SetIndicatorCurrent(INDIC_TAGATTR);
        for (const auto& [startPos, endPos] : attributes)
        {
            m_scintilla.IndicatorFillRange(startPos, endPos - startPos);
        }

        // Coloring indent guide line position
        if (m_scintilla.IndentationGuides() != Scintilla::IndentView::None)
        {
            auto columnAtCaret  = m_scintilla.Column(xmlTags.tagOpenStart);
            auto columnOpposite = m_scintilla.Column(xmlTags.tagCloseStart);

            auto lineAtCaret    = m_scintilla.LineFromPosition(xmlTags.tagOpenStart);
            auto lineOpposite   = m_scintilla.LineFromPosition(xmlTags.tagCloseStart);

            if (xmlTags.tagCloseStart != -1 && lineAtCaret != lineOpposite)
            {
                m_scintilla.BraceHighlight(xmlTags.tagOpenStart, xmlTags.tagCloseEnd - 1);
                m_scintilla.SetHighlightGuide((columnAtCaret < columnOpposite) ? columnAtCaret : columnOpposite);
            }
        }
    }

    // restore the original targets and search options to avoid the conflict with search/replace function
    m_scintilla.SetTargetStart(originalStartPos);
    m_scintilla.SetTargetEnd(originalEndPos);
    m_scintilla.SetSearchFlags(originalSearchFlags);
}

bool CScintillaWnd::GetSelectedCount(sptr_t& selByte, sptr_t& selLine) const
{
    auto selCount = m_scintilla.Selections();
    selByte       = 0;
    selLine       = 0;
    for (decltype(selCount) i = 0; i < selCount; ++i)
    {
        auto start = m_scintilla.SelectionNStart(i);
        auto end   = m_scintilla.SelectionNEnd(i);
        selByte += (start < end) ? end - start : start - end;

        start = m_scintilla.LineFromPosition(start);
        end   = m_scintilla.LineFromPosition(end);
        selLine += (start < end) ? end - start : start - end;
        ++selLine;
    }

    return true;
};

LRESULT CALLBACK CScintillaWnd::HandleScrollbarCustomDraw(WPARAM wParam, NMCSBCUSTOMDRAW* pCustomDraw)
{
    m_docScroll.SetCurrentPos(m_scintilla.VisibleFromDocLine(GetCurrentLineNumber()), CTheme::Instance().GetThemeColor(RGB(40, 40, 40), true));
    m_docScroll.SetTotalLines(m_scintilla.LineCount());
    return m_docScroll.HandleCustomDraw(wParam, pCustomDraw);
}

bool CScintillaWnd::GetXmlMatchedTagsPos(XmlMatchedTagsPos& xmlTags) const
{
    bool       tagFound         = false;
    auto       caret            = m_scintilla.CurrentPos();
    auto       searchStartPoint = caret;
    sptr_t     styleAt          = 0;
    FindResult openFound{};

    // Search back for the previous open angle bracket.
    // Keep looking whilst the angle bracket found is inside an XML attribute
    do
    {
        openFound        = FindText("<", searchStartPoint, 0, Scintilla::FindOption::None);
        styleAt          = m_scintilla.StyleAt(openFound.start);
        searchStartPoint = openFound.start - 1;
    } while (openFound.success && (styleAt == SCE_H_DOUBLESTRING || styleAt == SCE_H_SINGLESTRING) && searchStartPoint > 0);

    if (openFound.success && styleAt != SCE_H_CDATA)
    {
        // Found the "<" before the caret, now check there isn't a > between that position and the caret.
        FindResult closeFound{};
        searchStartPoint = openFound.start;
        do
        {
            closeFound       = FindText(">", searchStartPoint, caret, Scintilla::FindOption::None);
            styleAt          = m_scintilla.StyleAt(closeFound.start);
            searchStartPoint = closeFound.end;
        } while (closeFound.success && (styleAt == SCE_H_DOUBLESTRING || styleAt == SCE_H_SINGLESTRING) && searchStartPoint <= caret);

        if (!closeFound.success)
        {
            // We're in a tag (either a start tag or an end tag)
            auto nextChar = m_scintilla.CharAt(openFound.start + 1);

            /////////////////////////////////////////////////////////////////////////
            // CURSOR IN CLOSE TAG
            /////////////////////////////////////////////////////////////////////////
            if ('/' == nextChar)
            {
                xmlTags.tagCloseStart = openFound.start;
                auto docLength        = m_scintilla.Length();
                auto endCloseTag      = FindText(">", caret, docLength, Scintilla::FindOption::None);
                if (endCloseTag.success)
                {
                    xmlTags.tagCloseEnd = endCloseTag.end;
                }
                // Now find the tagName
                auto        position = openFound.start + 2;

                // UTF-8 or ASCII tag name
                std::string tagName;
                nextChar = m_scintilla.CharAt(position);
                // Checking for " or ' is actually wrong here, but it means it works better with invalid XML
                while (position < docLength && !IsXMLWhitespace(nextChar) && nextChar != '/' && nextChar != '>' && nextChar != '\"' && nextChar != '\'')
                {
                    tagName.push_back(static_cast<char>(nextChar));
                    ++position;
                    nextChar = m_scintilla.CharAt(position);
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
                auto position        = openFound.start + 1;
                auto docLength       = m_scintilla.Length();

                xmlTags.tagOpenStart = openFound.start;

                std::string tagName;
                nextChar = m_scintilla.CharAt(position);
                // Checking for " or ' is actually wrong here, but it means it works better with invalid XML
                while (position < docLength && !IsXMLWhitespace(nextChar) && nextChar != '/' && nextChar != '>' && nextChar != '\"' && nextChar != '\'')
                {
                    tagName.push_back(static_cast<char>(nextChar));
                    ++position;
                    nextChar = m_scintilla.CharAt(position);
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
                        if (m_scintilla.CharAt(closeAnglePosition - 1) == '/')
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

FindResult CScintillaWnd::FindText(const char* text, sptr_t start, sptr_t end, Scintilla::FindOption flags) const
{
    FindResult                returnValue = {0};

    Scintilla::TextToFindFull search{};
    search.lpstrText  = const_cast<char*>(text);
    search.chrg.cpMin = start;
    search.chrg.cpMax = end;
    auto result       = m_scintilla.FindTextFull(flags, &search);
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
    Scintilla::TextToFindFull ttf = {0};
    ttf.chrg.cpMin                = startPos;
    ttf.chrg.cpMax                = endPos;
    ttf.lpstrText                 = const_cast<char*>(toFind.c_str());
    return m_scintilla.FindTextFull(Scintilla::FindOption::None, &ttf);
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
        result = FindText(search.c_str(), searchStart, searchEnd, Scintilla::FindOption::None);
        if (result.success)
        {
            nextChar = static_cast<int>(m_scintilla.CharAt(result.end));
            styleAt  = m_scintilla.StyleAt(result.start);
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
                    if (-1 != closeAnglePosition && '/' != m_scintilla.CharAt(closeAnglePosition - 1))
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

    bool       isValidClose   = false;
    sptr_t     returnPosition = -1;

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

        closeAngle   = FindText(">", startPosition, endPosition, Scintilla::FindOption::None);
        if (closeAngle.success)
        {
            int style = static_cast<int>(m_scintilla.StyleAt(closeAngle.start));
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
        result        = FindText(search.c_str(), searchStart, searchEnd, Scintilla::FindOption::None);
        if (result.success)
        {
            nextChar = static_cast<int>(m_scintilla.CharAt(result.end));
            styleAt  = m_scintilla.StyleAt(result.start);

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
                        nextChar = static_cast<int>(m_scintilla.CharAt(whitespacePoint));

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

    auto                                   bufLen = end - start + 1;
    auto                                   buf    = std::make_unique<char[]>(bufLen + 1);
    Sci_TextRange                          tr{};
    tr.chrg.cpMin = start;
    tr.chrg.cpMax = end;
    tr.lpstrText  = buf.get();
    m_scintilla.GetTextRange(&tr);

    enum class AttrStates
    {
        Attr_Invalid,
        Attr_Key,
        Attr_Pre_Assign,
        Attr_Assign,
        Attr_String,
        Attr_Value,
        Attr_Valid
    } state            = AttrStates::Attr_Invalid;

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
    auto lexer = m_scintilla.Lexer();
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
        if (m_scintilla.Selections() > 1)
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
            default:
                break;
        }

        // Get Selection
        bool   bSelEmpty      = !!m_scintilla.SelectionEmpty();
        sptr_t lineStartStart = 0;
        sptr_t lineEndEnd     = 0;
        if (!bSelEmpty && braceCloseBuf[0])
        {
            auto selStart  = m_scintilla.SelectionStart();
            auto selEnd    = m_scintilla.SelectionEnd();
            auto lineStart = m_scintilla.LineFromPosition(selStart);
            auto lineEnd   = m_scintilla.LineFromPosition(selEnd);
            if (m_scintilla.PositionFromLine(lineEnd) == selEnd)
            {
                --lineEnd;
                selEnd = m_scintilla.LineEndPosition(lineEnd);
            }
            lineStartStart = m_scintilla.PositionFromLine(lineStart);
            lineEndEnd     = m_scintilla.LineEndPosition(lineEnd);
            if ((lineStartStart != selStart) || (lineEndEnd != selEnd) || (wParam == '(') || (wParam == '['))
            {
                // insert the brace before the selection and the closing brace after the selection
                m_scintilla.SetSel(static_cast<uptr_t>(-1), selStart);
                m_scintilla.BeginUndoAction();
                m_scintilla.InsertText(selStart, braceBuf);
                m_scintilla.InsertText(selEnd + 1, braceCloseBuf);
                m_scintilla.SetSel(selStart + 1, selStart + 1);
                m_scintilla.EndUndoAction();
                return true;
            }
            else
            {
                // get info
                auto tabIndent         = m_scintilla.TabWidth();
                auto indentAmount      = m_scintilla.LineIndentation(lineStart > 0 ? lineStart - 1 : lineStart);
                auto indentAmountFirst = m_scintilla.LineIndentation(lineStart);
                if (indentAmount == 0)
                    indentAmount = indentAmountFirst;
                m_scintilla.BeginUndoAction();

                // insert a new line at the end of the selected lines
                m_scintilla.SetSel(lineEndEnd, lineEndEnd);
                m_scintilla.NewLine();
                // now insert the end-brace and indent it
                m_scintilla.InsertText(static_cast<uptr_t>(-1), braceCloseBuf);
                m_scintilla.SetLineIndentation(lineEnd + 1, indentAmount);

                m_scintilla.SetSel(lineStartStart, lineStartStart);
                // now insert the start-brace and a NewLine after it
                m_scintilla.InsertText(static_cast<uptr_t>(-1), braceBuf);
                m_scintilla.SetSel(lineStartStart + 1, lineStartStart + 1);
                m_scintilla.NewLine();
                // now indent the line with the start brace
                m_scintilla.SetLineIndentation(lineStart, indentAmount);

                // increase the indentation of all selected lines
                if (indentAmount == indentAmountFirst)
                {
                    for (sptr_t line = lineStart + 1; line <= lineEnd + 1; ++line)
                    {
                        m_scintilla.SetLineIndentation(line, m_scintilla.LineIndentation(line) + tabIndent);
                    }
                }
                m_scintilla.EndUndoAction();
                return true;
            }
        }
        else
        {
            if (wParam == '{')
            {
                // Auto-add the closing brace, then the closing brace
                m_scintilla.BeginUndoAction();
                // insert the opening brace first
                m_scintilla.AddText(1, braceBuf);
                // insert the closing brace
                m_scintilla.AddText(1, braceCloseBuf);
                // go back one char
                m_scintilla.CharLeft();
                m_scintilla.EndUndoAction();
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
                auto visLineToScrollTo = m_scintilla.VisibleFromDocLine(m_lineToScrollToAfterPaint);
                visLineToScrollTo += m_wrapOffsetToScrollToAfterPaint;
                m_scintilla.SetFirstVisibleLine(visLineToScrollTo);
                --m_lineToScrollToAfterPaintCounter;
                if ((m_lineToScrollToAfterPaintCounter <= 0) || ((m_scintilla.FirstVisibleLine() + 2) > m_scintilla.VisibleFromDocLine(m_lineToScrollToAfterPaint)))
                {
                    m_lineToScrollToAfterPaint        = -1;
                    m_wrapOffsetToScrollToAfterPaint  = 0;
                    m_lineToScrollToAfterPaintCounter = 0;
                }
                UpdateLineNumberWidth();
            }
            break;
        case SCN_SAVEPOINTREACHED:
            EnableChangeHistory();
            break;
            case SCN_MODIFIED:
            if (pScn->modificationType & SC_MOD_INSERTTEXT)
            {
                if (!m_hugeLevelReached && m_scintilla.Length() > 500 * 1024 * 1024)
                {
                    // turn off folding
                    m_scintilla.SetProperty("fold", "0");
                    m_hugeLevelReached = true;
                }
            }
        default:
            break;
    }
}

void CScintillaWnd::SetTabSettings(TabSpace ts) const
{
    m_scintilla.SetTabWidth(static_cast<uptr_t>(CIniSettings::Instance().GetInt64(L"View", L"tabsize", 4)));
    if (ts == TabSpace::Default)
        m_scintilla.SetUseTabs(static_cast<uptr_t>(CIniSettings::Instance().GetInt64(L"View", L"usetabs", 1)));
    else
        m_scintilla.SetUseTabs(ts == TabSpace::Tabs ? 1 : 0);
    m_scintilla.SetBackSpaceUnIndents(true);
    m_scintilla.SetTabIndents(true);
    m_scintilla.SetTabDrawMode(Scintilla::TabDrawMode::StrikeOut);
}

void CScintillaWnd::SetReadDirection(Scintilla::Bidirectional rd) const
{
    // auto ex = GetWindowLongPtr(*this, GWL_EXSTYLE);
    // if (rd != R2L)
    //     ex &= WS_EX_LAYOUTRTL/*WS_EX_RTLREADING*/;
    // else
    //     ex |= WS_EX_LAYOUTRTL/*WS_EX_RTLREADING*/;
    // SetWindowLongPtr(*this, GWL_EXSTYLE, ex);
    m_scintilla.SetBidirectional(rd);
}

void CScintillaWnd::BookmarkAdd(sptr_t lineNo)
{
    if (lineNo == -1)
        lineNo = GetCurrentLineNumber();
    if (!IsBookmarkPresent(lineNo))
    {
        m_scintilla.MarkerAdd(lineNo, MARK_BOOKMARK);
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
        m_scintilla.MarkerDelete(lineNo, MARK_BOOKMARK);
        m_docScroll.RemoveLine(DOCSCROLLTYPE_BOOKMARK, lineNo);
        DocScrollUpdate();
    }
}

bool CScintillaWnd::IsBookmarkPresent(sptr_t lineNo) const
{
    if (lineNo == -1)
        lineNo = GetCurrentLineNumber();
    LRESULT state = m_scintilla.MarkerGet(lineNo);
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
        line = m_scintilla.MarkerNext(line + 1, (1 << MARK_BOOKMARK));
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

void CScintillaWnd::SetEOLType(Scintilla::EndOfLine eolType) const
{
    m_scintilla.SetEOLMode(eolType);
}

void CScintillaWnd::AppendText(sptr_t len, const char* buf) const
{
    m_scintilla.AppendText(len, buf);
}

std::string CScintillaWnd::GetLine(sptr_t line) const
{
    return m_scintilla.GetLine(line);
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
    m_scintilla.GetTextRange(&rangeStart);
    return strBuf.get();
}

std::string CScintillaWnd::GetSelectedText(SelectionHandling handling) const
{
    auto selText = m_scintilla.GetSelText();
    if (selText.empty() && handling != SelectionHandling::None)
    {
        selText = GetCurrentWord(handling == SelectionHandling::CurrentWordIfSelectionIsEmptyAndSelect);
    }
    return selText;
}

std::string CScintillaWnd::GetCurrentWord(bool select) const
{
    auto currentPos = m_scintilla.CurrentPos();
    auto startPos   = m_scintilla.WordStartPosition(currentPos, true);
    auto endPos     = m_scintilla.WordEndPosition(currentPos, true);
    if (select)
        m_scintilla.SetSelection(startPos, endPos);
    return GetTextRange(startPos, endPos);
}

std::string CScintillaWnd::GetCurrentLine() const
{
    auto lineLen = m_scintilla.GetCurLine(0, nullptr);
    return m_scintilla.GetCurLine(lineLen);
}

std::string CScintillaWnd::GetWordChars() const
{
    return m_scintilla.WordChars();
}

std::string CScintillaWnd::GetWhitespaceChars() const
{
    return m_scintilla.WhitespaceChars();
}

sptr_t CScintillaWnd::GetCurrentLineNumber() const
{
    return m_scintilla.LineFromPosition(m_scintilla.CurrentPos());
}

void CScintillaWnd::EnableChangeHistory() const
{
    Scintilla::ChangeHistoryOption historyOption = Scintilla::ChangeHistoryOption::Disabled;
    int                            width         = 0;
    if (CIniSettings::Instance().GetInt64(L"View", L"changeHistory", 1) != 0)
    {
        historyOption = static_cast<Scintilla::ChangeHistoryOption>(static_cast<int>(Scintilla::ChangeHistoryOption::Enabled) |
                                                                    static_cast<int>(Scintilla::ChangeHistoryOption::Markers));
        width         = CDPIAware::Instance().Scale(*this, 2);
    }

    auto symbols = (1 << SC_MARKNUM_HISTORY_REVERTED_TO_ORIGIN) | (1 << SC_MARKNUM_HISTORY_SAVED) | (1 << SC_MARKNUM_HISTORY_MODIFIED) | (1 << SC_MARKNUM_HISTORY_REVERTED_TO_MODIFIED);
    m_scintilla.SetMarginMaskN(SC_MARGE_HISTORY, symbols);
    m_scintilla.SetMarginWidthN(SC_MARGE_HISTORY, width);
    m_scintilla.SetChangeHistory(historyOption);
}
