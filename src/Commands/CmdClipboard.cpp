// This file is part of BowPad.
//
// Copyright (C) 2014-2022 - Stefan Kueng
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
#include "CmdClipboard.h"

#include "StringUtils.h"
#include "Theme.h"
#include "LexStyles.h"
#include "ClipboardHelper.h"
#include "OnOutOfScope.h"
#include <stdexcept>

static constexpr wchar_t CF_BPLEXER[] = {L"BP Lexer"};
static auto              CF_HTML      = RegisterClipboardFormat(L"HTML Format");

std::string              ClipboardBase::GetHtmlSelection() const
{
    if (!HasActiveDocument())
        return "";
    const auto& doc       = GetActiveDocument();
    const auto& lexerData = CLexStyles::Instance().GetLexerDataForLang(doc.GetLanguage());

    std::string sHtmlFragment;
    int         style       = 0;

    char        fontBuf[40] = {0};
    Scintilla().StyleGetFont(0, fontBuf);
    int      fontSize   = static_cast<int>(Scintilla().StyleGetSize(0));
    bool     bold       = !!Scintilla().StyleGetBold(0);
    bool     italic     = !!Scintilla().StyleGetItalic(0);
    bool     underlined = !!Scintilla().StyleGetUnderline(0);
    COLORREF fore       = static_cast<COLORREF>(Scintilla().StyleGetFore(0));
    COLORREF back       = static_cast<COLORREF>(Scintilla().StyleGetBack(0));
    if (CTheme::Instance().IsDarkTheme())
    {
        try
        {
            fore = lexerData.styles.at(0).foregroundColor;
        }
        catch (const std::out_of_range&)
        {
        }
        try
        {
            back = lexerData.styles.at(0).backgroundColor;
        }
        catch (const std::out_of_range&)
        {
        }
    }
    std::string styleHtml = CStringUtils::Format("<pre style=\"font-family:%s;font-size:%dpt;font-weight:%s;font-style:%s;text-decoration:%s;color:#%06x;background:#%06x;\">",
                                                 fontBuf, fontSize, bold ? "bold" : "normal", italic ? "italic" : "normal", underlined ? "underline" : "none",
                                                 GetRValue(fore) << 16 | GetGValue(fore) << 8 | GetBValue(fore),
                                                 GetRValue(back) << 16 | GetGValue(back) << 8 | GetBValue(back));
    sHtmlFragment += styleHtml;

    int  numSelections = static_cast<int>(Scintilla().Selections());
    bool spanSet       = false;
    for (int i = 0; i < numSelections; ++i)
    {
        int selStart = static_cast<int>(Scintilla().SelectionNStart(i));
        int selEnd   = static_cast<int>(Scintilla().SelectionNEnd(i));

        if ((selStart == selEnd) && (numSelections == 1))
        {
            auto curLine = GetCurrentLineNumber();
            selStart     = static_cast<int>(Scintilla().PositionFromLine(curLine));
            selEnd       = static_cast<int>(Scintilla().LineEndPosition(curLine));
        }

        int p = selStart;
        do
        {
            int s = static_cast<int>(Scintilla().StyleAt(p));
            if (s != style)
            {
                if (spanSet)
                    sHtmlFragment += "</span>";
                Scintilla().StyleGetFont(s, fontBuf);
                fontSize   = static_cast<int>(Scintilla().StyleGetSize(s));
                bold       = !!Scintilla().StyleGetBold(s);
                italic     = !!Scintilla().StyleGetItalic(s);
                underlined = !!Scintilla().StyleGetUnderline(s);
                fore       = static_cast<COLORREF>(Scintilla().StyleGetFore(s));
                back       = static_cast<COLORREF>(Scintilla().StyleGetBack(s));
                if (CTheme::Instance().IsDarkTheme())
                {
                    try
                    {
                        fore = lexerData.styles.at(s).foregroundColor;
                    }
                    catch (const std::out_of_range&)
                    {
                    }
                    try
                    {
                        back = lexerData.styles.at(s).backgroundColor;
                    }
                    catch (const std::out_of_range&)
                    {
                    }
                }
                styleHtml = CStringUtils::Format("<span style=\"font-family:%s;font-size:%dpt;font-weight:%s;font-style:%s;text-decoration:%s;color:#%06x;background:#%06x;\">",
                                                 fontBuf, fontSize, bold ? "bold" : "normal", italic ? "italic" : "normal", underlined ? "underline" : "none",
                                                 GetRValue(fore) << 16 | GetGValue(fore) << 8 | GetBValue(fore),
                                                 GetRValue(back) << 16 | GetGValue(back) << 8 | GetBValue(back));
                sHtmlFragment += styleHtml;
                style   = s;
                spanSet = true;
            }
            char        c = static_cast<char>(Scintilla().CharAt(p));
            std::string cs;
            cs += c;
            switch (c)
            {
                case ' ':
                    cs = "&nbsp;";
                    break;
                case '"':
                    cs = "&quot;";
                    break;
                case '<':
                    cs = "&lt;";
                    break;
                case '>':
                    cs = "&gt;";
                    break;
                default:
                    break;
            }
            sHtmlFragment += cs;
            ++p;
        } while (p < selEnd);
        sHtmlFragment += "\r\n";
    }
    sHtmlFragment += "</span></pre>";

    std::string header    = "Version:0.9\r\nStartHTML:<<<<<<<1\r\nEndHTML:<<<<<<<2\r\nStartFragment:<<<<<<<3\r\nEndFragment:<<<<<<<4\r\nStartSelection:<<<<<<<3\r\nEndSelection:<<<<<<<3\r\n";
    std::string pre       = "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.0 Transitional//EN\">\r\n<HTML><HEAD></HEAD>\r\n<BODY>\r\n<!--StartFragment-->";
    std::string post      = "<!--EndFragment--></BODY></HTML>";

    std::string sHtml     = header;
    int         startHtml = static_cast<int>(sHtml.length());
    sHtml += pre;
    int startFragment = static_cast<int>(sHtml.length());
    sHtml += sHtmlFragment;
    int endFragment = static_cast<int>(sHtml.length());
    sHtml += post;
    int endHtml = static_cast<int>(sHtml.length());

    // replace back offsets
    SearchReplace(sHtml, "<<<<<<<1", CStringUtils::Format("%08d", startHtml));
    SearchReplace(sHtml, "<<<<<<<2", CStringUtils::Format("%08d", endHtml));
    SearchReplace(sHtml, "<<<<<<<3", CStringUtils::Format("%08d", startFragment));
    SearchReplace(sHtml, "<<<<<<<4", CStringUtils::Format("%08d", endFragment));

    return sHtml;
}

