// This file is part of BowPad.
//
// Copyright (C) 2015-2017, 2020-2022 - Stefan Kueng
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
int               g_marginWidth                     = -1;
constexpr wchar_t ShowFoldingMarginSettingSection[] = L"View";
constexpr wchar_t ShowFoldingMarginSettingName[]    = L"ShowFoldingMargin";

class FoldLevelStack
{
public:
    int                  levelCount = 0; // 1-based level number
    Scintilla::FoldLevel levelStack[12]{};

    void                 push(Scintilla::FoldLevel level)
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

static bool Fold(Scintilla::ScintillaCall& scintilla, int level2Collapse = -1)
{
    FoldLevelStack levelStack;
    ResString      rFoldText(g_hRes, IDS_FOLDTEXT);
    auto           sFoldTextA = CUnicodeUtils::StdGetUTF8(rFoldText);

    scintilla.SetDefaultFoldDisplayText("...");

    auto maxLine = scintilla.LineCount();
    int  mode    = 0;
    for (auto line = 0; line < maxLine; ++line)
    {
        auto info = scintilla.FoldLevel(line);
        if ((info & Scintilla::FoldLevel::HeaderFlag) != Scintilla::FoldLevel::None)
        {
            auto level = info & Scintilla::FoldLevel::NumberMask;
            level      = static_cast<Scintilla::FoldLevel>(static_cast<int>(level) - static_cast<int>(Scintilla::FoldLevel::Base));
            levelStack.push(level);
            if (level2Collapse < 0 || levelStack.levelCount == level2Collapse)
            {
                mode = scintilla.FoldExpanded(line) ? 0 : 1;
                break;
            }
        }
    }

    for (auto line = 0; line < maxLine; ++line)
    {
        auto info = scintilla.FoldLevel(line);
        if ((info & Scintilla::FoldLevel::HeaderFlag) != Scintilla::FoldLevel::None)
        {
            auto level = info & Scintilla::FoldLevel::NumberMask;
            level      = static_cast<Scintilla::FoldLevel>(static_cast<int>(level) - static_cast<int>(Scintilla::FoldLevel::Base));
            levelStack.push(level);
            if (level2Collapse < 0 || levelStack.levelCount == level2Collapse)
            {
                if (scintilla.FoldExpanded(line))
                {
                    auto endStyled = scintilla.EndStyled();
                    auto len       = scintilla.TextLength();

                    if (endStyled < len)
                        scintilla.Colourise(0, -1);

                    auto headerLine = 0;
                    if ((info & Scintilla::FoldLevel::HeaderFlag) != Scintilla::FoldLevel::None)
                        headerLine = line;
                    else
                    {
                        headerLine = static_cast<int>(scintilla.FoldParent(line));
                        if (headerLine == -1)
                            return true;
                    }

                    if (scintilla.FoldExpanded(headerLine))
                    {
                        auto endLine   = scintilla.LastChild(line, static_cast<Scintilla::FoldLevel>(-1));
                        auto sFoldText = CStringUtils::Format(sFoldTextA.c_str(), static_cast<int>(endLine - line + 1));

                        scintilla.ToggleFoldShowText(headerLine, sFoldText.c_str());
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
    return Fold(Scintilla());
}

CCmdFoldLevel::CCmdFoldLevel(UINT customId, void* obj)
    : ICommand(obj)
    , m_customId(customId)
{
    m_customCmdId = cmdFoldLevel0 + customId;
}

bool CCmdFoldLevel::Execute()
{
    return Fold(Scintilla(), m_customId);
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
            g_marginWidth = Scintilla().MarginWidthN(SC_MARGIN_BACK);

        bool isOn       = Scintilla().MarginWidthN(SC_MARGIN_BACK) > 0;
        bool shouldBeOn = CIniSettings::Instance().GetInt64(
                              ShowFoldingMarginSettingSection, ShowFoldingMarginSettingName, g_marginWidth > 0 ? 1 : 0) != 0;
        if (isOn != shouldBeOn)
            Scintilla().SetMarginWidthN(SC_MARGIN_BACK, shouldBeOn ? g_marginWidth : 0);
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
        const auto& lineClick = Scintilla().LineFromPosition(pScn->position);
        if (shift && ctrl)
        {
            Fold(Scintilla());
        }
        else
        {
            const auto levelClick = Scintilla().FoldLevel(lineClick);
            if ((levelClick & Scintilla::FoldLevel::HeaderFlag) != Scintilla::FoldLevel::None)
            {
                ResString rFoldText(g_hRes, IDS_FOLDTEXT);
                auto      sFoldTextA = CUnicodeUtils::StdGetUTF8(rFoldText);

                if (shift)
                {
                    // Ensure all children visible
                    Scintilla().ExpandChildren(lineClick, levelClick);
                }
                else if (ctrl)
                {
                    auto maxLine = Scintilla().LastChild(lineClick, static_cast<Scintilla::FoldLevel>(-1));

                    for (auto line = lineClick; line < maxLine; ++line)
                    {
                        auto info = Scintilla().FoldLevel(line);
                        if ((info & Scintilla::FoldLevel::HeaderFlag) != Scintilla::FoldLevel::None)
                        {
                            auto level = info & Scintilla::FoldLevel::NumberMask;
                            level      = static_cast<Scintilla::FoldLevel>(static_cast<int>(level) - static_cast<int>(Scintilla::FoldLevel::Base));
                            if (Scintilla().FoldExpanded(line))
                            {
                                auto endStyled = Scintilla().EndStyled();
                                auto len       = Scintilla().TextLength();

                                if (endStyled < len)
                                    Scintilla().Colourise(0, -1);

                                sptr_t headerLine = 0;
                                if ((info & Scintilla::FoldLevel::HeaderFlag) != Scintilla::FoldLevel::None)
                                    headerLine = line;
                                else
                                {
                                    headerLine = Scintilla().FoldParent(line);
                                    if (headerLine == -1)
                                        return;
                                }

                                if (Scintilla().FoldExpanded(headerLine))
                                {
                                    auto endLine   = Scintilla().LastChild(line, static_cast<Scintilla::FoldLevel>(-1));
                                    auto sFoldText = CStringUtils::Format(sFoldTextA.c_str(), static_cast<int>(endLine - line + 1));

                                    Scintilla().ToggleFoldShowText(headerLine, sFoldText.c_str());
                                }
                            }
                        }
                    }

                    //.Scintilla().FOLDCHILDREN(lineClick, SC_FOLDACTION_TOGGLE);
                }
                else
                {
                    // Toggle this line
                    auto endStyled = Scintilla().EndStyled();
                    auto len       = Scintilla().TextLength();

                    if (endStyled < len)
                        Scintilla().Colourise(0, -1);

                    auto headerLine = lineClick;

                    auto endLine    = Scintilla().LastChild(lineClick, static_cast<Scintilla::FoldLevel>(-1));
                    auto sFoldText  = CStringUtils::Format(sFoldTextA.c_str(), static_cast<int>(endLine - lineClick + 1));

                    Scintilla().ToggleFoldShowText(headerLine, sFoldText.c_str());

                    //.Scintilla().FOLDLINE(lineClick, SC_FOLDACTION_TOGGLE);
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
    bool isOn = Scintilla().MarginWidthN(SC_MARGIN_BACK) > 0;
    if (!isOn)
    {
        Scintilla().SetMarginWidthN(SC_MARGIN_BACK, g_marginWidth);
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
    bool isOn = Scintilla().MarginWidthN(SC_MARGIN_BACK) > 0;
    if (isOn)
    {
        Scintilla().FoldAll(Scintilla::FoldAction::Expand);
        Scintilla().SetMarginWidthN(SC_MARGIN_BACK, 0);
        CIniSettings::Instance().SetInt64(ShowFoldingMarginSettingSection, ShowFoldingMarginSettingName, 0);
    }
    return true;
}
