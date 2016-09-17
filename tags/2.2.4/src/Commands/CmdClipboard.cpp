// This file is part of BowPad.
//
// Copyright (C) 2014-2016 - Stefan Kueng
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
#include "UnicodeUtils.h"
#include "Theme.h"
#include "LexStyles.h"
#include "ClipboardHelper.h"
#include "OnOutOfScope.h"

static const wchar_t CF_BPLEXER[] = { L"BP Lexer" };

std::string ClipboardBase::GetHtmlSelection()
{
    if (!HasActiveDocument())
        return "";
    CDocument doc = GetActiveDocument();
    const auto& lexerdata = CLexStyles::Instance().GetLexerDataForLang(CUnicodeUtils::StdGetUTF8(doc.m_language));

    std::string sHtmlFragment;
    int style = 0;

    char fontbuf[40] = {0};
    ScintillaCall(SCI_STYLEGETFONT, 0, (sptr_t)fontbuf);
    int fontSize = (int)ScintillaCall(SCI_STYLEGETSIZE, 0);
    bool bold = !!ScintillaCall(SCI_STYLEGETBOLD, 0);
    bool italic = !!ScintillaCall(SCI_STYLEGETITALIC, 0);
    bool underlined = !!ScintillaCall(SCI_STYLEGETUNDERLINE, 0);
    COLORREF fore = (COLORREF)ScintillaCall(SCI_STYLEGETFORE, 0);
    COLORREF back = (COLORREF)ScintillaCall(SCI_STYLEGETBACK, 0);
    if (CTheme::Instance().IsDarkTheme())
    {
        try { fore = lexerdata.Styles.at(0).ForegroundColor; }
        catch (const std::out_of_range&) {}
        try { back = lexerdata.Styles.at(0).BackgroundColor; }
        catch (const std::out_of_range&) {}
    }
    std::string stylehtml = CStringUtils::Format("<pre style=\"font-family:%s;font-size:%dpt;font-weight:%s;font-style:%s;text-decoration:%s;color:#%06x;background:#%06x;\">",
        fontbuf, fontSize, bold ? "bold" : "normal", italic ? "italic" : "normal", underlined ? "underline" : "none",
        GetRValue(fore)<<16 | GetGValue(fore)<<8 | GetBValue(fore),
        GetRValue(back)<<16 | GetGValue(back)<<8 | GetBValue(back));
    sHtmlFragment += stylehtml;


    int numSelections = (int)ScintillaCall(SCI_GETSELECTIONS);
    bool spanset = false;
    for (int i = 0; i < numSelections; ++i)
    {
        int selStart = (int)ScintillaCall(SCI_GETSELECTIONNSTART, i);
        int selEnd   = (int)ScintillaCall(SCI_GETSELECTIONNEND, i);

        if ((selStart == selEnd) && (numSelections == 1))
        {
            int curLine = (int)ScintillaCall(SCI_LINEFROMPOSITION, ScintillaCall(SCI_GETCURRENTPOS));
            selStart = (int)ScintillaCall(SCI_POSITIONFROMLINE, curLine);
            selEnd = (int)ScintillaCall(SCI_GETLINEENDPOSITION, curLine);
        }

        int p = selStart;
        do
        {
            int s = (int)ScintillaCall(SCI_GETSTYLEAT, p);
            if (s != style)
            {
                if (spanset)
                    sHtmlFragment += "</span>";
                ScintillaCall(SCI_STYLEGETFONT, s, (sptr_t)fontbuf);
                fontSize = (int)ScintillaCall(SCI_STYLEGETSIZE, s);
                bold = !!ScintillaCall(SCI_STYLEGETBOLD, s);
                italic = !!ScintillaCall(SCI_STYLEGETITALIC, s);
                underlined = !!ScintillaCall(SCI_STYLEGETUNDERLINE, s);
                fore = (COLORREF)ScintillaCall(SCI_STYLEGETFORE, s);
                back = (COLORREF)ScintillaCall(SCI_STYLEGETBACK, s);
                if (CTheme::Instance().IsDarkTheme())
                {
                    try { fore = lexerdata.Styles.at(s).ForegroundColor; }
                    catch (const std::out_of_range&) {}
                    try { back = lexerdata.Styles.at(s).BackgroundColor; }
                    catch (const std::out_of_range&) {}
                }
                stylehtml = CStringUtils::Format("<span style=\"font-family:%s;font-size:%dpt;font-weight:%s;font-style:%s;text-decoration:%s;color:#%06x;background:#%06x;\">",
                    fontbuf, fontSize, bold ? "bold" : "normal", italic ? "italic" : "normal", underlined ? "underline" : "none",
                    GetRValue(fore)<<16 | GetGValue(fore)<<8 | GetBValue(fore),
                    GetRValue(back)<<16 | GetGValue(back)<<8 | GetBValue(back));
                sHtmlFragment += stylehtml;
                style = s;
                spanset = true;
            }
            char c = (char)ScintillaCall(SCI_GETCHARAT, p);
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
            }
            sHtmlFragment += cs;
            ++p;
        } while (p < selEnd);
        sHtmlFragment += "\r\n";
    }
    sHtmlFragment += "</span></pre>";

    std::string header = "Version:0.9\r\nStartHTML:<<<<<<<1\r\nEndHTML:<<<<<<<2\r\nStartFragment:<<<<<<<3\r\nEndFragment:<<<<<<<4\r\nStartSelection:<<<<<<<3\r\nEndSelection:<<<<<<<3\r\n";
    std::string pre = "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.0 Transitional//EN\">\r\n<HTML><HEAD></HEAD>\r\n<BODY>\r\n<!--StartFragment-->";
    std::string post = "<!--EndFragment--></BODY></HTML>";

    std::string sHtml = header;
    int startHtml = (int)sHtml.length();
    sHtml += pre;
    int startFragment = (int)sHtml.length();
    sHtml += sHtmlFragment;
    int endFragment = (int)sHtml.length();
    sHtml += post;
    int endHtml = (int)sHtml.length();

    // replace back offsets
    SearchReplace(sHtml, "<<<<<<<1", CStringUtils::Format("%08d", startHtml));
    SearchReplace(sHtml, "<<<<<<<2", CStringUtils::Format("%08d", endHtml));
    SearchReplace(sHtml, "<<<<<<<3", CStringUtils::Format("%08d", startFragment));
    SearchReplace(sHtml, "<<<<<<<4", CStringUtils::Format("%08d", endFragment));

    return sHtml;
}

