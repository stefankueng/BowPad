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


class CCmdPrevNext : public ICommand
{
public:

    CCmdPrevNext(void * obj) : ICommand(obj)
    {
    }

    ~CCmdPrevNext(void)
    {
    }

    virtual bool Execute() override { return false; }
    virtual UINT GetCmdId() override { return cmdPrevNext; }

    virtual void ScintillaNotify(Scintilla::SCNotification * pScn) override;

    virtual void TabNotify(TBHDR * ptbhdr) override;
};


class CCmdPrevious : public ICommand
{
public:

    CCmdPrevious(void * obj) : ICommand(obj)
    {
    }

    ~CCmdPrevious(void)
    {
    }

    virtual bool Execute() override;
    virtual UINT GetCmdId() override { return cmdPrevious; }

    virtual HRESULT IUICommandHandlerUpdateProperty(REFPROPERTYKEY key, const PROPVARIANT* ppropvarCurrentValue, PROPVARIANT* ppropvarNewValue) override;

};

class CCmdNext : public ICommand
{
public:

    CCmdNext(void * obj) : ICommand(obj)
    {
    }

    ~CCmdNext(void)
    {
    }

    virtual bool Execute() override;
    virtual UINT GetCmdId() override { return cmdNext; }

    virtual HRESULT IUICommandHandlerUpdateProperty(REFPROPERTYKEY key, const PROPVARIANT* ppropvarCurrentValue, PROPVARIANT* ppropvarNewValue) override;

};
