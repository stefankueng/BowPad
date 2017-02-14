// This file is part of BowPad.
//
// Copyright (C) 2013-2014, 2016-2017 - Stefan Kueng
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

class CCmdConvertUppercase : public ICommand
{
public:

    CCmdConvertUppercase(void * obj) : ICommand(obj)
    {
    }

    ~CCmdConvertUppercase() = default;

    bool Execute() override;

    UINT GetCmdId() override { return cmdUppercase; }
    void AfterInit() override
    {
        InvalidateUICommand(UI_INVALIDATIONS_PROPERTY, &UI_PKEY_BooleanValue);
    }
};

class CCmdConvertLowercase : public ICommand
{
public:

    CCmdConvertLowercase(void * obj) : ICommand(obj)
    {
    }

    ~CCmdConvertLowercase() = default;

    bool Execute() override;

    UINT GetCmdId() override { return cmdLowercase; }
    void AfterInit() override
    {
        InvalidateUICommand(UI_INVALIDATIONS_PROPERTY, &UI_PKEY_BooleanValue);
    }
};

class CCmdConvertTitlecase : public ICommand
{
public:

    CCmdConvertTitlecase(void * obj) : ICommand(obj)
    {
    }

    ~CCmdConvertTitlecase() = default;

    bool Execute() override;

    UINT GetCmdId() override { return cmdTitlecase; }
    void AfterInit() override
    {
        InvalidateUICommand(UI_INVALIDATIONS_PROPERTY, &UI_PKEY_BooleanValue);
    }
};
