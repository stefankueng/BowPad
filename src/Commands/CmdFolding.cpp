// This file is part of BowPad.
//
// Copyright (C) 2015-2017, 2020 - Stefan Kueng
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
#include "SciLexer.h"

#include <functional>

namespace
{
int           g_marginWidth                     = -1;
const wchar_t ShowFoldingMarginSettingSection[] = L"View";
const wchar_t ShowFoldingMarginSettingName[]    = L"ShowFoldingMargin";

class FoldLevelStack
{
public:
    int levelCount = 0; // 1-based level number
    int levelStack[12]{};

    void push(int level)
    {
        while (levelCount != 0 && level <= levelStack[levelCount - 1])
        {
            --levelCount;
        }
        if (levelCount > (_countof(levelStack) - 1))
            return;
        levelStack[levelCount++] = level;
    }
};
} // namespace

static bool Fold(std::function<sptr_t(int, uptr_t, sptr_t)> ScintillaCall, int level2Collapse = -1)
{
    FoldLevelStack levelStack;

    ScintillaCall(SCI_SETDEFAULTFOLDDISPLAYTEXT, 0, (sptr_t) "...");

    auto maxLine = ScintillaCall(SCI_GETLINECOUNT, 0, 0);
    int  mode    = 0;
    for (auto line = 0; line < maxLine; ++line)
    {
        auto info = ScintillaCall(SCI_GETFOLDLEVEL, line, 0);
        if ((info & SC_FOLDLEVELHEADERFLAG) != 0)
        {
            int level = info & SC_FOLDLEVELNUMBERMASK;
            level -= SC_FOLDLEVELBASE;
            levelStack.push(level);
            if (level2Collapse < 0 || levelStack.levelCount == level2Collapse)
            {
                mode = ScintillaCall(SCI_GETFOLDEXPANDED, line, 0) ? 0 : 1;
                break;
            }
        }
    }

    for (auto line = 0; line < maxLine; ++line)
    {
        auto info = ScintillaCall(SCI_GETFOLDLEVEL, line, 0);
        if ((info & SC_FOLDLEVELHEADERFLAG) != 0)
        {
            int level = info & SC_FOLDLEVELNUMBERMASK;
            level -= SC_FOLDLEVELBASE;
            levelStack.push(level);
            if (level2Collapse < 0 || levelStack.levelCount == level2Collapse)
            {
                if (ScintillaCall(SCI_GETFOLDEXPANDED, line, 0) != mode)
                {
                    auto endStyled = ScintillaCall(SCI_GETENDSTYLED, 0, 0);
                    auto len       = ScintillaCall(SCI_GETTEXTLENGTH, 0, 0);

                    if (endStyled < len)
                        ScintillaCall(SCI_COLOURISE, 0, -1);

                    auto headerLine = 0;
                    if (info & SC_FOLDLEVELHEADERFLAG)
                        headerLine = line;
                    else
                    {
                        headerLine = (int)ScintillaCall(SCI_GETFOLDPARENT, line, 0);
                        if (headerLine == -1)
                            return true;
                    }

                    if (ScintillaCall(SCI_GETFOLDEXPANDED, headerLine, 0) != mode)
                    {
                        ScintillaCall(SCI_TOGGLEFOLDSHOWTEXT, headerLine, (sptr_t) "...");
                        //line = ScintillaCall(SCI_GETLASTCHILD, line, level2Collapse);
                    }
                }
            }
        }
    }
    return true;
}

CCmdFoldAll::CCmdFoldAll(void* obj)
    : ICommand(obj)
{
}

void CCmdFoldAll::AfterInit()
{
    InvalidateUICommand(UI_INVALIDATIONS_PROPERTY, &UI_PKEY_BooleanValue);
}

bool CCmdFoldAll::Execute()
{
    return Fold([=](int cmd, uptr_t wParam, sptr_t lParam) -> sptr_t {
        return ScintillaCall(cmd, wParam, lParam);
    });
}

CCmdFoldLevel::CCmdFoldLevel(UINT customId, void* obj)
    : m_customId(customId)
    , ICommand(obj)
{
    m_customCmdId = cmdFoldLevel0 + customId;
}

bool CCmdFoldLevel::Execute()
{
    return Fold([=](int cmd, uptr_t wParam, sptr_t lParam) -> sptr_t {
        return ScintillaCall(cmd, wParam, lParam);
    },
                m_customId);
}

CCmdInitFoldingMargin::CCmdInitFoldingMargin(void* obj)
    : ICommand(obj)
{
}

void CCmdInitFoldingMargin::AfterInit()
{
    InvalidateUICommand(UI_INVALIDATIONS_PROPERTY, &UI_PKEY_BooleanValue);
}

void CCmdInitFoldingMargin::TabNotify(TBHDR* ptbhdr)
{
    // Switching to this document.
    if (ptbhdr->hdr.code == TCN_SELCHANGE)
    {
        // Effectively query the margin width once and remember it.
        // Turning folding off sets the width to 0. Turning folding on restores the width to this saved value.
        if (g_marginWidth == -1)
            g_marginWidth = (int)ScintillaCall(SCI_GETMARGINWIDTHN, SC_MARGIN_BACK, sptr_t(0));

        bool isOn       = ((int)ScintillaCall(SCI_GETMARGINWIDTHN, SC_MARGIN_BACK, sptr_t(0))) > 0;
        bool shouldBeOn = CIniSettings::Instance().GetInt64(
                              ShowFoldingMarginSettingSection, ShowFoldingMarginSettingName, g_marginWidth > 0 ? 1 : 0) != 0;
        if (isOn != shouldBeOn)
            ScintillaCall(SCI_SETMARGINWIDTHN, SC_MARGIN_BACK, shouldBeOn ? g_marginWidth : 0);
    }
}

bool CCmdInitFoldingMargin::Execute()
{
    return true;
}

CCmdFoldingOn::CCmdFoldingOn(void* obj)
    : ICommand(obj)
{
}

void CCmdFoldingOn::AfterInit()
{
    InvalidateUICommand(UI_INVALIDATIONS_PROPERTY, &UI_PKEY_BooleanValue);
}

bool CCmdFoldingOn::Execute()
{
    bool isOn = ((int)ScintillaCall(SCI_GETMARGINWIDTHN, SC_MARGIN_BACK, sptr_t(0))) > 0;
    if (!isOn)
    {
        ScintillaCall(SCI_SETMARGINWIDTHN, SC_MARGIN_BACK, g_marginWidth);
        CIniSettings::Instance().SetInt64(ShowFoldingMarginSettingSection, ShowFoldingMarginSettingName, 1);
    }
    return true;
}

CCmdFoldingOff::CCmdFoldingOff(void* obj)
    : ICommand(obj)
{
}

void CCmdFoldingOff::AfterInit()
{
    InvalidateUICommand(UI_INVALIDATIONS_PROPERTY, &UI_PKEY_BooleanValue);
}

bool CCmdFoldingOff::Execute()
{
    bool isOn = ((int)ScintillaCall(SCI_GETMARGINWIDTHN, SC_MARGIN_BACK, sptr_t(0))) > 0;
    if (isOn)
    {
        ScintillaCall(SCI_FOLDALL, SC_FOLDACTION_EXPAND);
        ScintillaCall(SCI_SETMARGINWIDTHN, SC_MARGIN_BACK, 0);
        CIniSettings::Instance().SetInt64(ShowFoldingMarginSettingSection, ShowFoldingMarginSettingName, 0);
    }
    return true;
}
