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

class CCmdFont : public ICommand
{
public:

    CCmdFont(void * obj);
    ~CCmdFont() = default;

    bool Execute() override { return true; }

    UINT GetCmdId() override { return cmdFontOnly; }

    HRESULT IUICommandHandlerUpdateProperty(REFPROPERTYKEY key, const PROPVARIANT* ppropvarCurrentValue, PROPVARIANT* ppropvarNewValue) override;

    HRESULT IUICommandHandlerExecute(UI_EXECUTIONVERB verb, const PROPERTYKEY* key, const PROPVARIANT* /*ppropvarValue*/, IUISimplePropertySet* pCommandExecutionProperties) override;
private:
    bool            m_bBold;
    bool            m_bItalic;
    int             m_size;
    std::wstring    m_FontName;
};
