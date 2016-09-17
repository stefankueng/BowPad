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
#include "CmdPrevNext.h"

#include <deque>

class PositionData
{
public:
    PositionData()
        : index(-1)
        , line((size_t)-1)
        , column((size_t)-1)
    {
    }
    PositionData(int i, size_t l, size_t c)
        : index(i)
        , line(l)
        , column(c)
    {
    }
    ~PositionData(){}

    int             index;
    size_t          line;
    size_t          column;
};


std::deque<PositionData>    positions;
int                         currentDocIndex = -1;
int                         offsetBeforeEnd = 0;
size_t                      currentline = (size_t)-1;
bool                        ignore = false;

void CCmdPrevNext::ScintillaNotify( Scintilla::SCNotification * pScn )
{
    switch (pScn->nmhdr.code)
    {
    case SCN_UPDATEUI:
        {
            if (ignore)
                return;
            if (offsetBeforeEnd >= positions.size())
                offsetBeforeEnd = 0;
            size_t line = ScintillaCall(SCI_LINEFROMPOSITION, ScintillaCall(SCI_GETCURRENTPOS));
            if ((currentDocIndex < 0) || (currentline == -1))
            {
                if (offsetBeforeEnd)
                {
                    positions.resize(positions.size()-offsetBeforeEnd+1);
                    offsetBeforeEnd = 0;
                }
                currentDocIndex = GetCurrentTabIndex();
                if (currentDocIndex >= 0)
                {
                    size_t col  = ScintillaCall(SCI_GETCOLUMN, ScintillaCall(SCI_GETCURRENTPOS));
                    PositionData data(currentDocIndex, line, col);
                    positions.push_back(data);
                    currentline = line;
                    InvalidateUICommand(cmdPrevious, UI_INVALIDATIONS_STATE, NULL);
                    InvalidateUICommand(cmdNext, UI_INVALIDATIONS_STATE, NULL);
                }
                return;
            }
            else
            {
                // store a new position if it's more than 10 lines from the old one
                if (currentline > line)
                {
                    if ((currentline - line) > 10)
                    {
                        if (offsetBeforeEnd)
                        {
                            positions.resize(positions.size()-offsetBeforeEnd+1);
                            offsetBeforeEnd = 0;
                        }
                        size_t col  = ScintillaCall(SCI_GETCOLUMN, ScintillaCall(SCI_GETCURRENTPOS));
                        PositionData data(currentDocIndex, line, col);
                        positions.push_back(data);
                        InvalidateUICommand(cmdPrevious, UI_INVALIDATIONS_STATE, NULL);
                        InvalidateUICommand(cmdNext, UI_INVALIDATIONS_STATE, NULL);
                        currentline = line;
                    }
                }
                else if (currentline < line)
                {
                    if ((line - currentline) > 10)
                    {
                        if (offsetBeforeEnd)
                        {
                            positions.resize(positions.size()-offsetBeforeEnd+1);
                            offsetBeforeEnd = 0;
                        }
                        size_t col  = ScintillaCall(SCI_GETCOLUMN, ScintillaCall(SCI_GETCURRENTPOS));
                        PositionData data(currentDocIndex, line, col);
                        positions.push_back(data);
                        InvalidateUICommand(cmdPrevious, UI_INVALIDATIONS_STATE, NULL);
                        InvalidateUICommand(cmdNext, UI_INVALIDATIONS_STATE, NULL);
                        currentline = line;
                    }
                }
            }
            // don't store too many positions, drop the oldest ones
            if (positions.size() > 500)
                positions.pop_front();
        }
        break;
    }
}

