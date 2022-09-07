// This file is part of BowPad.
//
// Copyright (C) 2013-2016, 2019, 2021-2022 - Stefan Kueng
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
#include "CmdComment.h"
#include "LexStyles.h"

bool CCmdComment::Execute()
{
    // Get Selection
    bool bSelEmpty    = !!Scintilla().SelectionEmpty();
    bool bForceStream = false;
    auto selStart     = Scintilla().SelectionStart();
    auto selEnd       = Scintilla().SelectionEnd();
    auto lineStart    = Scintilla().LineFromPosition(selStart);
    auto lineEnd      = Scintilla().LineFromPosition(selEnd);
    auto curPos       = Scintilla().CurrentPos();
    if (!bSelEmpty)
    {
        if (Scintilla().PositionFromLine(lineEnd) == selEnd)
        {
            --lineEnd;
            selEnd = Scintilla().LineEndPosition(lineEnd);
        }

        auto lineStartStart = Scintilla().PositionFromLine(Scintilla().LineFromPosition(selStart));
        auto lineEndEnd     = Scintilla().LineEndPosition(Scintilla().LineFromPosition(selEnd));
        bForceStream        = (lineStartStart != selStart) || (lineEndEnd != selEnd);
    }
    else
    {
        selStart = Scintilla().LineIndentPosition(Scintilla().LineFromPosition(curPos));
        selEnd   = Scintilla().LineEndPosition(Scintilla().LineFromPosition(curPos));
    }
    if (!HasActiveDocument())
        return false;
    const auto& doc                = GetActiveDocument();
    const auto& lang               = doc.GetLanguage();
    std::string commentLine        = CLexStyles::Instance().GetCommentLineForLang(lang);
    std::string commentStreamStart = CLexStyles::Instance().GetCommentStreamStartForLang(lang);
    std::string commentStreamEnd   = CLexStyles::Instance().GetCommentStreamEndForLang(lang);
    bool        commentLineAtStart = CLexStyles::Instance().GetCommentLineAtStartForLang(lang);

    if (commentLine.empty())
        bForceStream = true;

    if (!bForceStream)
    {
        // insert a block comment, i.e. a comment marker at the beginning of each line
        // first find the leftmost indent of all lines
        sptr_t indent = INTPTR_MAX;
        for (auto line = lineStart; line <= lineEnd; ++line)
        {
            indent = min(Scintilla().LineIndentation(line), indent);
        }
        // now insert the comment marker at the leftmost indentation on every line
        Scintilla().BeginUndoAction();
        size_t insertedChars = 0;
        for (auto line = lineStart; line <= lineEnd; ++line)
        {
            auto pos = Scintilla().PositionFromLine(line);
            if (!commentLineAtStart)
            {
                int tabsize = static_cast<int>(Scintilla().TabWidth());
                for (decltype(indent) i = 0; i < indent;)
                {
                    char c = static_cast<char>(Scintilla().CharAt(pos));
                    if (c == '\t')
                        i += tabsize;
                    else
                        i += 1;
                    pos += 1;
                }
            }
            Scintilla().InsertText(pos, commentLine.c_str());
            insertedChars += commentLine.length();
        }
        Scintilla().EndUndoAction();
        if (!bSelEmpty)
            Scintilla().SetSel(selStart, selEnd + insertedChars);
        else
            Scintilla().SetSel(curPos + insertedChars, curPos + insertedChars);
    }
    else if (!commentStreamStart.empty())
    {
        // insert a stream comment
        Scintilla().BeginUndoAction();
        Scintilla().InsertText(selStart, commentStreamStart.c_str());
        Scintilla().InsertText(selEnd + commentStreamStart.length(), commentStreamEnd.c_str());
        Scintilla().EndUndoAction();
        if (!bSelEmpty)
            Scintilla().SetSel(selStart + commentStreamStart.length(), selEnd + commentStreamStart.length());
        else
            Scintilla().SetSel(curPos + commentStreamStart.length(), curPos + commentStreamStart.length());
    }

    return true;
}

