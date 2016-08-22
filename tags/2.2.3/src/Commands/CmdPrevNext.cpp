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
#include "CmdPrevNext.h"
#include "OnOutOfScope.h"

#include <deque>

class PositionData
{
public:
    PositionData()
        : id(-1)
        , line(-1)
        , column(-1)
    {
    }
    PositionData(int i, long l, long c)
        : id(i)
        , line(l)
        , column(c)
    {
    }

    bool operator == (const PositionData& other)
    {
        return (this->id == other.id && this->line == other.line && this->column == other.column);
    }
    bool operator != (const PositionData& other)
    {
        return (this->id != other.id || this->line != other.line || this->column != other.column);
    }

    int           id; // Doc Id.
    long          line;
    long          column;
};

namespace
{
std::deque<PositionData>    g_positions;
int                         g_currentDocId = -1;
size_t                      g_offsetBeforeEnd = 0;
long                        g_currentLine = -1;
bool                        g_ignore = false;
const long                  MAX_PREV_NEXT_POSITIONS = 500;
// Only store a new position if it's more than N lines from the old one.
const long                  POSITION_SAVE_GRANULARITY = 10;

void AddNewPosition(long id, long line, long col)
{
    PositionData data(id, line, col);
    if (g_positions.empty() || (g_positions.back() != data))
        g_positions.push_back(data);
}

bool ResizePositionSpace()
{
    if (g_offsetBeforeEnd)
    {
        size_t newSize = g_positions.size() - g_offsetBeforeEnd + 1;
        g_positions.resize(newSize);
        g_offsetBeforeEnd = 0;
        return true;
    }
    return false;
}

void SetCurrentLine(long line)
{
    g_currentLine = line;
}

}

void CCmdPrevNext::ScintillaNotify( Scintilla::SCNotification* pScn )
{
    switch (pScn->nmhdr.code)
    {
    case SCN_UPDATEUI:
        {
            if (g_ignore)
                return;
            if (g_offsetBeforeEnd >= (int)g_positions.size())
                g_offsetBeforeEnd = 0;
            long line = (long) ScintillaCall(SCI_LINEFROMPOSITION, ScintillaCall(SCI_GETCURRENTPOS));
            if (g_currentDocId < 0 || g_currentLine == -1)
            {
                ResizePositionSpace();
                g_currentDocId = GetDocIdOfCurrentTab();
                if (g_currentDocId >= 0)
                {
                    long col = (long) ScintillaCall(SCI_GETCOLUMN, ScintillaCall(SCI_GETCURRENTPOS));
                    AddNewPosition(g_currentDocId, line, col);
                    InvalidateUICommand(cmdPrevious, UI_INVALIDATIONS_STATE, nullptr);
                    InvalidateUICommand(cmdNext, UI_INVALIDATIONS_STATE, nullptr);
                }
                return;
            }
            else if (g_currentLine != line)
            {
                auto mark = [&]()
                {
                    ResizePositionSpace();
                    long col = (long)ScintillaCall(SCI_GETCOLUMN, ScintillaCall(SCI_GETCURRENTPOS));
                    AddNewPosition(g_currentDocId, line, col);
                    InvalidateUICommand(cmdPrevious, UI_INVALIDATIONS_STATE, nullptr);
                    InvalidateUICommand(cmdNext, UI_INVALIDATIONS_STATE, nullptr);
                    SetCurrentLine(line);
                };
                // Store a new position if it's more than 10 lines from the old one.
                if (g_currentLine > line)
                {
                    auto theDistance = g_currentLine - line;
                    if (theDistance > POSITION_SAVE_GRANULARITY)
                        mark();
                }
                else if (g_currentLine < line)
                {
                    auto theDistance = line - g_currentLine;
                    if (theDistance > POSITION_SAVE_GRANULARITY)
                        mark();
                }
            }
            // don't store too many positions, drop the oldest ones
            if (g_positions.size() > MAX_PREV_NEXT_POSITIONS)
                g_positions.pop_front();
        }
        break;
    }
}

void CCmdPrevNext::TabNotify( TBHDR* ptbhdr )
{
    switch (ptbhdr->hdr.code)
    {
    case TCN_SELCHANGE:
        {
            // document got activated
            g_currentDocId = GetDocIdOfCurrentTab();
        }
        break;
    // Not definite event of closure. User may cancel so don't erase saved
    // positions here. Do it on OnDocumentClose.
    // case TCN_TABDELETE:
    }
}

