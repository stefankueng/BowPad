// This file is part of BowPad.
//
// Copyright (C) 2024 - Stefan Kueng
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
#include <map>

namespace Scintilla
{
struct SCNotification;
};

class CCmdTail : public ICommand
{
public:
    CCmdTail(void* obj);

    ~CCmdTail() override = default;

    bool Execute() override;

    UINT GetCmdId() override { return cmdTail; }

        void TabNotify(TBHDR* ptbHdr) override;
    HRESULT IUICommandHandlerUpdateProperty(REFPROPERTYKEY key, const PROPVARIANT* /*pPropVarCurrentValue*/, PROPVARIANT* pPropVarNewValue) override;
        void    OnTimer(UINT id) override;

    private:
    UINT m_timerId;
};
