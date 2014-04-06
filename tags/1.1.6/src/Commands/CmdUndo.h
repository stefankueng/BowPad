// This file is part of BowPad.
//
// Copyright (C) 2013-2014 - Stefan Kueng
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
#include "MRU.h"

class CCmdUndo : public ICommand
{
public:

    CCmdUndo(void * obj) : ICommand(obj)
    {
        InvalidateUICommand(UI_INVALIDATIONS_STATE, NULL);
    }

    ~CCmdUndo(void)
    {
    }

    virtual bool Execute() override
    {
        ScintillaCall(SCI_UNDO);
        return true;
    }

    virtual UINT GetCmdId() override { return cmdUndo; }

    virtual void ScintillaNotify(Scintilla::SCNotification * pScn) override
    {
        if (pScn->nmhdr.code == SCN_MODIFIED)
            InvalidateUICommand(UI_INVALIDATIONS_STATE, NULL);
    }

    virtual HRESULT IUICommandHandlerUpdateProperty(REFPROPERTYKEY key, const PROPVARIANT* /*ppropvarCurrentValue*/, PROPVARIANT* ppropvarNewValue) override
    {
        if (UI_PKEY_Enabled == key)
        {
            return UIInitPropertyFromBoolean(UI_PKEY_Enabled, (ScintillaCall(SCI_CANUNDO) != 0), ppropvarNewValue);
        }
        return E_NOTIMPL;
    }

};


class CCmdRedo : public ICommand
{
public:

    CCmdRedo(void * obj) : ICommand(obj)
    {
        InvalidateUICommand(UI_INVALIDATIONS_STATE, NULL);
    }

    ~CCmdRedo(void)
    {
    }

    virtual bool Execute() override
    {
        ScintillaCall(SCI_REDO);
        return true;
    }

    virtual UINT GetCmdId() override { return cmdRedo; }

    virtual void ScintillaNotify(Scintilla::SCNotification * pScn) override
    {
        if (pScn->nmhdr.code == SCN_MODIFIED)
            InvalidateUICommand(UI_INVALIDATIONS_STATE, NULL);
    }

    virtual HRESULT IUICommandHandlerUpdateProperty(REFPROPERTYKEY key, const PROPVARIANT* /*ppropvarCurrentValue*/, PROPVARIANT* ppropvarNewValue) override
    {
        if (UI_PKEY_Enabled == key)
        {
            return UIInitPropertyFromBoolean(UI_PKEY_Enabled, (ScintillaCall(SCI_CANREDO) != 0), ppropvarNewValue);
        }
        return E_NOTIMPL;
    }

};
