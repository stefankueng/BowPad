// This file is part of BowPad.
//
// Copyright (C) 2015 - Stefan Kueng
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
#include "CmdFolding.h"

#include <functional>

static bool Fold(std::function<sptr_t(int, uptr_t, sptr_t)> ScintillaCall, int level2Collapse = -1)
{
    auto maxLine = ScintillaCall(SCI_GETLINECOUNT, 0, 0);
    int mode = 0;
    for (auto line = 0; line < maxLine; ++line)
    {
        auto level = ScintillaCall(SCI_GETFOLDLEVEL, line, 0);
        if (level & SC_FOLDLEVELHEADERFLAG)
        {
            level -= SC_FOLDLEVELBASE;
            if ((level2Collapse < 0) || (level2Collapse == (level & SC_FOLDLEVELNUMBERMASK)))
            {
                mode = ScintillaCall(SCI_GETFOLDEXPANDED, line, 0) ? 0 : 1;
                break;
            }
        }
    }

    for (auto line = 0; line < maxLine; ++line)
    {
        auto level = ScintillaCall(SCI_GETFOLDLEVEL, line, 0);
        if (level & SC_FOLDLEVELHEADERFLAG)
        {
            level -= SC_FOLDLEVELBASE;
            if ((level2Collapse < 0) || (level2Collapse == (level & SC_FOLDLEVELNUMBERMASK)))
            {
                if (ScintillaCall(SCI_GETFOLDEXPANDED, line, 0) != mode)
                {
                    auto endStyled = ScintillaCall(SCI_GETENDSTYLED, 0, 0);
                    auto len = ScintillaCall(SCI_GETTEXTLENGTH, 0, 0);

                    if (endStyled < len)
                        ScintillaCall(SCI_COLOURISE, 0, -1);

                    level = ScintillaCall(SCI_GETFOLDLEVEL, line, 0);

                    auto headerLine = 0;
                    if (level & SC_FOLDLEVELHEADERFLAG)
                        headerLine = line;
                    else
                    {
                        headerLine = (int)ScintillaCall(SCI_GETFOLDPARENT, line, 0);
                        if (headerLine == -1)
                            return true;
                    }

                    if (ScintillaCall(SCI_GETFOLDEXPANDED, headerLine, 0) != mode)
                    {
                        ScintillaCall(SCI_TOGGLEFOLD, headerLine, 0);
                    }
                }
            }
        }
    }
    return true;

}

CCmdFoldAll::CCmdFoldAll(void * obj) : ICommand(obj)
{
    InvalidateUICommand(UI_INVALIDATIONS_PROPERTY, &UI_PKEY_BooleanValue);
}

CCmdFoldAll::~CCmdFoldAll(void)
{
}

bool CCmdFoldAll::Execute()
{
    return Fold([=](int cmd, uptr_t wParam, sptr_t lParam)->sptr_t
    {
        return ScintillaCall(cmd, wParam, lParam);
    });
}



CCmdFoldLevel::CCmdFoldLevel(UINT customId, void * obj)
    : m_customId(customId)
    , ICommand(obj)
{
    m_customCmdId = cmdFoldLevel0 + customId;
}

bool CCmdFoldLevel::Execute()
{
    return Fold([=](int cmd, uptr_t wParam, sptr_t lParam)->sptr_t
    {
        return ScintillaCall(cmd, wParam, lParam);
    }, m_customId);
}