void CCmdPrevNext::TabNotify( TBHDR * ptbhdr )
{
    switch (ptbhdr->hdr.code)
    {
    case TCN_SELCHANGE:
        {
            // document got activated
            currentDocIndex = GetCurrentTabIndex();
        }
        break;
    case TCN_TABDROPPED:
        {
            // adjust the indexes
            int src = GetSrcTab();
            int dst = GetDstTab();
            for (auto it : positions)
            {
                if (it.index == src)
                    it.index = dst;
                else if (it.index == dst)
                    it.index = src;
            }
        }
        break;
    case TCN_TABDELETE:
        {
            // remove all positions in that tab
            auto it = positions.begin();
            size_t i = 0;
            for (; it != positions.end();)
            {
                if (it->index == ptbhdr->tabOrigin)
                {
                    it = positions.erase(it);
                }
                else
                {
                    if (it->index > ptbhdr->tabOrigin)
                        it->index--;
                    ++it;
                }
                ++i;
            }
            offsetBeforeEnd = 0;
            InvalidateUICommand(cmdPrevious, UI_INVALIDATIONS_STATE, NULL);
            InvalidateUICommand(cmdNext, UI_INVALIDATIONS_STATE, NULL);
        }
        break;
    }
}


bool CCmdPrevious::Execute()
{
    size_t i = 0;
    PositionData data;
    for (auto it = positions.crbegin(); it != positions.crend(); ++it)
    {
        if (i == offsetBeforeEnd)
        {
            data = *it;
            break;
        }
        ++i;
    }

    if (data.index >= 0)
    {
        ignore = true;
        if (GetCurrentTabIndex() != data.index)
            TabActivateAt(data.index);

        size_t pos = ScintillaCall(SCI_FINDCOLUMN, data.line, data.column);
        ScintillaCall(SCI_SETANCHOR, pos);
        ScintillaCall(SCI_SETCURRENTPOS, pos);
        ScintillaCall(SCI_CANCEL);
        ScintillaCall(SCI_SCROLLCARET);
        ScintillaCall(SCI_GRABFOCUS);
        currentline = data.line;
        offsetBeforeEnd++;
        ignore = false;
        InvalidateUICommand(cmdPrevious, UI_INVALIDATIONS_STATE, NULL);
        InvalidateUICommand(cmdNext, UI_INVALIDATIONS_STATE, NULL);
        return true;
    }
    return false;
}

HRESULT CCmdPrevious::IUICommandHandlerUpdateProperty( REFPROPERTYKEY key, const PROPVARIANT* /*ppropvarCurrentValue*/, PROPVARIANT* ppropvarNewValue )
{
    // enabled if there's something to go back to
    if (UI_PKEY_Enabled == key)
    {
        return UIInitPropertyFromBoolean(UI_PKEY_Enabled, positions.size() > offsetBeforeEnd, ppropvarNewValue);
    }
    return E_NOTIMPL;
}

bool CCmdNext::Execute()
{
    size_t i = 0;
    PositionData data;
    if (offsetBeforeEnd == 0)
        return false;
    --offsetBeforeEnd;
    for (auto it = positions.crbegin(); it != positions.crend(); ++it)
    {
        if (i == offsetBeforeEnd)
        {
            data = *it;
            break;
        }
        ++i;
    }

    if (data.index >= 0)
    {
        ignore = true;
        if (GetCurrentTabIndex() != data.index)
            TabActivateAt(data.index);

        size_t pos = ScintillaCall(SCI_FINDCOLUMN, data.line, data.column);
        ScintillaCall(SCI_SETANCHOR, pos);
        ScintillaCall(SCI_SETCURRENTPOS, pos);
        ScintillaCall(SCI_CANCEL);
        ScintillaCall(SCI_SCROLLCARET);
        ScintillaCall(SCI_GRABFOCUS);
        currentline = data.line;
        ignore = false;
        InvalidateUICommand(cmdPrevious, UI_INVALIDATIONS_STATE, NULL);
        InvalidateUICommand(cmdNext, UI_INVALIDATIONS_STATE, NULL);
        return true;
    }
    return false;
}

HRESULT CCmdNext::IUICommandHandlerUpdateProperty( REFPROPERTYKEY key, const PROPVARIANT* /*ppropvarCurrentValue*/, PROPVARIANT* ppropvarNewValue )
{
    // enabled if there's something to go forward to
    if (UI_PKEY_Enabled == key)
    {
        return UIInitPropertyFromBoolean(UI_PKEY_Enabled, offsetBeforeEnd > 0, ppropvarNewValue);
    }
    return E_NOTIMPL;
}