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

#include <vector>

class CCmdSessionLoad : public ICommand
{
public:

    CCmdSessionLoad(void * obj) : ICommand(obj)
    {
    }

    ~CCmdSessionLoad()
    {
    }

    bool Execute() override;

    UINT GetCmdId() override { return cmdSessionLoad; }

    void OnClose() override;

protected:
    void    RestoreSavedSession();
};

class CCmdSessionAutoLoad : public CCmdSessionLoad
{
public:

    CCmdSessionAutoLoad(void * obj);

    ~CCmdSessionAutoLoad()
    {
    }

    bool Execute() override;

    UINT GetCmdId() override { return cmdSessionAutoLoad; }

    void OnClose() override {}

    HRESULT IUICommandHandlerUpdateProperty(REFPROPERTYKEY key, const PROPVARIANT* /*ppropvarCurrentValue*/, PROPVARIANT* ppropvarNewValue) override;

    void AfterInit() override;

};

struct SessionItem
{
    SessionItem(const std::wstring& path , const CPosData& posData)
        : path(path), posData(posData) {}
    std::wstring path;
    CPosData posData;
};

class CCmdSessionRestoreLast : public ICommand
{
public:

    CCmdSessionRestoreLast(void * obj) : ICommand(obj)
    {
    }

    ~CCmdSessionRestoreLast()
    {
    }

    bool Execute() override;

    UINT GetCmdId() override { return cmdSessionLast; }

    void OnDocumentClose(int index) override;

private:
    std::vector<SessionItem> m_docstates;
};
