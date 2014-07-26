// This file is part of BowPad.
//
// Copyright (C) 2014 - Stefan Kueng
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

std::string ClipboardBase::GetHtmlSelection()
{
    if (!HasActiveDocument())
        return "";
    CDocument doc = GetActiveDocument();
    auto lexerdata = CLexStyles::Instance().GetLexerDataForLang(CUnicodeUtils::StdGetUTF8(doc.m_language));

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
        fore = lexerdata.Styles[0].ForegroundColor;
        back = lexerdata.Styles[0].BackgroundColor;
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
                    fore = lexerdata.Styles[s].ForegroundColor;
                    back = lexerdata.Styles[s].BackgroundColor;
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


bool CCmdCut::Execute()
{
    std::string sHtml = GetHtmlSelection();
    ScintillaCall(SCI_CUT);
    AddHtmlStringToClipboard(sHtml);
    return true;
}

void CCmdCut::ScintillaNotify(Scintilla::SCNotification * pScn)
{
    if (pScn->nmhdr.code == SCN_UPDATEUI)
        InvalidateUICommand(UI_INVALIDATIONS_STATE, NULL);
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
    return true;
}

void CCmdCutPlain::ScintillaNotify(Scintilla::SCNotification * pScn)
{
    if (pScn->nmhdr.code == SCN_UPDATEUI)
        InvalidateUICommand(UI_INVALIDATIONS_STATE, NULL);
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
    std::string sHtml = GetHtmlSelection();
    ScintillaCall(SCI_COPYALLOWLINE);
    AddHtmlStringToClipboard(sHtml);
    return true;
}


bool CCmdCopyPlain::Execute()
{
    ScintillaCall(SCI_COPYALLOWLINE);
    return true;
}

bool CCmdPaste::Execute()
{
    // test first if there's a file on the clipboard
    std::vector<std::wstring> files;
    bool bFilesOpened = false;
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
                    int filesDropped = DragQueryFile(hDrop, 0xffffffff, NULL, 0);
                    for (int i = 0 ; i < filesDropped ; ++i)
                    {
                        UINT len = DragQueryFile(hDrop, i, NULL, 0);
                        std::unique_ptr<wchar_t[]> pathBuf(new wchar_t[len+1]);
                        DragQueryFile(hDrop, i, pathBuf.get(), len+1);
                        files.push_back(pathBuf.get());
                    }
                }
            }
        }
    }
    for (const auto& file : files)
    {
        OpenFile(file.c_str(), true);
        bFilesOpened = true;
    }
    if (!bFilesOpened)
        ScintillaCall(SCI_PASTE);
    return true;
}

void CCmdPaste::ScintillaNotify(Scintilla::SCNotification * pScn)
{
    if (pScn->nmhdr.code == SCN_MODIFIED)
        InvalidateUICommand(UI_INVALIDATIONS_STATE, NULL);
}

HRESULT CCmdPaste::IUICommandHandlerUpdateProperty(REFPROPERTYKEY key, const PROPVARIANT* /*ppropvarCurrentValue*/, PROPVARIANT* ppropvarNewValue)
{
    if (UI_PKEY_Enabled == key)
    {
        return UIInitPropertyFromBoolean(UI_PKEY_Enabled, (ScintillaCall(SCI_CANPASTE) != 0), ppropvarNewValue);
    }
    return E_NOTIMPL;
}
