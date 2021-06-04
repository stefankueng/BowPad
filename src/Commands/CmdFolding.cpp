// This file is part of BowPad.
//
// Copyright (C) 2015-2017, 2020-2021 - Stefan Kueng
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
#include "ResString.h"
#include "StringUtils.h"
#include "UnicodeUtils.h"
#include "BowPad.h"

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

static bool Fold(std::function<sptr_t(int, uptr_t, sptr_t)> scintillaCall, int level2Collapse = -1)
{
    FoldLevelStack levelStack;
    ResString      rFoldText(g_hRes, IDS_FOLDTEXT);
    auto           sFoldTextA = CUnicodeUtils::StdGetUTF8(rFoldText);

    scintillaCall(SCI_SETDEFAULTFOLDDISPLAYTEXT, 0, reinterpret_cast<sptr_t>("..."));

    auto maxLine = scintillaCall(SCI_GETLINECOUNT, 0, 0);
    int  mode    = 0;
    for (auto line = 0; line < maxLine; ++line)
    {
        auto info = scintillaCall(SCI_GETFOLDLEVEL, line, 0);
        if ((info & SC_FOLDLEVELHEADERFLAG) != 0)
        {
            int level = info & SC_FOLDLEVELNUMBERMASK;
            level -= SC_FOLDLEVELBASE;
            levelStack.push(level);
            if (level2Collapse < 0 || levelStack.levelCount == level2Collapse)
            {
                mode = scintillaCall(SCI_GETFOLDEXPANDED, line, 0) ? 0 : 1;
                break;
            }
        }
    }

    for (auto line = 0; line < maxLine; ++line)
    {
        auto info = scintillaCall(SCI_GETFOLDLEVEL, line, 0);
        if ((info & SC_FOLDLEVELHEADERFLAG) != 0)
        {
            int level = info & SC_FOLDLEVELNUMBERMASK;
            level -= SC_FOLDLEVELBASE;
            levelStack.push(level);
            if (level2Collapse < 0 || levelStack.levelCount == level2Collapse)
            {
                if (scintillaCall(SCI_GETFOLDEXPANDED, line, 0) != mode)
                {
                    auto endStyled = scintillaCall(SCI_GETENDSTYLED, 0, 0);
                    auto len       = scintillaCall(SCI_GETTEXTLENGTH, 0, 0);

                    if (endStyled < len)
                        scintillaCall(SCI_COLOURISE, 0, -1);

                    auto headerLine = 0;
                    if (info & SC_FOLDLEVELHEADERFLAG)
                        headerLine = line;
                    else
                    {
                        headerLine = static_cast<int>(scintillaCall(SCI_GETFOLDPARENT, line, 0));
                        if (headerLine == -1)
                            return true;
                    }

                    if (scintillaCall(SCI_GETFOLDEXPANDED, headerLine, 0) != mode)
                    {
                        auto endLine   = scintillaCall(SCI_GETLASTCHILD, line, -1);
                        auto sFoldText = CStringUtils::Format(sFoldTextA.c_str(), static_cast<int>(endLine - line + 1));

                        scintillaCall(SCI_TOGGLEFOLDSHOWTEXT, headerLine, reinterpret_cast<sptr_t>(sFoldText.c_str()));
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
    : ICommand(obj)
    , m_customId(customId)
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

void CCmdInitFoldingMargin::TabNotify(TBHDR* ptbHdr)
{
    // Switching to this document.
    if (ptbHdr->hdr.code == TCN_SELCHANGE)
    {
        // Effectively query the margin width once and remember it.
        // Turning folding off sets the width to 0. Turning folding on restores the width to this saved value.
        if (g_marginWidth == -1)
            g_marginWidth = static_cast<int>(ScintillaCall(SCI_GETMARGINWIDTHN, SC_MARGIN_BACK, 0));

        bool isOn       = static_cast<int>(ScintillaCall(SCI_GETMARGINWIDTHN, SC_MARGIN_BACK, 0)) > 0;
        bool shouldBeOn = CIniSettings::Instance().GetInt64(
                              ShowFoldingMarginSettingSection, ShowFoldingMarginSettingName, g_marginWidth > 0 ? 1 : 0) != 0;
        if (isOn != shouldBeOn)
            ScintillaCall(SCI_SETMARGINWIDTHN, SC_MARGIN_BACK, shouldBeOn ? g_marginWidth : 0);
    }
}

void CCmdInitFoldingMargin::ScintillaNotify(SCNotification* pScn)
{
    // instead of using the flag SC_AUTOMATICFOLD_CLICK with SCI_SETAUTOMATICFOLD,
    // we handle the click here separately so we can set the collapsed fold text.
    if ((pScn->nmhdr.code == SCN_MARGINCLICK) && (pScn->margin == SC_MARGE_FOLDER))
    {
        const bool  ctrl      = (pScn->modifiers & SCMOD_CTRL) != 0;
        const bool  shift     = (pScn->modifiers & SCMOD_SHIFT) != 0;
        const auto& lineClick = ScintillaCall(SCI_LINEFROMPOSITION, pScn->position);
        if (shift && ctrl)
        {
            Fold([=](int cmd, uptr_t wParam, sptr_t lParam) -> sptr_t {
                return ScintillaCall(cmd, wParam, lParam);
            });
        }
        else
        {
            const auto levelClick = ScintillaCall(SCI_GETFOLDLEVEL, lineClick, 0);
            if (levelClick & SC_FOLDLEVELHEADERFLAG)
            {
                ResString rFoldText(g_hRes, IDS_FOLDTEXT);
                auto      sFoldTextA = CUnicodeUtils::StdGetUTF8(rFoldText);

                if (shift)
                {
                    // Ensure all children visible
                    ScintillaCall(SCI_EXPANDCHILDREN, lineClick, levelClick);
                }
                else if (ctrl)
                {
                    auto maxLine = ScintillaCall(SCI_GETLASTCHILD, lineClick, -1);
                    auto mode    = ScintillaCall(SCI_GETFOLDEXPANDED, lineClick, 0) ? 0 : 1;

                    for (auto line = lineClick; line < maxLine; ++line)
                    {
                        auto info = ScintillaCall(SCI_GETFOLDLEVEL, line, 0);
                        if ((info & SC_FOLDLEVELHEADERFLAG) != 0)
                        {
                            int level = info & SC_FOLDLEVELNUMBERMASK;
                            level -= SC_FOLDLEVELBASE;
                            if (ScintillaCall(SCI_GETFOLDEXPANDED, line, 0) != mode)
                            {
                                auto endStyled = ScintillaCall(SCI_GETENDSTYLED, 0, 0);
                                auto len       = ScintillaCall(SCI_GETTEXTLENGTH, 0, 0);

                                if (endStyled < len)
                                    ScintillaCall(SCI_COLOURISE, 0, -1);

                                sptr_t headerLine = 0;
                                if (info & SC_FOLDLEVELHEADERFLAG)
                                    headerLine = line;
                                else
                                {
                                    headerLine = ScintillaCall(SCI_GETFOLDPARENT, line, 0);
                                    if (headerLine == -1)
                                        return;
                                }

                                if (ScintillaCall(SCI_GETFOLDEXPANDED, headerLine, 0) != mode)
                                {
                                    auto endLine   = ScintillaCall(SCI_GETLASTCHILD, line, -1);
                                    auto sFoldText = CStringUtils::Format(sFoldTextA.c_str(), static_cast<int>(endLine - line + 1));

                                    ScintillaCall(SCI_TOGGLEFOLDSHOWTEXT, headerLine, reinterpret_cast<sptr_t>(sFoldText.c_str()));
                                }
                            }
                        }
                    }

                    //ScintillaCall(SCI_FOLDCHILDREN, lineClick, SC_FOLDACTION_TOGGLE);
                }
                else
                {
                    // Toggle this line
                    auto endStyled = ScintillaCall(SCI_GETENDSTYLED, 0, 0);
                    auto len       = ScintillaCall(SCI_GETTEXTLENGTH, 0, 0);

                    if (endStyled < len)
                        ScintillaCall(SCI_COLOURISE, 0, -1);

                    auto headerLine = lineClick;

                    auto endLine   = ScintillaCall(SCI_GETLASTCHILD, lineClick, -1);
                    auto sFoldText = CStringUtils::Format(sFoldTextA.c_str(), static_cast<int>(endLine - lineClick + 1));

                    ScintillaCall(SCI_TOGGLEFOLDSHOWTEXT, headerLine, reinterpret_cast<sptr_t>(sFoldText.c_str()));

                    //ScintillaCall(SCI_FOLDLINE, lineClick, SC_FOLDACTION_TOGGLE);
                }
            }
        }
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
    bool isOn = static_cast<int>(ScintillaCall(SCI_GETMARGINWIDTHN, SC_MARGIN_BACK, 0)) > 0;
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
    bool isOn = static_cast<int>(ScintillaCall(SCI_GETMARGINWIDTHN, SC_MARGIN_BACK, 0)) > 0;
    if (isOn)
    {
        ScintillaCall(SCI_FOLDALL, SC_FOLDACTION_EXPAND);
        ScintillaCall(SCI_SETMARGINWIDTHN, SC_MARGIN_BACK, 0);
        CIniSettings::Instance().SetInt64(ShowFoldingMarginSettingSection, ShowFoldingMarginSettingName, 0);
    }
    return true;
}
