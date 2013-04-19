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

#pragma once
#include "ICommand.h"
#include "BowPadUI.h"

class CCmdEOLWin : public ICommand
{
public:

    CCmdEOLWin(void * obj) : ICommand(obj)
    {
        InvalidateUICommand(UI_INVALIDATIONS_PROPERTY, &UI_PKEY_BooleanValue);
    }

    ~CCmdEOLWin(void)
    {
    }

    virtual bool Execute() 
    {
        ScintillaCall(SCI_SETEOLMODE, SC_EOL_CRLF);
        ScintillaCall(SCI_CONVERTEOLS, SC_EOL_CRLF);
        CDocument doc = GetDocument(GetCurrentTabIndex());
        doc.m_format = WIN_FORMAT;
        SetDocument(GetCurrentTabIndex(), doc);
        InvalidateUICommand(cmdEOLWin,  UI_INVALIDATIONS_PROPERTY, &UI_PKEY_BooleanValue);
        InvalidateUICommand(cmdEOLUnix, UI_INVALIDATIONS_PROPERTY, &UI_PKEY_BooleanValue);
        InvalidateUICommand(cmdEOLMac,  UI_INVALIDATIONS_PROPERTY, &UI_PKEY_BooleanValue);
        return true;
    }

    virtual UINT GetCmdId() { return cmdEOLWin; }

    virtual HRESULT IUICommandHandlerUpdateProperty( REFPROPERTYKEY key, const PROPVARIANT* /*ppropvarCurrentValue*/, PROPVARIANT* ppropvarNewValue ) 
    {
        if (UI_PKEY_BooleanValue == key)
        {
            return UIInitPropertyFromBoolean(UI_PKEY_BooleanValue, ScintillaCall(SCI_GETEOLMODE)==SC_EOL_CRLF, ppropvarNewValue);
        }
        return E_NOTIMPL;
    }

    virtual void TabNotify( TBHDR * ptbhdr )
    {
        if (ptbhdr->hdr.code == TCN_SELCHANGE)
        {
            InvalidateUICommand(UI_INVALIDATIONS_PROPERTY, &UI_PKEY_BooleanValue);
        }
    }
};

class CCmdEOLUnix : public ICommand
{
public:

    CCmdEOLUnix(void * obj) : ICommand(obj)
    {
        InvalidateUICommand(UI_INVALIDATIONS_PROPERTY, &UI_PKEY_BooleanValue);
    }

    ~CCmdEOLUnix(void)
    {
    }

    virtual bool Execute() 
    {
        ScintillaCall(SCI_SETEOLMODE, SC_EOL_LF);
        ScintillaCall(SCI_CONVERTEOLS, SC_EOL_LF);
        CDocument doc = GetDocument(GetCurrentTabIndex());
        doc.m_format = WIN_FORMAT;
        SetDocument(GetCurrentTabIndex(), doc);
        InvalidateUICommand(cmdEOLWin,  UI_INVALIDATIONS_PROPERTY, &UI_PKEY_BooleanValue);
        InvalidateUICommand(cmdEOLUnix, UI_INVALIDATIONS_PROPERTY, &UI_PKEY_BooleanValue);
        InvalidateUICommand(cmdEOLMac,  UI_INVALIDATIONS_PROPERTY, &UI_PKEY_BooleanValue);
        return true;
    }

    virtual UINT GetCmdId() { return cmdEOLUnix; }

    virtual HRESULT IUICommandHandlerUpdateProperty( REFPROPERTYKEY key, const PROPVARIANT* /*ppropvarCurrentValue*/, PROPVARIANT* ppropvarNewValue ) 
    {
        if (UI_PKEY_BooleanValue == key)
        {
            return UIInitPropertyFromBoolean(UI_PKEY_BooleanValue, ScintillaCall(SCI_GETEOLMODE)==SC_EOL_LF, ppropvarNewValue);
        }
        return E_NOTIMPL;
    }

    virtual void TabNotify( TBHDR * ptbhdr )
    {
        if (ptbhdr->hdr.code == TCN_SELCHANGE)
        {
            InvalidateUICommand(UI_INVALIDATIONS_PROPERTY, &UI_PKEY_BooleanValue);
        }
    }
};

class CCmdEOLMac : public ICommand
{
public:

    CCmdEOLMac(void * obj) : ICommand(obj)
    {
        InvalidateUICommand(UI_INVALIDATIONS_PROPERTY, &UI_PKEY_BooleanValue);
    }

    ~CCmdEOLMac(void)
    {
    }

    virtual bool Execute() 
    {
        ScintillaCall(SCI_SETEOLMODE, SC_EOL_CR);
        ScintillaCall(SCI_CONVERTEOLS, SC_EOL_CR);
        CDocument doc = GetDocument(GetCurrentTabIndex());
        doc.m_format = WIN_FORMAT;
        SetDocument(GetCurrentTabIndex(), doc);
        InvalidateUICommand(cmdEOLWin,  UI_INVALIDATIONS_PROPERTY, &UI_PKEY_BooleanValue);
        InvalidateUICommand(cmdEOLUnix, UI_INVALIDATIONS_PROPERTY, &UI_PKEY_BooleanValue);
        InvalidateUICommand(cmdEOLMac,  UI_INVALIDATIONS_PROPERTY, &UI_PKEY_BooleanValue);
        return true;
    }

    virtual UINT GetCmdId() { return cmdEOLMac; }

    virtual HRESULT IUICommandHandlerUpdateProperty( REFPROPERTYKEY key, const PROPVARIANT* /*ppropvarCurrentValue*/, PROPVARIANT* ppropvarNewValue ) 
    {
        if (UI_PKEY_BooleanValue == key)
        {
            return UIInitPropertyFromBoolean(UI_PKEY_BooleanValue, ScintillaCall(SCI_GETEOLMODE)==SC_EOL_CR, ppropvarNewValue);
        }
        return E_NOTIMPL;
    }

    virtual void TabNotify( TBHDR * ptbhdr )
    {
        if (ptbhdr->hdr.code == TCN_SELCHANGE)
        {
            InvalidateUICommand(UI_INVALIDATIONS_PROPERTY, &UI_PKEY_BooleanValue);
        }
    }
};

