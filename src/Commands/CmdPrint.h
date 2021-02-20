// This file is part of BowPad.
//
// Copyright (C) 2013-2017, 2021 - Stefan Kueng
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
    CCmdPrint(void* obj)
        : ICommand(obj)
    {
    }

    ~CCmdPrint() = default;

    bool Execute() override
    {
        Print(true);
        return true;
    }

    UINT GetCmdId() override { return cmdPrint; }
    void AfterInit() override
    {
        InvalidateUICommand(UI_INVALIDATIONS_PROPERTY, &UI_PKEY_BooleanValue);
    }

    void Print(bool bShowDlg);
};

class CCmdPrintNow : public CCmdPrint
{
public:
    CCmdPrintNow(void* obj)
        : CCmdPrint(obj)
    {
    }

    ~CCmdPrintNow() = default;

    bool Execute() override
    {
        Print(false);
        return true;
    }

    UINT GetCmdId() override { return cmdPrintNow; }
};

class CCmdPageSetup : public ICommand
{
public:
    CCmdPageSetup(void* obj)
        : ICommand(obj)
    {
    }

    ~CCmdPageSetup() = default;

    bool Execute() override;

    UINT GetCmdId() override { return cmdPageSetup; }
};
