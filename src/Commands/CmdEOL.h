// This file is part of BowPad.
//
// Copyright (C) 2013-2014, 2016 - Stefan Kueng
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

    CCmdEOLWin(void * obj);

    ~CCmdEOLWin();

    bool Execute() override;

    UINT GetCmdId() override { return cmdEOLWin; }

    HRESULT IUICommandHandlerUpdateProperty(REFPROPERTYKEY key, const PROPVARIANT* /*ppropvarCurrentValue*/, PROPVARIANT* ppropvarNewValue) override;

    void TabNotify(TBHDR * ptbhdr) override;
};

class CCmdEOLUnix : public ICommand
{
public:

    CCmdEOLUnix(void * obj);

    ~CCmdEOLUnix();

    bool Execute() override;

    UINT GetCmdId() override { return cmdEOLUnix; }

    HRESULT IUICommandHandlerUpdateProperty(REFPROPERTYKEY key, const PROPVARIANT* /*ppropvarCurrentValue*/, PROPVARIANT* ppropvarNewValue) override;

    void TabNotify(TBHDR* ptbhdr) override;
};

class CCmdEOLMac : public ICommand
{
public:

    CCmdEOLMac(void * obj);

    ~CCmdEOLMac();

    bool Execute() override;

    UINT GetCmdId() override { return cmdEOLMac; }

    HRESULT IUICommandHandlerUpdateProperty(REFPROPERTYKEY key, const PROPVARIANT* /*ppropvarCurrentValue*/, PROPVARIANT* ppropvarNewValue) override;

    void TabNotify(TBHDR * ptbhdr) override;
};
