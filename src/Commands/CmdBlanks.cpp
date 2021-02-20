// This file is part of BowPad.
//
// Copyright (C) 2013-2017, 2021 - Stefan Kueng
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
#include "CmdBlanks.h"
#include "ScintillaWnd.h"
#include "SciLexer.h"

bool CCmdTrim::Execute()
{
    if (ScintillaCall(SCI_GETSELECTIONEMPTY))
    {
        ScintillaCall(SCI_SETTARGETSTART, 0);
        ScintillaCall(SCI_SETTARGETEND, ScintillaCall(SCI_GETLENGTH));
    }
    else
    {
        ScintillaCall(SCI_TARGETFROMSELECTION);
    }

    ScintillaCall(SCI_SETSEARCHFLAGS, SCFIND_REGEXP | SCFIND_CXX11REGEX);
    sptr_t findRet = -1;
    ScintillaCall(SCI_BEGINUNDOACTION);
    const std::string sFindString = "([ \\t]+$)|([ \\t]+\\r)|([ \\t]+\\n)";
    do
    {
        findRet = ScintillaCall(SCI_SEARCHINTARGET, sFindString.length(), reinterpret_cast<sptr_t>(sFindString.c_str()));
        if (findRet >= 0)
        {
            sptr_t endPos = ScintillaCall(SCI_GETTARGETEND);
            char   c      = 0;
            do
            {
                --endPos;
                c = static_cast<char>(ScintillaCall(SCI_GETCHARAT, endPos));
            } while ((c == '\n') || (c == '\r'));
            ScintillaCall(SCI_SETTARGETEND, endPos + 1);
            ScintillaCall(SCI_REPLACETARGETRE, static_cast<uptr_t>(-1), reinterpret_cast<sptr_t>(""));

            if (ScintillaCall(SCI_GETSELECTIONEMPTY))
            {
                ScintillaCall(SCI_SETTARGETSTART, findRet + 1);
                ScintillaCall(SCI_SETTARGETEND, ScintillaCall(SCI_GETLENGTH));
            }
            else
            {
                ScintillaCall(SCI_TARGETFROMSELECTION);
            }
        }
    } while (findRet != -1);
    ScintillaCall(SCI_ENDUNDOACTION);
    return true;
}

bool CCmdTabs2Spaces::Execute()
{
    // convert the whole file, ignore the selection
    int  tabSize       = static_cast<int>(ScintillaCall(SCI_GETTABWIDTH));
    auto docLength     = ScintillaCall(SCI_GETLENGTH) + 1;
    auto curPos        = ScintillaCall(SCI_GETCURRENTPOS);
    bool bIgnoreQuotes = false;
    auto lexer         = ScintillaCall(SCI_GETLEXER);
    switch (lexer)
    {
        case SCLEX_XML:
        case SCLEX_HTML:
            bIgnoreQuotes = true;
            break;
    }
    auto source = std::make_unique<char[]>(docLength);
    ScintillaCall(SCI_GETTEXT, docLength, reinterpret_cast<sptr_t>(source.get()));

    // untabify the file
    // first find the number of spaces we have to insert.
    decltype(docLength) pos            = 0;
    decltype(docLength) inlinePos      = 0;
    decltype(docLength) spacesToInsert = 0;
    bool                inChar         = false;
    bool                inString       = false;
    bool                escapeChar     = false;
    char*               pBuf           = static_cast<char*>(source.get());
    for (decltype(docLength) i = 0; i < docLength; ++i, ++pos, ++pBuf)
    {
        ++inlinePos;
        if (escapeChar)
        {
            escapeChar = false;
            continue;
        }
        if (*pBuf == '\\')
            escapeChar = true;
        if (!bIgnoreQuotes && !inString && (*pBuf == '\''))
            inChar = !inChar;
        if (!bIgnoreQuotes && !inChar && (*pBuf == '\"'))
            inString = !inString;
        if ((*pBuf == '\n') || (*pBuf == '\r'))
            inChar = false;
        if (inChar || inString)
            continue;

        if ((*pBuf == '\r') || (*pBuf == '\n'))
            inlinePos = 0;
        // we have to convert all tabs
        if (*pBuf == '\t')
        {
            inlinePos += tabSize - 1;
            auto inlinePosTemp = tabSize - ((inlinePos + tabSize) % tabSize);
            if (inlinePosTemp == 0)
                inlinePosTemp = tabSize;
            spacesToInsert += (inlinePosTemp - 1); // minus one because the tab itself gets replaced
            inlinePos += inlinePosTemp;
        }
    }

    if (spacesToInsert)
    {
        auto setPos       = curPos;
        inlinePos         = 0;
        auto  newFileLen  = docLength + spacesToInsert;
        auto  destination = std::make_unique<char[]>(newFileLen);
        char* pBufStart   = destination.get();
        char* pOldBuf     = static_cast<char*>(source.get());
        pBuf              = pBufStart;
        inChar            = false;
        inString          = false;
        escapeChar        = false;
        for (decltype(docLength) i = 0; i < docLength; ++i)
        {
            ++inlinePos;
            if (escapeChar)
            {
                escapeChar = false;
                *pBuf++    = *pOldBuf++;
                continue;
            }
            if (*pOldBuf == '\\')
                escapeChar = true;
            if (!bIgnoreQuotes && !inString && (*pOldBuf == '\''))
                inChar = !inChar;
            if (!bIgnoreQuotes && !inChar && (*pOldBuf == '\"'))
                inString = !inString;
            if ((*pOldBuf == '\n') || (*pOldBuf == '\r'))
                inChar = false;
            if (inChar || inString)
            {
                *pBuf++ = *pOldBuf++;
                continue;
            }

            if ((*pOldBuf == '\r') || (*pOldBuf == '\n'))
                inlinePos = 0;
            if (*pOldBuf == '\t')
            {
                auto inlinePosTemp = tabSize - (((inlinePos - 1) + tabSize) % tabSize);
                if (inlinePosTemp == 0)
                    inlinePosTemp = tabSize;
                inlinePos += (inlinePosTemp - 1);
                for (decltype(inlinePosTemp) j = 0; j < inlinePosTemp; ++j)
                {
                    *pBuf++ = ' ';
                    if (i < curPos)
                        ++setPos;
                }
                pOldBuf++;
            }
            else
                *pBuf++ = *pOldBuf++;
        }
        ScintillaCall(SCI_BEGINUNDOACTION);
        ScintillaCall(SCI_SETTEXT, 0, reinterpret_cast<sptr_t>(destination.get()));
        ScintillaCall(SCI_ENDUNDOACTION);
        Center(setPos, setPos);
        return true;
    }
    return false;
}

