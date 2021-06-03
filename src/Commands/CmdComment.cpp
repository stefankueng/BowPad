// This file is part of BowPad.
//
// Copyright (C) 2013-2016, 2019, 2021 - Stefan Kueng
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
    bool   bSelEmpty    = !!ScintillaCall(SCI_GETSELECTIONEMPTY);
    bool   bForceStream = false;
    sptr_t selStart     = ScintillaCall(SCI_GETSELECTIONSTART);
    sptr_t selEnd       = ScintillaCall(SCI_GETSELECTIONEND);
    sptr_t lineStart    = ScintillaCall(SCI_LINEFROMPOSITION, selStart);
    sptr_t lineEnd      = ScintillaCall(SCI_LINEFROMPOSITION, selEnd);
    sptr_t curPos       = ScintillaCall(SCI_GETCURRENTPOS);
    if (!bSelEmpty)
    {
        if (ScintillaCall(SCI_POSITIONFROMLINE, lineEnd) == selEnd)
        {
            --lineEnd;
            selEnd = ScintillaCall(SCI_GETLINEENDPOSITION, lineEnd);
        }

        auto lineStartStart = ScintillaCall(SCI_POSITIONFROMLINE, ScintillaCall(SCI_LINEFROMPOSITION, selStart));
        auto lineEndEnd     = ScintillaCall(SCI_GETLINEENDPOSITION, ScintillaCall(SCI_LINEFROMPOSITION, selEnd));
        bForceStream        = (lineStartStart != selStart) || (lineEndEnd != selEnd);
    }
    else
    {
        selStart = ScintillaCall(SCI_GETLINEINDENTPOSITION, ScintillaCall(SCI_LINEFROMPOSITION, curPos));
        selEnd   = ScintillaCall(SCI_GETLINEENDPOSITION, ScintillaCall(SCI_LINEFROMPOSITION, curPos));
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
            indent = min(ScintillaCall(SCI_GETLINEINDENTATION, line), indent);
        }
        // now insert the comment marker at the leftmost indentation on every line
        ScintillaCall(SCI_BEGINUNDOACTION);
        size_t insertedChars = 0;
        for (auto line = lineStart; line <= lineEnd; ++line)
        {
            size_t pos = ScintillaCall(SCI_POSITIONFROMLINE, line);
            if (!commentLineAtStart)
            {
                int tabsize = static_cast<int>(ScintillaCall(SCI_GETTABWIDTH));
                for (decltype(indent) i = 0; i < indent;)
                {
                    char c = static_cast<char>(ScintillaCall(SCI_GETCHARAT, pos));
                    if (c == '\t')
                        i += tabsize;
                    else
                        i += 1;
                    pos += 1;
                }
            }
            ScintillaCall(SCI_INSERTTEXT, pos, reinterpret_cast<sptr_t>(commentLine.c_str()));
            insertedChars += commentLine.length();
        }
        ScintillaCall(SCI_ENDUNDOACTION);
        if (!bSelEmpty)
            ScintillaCall(SCI_SETSEL, selStart, selEnd + insertedChars);
        else
            ScintillaCall(SCI_SETSEL, curPos + insertedChars, curPos + insertedChars);
    }
    else if (!commentStreamStart.empty())
    {
        // insert a stream comment
        ScintillaCall(SCI_BEGINUNDOACTION);
        ScintillaCall(SCI_INSERTTEXT, selStart, reinterpret_cast<sptr_t>(commentStreamStart.c_str()));
        ScintillaCall(SCI_INSERTTEXT, selEnd + commentStreamStart.length(), reinterpret_cast<sptr_t>(commentStreamEnd.c_str()));
        ScintillaCall(SCI_ENDUNDOACTION);
        if (!bSelEmpty)
            ScintillaCall(SCI_SETSEL, selStart + commentStreamStart.length(), selEnd + commentStreamStart.length());
        else
            ScintillaCall(SCI_SETSEL, curPos + commentStreamStart.length(), curPos + commentStreamStart.length());
    }

    return true;
}

