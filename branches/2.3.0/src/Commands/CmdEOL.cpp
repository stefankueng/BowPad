// This file is part of BowPad.
//
// Copyright (C) 2014, 2016 - Stefan Kueng
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
#include "CmdEol.h"
#include "Document.h"

bool CCmdEOLBase::Execute()
{
    auto lineType = GetLineType();
    ScintillaCall(SCI_SETEOLMODE, lineType);
    ScintillaCall(SCI_CONVERTEOLS, lineType);
    if (HasActiveDocument())
    {
        auto& doc = GetModActiveDocument();
        doc.m_format = ToEOLFormat(lineType);
        UpdateStatusBar(true);
    }
    InvalidateUICommand(cmdEOLWin,  UI_INVALIDATIONS_PROPERTY, &UI_PKEY_BooleanValue);
    InvalidateUICommand(cmdEOLUnix, UI_INVALIDATIONS_PROPERTY, &UI_PKEY_BooleanValue);
    InvalidateUICommand(cmdEOLMac,  UI_INVALIDATIONS_PROPERTY, &UI_PKEY_BooleanValue);
    return true;
}

void CCmdEOLBase::TabNotify(TBHDR* ptbhdr)
{
    if (ptbhdr->hdr.code == TCN_SELCHANGE)
    {
        InvalidateUICommand(UI_INVALIDATIONS_PROPERTY, &UI_PKEY_BooleanValue);
    }
}

HRESULT CCmdEOLBase::IUICommandHandlerUpdateProperty(REFPROPERTYKEY key, const PROPVARIANT* /*ppropvarCurrentValue*/, PROPVARIANT* ppropvarNewValue)
{
    if (UI_PKEY_BooleanValue == key)
    {
        return UIInitPropertyFromBoolean(UI_PKEY_BooleanValue, ScintillaCall(SCI_GETEOLMODE)==GetLineType(), ppropvarNewValue);
    }
    return E_NOTIMPL;
}

CCmdEOLWin::CCmdEOLWin(void* obj) : CCmdEOLBase(obj)
{
    InvalidateUICommand(UI_INVALIDATIONS_PROPERTY, &UI_PKEY_BooleanValue);
}

CCmdEOLUnix::CCmdEOLUnix(void* obj) : CCmdEOLBase(obj)
{
    InvalidateUICommand(UI_INVALIDATIONS_PROPERTY, &UI_PKEY_BooleanValue);
}

CCmdEOLMac::CCmdEOLMac(void* obj) : CCmdEOLBase(obj)
{
    InvalidateUICommand(UI_INVALIDATIONS_PROPERTY, &UI_PKEY_BooleanValue);
}
