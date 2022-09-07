// This file is part of BowPad.
//
// Copyright (C) 2013-2014, 2016-2017, 2021-2022 - Stefan Kueng
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
    CCmdZoom100(void* obj)
        : ICommand(obj)
    {
    }

    ~CCmdZoom100() override = default;

    bool Execute() override
    {
        Scintilla().SetZoom(0);
        return true;
    }

    UINT GetCmdId() override { return cmdZoom100; }
};

class CCmdZoomIn : public ICommand
{
public:
    CCmdZoomIn(void* obj)
        : ICommand(obj)
    {
    }

    ~CCmdZoomIn() override = default;

    bool Execute() override
    {
        Scintilla().ZoomIn();
        return true;
    }

    UINT GetCmdId() override { return cmdZoomIn; }
};

class CCmdZoomOut : public ICommand
{
public:
    CCmdZoomOut(void* obj)
        : ICommand(obj)
    {
    }

    ~CCmdZoomOut() override = default;

    bool Execute() override
    {
        Scintilla().ZoomOut();
        return true;
    }

    UINT GetCmdId() override { return cmdZoomOut; }
};
