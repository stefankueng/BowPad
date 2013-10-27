// This file is part of BowPad.
//
// Copyright (C) 2013 - Stefan Kueng
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
#include "CmdConvertCase.h"
#include "UnicodeUtils.h"

#include <algorithm>

bool CCmdConvertUppercase::Execute()
{
    ScintillaCall(SCI_BEGINUNDOACTION);

    int numSelections = (int)ScintillaCall(SCI_GETSELECTIONS);
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

        std::unique_ptr<char[]> strbuf(new char[abs(selEnd-selStart) + 5]);
        Scintilla::Sci_TextRange rangestart;
        rangestart.chrg.cpMin = long(selStart);
        rangestart.chrg.cpMax = long(selEnd);
        rangestart.lpstrText = strbuf.get();
        ScintillaCall(SCI_GETTEXTRANGE, 0, (sptr_t)&rangestart);

        std::wstring selText = CUnicodeUtils::StdGetUnicode(strbuf.get());
        std::transform(selText.begin(), selText.end(), selText.begin(), ::towupper);
        std::string sUpper = CUnicodeUtils::StdGetUTF8(selText);

        ScintillaCall(SCI_SETTARGETSTART, selStart);
        ScintillaCall(SCI_SETTARGETEND, selEnd);
        ScintillaCall(SCI_REPLACETARGET, (WPARAM)-1, (LPARAM)sUpper.c_str());
    }
    ScintillaCall(SCI_ENDUNDOACTION);

    return true;
}

bool CCmdConvertLowercase::Execute()
{
    ScintillaCall(SCI_BEGINUNDOACTION);

    int numSelections = (int)ScintillaCall(SCI_GETSELECTIONS);
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

        std::unique_ptr<char[]> strbuf(new char[abs(selEnd-selStart) + 5]);
        Scintilla::Sci_TextRange rangestart;
        rangestart.chrg.cpMin = long(selStart);
        rangestart.chrg.cpMax = long(selEnd);
        rangestart.lpstrText = strbuf.get();
        ScintillaCall(SCI_GETTEXTRANGE, 0, (sptr_t)&rangestart);

        std::wstring selText = CUnicodeUtils::StdGetUnicode(strbuf.get());
        std::transform(selText.begin(), selText.end(), selText.begin(), ::towlower);
        std::string sUpper = CUnicodeUtils::StdGetUTF8(selText);

        ScintillaCall(SCI_SETTARGETSTART, selStart);
        ScintillaCall(SCI_SETTARGETEND, selEnd);
        ScintillaCall(SCI_REPLACETARGET, (WPARAM)-1, (LPARAM)sUpper.c_str());
    }
    ScintillaCall(SCI_ENDUNDOACTION);

    return true;
}
