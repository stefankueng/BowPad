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
#include "BaseDialog.h"
#include "DlgResizer.h"


class CCmdTrim : public ICommand
{
public:

    CCmdTrim(void * obj)
        : ICommand(obj)
    {
    }

    ~CCmdTrim(void)
    {
    }

    virtual bool Execute();

    virtual UINT GetCmdId() { return cmdTrim; }

};

class CCmdTabs2Spaces : public ICommand
{
public:

    CCmdTabs2Spaces(void * obj)
        : ICommand(obj)
    {
    }

    ~CCmdTabs2Spaces(void)
    {
    }

    virtual bool Execute();

    virtual UINT GetCmdId() { return cmdTabs2Spaces; }

};


class CCmdSpaces2Tabs : public ICommand
{
public:

    CCmdSpaces2Tabs(void * obj)
        : ICommand(obj)
    {
    }

    ~CCmdSpaces2Tabs(void)
    {
    }

    virtual bool Execute();

    virtual UINT GetCmdId() { return cmdSpaces2Tabs; }

};