bool CCmdSpaces2Tabs::Execute()
{
    // convert the whole file, ignore the selection
    int  tabSize       = static_cast<int>(ScintillaCall(SCI_GETTABWIDTH));
    auto docLength     = ScintillaCall(SCI_GETLENGTH) + 1;
    auto curPos        = ScintillaCall(SCI_GETCURRENTPOS);
    bool bIgnoreQuotes = false;
    auto lexer         = ScintillaCall(SCI_GETLEXER);
    switch (lexer)
    {
        case SCLEX_XML:
        case SCLEX_HTML:
            bIgnoreQuotes = true;
            break;
    }

    auto source = std::make_unique<char[]>(docLength);
    ScintillaCall(SCI_GETTEXT, docLength, reinterpret_cast<sptr_t>(source.get()));

    // tabify the file
    // first find out how many spaces we have to convert into tabs
    decltype(docLength)              count      = 0;
    decltype(docLength)              spaceCount = 0;
    std::vector<decltype(docLength)> spaceGroupPositions;
    decltype(docLength)              pos        = 0;
    bool                             inChar     = false;
    bool                             inString   = false;
    bool                             escapeChar = false;
    char*                            pBuf       = static_cast<char*>(source.get());
    for (decltype(docLength) i = 0; i < docLength; ++i, ++pos, ++pBuf)
    {
        if (escapeChar)
        {
            escapeChar = false;
            continue;
        }
        if (*pBuf == '\\')
            escapeChar = true;
        if (!bIgnoreQuotes && !inString && (*pBuf == '\''))
            inChar = !inChar;
        if (!bIgnoreQuotes && !inChar && (*pBuf == '\"'))
            inString = !inString;
        if ((*pBuf == '\n') || (*pBuf == '\r'))
            inChar = false;
        if (inChar || inString)
        {
            spaceCount = 0;
            continue;
        }

        if ((*pBuf == ' ') || (*pBuf == '\t'))
        {
            spaceCount++;
            if ((spaceCount == tabSize) || ((*pBuf == '\t') && (spaceCount > 1)))
            {
                spaceGroupPositions.push_back(pos - spaceCount + 1);
                count += (spaceCount - 1);
                spaceCount = 0;
            }
            if (*pBuf == '\t')
                spaceCount = 0;
        }
        else
            spaceCount = 0;
    }
    // now we have the number of space groups we have to convert to tabs
    // create a new file buffer and copy everything over there, replacing those space
    // groups with tabs.
    if (count)
    {
        auto setPos     = curPos;
        auto newFileLen = docLength;
        newFileLen -= count;
        auto  destination = std::make_unique<char[]>(newFileLen);
        char* pBufStart   = destination.get();
        char* pOldBuf     = static_cast<char*>(source.get());
        pBuf              = pBufStart;
        auto it           = spaceGroupPositions.begin();
        for (decltype(docLength) i = 0; i < (docLength); ++i)
        {
            if ((it != spaceGroupPositions.end()) && (*it == i))
            {
                *pBuf++    = '\t';
                spaceCount = 0;
                while ((spaceCount < tabSize) && (*pOldBuf == ' '))
                {
                    i++;
                    spaceCount++;
                    pOldBuf++;
                    if (i < curPos)
                        --setPos;
                }
                if ((spaceCount < tabSize) && (*pOldBuf == '\t'))
                    pBuf--;
                --i;
                ++it;
            }
            else
                *pBuf++ = *pOldBuf++;
        }
        ScintillaCall(SCI_BEGINUNDOACTION);
        ScintillaCall(SCI_SETTEXT, 0, reinterpret_cast<sptr_t>(destination.get()));
        ScintillaCall(SCI_ENDUNDOACTION);
        Center(setPos, setPos);
        return true;
    }
    return false;
}
