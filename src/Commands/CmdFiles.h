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

#include <string>
#include <vector>

#include <Shobjidl.h>
#include <Shellapi.h>
#include <atlbase.h>


class CCmdOpen : public ICommand
{
public:

    CCmdOpen(void * obj) : ICommand(obj)
    {
    }

    ~CCmdOpen(void)
    {
    }

    virtual bool Execute();
    virtual UINT GetCmdId() { return cmdOpen; }
};

class CCmdSave : public ICommand
{
public:
    CCmdSave(void * obj) : ICommand(obj)
    {
    }
    ~CCmdSave(){}


    virtual bool Execute();
    virtual UINT GetCmdId() { return cmdSave; }

    virtual void ScintillaNotify( Scintilla::SCNotification * pScn );

    virtual HRESULT IUICommandHandlerUpdateProperty( REFPROPERTYKEY key, const PROPVARIANT* ppropvarCurrentValue, PROPVARIANT* ppropvarNewValue );

};

class CCmdSaveAll : public ICommand
{
public:
    CCmdSaveAll(void * obj) : ICommand(obj)
    {
    }
    ~CCmdSaveAll(){}


    virtual bool Execute();
    virtual UINT GetCmdId() { return cmdSaveAll; }

    virtual void ScintillaNotify( Scintilla::SCNotification * pScn );

    virtual HRESULT IUICommandHandlerUpdateProperty( REFPROPERTYKEY key, const PROPVARIANT* ppropvarCurrentValue, PROPVARIANT* ppropvarNewValue );

};
