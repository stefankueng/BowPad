// This file is part of BowPad.
//
// Copyright (C) 2013-2014, 2016-2017, 2020-2022 - Stefan Kueng
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

class CCmdLineDuplicate : public ICommand
{
public:
    CCmdLineDuplicate(void* obj)
        : ICommand(obj)
    {
    }

    ~CCmdLineDuplicate() override = default;

    bool Execute() override
    {
        Scintilla().SelectionDuplicate();
        return true;
    }

    UINT GetCmdId() override { return cmdLineDuplicate; }
};

class CCmdLineSplit : public ICommand
{
public:
    CCmdLineSplit(void* obj)
        : ICommand(obj)
    {
    }

    ~CCmdLineSplit() override = default;

    bool Execute() override
    {
        Scintilla().TargetFromSelection();
        Scintilla().LinesSplit(0);
        return true;
    }

    UINT GetCmdId() override { return cmdLineSplit; }
};

class CCmdLineJoin : public ICommand
{
public:
    CCmdLineJoin(void* obj)
        : ICommand(obj)
    {
    }

    ~CCmdLineJoin() override = default;

    bool Execute() override
    {
        Scintilla().TargetFromSelection();
        Scintilla().LinesJoin();
        return true;
    }

    UINT GetCmdId() override { return cmdLineJoin; }
};

class CCmdLineUp : public ICommand
{
public:
    CCmdLineUp(void* obj)
        : ICommand(obj)
    {
    }

    ~CCmdLineUp() override = default;

    bool Execute() override
    {
        Scintilla().MoveSelectedLinesUp();
        return true;
    }

    UINT GetCmdId() override { return cmdLineUp; }
};

class CCmdLineDown : public ICommand
{
public:
    CCmdLineDown(void* obj)
        : ICommand(obj)
    {
    }

    ~CCmdLineDown() override = default;

    bool Execute() override
    {
        Scintilla().MoveSelectedLinesDown();
        return true;
    }

    UINT GetCmdId() override { return cmdLineDown; }
};
