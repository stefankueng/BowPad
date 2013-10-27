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

class CCmdLineDuplicate : public ICommand
{
public:

    CCmdLineDuplicate(void * obj) : ICommand(obj)
    {
    }

    ~CCmdLineDuplicate(void)
    {
    }

    virtual bool Execute()
    {
        ScintillaCall(SCI_LINEDUPLICATE);
        return true;
    }

    virtual UINT GetCmdId() { return cmdLineDuplicate; }
};

class CCmdLineSplit : public ICommand
{
public:

    CCmdLineSplit(void * obj) : ICommand(obj)
    {
    }

    ~CCmdLineSplit(void)
    {
    }

    virtual bool Execute()
    {
        ScintillaCall(SCI_TARGETFROMSELECTION);
        ScintillaCall(SCI_LINESSPLIT);
        return true;
    }

    virtual UINT GetCmdId() { return cmdLineSplit; }
};

class CCmdLineJoin : public ICommand
{
public:

    CCmdLineJoin(void * obj) : ICommand(obj)
    {
    }

    ~CCmdLineJoin(void)
    {
    }

    virtual bool Execute()
    {
        ScintillaCall(SCI_TARGETFROMSELECTION);
        ScintillaCall(SCI_LINESJOIN);
        return true;
    }

    virtual UINT GetCmdId() { return cmdLineJoin; }
};

class CCmdLineUp : public ICommand
{
public:

    CCmdLineUp(void * obj) : ICommand(obj)
    {
    }

    ~CCmdLineUp(void)
    {
    }

    virtual bool Execute()
    {
        ScintillaCall(SCI_MOVESELECTEDLINESUP);
        return true;
    }

    virtual UINT GetCmdId() { return cmdLineUp; }
};

class CCmdLineDown : public ICommand
{
public:

    CCmdLineDown(void * obj) : ICommand(obj)
    {
    }

    ~CCmdLineDown(void)
    {
    }

    virtual bool Execute()
    {
        ScintillaCall(SCI_MOVESELECTEDLINESDOWN);
        return true;
    }

    virtual UINT GetCmdId() { return cmdLineDown; }
};
