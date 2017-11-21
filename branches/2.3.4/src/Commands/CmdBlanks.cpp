// This file is part of BowPad.
//
// Copyright (C) 2013-2017 - Stefan Kueng
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
#include "BowPad.h"
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
        findRet = ScintillaCall(SCI_SEARCHINTARGET, sFindString.length(), (sptr_t)sFindString.c_str());
        if (findRet >= 0)
        {
            sptr_t endpos = ScintillaCall(SCI_GETTARGETEND);
            char c = 0;
            do
            {
                --endpos;
                c = (char)ScintillaCall(SCI_GETCHARAT, endpos);
            } while ((c == '\n') || (c == '\r'));
            ScintillaCall(SCI_SETTARGETEND, endpos+1);
            ScintillaCall(SCI_REPLACETARGETRE, (uptr_t)-1, (sptr_t)"");

            if (ScintillaCall(SCI_GETSELECTIONEMPTY))
            {
                ScintillaCall(SCI_SETTARGETSTART, findRet+1);
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
    int tabsize = (int)ScintillaCall(SCI_GETTABWIDTH);
    auto docLength = ScintillaCall(SCI_GETLENGTH) + 1;
    auto curpos = ScintillaCall(SCI_GETCURRENTPOS);
    bool bIgnoreQuotes = false;
    auto lexer = ScintillaCall(SCI_GETLEXER);
    switch (lexer)
    {
        case SCLEX_XML:
        case SCLEX_HTML:
            bIgnoreQuotes = true;
            break;
    }
    auto source = std::make_unique<char[]>(docLength);
    ScintillaCall(SCI_GETTEXT, docLength, (LPARAM)source.get());

    // untabify the file
    // first find the number of spaces we have to insert.
    decltype(docLength) pos = 0;
    decltype(docLength) inlinepos = 0;
    decltype(docLength) spacestoinsert = 0;
    bool inChar = false;
    bool inString = false;
    bool escapeChar = false;
    char * pBuf = (char*)source.get();
    for (decltype(docLength) i = 0; i < docLength; ++i, ++pos, ++pBuf)
    {
        ++inlinepos;
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
            inlinepos = 0;
        // we have to convert all tabs
        if (*pBuf == '\t')
        {
            inlinepos += tabsize - 1;
            auto inlinepostemp = tabsize - ((inlinepos + tabsize) % tabsize);
            if (inlinepostemp == 0)
                inlinepostemp = tabsize;
            spacestoinsert += (inlinepostemp - 1);      // minus one because the tab itself gets replaced
            inlinepos += inlinepostemp;
        }
    }

    if (spacestoinsert)
    {
        auto setpos = curpos;
        inlinepos = 0;
        auto newfilelen = docLength + spacestoinsert;
        auto destination = std::make_unique<char[]>(newfilelen);
        char * pBufStart = destination.get();
        char * pOldBuf = (char*)source.get();
        pBuf = pBufStart;
        inChar = false;
        inString = false;
        escapeChar = false;
        for (decltype(docLength) i = 0; i < docLength; ++i)
        {
            ++inlinepos;
            if (escapeChar)
            {
                escapeChar = false;
                *pBuf++ = *pOldBuf++;
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
                inlinepos = 0;
            if (*pOldBuf == '\t')
            {
                auto inlinepostemp = tabsize - (((inlinepos - 1) + tabsize) % tabsize);
                if (inlinepostemp == 0)
                    inlinepostemp = tabsize;
                inlinepos += (inlinepostemp - 1);
                for (decltype(inlinepostemp) j = 0; j < inlinepostemp; ++j)
                {
                    *pBuf++ = ' ';
                    if (i < curpos)
                        ++setpos;
                }
                pOldBuf++;
            }
            else
                *pBuf++ = *pOldBuf++;
        }
        ScintillaCall(SCI_BEGINUNDOACTION);
        ScintillaCall(SCI_SETTEXT, 0, (LPARAM)destination.get());
        ScintillaCall(SCI_ENDUNDOACTION);
        Center(setpos, setpos);
        return true;
    }
    return false;
}

bool CCmdSpaces2Tabs::Execute()
{
    // convert the whole file, ignore the selection
    int tabsize = (int)ScintillaCall(SCI_GETTABWIDTH);
    auto docLength = ScintillaCall(SCI_GETLENGTH) + 1;
    auto curpos = ScintillaCall(SCI_GETCURRENTPOS);
    bool bIgnoreQuotes = false;
    auto lexer = ScintillaCall(SCI_GETLEXER);
    switch (lexer)
    {
        case SCLEX_XML:
        case SCLEX_HTML:
            bIgnoreQuotes = true;
            break;
    }

    auto source = std::make_unique<char[]>(docLength);
    ScintillaCall(SCI_GETTEXT, docLength, (LPARAM)source.get());

    // tabify the file
    // first find out how many spaces we have to convert into tabs
    decltype(docLength) count = 0;
    decltype(docLength) spacecount = 0;
    std::vector<decltype(docLength)> spacegrouppositions;
    decltype(docLength) pos = 0;
    bool inChar = false;
    bool inString = false;
    bool escapeChar = false;
    char * pBuf = (char*)source.get();
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
            spacecount = 0;
            continue;
        }

        if ((*pBuf == ' ') || (*pBuf == '\t'))
        {
            spacecount++;
            if ((spacecount == tabsize) || ((*pBuf == '\t') && (spacecount > 1)))
            {
                spacegrouppositions.push_back(pos - spacecount + 1);
                count += (spacecount - 1);
                spacecount = 0;
            }
            if (*pBuf == '\t')
                spacecount = 0;
        }
        else
            spacecount = 0;
    }
    // now we have the number of space groups we have to convert to tabs
    // create a new file buffer and copy everything over there, replacing those space
    // groups with tabs.
    if (count)
    {
        auto setpos = curpos;
        auto newfilelen = docLength;
        newfilelen -= count;
        auto destination = std::make_unique<char[]>(newfilelen);
        char * pBufStart = destination.get();
        char * pOldBuf = (char*)source.get();
        pBuf = pBufStart;
        auto it = spacegrouppositions.begin();
        for (decltype(docLength) i = 0; i < (docLength); ++i)
        {
            if ((it != spacegrouppositions.end()) && (*it == i))
            {
                *pBuf++ = '\t';
                spacecount = 0;
                while ((spacecount < tabsize) && (*pOldBuf == ' '))
                {
                    i++;
                    spacecount++;
                    pOldBuf++;
                    if (i < curpos)
                        --setpos;
                }
                if ((spacecount < tabsize) && (*pOldBuf == '\t'))
                    pBuf--;
                --i;
                ++it;
            }
            else
                *pBuf++ = *pOldBuf++;
        }
        ScintillaCall(SCI_BEGINUNDOACTION);
        ScintillaCall(SCI_SETTEXT, 0, (LPARAM)destination.get());
        ScintillaCall(SCI_ENDUNDOACTION);
        Center(setpos, setpos);
        return true;
    }
    return false;
}
