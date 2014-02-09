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

class CCmdPrint : public ICommand
{
public:

    CCmdPrint(void * obj) : ICommand(obj)
    {
        InvalidateUICommand(UI_INVALIDATIONS_PROPERTY, &UI_PKEY_BooleanValue);
    }

    ~CCmdPrint(void)
    {
    }

    virtual bool Execute() override
    {
        Print(true);
        return true;
    }

    virtual UINT GetCmdId() override { return cmdPrint; }

    void Print(bool bShowDlg);
};

class CCmdPrintNow : public CCmdPrint
{
public:

    CCmdPrintNow(void * obj) : CCmdPrint(obj)
    {
    }

    ~CCmdPrintNow(void)
    {
    }

    virtual bool Execute() override
    {
        Print(false);
        return true;
    }

    virtual UINT GetCmdId() override { return cmdPrintNow; }
};

class CCmdPageSetup : public ICommand
{
public:

    CCmdPageSetup(void * obj) : ICommand(obj)
    {
    }

    ~CCmdPageSetup(void)
    {
    }

    virtual bool Execute() override;

    virtual UINT GetCmdId() override { return cmdPageSetup; }
};

