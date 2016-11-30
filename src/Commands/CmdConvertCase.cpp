// This file is part of BowPad.
//
// Copyright (C) 2013-2014, 2016 - Stefan Kueng
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

    auto numSelections = ScintillaCall(SCI_GETSELECTIONS);
    for (decltype(numSelections) i = 0; i < numSelections; ++i)
    {
        auto selStart = ScintillaCall(SCI_GETSELECTIONNSTART, i);
        auto selEnd   = ScintillaCall(SCI_GETSELECTIONNEND, i);

        if ((selStart == selEnd) && (numSelections == 1))
        {
            auto curLine = ScintillaCall(SCI_LINEFROMPOSITION, ScintillaCall(SCI_GETCURRENTPOS));
            selStart = ScintillaCall(SCI_POSITIONFROMLINE, curLine);
            selEnd = ScintillaCall(SCI_GETLINEENDPOSITION, curLine);
        }

        auto strbuf = std::make_unique<char[]>(abs(selEnd-selStart) + 5);
        Scintilla::Sci_TextRange rangestart;
        rangestart.chrg.cpMin = Sci_PositionCR(selStart);
        rangestart.chrg.cpMax = Sci_PositionCR(selEnd);
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

    auto numSelections = ScintillaCall(SCI_GETSELECTIONS);
    for (decltype(numSelections) i = 0; i < numSelections; ++i)
    {
        auto selStart = ScintillaCall(SCI_GETSELECTIONNSTART, i);
        auto selEnd   = ScintillaCall(SCI_GETSELECTIONNEND, i);

        if ((selStart == selEnd) && (numSelections == 1))
        {
            auto curLine = ScintillaCall(SCI_LINEFROMPOSITION, ScintillaCall(SCI_GETCURRENTPOS));
            selStart = ScintillaCall(SCI_POSITIONFROMLINE, curLine);
            selEnd = ScintillaCall(SCI_GETLINEENDPOSITION, curLine);
        }

        auto strbuf = std::make_unique<char[]>(abs(selEnd-selStart) + 5);
        Scintilla::Sci_TextRange rangestart;
        rangestart.chrg.cpMin = Sci_PositionCR(selStart);
        rangestart.chrg.cpMax = Sci_PositionCR(selEnd);
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

bool CCmdConvertTitlecase::Execute()
{
    ScintillaCall(SCI_BEGINUNDOACTION);

    auto numSelections = ScintillaCall(SCI_GETSELECTIONS);
    for (decltype(numSelections) i = 0; i < numSelections; ++i)
    {
        auto selStart = ScintillaCall(SCI_GETSELECTIONNSTART, i);
        auto selEnd = ScintillaCall(SCI_GETSELECTIONNEND, i);

        if ((selStart == selEnd) && (numSelections == 1))
        {
            auto curLine = ScintillaCall(SCI_LINEFROMPOSITION, ScintillaCall(SCI_GETCURRENTPOS));
            selStart = ScintillaCall(SCI_POSITIONFROMLINE, curLine);
            selEnd = ScintillaCall(SCI_GETLINEENDPOSITION, curLine);
        }

        auto strbuf = std::make_unique<char[]>(abs(selEnd - selStart) + 5);
        Scintilla::Sci_TextRange rangestart;
        rangestart.chrg.cpMin = Sci_PositionCR(selStart);
        rangestart.chrg.cpMax = Sci_PositionCR(selEnd);
        rangestart.lpstrText = strbuf.get();
        ScintillaCall(SCI_GETTEXTRANGE, 0, (sptr_t)&rangestart);

        std::wstring selText = CUnicodeUtils::StdGetUnicode(strbuf.get());
        if (selText.length() > 0)
        {
            selText[0] = (wchar_t)toupper(selText[0]);
            for (std::wstring::iterator it = selText.begin() + 1; it != selText.end(); ++it)
            {
                if (!isalpha(*(it - 1)) && islower(*it))
                {
                    *it = (wchar_t)toupper(*it);
                }
            }
        }

        std::string sUpper = CUnicodeUtils::StdGetUTF8(selText);

        ScintillaCall(SCI_SETTARGETSTART, selStart);
        ScintillaCall(SCI_SETTARGETEND, selEnd);
        ScintillaCall(SCI_REPLACETARGET, (WPARAM)-1, (LPARAM)sUpper.c_str());
    }
    ScintillaCall(SCI_ENDUNDOACTION);

    return true;
}
