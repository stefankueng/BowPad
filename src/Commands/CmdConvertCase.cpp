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

bool ChangeCase(Scintilla::ScintillaCall&                  scintillaCall,
                std::function<void(std::wstring& selText)> changeFunc)
{
    scintillaCall.BeginUndoAction();

    auto numSelections = scintillaCall.Selections();
    for (decltype(numSelections) i = 0; i < numSelections; ++i)
    {
        auto selStart = scintillaCall.SelectionNStart(i);
        auto selEnd   = scintillaCall.SelectionNEnd(i);

        if ((selStart == selEnd) && (numSelections == 1))
        {
            auto curLine = scintillaCall.LineFromPosition(scintillaCall.CurrentPos());
            selStart     = scintillaCall.PositionFromLine(curLine);
            selEnd       = scintillaCall.LineEndPosition(curLine);
        }

        auto          strBuf = std::make_unique<char[]>(abs(selEnd - selStart) + 5);
        Sci_TextRange rangeStart{};
        rangeStart.chrg.cpMin = static_cast<Sci_PositionCR>(selStart);
        rangeStart.chrg.cpMax = static_cast<Sci_PositionCR>(selEnd);
        rangeStart.lpstrText  = strBuf.get();
        scintillaCall.GetTextRange(&rangeStart);

        std::wstring selText = CUnicodeUtils::StdGetUnicode(strBuf.get());
        changeFunc(selText);
        std::string sUpper = CUnicodeUtils::StdGetUTF8(selText);

        scintillaCall.SetTargetStart(selStart);
        scintillaCall.SetTargetEnd(selEnd);
        scintillaCall.ReplaceTarget(-1, sUpper.c_str());
        scintillaCall.SetSelectionNStart(i, selStart);
        scintillaCall.SetSelectionNEnd(i, selEnd);
    }
    scintillaCall.EndUndoAction();

    return true;
}

bool CCmdConvertUppercase::Execute()
{
    return ChangeCase(CCmdConvertUppercase::Scintilla(), [](auto& selText) { CharUpper(selText.data()); });
}

bool CCmdConvertLowercase::Execute()
{
    return ChangeCase(CCmdConvertLowercase::Scintilla(), [](auto& selText) { CharLower(selText.data()); });
}

bool CCmdConvertTitlecase::Execute()
{
    return ChangeCase(CCmdConvertTitlecase::Scintilla(), [](auto& selText) {
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