void ClipboardBase::AddHtmlStringToClipboard(const std::string& sHtml)
{
    if (OpenClipboard(GetHwnd()))
    {
        HGLOBAL hClipboardData;
        size_t sLen = sHtml.length();
        hClipboardData = GlobalAlloc(GMEM_DDESHARE, (sLen+1)*sizeof(char));
        if (hClipboardData)
        {
            char * pchData;
            pchData = (char*)GlobalLock(hClipboardData);
            if (pchData)
            {
                strcpy_s(pchData, sLen+1, sHtml.c_str());
                if (GlobalUnlock(hClipboardData))
                {
                    UINT CF_HTML = RegisterClipboardFormat(L"HTML Format");
                    SetClipboardData(CF_HTML, hClipboardData);
                }
            }
        }
        CloseClipboard();
    }
}

void ClipboardBase::AddLexerToClipboard()
{
    CDocument doc = GetActiveDocument();
    std::string lang = CUnicodeUtils::StdGetUTF8(doc.m_language);
    if (!lang.empty())
    {
        if (OpenClipboard(GetHwnd()))
        {
            HGLOBAL hClipboardData;
            size_t sLen = lang.length();
            hClipboardData = GlobalAlloc(GMEM_DDESHARE, (sLen + 1)*sizeof(char));
            if (hClipboardData)
            {
                char * pchData;
                pchData = (char*)GlobalLock(hClipboardData);
                if (pchData)
                {
                    strcpy_s(pchData, sLen + 1, lang.c_str());
                    if (GlobalUnlock(hClipboardData))
                    {
                        UINT CF_LEXER = RegisterClipboardFormat(CF_BPLEXER);
                        SetClipboardData(CF_LEXER, hClipboardData);
                    }
                }
            }
            CloseClipboard();
        }
    }
}

