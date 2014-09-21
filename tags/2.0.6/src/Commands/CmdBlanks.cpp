// This file is part of BowPad.
//
// Copyright (C) 2013-2014 - Stefan Kueng
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

    ScintillaCall(SCI_SETSEARCHFLAGS, SCFIND_REGEXP);
    sptr_t findRet = -1;
    ScintillaCall(SCI_BEGINUNDOACTION);
    std::string sFindString = "([ \\t]+$)|([ \\t]+\\r)||([ \\t]+\\n)";
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
    size_t docLength = ScintillaCall(SCI_GETLENGTH) + 1;

    std::unique_ptr<char[]> source(new char[docLength]);
    ScintillaCall(SCI_GETTEXT, docLength, (LPARAM)source.get());

    // untabify the file
    // first find the number of spaces we have to insert.
    size_t pos = 0;
    size_t inlinepos = 0;
    size_t spacestoinsert = 0;
    bool inChar = false;
    bool inString = false;
    bool escapeChar = false;
    char * pBuf = (char*)source.get();
    for (size_t i = 0; i < docLength; ++i, ++pos, ++pBuf)
    {
        ++inlinepos;
        if (escapeChar)
        {
            escapeChar = false;
            continue;
        }
        if (*pBuf == '\\')
            escapeChar = true;
        if (!inString && (*pBuf == '\''))
            inChar = !inChar;
        if ((!inChar) && (*pBuf == '\"'))
            inString = !inString;
        if (inChar || inString)
            continue;

        if ((*pBuf == '\r') || (*pBuf == '\n'))
            inlinepos = 0;
        // we have to convert all tabs
        if (*pBuf == '\t')
        {
            inlinepos += tabsize - 1;
            long inlinepostemp = tabsize - ((inlinepos + tabsize) % tabsize);
            if (inlinepostemp == 0)
                inlinepostemp = tabsize;
            spacestoinsert += (inlinepostemp - 1);      // minus one because the tab itself gets replaced
            inlinepos += inlinepostemp;
        }
    }

    if (spacestoinsert)
    {
        inlinepos = 0;
        size_t newfilelen = docLength + spacestoinsert;
        std::unique_ptr<char[]> destination(new char[newfilelen]);
        char * pBufStart = destination.get();
        char * pOldBuf = (char*)source.get();
        pBuf = pBufStart;
        bool inChar = false;
        bool inString = false;
        bool escapeChar = false;
        for (size_t i = 0; i < docLength; ++i)
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
            if (!inString && (*pOldBuf == '\''))
                inChar = !inChar;
            if ((!inChar) && (*pOldBuf == '\"'))
                inString = !inString;
            if (inChar || inString)
            {
                *pBuf++ = *pOldBuf++;
                continue;
            }

            if ((*pOldBuf == '\r') || (*pOldBuf == '\n'))
                inlinepos = 0;
            if (*pOldBuf == '\t')
            {
                size_t inlinepostemp = tabsize - (((inlinepos - 1) + tabsize) % tabsize);
                if (inlinepostemp == 0)
                    inlinepostemp = tabsize;
                inlinepos += (inlinepostemp - 1);
                for (size_t j = 0; j < inlinepostemp; ++j)
                {
                    *pBuf++ = ' ';
                }
                pOldBuf++;
            }
            else
                *pBuf++ = *pOldBuf++;
        }
        ScintillaCall(SCI_BEGINUNDOACTION);
        ScintillaCall(SCI_SETTEXT, 0, (LPARAM)destination.get());
        ScintillaCall(SCI_ENDUNDOACTION);
        return true;
    }
    return false;
}

bool CCmdSpaces2Tabs::Execute()
{
    // convert the whole file, ignore the selection
    int tabsize = (int)ScintillaCall(SCI_GETTABWIDTH);
    size_t docLength = ScintillaCall(SCI_GETLENGTH) + 1;

    std::unique_ptr<char[]> source(new char[docLength]);
    ScintillaCall(SCI_GETTEXT, docLength, (LPARAM)source.get());

    // tabify the file
    // first find out how many spaces we have to convert into tabs
    size_t count = 0;
    size_t spacecount = 0;
    std::vector<size_t> spacegrouppositions;
    size_t pos = 0;
    bool inChar = false;
    bool inString = false;
    bool escapeChar = false;
    char * pBuf = (char*)source.get();
    for (size_t i = 0; i < docLength; ++i, ++pos, ++pBuf)
    {
        if (escapeChar)
        {
            escapeChar = false;
            continue;
        }
        if (*pBuf == '\\')
            escapeChar = true;
        if (!inString && (*pBuf == '\''))
            inChar = !inChar;
        if ((!inChar) && (*pBuf == '\"'))
            inString = !inString;
        if (inChar || inString)
        {
            spacecount = 0;
            continue;
        }

        if ((*pBuf == ' ') || (*pBuf == '\t'))
        {
            spacecount++;
            if ((spacecount == (size_t)tabsize) || ((*pBuf == '\t') && (spacecount > 1)))
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
        size_t newfilelen = docLength;
        newfilelen -= count;
        std::unique_ptr<char[]> destination(new char[newfilelen]);
        char * pBufStart = destination.get();
        char * pOldBuf = (char*)source.get();
        pBuf = pBufStart;
        auto it = spacegrouppositions.begin();
        for (size_t i = 0; i < (docLength); ++i)
        {
            if ((it != spacegrouppositions.end()) && (*it == i))
            {
                *pBuf++ = '\t';
                spacecount = 0;
                while ((spacecount < (size_t)tabsize) && (*pOldBuf == ' '))
                {
                    i++;
                    spacecount++;
                    pOldBuf++;
                }
                if ((spacecount < (size_t)tabsize) && (*pOldBuf == '\t'))
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
        return true;
    }
    return false;
}
