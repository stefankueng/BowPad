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

class CCmdZoom100 : public ICommand
{
public:

    CCmdZoom100(void * obj) : ICommand(obj)
    {
    }

    ~CCmdZoom100(void)
    {
    }

    virtual bool Execute()
    {
        ScintillaCall(SCI_SETZOOM, 0);
        return true;
    }

    virtual UINT GetCmdId() { return cmdZoom100; }
};

class CCmdZoomIn : public ICommand
{
public:

    CCmdZoomIn(void * obj) : ICommand(obj)
    {
    }

    ~CCmdZoomIn(void)
    {
    }

    virtual bool Execute()
    {
        ScintillaCall(SCI_ZOOMIN);
        return true;
    }

    virtual UINT GetCmdId() { return cmdZoomIn; }
};

class CCmdZoomOut : public ICommand
{
public:

    CCmdZoomOut(void * obj) : ICommand(obj)
    {
    }

    ~CCmdZoomOut(void)
    {
    }

    virtual bool Execute()
    {
        ScintillaCall(SCI_ZOOMOUT);
        return true;
    }

    virtual UINT GetCmdId() { return cmdZoomOut; }
};