bool CCmdUnComment::Execute()
{
    // Get Selection
    bool   bSelEmpty      = !!ScintillaCall(SCI_GETSELECTIONEMPTY);
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
        selStart          = ScintillaCall(SCI_GETSELECTIONSTART);
        selEnd            = ScintillaCall(SCI_GETSELECTIONEND);
        size_t selEndCorr = 0;
        // go back if selEnd is on an EOL char
        switch (ScintillaCall(SCI_GETCHARAT, selEnd))
        {
            case '\r':
            case '\n':
                --selEnd;
                ++selEndCorr;
                break;
            default:
                break;
        }
        switch (ScintillaCall(SCI_GETCHARAT, selEnd))
        {
            case '\r':
            case '\n':
                --selEnd;
                ++selEndCorr;
                break;
            default:
                break;
        }

        auto          strbuf = std::make_unique<char[]>(commentstreamstart.size() + commentstreamend.size() + 5);
        Sci_TextRange rangeStart;
        rangeStart.chrg.cpMin = static_cast<Sci_PositionCR>(selStart - commentstreamstart.length());
        rangeStart.chrg.cpMax = static_cast<Sci_PositionCR>(selStart);
        rangeStart.lpstrText  = strbuf.get();
        ScintillaCall(SCI_GETTEXTRANGE, 0, reinterpret_cast<sptr_t>(&rangeStart));
        bool bRemovedStream = false;
        if (!commentstreamstart.empty() && _stricmp(commentstreamstart.c_str(), strbuf.get()) == 0)
        {
            // find the end marker
            Sci_TextRange rangeEnd;
            rangeEnd.chrg.cpMin = static_cast<Sci_PositionCR>(selEnd);
            rangeEnd.chrg.cpMax = static_cast<Sci_PositionCR>(selEnd + commentstreamend.length());
            rangeEnd.lpstrText  = strbuf.get();
            ScintillaCall(SCI_GETTEXTRANGE, 0, reinterpret_cast<sptr_t>(&rangeEnd));
            if (_stricmp(commentstreamend.c_str(), strbuf.get()) == 0)
            {
                ScintillaCall(SCI_BEGINUNDOACTION);
                // remove the stream start marker
                ScintillaCall(SCI_SETSEL, rangeStart.chrg.cpMin, rangeStart.chrg.cpMax);
                ScintillaCall(SCI_REPLACESEL, 0, reinterpret_cast<sptr_t>(""));
                // remove the end start marker
                ScintillaCall(SCI_SETSEL, rangeEnd.chrg.cpMin - commentstreamstart.length(), rangeEnd.chrg.cpMax - commentstreamstart.length());
                ScintillaCall(SCI_REPLACESEL, 0, reinterpret_cast<sptr_t>(""));
                ScintillaCall(SCI_ENDUNDOACTION);
                ScintillaCall(SCI_SETSEL, selStart - commentstreamstart.length(), selEnd - commentstreamstart.length() + selEndCorr);
                bRemovedStream = true;
            }
        }
        if (!bRemovedStream)
        {
            lineStartStart = ScintillaCall(SCI_POSITIONFROMLINE, ScintillaCall(SCI_LINEFROMPOSITION, selStart));
            if (lineStartStart == selStart)
            {
                // remove block comments for each selected line
                sptr_t lineStart = ScintillaCall(SCI_LINEFROMPOSITION, selStart);
                sptr_t lineEnd   = ScintillaCall(SCI_LINEFROMPOSITION, selEnd);
                ScintillaCall(SCI_BEGINUNDOACTION);
                size_t removedChars = 0;
                for (auto line = lineStart; line <= lineEnd; ++line)
                {
                    size_t        pos = ScintillaCall(SCI_GETLINEINDENTPOSITION, line);
                    Sci_TextRange range;
                    range.chrg.cpMin = static_cast<Sci_PositionCR>(pos);
                    range.chrg.cpMax = static_cast<Sci_PositionCR>(pos + commentLine.length());
                    range.lpstrText  = strbuf.get();
                    ScintillaCall(SCI_GETTEXTRANGE, 0, reinterpret_cast<sptr_t>(&range));
                    if (_stricmp(commentLine.c_str(), strbuf.get()) == 0)
                    {
                        ScintillaCall(SCI_SETSEL, range.chrg.cpMin, range.chrg.cpMax);
                        ScintillaCall(SCI_REPLACESEL, 0, reinterpret_cast<sptr_t>(""));
                        removedChars += commentLine.length();
                    }
                }
                ScintillaCall(SCI_ENDUNDOACTION);
                ScintillaCall(SCI_SETSEL, selStart, selEnd - removedChars + selEndCorr);
            }
        }
    }
    else
    {
        // remove block comment for the current line
        auto strBuf = std::make_unique<char[]>(commentLine.size() + 5);
        auto curPos = ScintillaCall(SCI_GETCURRENTPOS);
        ScintillaCall(SCI_BEGINUNDOACTION);
        auto          pos = ScintillaCall(SCI_GETLINEINDENTPOSITION, ScintillaCall(SCI_LINEFROMPOSITION, curPos));
        Sci_TextRange range;
        range.chrg.cpMin = static_cast<Sci_PositionCR>(pos);
        range.chrg.cpMax = static_cast<Sci_PositionCR>(pos + commentLine.length());
        range.lpstrText  = strBuf.get();
        ScintillaCall(SCI_GETTEXTRANGE, 0, reinterpret_cast<sptr_t>(&range));
        if (!commentLine.empty() && (_stricmp(commentLine.c_str(), strBuf.get()) == 0))
        {
            ScintillaCall(SCI_SETSEL, range.chrg.cpMin, range.chrg.cpMax);
            ScintillaCall(SCI_REPLACESEL, 0, reinterpret_cast<sptr_t>(""));
            auto newCurPos = curPos > static_cast<sptr_t>(commentLine.length()) ? curPos - commentLine.length() : 0;
            ScintillaCall(SCI_SETSEL, newCurPos, newCurPos);
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
            auto findRet   = ScintillaCall(SCI_FINDTEXT, 0, reinterpret_cast<sptr_t>(&ttf));
            if (findRet >= 0)
            {
                ttf.lpstrText = const_cast<char*>(commentstreamend.c_str());
                auto findRet2 = ScintillaCall(SCI_FINDTEXT, 0, reinterpret_cast<sptr_t>(&ttf));
                // test if the start marker is found later than the end marker: if not, we found an independent comment outside of the current position
                if (findRet2 < findRet)
                {
                    // now find the end marker
                    ttf.chrg.cpMin = static_cast<Sci_PositionCR>(curPos);
                    ttf.chrg.cpMax = static_cast<Sci_PositionCR>(ScintillaCall(SCI_GETLENGTH));
                    ttf.lpstrText  = const_cast<char*>(commentstreamend.c_str());
                    auto findRet3  = ScintillaCall(SCI_FINDTEXT, 0, reinterpret_cast<sptr_t>(&ttf));
                    if (findRet3 >= 0)
                    {
                        ScintillaCall(SCI_SETSEL, findRet3, findRet3 + commentstreamend.length());
                        ScintillaCall(SCI_REPLACESEL, 0, reinterpret_cast<sptr_t>(""));
                        ScintillaCall(SCI_SETSEL, findRet, findRet + commentstreamstart.length());
                        ScintillaCall(SCI_REPLACESEL, 0, reinterpret_cast<sptr_t>(""));
                        ScintillaCall(SCI_SETSEL, curPos - commentstreamstart.length(), curPos - commentstreamstart.length());
                    }
                }
            }
        }
        ScintillaCall(SCI_ENDUNDOACTION);
    }

    return true;
}