void CCmdPrevNext::OnDocumentClose(int tab)
{
    auto docId = this->GetDocIDFromTabIndex(tab);
    for (auto it = g_positions.begin(); it != g_positions.end();)
    {
        if (it->id == docId)
            it = g_positions.erase(it);
        else
            ++it;
    }
    g_offsetBeforeEnd = 0;
    InvalidateUICommand(cmdPrevious, UI_INVALIDATIONS_STATE, nullptr);
    InvalidateUICommand(cmdNext, UI_INVALIDATIONS_STATE, nullptr);
}

bool CCmdPrevious::Execute()
{
    size_t i = 0;
    PositionData data;
    for (auto it = g_positions.crbegin(); it != g_positions.crend(); ++it)
    {
        if (i == g_offsetBeforeEnd + 1)
        {
            data = *it;
            break;
        }
        ++i;
    }

    if (data.id >= 0)
    {
        // Set ignore to true so we don't move the cursor to a position
        // and in doing so trigger that position recorded as it already is.
        g_ignore = true;
        OnOutOfScope
        (
            if (g_ignore)
                g_ignore = false;
        );
        if (GetDocIdOfCurrentTab() != data.id)
            TabActivateAt(GetTabIndexFromDocID(data.id));

        long pos = (long) ScintillaCall(SCI_FINDCOLUMN, data.line, data.column);
        ScintillaCall(SCI_SETANCHOR, pos);
        ScintillaCall(SCI_SETCURRENTPOS, pos);
        ScintillaCall(SCI_CANCEL);
        ScintillaCall(SCI_SCROLLCARET);
        ScintillaCall(SCI_GRABFOCUS);
        SetCurrentLine(data.line);
        ++g_offsetBeforeEnd;
        g_ignore = false;
        InvalidateUICommand(cmdPrevious, UI_INVALIDATIONS_STATE, nullptr);
        InvalidateUICommand(cmdNext, UI_INVALIDATIONS_STATE, nullptr);
        return true;
    }
    return false;
}

HRESULT CCmdPrevious::IUICommandHandlerUpdateProperty( REFPROPERTYKEY key, const PROPVARIANT* /*ppropvarCurrentValue*/, PROPVARIANT* ppropvarNewValue )
{
    // Enabled if there's something to go back to.
    if (UI_PKEY_Enabled == key)
    {
        return UIInitPropertyFromBoolean(UI_PKEY_Enabled, g_positions.size() > g_offsetBeforeEnd + 1, ppropvarNewValue);
    }
    return E_NOTIMPL;
}

bool CCmdNext::Execute()
{
    size_t i = 0;
    PositionData data;
    if (g_offsetBeforeEnd == 0)
        return false;
    for (auto it = g_positions.crbegin(); it != g_positions.crend(); ++it)
    {
        if (i == g_offsetBeforeEnd - 1)
        {
            data = *it;
            break;
        }
        ++i;
    }

    if (data.id >= 0)
    {
        g_ignore = true;
        OnOutOfScope
        (
            if (g_ignore)
                g_ignore = false;
        );
        if (GetDocIdOfCurrentTab() != data.id)
            TabActivateAt(GetTabIndexFromDocID(data.id));
        --g_offsetBeforeEnd;
        long pos = (long ) ScintillaCall(SCI_FINDCOLUMN, data.line, data.column);
        ScintillaCall(SCI_SETANCHOR, pos);
        ScintillaCall(SCI_SETCURRENTPOS, pos);
        ScintillaCall(SCI_CANCEL);
        ScintillaCall(SCI_SCROLLCARET);
        ScintillaCall(SCI_GRABFOCUS);
        SetCurrentLine(data.line);
        g_ignore = false;
        InvalidateUICommand(cmdPrevious, UI_INVALIDATIONS_STATE, nullptr);
        InvalidateUICommand(cmdNext, UI_INVALIDATIONS_STATE, nullptr);
        return true;
    }
    return false;
}

HRESULT CCmdNext::IUICommandHandlerUpdateProperty( REFPROPERTYKEY key, const PROPVARIANT* /*ppropvarCurrentValue*/, PROPVARIANT* ppropvarNewValue )
{
    // enabled if there's something to go forward to
    if (UI_PKEY_Enabled == key)
    {
        return UIInitPropertyFromBoolean(UI_PKEY_Enabled, g_offsetBeforeEnd > 0, ppropvarNewValue);
    }
    return E_NOTIMPL;
}