void ClipboardBase::SetLexerFromClipboard()
{
    CDocument doc = GetActiveDocument();
    if (doc.m_language.empty() || (doc.m_language.compare(L"Text") == 0))
    {
        CClipboardHelper clipboard;
        if (clipboard.Open(GetHwnd()))
        {
            UINT CF_LEXER = RegisterClipboardFormat(CF_BPLEXER);

            HANDLE hData = GetClipboardData(CF_LEXER);
            if (hData)
            {
                LPCSTR lptstr = (LPCSTR)GlobalLock(hData);
                if (lptstr != nullptr)
                {
                    OnOutOfScope(
                        GlobalUnlock(hData);
                    );
                    auto lang = CUnicodeUtils::StdGetUnicode(lptstr);
                    doc.m_language = lang;
                    SetDocument(GetDocIdOfCurrentTab(), doc);
                    SetupLexerForLang(lang);
                    UpdateStatusBar(true);
                }
            }
        }
    }
}


bool CCmdCut::Execute()
{
    bool bShift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
    std::string sHtml = GetHtmlSelection();
    ScintillaCall(SCI_CUT);
    if (bShift)
        AddHtmlStringToClipboard(sHtml);
    AddLexerToClipboard();
    return true;
}

void CCmdCut::ScintillaNotify(Scintilla::SCNotification * pScn)
{
    if (pScn->nmhdr.code == SCN_UPDATEUI)
        InvalidateUICommand(UI_INVALIDATIONS_STATE, nullptr);
}

HRESULT CCmdCut::IUICommandHandlerUpdateProperty(REFPROPERTYKEY key, const PROPVARIANT* /*ppropvarCurrentValue*/, PROPVARIANT* ppropvarNewValue)
{
    if (UI_PKEY_Enabled == key)
    {
        return UIInitPropertyFromBoolean(UI_PKEY_Enabled, (ScintillaCall(SCI_GETSELTEXT) > 1), ppropvarNewValue);
    }
    return E_NOTIMPL;
}

bool CCmdCutPlain::Execute()
{
    ScintillaCall(SCI_CUT);
    AddLexerToClipboard();
    return true;
}

void CCmdCutPlain::ScintillaNotify(Scintilla::SCNotification * pScn)
{
    if (pScn->nmhdr.code == SCN_UPDATEUI)
        InvalidateUICommand(UI_INVALIDATIONS_STATE, nullptr);
}

HRESULT CCmdCutPlain::IUICommandHandlerUpdateProperty(REFPROPERTYKEY key, const PROPVARIANT* /*ppropvarCurrentValue*/, PROPVARIANT* ppropvarNewValue)
{
    if (UI_PKEY_Enabled == key)
    {
        return UIInitPropertyFromBoolean(UI_PKEY_Enabled, (ScintillaCall(SCI_GETSELTEXT) > 1), ppropvarNewValue);
    }
    return E_NOTIMPL;
}


bool CCmdCopy::Execute()
{
    bool bShift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
    std::string sHtml = GetHtmlSelection();
    ScintillaCall(SCI_COPYALLOWLINE);
    if (bShift)
        AddHtmlStringToClipboard(sHtml);
    AddLexerToClipboard();
    return true;
}


bool CCmdCopyPlain::Execute()
{
    ScintillaCall(SCI_COPYALLOWLINE);
    AddLexerToClipboard();
    return true;
}

bool CCmdPaste::Execute()
{
    // test first if there's a file on the clipboard
    {
        CClipboardHelper clipboard;
        if (clipboard.Open(GetHwnd()))
        {
            HANDLE hData = GetClipboardData(CF_HDROP);
            if (hData)
            {
                HDROP hDrop = reinterpret_cast<HDROP>(hData);
                if (hDrop)
                {
                    OpenHDROP(hDrop);
                }
            }
        }
    }
    ScintillaCall(SCI_PASTE);
    SetLexerFromClipboard();
    return true;
}

void CCmdPaste::ScintillaNotify(Scintilla::SCNotification * pScn)
{
    if (pScn->nmhdr.code == SCN_MODIFIED)
        InvalidateUICommand(UI_INVALIDATIONS_STATE, nullptr);
}

HRESULT CCmdPaste::IUICommandHandlerUpdateProperty(REFPROPERTYKEY key, const PROPVARIANT* /*ppropvarCurrentValue*/, PROPVARIANT* ppropvarNewValue)
{
    if (UI_PKEY_Enabled == key)
    {
        return UIInitPropertyFromBoolean(UI_PKEY_Enabled, (ScintillaCall(SCI_CANPASTE) != 0), ppropvarNewValue);
    }
    return E_NOTIMPL;
}