void ClipboardBase::AddHtmlStringToClipboard(const std::string& sHtml) const
{
    CClipboardHelper clipBoard;
    if (clipBoard.Open(GetScintillaWnd()))
    {
        size_t  sLen           = sHtml.length();
        HGLOBAL hClipboardData = GlobalAlloc(GMEM_MOVEABLE, (sLen + 1) * sizeof(char));
        if (hClipboardData)
        {
            char* pchData = static_cast<char*>(GlobalLock(hClipboardData));
            if (pchData)
            {
                strcpy_s(pchData, sLen + 1, sHtml.c_str());
                GlobalUnlock(hClipboardData);
                SetClipboardData(CF_HTML, hClipboardData);
            }
        }
    }
}

void ClipboardBase::AddLexerToClipboard() const
{
    const auto& doc  = GetActiveDocument();
    const auto& lang = doc.GetLanguage();
    if (!lang.empty())
    {
        CClipboardHelper clipBoard;
        if (clipBoard.Open(GetScintillaWnd()))
        {
            auto    sLen           = lang.length();
            HGLOBAL hClipboardData = GlobalAlloc(GMEM_MOVEABLE, (sLen + 1) * sizeof(char));
            if (hClipboardData)
            {
                char* pchData = static_cast<char*>(GlobalLock(hClipboardData));
                if (pchData)
                {
                    strcpy_s(pchData, sLen + 1, lang.c_str());
                    GlobalUnlock(hClipboardData);
                    auto CF_LEXER = RegisterClipboardFormat(CF_BPLEXER);
                    SetClipboardData(CF_LEXER, hClipboardData);
                }
            }
        }
    }
}