bool CCmdUnComment::Execute()
{
    // Get Selection
    bool   bSelEmpty      = !!Scintilla().SelectionEmpty();
    sptr_t selStart       = 0;
    sptr_t selEnd         = 0;
    sptr_t lineStartStart = 0;

    if (!HasActiveDocument())
        return false;
    const auto& doc                = GetActiveDocument();
    const auto& lang               = doc.GetLanguage();
    std::string commentLine        = CLexStyles::Instance().GetCommentLineForLang(lang);
    std::string commentstreamstart = CLexStyles::Instance().GetCommentStreamStartForLang(lang);
    std::string commentstreamend   = CLexStyles::Instance().GetCommentStreamEndForLang(lang);

    if (!bSelEmpty)
    {
        selStart          = Scintilla().SelectionStart();
        selEnd            = Scintilla().SelectionEnd();
        size_t selEndCorr = 0;
        // go back if selEnd is on an EOL char
        switch (Scintilla().CharAt(selEnd))
        {
            case '\r':
            case '\n':
                --selEnd;
                ++selEndCorr;
                break;
            default:
                break;
        }
        switch (Scintilla().CharAt(selEnd))
        {
            case '\r':
            case '\n':
                --selEnd;
                ++selEndCorr;
                break;
            default:
                break;
        }

        auto          strBuf = std::make_unique<char[]>(commentstreamstart.size() + commentstreamend.size() + 5);
        Sci_TextRange rangeStart{};
        rangeStart.chrg.cpMin = static_cast<Sci_PositionCR>(selStart - commentstreamstart.length());
        rangeStart.chrg.cpMax = static_cast<Sci_PositionCR>(selStart);
        rangeStart.lpstrText  = strBuf.get();
        Scintilla().GetTextRange(&rangeStart);
        bool bRemovedStream = false;
        if (!commentstreamstart.empty() && _stricmp(commentstreamstart.c_str(), strBuf.get()) == 0)
        {
            // find the end marker
            Sci_TextRange rangeEnd{};
            rangeEnd.chrg.cpMin = static_cast<Sci_PositionCR>(selEnd);
            rangeEnd.chrg.cpMax = static_cast<Sci_PositionCR>(selEnd + commentstreamend.length());
            rangeEnd.lpstrText  = strBuf.get();
            Scintilla().GetTextRange(&rangeEnd);
            if (_stricmp(commentstreamend.c_str(), strBuf.get()) == 0)
            {
                Scintilla().BeginUndoAction();
                // remove the stream start marker
                Scintilla().SetSel(rangeStart.chrg.cpMin, rangeStart.chrg.cpMax);
                Scintilla().ReplaceSel("");
                // remove the end start marker
                Scintilla().SetSel(rangeEnd.chrg.cpMin - commentstreamstart.length(), rangeEnd.chrg.cpMax - commentstreamstart.length());
                Scintilla().ReplaceSel("");
                Scintilla().EndUndoAction();
                Scintilla().SetSel(selStart - commentstreamstart.length(), selEnd - commentstreamstart.length() + selEndCorr);
                bRemovedStream = true;
            }
        }
        if (!bRemovedStream)
        {
            lineStartStart = Scintilla().PositionFromLine(Scintilla().LineFromPosition(selStart));
            if (lineStartStart == selStart)
            {
                // remove block comments for each selected line
                auto lineStart = Scintilla().LineFromPosition(selStart);
                auto lineEnd   = Scintilla().LineFromPosition(selEnd);
                Scintilla().BeginUndoAction();
                size_t removedChars = 0;
                for (auto line = lineStart; line <= lineEnd; ++line)
                {
                    auto          pos = Scintilla().LineIndentPosition(line);
                    Sci_TextRange range{};
                    range.chrg.cpMin = static_cast<Sci_PositionCR>(pos);
                    range.chrg.cpMax = static_cast<Sci_PositionCR>(pos + commentLine.length());
                    range.lpstrText  = strBuf.get();
                    Scintilla().GetTextRange(&range);
                    if (_stricmp(commentLine.c_str(), strBuf.get()) == 0)
                    {
                        Scintilla().SetSel(range.chrg.cpMin, range.chrg.cpMax);
                        Scintilla().ReplaceSel("");
                        removedChars += commentLine.length();
                    }
                }
                Scintilla().EndUndoAction();
                Scintilla().SetSel(selStart, selEnd - removedChars + selEndCorr);
            }
        }
    }
    else
    {
        // remove block comment for the current line
        auto strBuf = std::make_unique<char[]>(commentLine.size() + 5);
        auto curPos = Scintilla().CurrentPos();
        Scintilla().BeginUndoAction();
        auto          pos = Scintilla().LineIndentPosition(Scintilla().LineFromPosition(curPos));
        Sci_TextRange range{};
        range.chrg.cpMin = static_cast<Sci_PositionCR>(pos);
        range.chrg.cpMax = static_cast<Sci_PositionCR>(pos + commentLine.length());
        range.lpstrText  = strBuf.get();
        Scintilla().GetTextRange(&range);
        if (!commentLine.empty() && (_stricmp(commentLine.c_str(), strBuf.get()) == 0))
        {
            Scintilla().SetSel(range.chrg.cpMin, range.chrg.cpMax);
            Scintilla().ReplaceSel("");
            auto newCurPos = curPos > static_cast<sptr_t>(commentLine.length()) ? curPos - commentLine.length() : 0;
            Scintilla().SetSel(newCurPos, newCurPos);
        }
        else if (!commentstreamstart.empty() && !commentstreamend.empty())
        {
            // try to find a stream comment the current position is maybe in, and if yes remove the stream comment

            Sci_TextToFind ttf = {0};
            ttf.chrg.cpMin     = static_cast<Sci_PositionCR>(curPos);
            if (ttf.chrg.cpMin > static_cast<Sci_PositionCR>(commentstreamstart.length()))
                ttf.chrg.cpMin--;
            ttf.chrg.cpMax = 0;
            ttf.lpstrText  = const_cast<char*>(commentstreamstart.c_str());
            auto findRet   = Scintilla().FindText(Scintilla::FindOption::None, &ttf);
            if (findRet >= 0)
            {
                ttf.lpstrText = const_cast<char*>(commentstreamend.c_str());
                auto findRet2 = Scintilla().FindText(Scintilla::FindOption::None, &ttf);
                // test if the start marker is found later than the end marker: if not, we found an independent comment outside of the current position
                if (findRet2 < findRet)
                {
                    // now find the end marker
                    ttf.chrg.cpMin = static_cast<Sci_PositionCR>(curPos);
                    ttf.chrg.cpMax = static_cast<Sci_PositionCR>(Scintilla().Length());
                    ttf.lpstrText  = const_cast<char*>(commentstreamend.c_str());
                    auto findRet3  = Scintilla().FindText(Scintilla::FindOption::None, &ttf);
                    if (findRet3 >= 0)
                    {
                        Scintilla().SetSel(findRet3, findRet3 + commentstreamend.length());
                        Scintilla().ReplaceSel("");
                        Scintilla().SetSel(findRet, findRet + commentstreamstart.length());
                        Scintilla().ReplaceSel("");
                        Scintilla().SetSel(curPos - commentstreamstart.length(), curPos - commentstreamstart.length());
                    }
                }
            }
        }
        Scintilla().EndUndoAction();
    }

    return true;
}
