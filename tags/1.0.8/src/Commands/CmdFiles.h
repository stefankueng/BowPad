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
        InvalidateUICommand(UI_INVALIDATIONS_STATE, NULL);
    }
    ~CCmdSave(){}


    virtual bool Execute();
    virtual UINT GetCmdId() { return cmdSave; }

    virtual void ScintillaNotify( Scintilla::SCNotification * pScn );
    virtual void TabNotify(TBHDR * ptbhdr);

    virtual HRESULT IUICommandHandlerUpdateProperty( REFPROPERTYKEY key, const PROPVARIANT* ppropvarCurrentValue, PROPVARIANT* ppropvarNewValue );

};

class CCmdSaveAll : public ICommand
{
public:
    CCmdSaveAll(void * obj) : ICommand(obj)
    {
        InvalidateUICommand(UI_INVALIDATIONS_STATE, NULL);
    }
    ~CCmdSaveAll(){}


    virtual bool Execute();
    virtual UINT GetCmdId() { return cmdSaveAll; }

    virtual void ScintillaNotify( Scintilla::SCNotification * pScn );
    virtual void TabNotify(TBHDR * ptbhdr);

    virtual HRESULT IUICommandHandlerUpdateProperty( REFPROPERTYKEY key, const PROPVARIANT* ppropvarCurrentValue, PROPVARIANT* ppropvarNewValue );

};

class CCmdSaveAs : public ICommand
{
public:
    CCmdSaveAs(void * obj) : ICommand(obj)
    {
    }
    ~CCmdSaveAs(){}


    virtual bool Execute();
    virtual UINT GetCmdId() { return cmdSaveAs; }
};

class CCmdReload : public ICommand
{
public:
    CCmdReload(void * obj) : ICommand(obj)
    {
    }
    ~CCmdReload(){}


    virtual bool Execute()
    {
        if (HasActiveDocument())
        {
            CDocument doc = GetActiveDocument();
            ReloadTab(GetActiveTabIndex(), doc.m_encoding);
        }
        return true;
    }
    virtual UINT GetCmdId() { return cmdReload; }

    virtual HRESULT IUICommandHandlerUpdateProperty( REFPROPERTYKEY key, const PROPVARIANT* ppropvarCurrentValue, PROPVARIANT* ppropvarNewValue );

    virtual void TabNotify( TBHDR * ptbhdr );
};