void ClipboardBase::SetLexerFromClipboard() const
{
    auto& doc = GetModActiveDocument();
    if (doc.GetLanguage().empty() || (doc.GetLanguage().compare("Text") == 0))
    {
        CClipboardHelper clipboard;
        if (clipboard.Open(GetScintillaWnd()))
        {
            auto   CF_LEXER = RegisterClipboardFormat(CF_BPLEXER);

            HANDLE hData    = GetClipboardData(CF_LEXER);
            if (hData)
            {
                LPCSTR lptstr = static_cast<LPCSTR>(GlobalLock(hData));
                if (lptstr != nullptr)
                {
                    OnOutOfScope(
                        GlobalUnlock(hData););
                    auto lang = lptstr;
                    SetupLexerForLang(lang);
                    doc.SetLanguage(lang);
                    UpdateStatusBar(true);
                }
            }
        }
    }
}

bool CCmdCut::Execute()
{
    Scintilla().Cut();
    bool bShift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
    if (bShift)
    {
        std::string sHtml = GetHtmlSelection();
        AddHtmlStringToClipboard(sHtml);
    }
    AddLexerToClipboard();
    return true;
}

void CCmdCut::ScintillaNotify(SCNotification* pScn)
{
    if (pScn->nmhdr.code == SCN_UPDATEUI)
        InvalidateUICommand(UI_INVALIDATIONS_STATE, nullptr);
}

bool CCmdCutPlain::Execute()
{
    bool bEmpty = Scintilla().SelectionEmpty() != 0;
    if (bEmpty)
        Scintilla().LineCut();
    else
        Scintilla().Cut();
    AddLexerToClipboard();
    return true;
}

void CCmdCutPlain::ScintillaNotify(SCNotification* pScn)
{
    if (pScn->nmhdr.code == SCN_UPDATEUI)
        InvalidateUICommand(UI_INVALIDATIONS_STATE, nullptr);
}

bool CCmdCopy::Execute()
{
    Scintilla().CopyAllowLine();
    bool bShift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
    if (bShift)
    {
        std::string sHtml = GetHtmlSelection();
        AddHtmlStringToClipboard(sHtml);
    }
    AddLexerToClipboard();
    return true;
}

bool CCmdCopyPlain::Execute()
{
    Scintilla().CopyAllowLine();
    AddLexerToClipboard();
    return true;
}

bool CCmdPaste::Execute()
{
    // test first if there's a file on the clipboard
    {
        CClipboardHelper clipboard;
        if (clipboard.Open(GetScintillaWnd()))
        {
            HANDLE hData = GetClipboardData(CF_HDROP);
            if (hData)
            {
                HDROP hDrop = static_cast<HDROP>(hData);
                OpenHDROP(hDrop);
            }
        }
    }
    Scintilla().Paste();
    SetLexerFromClipboard();
    return true;
}

void CCmdPaste::OnClipboardChanged()
{
    InvalidateUICommand(UI_INVALIDATIONS_STATE, nullptr);
}

HRESULT CCmdPaste::IUICommandHandlerUpdateProperty(REFPROPERTYKEY key, const PROPVARIANT* /*pPropVarCurrentValue*/, PROPVARIANT* pPropVarNewValue)
{
    if (UI_PKEY_Enabled == key)
    {
        return UIInitPropertyFromBoolean(UI_PKEY_Enabled, (Scintilla().CanPaste() != 0), pPropVarNewValue);
    }
    return E_NOTIMPL;
}

