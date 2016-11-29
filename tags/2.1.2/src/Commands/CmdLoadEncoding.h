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

class CCmdLoadAsEncoded : public ICommand
{
public:

    CCmdLoadAsEncoded(void * obj) : ICommand(obj)
    {
    }

    ~CCmdLoadAsEncoded(void)
    {
    }

    virtual bool Execute() override { return false; }
    virtual UINT GetCmdId() override { return cmdLoadAsEncoding; }


    virtual HRESULT IUICommandHandlerUpdateProperty(REFPROPERTYKEY key, const PROPVARIANT* ppropvarCurrentValue, PROPVARIANT* ppropvarNewValue) override;

    virtual HRESULT IUICommandHandlerExecute(UI_EXECUTIONVERB verb, const PROPERTYKEY* key, const PROPVARIANT* ppropvarValue, IUISimplePropertySet* pCommandExecutionProperties) override;

    virtual void TabNotify(TBHDR * ptbhdr) override;

};

class CCmdConvertEncoding : public ICommand
{
public:

    CCmdConvertEncoding(void * obj) : ICommand(obj)
    {
    }

    ~CCmdConvertEncoding(void)
    {
    }

    virtual bool Execute() override { return false; }
    virtual UINT GetCmdId() override { return cmdConvertEncoding; }


    virtual HRESULT IUICommandHandlerUpdateProperty(REFPROPERTYKEY key, const PROPVARIANT* ppropvarCurrentValue, PROPVARIANT* ppropvarNewValue) override;

    virtual HRESULT IUICommandHandlerExecute(UI_EXECUTIONVERB verb, const PROPERTYKEY* key, const PROPVARIANT* ppropvarValue, IUISimplePropertySet* pCommandExecutionProperties) override;

    virtual void TabNotify(TBHDR * ptbhdr) override;

};