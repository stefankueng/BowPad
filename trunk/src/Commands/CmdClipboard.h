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

class CCmdCut : public ICommand
{
public:

    CCmdCut(void * obj) : ICommand(obj)
    {
    }

    ~CCmdCut(void)
    {
    }

    virtual bool Execute() 
    {
        ScintillaCall(SCI_CUT);
        return true;
    }

    virtual UINT GetCmdId() { return cmdCut; }

    virtual void ScintillaNotify( Scintilla::SCNotification * pScn ) 
    {
        if (pScn->nmhdr.code == SCN_UPDATEUI)
            InvalidateUICommand(UI_INVALIDATIONS_STATE, NULL);
    }

    virtual HRESULT IUICommandHandlerUpdateProperty( REFPROPERTYKEY key, const PROPVARIANT* /*ppropvarCurrentValue*/, PROPVARIANT* ppropvarNewValue ) 
    {
        if (UI_PKEY_Enabled == key)
        {
            return UIInitPropertyFromBoolean(UI_PKEY_Enabled, (ScintillaCall(SCI_GETSELTEXT) > 1), ppropvarNewValue);
        }
        return E_NOTIMPL;
    }

};


class CCmdCopy : public ICommand
{
public:

    CCmdCopy(void * obj) : ICommand(obj)
    {
    }

    ~CCmdCopy(void)
    {
    }

    virtual bool Execute() 
    {
        ScintillaCall(SCI_COPYALLOWLINE);
        return true;
    }

    virtual UINT GetCmdId() { return cmdCopy; }
};

class CCmdPaste : public ICommand
{
public:

    CCmdPaste(void * obj) : ICommand(obj)
    {
    }

    ~CCmdPaste(void)
    {
    }

    virtual bool Execute() 
    {
        ScintillaCall(SCI_PASTE);
        return true;
    }

    virtual UINT GetCmdId() { return cmdPaste; }

    virtual void ScintillaNotify( Scintilla::SCNotification * pScn ) 
    {
        if (pScn->nmhdr.code == SCN_MODIFIED)
            InvalidateUICommand(UI_INVALIDATIONS_STATE, NULL);
    }

    virtual HRESULT IUICommandHandlerUpdateProperty( REFPROPERTYKEY key, const PROPVARIANT* /*ppropvarCurrentValue*/, PROPVARIANT* ppropvarNewValue ) 
    {
        if (UI_PKEY_Enabled == key)
        {
            return UIInitPropertyFromBoolean(UI_PKEY_Enabled, (ScintillaCall(SCI_CANPASTE) != 0), ppropvarNewValue);
        }
        return E_NOTIMPL;
    }

};
