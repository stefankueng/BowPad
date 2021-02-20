// This file is part of BowPad.
//
// Copyright (C) 2013-2014, 2016-2018, 2020-2021 - Stefan Kueng
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

#include <functional>

bool ChangeCase(std::function<sptr_t(int msg, uptr_t wParam, sptr_t lParam)> scintillaCall,
                std::function<void(std::wstring& selText)>                   changeFunc)
{
    scintillaCall(SCI_BEGINUNDOACTION, 0, 0);

    auto numSelections = scintillaCall(SCI_GETSELECTIONS, 0, 0);
    for (decltype(numSelections) i = 0; i < numSelections; ++i)
    {
        auto selStart = scintillaCall(SCI_GETSELECTIONNSTART, i, 0);
        auto selEnd   = scintillaCall(SCI_GETSELECTIONNEND, i, 0);

        if ((selStart == selEnd) && (numSelections == 1))
        {
            auto curLine = scintillaCall(SCI_LINEFROMPOSITION, scintillaCall(SCI_GETCURRENTPOS, 0, 0), 0);
            selStart     = scintillaCall(SCI_POSITIONFROMLINE, curLine, 0);
            selEnd       = scintillaCall(SCI_GETLINEENDPOSITION, curLine, 0);
        }

        auto          strBuf = std::make_unique<char[]>(abs(selEnd - selStart) + 5);
        Sci_TextRange rangeStart{};
        rangeStart.chrg.cpMin = static_cast<Sci_PositionCR>(selStart);
        rangeStart.chrg.cpMax = static_cast<Sci_PositionCR>(selEnd);
        rangeStart.lpstrText  = strBuf.get();
        scintillaCall(SCI_GETTEXTRANGE, 0, reinterpret_cast<sptr_t>(&rangeStart));

        std::wstring selText = CUnicodeUtils::StdGetUnicode(strBuf.get());
        changeFunc(selText);
        std::string sUpper = CUnicodeUtils::StdGetUTF8(selText);

        scintillaCall(SCI_SETTARGETSTART, selStart, 0);
        scintillaCall(SCI_SETTARGETEND, selEnd, 0);
        scintillaCall(SCI_REPLACETARGET, static_cast<uptr_t>(-1), reinterpret_cast<sptr_t>(sUpper.c_str()));
        scintillaCall(SCI_SETSELECTIONNSTART, i, selStart);
        scintillaCall(SCI_SETSELECTIONNEND, i, selEnd);
    }
    scintillaCall(SCI_ENDUNDOACTION, 0, 0);

    return true;
}

bool CCmdConvertUppercase::Execute()
{
    auto sciCall = std::bind(&CCmdConvertUppercase::ScintillaCall, *this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
    return ChangeCase(sciCall, [](auto& selText) { CharUpper(selText.data()); });
}

bool CCmdConvertLowercase::Execute()
{
    auto sciCall = std::bind(&CCmdConvertLowercase::ScintillaCall, *this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
    return ChangeCase(sciCall, [](auto& selText) { CharLower(selText.data()); });
}

bool CCmdConvertTitlecase::Execute()
{
    auto sciCall = std::bind(&CCmdConvertTitlecase::ScintillaCall, *this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
    return ChangeCase(sciCall, [](auto& selText) {
        if (selText.length() > 0)
        {
            wchar_t upperChar[] = {selText[0], 0};
            CharUpper(upperChar);
            selText[0] = upperChar[0];
            for (std::wstring::iterator it = selText.begin() + 1; it != selText.end(); ++it)
            {
                if (!IsCharAlpha(*(it - 1)) && IsCharLower(*it))
                {
                    upperChar[0] = *it;
                    CharUpper(upperChar);
                    *it = upperChar[0];
                }
            }
        }
    });
}
