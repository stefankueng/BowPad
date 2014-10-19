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

class CCmdDelete : public ICommand
{
public:

    CCmdDelete(void * obj) : ICommand(obj)
    {
    }

    ~CCmdDelete(void)
    {
    }

    bool Execute() override
    {
        ScintillaCall(SCI_CLEAR);
        return true;
    }

    UINT GetCmdId() override { return cmdDelete; }
};


class CCmdSelectAll : public ICommand
{
public:

    CCmdSelectAll(void * obj) : ICommand(obj)
    {
    }

    ~CCmdSelectAll(void)
    {
    }

    bool Execute() override
    {
        ScintillaCall(SCI_SELECTALL);
        return true;
    }

    UINT GetCmdId() override { return cmdSelectAll; }
};

class CCmdGotoBrace : public ICommand
{
public:

    CCmdGotoBrace(void * obj) : ICommand(obj)
    {
    }

    ~CCmdGotoBrace(void)
    {
    }

    bool Execute() override
    {
        GotoBrace();
        return true;
    }

    UINT GetCmdId() override { return cmdGotoBrace; }
};

class CCmdToggleTheme : public ICommand
{
public:
    CCmdToggleTheme(void * obj);
    ~CCmdToggleTheme();

    bool Execute() override;

    UINT GetCmdId() override { return cmdToggleTheme; }

    HRESULT IUICommandHandlerUpdateProperty(REFPROPERTYKEY key, const PROPVARIANT* /*ppropvarCurrentValue*/, PROPVARIANT* ppropvarNewValue) override;

private:
    UI_HSBCOLOR text;
    UI_HSBCOLOR back;
    UI_HSBCOLOR high;
};

class CCmdConfigShortcuts : public ICommand
{
public:

    CCmdConfigShortcuts(void * obj) : ICommand(obj)
    {
    }

    ~CCmdConfigShortcuts(void)
    {
    }

    bool Execute() override;

    UINT GetCmdId() override { return cmdConfigShortcuts; }
};

class CCmdAutoBraces : public ICommand
{
public:

    CCmdAutoBraces(void * obj);

    ~CCmdAutoBraces(void);

    bool Execute() override;

    UINT GetCmdId() override { return cmdAutoBraces; }

    HRESULT IUICommandHandlerUpdateProperty(REFPROPERTYKEY key, const PROPVARIANT* /*ppropvarCurrentValue*/, PROPVARIANT* ppropvarNewValue) override;
};

class CCmdViewFileTree : public ICommand
{
public:

    CCmdViewFileTree(void * obj);

    ~CCmdViewFileTree(void);

    bool Execute() override;

    UINT GetCmdId() override { return cmdFileTree; }

    HRESULT IUICommandHandlerUpdateProperty(REFPROPERTYKEY key, const PROPVARIANT* /*ppropvarCurrentValue*/, PROPVARIANT* ppropvarNewValue) override;
};
