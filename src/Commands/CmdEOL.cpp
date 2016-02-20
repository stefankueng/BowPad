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

CCmdEOLWin::CCmdEOLWin(void * obj) : ICommand(obj)
{
    InvalidateUICommand(UI_INVALIDATIONS_PROPERTY, &UI_PKEY_BooleanValue);
}

CCmdEOLWin::~CCmdEOLWin()
{
}

bool CCmdEOLWin::Execute()
{
    ScintillaCall(SCI_SETEOLMODE, SC_EOL_CRLF);
    ScintillaCall(SCI_CONVERTEOLS, SC_EOL_CRLF);
    if (HasActiveDocument())
    {
        CDocument doc = GetActiveDocument();
        doc.m_format = WIN_FORMAT;
        SetDocument(GetDocIdOfCurrentTab(), doc);
        UpdateStatusBar(true);
    }
    InvalidateUICommand(cmdEOLWin,  UI_INVALIDATIONS_PROPERTY, &UI_PKEY_BooleanValue);
    InvalidateUICommand(cmdEOLUnix, UI_INVALIDATIONS_PROPERTY, &UI_PKEY_BooleanValue);
    InvalidateUICommand(cmdEOLMac,  UI_INVALIDATIONS_PROPERTY, &UI_PKEY_BooleanValue);
    return true;
}

HRESULT CCmdEOLWin::IUICommandHandlerUpdateProperty(REFPROPERTYKEY key, const PROPVARIANT* /*ppropvarCurrentValue*/, PROPVARIANT* ppropvarNewValue)
{
    if (UI_PKEY_BooleanValue == key)
    {
        return UIInitPropertyFromBoolean(UI_PKEY_BooleanValue, ScintillaCall(SCI_GETEOLMODE)==SC_EOL_CRLF, ppropvarNewValue);
    }
    return E_NOTIMPL;
}

void CCmdEOLWin::TabNotify(TBHDR * ptbhdr)
{
    if (ptbhdr->hdr.code == TCN_SELCHANGE)
    {
        InvalidateUICommand(UI_INVALIDATIONS_PROPERTY, &UI_PKEY_BooleanValue);
    }
}

CCmdEOLUnix::CCmdEOLUnix(void * obj) : ICommand(obj)
{
    InvalidateUICommand(UI_INVALIDATIONS_PROPERTY, &UI_PKEY_BooleanValue);
}

CCmdEOLUnix::~CCmdEOLUnix()
{
}

bool CCmdEOLUnix::Execute()
{
    ScintillaCall(SCI_SETEOLMODE, SC_EOL_LF);
    ScintillaCall(SCI_CONVERTEOLS, SC_EOL_LF);
    if (HasActiveDocument())
    {
        CDocument doc = GetActiveDocument();
        doc.m_format = UNIX_FORMAT;
        SetDocument(GetDocIdOfCurrentTab(), doc);
        UpdateStatusBar(true);
    }
    InvalidateUICommand(cmdEOLWin,  UI_INVALIDATIONS_PROPERTY, &UI_PKEY_BooleanValue);
    InvalidateUICommand(cmdEOLUnix, UI_INVALIDATIONS_PROPERTY, &UI_PKEY_BooleanValue);
    InvalidateUICommand(cmdEOLMac,  UI_INVALIDATIONS_PROPERTY, &UI_PKEY_BooleanValue);
    return true;
}

HRESULT CCmdEOLUnix::IUICommandHandlerUpdateProperty(REFPROPERTYKEY key, const PROPVARIANT* /*ppropvarCurrentValue*/, PROPVARIANT* ppropvarNewValue)
{
    if (UI_PKEY_BooleanValue == key)
    {
        return UIInitPropertyFromBoolean(UI_PKEY_BooleanValue, ScintillaCall(SCI_GETEOLMODE)==SC_EOL_LF, ppropvarNewValue);
    }
    return E_NOTIMPL;
}

void CCmdEOLUnix::TabNotify(TBHDR * ptbhdr)
{
    if (ptbhdr->hdr.code == TCN_SELCHANGE)
    {
        InvalidateUICommand(UI_INVALIDATIONS_PROPERTY, &UI_PKEY_BooleanValue);
    }
}

CCmdEOLMac::CCmdEOLMac(void * obj) : ICommand(obj)
{
    InvalidateUICommand(UI_INVALIDATIONS_PROPERTY, &UI_PKEY_BooleanValue);
}

CCmdEOLMac::~CCmdEOLMac()
{
}

bool CCmdEOLMac::Execute()
{
    ScintillaCall(SCI_SETEOLMODE, SC_EOL_CR);
    ScintillaCall(SCI_CONVERTEOLS, SC_EOL_CR);
    if (HasActiveDocument())
    {
        CDocument doc = GetActiveDocument();
        doc.m_format = MAC_FORMAT;
        SetDocument(GetDocIdOfCurrentTab(), doc);
        UpdateStatusBar(true);
    }
    InvalidateUICommand(cmdEOLWin,  UI_INVALIDATIONS_PROPERTY, &UI_PKEY_BooleanValue);
    InvalidateUICommand(cmdEOLUnix, UI_INVALIDATIONS_PROPERTY, &UI_PKEY_BooleanValue);
    InvalidateUICommand(cmdEOLMac,  UI_INVALIDATIONS_PROPERTY, &UI_PKEY_BooleanValue);
    return true;
}

HRESULT CCmdEOLMac::IUICommandHandlerUpdateProperty(REFPROPERTYKEY key, const PROPVARIANT* /*ppropvarCurrentValue*/, PROPVARIANT* ppropvarNewValue)
{
    if (UI_PKEY_BooleanValue == key)
    {
        return UIInitPropertyFromBoolean(UI_PKEY_BooleanValue, ScintillaCall(SCI_GETEOLMODE)==SC_EOL_CR, ppropvarNewValue);
    }
    return E_NOTIMPL;
}

void CCmdEOLMac::TabNotify(TBHDR * ptbhdr)
{
    if (ptbhdr->hdr.code == TCN_SELCHANGE)
    {
        InvalidateUICommand(UI_INVALIDATIONS_PROPERTY, &UI_PKEY_BooleanValue);
    }
}