bool CCmdPasteHtml::Execute()
{
    // test first if there's a file on the clipboard
    {
        CClipboardHelper clipboard;
        if (clipboard.Open(GetScintillaWnd()))
        {
            HANDLE hData = GetClipboardData(CF_HTML);
            if (hData)
            {
                char*       buffer = static_cast<char*>(GlobalLock(hData));
                std::string str(buffer);
                ::GlobalUnlock(hData);
                std::string baseUrl;
                size_t      htmlStart = 0;
                size_t      fragStart = 0;
                size_t      fragEnd   = 0;
                HtmlExtractMetadata(str, &baseUrl, &htmlStart, &fragStart, &fragEnd);
                auto htmlStr = str.substr(fragStart, fragEnd - fragStart);
                Scintilla().AddText(htmlStr.size(), htmlStr.c_str());
            }
        }
    }
    return true;
}

void CCmdPasteHtml::OnClipboardChanged()
{
    InvalidateUICommand(UI_INVALIDATIONS_STATE, nullptr);
}

HRESULT CCmdPasteHtml::IUICommandHandlerUpdateProperty(REFPROPERTYKEY key, const PROPVARIANT* /*pPropVarCurrentValue*/, PROPVARIANT* pPropVarNewValue)
{
    if (UI_PKEY_Enabled == key)
    {
        return UIInitPropertyFromBoolean(UI_PKEY_Enabled, IsClipboardFormatAvailable(CF_HTML), pPropVarNewValue);
    }
    return E_NOTIMPL;
}

void CCmdPasteHtml::HtmlExtractMetadata(const std::string& cfHtml,
                                        std::string*       baseURL,
                                        size_t*            htmlStart,
                                        size_t*            fragmentStart,
                                        size_t*            fragmentEnd) const
{
    // Obtain base_url if present.
    if (baseURL)
    {
        static std::string srcURLStr("SourceURL:");
        auto               lineStart = cfHtml.find(srcURLStr);
        if (lineStart != std::string::npos)
        {
            auto srcEnd   = cfHtml.find("\n", lineStart);
            auto srcStart = lineStart + srcURLStr.length();
            if (srcEnd != std::string::npos && srcStart != std::string::npos)
            {
                *baseURL = cfHtml.substr(srcStart, srcEnd - srcStart);
                *baseURL = CStringUtils::trim(*baseURL);
            }
        }
    }

    // Find the markup between "<!--StartFragment-->" and "<!--EndFragment-->".
    // If the comments cannot be found, like copying from OpenOffice Writer,
    // we simply fall back to using StartFragment/EndFragment bytecount values
    // to determine the fragment indexes.
    std::string cfHtmlLower = cfHtml;
    std::transform(cfHtmlLower.begin(), cfHtmlLower.end(), cfHtmlLower.begin(), [](char c) { return static_cast<char>(::tolower(c)); });

    auto markupStart = cfHtmlLower.find("<html", 0);
    if (htmlStart)
    {
        *htmlStart = markupStart;
    }
    auto tagStart = cfHtml.find("<!--StartFragment", markupStart);
    if (tagStart == std::string::npos)
    {
        static std::string startFragmentStr("StartFragment:");
        auto               startFragmentStart = cfHtml.find(startFragmentStr);
        if (startFragmentStart != std::string::npos)
        {
            *fragmentStart = static_cast<size_t>(atoi(cfHtml.c_str() +
                                                      startFragmentStart + startFragmentStr.length()));
        }

        static std::string endFragmentStr("EndFragment:");
        auto               endFragmentStart = cfHtml.find(endFragmentStr);
        if (endFragmentStart != std::string::npos)
        {
            *fragmentEnd = static_cast<size_t>(atoi(cfHtml.c_str() +
                                                    endFragmentStart + endFragmentStr.length()));
        }
    }
    else
    {
        *fragmentStart = cfHtml.find('>', tagStart) + 1;
        auto tagEnd    = cfHtml.rfind("<!--EndFragment", std::string::npos);
        *fragmentEnd   = cfHtml.rfind('<', tagEnd);
    }
}
