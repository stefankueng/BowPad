// This file is part of BowPad.
//
// Copyright (C) 2013-2014, 2016-2018 - Stefan Kueng
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
#include <functional>

bool ChangeCase(std::function<sptr_t(int msg, uptr_t wParam, sptr_t lParam)> ScintillaCall,
                std::function<void(std::wstring& selText)> changeFunc)
{
    ScintillaCall(SCI_BEGINUNDOACTION, 0, 0);

    auto numSelections = ScintillaCall(SCI_GETSELECTIONS, 0, 0);
    for (decltype(numSelections) i = 0; i < numSelections; ++i)
    {
        auto selStart = ScintillaCall(SCI_GETSELECTIONNSTART, i, 0);
        auto selEnd = ScintillaCall(SCI_GETSELECTIONNEND, i, 0);

        if ((selStart == selEnd) && (numSelections == 1))
        {
            auto curLine = ScintillaCall(SCI_LINEFROMPOSITION, ScintillaCall(SCI_GETCURRENTPOS, 0, 0), 0);
            selStart = ScintillaCall(SCI_POSITIONFROMLINE, curLine, 0);
            selEnd = ScintillaCall(SCI_GETLINEENDPOSITION, curLine, 0);
        }

        auto strbuf = std::make_unique<char[]>(abs(selEnd - selStart) + 5);
        Sci_TextRange rangestart;
        rangestart.chrg.cpMin = Sci_PositionCR(selStart);
        rangestart.chrg.cpMax = Sci_PositionCR(selEnd);
        rangestart.lpstrText = strbuf.get();
        ScintillaCall(SCI_GETTEXTRANGE, 0, (sptr_t)&rangestart);

        std::wstring selText = CUnicodeUtils::StdGetUnicode(strbuf.get());
        changeFunc(selText);
        std::string sUpper = CUnicodeUtils::StdGetUTF8(selText);

        ScintillaCall(SCI_SETTARGETSTART, selStart, 0);
        ScintillaCall(SCI_SETTARGETEND, selEnd, 0);
        ScintillaCall(SCI_REPLACETARGET, (WPARAM)-1, (LPARAM)sUpper.c_str());
        ScintillaCall(SCI_SETSELECTIONNSTART, i, selStart);
        ScintillaCall(SCI_SETSELECTIONNEND, i, selEnd);
    }
    ScintillaCall(SCI_ENDUNDOACTION, 0, 0);

    return true;
}

bool CCmdConvertUppercase::Execute()
{
    auto SciCall = std::bind(&CCmdConvertUppercase::ScintillaCall, *this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
    return ChangeCase(SciCall, [](auto& selText) { CharUpper(selText.data()); });
}

bool CCmdConvertLowercase::Execute()
{
    auto SciCall = std::bind(&CCmdConvertLowercase::ScintillaCall, *this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
    return ChangeCase(SciCall, [](auto& selText) { CharLower(selText.data()); });
}

bool CCmdConvertTitlecase::Execute()
{
    auto SciCall = std::bind(&CCmdConvertTitlecase::ScintillaCall, *this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
    return ChangeCase(SciCall, [](auto& selText)
    {
        if (selText.length() > 0)
        {
            selText[0] = (wchar_t)LOWORD(CharUpper((LPWSTR)selText[0]));
            for (std::wstring::iterator it = selText.begin() + 1; it != selText.end(); ++it)
            {
                if (!IsCharAlpha(*(it - 1)) && IsCharLower(*it))
                {
                    *it = (wchar_t)LOWORD(CharUpper((LPWSTR)*it));
                }
            }
        }
    });
}
